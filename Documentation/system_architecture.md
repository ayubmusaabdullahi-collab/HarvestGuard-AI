# HarvestGuard AI – System Architecture

## 1. Overview

HarvestGuard AI uses an ESP32-based IoT architecture to monitor agricultural storage environments, analyse environmental conditions, and support intelligent environmental management.

The system integrates sensing, embedded processing, AI-assisted decision support, user monitoring, and automated control.

## 2. System Modules

### A. Sensing Module
The sensing module collects environmental information using:
- Two DHT11 sensors for indoor and outdoor temperature and humidity.
- A resistive moisture sensor for floor moisture monitoring.

### B. Embedded Processing Module
The ESP32 acts as the central controller. It:
- Collects sensor readings.
- Processes environmental measurements.
- Compares indoor and outdoor conditions.
- Controls connected output devices.
- Makes monitoring information available to the dashboard.

### C. AI Analysis Module
The AI component analyses environmental information and supports decisions by:
- Identifying potentially harmful environmental conditions.
- Interpreting indoor and outdoor environmental differences.
- Providing environmental recommendations.
- Supporting better ventilation decisions.

### D. User Interface Module
The system provides information through:
- A 16×2 LCD.
- A web-based monitoring dashboard.
- Visual LED indicators.
- Audible buzzer alerts.

### E. Environmental Control Module
The control module supports automated environmental management through:
- A relay module.
- A 5V ventilation fan.
- Appropriate ventilation control decisions.

## 3. Data Flow

1. Sensors collect environmental readings.
2. Readings are transmitted to the ESP32.
3. The ESP32 processes the information.
4. Environmental information is made available to the AI analysis component.
5. The AI component supports recommendations.
6. Monitoring information is presented through the dashboard and LCD.
7. Alerts and control actions are activated when necessary.

## 4. Design Objectives

- Affordable implementation.
- Continuous environmental monitoring.
- Intelligent decision support.
- Practical automation.
- Modular hardware and software design.
- Applicability to agricultural storage environments.

## 5. Engineering Considerations

The system requires appropriate sensor calibration, reliable power supply, safe relay wiring, and testing under different environmental conditions.

AI recommendations should be validated against actual environmental measurements before automated actions are performed.

---

**Project:** HarvestGuard AI  
**Architecture:** IoT-Based Environmental Monitoring and AI-Assisted Control  
**Controller:** ESP32
