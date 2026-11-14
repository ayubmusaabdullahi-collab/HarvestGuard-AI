# HarvestGuard AI – Hardware Documentation

**Firmware Version:** 2.1  
**Microcontroller:** ESP32 DevKit V1  
**Project:** AI-Assisted Agricultural Storage Monitoring System

## 1. Hardware Overview

HarvestGuard AI uses an ESP32 DevKit V1 as its central embedded controller. The hardware integrates environmental sensors, an LCD interface, visual and audible indicators, and an automated ventilation system.

The system monitors indoor and outdoor environmental conditions and uses the collected information to support intelligent storage management.

## 2. Bill of Materials

| Component | Quantity | Purpose |
|---|---:|---|
| ESP32 DevKit V1 | 1 | Main controller |
| DHT11 sensor | 2 | Temperature and humidity monitoring |
| 16×2 LCD with I2C backpack | 1 | Display interface |
| YL-69 / FC-28 moisture sensor | 1 | Floor moisture measurement |
| 5V DC fan | 1 | Ventilation |
| Relay module | 1 | Fan switching |
| Green LED | 1 | Safe indication |
| Yellow LED | 1 | Warning indication |
| Red LED | 1 | Danger indication |
| Fault LED | 1 | Fault indication |
| Passive buzzer | 1 | Audible alerts |
| Resistors | As required | LED current limiting and sensor interfacing |
| Jumper wires | As required | Electrical connections |
| Breadboard / PCB | 1 | Circuit assembly |
| Suitable power supply | As required | System power |

## 3. ESP32 Pin Configuration

The following assignments are taken directly from HarvestGuard AI Firmware v2.1.

| Hardware | ESP32 GPIO | Interface |
|---|---:|---|
| Indoor DHT11 | GPIO18 | Digital |
| Outdoor DHT11 | GPIO19 | Digital |
| LCD SDA | GPIO21 | I2C |
| LCD SCL | GPIO22 | I2C |
| Fan relay IN | GPIO23 | Digital output |
| Green LED | GPIO25 | Digital output |
| Yellow LED | GPIO26 | Digital output |
| Red LED | GPIO27 | Digital output |
| Passive buzzer | GPIO32 | PWM/tone output |
| Fault LED | GPIO33 | Digital output |
| Floor sensor AO | GPIO34 | Analog input |

**Pin assignments are fixed according to Firmware v2.1.**

## 4. Hardware Wiring Connections

### 4.1 Indoor DHT11 Sensor

The indoor DHT11 measures temperature and relative humidity inside the agricultural storage environment.

| DHT11 Pin | ESP32 Connection |
|---|---|
| VCC | 3.3V |
| DATA / S | GPIO18 |
| GND | GND |

### 4.2 Outdoor DHT11 Sensor

The outdoor DHT11 measures the surrounding environmental conditions.

| DHT11 Pin | ESP32 Connection |
|---|---|
| VCC | 3.3V |
| DATA / S | GPIO19 |
| GND | GND |

For bare DHT11 sensors, use the manufacturer's recommended DATA pull-up resistor. Sensor modules may already contain one.

### 4.3 LCD Display (16×2 I2C)

The LCD displays environmental readings and system status.

| LCD Pin | ESP32 Connection |
|---|---|
| VCC | Suitable regulated supply |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

**I2C Address:** 0x27

The LCD interface uses the ESP32 Wire library with custom SDA and SCL assignments.

If the LCD backpack uses 5V I2C pull-ups, install a bidirectional logic-level shifter between the backpack and ESP32.

### 4.4 Floor Moisture Sensor

The resistive moisture sensor measures moisture conditions at the storage floor.

| Sensor Pin | ESP32 Connection |
|---|---|
| VCC | Suitable sensor supply |
| GND | GND |
| AO | GPIO34 |

GPIO34 is configured as an analog input.

The ESP32 ADC uses 12-bit resolution, producing readings from 0 to 4095.

The current firmware records raw ADC values. Calibration and validated wet/dry thresholds are still required before interpreting these values as moisture percentages.

### 4.5 Relay and 5V Ventilation Fan

The relay controls the ventilation fan according to the ESP32's environmental control decisions.

