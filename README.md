Port of Pure Engineering example for the Lepton 3 module (based on spitest example by Anton Vorontsov)

Sample program that grabs a frame on the Pi and outputs it as a PGM file, then launches gpicview

Defaults to SPI channel 1 - update the "/dev/spidev0.1" value accordingly

Build with 'make'

Increase the spidev buffer size by adding "spidev.buffer=131072" to /boot/cmdline.txt

Steps: 

sudo /boot/cmdline.txt

Add "spidev.bufsiz=131072" to the arguments

ctrl+o to save
ctrl+x to exit

sudo reboot




