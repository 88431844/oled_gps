# NodeMCU D1 mini OLED GPS

Arduino IDE sketch for testing a NodeMCU/Wemos D1 mini with a 0.96 inch SSD1306 I2C OLED and a NEO-6M GPS module.

## Wiring

### OLED 0.96 I2C

| OLED | D1 mini |
| --- | --- |
| GND | GND |
| VCC | 3.3V |
| SCL | D1 / GPIO5 |
| SDA | D2 / GPIO4 |

### NEO-6M GPS

| GPS | D1 mini |
| --- | --- |
| GND | GND |
| VCC | 3.3V or 5V, depending on your module |
| TX | D5 / GPIO14 |
| RX | D6 / GPIO12, optional |

If you are not sure whether the GPS RX pin is 3.3V tolerant, leave GPS RX disconnected. GPS TX to D5 is enough for receiving location data.

## Arduino IDE setup

1. Install the ESP8266 board package.
2. Select `LOLIN(WEMOS) D1 R2 & mini` or another compatible D1 mini board.
3. Install these libraries from Library Manager:
   - `TinyGPSPlus`
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
4. Open `oled_gps.ino`, compile, and upload.
5. Open Serial Monitor at `115200` baud for debug logs.

## Behavior

- During GPS search, the OLED shows a running animation, uptime, parsed GPS byte count, and satellite count when available.
- If no GPS bytes arrive for more than 5 seconds, the OLED shows `No GPS data` to help diagnose wiring or power issues.
- After a valid GPS fix, the OLED shows satellite count, latitude, longitude, and Beijing time converted from GPS UTC time.

For first fix, place the GPS antenna outdoors or near a window with open sky. Cold start can take several minutes.
