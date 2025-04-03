#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "ceauto00_plus";
const char* password = "@ceauto00";

// GitHub URL for the version file and .bin firmware file
const char* versionURL = "https://raw.githubusercontent.com/yourusername/yourrepo/main/version.json";
const char* firmwareURL = "https://github.com/yourusername/yourrepo/releases/download/v1.0.0/firmware.bin";

// EEPROM settings
#define EEPROM_SIZE 512
#define VERSION_ADDRESS 0

String currentVersion = "1.0.0";  // This will be the version of the firmware you're checking
String storedVersion;

// Function to check the version on GitHub
bool checkForUpdate() {
  HTTPClient http;
  http.begin(versionURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);  // Allocate memory for parsing JSON
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("Failed to parse JSON");
      return false;
    }
    
    String latestVersion = doc["version"];  // Assuming the JSON contains a key "version"

    if (latestVersion != storedVersion) {
      return true;
    }
  } else {
    Serial.println("Failed to fetch version info");
  }
  http.end();
  return false;
}

// Function to update the firmware via OTA
void updateFirmware() {
  Serial.println("Starting OTA update...");
  ArduinoOTA.begin();  // Start the OTA process
}

void setup() {
  Serial.begin(9600);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Read the stored version from EEPROM
  storedVersion = "";
  for (int i = 0; i < 10; i++) {
    storedVersion += char(EEPROM.read(VERSION_ADDRESS + i));
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    timeout++;
    if (timeout > 30) {  // Timeout after 30 seconds
      Serial.println("Failed to connect to WiFi.");
      return;
    }
  }
  Serial.println("Connected to WiFi");

  // Check for updates
  if (checkForUpdate()) {
    Serial.println("New firmware found! Updating...");
    updateFirmware();
  } else {
    Serial.println("No update required.");
  }
}

void loop() {
  // Handle OTA updates during operation
  ArduinoOTA.handle();

  // Print the stored version to the Serial Monitor
  Serial.println("Hello from version " + storedVersion);
}
