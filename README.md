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

Full Status Output (driven high when full capacity is reached)

    D0

Vacant Status Output (driven high when not at full capacity)

    D1

Both output pins can also be changed by redefining `VACANCY_FULL_PIN` and `VACANCY_VACANT_PIN`

    #define VACANCY_FULL_PIN        D0
    #define VACANCY_VACANT_PIN      D1

Analog PWM Control Input

    A0

Can also be changed by redefining `VACANCY_PWM_CONTROL_PIN`

    #define VACANCY_PWM_CONTROL_PIN A0

# Vacancy Status Output

#### Inverted Output

The vacancy status output pins can also be configured to be inverted, or drive a PWM signal that may be controlled via the analog input `A0`.  
To invert the output signals (so that they are driven low on activation), modify the following definition to `1`. This does not affect PWM output.

    #define VACANCY_INVERT_OUTPUT   1

#### PWM Output

To output a PWM signal, set the following definition to `1`, and optionally change the frequency with the `VACANCY_PWM_FREQUENCY` definition.

    #define VACANCY_PWM_ENABLED     1
    #define VACANCY_PWM_FREQUENCY   1000

The duty cycle of the PWM signal can be optionally controlled via an analog input from the `A0` pin.  
The multiplier applied to the read value can be changed by modifying the `VACANCY_PWM_MULTIPLIER` definition.

    #define VACANCY_PWM_MULTIPLIER  0.15

# Usage

Before flashing set the `ssid` and `password`. 
Until the Display gets polled it shows it's IP. 
It expects a request in under **10s** or it goes back to showing it's IP.

A Request to `http://<IP>:80/` will receive a Response with content type `application/json`.
It's content might look like this: 

    { "number": 0 }
