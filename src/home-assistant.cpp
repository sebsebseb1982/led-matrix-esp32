#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "secrets.h"
#include "home-assistant.h"

// ATTENTION FUSEAU : HA renvoie ses timestamps en UTC, et mktime() interprete
// un struct tm en heure *locale*. Cela n'est correct que parce que
// configTime(0, 0, ...) fixe le fuseau a UTC (voir wifi-connection.cpp).
// timegm() n'existe pas dans la newlib ESP32, d'ou ce couplage assume.
// Changer le fuseau dans configTime decalerait toutes les series.

static String getTimestamp(long hoursBack) {
  time_t now = time(NULL);
  time_t then = now - (time_t)hoursBack * 3600;

  struct tm* tm_info = gmtime(&then);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
  return String(buffer) + "Z";
}

// Parse un timestamp ISO UTC de HA en unix ts (voir la note sur le fuseau).
static long parseHaTimestamp(const char* s) {
  struct tm tm_info;
  memset(&tm_info, 0, sizeof(tm_info));
  strptime(s, "%Y-%m-%dT%H:%M:%S", &tm_info);
  tm_info.tm_isdst = -1;
  return (long)mktime(&tm_info);
}

// GET sur l'API HA : URL, bearer, retry des echecs *reseau* uniquement
// (un 401 ou un 404 ne reussira jamais, inutile de le rejouer), puis
// deserialisation. Retourne false si quoi que ce soit echoue.
static bool haGet(const String& path, JsonDocument& doc, const JsonDocument* filter = nullptr) {
  HTTPClient http;
  http.begin(String(F("http://")) + SECRET_HOME_ASSISTANT_HOST + path);
  http.useHTTP10(true);
  http.addHeader("Authorization", String(F("Bearer ")) + SECRET_HOME_ASSISTANT_TOKEN);

  int httpCode;
  int retry = 0;
  do {
    httpCode = http.GET();
  } while (httpCode <= 0 && ++retry < HTTP_RETRY);

  if (httpCode != 200) {
    Serial.printf("[ha] KO %d\n", httpCode);
    http.end();
    return false;
  }

  DeserializationError error = filter
    ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
    : deserializeJson(doc, http.getStream());
  http.end();

  if (error) {
    Serial.printf("[ha] json: %s\n", error.c_str());
    return false;
  }
  return true;
}

// Filtre ArduinoJson commun aux appels /history : ne garde que state et
// last_changed pour economiser la RAM.
static void historyFilter(JsonDocument& filter) {
  filter[0][0]["state"] = true;
  filter[0][0]["last_changed"] = true;
}

// Construit le chemin /history/period pour une fenetre [startHoursBack, endHoursBack].
// minimal_response retire les attributs des points intermediaires : a desactiver
// quand on lit un attribut plutot que l'etat.
static String historyPath(const String& entityId, long startHoursBack, long endHoursBack,
                          bool minimal = true) {
  String path;
  path += F("/api/history/period/");
  path += getTimestamp(startHoursBack);
  path += F("?filter_entity_id=");
  path += entityId;
  path += F("&end_time=");
  path += getTimestamp(endHoursBack);
  if (minimal) path += F("&minimal_response");
  return path;
}

// Extrait le tableau de points d'une reponse /history. null si vide.
static JsonArray historyPoints(JsonDocument& doc) {
  JsonArray result = doc.as<JsonArray>();
  if (result.isNull() || result.size() == 0) return JsonArray();
  return result[0].as<JsonArray>();
}

// true si l'etat est une valeur numerique exploitable ("unavailable", "unknown"... sont rejetes).
static bool isNumericState(const char* stateStr) {
  return stateStr && (isdigit((unsigned char)stateStr[0]) || stateStr[0] == '-');
}

String HomeAssistant::getEntityState(const String& entityId) {
  JsonDocument doc;
  if (!haGet(String(F("/api/states/")) + entityId, doc)) return "?";
  return doc["state"].as<String>();
}

static void fetchChunk(const String& entityId, float* out, int outSize,
                       long startHoursBack, long endHoursBack,
                       long globalStartTs, long totalSeconds) {
  JsonDocument filter;
  historyFilter(filter);

  JsonDocument doc;
  if (!haGet(historyPath(entityId, startHoursBack, endHoursBack), doc, &filter)) return;

  JsonArray entityHistory = historyPoints(doc);
  if (entityHistory.isNull()) return;

  Serial.printf("[ha] chunk [-%ldh..-%ldh]\n", startHoursBack, endHoursBack);

  // HA renvoie en premier element l'etat *anterieur* a start_time : son
  // last_changed precede la fenetre demandee et serait clampe sur out[0],
  // que les 6 chunks se rechireraient a tour de role. On ne garde ce point
  // d'amorce que pour le chunk le plus ancien, seul a borner vraiment out[0].
  long chunkStartTs = globalStartTs + totalSeconds - startHoursBack * 3600L;
  bool isOldestChunk = (chunkStartTs <= globalStartTs);

  for (JsonObject state : entityHistory) {
    const char* stateStr = state["state"];
    if (!isNumericState(stateStr)) continue;

    const char* tsStr = state["last_changed"];
    if (!tsStr) continue;

    long ts = parseHaTimestamp(tsStr);
    if (ts < chunkStartTs && !isOldestChunk) continue;

    long offset = ts - globalStartTs;
    if (offset < 0) offset = 0;
    if (offset > totalSeconds) offset = totalSeconds;

    int idx = (int)((offset / (float)totalSeconds) * (outSize - 1));
    out[idx] = atof(stateStr);
  }
}

