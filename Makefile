CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0` -lm -lpthread -mwindows
LDFLAGS = `pkg-config --libs gtk+-3.0`

all: main.c
	$(CC) -o catskill main.c catskillgfx.c catskillgame.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f catskill