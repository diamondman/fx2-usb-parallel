fx2-usb-parallel
================

Generic firmware for the Cypress FX2lp controller to work with devices controlled with 6800 or 8080 parallel.

The purpose of this firmware is to allow people developing simple embedded systems to test devices that run over low voltage parallel. It is also a very simple firmware for learning how to work with the FX2. It runs on the Cypress FX2lp, and though I used the official dev kit, and setup should work.

The device id of chips flashed with this firmwarae are a default dev kit value, so very much not production ready, but that is very easy to change.

The firmware is unaware of what is attached to it, and it is up to whatever sends the firmware the uSB commands to write data to the parallel lines to make the data mean something to the target device.

Loading is easy and can be done with fxload or 
  sudo cycfx2prog prg:build/parallel.ihx run
