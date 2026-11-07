/*
  ESP32 COMPACT ENVIRONMENTAL MONITORING DEVICE

  Hardware:
  - ESP32 WROOM
  - DHT22       : GPIO 27
  - DS18B20     : GPIO 18
  - SSD1306 OLED: I2C SDA 21, SCL 22
  - Push Button : GPIO 32

  Firebase:
  - FirebaseClient by Mobizt
  - No authentication (public database rules)
  - Upload interval: 15 minutes
  - Firebase server timestamp in Unix milliseconds

  DS18B20:
  - 10-bit resolution
  - Non-blocking conversion
*/

#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

#include <time.h>
#include <sys/time.h>

// =====================================================
// 1. WIFI CONFIGURATION
// =====================================================

#define WIFI_SSID       "3RTV"
#define WIFI_PASSWORD   "vv110668"

// =====================================================
// 2. FIREBASE CONFIGURATION
// =====================================================

#define DATABASE_URL "https://rhiza-project-default-rtdb.asia-southeast1.firebasedatabase.app/"

// =====================================================
// 3. PIN CONFIGURATION
// =====================================================

#define DHT_PIN       27
#define DHT_TYPE      DHT22

#define DS18B20_PIN   18

#define BUTTON_PIN    32

#define OLED_SDA      21
#define OLED_SCL      22

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C

// =====================================================
// 4. TIMING CONFIGURATION
// =====================================================

const unsigned long DHT_INTERVAL = 2500;

const unsigned long DS_INTERVAL = 2000;
const unsigned long DS_CONVERSION_TIME = 200;

const unsigned long BUTTON_DEBOUNCE = 50;

const unsigned long FIREBASE_INTERVAL = 300000UL; // 5 minutes

const unsigned long WIFI_RETRY_INTERVAL = 10000;

// =====================================================
// 5. SENSOR OBJECTS
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

OneWire oneWire(DS18B20_PIN);
DallasTemperature dsSensors(&oneWire);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// 6. FIREBASE OBJECTS
// =====================================================

WiFiClientSecure sslClient;

AsyncClientClass aClient(sslClient);

NoAuth noAuth;

FirebaseApp app;

RealtimeDatabase Database;

bool firebaseInitialized = false;

// =====================================================
// 7. SENSOR VARIABLES
// =====================================================

float temperatureDHT = NAN;
float humidityDHT = NAN;

float temperatureDS = NAN;

bool dhtValid = false;
bool dsValid = false;

// =====================================================
// 8. DISPLAY VARIABLES
// =====================================================

// false = DS18B20
// true  = DHT22

bool displayMode = true;

bool displayNeedsUpdate = true;

// =====================================================
// 9. SENSOR TIMERS
// =====================================================

unsigned long lastDHTRead = 0;

unsigned long lastDSRequest = 0;
unsigned long dsRequestStarted = 0;

bool dsConversionPending = false;

// =====================================================
// 10. BUTTON VARIABLES
// =====================================================

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChange = 0;

// =====================================================
// 11. FIREBASE TIMER
// =====================================================

unsigned long lastFirebaseUpload = 0;

// =====================================================
// 12. WIFI TIMER
// =====================================================

unsigned long lastWiFiAttempt = 0;

// =====================================================
// 13. FUNCTION DECLARATIONS
// =====================================================

void connectWiFi();

void initializeFirebase();

void maintainWiFi();

void readDHT22();

void startDS18B20();

void readDS18B20();

void checkButton();

void updateDisplay();

void uploadToFirebase();

