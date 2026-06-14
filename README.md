# LoRaWAN
LoRaWAN implementation with temperature and humidity sensor / display based on LilyGo T-Echo, LoRaWAN gateway RAK7246G and TTN

## Project plan
1. connect temperature / humidity sensor SHT31 to T-Echo Lite
2. show the measurements on the display  of T-Echo Lite
3. configure the gateway and register it with TTN
4. create application in TTN and get the TTN keys
5. extend the code on T-Echo Lite about LoRaWAN implementation (sensor authentication, measurement upload)

## Connect sensor to T-Echo
- connect SHT31 to T-Echo Lite using the following pins (to be verified)

| Sensor pin	| T-Echo pin	| Arduino pin
|---------------|---------------|-------------
| VCC			| 3V3			| 3.3V power
| GND			| GND			| Ground
| SDA			| P1.04 (PI.04) | 36
| SCL			| P1.03 (PI.03) | 35

## Show measurements on T-Echo


## Configure gateway
- configure gateway network, internet access, LoRa band, TTN network
 - RAK7246G:
  - [RAK7246G Quick Start Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/quickstart)
  - verify / change the TTN server name to new TTN V3 name (assuming EU1 selected in gateway setup in TTN): eu1.cloud.thethings.network
  - enable and restart LoRa services (assuming service name ttn-gateway): sudo systemctl enable ttn-gateway | sudo systemctl start ttn-gateway | sudo systemctl status ttn-gateway
  - verify communication of the gateway with TTN: sudo journalctl -u ttn-gateway -f 
 - RAK5146 PiHAT Kit for LoRaWAN and Concentrator (SPI model)
  - [1. RAK5146 Setup with LoRa Basic Station](https://lora.vsb.cz/index.php/433-868-mhz-rak5146l-rak5146-lora-basics-station/)
  - [2. Setting Up a LoRa Gateway with Raspberry Pi and RAK5146](https://medium.com/@techworldthink/setting-up-a-lora-gateway-with-raspberry-pi-and-rak5146-f1af49a84ff7)
  - Raspberry Pi OS setup acc. 1.
  - Remote connection via SSH acc. 1.
  - Post-installation steps (Update, upgrade and reboot Raspberry Pi) acc. 1
  - Setup Raspberry Pi’s Interfaces (SPI, I2C) acc. 1.
  - Install RAK Common for Gateway acc. 2.
  - Configure the Gateway for TTN acc. 2.

## Register in TTN
- register the gateway with TTN (outdated, still helpful)

[RAK7246G LoRaWAN Network Server Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)

- create organization and application in TTN (outdated, still helpful)

[Setup LoRaWAN Network in TTN](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)

- register gateway in TTNS

[Setup LoRaWAN gateway on TTN Gateway Pro](https://www.thethingsindustries.com/docs/getting-started/3-try-starter-kit/)

## Get TTN application keys
- register application with TTN (outdated)

[RAK7246G LoRaWAN Network Server Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)


## Send measurements to TTN
- use RadioLib in LoRaWAN mode
 - implement OTAA (over the air activation) of the sensor / T-Echo
 - create the LoRaWAN payload for measurement data with CayenneLPP
- use CayenneLPP library to create LoRaWAN paylod
