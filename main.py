from machine import Pin, I2C
from umqtt.simple import MQTTClient
from ustruct import unpack
import sys
import time
import ujson


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

def mqqt_connect():
    client = MQTTClient('unnamed1', '192.168.0.10')
    client.connect()

def define_pins_addresses():
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
    i2c.writeto_mem(addr, CTRL_Reg4, bytearray([18])) #resolution 4G and high resolution output

def convert_to_ms (high,low):
    a = bytearray(high)
    b = bytearray(low)
    aa = b + a
    Gval = unpack('<h', aa)[0] #signed integer of G value on a scale up to 32768 being 4G
    return ((Gval*39.2)/32768);

def check_safe (current_val , prev_val):
    diff_ = current_val - prev_val
    if abs(diff_) > 10 : #collision occured
        verdict = False
    else:
        verdict = True

    return verdict;


xval = 0
yval = 0
zval = 0
ms = "m/s^2"

do_connect() #connect to the internet /wifi network
mqqt_connect() #connect to mqqt connect
define_pins_addresses() #define pins of i2c and give names to addresses we need

while True:
    x_h = i2c.readfrom_mem(addr,OUT_RegX_H,1)
    x_l = i2c.readfrom_mem(addr,OUT_RegX_L,1)
    y_h = i2c.readfrom_mem(addr,OUT_RegY_H,1)
    y_l = i2c.readfrom_mem(addr,OUT_RegY_L,1)
    z_h = i2c.readfrom_mem(addr,OUT_RegZ_H,1)
    z_l = i2c.readfrom_mem(addr,OUT_RegZ_L,1)
    #a = bytearray(alpha)
    #beta = unpack('<H', a)[0]

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
    if (check_x = True or check_y = True or check_z = True) :
        verdict = "safe"
    else:
        verdict = "unsafe - possible collision"

    #need values as string
    xvall = str(xval)
    yvall = str(yval)
    zvall = str(zval)


    payload = ujson.dumps({"xacc": (xvall + ms) , "yacc": (yvall + ms) , "zacc": (zvall + ms), "verdict": verdict})
    print (payload)
    client.publish('esys/<fantastic four>/...', bytearray(str(payload)))
    time.sleep(0.5)  # Delay for 0.5 seconds
