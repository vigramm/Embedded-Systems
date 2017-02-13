from machine import Pin, I2C
from umqtt.simple import MQTTClient
from ustruct import unpack
import sys
import time


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
    Gval = unpack('<h', aa)[0] #signed integer
    return ((Gval*39.2)/32768);

#do_connect() #connect to the internet /wifi network

i2c = I2C(scl = Pin(4),sda = Pin(5),freq = 500000)

addr = i2c.scan()[0] #Finding the address of the device

#Setting the registers to variables
#vighram you need to say why you setting the registers to variables
CTRL_Reg1 = 0x20
OUT_RegX_L = 0x28
OUT_RegY_L = 0x2A
OUT_RegZ_L = 0x2C
OUT_RegX_H = 0x29
OUT_RegY_H = 0x2B
OUT_RegZ_H = 0x2D
CTRL_Reg4 = 0x23 #this sets it to +-16G resolution and high resolution output

#Setting certain bits to 0 and certain bits to 1 depending on our needs
#I2C.writeto_mem(addr, memaddr, buf)
i2c.writeto_mem(addr, CTRL_Reg1, bytearray([23]))
i2c.writeto_mem(addr, CTRL_Reg4, bytearray([18])) #resolution 8G and high res output
#Reading the data
#I2C.readfrom_mem(addr, memaddr, nbytes)
alpha = i2c.readfrom_mem(addr,CTRL_Reg1,1)
ms = "m/s^2"

while True:
    x_h = i2c.readfrom_mem(addr,OUT_RegX_H,1)
    x_l = i2c.readfrom_mem(addr,OUT_RegX_L,1)
    y_h = i2c.readfrom_mem(addr,OUT_RegY_H,1)
    y_l = i2c.readfrom_mem(addr,OUT_RegY_L,1)
    z_h = i2c.readfrom_mem(addr,OUT_RegZ_H,1)
    z_l = i2c.readfrom_mem(addr,OUT_RegZ_L,1)
    #a = bytearray(alpha)
    #beta = unpack('<H', a)[0]
    xval = convert_to_ms(x_h , x_l)
    yval = convert_to_ms(y_h , y_l)
    zval = convert_to_ms(z_h , z_l)

    #Reading and printing the rest of the data

    #print(i2c.readfrom_mem(addr,OUT_RegX_H,1),i2c.readfrom_mem(addr,OUT_RegX_L,1))
    print (xval,ms )
    print (yval,ms )
    print (zval,ms )
    time.sleep(1.0)  # Delay for 3 seconds


#client = MQTTClient('unnamed1', '192.168.0.10')
#client.connect()
#client.publish('esys/<fantastic four>/...', bytearray(str(beta)))
