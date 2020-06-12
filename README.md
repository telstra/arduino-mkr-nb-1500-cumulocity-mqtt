# Telstra Arduino MKR NB 1500 Examples

This sketch demonstrates how to:

- Register an MKR NB 1500 on Cumulocity over MQTT
- Send measurements from an MKR NB 1500 to Cumulocity over MQTT

If you are looking to use the Cumulocity REST Interface over HTTP see [this example instead](https://github.com/telstra/arduino-mkr-nb-1500-cumulocity).

For further details on Cumulocity's MQTT implementation refer to the [Cumulocity documentation](https://cumulocity.com/guides/device-sdk/mqtt/#device-integration).


## Prerequisites

- [An Arduino MKR NB 1500 board](https://dev.telstra.com/iot-marketplace/arduino-mkr-nb-1500)
- [A Telstra Cat-M1 IoT SIM card with an active subscription](https://dev.telstra.com/content/telstra-iot-data-sim-plans)
- A Telstra IoT (Cumulocity) account and tenancy
- A Computer with the Arduino IDE installed
- (Optional) Any temperature sensor


## How to run

1. Update `arduino_secrets.h` with your Cumulocity domain and static user credentials (different to your normal credentials)
2. Upload the sketch and open the Serial Monitor
3. Log in to Cumulocity and switch to the Device Management application
4. Navigate to Devices > Registration
5. Register device > General device registration. Enter the IMEI printed to the Serial Monitor in the Device ID field.
6. Accept the device
7. Navigate to Devices > All devices. Your device should have appeared and will send a measurement every minute
