#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

const uint8_t OLED_SDA_PIN = D2;
const uint8_t OLED_SCL_PIN = D1;
const uint8_t GPS_RX_PIN = D5;  // D1 mini RX pin: connect to NEO-6M TX
const uint8_t GPS_TX_PIN = D6;  // D1 mini TX pin: connect to NEO-6M RX, optional
const uint32_t GPS_BAUD = 9600;
const uint32_t SERIAL_BAUD = 115200;
const uint32_t DISPLAY_INTERVAL_MS = 1000;
const uint32_t NO_GPS_DATA_WARNING_MS = 5000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;

uint32_t lastDisplayUpdate = 0;
uint32_t lastGpsByteAt = 0;
uint32_t gpsBytesRead = 0;
bool oledReady = false;

struct BeijingTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  bool valid;
};

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (month == 2 && isLeapYear(year)) {
    return 29;
  }

  return days[month - 1];
}

BeijingTime getBeijingTime() {
  BeijingTime beijingTime;
  beijingTime.valid = false;

  if (!gps.date.isValid() || !gps.time.isValid()) {
    return beijingTime;
  }

  beijingTime.year = gps.date.year();
  beijingTime.month = gps.date.month();
  beijingTime.day = gps.date.day();
  beijingTime.hour = gps.time.hour() + 8;
  beijingTime.minute = gps.time.minute();
  beijingTime.second = gps.time.second();

  if (beijingTime.hour >= 24) {
    beijingTime.hour -= 24;
    beijingTime.day++;

    if (beijingTime.day > daysInMonth(beijingTime.year, beijingTime.month)) {
      beijingTime.day = 1;
      beijingTime.month++;

      if (beijingTime.month > 12) {
        beijingTime.month = 1;
        beijingTime.year++;
      }
    }
  }

  beijingTime.valid = true;
  return beijingTime;
}

const char *courseToCardinal(double courseDegrees) {
  static const char *directions[] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
  };

  uint8_t index = (uint8_t)((courseDegrees + 22.5) / 45.0) % 8;
  return directions[index];
}

void drawCenteredText(const char *text, int16_t y, uint8_t textSize) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  display.setCursor((SCREEN_WIDTH - width) / 2, y);
  display.print(text);
}

void drawProgressBar(uint8_t progressStep) {
  const int16_t x = 0;
  const int16_t y = 56;
  const int16_t width = 128;
  const int16_t height = 8;
  const int16_t segmentWidth = 20;
  const int16_t maxOffset = width - segmentWidth - 2;
  const int16_t offset = (progressStep * 8) % maxOffset;

  display.drawRect(x, y, width, height, SSD1306_WHITE);
  display.fillRect(x + 1 + offset, y + 1, segmentWidth, height - 2, SSD1306_WHITE);
}

void showBootScreen() {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("OLED GPS", 0, 2);
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.println("OLED: OK 0x3C");
  display.println("GPS:  NEO-6M");
  display.print("Baud: ");
  display.println(GPS_BAUD);
  display.println("I2C: SDA D2 SCL D1");
  display.display();
}

void showSearchingScreen() {
  uint32_t uptimeSeconds = millis() / 1000;
  uint8_t satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
  bool noGpsData = gpsBytesRead == 0 || (millis() - lastGpsByteAt > NO_GPS_DATA_WARNING_MS);
  uint8_t animationStep = uptimeSeconds % 4;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Searching GPS");
  for (uint8_t i = 0; i < animationStep; i++) {
    display.print('.');
  }

  display.setCursor(0, 12);
  display.print("Sat: ");
  if (gps.satellites.isValid()) {
    display.print(satellites);
  } else {
    display.print("--");
  }

  display.setCursor(64, 12);
  display.print("Run: ");
  display.print(uptimeSeconds);
  display.print('s');

  display.setCursor(0, 24);
  display.print("Chars: ");
  display.print(gpsBytesRead);

  display.setCursor(0, 36);
  if (noGpsData) {
    display.print("No GPS data");
    display.setCursor(0, 46);
    display.print("Check TX/RX & power");
  } else {
    display.print("Receiving NMEA data");
    display.setCursor(0, 46);
    display.print("Move to open sky");
  }

  drawProgressBar(uptimeSeconds);
  display.display();
}

