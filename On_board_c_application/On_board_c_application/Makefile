CC = arm-linux-gnueabihf-gcc
CFLAGS = -static -Wall -O2
TARGET = video_server
SRC = video_server.c
BOARD_IP = 10.101.4.161

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

deploy: $(TARGET)
	scp -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa $(TARGET) root@$(BOARD_IP):/root/

clean:
	rm -f $(TARGET)
