#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#define WiFimodeButton D6

String ssid, pass;
const String accesspassword = "pass";
const char* WiFiHostname = "mydevice";

ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  //https://www.mischianti.org/2020/06/22/wemos-d1-mini-esp8266-integrated-littlefs-filesystem-part-5/
  LittleFS.begin();
  File settingsFile = LittleFS.open("/settings.txt", "r");
  String settingsString;
  if (settingsFile && !settingsFile.isDirectory()) {
    settingsString = settingsFile.readString();
  }
  settingsFile.close();

  //https://arduinojson.org/v6/doc/upgrade/
  DynamicJsonDocument doc(10024);
  DeserializationError error = deserializeJson(doc, settingsString);
  if (!error)
  {
    ssid = doc["ssid"].as<String>();
    pass = doc["pass"].as<String>();
    int variable = doc["variable"];
  }

  WiFi.softAPdisconnect();
  WiFi.disconnect();

  pinMode(WiFimodeButton, INPUT_PULLUP);
  if (digitalRead(WiFimodeButton) == LOW || (ssid == "" || pass.length() < 8)) {
    //access point part
    Serial.println("Creating Accesspoint");
    IPAddress apIP(192, 168, 1, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("aquarium", "12354566545t");
    Serial.print("IP address:\t");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    //station part

    Serial.print("connecting to...");
    Serial.println(ssid + " " + pass);
    WiFi.hostname(WiFiHostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    /*
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
    */
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  Serial.println();

  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_POST, handleRootPost);
  //server.on("/", handleRoot);
  server.on("/login", handleLogin);


  const char * headerkeys[] = {"Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);
  server.begin();
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  if (!is_authentified()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  String content = R"rawliteral(<!DOCTYPE html><html>
   <head>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <meta charset="UTF-8">
      <title>My device</title>
   </head>
   <body>
      <p align="center"><a href="/login?DISCONNECT=YES">LOGOUT!</a></p>
      <form action='/' method='POST'>
         Home WiFi Network Name (SSID): <input type='text' name='SSID' value='$ssid'><br>
         Password: <input type='password' name='wifipass' minlength='8'><br>
          <input type='submit' value='Submit'>
      </form>
         </body>
</html>)rawliteral";

  content.replace("$ssid", ssid);
  server.send(200, "text/html", content);
}

void handleRootPost() {
  if (!is_authentified()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  if ((server.hasArg("variable") && server.arg("variable") != "") || (server.hasArg("SSID") && server.arg("SSID") != "")) {
    DynamicJsonDocument doc(10024);
    int variable;
    doc["variable"] = variable = server.arg("variable").toInt();

    doc["ssid"] = ssid = server.arg("SSID");
    if (server.hasArg("wifipass") && server.arg("wifipass").length() >= 8)
      doc["pass"] = server.arg("wifipass");
    else
      doc["pass"] = pass;

    File settingsFile = LittleFS.open("/settings.txt", "w");
    String settingsString;
    serializeJson(doc, settingsString);
    settingsFile.print(settingsString);
    settingsFile.close();

    if (server.hasArg("SSID") && server.arg("SSID") != "" && server.hasArg("wifipass") && server.arg("wifipass") != "")
    {
      ESP.restart();
    }
  }

  handleRoot();
}

bool is_authentified() {
  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    String cookielogin = "WEATHERSTATION=" + md5(accesspassword);
    if (cookie.indexOf(cookielogin) != -1) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;
}

void handleLogin() {
  String msg;
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")) {
    Serial.println("Disconnection");
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "WEATHERSTATION=0");
    server.send(301);
    return;
  }
  if (server.hasArg("PASSWORD")) {
    if (server.arg("PASSWORD") == accesspassword) {
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      String cookielogin = "WEATHERSTATION=" + md5(accesspassword);
      server.sendHeader("Set-Cookie", cookielogin);
      server.send(301);
      Serial.println("Log in Successful");
      return;
    }
    msg = "Wrong username/password! try again.";
    Serial.println("Log in Failed");
  }
  String content = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>" + String(WiFiHostname) + "</title></head>";
  content += "<body><form action='/login' method='POST'>";
  content += "Password:<input type='password' name='PASSWORD' placeholder='password'><br>";
  content += "<input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "</body></html>";
  server.send(200, "text/html", content);
}

String md5(String str) {
  MD5Builder _md5;
  _md5.begin();
  _md5.add(String(str));
  _md5.calculate();
  return _md5.toString();
}
