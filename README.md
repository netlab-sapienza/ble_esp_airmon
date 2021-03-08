#  BLE ESP Air monitoring

## Project description

This project involves the connection between ESP32 and 1 or more Android phones that send the following data to ESP32:
- Latitude
- Longitude
- Time

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

What things you need to install the software and how to install them

- Arduino IDE downloadable from [here](https://www.arduino.cc/en/software)
- Insert this link https://dl.espressif.com/dl/package_esp32_index.json to add ESP32 module inside Arduino IDE: `File->Settings->Additional Boards Manager URLs`
- In `Tools->Board->Bords Manager...` search for `ESP32` and install the package
- In `Sketch->Include Library->Manage Libraries...` search for `ESP32 BLE Arduino`, by Neil Kolban and install the library

Now you're ready to run the code.

### Installing

- `git clone https://github.com/BE-Mesh/ble_esp_airmon.git`
- In the Arduino IDE go to `File->Open` and select the `BLE_ESP32.ino` file
- Connect the ESP32 with a USB cable
- Upload the file using the `Upload` button

## Presentation
You can find the presentation of the project [here](https://github.com/BE-Mesh/ble_esp_airmon/blob/main/Presentation/PresentationProjectBLE.ppt)

## Developers
- [Alessio Amatucci](https://github.com/Alexius22)
- [Federica Di Giacomo](https://github.com/Federicadgc)
- [Alessandro Paniccià](https://github.com/Hoken-rgb)