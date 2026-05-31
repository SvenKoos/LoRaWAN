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
Sensor pin	T-Echo pin		Arduino pin
VCC			3V3				3.3V power
GND			GND				Ground
SDA			P1.04 (PI.04)	36
SCL			P1.03 (PI.03)	^35

## Show measurements on T-Echo


## Configure gateway


## Get TTN keys


## Send measurements to TTN
- use RadioLib in LoRaWAN mode
 + implement OTAA (over the air activation) of the sensor / T-Echo
 + create the LoRaWAN payload for measurement data with CayenneLPP
 
 
