CFLAGS = -Wall -g -std=c99 `pkg-config --cflags gtk+-3.0` -lpthread -lm 
LDFLAGS = `pkg-config --libs gtk+-3.0`


ifeq ($(OS),Windows_NT)
    CFLAGS += -mwindows
else
    CFLAGS += -ldl
endif

all: catskill 

catskill: main.o catskillgfx.o catskillgame.o
	$(CC) main.o catskillgfx.o catskillgame.o $(CFLAGS) $(LDFLAGS) -o catskill

main.o: main.c catskillgfx.o catskillgame.o
	$(CC) -c $(CFLAGS) $(LDFLAGS) main.c

catskillgfx.o: catskillgfx.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) catskillgfx.c

catskillgame.o: catskillgame.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) catskillgame.c

clean:
	rm -f main.o catskillgfx.o catskillgame.o catskill catskill.exe