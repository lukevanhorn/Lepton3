/*
    Free use

    Luke Van Horn, 2017
*/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <string.h>

#define LEP_I2C_DEVICE_ADDRESS 0x2A
#define I2C_PORT "/dev/i2c-1"

#define LEP_POWER_REG 0x00
#define LEP_STATUS_REG 0x02
#define LEP_COMMAND_REG 0x04

#define LEP_MOD_AGC 0x01
#define LEP_MOD_SYS 0x02
#define LEP_MOD_VID 0x03
#define LEP_MOD_OEM 0x80
#define LEP_MOD_RAD 0xE0

#define LEP_COM_GET 0x0
#define LEP_COM_SET 0x1
#define LEP_COM_RUN 0x2


uint8_t lep_status_reg[2] = { 0x00, 0x02 };
uint8_t lep_command_reg[2] = { 0x00, 0x04 };
uint8_t lep_data_reg[2] = { 0x00, 0x08 };

struct write_register {
    uint16_t address;
    uint16_t data; 
};

struct read_register {
    uint16_t address;
};



int device;
int8_t status_code;

uint8_t booted, boot_mode, busy;
uint8_t status[2] = {0};
//uint8_t command[4] = { 0x00, 0x04, 0x00, 0x00 };
//uint8_t length_reg[4] = { 0x00, 0x06 };
uint16_t rx_data[16] = {0};
uint16_t tx_data[16] = {0};

/* utility function for rpi to make big endian */
uint16_t regRE(uint16_t val) {
    uint16_t res = 0;
    res |= (val & 0x00FF) << 8;
    res |= (val & 0xFF00) >> 8;
    
    return res;
}

/* convert enums endianness */
int enumRE(int val) {
    int res = 0;
    res |= (val & 0x0000FF00) << 8;
    res |= (val & 0x000000FF) << 24;
    res |= (val & 0xFF000000) >> 24;
    res |= (val & 0x00FF0000) >> 8;
    
    return res;
}

/* reads the status register for boot, bootmode, and busy flags */
void checkStatus() {
   	if(write(device, lep_status_reg, 2) < 0) {
   	    printf("Error writing to i2c device\n");
   	    abort();
   	}

   	if(read(device, status, 2) < 0) {
   	    printf("Error reading from status register \n");
   	    abort();   	    
   	}

    status_code = (int8_t)status[0];    
    booted = ((status[1] & 0x04) >> 2);
    boot_mode = ((status[1] & 0x02) >> 1);
    busy = (status[1] & 0x01);

    //printf("status code: %d booted: %d boot mode: %d busy: %d\n", status_code, booted, boot_mode, busy);
    
    return;
}

/* polls the status until busy flag is reset or tries = 0 */
int waitForReady(int tries) {
    
    checkStatus();
    
    while(!booted || busy) {
        if(tries <= 0) {
       	    printf("Timeout waiting for device to be ready.  quitting. \n");
       	    return (booted | busy);
        }
        
        checkStatus();
        
        tries--;
    } 
    
    return busy;
}

/* issues read command and stores data in rx_data buffer 
   
   mod: [Module ID] (AGC, SYS, VID, OEM, RAD)
   com: [Command ID] (0x00, 0x08, 0x0C, etc...)
   len: [words to read] (16bit words)
*/
int readData(uint8_t mod, uint8_t com, uint16_t length) {

    int count = 0;
    struct write_register tx;    
    uint8_t command[4] = { 0x00, 0x00, 0x00, 0x00 };

    if(waitForReady(10)) {
        return -1;
    }
    
    
    tx.address = regRE(6);
    tx.data = regRE(length);
    write(device, &tx, sizeof(write_register));     

    /* set the oem bit if necessary */
    if(mod == LEP_MOD_OEM || mod == LEP_MOD_RAD) {
        mod |= 0x40;
    }
    
    tx.address = regRE(4);
    tx.data = ((0x00FF & mod) << 8) + com;
    write(device, &tx, sizeof(write_register));   

    /* set the data length register (big endian) */
    //command[1] = 0x06; /* data length register */
    //command[2] = (uint8_t)(length & 0x00FF);
    //command[3] = (uint8_t)((length & 0xFF00) >> 8);
    //write(device, command, 4);    
    
    /* set module id, command id, and command type */
    //command[1] = 0x04;  /* command register */
    //command[2] = 0x0F & mod;
    //command[3] = com + LEP_COM_GET;
    
    /* set the oem bit if necessary */
    //if(mod == LEP_MOD_OEM || mod == LEP_MOD_RAD) {
    //    command[2] |= 0x40;
    //}

    /* write to command register */
    //write(device, command, 4);
    
    if(waitForReady(10)) {
        return -1;
    }

    tx.address = regRE(8);
    write(device, &tx, 2);
    
    /* send the read command for the data register */
    //command[1] = 0x08;  /* data register */
    //write(device, command, 2);    
    
    /* read data bytes = 2 x word length*/
    count = read(device, rx_data, (length * 2));

   	if(count < 0) {
   	    printf("Error reading from data register \n");
   	    abort();   	    
   	}
   	
   	//todo: check crc
   	
   	return count;
}

