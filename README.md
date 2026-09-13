# LoRaWAN
LoRaWAN implementation with temperature, humidity, and CO2 sensor / display based on LilyGo T-Echo, LoRaWAN gateway RAK7246G / RAK5146, TTN, and Datacake

## Project plan
1. connect temperature / humidity sensor SHT31 to T-Echo Lite ==> achieved
2. show the measurements on the display  of T-Echo Lite ==> achieved
3. configure the gateway ==> achieved
4. register the gateway in TTN  ==> achieved
5. create application in TTN and get the TTN keys for the end device ==> achieved
6. extend the code on T-Echo Lite about LoRaWAN implementation (device activation, measurement upload) ==> achieved
7. Connect an IoT data dashboard to the data in TTN (with Datacake) ==> achieved
8. implement sleep mode with RAM retention for nRF52 ==> achieved
9. complete the sensor set to full air quality monitoring ==> started

## 1. Connect sensor to T-Echo
- connect SHT31 to T-Echo Lite using the following pins

| Sensor pin	| T-Echo pin	| Arduino pin
|---------------|---------------|-------------
| VCC			| 3V3			| 3.3V power
| GND			| GND			| Ground
| SDA			| P1.03 (PI.03) | 35
| SCL			| P1.04 (PI.04) | 36

## 2. Show measurements on T-Echo
- achieved with "Sensor connected" submission to GitHub

## 3. Configure gateway
- configure gateway network, internet access, LoRa band, TTN network
### RAK7246G / RAK7248G:
  - [RAK7246G Quick Start Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/quickstart)
  - [RAK7246G Quick Start Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7248/quickstart/)
  - verify / change the TTN server name to new TTN V3 name (assuming EU1 selected in gateway setup in TTN; check the gateway configuration in general settings in TTN console; s. chapter Register gateway in TTN; assuming ask-it as organization name): ask-it.eu1.cloud.thethings.industries
  - enable and restart LoRa services (assuming service name ttn-gateway): sudo systemctl enable ttn-gateway | sudo systemctl start ttn-gateway | sudo systemctl status ttn-gateway
  - verify communication of the gateway with TTN: sudo journalctl -u ttn-gateway -f 
  - verify gateway connectivity status in TTN console (should be Connected)
