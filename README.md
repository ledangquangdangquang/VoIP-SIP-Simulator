# SIP with Linphone

## Develop Setup
### Ubuntu
**1. Import `.ova` file into Virtualbox**

- [Download .ova here](https://drive.google.com/file/d/1nVV5_vkfMWg4xNcgTEhkA2DsERjjYdAE/view?usp=sharing)
- password: 102112

**2. Open Qt -> Open project -> `~/Projects/SIP_addServer` -> Select `CMakeLists.txt`**

**3. Replace your ip into `mainwindow.cpp` file**

- To show your ip:
```Bash
hostname -I
```

**4. Run**

- Click the Play button in the bottom-left corner of the Qt app


### Other device (Android, Window, Linux, ...)
**1. Download Linphone**

https://www.linphone.org/en/download/

**2. Select third party**

Setup info user 2:
```
Username: 1002
Password: 123456
Domain / SIP Server: <your-ip>:5060
```
---

## Asterisk CLI:
```Bash
sudo asterisk -rvvv
```
```
pjsip show endpoints
```

## Demo

<video src="Demo/sip.mp4" controls width="100%"></video>
