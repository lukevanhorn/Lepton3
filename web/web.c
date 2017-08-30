/*
	Adapted from Beej's and lepton examples.  

	Luke Van Horn, 2017
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <sys/time.h>

#define _DEBUG 1

#define WWW_DIR "www"	

#define PORT    8080
#define MAXMSG  5120

#define BUFFER_SIZE 4096
 
char http_header_ok[] = "HTTP/1.1 200 OK\r\nSERVER: LeptonPi\r\nConnection: Close\r\n";
char http_header_404[] = "HTTP/1.1 404 Not Found\r\nSERVER: LeptonPi\r\nConnection: Close\r\n";
char http_header_content_html[] = "Content-Type: text/html\r\n\r\n";
char http_header_content_css[] = "Content-Type: text/css\r\n\r\n";
char http_header_content_txt[] = "Content-Type: text/plain\r\n\r\n";
char http_header_content_js[] = "Content-Type: application/javascript\r\n\r\n";
char http_header_content_json[] = "Content-Type: application/json\r\n\r\n";
char http_header_content_ico[] = "Content-Type: image/vnd.microsoft.icon\r\n\r\n";
char http_header_content_png[] = "Content-Type: image/png\r\n\r\n";
char http_header_content_plain[] = "Content-Type: text/plain\r\n\r\n";

char http_content_404[] = "<html><body><h1>404 Not Found</h1></body></html>";

struct http_request {
	char buffer[MAXMSG];
	char *pos;
    char content[100];      
    char type[100];    
};

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
static uint8_t status_bits = 0;

int8_t last_packet = -1;

#define VOSPI_FRAME_SIZE (164)
#define LEP_SPI_BUFFER (118080)
/* modify /boot/cmdline.txt to include spidev.bufsiz=131072 */

static uint8_t rx_buf[LEP_SPI_BUFFER] = {0};
static unsigned int lepton_image[240][80];

static int spi_fd;

void debug(const char *fmt, ...)
{
#ifdef _DEBUG
	va_list ap;
	va_start(ap, fmt);

	vprintf(fmt, ap);

	va_end(ap);
#endif
}

