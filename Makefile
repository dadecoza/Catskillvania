CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0` -lm -lpthread
LDFLAGS = `pkg-config --libs gtk+-3.0`


ifeq ($(OS),Windows_NT)
    CFLAGS += -mwindows
else
    CFLAGS += -ldl
endif

all: main.c catskillgfx.c catskillgame.c
	$(CC) -o catskill main.c catskillgfx.c catskillgame.c miniaudio.h $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f catskill