| Relay Pin | Connection |
|---|---|
| IN | ESP32 GPIO23 |
| VCC | Compatible regulated relay supply |
| GND | Common ground with ESP32 for a non-isolated control arrangement |

The relay contacts are used to switch the fan's separate 5V supply.

**Relay contact connections:**

- COM: Positive supply connection to be switched.
- NO: Fan positive terminal.
- Fan negative terminal: Supply negative.

This arrangement makes the fan operate when the relay is energised.

Never power the fan directly from an ESP32 GPIO pin. Confirm relay contact ratings, fan current requirements, and relay module logic compatibility before operation.

### 4.6 Indicator LEDs

The four LEDs communicate the system's operating condition.

| LED | ESP32 GPIO | Indication |
|---|---:|---|
| Green | GPIO25 | SAFE |
| Yellow | GPIO26 | WARNING |
| Red | GPIO27 | DANGER |
| Fault | GPIO33 | Fault indication |

Each LED must have an appropriate series current-limiting resistor, typically 220–330 ohms for common indicator LEDs.

### 4.7 Passive Buzzer

The passive buzzer provides audible alerts.

| Buzzer Pin | ESP32 Connection |
|---|---|
| Positive | GPIO32 |
| Negative | GND |

The firmware generates a 5,000 Hz tone using the Arduino tone function.

The buzzer is programmed to produce different beep patterns for danger and fault conditions.

## 5. Power Supply Considerations

The hardware requires appropriate power distribution.

Important considerations:

- The ESP32 operates with 3.3V GPIO logic.
- The DHT11 sensors should receive a compatible supply.
- The LCD and relay require supplies compatible with their modules.
- The ventilation fan uses a separate 5V supply.
- All non-isolated low-voltage control circuits should have an appropriate common ground.
- The ESP32 GPIO pins must never receive a 5V logic signal.
- Avoid powering the fan directly from the ESP32 development board.

Use properly regulated supplies and verify current requirements before connecting the complete system.

## 6. Hardware Operating Sequence

1. The ESP32 starts and initialises its peripherals.
2. Both DHT11 sensors begin collecting environmental readings.
3. The floor sensor supplies analog readings.
4. The ESP32 processes temperature and humidity measurements.
5. The LCD displays indoor conditions, outdoor conditions, system status and fault information.
6. LEDs indicate the current system condition.
7. The buzzer generates alerts when required.
8. The relay controls the ventilation fan when the programmed conditions permit.
9. The ESP32 web server makes monitoring information available through the dashboard.

## 7. Hardware Testing Procedure

The following tests should be completed during prototype validation.

| Test | Expected Result |
|---|---|
| ESP32 startup | Successful initialisation |
| Indoor DHT11 | Valid indoor readings |
| Outdoor DHT11 | Valid outdoor readings |
| LCD test | Correct display output |
| Floor sensor | Changing ADC readings under different moisture conditions |
| Green LED | Activates in SAFE condition |
| Yellow LED | Activates in WARNING condition |
| Red LED | Activates in DANGER condition |
| Fault LED | Activates under fault conditions |
| Passive buzzer | Produces programmed alert tones |
| Relay test | Switches according to control logic |
| Fan test | Operates when ventilation conditions are suitable |
| Dashboard test | Displays available sensor information |

## 8. Known Hardware and Firmware Considerations

- The floor sensor analog input is enabled, but validated fault detection and moisture calibration remain future improvements.
- DHT11 readings are retained at their last valid values when a sensor reading fails.
- The fan has minimum runtime, minimum OFF time, maximum runtime and cooldown protections.
- Unsafe incoming air conditions can prevent ventilation.
- Sensor faults force the ventilation fan OFF.
- AI recommendations do not override the ESP32 safety rules.

## 9. Conclusion

The HarvestGuard AI hardware architecture demonstrates the integration of embedded systems, environmental sensing, electronic interfacing, automation and intelligent decision support.

The design provides a foundation for developing affordable smart agricultural storage monitoring solutions.

---

**Project:** HarvestGuard AI  
**Controller:** ESP32 DevKit V1  
**Firmware:** v2.1  
**Domain:** Embedded Systems | IoT | Smart Agriculture | Artificial Intelligence
