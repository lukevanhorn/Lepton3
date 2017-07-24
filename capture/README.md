Port of Pure Engineering example for the Lepton 3 module

Sample program that grabs a frame on the Pi and outputs it as a PGM file

### Defaults 
SPI channel 1 - update the "/dev/spidev0.1" value accordingly

### Build: 
```gcc capture.c -o capture```

### Note:
I've had to hard power-cycle the raspberry pi after changing values related to timing of SPI.  Otherwise, the capture stops working. Simply rebooting the pi or disconnecting the lepton hasn't worked for me.