/* issues read command and stores data in rx_data buffer 
   
   mod: [Module ID] (AGC, SYS, VID, OEM, RAD)
   com: [Command ID] (0x00, 0x08, 0x0C, etc...)
   len: [words to read] (16bit words)
*/
int writeData(uint8_t mod, uint8_t com, uint16_t * data, uint16_t length) {

    uint8_t i = 0;
    int count = 0;

    struct write_register tx;    

    uint8_t command[20] = {0};

    if(waitForReady(10)) {
        return -1;
    }

    /* set the data registers */


    while(i < length) {
        tx.address = regRE(8 + (i *2));
        tx.data = data[i++];

        write(device, &tx, sizeof(write_register));         
    } 

    tx.address = regRE(6);
    tx.data = regRE(length);
    write(device, &tx, sizeof(write_register));     

    /* set the oem bit if necessary */
    if(mod == LEP_MOD_OEM || mod == LEP_MOD_RAD) {
        mod |= 0x40;
    }
    
    tx.address = regRE(4);
    tx.data = ((0x00FF & mod) << 8) + com + 0x01;
    write(device, &tx, sizeof(write_register));     

    //command[1] = 0x08;
    //memcpy(&command[2], data, (length * 2));
    
    //write(device, command, (length * 2) + 2);       

    /* set the data length register (big endian) */
    //command[1] = 0x06; /* data length register */
    //command[2] = 0x00;
    //command[3] = length;
    //write(device, command, 4);    
    
    /* set module id, command id, and command type */
    //command[1] = 0x04;  /* command register */
    //command[2] = 0x0F & mod;
    //command[3] = com + 0x01;

    /* write to command register */
    //write(device, command, 4);
    
    if(waitForReady(10)) {
        return -1;
    }
   	
   	return status_code;
}

int main(int argc, char *argv[])
{
    int result;

    printf("opening device\n");
    
    device = open(I2C_PORT, O_RDWR);

    if(device < 0) {
        printf("Error Opening Device %d\n", device);
        abort();
    } 
    
    result = ioctl(device, I2C_SLAVE, LEP_I2C_DEVICE_ADDRESS);
    if(result < 0) {
        printf("Error Connecting to Device %d\n", device);
        abort();
    }
    
    printf("successfully opened port %d\n", result);
    
    if(readData(LEP_MOD_AGC, 0x00, 2) > 0) {
        printf("AGC enabled: %d\n", enumRE(rx_data));
    }
    
    tx_data[0] = enumRE(1);
    
    writeData(LEP_MOD_AGC, 0x00, tx_data, 2);
    printf("Set AGC Enable: %d\n", status_code);

    if(readData(LEP_MOD_AGC, 0x00, 2) > 0) {
        printf("AGC enabled: %d\n", enumRE(rx_data));
    }
    
    if(readData(LEP_MOD_AGC, 0x08, 4) > 0) {
        printf("AGC ROI: startcol: %d startrow: %d endcol: %d endrow: %d\n", regRE(rx_data), regRE(rx_data[1]), regRE(rx_data[2]), regRE(rx_data[3]));
    }
    
    if(readData(LEP_MOD_AGC, 0x0C, 4) > 0) {
        printf("AGC HIST: min: %d max: %d mean: %d num: %d\n", regRE(rx_data), regRE(rx_data[1]), regRE(rx_data[2]), regRE(rx_data[3]));
    }
    
    if(readData(LEP_MOD_AGC, 0x24, 1) > 0) {
        printf("AGC DAMPENING: %d\n", regRE(rx_data));
    }
    
    /* shutter position */
    if(readData(LEP_MOD_SYS, 0x38, 2) > 0) {
        printf("Shutter position: %d\n", enumRE(rx_data));
    }
    
    
    //printf("close shutter: %d\n", writeData(LEP_MOD_SYS, 0x38, tx_data, 2));
    
    return(result);
}