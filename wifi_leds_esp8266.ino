/*
  Name:    wifi_test.ino
  Created: 2015-12-25 17:56:27
  Author:  Heartr
*/
#include <ESP8266WiFi.h>
#include <WebSocketClient.h>
#include <ESP8266WebServer.h>
#include <ShiftRegister74HC595.h>
#include <EEPROM.h>
#include "CaptivePortal.h"

ESP8266WebServer server(80);
WebSocketClient webSocketClient;

CaptivePortal captivePortal;

String stassid;
String stapwd;

const char* apssid = "Mail Config";
const char* appwd = "supersecure";

String hostname2 = "MailNoSpace"; // hostname variable is reserved for the actuall hostname of the device.
byte requestDelay = 1;

const String html_head = "<!DOCTYPE html> <html lang=\"en\"> <head> <meta charset=\"utf-8\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" user-scalable=\"no\" /> <title>Mail Config</title> <style> input[type=text], input[type=password] { width: 100%; padding: 4px; margin: 4px; } input[type=submit] { margin: 4px; } form:first-of-type { padding-top: 0; } form { padding: 40px; padding-bottom: 0; } .container { max-width: 450px; margin: 0 auto; } h1, p { font-family: helvetica; font-weight: 100; color: rgba(0, 0, 0, 0.85); margin-top: 0; } h1 { margin-top: 45px; } p { font-size: .9em; } </style> </head>";
const String html_get_1 = " <body> <div class=\"container\"> <h1> Configuration page </h1> <form action=\"/\" method=\"post\"> <input type=\"text\" name=\"ssid\" value=\"\" placeholder=\"Network name\" /> <input type=\"password\" name=\"pwd\" value=\"\" placeholder=\"Password\" /> <input type=\"submit\" value=\"Submit\" /> <p> On submit the wifi module will try to connect to the submited network, if it succeeds the module will close this network (Mail Config). </p> </form> <form action=\"/\" method=\"post\"> <input type=\"text\" name=\"hostname\" placeholder=\"Hostname\" /> <input type=\"submit\" value=\"Submit\" /> <p> This is the name known to the router when connected. Current hostname is ";
const String html_get_2 = ". </p> </form> <form action=\"/\" method=\"post\"> <input type=\"number\" min=\"0\" max=\"255\" name=\"number\" placeholder=\"Delay in sec\" /> <input type=\"submit\" value=\"Submit\" /> <p> It is possible to edit the delay between requests here (default is 20 current is ";
const String html_get_3 = "). </p> </form> <form action=\"/\" method=\"post\"> <input type=\"submit\" name=\"skip\" value=\"Skip\" /> <p> Pressing this button will cause the wifi module to try to use the last entered network credentials (";
const String html_get_4 = "). </p> </form> </div> </body> </html>";

const String html_post_error = "<body> <form action=\"/\" method=\"get\"> <h1>Something went wrong!</h1> <p> It could be anything... </p> <input value=\"Back\" type=\"submit\" /> </form> </body> </html>";
const String html_post_success = "<body> <form action=\"/\" method=\"get\"> <h1>Probably success!</h1> <p> Just wait a couple of seconds, if the hotspot still exists re enter the network name and password. </p> <input value=\"Back\" type=\"submit\" /> </form> </body> </html>";

char* host = "number.adamh.se";
const char* numberUrl = "one";

const IPAddress apIP(192, 168, 1, 1);
const IPAddress staIP(192, 168, 1, 137);


ShiftRegister74HC595 sr(1, 0, 1, 2); // (number of shift registers, data pin, clock pin, latch pin)

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

boolean isValidNumber(String str) {
  for (byte i = 0; i < str.length(); i++)
    if (!isDigit(str.charAt(i))) return false;

  return true;
}

void write_EEPROM(String stassid, String stapwd, String hostname2, byte requestDelay) {
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

bool read_EEPROM(String &stassid, String &stapwd, String &hostname2, byte &requestDelay) {
  Serial.println("Reading EEPROM");
  char c;

  String essid;
  for (int i = 0; i < 32; ++i) { // According to IEEE standards.
    c = char(EEPROM.read(i));
    if (c != 0) essid += c;
    else break;
  }

  Serial.print("SSID: ");
  Serial.println(essid);

  String epwd = "";
  for (int i = 32; i < 96; ++i) { // 63 chars, I think, but who knows
    c = char(EEPROM.read(i));
    if (c != 0) epwd += c;
    else break;
  }

  Serial.print("PWD: ");
  Serial.println(epwd);

  String ehostname2 = "";
  for (int i = 96; i < 128; ++i) {
    c = char(EEPROM.read(i));
    if (c != 0) ehostname2 += c;
    else break;
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


WiFiClient client;

bool connectWebSocket() {

  if (client.connect(host, 80)) {
    Serial.println("Connected");
  } else {
    Serial.println("Connection failed.");
    return false;
  }

  webSocketClient.path = "/";
  webSocketClient.host = host;

  if (webSocketClient.handshake(client)) {
    Serial.println("Handshake successful");
  } else {
    Serial.println("Handshake failed.");
    return false;
  }


  webSocketClient.sendData("{'NumberUrl':'" + String(numberUrl) + "'}");
  return true;
}


void downloadData() {
  if (!client.connected()) {
    if (!connectWebSocket())
      return;
  }

  String data;
  webSocketClient.getData(data);
  //{"NumberUrl":"asd","BitName":null,"Value":1}

  if (data.length() > 0) {
    // parse data here.
    data = data.substring(data.indexOf("Value\":") + String("Value\":").length());
    data = data.substring(0, data.indexOf("}"));
    if (!isValidNumber(data))
      return;

    uint8_t pinValues[] = { data.toInt() };
    sr.setAll(pinValues); // We only have one shift register
  }
}


void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("Waiting 5 sec.");
  delay(5000);
  Serial.println("Booting...");

  EEPROM.begin(32 + 64 + 32 + 1); // max: 4096. wifi name + wifi pass + hostname + request delay


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
  bool len = read_EEPROM(stassid, stapwd, hostname2, requestDelay);

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

  write_EEPROM(stassid, stapwd, hostname2, requestDelay);

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

  delay(3000);

  connectWebSocket();

}


void loop() {
  downloadData();
  delay(((int)requestDelay) * 1000);
  //ESP.deepSleep(30 * 1000000); // should be 30sec, GPIO0 and GPIO2 needs to be pulled high for deepsleep to work (mostly, needs more research).
  //delay(1000); // Once in a time there was a bug in the standard esp library.
  Serial.println("Waking up");
}