void firebaseCallback(AsyncResult &result);

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("============================");
  Serial.println("   ESP32 SENSOR SYSTEM");
  Serial.println("============================");

  Serial.println("INITIALIZING...");

  // -----------------------------------------------
  // BUTTON
  // -----------------------------------------------

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // -----------------------------------------------
  // OLED
  // -----------------------------------------------

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println("OLED INITIALIZATION FAILED!");

    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("ESP32 SENSOR SYSTEM");
  display.println("Initializing...");
  display.display();

  // -----------------------------------------------
  // DHT22
  // -----------------------------------------------

  dht.begin();

  Serial.println("DHT22 READY");

  // -----------------------------------------------
  // DS18B20
  // -----------------------------------------------

  dsSensors.begin();

  dsSensors.setResolution(10);

  dsSensors.setWaitForConversion(false);

  Serial.println("DS18B20 READY");

  // -----------------------------------------------
  // INITIAL SENSOR READ
  // -----------------------------------------------

  readDHT22();

  startDS18B20();

  // -----------------------------------------------
  // WIFI
  // -----------------------------------------------

  connectWiFi();

  // -----------------------------------------------
  // NTP TIME SYNCHRONIZATION
  // -----------------------------------------------

  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;

  if (getLocalTime(&timeinfo, 10000)) {
    Serial.println("NTP TIME SYNCHRONIZED!");
  } else {
    Serial.println("NTP TIME SYNCHRONIZATION FAILED!");
  }

  // -----------------------------------------------
  // FIREBASE
  // -----------------------------------------------

  initializeFirebase();

  // Start the 15-minute interval from setup.
  lastFirebaseUpload = millis();

  Serial.println("SYSTEM READY");

  displayNeedsUpdate = true;
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // Maintain Firebase background tasks.
  app.loop();

  // Maintain Wi-Fi connection.
  maintainWiFi();

  // Check button.
  checkButton();

  unsigned long currentMillis = millis();

  // -----------------------------------------------
  // DHT22 READING
  // -----------------------------------------------

  if (currentMillis - lastDHTRead >= DHT_INTERVAL) {

    lastDHTRead = currentMillis;

    readDHT22();
  }

  // -----------------------------------------------
  // DS18B20 CONVERSION
  // -----------------------------------------------

  if (dsConversionPending) {

    if (currentMillis - dsRequestStarted >=
        DS_CONVERSION_TIME) {

      readDS18B20();

      dsConversionPending = false;
    }
  }

  // Start the next DS18B20 conversion.
  if (!dsConversionPending &&
      currentMillis - lastDSRequest >= DS_INTERVAL) {

    startDS18B20();
  }

  // -----------------------------------------------
  // FIREBASE UPLOAD
  // -----------------------------------------------

  if (currentMillis - lastFirebaseUpload >=
      FIREBASE_INTERVAL) {

    if (WiFi.status() == WL_CONNECTED &&
        app.ready()) {

      lastFirebaseUpload = currentMillis;

      uploadToFirebase();
    }
  }

  // -----------------------------------------------
  // OLED
  // -----------------------------------------------

  if (displayNeedsUpdate) {

    updateDisplay();

    displayNeedsUpdate = false;
  }
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {

  Serial.println();
  Serial.println("CONNECTING TO WIFI...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");

    delay(300);
  }

  Serial.println();

  Serial.println("WIFI CONNECTED!");

  Serial.print("IP ADDRESS: ");

  Serial.println(WiFi.localIP());
}

// =====================================================
// WIFI RECONNECTION
// =====================================================

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWiFiAttempt >=
      WIFI_RETRY_INTERVAL) {

    lastWiFiAttempt = millis();

    Serial.println("WIFI DISCONNECTED!");

    Serial.println("RECONNECTING...");

    WiFi.reconnect();
  }
}

// =====================================================
// FIREBASE INITIALIZATION
// =====================================================

void initializeFirebase() {

  Serial.println();
  Serial.println("INITIALIZING FIREBASE...");

  Serial.print("Firebase Client Version: ");

  Serial.println(FIREBASE_CLIENT_VERSION);

  // Skip certificate verification.
  // Suitable for testing, but use certificate
  // verification in a production deployment.

  sslClient.setInsecure();

  // Configure timeout for ESP32 Core 3.x.
  sslClient.setConnectionTimeout(10000);

  sslClient.setHandshakeTimeout(15);

  // Initialize unauthenticated Firebase access.
  initializeApp(
    aClient,
    app,
    getAuth(noAuth)
  );

  // Bind Firebase Realtime Database.
  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);

  firebaseInitialized = true;

  Serial.println("FIREBASE INITIALIZED");
}

// =====================================================
// DHT22 READING
// =====================================================

void readDHT22() {

  float newTemperature = dht.readTemperature();

  float newHumidity = dht.readHumidity();

  if (isnan(newTemperature) || isnan(newHumidity)) {

    dhtValid = false;

    Serial.println("DHT22 ERROR!");

  } else {

    temperatureDHT = newTemperature;

    humidityDHT = newHumidity;

    dhtValid = true;

    Serial.println("DHT22:");

    Serial.print("Temperature: ");

    Serial.print(temperatureDHT, 2);

    Serial.println(" C");

    Serial.print("Humidity: ");

    Serial.print(humidityDHT, 2);

    Serial.println(" %");
  }

  if (displayMode) {

    displayNeedsUpdate = true;
  }
}

