CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 libusb-1.0` -Wall
LIBS = `pkg-config --libs gtk+-3.0 libusb-1.0`

TARGET = casio_unlocker

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LIBS)

clean:
	rm -f $(TARGET)