static void save_json_file(void)
{
    int i;
    int j;
    unsigned int maxval = 0;
    unsigned int minval = UINT_MAX;
    char image_name[32];
    int rgb_conv = 64;
    int rgb_val = 0;
    
    sprintf(image_name, "www/lepton.json");

    FILE *f = fopen(image_name, "w+");
    if (f == NULL)
    {
        debug("Error opening file!\n");
        exit(1);
    }

    debug("Calculating min/max values for proper scaling...\n");
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
    
    rgb_conv = 16383 / (maxval - minval);
    
    fprintf(f,"{ \"data\": [");
    for(i=0; i < 240; i += 2)
    {
        fprintf(f, "\n%s[",  i > 0 ? "," : "");  //start of row
        
        /* first 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            rgb_val = ((lepton_image[i][j] - minval) * rgb_conv) / 64;
            //printf(" %d,%d ", ((lepton_image[i][j] - minval) * rgb_conv) / 64, lepton_image[i][j] / 64);
            fprintf(f,"%s\"%x%x%x\"", j > 0 ? "," : "", rgb_val, rgb_val, rgb_val);            
        }

        /* second 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            rgb_val = ((lepton_image[i+1][j] - minval) * rgb_conv) / 64;
            fprintf(f,",\"%x%x%x\"", rgb_val, rgb_val, rgb_val);
        }   
        
        fprintf(f, "]");  //end of row
    }
    fprintf(f,"]}");

    fclose(f);
}


void http_request_init(struct http_request *request) 
{
	memset(request->buffer, 0, sizeof(request->buffer));
	memset(request->content, 0, sizeof(request->content));
	memset(request->type, 0, sizeof(request->type));

	request->pos = request->buffer;
}

struct http_request *http_request_create(void) 
{
	struct http_request *request = (struct http_request *)malloc(sizeof(struct http_request));
	if (!request) {
		return NULL;
	}

	http_request_init(request);

	return request;
}
 

void http_request_free(struct http_request *request) 
{
	free(request);
}

int make_socket (uint16_t port)
{
	int sock;
	struct sockaddr_in name;
     
	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("socket");
		exit (EXIT_FAILURE);
	}

	int enable = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}

	memset(&name, '0', sizeof(name));

	name.sin_family = AF_INET;
	name.sin_port = htons (port);
	name.sin_addr.s_addr = htonl(INADDR_ANY); 

	if (bind (sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		perror ("bind");
		exit (EXIT_FAILURE);
	}

	debug("bound to socket\n");
     
	return sock;
}

int send_image_data(char *content_request, char *content_type, int sock) {

	char * pos; 
	char imageBuffer[200000] = "\0";

	pos = &imageBuffer[0];

	pos += sprintf(pos,"{\"data\":[");

    for(i=0; i < 240; i += 2)
    {
		pos += sprintf(pos, "%s[%d",  (i > 0 ? "," : ""), lepton_image[i][0]);  //start of row
	
        /* first 80 pixels in row */
        for(j = 1; j < 80; j++)
        {
			pos += sprintf(pos, ",%d", lepton_image[i][j]);
        }

        /* second 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
			pos += sprintf(pos, ",%d", lepton_image[i+1][j]);
        }   
        
        pos += sprintf(pos, "]");  //end of row
    }
	
	pos += sprintf(pos,"]}");


	send(sock, http_header_ok, sizeof(http_header_ok), 0);	
	sprintf(content_length, "Content-Length: %d\r\n", file_length);
	send(sock, http_header_content_json, strlen(http_header_content_json), 0);		

	send(sock, imageBuffer, strlen(imageBuffer), 0);

	debug("sent image data: %d bytes\r\n", strlen(imageBuffer));

	return 1;
}

int get_content(char *content_request, char *content_type, int sock)
{
	int n;
	FILE *pFile;
	char *buffer;
	char fileBuffer[1024];
	char send_buffer[BUFFER_SIZE];
	int bytes_to_read, bytes_read, bytes_sent, bytes_remaining, b_len = 0;
	char *b_pos;
	char *response_header;
	char *response_content_type;
	size_t file_length, result;

	char filename[100]; 	
	char content_length[100];
	
	memset(&filename, 0, sizeof(filename));
	
	sprintf(filename, "%s%s", WWW_DIR, content_request);
	debug("open: %s\n", filename);
	pFile = fopen (filename, "r");

	if (pFile == NULL) {
		//send 404 not found
		perror("404 Not Found");
		send(sock,http_header_404,sizeof(http_header_404), 0);
		send(sock, http_header_content_html, strlen(http_header_content_html), 0);	
		send(sock,http_content_404,sizeof(http_content_404), 0);
		return -1;
	}
	
	//Send the header
	
	send(sock, http_header_ok, sizeof(http_header_ok), 0);	
	
	fseek(pFile , 0 , SEEK_END);
	file_length = ftell (pFile);
	rewind (pFile);
	
	sprintf(content_length, "Content-Length: %d\r\n", file_length);
	send(sock, content_length, strlen(content_length), 0);

	if (strstr(content_request, ".txt") > 0) {
		response_content_type = http_header_content_txt;
	} else if(strstr(content_request, ".html") > 0) {
		response_content_type = http_header_content_html;
	} else if(strstr(content_request, ".js") > 0) {
		response_content_type = http_header_content_js;
	} else if(strstr(content_request, ".css") > 0) {
		response_content_type = http_header_content_css;
	} else if(strstr(content_request, ".ico") > 0) {
		response_content_type = http_header_content_ico;
	} else if(strstr(content_request, ".png") > 0) {
		response_content_type = http_header_content_png;
	}
	
	send(sock, response_content_type, strlen(response_content_type), 0);	
		
	buffer = (char*) malloc(file_length + 1);
	bytes_read = fread(buffer,1,file_length,pFile);
	if(bytes_read == 0) {
		perror("Error Reading File");
		return -1;	
	}
	send(sock, buffer, bytes_read, 0);

	fclose (pFile);

	free(buffer);
	
	debug("sent file: %s %d %d\r\n",filename, file_length, bytes_read);

	return 1;
}

int parse_request_uri(struct http_request *request)
{
	char *pch;
	int len;

	request->pos = strstr(request->buffer, "GET ");
	if(request->pos == NULL) {
		return -1;
    }

	request->pos += 4;

	//copy uri - ignoring the parameters
	len = strcspn(request->pos, "? \n");
	strncpy(request->content, request->pos, len);
	request->pos += len;
	
	debug("content: %s\n", request->content);

	return 0;
}

int parse_request_type(struct http_request *request) 
{
	char *pch;
	int len;

	request->pos = strstr(request->buffer, "Accept: ");
	if(request->pos == NULL) {
		return -1;
    }

	request->pos += strlen("Accept: ");

	//copy first request type
	len = strcspn(request->pos, ", \n");
	strncpy(request->type, request->pos, len);
	request->pos += len;
	
	debug("type: %s\n", request->type);

	return 0;
}

int read_from_client (int filedes)
{
	int nbytes, assigned;

	struct http_request *request = http_request_create();
	
	int len, chan;
	
	char response_text[100];
	char *pch;
	
	while(1) {
		
		nbytes = read (filedes, request->buffer, MAXMSG);
		if (nbytes < 0) {
			perror ("read error");
			break;
		} else if (nbytes == 0) {
			break;
		} else {
			//parse packet

			debug("Server request: `%s'\n\n", request->buffer);

			if (parse_request_uri(request) < 0) {
				break;	
			}			
			
			if (parse_request_type(request) < 0) {
				break;		
			}					

			if(strlen(request->content) == 1) {
				get_content("/index.html","text/html", filedes);
				break;
			}

			//image request
			if(strncmp(request->content, "/lepton.json", strlen("/lepton.json")) == 0) {
			    send_image_data();                
			}
			
			get_content(request->content,request->type, filedes);

			break;
		}
		
	}

	http_request_free(request);

	return -1;
}
 
static const char *strnchr(const char *str, size_t len, char c)
{
   const char *e = str + len;
   do {
      if (*str == c) {
         return str;
      }
   } while (++str < e);
   return NULL;
}

int transfer()
{
    int ret;
    int i;
    int ip;
    uint8_t packet_number = 0;
    uint8_t segment = 0;
    uint8_t current_segment = 0;
    int packet = 0;
    int state = 0;  //set to 1 when a valid segment is found
    int pixel = 0;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)NULL,
        .rx_buf = (unsigned long)rx_buf,
        .len = LEP_SPI_BUFFER,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits
    };    
    
    //debug("looking for segments\n");
    status_bits = 0;
    
    ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        pabort("can't read spi data");
    }
    
    for(ip = 0; ip < (ret / VOSPI_FRAME_SIZE); ip++) {
        packet = ip * VOSPI_FRAME_SIZE;

        //check for invalid packet number
        if((rx_buf[packet] & 0x0f) == 0x0f) {
            state = 0;
            continue;
        }

        packet_number = rx_buf[packet + 1];
        
        if(packet_number > 0 && state == 0) {
            continue;
        }
        
        if(state == 1 && packet_number == 0) {
            state = 0;  //reset for new segment
        }
        
        //look for the start of a segment
        if(state == 0 && packet_number == 0 && (packet + (20 * VOSPI_FRAME_SIZE)) < ret) {
            segment = (rx_buf[packet + (20 * VOSPI_FRAME_SIZE)] & 0x70) >> 4;
            if(segment > 0 && segment < 5 && rx_buf[packet + (20 * VOSPI_FRAME_SIZE) + 1] == 20) {
                state = 1;
                current_segment = segment;
                //debug("new segment: %x \n", segment);
            } 
        }
        
        if(!state) {
            continue;
        }

        for(i = 4; i < VOSPI_FRAME_SIZE; i+=2)
        {
            pixel = packet_number + ((current_segment - 1) * 60);
            lepton_image[pixel][(i - 4) / 2] = (rx_buf[packet + i] << 8 | rx_buf[packet + (i + 1)]);
        }
        
        if(packet_number == 59) {
            //set the segment status bit
            status_bits |= ( 0x01 << (current_segment - 1));
        }        
    }
    
    //debug("found %x\n", status_bits);
        
    return status_bits;
}


int main (void)
{
	int sock;
	fd_set active_fd_set, read_fd_set;
	int i;
	struct sockaddr_in clientname;
	size_t size;
   struct timeval tv;
	
    int ret = 0;

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0)
    {
        pabort("can't open device");
    }

    ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't set spi mode");
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't get spi mode");
    }

    ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't set bits per word");
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't get bits per word");
    }

    ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't set max speed hz");
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't get max speed hz");
    }

    debug("spi mode: %d\n", mode);
    debug("bits per word: %d\n", bits);
	debug("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
    
    //while(status_bits != 0x0f) { transfer(); }
                
    //status_bits = 0x0f;
            
    //save_json_file();
    
	/* Create the socket and set it up to accept connections. */
	sock = make_socket (PORT);
	if (listen (sock, 1) < 0) {
		perror ("listen");
		exit (EXIT_FAILURE);
	}
     
	/* Initialize the set of active sockets. */
	FD_ZERO (&active_fd_set);
	FD_SET (sock, &active_fd_set);
     
    /* wait one second between select blocking calls - 
	   note: This isn't working on the pi and just loops continuously.  
	   todo: add a time check/delay to transfer() call in the loop
	*/
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
	while (1)
    {
        transfer();
		
		/* look for input on one or more active sockets. */
		read_fd_set = active_fd_set;
		if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, &tv) < 0)
        {
			perror ("select");
			exit (EXIT_FAILURE);
        }
     
		/* Service all the sockets with input pending. */
		for (i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET (i, &read_fd_set))
			{
				if (i == sock)
				{
					/* Connection request on original socket. */
					int new;
					size = sizeof(clientname);
					new = accept(sock,(struct sockaddr *) &clientname, &size);
					if (new < 0)
					{
						perror ("accept");
						exit (EXIT_FAILURE);
					}
					//fprintf(stderr, "Server: connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));

					FD_SET (new, &active_fd_set);
				}
				else
				{
					/* Data arriving on an already-connected socket. */
					if (read_from_client (i) < 0)
					{
						close (i);
						FD_CLR (i, &active_fd_set);close (i);
						FD_CLR (i, &active_fd_set);
					}
				}
			}
		}
	}
}