// =====================================================
// START DS18B20 CONVERSION
// =====================================================

void startDS18B20() {

  dsSensors.requestTemperatures();

  dsConversionPending = true;

  dsRequestStarted = millis();

  lastDSRequest = millis();
}

// =====================================================
// READ DS18B20
// =====================================================

void readDS18B20() {

  float newTemperature =
    dsSensors.getTempCByIndex(0);

  if (newTemperature == DEVICE_DISCONNECTED_C ||
      isnan(newTemperature) ||
      newTemperature < -55 ||
      newTemperature > 125) {

    dsValid = false;

    Serial.println("DS18B20 ERROR!");

  } else {

    temperatureDS = newTemperature;

    dsValid = true;

    Serial.print("DS18B20: ");

    Serial.print(temperatureDS, 2);

    Serial.println(" C");
  }

  if (!displayMode) {

    displayNeedsUpdate = true;
  }
}

// =====================================================
// BUTTON HANDLING
// =====================================================

void checkButton() {

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {

    lastButtonChange = millis();

    lastButtonReading = reading;
  }

  if (millis() - lastButtonChange >=
      BUTTON_DEBOUNCE) {

    if (reading != stableButtonState) {

      stableButtonState = reading;

      // Active LOW button press.
      if (stableButtonState == LOW) {

        displayMode = !displayMode;

        displayNeedsUpdate = true;

        Serial.println();

        if (displayMode) {

          Serial.println("DISPLAY MODE: DHT22");

        } else {

          Serial.println("DISPLAY MODE: DS18B20");
        }
      }
    }
  }
}

// =====================================================
// OLED DISPLAY
// =====================================================

void updateDisplay() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);

  display.println("ENVIRONMENT MONITOR");

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  // -----------------------------------------------
  // DHT22 DISPLAY
  // -----------------------------------------------

  if (displayMode) {

    display.setCursor(0, 17);

    display.println("DHT22 SENSOR");

    display.setCursor(0, 31);

    display.print("Temperature: ");

    if (dhtValid) {

      display.print(temperatureDHT, 1);

      display.println(" C");

    } else {

      display.println("ERROR");
    }

    display.setCursor(0, 43);

    display.print("Humidity: ");

    if (dhtValid) {

      display.print(humidityDHT, 1);

      display.println(" %");

    } else {

      display.println("ERROR");
    }
  }

  // -----------------------------------------------
  // DS18B20 DISPLAY
  // -----------------------------------------------

  else {

    display.setCursor(0, 17);

    display.println("DS18B20 SENSOR");

    display.setCursor(0, 34);

    display.print("Temperature: ");

    if (dsValid) {

      display.print(temperatureDS, 2);

      display.println(" C");

    } else {

      display.println("ERROR");
    }
  }

  display.drawLine(0, 53, 127, 53, SSD1306_WHITE);

  display.setCursor(0, 56);

  display.print("WiFi: ");

  if (WiFi.status() == WL_CONNECTED) {

    display.print("CONNECTED");

  } else {

    display.print("OFFLINE");
  }

  display.display();
}

// =====================================================
// FIREBASE UPLOAD
// =====================================================

