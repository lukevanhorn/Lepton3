Set of examples to capture data from Lepton 3 on the raspberry pi zero (and other models).

It seems the pi zero and earlier gen models have trouble keeping up with the data rate of the Lepton3.  My solution is to increase the device butter size to allow storing an entire frame of video and setting the speed to 16Mhz.

### Samples
1. capture - this is the port of the pure engineering example image capture code.
2. web - a self-contained web server that writes the output to a canvas element.
3. control - issues commands over TWI to the lepton (work in progress)

### Build
```gcc [filename].c -o [filename]```

### Increasing the SPI Buffer Size
Increase the spidev buffer size by adding "spidev.buffer=131072" to /boot/cmdline.txt
``` 
sudo /boot/cmdline.txt

Add "spidev.bufsiz=131072" to the arguments

ctrl+o to save
ctrl+x to exit

sudo reboot
```




