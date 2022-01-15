#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include "MyWire.h"

#define MAX_PARAM_LEN 128

static const char page_header[] PROGMEM = R"(
<html><head><title>EmonPub</title></head>
<body><h1>EmonPub</h1>
<h2>Lastest Values:</h2><pre>%s</pre>
<h2>EmonCMS Configuration:</h2><table>)";

static const char page_server[] PROGMEM = R"(
  <tr><td>server (SSL):<td>%s</tr>)";

static const char page_path[] PROGMEM = R"(
  <tr><td>path:<td>%s</tr>)";

static const char page_node[] PROGMEM = R"(
  <tr><td>node:<td>%s</tr>)";

static const char page_key[] PROGMEM = R"(
  <tr><td>write key:<td>%s</tr>)";

static const char page_footer[] PROGMEM = R"(
</table><h2>Configuration Captive Portal:</h2>
<a href="/config">Start</a></body></html>)";

// Configs in memory.
char emoncms_server[MAX_PARAM_LEN] = "emonscms.org";
char emoncms_path[MAX_PARAM_LEN] = "";
char emoncms_node[MAX_PARAM_LEN] = "";
char emoncms_key[MAX_PARAM_LEN] = "";

// EmonCMS parameters.
WiFiManagerParameter emoncms_server_param("emoncms_server", "server", emoncms_server,
    MAX_PARAM_LEN);
WiFiManagerParameter emoncms_path_param("emoncms_path", "path", emoncms_path,
    MAX_PARAM_LEN);
WiFiManagerParameter emoncms_key_param("emoncms_key", "api key", emoncms_key,
    MAX_PARAM_LEN);
WiFiManagerParameter emoncms_node_param("emoncms_node", "node", emoncms_node,
    MAX_PARAM_LEN);

// Global state.
WiFiManager wifi_manager;
ESP8266WebServer server(80);
char reply_buffer[2048];
bool should_save_config = false;
char latest_data[512] = "";
unsigned long last_update = 0;

// Utility function to encode serialized json data.
String urlencode(String str) {
  String encodedString="";
  char c;
  char code0;
  char code1;
  for (int i =0; i < str.length(); i++){
    c=str.charAt(i);
    if (c == ' '){
      encodedString+= '+';
    } else if (isalnum(c)){
      encodedString+=c;
    } else{
      code1=(c & 0xf)+'0';
      if ((c & 0xf) >9){
        code1=(c & 0xf) - 10 + 'A';
      }
      c=(c>>4)&0xf;
      code0=c+'0';
      if (c > 9){
        code0=c - 10 + 'A';
      }
      encodedString+='%';
      encodedString+=code0;
      encodedString+=code1;
    }
    yield();
  }
  return encodedString;
}

// Callback notifying us of the need to save config.
void save_config_callback () {
  Serial.println("Should save config");
  should_save_config = true;
}

// Load config from flash.
void load_config() {
  Serial.println("Loaading config");
  if (!SPIFFS.exists("/config.json")) {
    Serial.println("Cannot find config file");
    return;
  }

  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  size_t size = file.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  file.readBytes(buf.get(), size);

  DynamicJsonDocument json(1024);
  auto deserializeError = deserializeJson(json, buf.get());
  serializeJson(json, Serial);
  if (!deserializeError) {
    Serial.println("Parsed json");
    strcpy(emoncms_server, json["emoncms_server"]);
    strcpy(emoncms_path, json["emoncms_path"]);
    strcpy(emoncms_node, json["emoncms_node"]);
    strcpy(emoncms_key, json["emoncms_key"]);
  } else {
    Serial.println("Failed to load json config");
  }
  file.close();
}

// Store config to flash.
void save_config() {
  Serial.println("Saving config");
  DynamicJsonDocument json(1024);
  json["emoncms_server"] = emoncms_server;
  json["emoncms_path"] = emoncms_path;
  json["emoncms_node"] = emoncms_node;
  json["emoncms_key"] = emoncms_key;

  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
  }
  serializeJson(json, Serial);
  serializeJson(json, file);
  file.close();
}

// Initialize web server.
void init_server() {
  server.begin();
  server.on("/config", config_handler);
  server.onNotFound(status_handler);
}

