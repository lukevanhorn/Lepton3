
TARGET = raspberrypi_capture
LIBS = -lm
CC = gcc
CFLAGS = -g -Wall -L./libs/Debug
INCPATH = -I. -I./libs 

.PHONY: default all clean

default: $(TARGET)
all: sdk default

sdk: make -C ./libs

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
	-rm -f ./libs/*.o  
	-rm -f ./libs/Debug/*.*
	
	

