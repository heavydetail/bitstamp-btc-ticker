/*
    Tamás Nyilánszky / heavydetail 2017
    Fetch the latest BTC/ETH price from Bitstamp and display them on a 96x64px
    SSD1331 OLED display.
    It depends on the SSD_13XX library by sumotoy, a faster alternative to the
    Adafruit
    Download link: https://github.com/sumotoy/SSD_13XX
*/
#include <SPI.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
// Load configuration
#include "config.h"

// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     12
#define TFT_RST    13  // you can also connect this to the Arduino reset
// in which case, set this #define pin to -1!
#define TFT_DC     15

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// Option 2: use any pins but a little slower!
#define TFT_SCLK 0   // set these to be whatever pins you like!
#define TFT_MOSI 2   // set these to be whatever pins you like!
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

const char* host = "www.bitstamp.net";
const int httpsPort = 443;
boolean APIconnection = false;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char* fingerprint = "D0 26 AB 06 64 07 BC 88 56 6D 83 BE 0A 29 00 B5 10 E5 27 D2";
WiFiClientSecure client;

String dataLine1 = "{\"high\": \"0000.00\", \"last\": \"0000.00\", \"low\": \"0000.00\"}";
String dataLine2 = "{\"high\": \"0000.00\", \"last\": \"0000.00\", \"low\": \"0000.00\"}";

void setup() {
  Serial.begin(115200);
  Serial.println("\nST7735 TFT Initializing...");

  // Use this initializer if you're using a 1.8" TFT
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

  tft.fillScreen(ST7735_BLACK);
  Serial.print("Done");
  //tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.println("Connecting");
  Serial.println("Connecting...");
  //WiFi.mode(WIFI_STA);
  WiFi.begin(essid, wifiKey);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(essid, wifiKey);
    delay(500);
  }
  tft.print(WiFi.localIP());
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  bool fresh1, fresh2;
  if (fresh1 = connectToHost()) {
    dataLine1 = fetchUrl(tickerUrl1);
  }
  if (fresh2 = connectToHost()) {
    dataLine2 = fetchUrl(tickerUrl2);
  }

  // Switch screen every 10 seconds, 6 times. This means 1 refresh
  // of the data per minute. We do 2 requests, and Bitstamp has a
  // limit of 600 requests per 10 minutes before it blocks your IP.
  for (int i = 0; i < 6; i++) {
    tft.fillScreen(ST7735_BLACK);
    if (dataLine1.startsWith("{")) {
      printPriceData(fresh1, 0, 0, coinName1, dataLine1, i % 2);
    }
    if (dataLine2.startsWith("{")) {
      printPriceData(fresh2, 0, 1, coinName2, dataLine2, i % 2);
    }
    delay(10000);
  }
}

bool connectToHost() {
  /*
     Connect to the host over HTTPS. Also verify the certificate's
     fingerprint. Return true if the connection succeeds.
  */
  Serial.print("Connecting to Bitstamp   ");
  bool connection = client.connect(host, httpsPort);
  if (connection) {
    Serial.println("OK");
    APIconnection = true;
  } else {
    Serial.println("FAIL");
    APIconnection = false;
  }
  Serial.print("Verifying fingerprint    ");
  connection = connection && client.verify(fingerprint, host);
  if (connection) {
    Serial.println("OK");
  } else {
    Serial.println("FAIL");
  }
  return connection;
}

