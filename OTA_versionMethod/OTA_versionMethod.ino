#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "ceauto00_plus";
const char* password = "@ceauto00";

// GitHub URLs
const char* versionURL = "https://raw.githubusercontent.com/Hamshary99/OTA_versionMethod/refs/heads/main/OTA_versionMethod/version.json";
const char* firmwareURL = "https://raw.githubusercontent.com/Hamshary99/OTA_versionMethod/refs/heads/main/OTA_versionMethod/build/esp32.esp32.esp32doit-devkit-v1/OTA_versionMethod.ino.bin";

/// EEPROM settings
#define EEPROM_SIZE 512
#define VERSION_ADDRESS 0
String currentVersion = "1.0.0";  
String storedVersion;

// Use the LED pin
#define LED_PIN 2  // Most ESP32 dev boards use GPIO 2 for built-in LED

// Update check interval (in milliseconds)
const unsigned long CHECK_INTERVAL = 24 * 60 * 60 * 1000; // Check once per day
unsigned long lastCheckTime = 0;

// Function to check if EEPROM has been initialized with version data
bool isEEPROMValid() {
  // Check first few bytes to see if they contain reasonable ASCII characters
  for (int i = 0; i < 5; i++) {
    byte value = EEPROM.read(VERSION_ADDRESS + i);
    // Check if value is a digit, dot, or null terminator
    if (!(isdigit(value) || value == '.' || value == 0)) {
      return false;
    }
    // If we hit a null terminator early and there are at least 3 characters (e.g., "1.0")
    // then we can consider it valid
    if (value == 0 && i >= 3) {
      return true;
    }
  }
  return true;
}

void readEEPROMVersion() {
  // First check if EEPROM has valid data
  if (!isEEPROMValid()) {
    Serial.println("EEPROM not initialized with version data - initializing now");
    writeEEPROMVersion(currentVersion);
    storedVersion = currentVersion;
    return;
  }
  
  // Read version from EEPROM
  char buffer[16] = {0};  // Initialize all to zero
  for (int i = 0; i < 15; i++) {
    byte value = EEPROM.read(VERSION_ADDRESS + i);
    // Stop if we hit a null terminator or non-printable character
    if (value == 0 || value > 127) {
      break;
    }
    buffer[i] = (char)value;
  }
  
  storedVersion = String(buffer);
  
  // Verify the string looks like a version (contains digits and dots)
  bool isValid = false;
  for (int i = 0; i < storedVersion.length(); i++) {
    if (isdigit(storedVersion[i]) || storedVersion[i] == '.') {
      isValid = true;
      break;
    }
  }
  
  if (!isValid || storedVersion.length() < 3) {
    Serial.println("Invalid version format in EEPROM. Resetting to default.");
    writeEEPROMVersion(currentVersion);
    storedVersion = currentVersion;
  } else {
    Serial.println("Stored Version: " + storedVersion);
  }
}

void writeEEPROMVersion(String version) {
  Serial.println("Writing version to EEPROM: " + version);
  
  // First erase the version area
  for (int i = 0; i < 16; i++) {
    EEPROM.write(VERSION_ADDRESS + i, 0);
  }
  
  // Then write the new version
  for (int i = 0; i < version.length(); i++) {
    EEPROM.write(VERSION_ADDRESS + i, version[i]);
  }
  // Ensure null termination
  EEPROM.write(VERSION_ADDRESS + version.length(), 0);
  
  // Commit changes to flash
  if (EEPROM.commit()) {
    Serial.println("EEPROM write successful");
  } else {
    Serial.println("ERROR: EEPROM commit failed");
  }
}

