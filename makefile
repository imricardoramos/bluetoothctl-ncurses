OBJS = main.o
SOURCE = main.c
HEADER =
OUT = bluetoothctl-ncurses
CC = gcc
FLAGS = -g -c -Wall
LFLAGS = -lmenu `pkg-config --libs ncurses`

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c

clean:
	rm -f $(OBJS) $(OUT)