void showFixedScreen() {
  BeijingTime beijingTime = getBeijingTime();
  bool courseReady = gps.course.isValid() && gps.course.age() < 5000 && gps.speed.kmph() >= 1.0;
  bool speedReady = gps.speed.isValid() && gps.speed.age() < 5000;
  double courseDegrees = courseReady ? gps.course.deg() : 0.0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (courseReady) {
    display.print(courseToCardinal(courseDegrees));
  } else {
    display.print("--");
  }

  display.setTextSize(1);
  display.setCursor(58, 0);
  display.print("Sat:");
  if (gps.satellites.isValid()) {
    display.print(gps.satellites.value());
  } else {
    display.print("--");
  }

  display.setCursor(0, 20);
  display.print("Course: ");
  if (courseReady) {
    display.print(courseDegrees, 1);
    display.print((char)247);
  } else {
    display.print("move >1km/h");
  }

  display.setCursor(0, 32);
  display.print("Speed: ");
  if (speedReady) {
    display.print(gps.speed.kmph(), 1);
    display.print(" km/h");
  } else {
    display.print("--.- km/h");
  }

  display.setCursor(0, 44);
  if (beijingTime.valid) {
    char timeLine[24];
    snprintf(timeLine, sizeof(timeLine), "BJT %02d:%02d:%02d", beijingTime.hour, beijingTime.minute, beijingTime.second);
    display.print(timeLine);
  } else {
    display.print("Time syncing...");
  }

  display.setCursor(0, 56);
  display.print("N E S W by movement");

  display.display();
}

void updateDisplay() {
  if (!oledReady || millis() - lastDisplayUpdate < DISPLAY_INTERVAL_MS) {
    return;
  }

  lastDisplayUpdate = millis();

  if (gps.location.isValid() && gps.location.age() < 5000) {
    showFixedScreen();
  } else {
    showSearchingScreen();
  }
}

void printGpsStatus() {
  static uint32_t lastLogAt = 0;

  if (millis() - lastLogAt < DISPLAY_INTERVAL_MS) {
    return;
  }

  lastLogAt = millis();
  Serial.print("bytes=");
  Serial.print(gpsBytesRead);
  Serial.print(" chars=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sat=");
  if (gps.satellites.isValid()) {
    Serial.print(gps.satellites.value());
  } else {
    Serial.print("--");
  }
  Serial.print(" fix=");
  Serial.print(gps.location.isValid() ? "yes" : "no");

  if (gps.location.isValid()) {
    Serial.print(" lat=");
    Serial.print(gps.location.lat(), 6);
    Serial.print(" lng=");
    Serial.print(gps.location.lng(), 6);
  }

  Serial.println();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println();
  Serial.println("OLED GPS starting...");

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (!oledReady) {
    Serial.println("OLED init failed. Check address 0x3C/0x3D and SDA/SCL wiring.");
  } else {
    Serial.println("OLED init OK.");
    showBootScreen();
  }

  gpsSerial.begin(GPS_BAUD);
  lastGpsByteAt = millis();
  Serial.println("GPS serial started at 9600 baud.");
  Serial.println("Wiring: GPS TX -> D5, GPS RX -> D6 optional, OLED SDA -> D2, SCL -> D1.");

  delay(2000);
  lastDisplayUpdate = 0;
}

void loop() {
  while (gpsSerial.available() > 0) {
    char incoming = gpsSerial.read();
    gps.encode(incoming);
    gpsBytesRead++;
    lastGpsByteAt = millis();
  }

  updateDisplay();
  printGpsStatus();
}
