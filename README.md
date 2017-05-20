# ESP8266-PlantMonitor
ESP8266 MQTT based soil moisture measuring plant monitor.

This project is intended to use battery powered, solar panel charged ESP8266 ESP-12E with 16 channel analog multiplexer sensing up to 16 analog sources.

![Wiring Diagram](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/wiring%20diagram.PNG?raw=true)

There is also voltage divider providing measuring of battery voltage, output of voltage divider is connected to channel 0 of analog multiplexer.

## Final installation
![Final installation](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/final%20assembly.jpg?raw=true)

![Final installation](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/final%20assembly2.jpg?raw=true)

![Mounted device to wall holder](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/mounted%20circuit.jpg?raw=true)

## Configuration	
there is file ESP8266-PlantMonitor/espPlantMonitor/src/user_config.h
where you should adjust settings such as your SSID and password, MQTT broker address, Name of device, address of NTP server and so on. 
You can also adjust sleep interval . 

    #define SLEEP_TIME             1800           // [SleepTime] Time to sleep (0 = sleep forever, 1 - X seconds)

There is functionality of AP configuration. If device cannot connect to WiFi in config, or if there are no settings saved on flash, device boots into Access Point mode, so you can connect to it, you will be forwarded to configuration page where you can adjust basic settings. 

## Principle of work
Device wakes up every period of time (default 30 minutes), connects to WiFi, connects to MQTT broker, turn on GPIO 5 which provides power for voltage regulator, powering all soil moisture circuits.

It then multiplex through first 9 channels in my case, measuring and calculating each humidity in percentage. After sending all data through MQTT it goes for sleep for configured period. 
I use soil moisture circuit with voltage comparator, providing analog output which is inversely proportional to the input voltage. 
![Soil moisture circuit](https://www.elecrow.com/images/s/201405/13995201090.jpg)

To save some power there is voltage regulator providing 3,3 V (for stable calculations of measured output from sensors) , which is turned on just for necessary time.

Battery consumption of whole device is quite big because of ESP8266, It can probably operate from few hours to few days, of course it depends on sleep interval. There is small solar pannel providing about 4,5 Volts, with circuit for charging Li-po batteries, which can juice up the battery through day so device can operate independently for long time. 

![Soil moisture circuits stack](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/soil%20moisture%20circuits%20stack.jpg?raw=true)

![enter image description here](https://github.com/Luc3as/ESP8266-PlantMonitor/blob/master/Docs/assembled%20pcbs.jpg?raw=true)



## Loading firmware to device
There are 2 buttons connected, one is for restarting device, and second one to boot device into firmware upload mode. You need to connect FTDI programmer to TX and RX pins as ususal. 

## Shopping list
I'll sum up all links I bought for this setup. 
[ESP-12E](https://www.aliexpress.com/item/2015-New-version-1PCS-ESP-12F-ESP-12E-upgrade-ESP8266-remote-serial-Port-WIFI-wireless-module/32649968409.html?spm=2114.13010608.0.0.w4pxDL)

[Multiplexer](https://www.aliexpress.com/item/CD74HC4067-16-Channel-Analog-Digital-Multiplexer-Breakout-Board-Module-For-Arduino/32646637082.html?spm=2114.13010608.0.0.2vcShQ)

[Voltage regulator](https://www.aliexpress.com/item/High-Quality-10Pcs-lot-5V-to-3-3V-For-DC-DC-Step-Down-Power-Supply-Buck/32553127952.html?spm=2114.13010608.0.0.kcr6Hl)

[Charger](https://www.aliexpress.com/item/Free-shipping-5pcs-TP4056-1A-Lipo-Battery-Charging-Board-Charger-Module-lithium-battery-DIY-MICRO-Port/32438089423.html?spm=2114.13010608.0.0.JdlgfR)

[Moisture sensors](https://www.aliexpress.com/item/FREE-SHIPPING-5pcs-lot-Soil-moisture-meter-testing-module-soil-humidity-sensor-robot-intelligent-car-for/32354569832.html?spm=2114.13010608.0.0.6gY1YW)

[Buttons](https://www.aliexpress.com/item/LHLL-100-Pcs-6-x-6mm-x-9-5mm-PCB-Momentary-Tactile-Tact-Push-Button-Switch/32711550204.html?spm=2114.13010608.0.0.aKemPF)

[Battery](https://www.aliexpress.com/item/3pcs-lot-Rc-Lipo-Battery-3-7V-1500mah-25c-for-RC-Airplane-Quadcopter-Helicopter-Car/32804996488.html?spm=2114.01010208.3.1.Fk4g51&ws_ab_test=searchweb0_0,searchweb201602_5_10152_10065_10151_10068_5010019_10136_10157_10137_10060_10138_10155_10062_10156_437_10154_10056_10055_10054_10059_303_100031_10099_10103_10102_10096_10147_10052_10053_10107_10050_10142_10051_10171_10084_10083_5190020_10119_10080_10082_10081_10110_519_10111_10112_10113_10114_10181_10037_10183_10182_10185_10032_10078_10079_10077_10073_10123_10120_142-10120,searchweb201603_2,ppcSwitch_5&btsid=2ce0c907-c13b-4d49-9a15-6604d871d6b2&algo_expid=1557efd2-43e1-4ea7-9ccf-d270574fd212-0&algo_pvid=1557efd2-43e1-4ea7-9ccf-d270574fd212)

[Solar panel](https://www.aliexpress.com/item/5V-4-5W-Epoxy-Solar-Panel-Photovoltaic-Panel-Polycrystalline-Solar-Cell-for-Mini-Sun-Power-Energy/32727251431.html?spm=2114.01010208.3.50.h5lah7&ws_ab_test=searchweb0_0,searchweb201602_5_10152_10065_10151_10068_5010019_10136_10157_10137_10060_10138_10155_10062_10156_437_10154_10056_10055_10054_10059_303_100031_10099_10103_10102_10096_10147_10052_10053_10107_10050_10142_10051_10171_10084_10083_5190020_10119_10080_10082_10081_10110_519_10111_10112_10113_10114_10181_10037_10183_10182_10185_10032_10078_10079_10077_10073_10123_10120_142-10120,searchweb201603_2,ppcSwitch_5&btsid=79f7213f-5251-4b23-983e-32aa42167da3&algo_expid=aece1f5d-0ef7-462c-a3ab-fbd0a9f45836-6&algo_pvid=aece1f5d-0ef7-462c-a3ab-fbd0a9f45836)

## To do 
* Allowing for waking up remotely, so we can configure device through web browser and then ability to put the device back to sleep
* Adding more configuration items to web interface
