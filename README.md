# Embedded-Systems 
# Imperial College London EEE Department- Year 3
# Group: Fantastic Four

In the NFL, collisions are a common occurence. We have implemented an IOT(Internet of things) device that provides crucial data as to when american football players have encountered a serious collision. The device will also inform the monitoring staff as to how serious the collision is.

We have use an accelerometer(LIS3DH) to record changes in acceleration and a wifi module(Esp8266) to send data to the monitoring staff.

# Requirements to implement this code

- [Accelerometer LIS3DH](http://www.st.com/content/ccc/resource/technical/document/datasheet/3c/ae/50/85/d6/b1/46/fe/CD00274221.pdf/files/CD00274221.pdf/jcr:content/translations/en.CD00274221.pdf)
- [WiFi Module Esp8266](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/overview)
- [MicroPython](https://docs.micropython.org/en/latest/esp8266/index.html)
- Ampy

# Setup

- Purchase accelerometer and wifi module.
- [Download](https://www.python.org/downloads/) and install the latest version of Python.
- Install ampy: Run ==pip install adafruit-ampy== on Terminal.

# Deployment

- Boot.py is the first code that runs when esp8266 is connected to power.
- main.py runs after and this contains the code for the functionalities of the accelerometer. It is here that we implement the detection of a collision

# Additional Suggestions

- Refer to datasheet of LIS3DH and Esp8266 to understand the register allocation and i2c memory addresses used. 



