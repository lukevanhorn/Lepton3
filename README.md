Set of example code to capture data from Lepton 3 on the raspberry pi zero (and other models).

It seems the pi zero and earlier gen models have trouble keeping up with the data rate of the Lepton3.  My solution is to increase the device butter size to allow storing an entire frame of video and setting the speed to 16Mhz.

Samples:
    capture - this is the port of the pure engineering example image capture code.
    web - a self-contained web server that writes the output to a canvas element.
    control - issues commands over TWI to the lepton -- work in progress.

Build with 'gcc [filename].c -o [filename]'

### Increasing the SPI Buffer Size

Increase the spidev buffer size by adding "spidev.buffer=131072" to /boot/cmdline.txt

#### Steps
``` 
sudo /boot/cmdline.txt

Add "spidev.bufsiz=131072" to the arguments

ctrl+o to save
ctrl+x to exit

sudo reboot
```




