#include <WiFi.h>
#include "wifi-connection.h"
#include "secrets.h"

unsigned int WiFiConnection::nbConnection = 0;

void WiFiConnection::setup() {
  Serial.println("[wifi] connexion en cours...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    retry++;

    if(retry == 20) {
      Serial.println("[wifi] echec connexion, restart");
      ESP.restart();
    }
  }
  nbConnection++;
  Serial.println("");
  Serial.print("[wifi] connecte, IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("[wifi] config NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("[wifi] NTP configure, attente sync...");
  int ntpRetries = 0;
  struct tm tminfo;
  while (!getLocalTime(&tminfo) && ntpRetries < 10) {
    delay(500);
    ntpRetries++;
  }
  if (ntpRetries >= 10) {
    Serial.println("[wifi] ERREUR: NTP sync echoue");
  } else {
    Serial.printf("[wifi] NTP sync OK: %02d:%02d:%02d\n", tminfo.tm_hour, tminfo.tm_min, tminfo.tm_sec);
  }
}

void WiFiConnection::loop() {
  if(WiFi.status() != WL_CONNECTED) {
    setup();
  }
}
