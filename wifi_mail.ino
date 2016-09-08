/*
  Name:    wifi_test.ino
  Created: 2015-12-25 17:56:27
  Author:  Heartr
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "CaptivePortal.h"

ESP8266WebServer server(80);

CaptivePortal captivePortal;

String stassid;
String stapwd;

const char* apssid = "Mail Config";
const char* appwd = "supersecure";

String hostname2 = "MailNoSpace";
byte requestDelay = 20;

const String html_head = "<!DOCTYPE html> <html lang=\"en\"> <head> <meta charset=\"utf-8\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" user-scalable=\"no\" /> <title>Mail Config</title> <style> input[type=text], input[type=password] { width: 100%; padding: 4px; margin: 4px; } input[type=submit] { margin: 4px; } form:first-of-type { padding-top: 0; } form { padding: 40px; padding-bottom: 0; } .container { max-width: 450px; margin: 0 auto; } h1, p { font-family: helvetica; font-weight: 100; color: rgba(0, 0, 0, 0.85); margin-top: 0; } h1 { margin-top: 45px; } p { font-size: .9em; } </style> </head>";
const String html_get_1 = " <body> <div class=\"container\"> <h1> Configuration page </h1> <form action=\"/\" method=\"post\"> <input type=\"text\" name=\"ssid\" value=\"\" placeholder=\"Network name\" /> <input type=\"password\" name=\"pwd\" value=\"\" placeholder=\"Password\" /> <input type=\"submit\" value=\"Submit\" /> <p> On submit the wifi module will try to connect to the submited network, if it succeeds the module will close this network (Mail Config). </p> </form> <form action=\"/\" method=\"post\"> <input type=\"text\" name=\"hostname\" placeholder=\"Hostname\" /> <input type=\"submit\" value=\"Submit\" /> <p> This is the name known to the router when connected. Current hostname is ";
const String html_get_2 = ". </p> </form> <form action=\"/\" method=\"post\"> <input type=\"number\" min=\"0\" max=\"255\" name=\"number\" placeholder=\"Delay in sec\" /> <input type=\"submit\" value=\"Submit\" /> <p> It is possible to edit the delay between requests here (default is 20 current is ";
const String html_get_3 = "). </p> </form> <form action=\"/\" method=\"post\"> <input type=\"submit\" name=\"skip\" value=\"Skip\" /> <p> Pressing this button will cause the wifi module to try to use the last entered network credentials (";
const String html_get_4 = "). </p> </form> </div> </body> </html>";

const String html_post_error = "<body> <form action=\"/\" method=\"get\"> <h1>Something went wrong!</h1> <p> It could be anything... </p> <input value=\"Back\" type=\"submit\" /> </form> </body> </html>";
const String html_post_success = "<body> <form action=\"/\" method=\"get\"> <h1>Probably success!</h1> <p> Just wait a couple of seconds, if the hotspot still exists re enter the network name and password. </p> <input value=\"Back\" type=\"submit\" /> </form> </body> </html>";

const char* host = "mail.adamh.se";
const char* path = "/all";
const IPAddress apIP(192, 168, 1, 1);
const IPAddress staIP(192, 168, 1, 137);

const int dataPin = 2;
bool login = false;
String NL = "\r\n";
int mailflags = 0, mailflags_n = 0;
String view_get() {
  return html_head + html_get_1 + hostname2 + html_get_2 + String(requestDelay) + html_get_3 + stassid + html_get_4;
}

String view_post_s() {
  return html_head + html_post_success;
}

String view_post_e() {
  return html_head + html_post_error;
}

void sendPulse(bool high) {
  digitalWrite(dataPin, HIGH);
  delay(2);
  if (high) delay(4); // May need to change this.
  digitalWrite(dataPin, LOW);
}

void sendInt(int n) {
  for (int t = 1; t != 0b100000 ; t <<= 1) { // Send 8 bits
    sendPulse(bool(n & t));
    delay(2);
  }
}

boolean isValidNumber(String str) {
  for (byte i = 0; i < str.length(); i++)
    if (!isDigit(str.charAt(i))) return false;

  return true;
}

void downloadData() {
  WiFiClient client;

  if (!client.connect(host, 80)) {
    Serial.println("Connection failed");
    return;
  }

  client.print("GET " + String(path) + " HTTP/1.1" + NL + "Host: " + String(host) + NL + "Connection: close" + NL + NL);

  int timeout = 50;
  while (!client.available() && timeout-- > 0) delay(200); // worst 10s

  if (timeout <= 0) {
    Serial.println("Could not connect to host, connection timeout.");
    client.stop();
    return;
  }

  // Read all the lines of the reply from server and print them to Serial
  String line = client.readStringUntil('\r');
  if (line.length() < 12 || line.substring(9, 12) != "200") { // line.length ? this needs testing!!
    Serial.println("Cant reach " + String(host) + String(path) + " error code: " + line.substring(9, 12));
    client.stop();
    return;
  }

  // read headers
  timeout = 500;
  while (client.available() && timeout-- > 0) {
    line = client.readStringUntil('\r');
    if (line == "\n") {
      line = client.readStringUntil('\r');
      break;
    }
    delay(10);
  }

  if (timeout < 0) {
    Serial.println("Inavlid response from server, too much data");
    client.stop();
    return;
  }
  ////

  line = client.readStringUntil('\r');
  line = line.substring(1);

  delay(100); // WDT?

  if (line.length() < 1 || !isValidNumber(line)) {
    Serial.println("Invalid response from server. (" + line + ")");
    client.stop();
    return;
  }
  if (mailflags == line.toInt()) {
    mailflags_n++;
    if (mailflags_n >= 4) {
      Serial.println("Response from server, value unchanged");
      client.stop();
      return;
    }
  } else {
    mailflags_n = 0;
  }

  mailflags = line.toInt();

  Serial.println("Response from server (mail flags): " + line);
  sendInt(mailflags);
  client.stop();
}

void write_EEPROM() {
  Serial.println("Writing EEPROM bytes: " + stassid + " " + stapwd + " " + hostname2 + " " + requestDelay);
  
  // stassid
  for (int i = 0; i < stassid.length(); ++i)
    EEPROM.write(i, stassid[i]);

  for (int i = stassid.length(); i < 32; ++i)
    EEPROM.write(i, 0);
  ////

  //stapwd
  for (int i = 0; i < stapwd.length(); ++i)
    EEPROM.write(i + 32, stapwd[i]);

  for (int i = stapwd.length(); i < 64; ++i)
    EEPROM.write(i + 32, 0);
  ////

  // hostname2
  for (int i = 0; i < hostname2.length(); ++i)
    EEPROM.write(i + 96, hostname2[i]);

  for (int i = hostname2.length(); i < 32; ++i)
    EEPROM.write(i + 96, 0);
  ////

  // requestDelay
  EEPROM.write(128, requestDelay);
  ////

  EEPROM.commit();
  Serial.println("Done");
}

bool read_EEPROM() {
  Serial.println("Reading EEPROM");
  char c;

  String essid;
  for (int i = 0; i < 32; ++i) { // According to IEEE standards.
    c = char(EEPROM.read(i));
    if (c != 0) essid += c;
  }

  Serial.print("SSID: ");
  Serial.println(essid);

  String epwd = "";
  for (int i = 32; i < 96; ++i) { // 63 chars, I think, but who knows
    c = char(EEPROM.read(i));
    if (c != 0) epwd += c;
  }

  Serial.print("PWD: ");
  Serial.println(epwd);

  String ehostname2 = "";
  for (int i = 96; i < 128; ++i) {
    c = char(EEPROM.read(i));
    if (c != 0) ehostname2 += c;
  }

  Serial.print("Hostname: ");
  Serial.println(ehostname2);

  byte erequestDelay = EEPROM.read(128);

  Serial.print("Request Delay: ");
  Serial.println(String(erequestDelay));

  if (erequestDelay != 0) {
    requestDelay = erequestDelay;
  }

  if (ehostname2.length() > 3) {
    hostname2 = ehostname2;
  }

  if (essid.length() > 0 && epwd.length() > 0) {
    stassid = essid;
    stapwd = epwd;
    return true;
  }
  return false;
}

void handle_config_get() {
  // write html
  server.send(200, "text/html", view_get());
}

void handle_config_post() {
  if (server.hasArg("hostname")) {
    if (server.arg("hostname").length() > 32 || server.arg("hostname").length() < 4) {
      server.send(200, "text/html", view_post_e());
      return;
    }

    hostname2 = server.arg("hostname");
    server.send(200, "text/html", view_get());
    Serial.println("Changed hostname to " + hostname2);
    return;
  }

  if (server.hasArg("number")) {
    if (server.arg("number").length() > 3 || server.arg("number").length() < 1 || !isValidNumber(server.arg("number"))) {
      server.send(200, "text/html", view_post_e());
      return;
    }

    int n = server.arg("number").toInt();
    if (n > 255 || n < 1)
    {
      server.send(200, "text/html", view_post_e());
      return;
    }

    requestDelay = (byte)n;
    server.send(200, "text/html", view_get());
    Serial.println("Changed request delay to " + requestDelay);
    return;
  }

  if (server.hasArg("skip")) {
    server.send(200, "text/html", view_post_s());
    login = true;
    Serial.println("Skipping config page");
    return;
  }

  if (server.hasArg("ssid") && server.hasArg("pwd")) {
    String ssid = server.arg("ssid");
    String pwd = server.arg("pwd");

    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PWD: ");
    Serial.println(pwd);

    if (ssid.length() < 4 || ssid.length() > 32 || pwd.length() < 4 || pwd.length() > 64) {
      server.send(200, "text/html", view_post_e());
      return;
    }

    server.send(200, "text/html", view_post_s());

    captivePortal.stop();
    WiFi.begin(ssid.c_str(), pwd.c_str()); // WiFi mode should be set by itself (WiFi.begin())
    WiFi.hostname(hostname2.c_str());

    Serial.println("Trying to connect to network. Temporary stoping dns");
    int start = millis();
    while (millis() - start < 10 * 1000) { // 30sec timeout
      delay(500);
      Serial.print(".");
      if (WiFi.status() == WL_CONNECTED) {
        login = true;
        stassid = ssid;
        stapwd = pwd;
        Serial.println("");
        return;
      }
    }

    Serial.println("Failed to connect the to network. Starting dns");
    WiFi.disconnect();
    captivePortal.start(53, apIP);
    delay(100);

  } else {
    server.send(200, "text/html", view_post_e());
  }
}


void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("Waiting 5 sec.");
  delay(5000);
  Serial.println("Booting...");

  EEPROM.begin(32 + 64 + 32 + 1); // max: 4096. wifi name + wifi pass + hostname + request delay

  pinMode(2, OUTPUT);
  digitalWrite(dataPin, LOW);

  WiFi.disconnect(); // Clear previous config.
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  delay(100);
  WiFi.softAP(apssid, appwd);

  Serial.println("Setting up AP");
  delay(2000);

  captivePortal.start(53, apIP);

  server.on("/", HTTP_POST, handle_config_post);
  server.onNotFound(handle_config_get);
  server.begin();

  int start, t, c;
  bool len = read_EEPROM();

  start = t = c = millis();
  while (!(((c - start > 1000 * 60 * 5) && len) || login)) {
    captivePortal.processNextRequest();
    yield();
    server.handleClient();
    if (c - t > 1000) {
      Serial.print(".");
      t = c;
    }
    yield();
    c = millis(); // Reduces millis calls to just one
  }

  Serial.println("");
  Serial.println("WiFi connected, closing AP and DNS");

  write_EEPROM();

  captivePortal.stop();
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_STA);
  delay(100);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Internet!");
  } else if (stassid.length() > 0 && stapwd.length() > 0) {
    Serial.println("Connecting to saved network");

    WiFi.begin(stassid.c_str(), stapwd.c_str());
    WiFi.hostname(hostname2.c_str());

    int start = millis();
    while (millis() - start < 10 * 1000 && WiFi.status() != WL_CONNECTED ) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("");
  } else {
    Serial.println("No internet");
  }

  sendInt(0);
  delay(3000);
}

void loop() {
  downloadData();
  delay(((int)requestDelay) * 1000);
  //ESP.deepSleep(30 * 1000000); // should be 30sec, GPIO0 and GPIO2 needs to be pulled high for deepsleep to work (mostly, needs more research).
  //delay(1000); // Once in a time there was a bug in the standard esp library.
  Serial.println("Waking up");
}