// Web handler for the main status page.
void status_handler() {
  char* buf = reply_buffer;
  buf += sprintf(buf, page_header, latest_data);
  buf += sprintf(buf, page_server, emoncms_server);
  buf += sprintf(buf, page_path, emoncms_path);
  buf += sprintf(buf, page_node, emoncms_node);
  buf += sprintf(buf, page_key, emoncms_key);
  buf += sprintf(buf, page_footer);
  server.send(200, "text/html", reply_buffer);
}

// Web handler for the config request. It starts the configuration portal for
// only 3 minutes.
void config_handler() {
  server.send(200, "text/plain", "Done: Connect to AP EmonPub.");
  server.stop();
  wifi_manager.setCaptivePortalEnable(true);
  wifi_manager.setConfigPortalTimeout(180);
  wifi_manager.startConfigPortal("EmonPub");
  init_server();
}

// Read from one emontx via the Wire library.
void read_from_wire(int device, JsonDocument &data) {
  String line;
  MyWire.requestFrom(device, BUFFER_LENGTH);
  int avail = MyWire.available();
  for (int i = 0; i < avail; i++) {
    line += (char) MyWire.read();
  }
  //Serial.println(String("From device ") + device + ": " + line);

  // Parse the CSV line (store it in `data`).
  line.trim(); // Get rid of any whitespace, newlines etc
  int len = line.length();
  if (len > 0) {
    for (int i = 0; i < len; i++) {
      String name = "";

      // Get the name
      while (i < len && line[i] != ':') {
        name += line[i++];
      }

      if (i++ >= len) {
        break;
      }

      // Get the value
      String value = "";
      while (i < len && line[i] != ',') {
        value += line[i++];
      }

      if (name.length() > 0 && value.length() > 0) {
        // IMPROVE: check that value is only a number, toDouble() will skip
        // white space and and chars after the number
        data[name] = value.toDouble();
      }
    }
  }
}

// Task to upload values to EmonCMS (every 5 seconds).
void emoncms_publish(JsonDocument &data) {
  Serial.println("Publishing to EmonCMS.");
  String serialized = "";
  serializeJson(data, serialized);
  Serial.println("Serialized value: " + serialized);

  String url = (
      String(emoncms_path) + "/input/post?fulljson=" + urlencode(serialized) +
      "&node=" + emoncms_node + "&apikey=" + emoncms_key);

  WiFiClientSecure client;
  client.setInsecure();
  client.connect(emoncms_server, 443);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
      "Host: " + emoncms_server + "\r\n" +
      "User-Agent: EmonPubESP8266\r\n" +
      "Connection: close\r\n\r\n");

  Serial.println("Request sent. Reply:");
  while (client.connected()) {
    Serial.println(client.readStringUntil('\n'));
  }
}

// Task to read values from emontx devices (every 5 seconds).
void emontx_read(JsonDocument &data) {
  Serial.println("Reading from emontx.");
  read_from_wire(10, data);
  read_from_wire(9, data);
  read_from_wire(8, data);
}

void setup() {
  // Initialize Wire.
  MyWire.begin();

  // We have it only for debugging purposes.
  Serial.begin(115200);

  // Wifi manager.
  wifi_manager.setSaveConfigCallback(save_config_callback);
  wifi_manager.addParameter(&emoncms_server_param);
  wifi_manager.addParameter(&emoncms_path_param);
  wifi_manager.addParameter(&emoncms_node_param);
  wifi_manager.addParameter(&emoncms_key_param);
  wifi_manager.setDarkMode(true);
  wifi_manager.setScanDispPerc(true);
  wifi_manager.setConfigPortalTimeout(180);
  wifi_manager.autoConnect("EmonPub");
  Serial.println("Connected");

  // Load new config values and save them if they have been modified.
  Serial.println("Mounting FS");
  if (SPIFFS.begin()) {
    if (should_save_config) {
      strcpy(emoncms_server, emoncms_server_param.getValue());
      strcpy(emoncms_path, emoncms_path_param.getValue());
      strcpy(emoncms_key, emoncms_key_param.getValue());
      strcpy(emoncms_node, emoncms_node_param.getValue());
      save_config();
    } else {
      load_config();
    }
  }

  // Normal operations web server.
  init_server();
}

void loop() {
  server.handleClient();

  // Update values every 5 seconds.
  if (last_update == 0 || (millis() - last_update > 5000)) {
    last_update = millis();
    StaticJsonDocument<1024> data;
    emontx_read(data);
    emoncms_publish(data);
    serializeJsonPretty(data, latest_data);
  }
}