### RAK5146 PiHAT Kit for LoRaWAN and Concentrator (SPI model)
  - referenced documentation:
    - [1. RAK5146 Setup with LoRa Basic Station](https://lora.vsb.cz/index.php/433-868-mhz-rak5146l-rak5146-lora-basics-station/)
    - [2. Setting Up a LoRa Gateway with Raspberry Pi and RAK5146](https://medium.com/@techworldthink/setting-up-a-lora-gateway-with-raspberry-pi-and-rak5146-f1af49a84ff7)
    - [3. RAK Common for Gateway](https://github.com/RAKWireless/rak_common_for_gateway)
    - [4. RAK2287 Quick Start Guide](https://docs.rakwireless.com/product-categories/wislink/rak5146/quickstart/)
  - RAK PiOS approach chosen:
    - [image download](https://github.com/RAKWireless/rakpios/releases/download/2026-08-07-rakpios-1.0.0-trixie-arm64/20260807-rakpios-1.0.0-arm64-lite.img.xz) 
    - [Quick start guide](https://docs.rakwireless.com/product-categories/software-apis-and-libraries/rakpios/quickstart)
    - 1. flash the image
	- 2. initial boot / reboot
	- 3. startup with WiFi AP mode
	- 4. change network configuration
	- 5. post-installation steps (update, upgrade and reboot)
	- 6. install / update rakpios-cli
	- 7. select UDP packet forwarder, change the server name in environment variables, update the garteway EUI in TTN
	- 8. start the service and observe the logs in cli / connectivity status in TTN

## 4. Register application and gateway in TTN
- register application with TTN (outdated, still helpful)
  - [RAK7246G LoRaWAN Network Server Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)

- register the gateway with TTN (outdated, still helpful)
  - [RAK7246G LoRaWAN Network Server Guide](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)

- create organization and application in TTN (outdated, still helpful)
  - [Setup LoRaWAN Network in TTN](https://docs.rakwireless.com/product-categories/wisgate/rak7246g/lorawan-network-server-guide)

- register gateway in TTNS (helpful, but related to TTN gateway)
  - [Setup LoRaWAN gateway on TTN Gateway Pro](https://www.thethingsindustries.com/docs/getting-started/3-try-starter-kit/)
    - for RAK LoRa gateway: ensure disabling authenticated connection

## 5. Get TTN device keys
- create a custom end device in TTN console (application section)
  - JoinEUI: enter 00 00 00 00 00 00 00 00
  - DevEUI: use generate button to create new device ID
  - AppKey: use generate button to create new application key
  - configure CayenneLPP decoder for end-device
- select LoRa parameters
  - LoRa version 1.0.3 (works with RadioLib)
  - bandwith plan according to region e.g., EU 863-870 SF90 RX2

## 6. Send measurements to TTN
- use RadioLib in LoRaWAN mode
  - implement OTAA (over the air activation) of the T-Echo
    - using the JoinEUI, DevEUI, AppKey, NwkKey (same as AppKey)
- use CayenneLPP library to create LoRaWAN paylod
- hint: for development purposes only change the settings of the end device in Join settings: Resets join nonces - Enabled

## 7. Connect Datacake with TTN to visualize the measurement data
- create a Datacake account
- add a new device:
  - create a new product
  - select TTN V3 as network server
  - enter device EUI and ID exactly from TTN
  - select free data plan
- change configuration of the new device
  - product and hardware: 
    - adapt the payload decoder (hint: use use rawPayload, not payload data)
	- create fields TEMPERATURE, HUMIDITY, VOLTAGE (Numeric with appropriate semantic), LOCATION (Location type and sematic)

```Javascript  
function Decoder(bytes, port) {
    var measurements = [];
    
    try {
        // Bei TTN-Integrationen in Datacake liegen die Webhook-Daten in rawPayload
        if (typeof rawPayload !== 'undefined' && rawPayload.uplink_message) {
            
            // 1. Sensordaten aus dem Decoded Payload auslesen
            if (rawPayload.uplink_message.decoded_payload) {
                var dec = rawPayload.uplink_message.decoded_payload;
                
                if (dec.temperature_1 !== undefined) {
                    measurements.push({ field: "TEMPERATURE", value: dec.temperature_1 });
                }
                if (dec.relative_humidity_2 !== undefined) {
                    measurements.push({ field: "HUMIDITY", value: dec.relative_humidity_2 });
                }
                if (dec.analog_in_3 !== undefined) {
                    measurements.push({ field: "VOLTAGE", value: dec.analog_in_3 });
                }
            }
            
            // 2. GPS-Position sicher auslesen (falls in TTN Registry gesetzt)
            if (rawPayload.uplink_message.locations && rawPayload.uplink_message.locations.user) {
                var lat = rawPayload.uplink_message.locations.user.latitude;
                var lon = rawPayload.uplink_message.locations.user.longitude;
                
                if (lat !== undefined && lon !== undefined) {
                    measurements.push({ 
                        field: "LOCATION", // Muss exakt wie dein Geo-Feld in Datacake heißen
                        value: "(" + lat + "," + lon + ")" 
                    });
                }
            }
        }
    } catch (e) {
        // Fehler abfangen, damit der Decoder nicht abstürzt
    }
    
    return measurements;
}
```

## 8. Implement power saving mode for T-Echo Lite
- implement sleep mode with RAM retention for nRF52
  - considering only uplink to TTN for RadioLib
  - using the 2min cycle implemented in measurement upload to TTN / Datacake
  - solution: 
    - delay() in loop()
	- custom delay for RadioLib
  
## 9. Add CO2 sensor for full air quality monitoring
- add SCD40 sensor to I2C bus
  - follow the connecvtivity plan in 1.
- extend the software to handle multiple sensors
  - introduce automatic detection of supported sensor types
  - add CO2 concentration to CayenneLLP payload
- adapt the Datacake configuration (fields, decoder)
  