void uploadToFirebase() {

  if (!firebaseInitialized || !app.ready()) {

    Serial.println("FIREBASE NOT READY!");

    return;
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WIFI NOT CONNECTED!");

    return;
  }

  Serial.println();

  Serial.println("============================");

  Serial.println("FIREBASE UPLOAD");

  Serial.println("============================");

  // -----------------------------------------------
  // GENERATE SHARED TIMESTAMPS
  // -----------------------------------------------

  struct timeval tv;

  if (gettimeofday(&tv, nullptr) != 0 || tv.tv_sec < 1700000000) {
    Serial.println("INVALID SYSTEM TIME!");
    return;
  }

  // Unix timestamp in milliseconds
  uint64_t timestampUnixMs =
      ((uint64_t)tv.tv_sec * 1000ULL) +
      (tv.tv_usec / 1000);

  // Convert to local time (UTC+8)
  struct tm timeinfo;
  localtime_r(&tv.tv_sec, &timeinfo);

  // Human-readable timestamp
  char timestampBuffer[40];

  strftime(
    timestampBuffer,
    sizeof(timestampBuffer),
    "%Y-%m-%d %H:%M:%S",
    &timeinfo
  );

  char timestampReadable[50];

  snprintf(
    timestampReadable,
    sizeof(timestampReadable),
    "%s.%03ld +08:00",
    timestampBuffer,
    (long)(tv.tv_usec / 1000)
  );

  // Firebase JSON writer
  JsonWriter writer;

  // History key: YYYY-MM-DD_HH-MM-SS
  char historyKey[30];

  strftime(
    historyKey,
    sizeof(historyKey),
    "%Y-%m-%d_%H-%M-%S",
    &timeinfo
  );

  Serial.print("Unix Timestamp: ");
  Serial.println((unsigned long long)timestampUnixMs);

  Serial.print("Readable Timestamp: ");
  Serial.println(timestampReadable);

  // -----------------------------------------------
  // DHT22 JSON
  // -----------------------------------------------

  object_t dhtStatus;
  object_t dhtTemperature;
  object_t dhtHumidity;

  object_t dhtJSON;

  writer.create(
    dhtStatus,
    "Status",
    string_t(dhtValid ? "OK" : "ERROR")
  );

  if (dhtValid) {

    writer.create(
      dhtTemperature,
      "Temperature",
      number_t(temperatureDHT, 2)
    );

    writer.create(
      dhtHumidity,
      "Humidity",
      number_t(humidityDHT, 2)
    );

    writer.join(
      dhtJSON,
      3,
      dhtTemperature,
      dhtHumidity,
      dhtStatus
    );

  } else {

    writer.join(
      dhtJSON,
      1,
      dhtStatus
    );
  }

  // -----------------------------------------------
  // DS18B20 JSON
  // -----------------------------------------------

  object_t dsStatus;
  object_t dsTemperature;

  object_t dsJSON;

  writer.create(
    dsStatus,
    "Status",
    string_t(dsValid ? "OK" : "ERROR")
  );

  if (dsValid) {

    writer.create(
      dsTemperature,
      "Temperature",
      number_t(temperatureDS, 2)
    );

    writer.join(
      dsJSON,
      2,
      dsTemperature,
      dsStatus
    );

  } else {

    writer.join(
      dsJSON,
      1,
      dsStatus
    );
  }

  // -----------------------------------------------
  // COMBINE SENSOR DATA AND TIMESTAMPS
  // -----------------------------------------------

  object_t dhtNode;
  object_t dsNode;

  object_t unixTimestampNode;
  object_t readableTimestampNode;

  object_t finalJSON;

  // Sensor nodes
  writer.create(dhtNode, "DHT22", dhtJSON);
  writer.create(dsNode, "DS18B20", dsJSON);

  // Shared timestamp nodes
  writer.create(
    unixTimestampNode,
    "TimestampUnixMs",
    number_t((double)timestampUnixMs, 0)
  );

  writer.create(
    readableTimestampNode,
    "TimestampReadable",
    string_t(timestampReadable)
  );

  // Combine everything into one JSON object
  writer.join(
    finalJSON,
    4,
    unixTimestampNode,
    readableTimestampNode,
    dhtNode,
    dsNode
  );

  // -----------------------------------------------
  // UPDATE LATEST SENSOR DATA
  // -----------------------------------------------

  Serial.println("Updating latest sensor data...");

  Database.set<object_t>(
    aClient,
    "/Sensors",
    finalJSON,
    firebaseCallback,
    "sensorUpload"
  );

  // -----------------------------------------------
  // SAVE HISTORICAL SENSOR DATA
  // -----------------------------------------------

  Serial.println("Saving historical sensor data...");

  String historyPath =
      "/SensorHistory/" + String(historyKey);

  Database.set<object_t>(
    aClient,
    historyPath,
    finalJSON,
    firebaseCallback,
    "sensorHistoryUpload"
  );

  Serial.print("History path: ");
  Serial.println(historyPath);
}
// =====================================================
// FIREBASE CALLBACK
// =====================================================

void firebaseCallback(AsyncResult &result) {

  if (result.isError()) {

    Serial.println();

    Serial.println("FIREBASE UPLOAD FAILED!");

    Serial.print("Error: ");

    Serial.println(result.error().message());

    Serial.print("Error code: ");

    Serial.println(result.error().code());
  }

  if (result.isEvent()) {

    Serial.print("Firebase event: ");

    Serial.println(result.eventLog().message());
  }

  if (result.isDebug()) {

    Serial.print("Firebase debug: ");

    Serial.println(result.debug());
  }

  if (result.available()) {

    Serial.println();

    Serial.println("FIREBASE RESPONSE:");

    Serial.println(result.c_str());

    Serial.println("UPLOAD RESPONSE RECEIVED");
  }
}