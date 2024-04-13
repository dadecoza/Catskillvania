CFLAGS = -Wall -g -std=c99 `pkg-config --cflags gtk+-3.0` -lpthread -lm -lgme 
LDFLAGS = `pkg-config --libs gtk+-3.0`


ifeq ($(OS),Windows_NT)
    CFLAGS += -mwindows
else
    CFLAGS += -ldl
endif

all: catskill 

catskill: catskillgfx.o catskillgame.o catskillmusic.o main.o
	$(CC) catskillmusic.o catskillgfx.o catskillgame.o main.o $(CFLAGS) $(LDFLAGS) -o catskill

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

catskillgfx.o: catskillgfx.c
	$(CC) -c $(CFLAGS) catskillgfx.c

catskillmusic.o: catskillmusic.c
	$(CC) -c $(CFLAGS) catskillmusic.c

clean:
	rm -f main.o catskillgfx.o catskillgame.o catskillmusic.o catskill catskill.exe 