void printPriceData(bool fresh, int x, int y, char* unit, String line, int stage) {
  /*
     Print the current price, and some extra data on the second row.
     Second row alternates between (low, high) and (total_diff, diff_since_open)
     Total diff is the percentage difference between the daily low and high.
     Diff since open is the percentage difference between the opening price and
     the current one.
  */
  //x+=10;
  //y+=10;
  tft.setCursor(x, y * 32);

  tft.setTextColor(ST7735_MAGENTA);
  tft.print(unit);
  tft.print(": ");

  String high = getStringFromJSON("high", line);
  String low = getStringFromJSON("low", line);
  String last = getStringFromJSON("last", line);
  String open = getStringFromJSON("open", line);

  // See if the price went up or down so we can color the price accordingly.
  float last_f = last.toFloat();
  float open_f = open.toFloat();
  float pct_since_open = 100.0 * (last_f - open_f) / open_f;

  // First print the current price.
  tft.setCursor(70, y * 32);                       //HERE

  if (fresh) {
    if (pct_since_open >= 0.1) {
      tft.setTextColor(ST7735_GREEN);
    } else if (pct_since_open <= -0.1) {
      tft.setTextColor(ST7735_RED);
    } else {
      tft.setTextColor(ST7735_WHITE);
    }
  } else {
    tft.setTextColor(ST7735_YELLOW);
  }
  tft.print(last);

  //Large BTC/EUR
  if (unit == "BTC")
  { tft.setTextSize(2);
    tft.setCursor(10, 90);
    tft.print(last.toFloat());
    tft.setCursor(10, 70);
    tft.setTextColor(ST7735_MAGENTA);
    tft.println("BTC/EUR");
    tft.setTextSize(1);
  }

  // Pick the correct data to display on the 2nd line.
  if (stage == 0) {
    if (fresh) {
      tft.setTextColor(ST7735_WHITE);
    } else {
      tft.setTextColor(ST7735_YELLOW);
    }
    tft.setCursor(10, y * 32 + 16);                 //HERE

    tft.print(low);
    tft.setCursor(70, y * 32 + 16);                  //HERE

    tft.print(high);
  } else if (stage == 1) {
    float high_f = high.toFloat();
    float low_f = low.toFloat();
    float low_high_spread = 100.0 * (high_f - low_f) / low_f;

    tft.setCursor(10, y * 32 + 16);

    if (fresh) {
      tft.setTextColor(ST7735_WHITE);
    } else {
      tft.setTextColor(ST7735_YELLOW);
    }
    tft.print(low_high_spread, 2);
    tft.print("%");


    tft.setCursor(70, y * 32 + 16);                 //HERE


    if (fresh) {
      if (pct_since_open >= 0.1) {
        tft.setTextColor(ST7735_GREEN);
      } else if (pct_since_open <= -0.1) {
        tft.setTextColor(ST7735_RED);
      } else {
        tft.setTextColor(ST7735_WHITE);
      }
    } else {
      tft.setTextColor(ST7735_YELLOW);
    }
    if (pct_since_open >= 0) {
      tft.print("+");
    } else {
      tft.print("-");
    }
    tft.print(abs(pct_since_open), 2);
    tft.print("%");
  }

  //Uptime:
  tft.setTextSize(1);
  tft.setCursor(0, 150);
  tft.setTextColor(ST7735_WHITE);
  tft.print("Uptime: ");
  tft.setTextColor(ST7735_MAGENTA);
  tft.print((millis() / 1000) / 60);
  tft.setTextColor(ST7735_WHITE);
  tft.print(" min");

  //Connection Status:
  if (APIconnection) {
    
    tft.fillCircle(115,150,5, ST7735_GREEN);
    
  } else {
    tft.fillCircle(115,150,5, ST7735_RED);
  }
}

String fetchUrl(char* url) {
  /*
     Fetch a URL and return the data. In our case,
     everything comes on one line, which saves us
     some headaches.
  */
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266 Bitstamp Ticker\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println(String("Fetched https://") + host + "/" + url);
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println("Received data:");
  Serial.println(line);
  return line;
}

String getStringFromJSON(String needle, String haystack) {
  String searchStr = "\"" + needle + "\":";
  int beginPos = haystack.indexOf(searchStr) + searchStr.length() + 2;
  int endPos = haystack.indexOf("\"", beginPos);
  String result = haystack.substring(beginPos, endPos);
  return result;
}
