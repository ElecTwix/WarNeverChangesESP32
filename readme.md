# War Never Changes
## Introduction
This tool design to exploit 802.11 deauth attack using ESP32.
Deauth attacks will not work with 802.11w enabled networks. 

## Usage 
```bash
idf.py build
idf.py -p PORT flash
```

### TODO

### Basics

#### Attacks

[X] Create deauth attack
[X] Create rough AP

### Configuration

[ ] Create config to set target's SSID


#### Credits

[https://github.com/risinek](risinek)'s  [esp32-wifi-penetration-tool](https://github.com/risinek/esp32-wifi-penetration-tool) to learn how to bypass use [Wi-Fi Stack Libraries (WSL) Bypasser component](https://github.com/risinek/esp32-wifi-penetration-tool/tree/master/components/wsl_bypasser).
