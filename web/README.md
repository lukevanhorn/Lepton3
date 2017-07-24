Based on Beej's socket examples, spitest example by Anton Vorontsov, Port of Pure Engineering example for the Lepton 3 module

This expands on the raspberrypi_capture example, writing the output to a javascript array structure and serving as a simple webserver to display.  The server will continuously read from the lepton3. Data will be written out when the javascript file is requested.

### Todo: 
1. skip writing to a file and stream from memory
2. stream changes over websockets to give a more realtime update
3. add a timer to delay reading from the SPI video stream. (select blocking delay isn't working on the pi).

### Defaults: 
1. SPI channel 1 - update the "/dev/spidev0.1" value accordingly
2. Port - 8080

### Build 
```gcc web.c -o web```





