#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiManager.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
#define BUTTON_PIN 0 // change to 14 if needed

#include <LittleFS.h>
#include <ArduinoJson.h>

struct Config {
  String ssid;
  String password;
  String portainerURL;
  String portainerToken;
  String haURL;
  String haToken;
  bool vpnEnabled;
};

Config config;


TwoWire OLED_Wire = TwoWire(1);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &OLED_Wire, OLED_RESET);

String menuItems[] = {
  "Device Stats",
  "Wi-Fi Tools",
  "Service Status",
  "Home Control",
  "Utilities"
};

const int MENU_COUNT = sizeof(menuItems) / sizeof(menuItems[0]);
int currentIndex = 0;
bool inSubmenu = false;
unsigned long buttonPressTime = 0;
bool buttonHeld = false;

bool loadConfig() {
  if (!LittleFS.begin()) {
  Serial.println("LittleFS mount failed, formatting...");
  LittleFS.format();  // ⚠️ nukes the FS
  if (!LittleFS.begin()) {
    Serial.println("Format failed too. FS dead?");
    return false;
  }
}

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount FS");
    return false;
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Config file not found.");
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse config");
    return false;
  }

  config.ssid = doc["wifi"]["ssid"].as<String>();
  config.password = doc["wifi"]["password"].as<String>();
  config.portainerURL = doc["portainer"]["url"].as<String>();
  config.portainerToken = doc["portainer"]["token"].as<String>();
  config.haURL = doc["ha"]["url"].as<String>();
  config.haToken = doc["ha"]["token"].as<String>();
  config.vpnEnabled = doc["vpn"]["enabled"].as<bool>();

  Serial.println("Config loaded successfully");
  return true;
}
bool saveConfig() {
  StaticJsonDocument<1024> doc;

  doc["wifi"]["ssid"] = config.ssid;
  doc["wifi"]["password"] = config.password;
  doc["portainer"]["url"] = config.portainerURL;
  doc["portainer"]["token"] = config.portainerToken;
  doc["ha"]["url"] = config.haURL;
  doc["ha"]["token"] = config.haToken;
  doc["vpn"]["enabled"] = config.vpnEnabled;

  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("Config saved.");
  return true;
}


void setup() {
  Serial.begin(115200);
  OLED_Wire.begin(18, 17);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init failed");
    while (true);
  }

  displayMenu();
  
}
  void connectToWiFi() {
    WiFiManager wm;

    if (!loadConfig()) {
      Serial.println("No config found. Starting WiFi portal...");
      wm.autoConnect("LoRa");
      config.ssid = WiFi.SSID();
      config.password = WiFi.psk();
      saveConfig();  // store new values
    } else {
      WiFi.begin(config.ssid.c_str(), config.password.c_str());
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Failed. Starting portal...");
        wm.autoConnect("LoRa");
        config.ssid = WiFi.SSID();
        config.password = WiFi.psk();
        saveConfig();
      }
    }
  }

void loop() {
  static bool buttonPressed = false;

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressTime = millis();
    } else if (!buttonHeld && millis() - buttonPressTime > 700) {
      buttonHeld = true;
      handleLongPress();
    }
  } else {
    if (buttonPressed && !buttonHeld) {
      handleShortPress();
    }
    buttonPressed = false;
    buttonHeld = false;
  }

  delay(50);
}

void handleShortPress() {
  if (!inSubmenu) {
    currentIndex = (currentIndex + 1) % MENU_COUNT;
    displayMenu();
  }
}

void handleLongPress() {
  if (!inSubmenu) {
    inSubmenu = true;
    launchFeature(menuItems[currentIndex]);
  } else {
    inSubmenu = false;
    displayMenu();
  }
}

void displayMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < MENU_COUNT; i++) {
    if (i == currentIndex) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(menuItems[i]);
  }

  display.display();
}

void launchFeature(String name) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Launching:");
  display.println(name);
  display.display();

  delay(500);

  // Placeholder for real features
  if (name == "Device Stats") runDeviceStats();
  else if (name == "Wi-Fi Tools") runWiFiTools();
  else if (name == "Service Status") runServiceStatus();
  else if (name == "Home Control") runHomeControl();
  else if (name == "Utilities") runUtilities();
}

void runDeviceStats() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Uptime:");
  display.printf("%lus\n", millis() / 1000);
  display.println("\nHold to go back");
  display.display();
}

void runWiFiTools() {
  connectToWiFi();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Scanning Wi-Fi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();

  display.clearDisplay();
  if (n == 0) {
    display.println("No networks found");
  } else {
    display.printf("%d networks:\n", n);
    for (int i = 0; i < n && i < 5; ++i) {
      display.printf("%d: %s\nRSSI: %d dBm\n\n", i + 1,
                     WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  }

  display.println("Hold to go back");
  display.display();
}

void runServiceStatus() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Infra Monitor");
  display.println("Coming soon...");
  display.display();
}

void runHomeControl() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Home Assistant");
  display.println("Coming soon...");
  display.display();
}

void runUtilities() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("System Tools");
  display.println("Coming soon...");
  display.display();
}
