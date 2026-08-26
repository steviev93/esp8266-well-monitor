ESP8266 Well Water Monitor

A solar-powered IoT system for remotely monitoring residential well-water depth using an ESP8266, ADS1115 ADC, and 4–20 mA submersible pressure sensor.




Overview

The monitor converts the sensor's 4–20 mA output into a measurable voltage using a 150 Ω shunt resistor, producing approximately 0.6–3.0 V across the sensor's operating range. An ADS1115 digitizes the signal and communicates with the ESP8266 over I²C.

The ESP8266 processes the reading, converts it into water-level data, and transmits telemetry over Wi-Fi. The sensor loop is switched by a MOSFET so it can be powered only when a measurement is required.

Hardware
ESP8266 NodeMCU
ADS1115 16-bit ADC
4–20 mA submersible pressure/depth sensor
150 Ω current-shunt resistor
IRLB8721 MOSFET for sensor power control
12 V LiFePO4 battery
Solar charging system
Custom KiCad PCB

KiCad schematic and PCB source files are available in hardware/.

Firmware

Firmware is written in C and organized into separate hardware, driver, service, and application layers.

Key features include:

ADS1115 I²C driver
4–20 mA signal conversion and water-level calculation
Wi-Fi connection management
HTTP telemetry
In-memory buffering during connection failures
NVS-backed storage for retained data/configuration
Automatic replay of buffered telemetry after reconnect
Health monitoring and recovery logic
PCB V1 Erratum

The first PCB revision contained an error in the ADS1115 power connection. The ADC supply was routed to the wrong ESP8266 board power connection rather than the regulated 3.3 V rail, resulting in unstable operation and ESP8266 resets.

The prototype was reworked by cutting the incorrect VCC trace and manually connecting the ADS1115 supply to 3.3 V.

A future PCB revision should correct the power-net assignment in the schematic/layout.

Repository Structure
application/ – sampling, telemetry, buffering, supervision
drivers/ – ADS1115 device driver
hal/ – GPIO and I²C hardware abstraction
services/ – Wi-Fi, HTTP, and persistent storage
hardware/ – KiCad schematic, PCB layout, and schematic image
main/ – application entry point
Status

Functional prototype and firmware complete. Hardware design is retained in the repository for continued PCB revision and field-development work.