int HomeAssistant::getRawSeries(const String& entityId, long hoursBack,
                                long* timestamps, float* values, int maxPoints) {
  JsonDocument filter;
  historyFilter(filter);

  JsonDocument doc;
  if (!haGet(historyPath(entityId, hoursBack, 0), doc, &filter)) return 0;

  JsonArray entityHistory = historyPoints(doc);
  if (entityHistory.isNull()) return 0;

  // Decoupage de la fenetre en maxPoints buckets temporels egaux.
  // Chaque point recu est accumule dans son bucket ; on fait la moyenne a la fin.
  // Cela garantit que tous les points recus contribuent au resultat,
  // quelle que soit leur repartition dans le temps.
  long tStart     = (long)time(nullptr) - hoursBack * 3600L;
  long bucketSecs = (hoursBack * 3600L) / maxPoints;
  if (bucketSecs < 1) bucketSecs = 1;

  int cnts[maxPoints];
  memset(cnts,   0, maxPoints * sizeof(int));
  memset(values, 0, maxPoints * sizeof(float));

  int totalReceived = 0;
  for (JsonObject state : entityHistory) {
    const char* stateStr = state["state"];
    if (!isNumericState(stateStr)) continue;
    const char* tsStr = state["last_changed"];
    if (!tsStr) continue;

    long ts = parseHaTimestamp(tsStr);

    int bucket = (int)((ts - tStart) / bucketSecs);
    if (bucket < 0)          bucket = 0;
    if (bucket >= maxPoints) bucket = maxPoints - 1;

    values[bucket] += atof(stateStr);
    cnts[bucket]++;
    totalReceived++;
  }

  // Compaction : calcul de la moyenne et suppression des buckets vides.
  int stored = 0;
  for (int i = 0; i < maxPoints; i++) {
    if (cnts[i] > 0) {
      timestamps[stored] = tStart + (long)i * bucketSecs + bucketSecs / 2;
      values[stored]     = values[i] / (float)cnts[i];
      stored++;
    }
  }

  Serial.printf("[ha] getRawSeries %s: %d points stockes (total recu: %d)\n", entityId.c_str(), stored, totalReceived);
  return stored;
}

bool HomeAssistant::getSunTimes(time_t& nextRising, time_t& nextSetting) {
  JsonDocument filter;
  filter["attributes"]["next_rising"]  = true;
  filter["attributes"]["next_setting"] = true;

  JsonDocument doc;
  if (!haGet(String(F("/api/states/sun.sun")), doc, &filter)) return false;

  const char* risingStr  = doc["attributes"]["next_rising"];
  const char* settingStr = doc["attributes"]["next_setting"];
  if (!risingStr || !settingStr) return false;

  nextRising  = (time_t)parseHaTimestamp(risingStr);
  nextSetting = (time_t)parseHaTimestamp(settingStr);

  Serial.printf("[ha] sun: next_rising=%s next_setting=%s\n", risingStr, settingStr);
  return true;
}

int HomeAssistant::getHeatingColumns(const String& entityId, long hoursBack, bool* out, int outSize) {
  for (int i = 0; i < outSize; i++) out[i] = false;

  long totalSeconds  = hoursBack * 3600L;
  long globalStartTs = (long)time(NULL) - totalSeconds;

  auto toCol = [&](long ts) -> int {
    long offset = ts - globalStartTs;
    if (offset < 0) offset = 0;
    if (offset > totalSeconds) offset = totalSeconds;
    return (int)((offset / (float)totalSeconds) * (outSize - 1));
  };

  // hvac_action change sans toucher a l'etat (qui reste "heat") : seul
  // last_updated date ces changements, last_changed resterait fige.
  JsonDocument filter;
  filter[0][0]["last_updated"] = true;
  filter[0][0]["attributes"]["hvac_action"] = true;

  // Les etats arrivent dans l'ordre chronologique, chunk le plus ancien en
  // premier : chaque nouvel etat clot l'intervalle ouvert par le precedent.
  // Une colonne est marquee des qu'une portion de chauffe la touche, si bien
  // que les cycles PWM courts du PID apparaissent comme une plage continue.
  long prevTs      = globalStartTs;
  bool prevHeating = false;
  auto closeInterval = [&](long ts) {
    if (prevHeating)
      for (int x = toCol(prevTs); x <= toCol(ts); x++) out[x] = true;
  };

  // Chunks de 4 h : sans minimal_response chaque etat porte tous les attributs
  // du PID, et le thermostat en publie un par cycle.
  const long CHUNK_HOURS = 4;
  for (long start = hoursBack; start > 0; start -= CHUNK_HOURS) {
    long end = start - CHUNK_HOURS;
    if (end < 0) end = 0;

    JsonDocument doc;
    if (!haGet(historyPath(entityId, start, end, false), doc, &filter)) continue;
    JsonArray entityHistory = historyPoints(doc);
    if (entityHistory.isNull()) continue;

    for (JsonObject state : entityHistory) {
      const char* tsStr = state["last_updated"];
      if (!tsStr) continue;
      // Le point d'amorce de chaque chunk est anterieur au precedent : on le
      // ramene a prevTs pour garder la chronologie.
      long ts = max(parseHaTimestamp(tsStr), prevTs);
      const char* action = state["attributes"]["hvac_action"];

      closeInterval(ts);
      prevTs      = ts;
      prevHeating = action && strcmp(action, "heating") == 0;
    }
    delay(100);
  }
  closeInterval(globalStartTs + totalSeconds);

  int heatingCount = 0;
  for (int i = 0; i < outSize; i++)
    if (out[i]) heatingCount++;
  Serial.printf("[ha] %s: %d/%d colonnes en chauffe\n", entityId.c_str(), heatingCount, outSize);
  return heatingCount;
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
