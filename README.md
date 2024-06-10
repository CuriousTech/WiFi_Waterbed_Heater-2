# WiFi Waterbed Heater 2  
WiFi Waterbed heater with touchscreen  

A waterbed heater with a 2.4" Nextion HMI touchscreen, speaker and PIR sensor for more fuctionality than the first one. This has a large display for an alarm clock. The PIR sensor turns on the display as well as a WiFi dimmer that uses my code. The small speaker inside is used for the alarm, touch clicks and accepts external command (used for doorbell). The rest of the code was taken from the older version. The supply and relay are now 5V.  
  
2024: LD2410 support, only with ESP32.  The board is about the same, but can send schematic/board layout if requested.  I used an old ESP32S2, but can easily change it.  The LD2410 code is a wrapper to similate PIR with a presence hold, but holds presence when a single target energy drops enough to trigger non-presence while distance is below the bed threshold (200cm from center of headboard to foot of bed).  Additionally, when a target enters the room, the lights turn on.  When all targets are within bed range, lights turn off, fan turns on.  When any target leaves bed range, lights turn on, fan off. And finally, all off when targets leave presence range.  
  
3D case for the supply is from the orginal. The display face is from the HVAC project.  
  
![Image](http://www.curioustech.net/images/wb2.jpg)  
![WebUI](http://www.curioustech.net/images/wb2web3.png)  
