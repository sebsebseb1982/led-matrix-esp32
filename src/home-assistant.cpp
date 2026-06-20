#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "secrets.h"
#include "home-assistant.h"

static String getTimestamp(long hoursBack) {
  long now = time(NULL);
  long then = now - (hoursBack * 3600L);

  struct tm* tm_info = gmtime(&then);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
  return String(buffer) + "Z";
}

String HomeAssistant::getEntityState(const String& entityId) {
  HTTPClient http;

  String url;
  url += F("http://");
  url += SECRET_HOME_ASSISTANT_HOST;
  url += F("/api/states/");
  url += entityId;

  http.begin(url);
  http.useHTTP10(true);
  String bearer;
  bearer += F("Bearer ");
  bearer += SECRET_HOME_ASSISTANT_TOKEN;
  http.addHeader("Authorization", bearer);

  int httpCode;
  int retry = 0;

  do {
    httpCode = http.GET();
    retry++;
  } while (httpCode <= 0 && retry < HTTP_RETRY);

  if (httpCode != 200) {
    http.end();
    return "?";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error) return "?";
  return doc["state"].as<String>();
}

static void fetchChunk(const String& entityId, float* out, int outSize,
                       long startHoursBack, long endHoursBack,
                       long globalStartTs, long totalSeconds) {
  HTTPClient http;

  String url;
  url += F("http://");
  url += SECRET_HOME_ASSISTANT_HOST;
  url += F("/api/history/period/");
  url += getTimestamp(startHoursBack);
  url += F("?filter_entity_id=");
  url += entityId;
  url += F("&end_time=");
  url += getTimestamp(endHoursBack);
  url += F("&minimal_response");

  http.begin(url);
  http.useHTTP10(true);
  String bearer;
  bearer += F("Bearer ");
  bearer += SECRET_HOME_ASSISTANT_TOKEN;
  http.addHeader("Authorization", bearer);

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[ha] chunk KO: %d\n", httpCode);
    http.end();
    return;
  }

  Serial.printf("[ha] chunk [-%ldh..-%ldh] %d bytes\n", startHoursBack, endHoursBack, http.getSize());

  JsonDocument filter;
  filter[0][0]["state"] = true;
  filter[0][0]["last_changed"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (error) {
    Serial.printf("[ha] deserializeJson failed: %s\n", error.c_str());
    return;
  }

  JsonArray result = doc.as<JsonArray>();
  if (result.isNull() || result.size() == 0) return;

  JsonArray entityHistory = result[0].as<JsonArray>();
  if (entityHistory.isNull()) return;

  for (JsonObject state : entityHistory) {
    const char* stateStr = state["state"];
    if (!stateStr || (!isdigit((unsigned char)stateStr[0]) && stateStr[0] != '-')) continue;
    float value = atof(stateStr);

    const char* tsStr = state["last_changed"];
    if (!tsStr) continue;

    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));
    strptime(tsStr, "%Y-%m-%dT%H:%M:%S", &tm_info);
    tm_info.tm_isdst = -1;
    long ts = (long)mktime(&tm_info);

    long offset = ts - globalStartTs;
    if (offset < 0) offset = 0;
    if (offset > totalSeconds) offset = totalSeconds;

    int idx = (int)((offset / (float)totalSeconds) * (outSize - 1));
    out[idx] = value;
  }
}

int HomeAssistant::getRawSeries(const String& entityId, long hoursBack,
                                long* timestamps, float* values, int maxPoints) {
  HTTPClient http;

  String url;
  url += F("http://");
  url += SECRET_HOME_ASSISTANT_HOST;
  url += F("/api/history/period/");
  url += getTimestamp(hoursBack);
  url += F("?filter_entity_id=");
  url += entityId;
  url += F("&end_time=");
  url += getTimestamp(0);
  url += F("&minimal_response");

  http.begin(url);
  http.useHTTP10(true);
  String bearer;
  bearer += F("Bearer ");
  bearer += SECRET_HOME_ASSISTANT_TOKEN;
  http.addHeader("Authorization", bearer);

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[ha] getRawSeries KO: %d\n", httpCode);
    http.end();
    return 0;
  }

  JsonDocument filter;
  filter[0][0]["state"] = true;
  filter[0][0]["last_changed"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (error) {
    Serial.printf("[ha] getRawSeries deserialize failed: %s\n", error.c_str());
    return 0;
  }

  JsonArray result = doc.as<JsonArray>();
  if (result.isNull() || result.size() == 0) return 0;

  JsonArray entityHistory = result[0].as<JsonArray>();
  if (entityHistory.isNull()) return 0;

  // HA renvoie les points du plus ancien au plus recent.
  // On garde toujours les derniers maxPoints : quand le buffer est plein,
  // on decale d'un cran et on ecrit le nouveau point a la fin.
  int count = 0;
  for (JsonObject state : entityHistory) {
    const char* stateStr = state["state"];
    if (!stateStr || (!isdigit((unsigned char)stateStr[0]) && stateStr[0] != '-')) continue;
    const char* tsStr = state["last_changed"];
    if (!tsStr) continue;

    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));
    strptime(tsStr, "%Y-%m-%dT%H:%M:%S", &tm_info);
    tm_info.tm_isdst = -1;

    long ts  = (long)mktime(&tm_info);
    float val = atof(stateStr);

    if (count < maxPoints) {
      timestamps[count] = ts;
      values[count]     = val;
    } else {
      memmove(timestamps, timestamps + 1, (maxPoints - 1) * sizeof(long));
      memmove(values,     values     + 1, (maxPoints - 1) * sizeof(float));
      timestamps[maxPoints - 1] = ts;
      values[maxPoints - 1]     = val;
    }
    count++;
  }

  int stored = min(count, maxPoints);
  Serial.printf("[ha] getRawSeries %s: %d points stockes (total recu: %d)\n", entityId.c_str(), stored, count);
  return stored;
}

int HomeAssistant::getTimeSeries(const String& entityId, long hoursBack, float* out, int outSize) {
  Serial.printf("[ha] getTimeSeries(%s, %ldh, %d)\n", entityId.c_str(), hoursBack, outSize);

  for (int i = 0; i < outSize; i++) out[i] = NAN;

  long now = time(NULL);
  long globalStartTs = now - hoursBack * 3600L;
  long totalSeconds  = hoursBack * 3600L;

  const long CHUNK_HOURS = 4;

  for (long start = hoursBack; start > 0; start -= CHUNK_HOURS) {
    long end = start - CHUNK_HOURS;
    if (end < 0) end = 0;
    fetchChunk(entityId, out, outSize, start, end, globalStartTs, totalSeconds);
    delay(100);
  }

  int validCount = 0;
  for (int i = 0; i < outSize; i++)
    if (!isnan(out[i])) validCount++;

  Serial.printf("[ha] %s: %d/%d cases renseignees\n", entityId.c_str(), validCount, outSize);
  return validCount;
}
