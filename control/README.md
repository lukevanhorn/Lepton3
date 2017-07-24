This communicates with the lepton 3 over I2C / TWI.  

It's still a work in progress, but some functions are working (reading most values and setting AGC enable/disable). 

My answer to dealing with big endian complexity (pi is LE) is to avoid using/converting between 16 bit values whenever possible.

### Defaults: 
I2C device 1: "/dev/i2c-1"

### Build 
```gcc control.c -o control```



