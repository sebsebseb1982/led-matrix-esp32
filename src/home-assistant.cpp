#include <HTTPClient.h>
#include <ArduinoJson.h>

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

String HomeAssistant::getEntityState(String entityName) {
  HTTPClient http;

  String homeAssistantURL;
  homeAssistantURL += F("http://");
  homeAssistantURL += SECRET_HOME_ASSISTANT_HOST;
  homeAssistantURL += F("/api/states/");
  homeAssistantURL += entityName;

  http.begin(homeAssistantURL);
  String bearer;
  bearer += F("Bearer ");
  bearer += SECRET_HOME_ASSISTANT_TOKEN;
  http.addHeader("Authorization", bearer);

  int httpCode;
  int retry = 0;

  do {
    httpCode = http.GET();
    retry++;
    Serial.println("...");
  } while (httpCode <= 0 && retry < HTTP_RETRY);

  if (httpCode > 0) {
    Serial.println("OK");
    Serial.println("");

    String response = http.getString();
    http.end();
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return "?";
    }

    return doc["state"];

  } else {
    String error;
    error += F("KO -> code erreur = ");
    error += String(httpCode);
    Serial.println(error);
    Serial.println("");
    return "?";
  }
}

int HomeAssistant::getHistory(String entityName, HistoryPoint* points, int maxPoints, long hoursBack) {
  Serial.printf("[ha] getHistory(%s, %dh)\n", entityName.c_str(), hoursBack);

  HTTPClient http;

  String url;
  url += F("http://");
  url += SECRET_HOME_ASSISTANT_HOST;
  url += F("/api/history/start_date=");
  url += getTimestamp(hoursBack);
  url += F("&entity_id=");
  url += entityName;

  Serial.printf("[ha] url=%s\n", url.c_str());

  http.begin(url);
  String bearer;
  bearer += F("Bearer ");
  bearer += SECRET_HOME_ASSISTANT_TOKEN;
  http.addHeader("Authorization", bearer);

  int httpCode;
  int retry = 0;

  do {
    httpCode = http.GET();
    retry++;
    Serial.printf("[ha] httpCode=%d retry=%d\n", httpCode, retry);
  } while (httpCode <= 0 && retry < HTTP_RETRY);

  if (httpCode != 200) {
    Serial.printf("[ha] History KO: %d\n", httpCode);
    http.end();
    return 0;
  }

  String response = http.getString();
  Serial.printf("[ha] reponse=%d bytes\n", response.length());
  http.end();

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.printf("[ha] deserializeJson failed: %s\n", error.c_str());
    return 0;
  }

  JsonArray array = doc.as<JsonArray>();
  int count = 0;
  
  for (JsonObject item : array) {
    if (count >= maxPoints) break;
    
    JsonArray stateHistory = item["state_history"].as<JsonArray>();
    for (JsonObject state : stateHistory) {
      if (count >= maxPoints) break;
      
      HistoryPoint p;
      p.ts = (int)state["last_changed_ts"].as<long>();
      p.value = state["state"].as<float>();
      points[count++] = p;
    }
  }

  Serial.printf("[ha] %s: %d points retournes\n", entityName.c_str(), count);
  if (count > 0) {
    Serial.printf("[ha] premier: ts=%ld val=%.1f\n", points[0].ts, points[0].value);
    Serial.printf("[ha] dernier: ts=%ld val=%.1f\n", points[count-1].ts, points[count-1].value);
  }
  return count;
}