/*
Copyright (c) 2014, Pure Engineering LLC
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


*******************************************
July 2017
Modified by Luke Van Horn for Lepton 3
 
 */


#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
    perror(s);
    abort();
}

static const char *device = "/dev/spidev0.1";
static uint8_t mode = SPI_CPOL | SPI_CPHA;
static uint8_t bits = 8;
static uint32_t speed = 16000000;
static uint16_t delay = 65535;

int8_t last_packet = -1;
uint8_t current_segment = 1;

int discarded = 0;
int total = 0;
int valid = 0;

#define VOSPI_FRAME_SIZE (164)
#define LEP_SPI_BUFFER (9840)

uint8_t lepton_frame_packet[VOSPI_FRAME_SIZE] = {0};
uint8_t rx_buf[LEP_SPI_BUFFER] = {0};
uint8_t tx_buf[LEP_SPI_BUFFER] = {0};
static unsigned int lepton_image[240][80];
int image_index = 0;

static void save_pgm_file(void)
{
    int i;
    int j;
    unsigned int maxval = 0;
    unsigned int minval = UINT_MAX;
    char image_name[32];

    do {
        sprintf(image_name, "images/IMG_%.4d.pgm", image_index);
        image_index += 1;
        if (image_index > 9999) 
        {
            image_index = 0;
            break;
        }

    } while (access(image_name, F_OK) == 0);

    FILE *f = fopen(image_name, "w+");
    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    printf("Calculating min/max values for proper scaling...\n");
    for(i = 0; i < 240; i++)
    {
        
        for(j = 0; j < 80; j++)
        {
            if (lepton_image[i][j] > maxval) {
                maxval = lepton_image[i][j];
            }
            if (lepton_image[i][j] < minval) {
                minval = lepton_image[i][j];
            }
        }
    }
    printf("maxval = %u\n",maxval);
    printf("minval = %u\n",minval);
    
    fprintf(f,"P2\n160 120\n%u\n",maxval-minval);
    for(i=0; i < 240; i += 2)
    {
        /* first 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            fprintf(f,"%d ", lepton_image[i][j] - minval);
        }

        /* second 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            fprintf(f,"%d ", lepton_image[i + 1][j] - minval);
        }        
        fprintf(f,"\n");
    }
    fprintf(f,"\n\n");

    fclose(f);

    //launch image viewer
    //execlp("gpicview", image_name, NULL);
}

int transfer(int fd)
{
    int ret;
    int i;
    int ip;
    uint8_t packet_number = 0;
    uint8_t segment = 0;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = NULL,
        .rx_buf = (unsigned long)rx_buf,
        .len = LEP_SPI_BUFFER,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits
    };    
    
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        pabort("can't read spi data");
    }
    
    for(ip = 0; ip < (LEP_SPI_BUFFER / 164); ip++) {
        memcpy(lepton_frame_packet, &rx_buf[ip * VOSPI_FRAME_SIZE], VOSPI_FRAME_SIZE);

        if((lepton_frame_packet[0] & 0x0f) == 0x0f) {
            continue;
        }
        
        packet_number = lepton_frame_packet[1];
        if(packet_number != (last_packet + 1)) {
            continue;
        }
        
        last_packet = (int8_t)packet_number;
        
        if(packet_number == 20) {
            segment = ((lepton_frame_packet[0] & 0x70) >> 4);
            if(segment != current_segment) {
                //printf("invalid segment: total: %d, valid: %d, discarded: %d, last_packet: %d, segment: %d, current_segment: %d\n", total, valid, discarded, last_packet, segment, current_segment);
                last_packet = -1;
                current_segment = 1;
                total = 0;
                continue;
            } 
            
            //printf("valid segment: total: %d, valid: %d, discarded: %d, last_packet: %d, segment: %d, current_segment: %d\n", total, valid, discarded, last_packet, segment, current_segment);

        }
    
        total = (packet_number + 1) + ((current_segment - 1) * 60);
            
        for(i = 0; i < 80; i++)
        {
            lepton_image[total - 1][i] = (lepton_frame_packet[(2*i)+4] << 8 | lepton_frame_packet[(2*i)+5]);
        }
        
        valid++;
        
        if(packet_number == 59) {
            current_segment += 1;
            last_packet = -1;
            //printf("total: %d, valid: %d, discarded: %d, packet_number: %d, last_packet: %d, segment: %d, current_segment: %d\n", total, valid, discarded, packet_number, last_packet, segment, current_segment);
        }
    }
    
    return total;
}
 
int main(int argc, char *argv[])
{
    int ret = 0;
    int fd;

    fd = open(device, O_RDWR);
    if (fd < 0)
    {
        pabort("can't open device");
    }

    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't set spi mode");
    }

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't get spi mode");
    }

    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't set bits per word");
    }

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't get bits per word");
    }

    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't set max speed hz");
    }

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't get max speed hz");
    }

    printf("spi mode: %d\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
    
    sleep(.2);  
    
    while(total != 240) { transfer(fd); }
    
    printf("total: %d, valid: %d\n", total, valid);
    
    close(fd);

    save_pgm_file();

    return ret;
}
