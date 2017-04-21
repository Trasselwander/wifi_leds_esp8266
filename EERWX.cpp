#include <EEPROM.h>
#include <Arduino.h>

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

