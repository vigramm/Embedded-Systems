from machine import Pin, I2C
from umqtt.simple import MQTTClient
from ustruct import unpack
from machine import RTC
import machine
import sys
import time
import ujson
import math

#Connect to the internet
def do_connect():
    import network
    sta_if = network.WLAN(network.STA_IF)
    if not sta_if.isconnected():
        print('connecting to network...')
        sta_if.active(True)
        sta_if.connect('EEERover', 'exhibition')
        while not sta_if.isconnected():
            pass
    print('network config:', sta_if.ifconfig())

def convert_to_ms (high,low):
    a = bytearray(high)
    b = bytearray(low)
    aa = b + a
    Gval = unpack('<h', aa)[0] #signed integer of G value on a scale up to 32768 being 4G
    return ((Gval*39.2)/32768);

#Check if a collision has occured
def check_safe (current_val , prev_val):
    diff_ = current_val - prev_val

    if abs(diff_) > 5 : #collision occured

        verdit = False
    else:
        verdit = True
    return verdit;

i2c = I2C(scl = Pin(4),sda = Pin(5),freq = 500000) #define i2c pins
addr = i2c.scan()[0] #Finding the address of the device

#Setting the registers to variables
CTRL_Reg1 = 0x20
OUT_RegX_L = 0x28
OUT_RegY_L = 0x2A
OUT_RegZ_L = 0x2C
OUT_RegX_H = 0x29
OUT_RegY_H = 0x2B
OUT_RegZ_H = 0x2D
CTRL_Reg4 = 0x23

#Setting certain bits to 0 and certain bits to 1 depending on our needs
#I2C.writeto_mem(addr, memaddr, buf)
i2c.writeto_mem(addr, CTRL_Reg1, bytearray([23]))
i2c.writeto_mem(addr, CTRL_Reg4, bytearray([24])) #resolution 4G

#client = MQTTClient('unnamed1', '192.168.0.10')
#client.connect()


xval = 0.0
yval = 0.0
zval = 0.0

ms = "m/s^2"
rtc = machine.RTC()


def t3_publication(topic, msg):
    global rtcdata
    timeee = ujson.loads(msg)['date']
    print(timeee)
    year = int(timeee[0:4])
    month = int(timeee[5:7])
    day = int(timeee[8:10])
    weekday = 4
    hours = int(timeee[11:13])
    minutes = int(timeee[14:16])
    seconds = int(timeee[17:19])
    subseconds = 0
    rtcdata = (year,month,day,weekday,hours,minutes,seconds, subseconds)
    #print (rtcdata)
    #rtc.datetime(rtcdata)
    #print(rtc.datetime())

do_connect()
client = MQTTClient('unnamed1', '192.168.0.10')
client.set_callback(t3_publication)
client.connect()
client.subscribe(b'esys/time')
a = True
while a :
    if True:
        # Blocking wait for message
        client.wait_msg()
        print ("yes2")
        a = False
    else:
        # Non-blocking wait for message
        client.check_msg()
        # Then need to sleep to avoid 100% CPU usage (in a real
        # app other useful actions would be performed instead)
    time.sleep(4)
client.disconnect()

print (rtcdata)
rtc.datetime(rtcdata)

client.connect()

while True:
    #Reading acceleration values
    x_h = i2c.readfrom_mem(addr,OUT_RegX_H,1)
    x_l = i2c.readfrom_mem(addr,OUT_RegX_L,1)
    y_h = i2c.readfrom_mem(addr,OUT_RegY_H,1)
    y_l = i2c.readfrom_mem(addr,OUT_RegY_L,1)
    z_h = i2c.readfrom_mem(addr,OUT_RegZ_H,1)
    z_l = i2c.readfrom_mem(addr,OUT_RegZ_L,1)

    a=xval
    b=yval
    c=zval

    xval = convert_to_ms(x_h , x_l)
    yval = convert_to_ms(y_h , y_l)
    zval = convert_to_ms(z_h , z_l)

    #check which values are safe
    check_x = check_safe (xval,a)
    check_y = check_safe (yval,b)
    check_z = check_safe (zval,c)

    #give verdict
    if check_x or check_y or check_z :
        verdict_ = "safe"
    else:
        verdict_ = "unsafe - possible collision"

    #need values as string
    xvall = str(xval)
    yvall = str(yval)
    zvall = str(zval)



    current_time = rtc.datetime()
    #acceleration_data = ["date/time",current_time,""] if you want the order
    payload = ujson.dumps({"date/time": (current_time) , "xacc": (xvall + ms) , "yacc": (yvall + ms) , "zacc": (zvall + ms), "verdict": verdict_})
    print (payload)
    client.publish('esys/<fantastic four>/...', bytearray(str(payload)))
    time.sleep(1.5)  # Delay for 0.5 seconds
