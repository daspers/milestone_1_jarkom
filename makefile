
CC = g++
CFLAG = -Wall --std=c++11

all: receiver sender

receiver: receiver.cpp
	${CC} ${CFLAG} receiver.cpp -o recvfile

sender: sender.cpp
	${CC} ${CFLAG} sender.cpp -o sendfile

clean:
	rm *.o