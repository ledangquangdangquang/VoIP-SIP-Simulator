# SIP with Linphone

## Config file **Asterisk**
```
sudo nano /etc/asterisk/extensions.conf
```
```
sudo nano /etc/asterisk/pjsip.conf
```

## Develop
### Ubuntu
**1. Import `.ova` file into Virtualbox**

- [Download .ova here](google.com)
- password: 102112

**2. Open Qt -> Open project -> `~/Projects/SIP_addServer` -> Select `CMakeLists.txt`**

**3. Replace your ip into `mainwindow.cpp` file**
- To show your ip:
```Bash
hostname -I
```


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
