# ESP32 Room monitor
Code for monitoring room temperature, humidity, etc with ESP32.

This repository contains a ESP-IDF built code that reads different sensors and publishes readings via MQTT.
The device can be connected to WiFi using the EspTouch application.

## Compiling
Use the ESP-IDF to compile the code.

```bash
. $HOME/esp/esp-idf/export.sh # Depending on your esp-idf installation
idf.py build
idf.py flash
idf.py monitor
```

