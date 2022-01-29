# PCB_exposure
Arduino code for UV exposure device
The device is used to transfer printed circuit patterns on a photoresist surface to create the printed circuit 
by etching the non-exposed surface(s). (depending on the resist film chemistry either exposed or non-exposed part
of the pattern can be etched away).
Main characteristics of the device:
 * Single or double sided circuits can be exposed for a preset time
 * "UV"-leds are used in the exposure. Wavelength range about 395-405 nm
 * PID controlled vacuum is applied to press the pattern against the photoresist resulting in sharp lines and contours.
 * a rotary encoder is used to input run parameters for the exposure
 * a start/stop button controls the run. Stop is used only if needed to stop the exposure before time-out.
 * an Arduinon Nano compatible microcontroller is used. Probably most Arduino flavors will work here.
