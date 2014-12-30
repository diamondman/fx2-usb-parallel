#!/usr/bin/python3

import usb1
import time
import binascii
import logging
from PIL import Image
from itertools import chain

class USBParallelController(object):
    MODE_AUTO = 2
    MODE_DATA = 1
    MODE_CMD = 0
    def __init__(self, dev=None, con=None, vendor_id=None, prod_id=None, log=False):
        self._mode = -1
        self._log = log
        if dev:
            self.dev = dev
        else:
            if not con:
                con = usb1.USBContext()
            if not (prod_id and vendor_id):
                raise ValueError("If no dev provided, vendor_id and prod_id are required.")
            self.dev = con.openByVendorIDAndProductID(vendor_id, prod_id)
            if not self.dev:
                raise TypeError("FAILED TO FIND USB DEVICE") #Can't think of better error now

    def __del__(self):
        if self.dev:
            self.dev.close()

    def read_ioc(self):
        return self.dev.controlRead(0xC0, 0xb5, 3, 0, 1)

    @property
    def ioc(self):
        return bin(int(binascii.hexlify(self.dev.controlRead(0xC0, 0xb5, 3, 0, 1)).decode(),16))[2:].zfill(8)

    @property
    def address_mode(self):
        return self._mode
    @address_mode.setter
    def address_mode(self, mode):
        if mode not in [self.MODE_AUTO, self.MODE_DATA, self.MODE_CMD]:
            raise ValueError("Mode has to be either the MODE_AUTO, MODE_DATA, or MODE_CMD constants.")
        if self._mode != mode:
            self.dev.controlWrite(0x40, 0xb5, 1, mode, b'')
            self._mode=mode

    def command(self, cmd, data=None, delay=None):
        if isinstance(data, int):
            self.address_mode = self.MODE_AUTO
            val = bytes([cmd, data])
            logging.debug("%s %s %s"%("CMD+DATA", binascii.hexlify(val), len(val)))
            self.dev.bulkWrite(2,val)
        else:
            self.address_mode = self.MODE_CMD
            val = bytes([cmd])
            logging.debug("%s %s %s"%("CMD", binascii.hexlify(val), len(val)))
            self.dev.bulkWrite(2,val)
            if data is not None:
                self.command_extra_data(data)

        if delay:
            time.sleep(delay/(1000*1000))

    def command_extra_data(self, data):
        self.address_mode = self.MODE_DATA
        #logging.debug("%s %s"%("DATA", len(args))) #binascii.hexlify(bytes(args)),
        self.dev.bulkWrite(2, data)


class SEPS525DisplayDriver(object):
    def __init__(self, controller):
        self._ctrl = controller

class Oled160128RGB_ParallelController(SEPS525DisplayDriver):
    def __init__(self, controller):
        self._ctrl = controller

    def fullreset(self):
        for i in [2,3,1]:
            self._ctrl.dev.controlWrite(0x40, 0xb5, 2, i, b'')
            time.sleep(0.000005)

    def display_onoff(self, onoff):
        self._ctrl.command(0x06, 1 if onoff else 0)

    def set_power(self):
        self._ctrl.command(0x10, 0x56)# Set Driving Current of Red
        self._ctrl.command(0x11, 0x4D)# Set Driving Current of Green
        self._ctrl.command(0x12, 0x46)# Set Driving Current of Blue
        self._ctrl.command(0x08, 0x04)# Set Pre ‐ Charge Time of Red
        self._ctrl.command(0x09, 0x05)# Set Pre ‐ Charge Time of Green
        self._ctrl.command(0x0A, 0x05)# Set Pre ‐ Charge Time of Blue
        self._ctrl.command(0x0B, 0x9D)# Set Pre ‐ Charge Current of Red
        self._ctrl.command(0x0C, 0x8C)# Set Pre ‐ Charge Current of Green
        self._ctrl.command(0x0D, 0x57)# Set Pre ‐ Charge Current of Blue
        self._ctrl.command(0x80, 0x01)# Set Reference Voltage Controlled by External Resister

    def display_init(self):
        self.fullreset()
        self._ctrl.command(0x04, 0x03, delay=2000) # ANALOG RESET and osc off
        self._ctrl.command(0x04, 0x00, delay=2000) # Restore normal operation

        self.display_onoff(False) #?
        self._ctrl.command(0x2, 0x01)# Use internal clock w/ external resistor
        self._ctrl.command(0x03, 0x90)# Set Frame Rate as 120Hz [14]
        self._ctrl.command(0x28, 0x7F)# 1/128 Duty (0x0F~0x7F)
        self._ctrl.command(0x29, 0x00)# Set Mapping RAM Display Start Line (0x00~0x7F)
        self._ctrl.command(0x14, 0x01)# Set MCU Interface Mode DOCS SAY 0x31?!
        self._ctrl.command(0x16, 0x76)# 6 bit trible write mode

        self.set_power();

        self._ctrl.command(0x13, 0x00)

        #DOCS SAY CLEAR SCREEN but writing to ram breaks everything

        self.display_onoff(True) # Display On (0x00/0x01)

    def set_drawing_box(self, left, right, top, bottom):
        self._ctrl.command(0x17, left) #set column start address
        self._ctrl.command(0x18, right) #set column end address
        self._ctrl.command(0x19, top) #set row start address
        self._ctrl.command(0x1A, bottom) #set row end address

    def draw_full_image(self, image_data=None, color_data=None):
        """image_data = Imageinstance.getdata()
        color_data = bytesif the pixels in order."""
        print("BEFORE")
        self.set_drawing_box(0, 159, 0, 127)

        if not color_data:
            self._ctrl.command(0x22) #write to RAM command
            if not image_data:
                raise TypeError("image_data or color_data have to be not null")
            for i in range(128):
                color = []
                for j in range(160):
                    color += list(image_data[(i)*160+(j)])
                self._ctrl.command_extra_data(*color);

        else:
            print("doing fast way", len(color_data))
            self._ctrl.command(0x22, color_data)
            print('done fast way')
        print("AFTER")



def main():
    logging.basicConfig(level=logging.INFO)#DEBUG)
    color0 = [0,0xff,0]*(160*128)

    img1 = Image.open('/home/diamondman/Pictures/scaled_cait.png')
    rgb1 = img1.convert('RGB')
    dat1 = rgb1.getdata()
    color1 = bytes(list(chain(*list(dat1))))

    img2 = Image.open('/home/diamondman/Pictures/lizard.png')
    rgb2 = img2.convert('RGB')
    dat2 = rgb2.getdata()
    color2= bytes(list(chain(*list(dat2))))

    controller = USBParallelController(vendor_id=0x4b4, prod_id=0x1004)
    oled = Oled160128RGB_ParallelController(controller)
    oled.display_init()

    for i in range(10):
        #logging.info("ZEROth")
        #oled.draw_full_image(image_data=dat0)
        #oled.draw_full_image(color_data=color0)
        #time.sleep(0.1)

        #logging.info("FIRST")
        print("Drawing cait")
        #oled.draw_full_image(image_data=dat1)
        oled.draw_full_image(color_data=color1)
        time.sleep(0.5)

        print("DRAWING lizard")
        #logging.info("NEXT")
        #oled.draw_full_image(image_data=dat2)
        oled.draw_full_image(color_data=color2)
        time.sleep(0.5)

    logging.info("OFF")
    oled.display_onoff(False)

if __name__ == "__main__":
    main()
