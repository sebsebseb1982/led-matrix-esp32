#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "common.h"
#include "secrets.h"
#include "home-assistant.h"

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