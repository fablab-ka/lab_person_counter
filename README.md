# Lab Person Counter

A simple ESP8266 and MAX7219 based contraption to easily let people count themselves when entering the Makerspace.
Tested with an nodemcu chip

# Wiring

Increase Button

    D4 and GND

Decrease Button

    D3 and GND

MAX7219 panels (at least 4)

    CLK_PIN   D5
    DATA_PIN  D7
    CS_PIN    D8
    VCC       5V
    GND       GND

# Usage

Before flashing set the `ssid` and `password`. 
Until the Display gets polled it shows it's IP. 
It expects a request in under **10s** or it goes back to showing it's IP.

A Request to `http://<IP>:80/` will receive a Response with content type `application/json`.
It's content might look like this: 

    { "number": 0 }
