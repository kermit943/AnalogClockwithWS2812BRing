// Analoguhr mit Neopixelring

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

//#include <PubSubClient.h>

/*---FastLED---*/

#define NUM_LEDS 12

//#define DATA_PIN 5 // for WEMOS-D1
#define DATA_PIN 0 // for ESP-01
CRGB leds[NUM_LEDS];

const char *ssid = "QUINGDOM";
const char *password = "DJJBXRDD";
const char *id = "AnalogUhr"; // Hostname for OTA

// const char* mqtt_server = "192.168.0.20";

IPAddress timeServer(130, 149, 17,
                     21); // time.nist.gov NTP server  ptbtime1.ptb.de
const int timeZone = 2;   // Central European Time: 1=Winterzeit 2=Sommerzeit!!

WiFiUDP Udp;
unsigned int localPort = 8888; // local port to listen for UDP packets

void setup() {

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  Serial.begin(115200);
  Serial.println("TimeNTP");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  ArduinoOTA.setHostname(id);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using
    // SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);

  cicle(2);
  Blink(2);
}

time_t prevDisplay = 0; // when the digital clock was displayed

int prev_hour;
int prev_min;
int min_buf;

void loop() {

  ArduinoOTA.handle(); // OTA-Stuff

  if (prev_hour != hour() || timeStatus() != timeSet) {
    setSyncProvider(getNtpTime);
    prev_hour = hour();
  }

  if (prev_min != minute()) {
    if (min_buf >= 5) {
      showtime();
      min_buf = 0;
    } else {
      min_buf++;
    }
  }

  delay(200);
}

/*--------*/

void cicle(int repeats) {
  for (int i = 0; i < repeats; i++) {
    for (int dot = 0; dot < (NUM_LEDS + 1); dot++) {
      leds[dot] = CRGB::Aqua;
      FastLED.show();
      // clear this led for the next time around the loop
      leds[dot] = CRGB::Black;
      delay(30);
    }
  }
}

void Blink(int repeats) {
  for (int i = 0; i < repeats; i++) {
    fill_solid(leds, NUM_LEDS, CRGB(150, 0, 0));
    FastLED.show();
    delay(100);
    FastLED.clear();
    FastLED.show();
    delay(100);
  }
}

void showtime() {
  int h = hourFormat12();
  int m = minute();
  while (m % 5) {
    m++;
  }
  m /= 5;

  if (m == 12) {
    m = 0;
  }
  if (h == 12) {
    h = 0;
  }

  if (m == h) {
    FastLED.clear();
    leds[h] = CRGB(40, 40, 0);
    // leds [m] = CRGB(40, 0, 40);
    FastLED.show();
  } else {
    FastLED.clear();
    leds[h] = CRGB(0, 40, 40);
    leds[m] = CRGB(40, 0, 40);
    FastLED.show();
  }
}
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing
                                    // packets

time_t getNtpTime() {
  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