// Function to compare version strings (simple semver comparison)
bool isNewerVersion(String newVersion, String currentVersion) {
  // Split versions into major.minor.patch components
  int newMajor = 0, newMinor = 0, newPatch = 0;
  int curMajor = 0, curMinor = 0, curPatch = 0;
  
  sscanf(newVersion.c_str(), "%d.%d.%d", &newMajor, &newMinor, &newPatch);
  sscanf(currentVersion.c_str(), "%d.%d.%d", &curMajor, &curMinor, &curPatch);
  
  // Compare major version first
  if (newMajor > curMajor) return true;
  if (newMajor < curMajor) return false;
  
  // Major versions are equal, compare minor versions
  if (newMinor > curMinor) return true;
  if (newMinor < curMinor) return false;
  
  // Minor versions are equal, compare patch versions
  return (newPatch > curPatch);
}

// Function to check for updates
bool checkForUpdate() {
  HTTPClient http;
  
  Serial.println("Checking for firmware updates...");
  http.begin(versionURL);
  http.setUserAgent("ESP32-HTTP-Update");
  
  int httpCode = http.GET();
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Received JSON: " + payload);  
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON Parsing Error: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }
    
    String latestVersion = doc["version"];
    Serial.println("Latest Version: " + latestVersion);
    Serial.println("Current Version: " + storedVersion);
    
    // Check if the new version is actually newer
    if (isNewerVersion(latestVersion, storedVersion)) {
      // Store the full firmware URL for update
      String fullFirmwareURL = String(firmwareURL) + "v" + latestVersion + "/firmware.bin";
      Serial.println("New firmware available at: " + fullFirmwareURL);
      
      // Update the stored version after successful check but before update
      // This allows us to preserve the target version even if update fails
      writeEEPROMVersion(latestVersion);
      
      http.end();
      return true;
    } else {
      Serial.println("Current firmware is up to date.");
    }
  } else {
    Serial.println("Failed to fetch version info");
  }
  
  http.end();
  return false;
}

// Function to update firmware
void updateFirmware(String newVersion) {
  // Construct the full firmware URL
  String fullFirmwareURL = String(firmwareURL) + "v" + newVersion + "/firmware.bin";
  Serial.println("Starting firmware update from: " + fullFirmwareURL);
  
  // Turn on LED to indicate update in progress
  digitalWrite(LED_PIN, HIGH);
  
  // Configure HTTP update
  httpUpdate.setLedPin(LED_PIN, LOW);
  
  // Add a small delay to allow serial output to complete
  delay(100);
  
  // Create WiFiClient for HTTPUpdate
  WiFiClient client;
  
  // Call update with proper parameters
  t_httpUpdate_return ret = httpUpdate.update(client, fullFirmwareURL, storedVersion);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP Update Failed - Error (%d): %s\n", 
                   httpUpdate.getLastError(),
                   httpUpdate.getLastErrorString().c_str());
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update needed");
      break;
      
    case HTTP_UPDATE_OK:
      Serial.println("Update successful! Rebooting...");
      // The device will restart automatically after successful update
      break;
  }
  
  // Turn off LED if we reach here (meaning update likely failed)
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n");
  Serial.println("ESP32 OTA Updater Starting");
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
    // Flash LED to indicate error
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  // Read stored version
  readEEPROMVersion();
  
  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    timeout++;
    if (timeout > 30) {
      Serial.println("\nWiFi Connection Failed. Will try again later.");
      return;
    }
  }
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Check for updates on startup
  if (checkForUpdate()) {
    Serial.println("New firmware found! Updating...");
    updateFirmware(storedVersion);
  } else {
    Serial.println("No update required.");
  }
}

void loop() {
  // Periodically check for updates
  unsigned long currentTime = millis();
  
  // Check if it's time to check for updates again or if millis() wrapped around
  if ((currentTime - lastCheckTime >= CHECK_INTERVAL) || (currentTime < lastCheckTime)) {
    if (WiFi.status() == WL_CONNECTED) {
      if (checkForUpdate()) {
        updateFirmware(storedVersion);
      }
    } else {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.begin(ssid, password);
    }
    lastCheckTime = currentTime;
  }
  
  // Your application's main code goes here
  Serial.println("Running version: " + storedVersion);
  delay(10000);  // Print status every 10 seconds
}