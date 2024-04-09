// Game & graphics driver for gameBadgePico (MGC 2023)
#include "catskillgfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

guchar playfield[ROWS * COLS * BYTES_PER_PIXEL];

FILE *file;

bool fileActive = false; // True =  a file is open (reading level, playing audio) False = file not open, available for use

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

static uint16_t linebuffer[2][3840]; // Sets up 2 buffers of 120 longs x 16 lines high
                                     //--------Y---X--- We put the X value second so we can use a pointer to rake the data
static uint16_t nameTable[32][32];   // 4 screens of tile data. Can scroll around it NES-style (LCD is 15x15, tile is 16X16, slightly larger) X is 8 bytes wider to hold the palette reference (similar to NES but with 1 cell granularity)
static uint16_t patternTable[8192];  // 256 char patterns X 8 lines each, 16 bits per line. Same size as NES but chunky pixel, not bitplane and stored in shorts
static uint16_t spriteBuffer[14400];
uint16_t paletteRGB[64];         // Stores the 32 colors as RGB values, pulled from nesPaletteRGBtable (with space for 32 extra)
uint16_t nesPaletteRGBtable[64]; // Stores the current NES palette in 16 bit RGB format

#define spriteAlphaColor 0x0001 // The 16-bit LCD RGB color that is used for sprite transparency (make sure nothing in the palette matches it)

uint16_t baseASCII = 32;            // Stores what tile in the pattern table is the start of printable ASCII (space, !, ", etc...) User can change the starting position to put ASCII whereever they want in pattern table, but this is the default
uint8_t textWrapEdges[2] = {0, 14}; // Sets a left and right edge where text wraps in the tilemap. Use can change this, default is left side of scroll, one screen wide
bool textWordWrap = true;           // Is spacing word-wrap enabled (fancy!)

uint8_t winX[32];
uint8_t winXfine[32];
uint8_t winY = 0;
uint8_t winYfine = 0;
uint8_t winYJumpList[16];  // One entry per screen row. If flag set, winY coarse jumps to nametable row specified and yfinescroll is reset to 0
uint8_t winYreset = 0;     // What coarse Y rollovers to in nametable when it reaches winYrollover
uint8_t winYrollover = 31; // If coarse window Y exceeds this, reset to winYreset. Use this drawing status bars or creating vertical margins for word wrap
int xLeft = -1;            // Sprite window limits. Use this to prevent sprites from drawing over status bar (reduce yBottom by 12)
int xRight = 120;
int yTop = -1;
int yBottom = 120;

uint32_t audioSamples = 0;
uint8_t whichAudioBuffer = 0;
bool audioPlaying = false;

#define audioBufferSize 512
uint32_t audioBuffer0[audioBufferSize] __attribute__((aligned(audioBufferSize)));
uint32_t audioBuffer1[audioBufferSize] __attribute__((aligned(audioBufferSize)));

bool fillBuffer0flag = false;
bool fillBuffer1flag = false;
bool endAudioBuffer0flag = false;
bool endAudioBuffer1flag = false;
int audio_pin_slice;

bool backlightOnFlag = false;

int currentAudioPriority = 0;

const int notes[] = {
    261, 277, 293, 311, 329, 349, 369, 391, 415, 440, 466, 493};

int indexToGPIO[9] = {7, 11, 9, 13, 21, 5, 4, 3, 2}; // Button index is UDLR SEL STR A B C  this maps that to the matching GPIO

volatile uint8_t buttonValue = 0;
bool debounce[10] = {false, false, false, false, true, true, true, true, true, true}; // Index of which buttons have debounce (button must open before it can re-trigger)
uint8_t debounceStart[10] = {0, 0, 0, 0, 5, 5, 1, 1, 1, 0};                           // If debounce, how many frames must button be open before it can re-trigger.
uint8_t debounceTimer[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};                           // The debounceStart time is copied here, and debounceTimer is what's decrimented

// Used to pre-store GPIO-to-channel numbers for PWM (5th channel reserved for PCM audio wave files)
uint8_t slice_numbers[4];
uint8_t chan_nummbers[4];

void setButton(uint8_t b)
{
    switch (b)
    {
    case 97:
    case 81:
        buttonValue = left_but;
        break;
    case 119:
    case 82:
        buttonValue = up_but;
        break;
    case 100:
    case 83:
        buttonValue = right_but;
        break;
    case 115:
    case 84:
        buttonValue = down_but;
        break;
    case 131:
    case 13:
    case 233:
        buttonValue = A_but;
        break;
    case 32:
        buttonValue = C_but;
        break;
    case 227:
    case 228:
        buttonValue = B_but;
        break;
    default:
        buttonValue = no_but;
    }
}

// Returns a boolean of the button state, using debounce settings
bool button(uint8_t which)
{ // A, B, C, select, start, up, down, left, right
    // printf("Which: %d, buttonValue: %d\n", which, buttonValue);
    if (debounce[which] == 1)
    { // Switch has debounce?
        if (debounceTimer[which] == 0)
        { // Has timer cleared?
            if (buttonValue == which)
            {                                                // OK now you can check for a new button press
                debounceTimer[which] = debounceStart[which]; // Yes? Set new timer
                return true;                                 // and return button pressed
            }
        }
    }
    else
    {
        if (buttonValue == which)
        { // Button pressed? That's all we care about
            return true;
        }
    }
    return false;
}

void initGfx()
{
    int i;
    int p = 0;
    for (i = 0; i < (ROWS * COLS); i++)
    {
        playfield[p++] = 0;
        playfield[p++] = 0;
        playfield[p++] = 0;
    }
}

// Enables/disables button debounce. If useDebounce = true, button must be released for frames before it can be re-triggered (usually ABC, or D-pad during menus)
void setButtonDebounce(int which, bool useDebounce, uint8_t frames)
{

    debounce[which] = useDebounce;

    if (useDebounce == true && frames < 1)
    { // If debounce you must have at minimum 1 frame of debounce time (at least one frame off before can retrigger)
        frames = 1;
    }
    else
    {
        frames = frames * 4;
    }

    debounceStart[which] = frames;
    debounceTimer[which] = frames; // Reset this as if a button has been pressed. This allows us to switch without false edges
}

// Must be called once per frame to service the button debounce
void serviceDebounce()
{ // Must be called every frame, even if paused (because the buttons need to work to unpause!)
    for (int x = 0; x < 9; x++)
    { // Scan all buttons
        if (debounce[x] == 1)
        { // Button uses debounce?
            if (debounceTimer[x] > 0)
            { // Is the value above 0? That means button was pressed in the past
                if (buttonValue != no_but)
                {                       // Has button opened?
                    debounceTimer[x]--; // If so, decrement debounce while it's open. When debounce reaches 0, it can be re-triggered under button funtion
                }
            }
        }
    }
}

// Loads the YY-CHR 64 color palette file (.pal) from file into RAM. This must be called before calling loadPalette as that uses this index to fill tables. Only need to call this once after boot (color selection palette is static)
bool loadRGB(const char *path)
{
    FILE *file;
    file = fopen(path, "rb");
    if (!file)
    {
        printf("Unable to open RGB palette file!\n");
        return false;
    }
    char r, g, b;
    for (int x = 0; x < 64; x++)
    {
        fread(&r, sizeof(r), 1, file);
        fread(&g, sizeof(g), 1, file);
        fread(&b, sizeof(b), 1, file); // Convert each 3 byte 24-bit RGB value from disk to 16-bit color in RAM that uses same color space as ST7789 LCD
        updatePaletteRGB(x, r, g, b);  // We do as much math up front during loads to save time during frame render (trading RAM for speed)
    }
    fclose(file);
    return true;
}

// Loads  the YY-CHR palette data file (.dat) from file into RAM. There are 8 palettes of 4 colors each (sprites use color 0 as transparency, tiles can set 4 unique colors)
void loadPalette(const char *path)
{
    FILE *file;
    file = fopen(path, "rb");
    if (!file)
    {
        printf("Unable to open palette file!\n");
        return;
    }
    char c;
    for (int x = 0; x < 64; x++)
    {
        fread(&c, sizeof(c), 1, file);
        updatePalette(x, c); // Use palette.dat file to create an RGB reference for all 32 colors
    }
    fclose(file);
    // This loads 32 byte indexes from disk that point to the table loaded by loadRGB, ie: if palette 0, color 0 (first byte in file) contains a 1, it
    // is referencing the second byte (index[1]) of the nesPaletteRGBtable[] table.
}

// To save math we copy the RGB values (as a 16-bit number) to the paletteRGB index.
void updatePalette(int position, int theIndex)
{ // Allows game code to update palettes off flash/SD

    paletteRGB[position] = nesPaletteRGBtable[theIndex];
    // Updates the palette table with the 16-bit value stored in the master list of available colors (that were loaded in with loadRGB)
    // You can also use this function for dynamic palette changes, like the Mega Man 2 waterfall
}

// Converts a 24-bit RGB value (such as RGB file) to the 16-bit color of the ST7789 LCD
void updatePaletteRGB(int position, char r, char g, char b)
{ // Allows game code to update palettes off flash/SD

    uint16_t red = r;
    uint16_t green = g;
    uint16_t blue = b;

    red &= 0xF8;   // Lob off bottom 3 bits
    red <<= 8;     // Pack to MSB
    green &= 0xFC; // Lob off bottom 2 bits (because evolution)
    green <<= 3;   // Pack to middle
    blue &= 0xF8;  // Lobb off bottom 3 bits
    blue >>= 3;    // Pack to LSB

    nesPaletteRGBtable[position] = red | green | blue; // Put the 16-bit color value in the palette RGB index (max 64 colors)
}

// Loads a YY-CHR .nes file (pattern table data, aka graphics) from disk and stores it in Pico RAM
void loadPattern(const char *path, uint16_t start, uint16_t length)
{ // Loads a pattern file into memory. Use start & length to only change part of the pattern, like of like bank switching!

    unsigned char lowBit[8];
    unsigned char highBit[8];
    FILE *file;
    file = fopen(path, "rb");
    if (!file)
    {
        printf("Unable to open pattern file!\n");
        return;
    }

    for (uint16_t numChar = start; numChar < (start + length); numChar++)
    { // Read a bitplane filestream created by YY-CHR
        fread(lowBit, sizeof(lowBit), 1, file);
        fread(highBit, sizeof(highBit), 1, file);
        convertBitplanePattern(numChar << 3, lowBit, highBit); // Pass array points to function that will convert to chunky pixels
    }
    fclose(file);
    // NES pattern tables used planer (bitplane) graphics. Each pixel was represented by 2 bits (thus 4 colors) but the bits were not next to each other in memory
    // They were stored at different offsets (planer) This was used by a lot of computers in the 80's such as the Atari ST and Amiga. A 4 bit (16 color) image was
    // stored in memory as (4) separate 1 bit patterns and combined by video hardware https://en.wikipedia.org/wiki/Planar_(computer_graphics)
    // Later systems such as the Master System/Genesis used "chunky" pixels in which multiple bits used to define color were stored next to each other in the same byte

    // By default gamebadge has 8192 words of pattern data (chunky pixel) which equals (4) 16x16 tile patterns that can be stored in memory at once (twice that of the NES)
    // Unlike the NES you can either any tile for either spites or tiles
}

// In tilemap position X/Y, draw the tile at index "whattile" from the pattern table using whatPalette (0-7)
void drawTile(int xPos, int yPos, uint16_t whatTile, char whatPalette, int flags)
{

    flags &= 0xF8; // You can use the top 5 bits for attributes, we lob off anything below that (the lower 3 bits are for tile palette)

    nameTable[yPos][xPos] = (flags << 8) | (whatPalette << 8) | whatTile; // Palette points to a grouping of 4 colors, so we shift the palette # 2 to the left to save math during render
}

// In tilemap position X/Y, draw the tile at position tileX to the right, tileY down, from the pattern table using whatPalette (0-7)
void drawTileXY(int tileX, int tileY, uint16_t patternX, uint16_t patternY, char whatPalette, int flags)
{

    flags &= 0xF8; // You can use the top 5 bits for attributes, we lob off anything below that (the lower 3 bits are for tile palette)

    uint16_t whatTile = (patternY * 16) + patternX; // Do this math for the user :)

    nameTable[tileY][tileX] = (flags << 8) | (whatPalette << 8) | whatTile; // Palette points to a grouping of 4 colors, so we shift the palette # 2 to the left to save math during render
}

// Sets flags on a tile (blocking, platform, etc). By default drawTile sets these as 0. You must use setTileType after using drawTile
void setTileType(int tileX, int tileY, int flags)
{

    flags &= 0xF8; // You can use the top 5 bits for attributes, we lob off anything below that (the lower 3 bits are for tile palette)

    nameTable[tileY][tileX] &= 0x07FF; // AND off the top 5 bits

    nameTable[tileY][tileX] |= (flags << 8); // OR it into the tile in nametable
}

void tileDirect(int tileX, int tileY, uint16_t theData)
{ // Copies data directy into tile map

    nameTable[tileY][tileX] = theData;
}

void setTilePalette(int tileX, int tileY, int whatPalette)
{

    whatPalette <<= 8; // Bitshift it over

    whatPalette &= 0x0300; // Ensure it won't overwrite anything else

    nameTable[tileY][tileX] &= 0xF8FF; // Erase those 3 bits from the tile attb

    nameTable[tileY][tileX] |= whatPalette; // OR in new color
}

// Retrieves the flags set by the function above (bit-shifted back into the low byte) Use this to detect collisions, spikes and stuff
int getTileType(int tileX, int tileY)
{

    return (nameTable[tileY][tileX] >> 8) & 0xF8;
}

// Retrieves the value of the tile itself. Use this to pull text from the screen like password entry
int getTileValue(int tileX, int tileY)
{

    return nameTable[tileY][tileX] & 0xFF; // Lob off top, return lower byte
}

// Used to fill or clear a large amount of tiles (should do on boot in case garbage in RAM though in theory arrays should boot at 0's)
void fillTiles(int startX, int startY, int endX, int endY, uint16_t whatTile, char whatPalette)
{

    endX++;
    endY++;

    for (int y = startY; y < endY; y++)
    {
        for (int x = startX; x < endX; x++)
        {
            drawTile(x, y, whatTile, whatPalette, 0);
        }
    }
}

// Used to fill or clear a large amount of tiles (should do on boot in case garbage in RAM though in theory arrays should boot at 0's)
void fillTilesXY(int startX, int startY, int endX, int endY, uint16_t patternX, uint16_t patternY, char whatPalette)
{

    endX++;
    endY++;

    uint16_t whatTile = (patternY * 16) + patternX; // Do this math for the user :)

    for (int y = startY; y < endY; y++)
    {
        for (int x = startX; x < endX; x++)
        {
            drawTile(x, y, whatTile, whatPalette, 0);
        }
    }
}

// By default drawing decimal/text assumes printable ASCII starts at pattern tile 32 (space) Use this function if you put ASCII elsewhere in your pattern table
void setASCIIbase(uint16_t whatBase)
{

    baseASCII = whatBase;
}

// If drawing text with doWrap = true, this sets the left and right margins in the name table (min 0, max 31)
void setTextMargins(int left, int right)
{

    textWrapEdges[0] = left;
    textWrapEdges[1] = right;

    // If you want stuff to wrap within a static screen, you'd use 0, 14 (the default)
    // If you want to fill the name table with text, such as frame a webpage, you could set 0, 31 and then scroll left and right to read all text
    // Text carriage returns wrap within the same confines defined by setCoarseYRollover(int topRow, int bottomRow);
    // Text that goes past bottomRow will wrap to topRow
}

// Draws a string of horizontal text at position x,y in the tilemap and if doWrap=true follows word wrap rules, else just draws from left to right
void drawText(const char *text, uint8_t x, uint8_t y, bool doWrap)
{

    // gpio_put(15, 1);

    if (doWrap)
    {

        bool cr = false; // Set TRUE to perform a carriage return (several things can cause this hence the flag)

        while (*text)
        {
            if (*text == ' ')
            { // If current character is a space, then we need to do special checks
                // Serial.print("Space found. ");
                if (x == textWrapEdges[0])
                { // No reason to draw spaces on left margin
                    // Serial.println("Left margin skip");
                    text++; // Advance pointer but not X
                }
                else
                { // Not on left margin?
                    if (x == textWrapEdges[1])
                    { // Does this space land on the right margin?
                        // Serial.println("On right margin. Draw space and do CR");
                        nameTable[y][x] = *text++; // Draw the space...
                        cr = true;                 // and set carriage return for next char
                    }
                    else
                    { // OK so there is at least 1 character of space after this space " "
                        // Serial.print("Next word='");
                        int charCount = 0;         // Let's count the characters in the next word
                        nameTable[y][x++] = *text; // Draw the space and advance x

                        /*
                        while (*text == 32)
                        { // Move text past the space and keep going until we find a non-space
                            *text++;
                        }
                        */
                        // Compiler is complaining
                        if (*text == 0)
                        {           // Ok we're past all the damn spaces. Did we hit end of string because some asshole typed "hello world!   " ?
                            return; // Fuck it we're done!
                        }

                        while (*text != 32 && *text != 0)
                        { // Look for the next space or end of string
                            // Serial.write(*text);						//Debug
                            charCount++; // Count how many chars we find in front of next space or end
                            text += 1;   // Advance the pointer
                        }

                        // Serial.print("' length = ");
                        // Serial.print(charCount, DEC);
                        // Serial.print(" WRAP? ");
                        text -= charCount; // Rollback pointer to start of word because we still need to draw it
                        if (charCount > ((textWrapEdges[1] + 1) - x))
                        { // Check if enough characters to draw this word (offset +1 because we need count not index)
                            // Serial.println("YES");						//Have to make sure stuff like " to" or " I" renders
                            cr = true;
                        }
                        else
                        {
                            // Serial.println("NO");
                        }
                    }
                }
            }
            else
            {
                if (x == textWrapEdges[1])
                {           // Will this character land on the right margin?
                    text++; // Check next character

                    if (*text == ' ')
                    {                              // Single character with a space after? "a ", " I"...
                        text--;                    // Backup pointer
                        nameTable[y][x] = *text++; // and print it, advancing the pointer again. Next line will see the space and skip it since beginning of line
                    }
                    else
                    {
                        nameTable[y][x] = '-'; // Draw a hyphen
                        text--;                // Backup pointer so the character that wasn't ' ' will print on next line
                    }
                    cr = true;
                }
                else
                {
                    nameTable[y][x++] = *text++; // Standard draw
                }
            }

            if (cr == true)
            { // Flag to do carriage return?
                // Serial.println("Carriage Return");
                cr = false;           // Clear flag
                x = textWrapEdges[0]; // Jump to left margin
                if (++y > winYrollover)
                { // Advance Y row and rollover if it goes past bottom limit
                    y = winYreset;
                }
            }
            // Serial.print(x, DEC);
            // Serial.print(" ");
            // Serial.println(y, DEC);
        }
    }
    else
    {
        while (*text)
        {
            nameTable[y][x] &= 0xFF00;    // Retain attb and color bits
            nameTable[y][x++] |= *text++; // Just draw the characters from left to write
        }
    }

    // gpio_put(15, 0);
}

// Draws a string of horizontal text at position x,y in the sprite layer
void drawSpriteText(const char *text, uint8_t x, uint8_t y, int whatPalette)
{

    while (*text)
    {
        drawSpriteTile(x, y, *text++, whatPalette, 0, 0);
        x += 8;
    }
}

void drawDecimalXY(int32_t theValue, uint8_t x, uint8_t y)
{ // Send up to a 9 digit decimal value to memory

    int zPad = 0;                 // Flag for zero padding
    uint32_t divider = 100000000; // Divider starts at 900 million = max decimal printable is 999,999,999

    for (int xx = 0; xx < 9; xx++)
    { // 9 digit number
        if (theValue >= divider)
        {
            nameTable[y][x++] = '0' + (theValue / divider);
            theValue %= divider;
            zPad = 1;
        }
        else if (zPad || divider == 1)
        {
            nameTable[y][x++] = '0';
        }
        divider /= 10;
    }
}

void drawDecimal(int32_t theValue, uint8_t y)
{ // Same as above but centers the number on screen. Good for end of level scores

    int zPad = 0;                 // Flag for zero padding
    uint32_t divider = 100000000; // Divider starts at 900 million = max decimal printable is 999,999,999

    char theText[10];
    theText[0] = '0'; // Hopefully you don't suck this bad, but if you do...
    for (int g = 1; g < 10; g++)
    { // Ensure zero termination
        theText[g] = 0;
    }

    int x = 0;

    for (int xx = 0; xx < 9; xx++)
    { // 9 digit number
        if (theValue >= divider)
        {
            theText[x++] = '0' + (theValue / divider);
            theValue %= divider;
            zPad = 1;
        }
        else if (zPad || divider == 1)
        {
            theText[x++] = '0';
        }
        divider /= 10;
    }

    x--; // Get rid of last inc

    int xPos = (15 - x) / 2; // Center text
    drawText(theText, xPos, y, false);
}

void drawSpriteDecimal(int32_t theValue, uint8_t x, uint8_t y, int whatPalette)
{ // Send up to a 9 digit decimal value to memory

    int zPad = 0;                 // Flag for zero padding
    uint32_t divider = 100000000; // Divider starts at 900 million = max decimal printable is 999,999,999

    for (int xx = 0; xx < 9; xx++)
    { // 9 digit number
        if (theValue >= divider)
        {
            drawSpriteTile(x, y, '0' + (theValue / divider), whatPalette, 0, 0);
            theValue %= divider;
            zPad = 1;
            x += 8;
        }
        else if (zPad || divider == 1)
        {
            drawSpriteTile(x, y, '0', whatPalette, 0, 0);
            x += 8;
        }
        divider /= 10;
    }
}

void drawSpriteDecimalRight(int32_t theValue, uint8_t xRight, uint8_t y, int whatPalette)
{ // Send up to a 9 digit decimal value to memory

    int zPad = 0;                 // Flag for zero padding
    uint32_t divider = 100000000; // Divider starts at 900 million = max decimal printable is 999,999,999

    char theText[10];
    theText[0] = '0'; // Hopefully you don't suck this bad, but if you do...
    for (int g = 1; g < 10; g++)
    { // Ensure zero termination
        theText[g] = 0;
    }

    int x = 0;

    for (int xx = 0; xx < 9; xx++)
    { // 9 digit number
        if (theValue >= divider)
        {
            theText[x++] = '0' + (theValue / divider);
            theValue %= divider;
            zPad = 1;
        }
        else if (zPad || divider == 1)
        {
            theText[x++] = '0';
        }
        divider /= 10;
    }

    x--; // Get rid of last inc

    do
    {
        drawSpriteTile(xRight, y, theText[x--], whatPalette, 0, 0);
        xRight -= 8;
    } while (x > -1);
}

// Draws a sprite at xPos/yPos using a range of tiles from the pattern table, starting at tileX/Y, ending at xWide/yHigh, using whichpalette and flipped V/H if true
void drawSpriteRange(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint16_t xWide, uint16_t yHigh, uint8_t whichPalette, bool hFlip, bool vFlip)
{

    int tileYstart = tileY;
    int tileYend = tileY + yHigh;
    int tileYdir = 1;

    if (vFlip)
    {
        tileYstart = tileY + yHigh;
        tileYend = tileY;
        tileYdir = -1;
    }

    for (int yTiles = tileYstart; yTiles != tileYend; yTiles += tileYdir)
    {

        int xPosTemp = xPos;

        if (hFlip)
        {
            for (int xTiles = (tileX + (xWide - 1)); xTiles > (tileX - 1); xTiles--)
            {
                drawSpriteSingle(xPosTemp, yPos, xTiles, yTiles, whichPalette, true, vFlip); // Use other function to draw a series of 8x8 sprites
                xPosTemp += 8;
            }
        }
        else
        {
            for (int xTiles = tileX; xTiles < (tileX + xWide); xTiles++)
            {
                drawSpriteSingle(xPosTemp, yPos, xTiles, yTiles, whichPalette, false, vFlip); // Use other function to draw a series of 8x8 sprites
                xPosTemp += 8;
            }
        }

        yPos += 8;
    }
}

// Draws a single 8x8 sprite at xPos/yPos using a tile from the pattern table at tileX/Y, using whichpalette and flipped V/H if true
void drawSpriteSingle(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint8_t whichPalette, bool hFlip, bool vFlip)
{

    // gpio_put(15, 1);

    tileX <<= 3;
    tileY <<= 7;

    int lineYdir = 1; // Normal sprites
    int vFlipOffset = 0;

    if (vFlip)
    { // Vertical flip
        lineYdir = -1;
        vFlipOffset = 7;
    }

    if (hFlip)
    {
        uint16_t *sp = &spriteBuffer[(yPos * 120) + xPos]; // Use pointer so less math later on
        uint16_t *tilePointer = &patternTable[tileX + tileY + vFlipOffset];

        for (int yPixel = yPos; yPixel < (yPos + 8); yPixel++)
        {

            uint16_t temp = *tilePointer; // Get pointer for sprite tile

            for (int xPixel = xPos; xPixel < (xPos + 8); xPixel++)
            {

                uint8_t twoBits = temp & 0x03; // Get lowest 2 bits

                // For a sprite pixel to be drawn, it must be within the current window, not a 0 index color, and not over an existing sprite pixel
                if (xPixel > xLeft && xPixel < xRight && yPixel > yTop && yPixel < yBottom && twoBits != 0 && *sp == spriteAlphaColor)
                {
                    *sp = paletteRGB[(whichPalette << 2) + twoBits];
                }
                sp++;
                temp >>= 2; // Shift for next 2 bit color pair
            }

            tilePointer += lineYdir;
            sp += (120 - 8);
        }
    }
    else
    {
        uint16_t *sp = &spriteBuffer[(yPos * 120) + xPos]; // Use pointer so less math later on

        uint16_t *tilePointer = &patternTable[tileX + tileY + vFlipOffset];

        for (int yPixel = yPos; yPixel < (yPos + 8); yPixel++)
        {

            uint16_t temp = *tilePointer; // Get pointer for sprite tile

            for (int xPixel = xPos; xPixel < (xPos + 8); xPixel++)
            {

                uint8_t twoBits = temp >> 14; // Smash this down into 2 lowest bits

                // For a sprite pixel to be drawn, it must be within the current window, not a 0 index color, and not over an existing sprite pixel
                if (xPixel > xLeft && xPixel < xRight && yPixel > yTop && yPixel < yBottom && twoBits != 0 && *sp == spriteAlphaColor)
                {
                    *sp = paletteRGB[(whichPalette << 2) + twoBits];
                }
                sp++;
                temp <<= 2; // Shift for next 2 bit color pair
            }

            tilePointer += lineYdir;
            sp += (120 - 8);
        }
    }

    // gpio_put(15, 0);
}

// Draws a single 8x8 sprite at xPos/yPos using whichTile # from the pattern table, using whichpalette and flipped V/H if true
void drawSpriteTile(int xPos, int yPos, uint16_t whichTile, uint8_t whichPalette, bool hFlip, bool vFlip)
{

    // gpio_put(15, 1);

    whichTile <<= 3; // Multiple by 8

    int lineYdir = 1; // Normal sprites
    int vFlipOffset = 0;

    if (vFlip)
    { // Vertical flip
        lineYdir = -1;
        vFlipOffset = 7;
    }

    if (hFlip)
    {
        uint16_t *sp = &spriteBuffer[(yPos * 120) + xPos]; // Use pointer so less math later on
        uint16_t *tilePointer = &patternTable[whichTile + vFlipOffset];

        for (int yPixel = yPos; yPixel < (yPos + 8); yPixel++)
        {

            uint16_t temp = *tilePointer; // Get pointer for sprite tile

            for (int xPixel = xPos; xPixel < (xPos + 8); xPixel++)
            {

                uint8_t twoBits = temp & 0x03; // Get lowest 2 bits

                // For a sprite pixel to be drawn, it must be within the current window, not a 0 index color, and not over an existing sprite pixel
                if (xPixel > xLeft && xPixel < xRight && yPixel > yTop && yPixel < yBottom && twoBits != 0 && *sp == spriteAlphaColor)
                {
                    *sp = paletteRGB[(whichPalette << 2) + twoBits];
                }
                sp++;
                temp >>= 2; // Shift for next 2 bit color pair
            }

            tilePointer += lineYdir;
            sp += (120 - 8);
        }
    }
    else
    {
        uint16_t *sp = &spriteBuffer[(yPos * 120) + xPos]; // Use pointer so less math later on

        uint16_t *tilePointer = &patternTable[whichTile + vFlipOffset];

        for (int yPixel = yPos; yPixel < (yPos + 8); yPixel++)
        {

            uint16_t temp = *tilePointer; // Get pointer for sprite tile

            for (int xPixel = xPos; xPixel < (xPos + 8); xPixel++)
            {

                uint8_t twoBits = temp >> 14; // Smash this down into 2 lowest bits

                // For a sprite pixel to be drawn, it must be within the current window, not a 0 index color, and not over an existing sprite pixel
                if (xPixel > xLeft && xPixel < xRight && yPixel > yTop && yPixel < yBottom && twoBits != 0 && *sp == spriteAlphaColor)
                {
                    *sp = paletteRGB[(whichPalette << 2) + twoBits];
                }
                sp++;
                temp <<= 2; // Shift for next 2 bit color pair
            }

            tilePointer += lineYdir;
            sp += (120 - 8);
        }
    }

    // gpio_put(15, 0);
}

// Wipes entire sprite buffer with the transparent color
void clearSprite()
{

    for (int x = 0; x < 14400; x++)
    {
        spriteBuffer[x] = spriteAlphaColor; // spriteAlphaColor;
    }
    // The sprite buffer is cleared as each frame is rendered so user doesn't need to run this every frame, it's best for setup/boot purposes
    // Once frameDrawing == false you can be assured the sprite buffer has been cleared during render and is ready to be filled with the next frame of data
}

// Converts the 2 bitplane YY-CHR NES graphics to chunky pixels in Pico memory (done on pattern load)
void convertBitplanePattern(uint16_t position, unsigned char *lowBitP, unsigned char *highBitP)
{

    for (int xx = 0; xx < 8; xx++) // Now convert each row of the char...
    {
        unsigned char lowBit = lowBitP[xx]; // Get temps
        unsigned char highBit = highBitP[xx];

        uint16_t tempShort = 0;

        for (int g = 0; g < 8; g++) // 2 bits at a time
        {
            unsigned char bits = ((lowBit & 0x80) >> 1) | (highBit & 0x80); // Build 2 bits from 2 pairs of MSB in bytes (the bitplanes)

            lowBit <<= 1;
            highBit <<= 1;

            bits >>= 6; // Shift into LSB

            tempShort <<= 2; // Free up 2 bits at bottom

            tempShort |= bits;
        }

        patternTable[position++] = tempShort; // Put combined bitplane bytes as a short into buffer (we send this to new file)
    }
}

// Sets display rows to special functions (such as statis status bars or other effects)
void setWinYjump(int jumpFrom, int nextRow)
{

    winYJumpList[jumpFrom] = 0x80 | nextRow;

    // Gamebadge always draws 15 rows (top to bottom) of tile graphics (120 pixels wide x 8 tall)
    // This function allows you to set "freeze" functions on these rows. You can use this to create static (non-moving) status bars like Super Mario or Castlevania
    // The row values set here refer to the rows as displayed on the LCD, not whatever portion of the tile map is being shown per setWindow
    // This allows you to freeze rows, but still perform vertical and horizontal scrolls above or below it

    // Example usage:
    // Setup:
    // CHECK THIS BJH
    // setWinYjump(0, 1);			//Sets row 0 as static, jump to row 1
    // seyWinYjump(1, 2);			//Also sets row 1 as static, jump to row 2
    // setCoarseYRollover(2, 31);	//Tells the tilemap to scroll between rows 2 and 31 (excluding the top 2)

    // Per game frame:
    // setWindow(n, nn);				//Set overall scroll
    // setWindowSlice(0, 0);			//Freeze scroll of row 0
    // setWindowSlice(1, 0);			//Freeze scroll of row 1

    // Data written to the first upper left 2 rows (15 tiles wide) will be static, the rest of the display will scroll

    // To make a status bar at the bottom of the screen it's mostly the same but a little different:

    // Setup:
    // setWinYjump(13, 30);			//Sets row 13 as static, jump to row 30
    // seyWinYjump(14, 31);			//Sets row 14 as static, jump to row 31
    // setCoarseYRollover(0, 29);	//Tells the tilemap to scroll between rows 0 and 29 (excluding the bottom 2)

    // Per game frame:
    // setWindow(n, nn);				//Set overall scroll
    // setWindowSlice(30, 0);			//Freeze scroll of row 30
    // setWindowSlice(31, 0);			//Freeze scroll of row 31

    // In this example you'd draw your status bar at the bottom of the tilemap (rows 30 and 31) and these commands tell the display to
    // jump there on (rendered) rows 13 and 14. All tilemap rows below 30 will scroll normally
}

// Clears the row jumps set by setWinYjump
void clearWinYjump(int whichRow)
{

    winYJumpList[whichRow] = 0;
}

// Sets LCD window in which sprites can be drawn, 0, 119, 0, 119 = full screen (boot default)
void setSpriteWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{

    xLeft = x0 - 1;
    xRight = x1 + 1;
    yTop = y0 - 1;
    yBottom = y1 + 1;
    // Use this to prevent sprites from drawing over static status bars (0, 119, 0, 103 to exclude the bottom 2 rows)
}

// Sets the universal fine scroll window over the name table. Must be from 0-239, 0-239, when exceeding 239 you should "roll over" back to 0
void setWindow(uint8_t x, uint8_t y)
{

    for (int g = 0; g < 32; g++)
    {

        winX[g] = x >> 3;
        winXfine[g] = x & 0x07;
        winY = y >> 3;
        winYfine = y & 0x07;
    }
}

// Allows each row (8 pixels high) of the tile map to be fine horizontally scrolled at different rates (like fake parallax scrolling)
void setWindowSlice(int whichRow, uint8_t x)
{

    winX[whichRow] = x >> 3;
    winXfine[whichRow] = x & 0x07;

    // whichRow is 0-31, x is 0-239 like setWindow
    // Use setWindow to set main window, then use this to control individual rows. For status displays, set their scroll to a fixed 0 so they don't move with rest of screen
}

// Sets which tile map rows are the bounderies for vertical scrolling rollover. 0-31 use whole nametable, 2-31, use top 2 rows as a static display, 0-29, use bottom 2 rows of a static display (see setWinYjump)
void setCoarseYRollover(int topRow, int bottomRow)
{ // Sets at which row the nametable resets back to the top. If using bottom 2 rows as a status display

    winYreset = topRow;
    winYrollover = bottomRow;

    // When you scroll far enough down in the nametable where there isn't enough data to draw, it will "roll over" back to the top (just like the NES)
    // This is why once you've scrolled 239 pixels left or right up or down you should reset your window back to 0
    // But until you reach that limit, gamebadge will display the "rolled over" pixels. In the case of vertical scroll, which this function
    // controls, once row 31 is drawn any additional rows on the display revert back to row 0 (rollover)
    // By default the rollover uses the entire vertical display (32 rows) but if you're using static rows for status bars (in conjunction with setWinYjump) you'll want
    // to use this function to change the rollover to only include valid background data (not status bars)
}

int lcdState = 0;
bool localFrameDrawFlag = false;
volatile bool lcdDMAcomplete = false;
bool isRendering = false; // Render status flag 0 = null true = render in progress false = render complete

int whichBuffer = 0; // We have 2 row buffers. We draw one, send via DMA, and draw next while DMA is running
int lastDMA = 5;     // Round-robin the DMA's

uint8_t fineYsubCount = 0;
uint8_t fineYpointer; // = winYfine;
uint8_t coarseY;      // = winY;
uint16_t *sp;         // = &spriteBuffer[0];
guchar *pp;           // = &playfield[0];

uint8_t renderRow = 0;

uint8_t scrollYflag = 0;

bool LCDupdatePause = false;

void pauseLCD(bool state)
{ // Prevents the LCD from drawing a new frame (hold on old frame) Useful for loading new graphics/game transitions

    LCDupdatePause = state;
}

bool getRenderStatus()
{ // Core0 calls this to see if rendering is done, and when done Core0 runs logic
    return isRendering;
}

void drawLineOfPlayfield(const uint16_t *data, int whatSize)
{
    for (int i = 0; i < whatSize; i++)
    {
        uint16_t rgb565 = *data++;
        guchar r = (rgb565 & 0xF800) >> 8;
        guchar g = (rgb565 & 0x07E0) >> 3;
        guchar b = (rgb565 & 0x001F) << 3;
        *pp++ = r;
        *pp++ = g;
        *pp++ = b;
    }
}

void drawPlayfield()
{
    localFrameDrawFlag = false;
    fineYsubCount = 0; // This is stuff we used to setup in sendframe
    fineYpointer = winYfine;
    coarseY = winY;
    sp = &spriteBuffer[0]; // Use pointer so less math later on
    pp = &playfield[0];
    renderRow = 0;
    scrollYflag = 0;
    isRendering = true;

    while (isRendering)
    {
        RenderRow();
        drawLineOfPlayfield(&linebuffer[whichBuffer][0], 3840);

        if (++whichBuffer > 1)
        { // Switch up buffers, we will draw the next while the prev is being DMA'd to LCD
            whichBuffer = 0;
        }

        if (++renderRow == 15)
        {                    // LCD done?
            isRendering = 0; // Render complete, wait for next draw flag (this keeps Core0 from drawing in screen
        }
    }
}

void RenderRow()
{

    uint16_t temp;
    uint16_t tempPalette;

    uint16_t *pointer = &linebuffer[whichBuffer][0]; // Use pointer so less math later on

    // gpio_put(27, 1);

    if (winYJumpList[renderRow] & 0x80)
    {                                             // Flag to jump to a different row in the nametable? (like for a status bar)
        coarseY = winYJumpList[renderRow] & 0x1F; // Jump to row indicated by bottom 5 bits
        fineYpointer = 0;                         // Reset fine pointer so any following rows are static
    }
    else
    { // Not a jump line?
        if (scrollYflag == 0)
        {                            // Is this first line with fine Y scroll enabled? (under a status bar at the top perhaps)
            scrollYflag = 1;         // Set flag so this only happens once
            fineYpointer = winYfine; // Get Y scroll value
            coarseY = winY;
        }
    }

    for (int yLine = 0; yLine < 16; yLine++)
    { // Each char line is 16 pixels in ST7789 memory, so we send each vertical line twice

        uint8_t fineXPointer = winXfine[coarseY];                           // Copy the fine scrolling amount so we can use it as a byte pointer when scanning in graphics
        uint16_t *tilePointer = &nameTable[coarseY][winX[coarseY]];         // Get pointer for this character line
        uint8_t winXtemp = winX[coarseY];                                   // Temp copy for finding edge of tilemap and rolling back over
        temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer]; // Get first line of pattern
        temp <<= (fineXPointer << 1);                                       // Pre-shift for horizontal scroll

        tempPalette = (*tilePointer & 0x700) >> 6; // Palette is lower 3 bits of upper word. Mask and shift 6 to the right to get the palette index 0bxxxPPPbb P = palette pointer b = bits (the 4 colors per palette)

        if (yLine & 0x01)
        {              // Drawing odd line on LCD/second line of fat pixels?
            sp -= 120; // Revert sprite buffer point to draw every line twice
            for (int xChar = 0; xChar < 120; xChar++)
            { // 15 characters wide

                uint8_t twoBits = temp >> 14; // Smash this down into 2 lowest bits
                if (*sp == spriteAlphaColor)
                {                                                           // Transparent sprite pixel? Draw background color...
                    uint16_t tempShort = paletteRGB[tempPalette | twoBits]; // Add those 2 bits to palette index to get color (0-3)
                    // uint16_t tempShort = paletteRGB[(*tilePointer >> 8) | twoBits];	//Add those 2 bits to palette index to get color (0-3)
                    *pointer++ = tempShort; //...twice (fat pixels)
                    *pointer++ = tempShort;
                }
                else
                {                     // Else draw sprite pixel...
                    *pointer++ = *sp; //..twice (fat pixels)
                    *pointer++ = *sp;
                }
                *sp = spriteAlphaColor; // When rendering the doubled sprite line, erase it so it's empty for next frame
                sp++;
                temp <<= 2; // Shift for next 2 bit color pair

                if (++fineXPointer == 8)
                { // Advance fine scroll count and jump to next char if needed
                    fineXPointer = 0;
                    tilePointer++; // Advance tile pointer in memory
                    if (++winXtemp == 32)
                    {                      // Rollover edge of tiles X?
                        tilePointer -= 32; // Roll tile X pointer back 32
                    }
                    temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer]; // Fetch next line from pattern table
                    tempPalette = (*tilePointer & 0x700) >> 6;                          // Palette is lower 3 bits of upper word. Mask and shift 6 to the right to get the palette index 0bxxxPPPbb P = palette pointer b = bits (the 4 colors per palette)
                }
            }
        }
        else
        {
            for (int xChar = 0; xChar < 120; xChar++)
            { // 15 characters wide

                uint8_t twoBits = temp >> 14; // Smash this down into 2 lowest bits
                if (*sp == spriteAlphaColor)
                {                                                           // Transparent sprite pixel? Draw background color...
                    uint16_t tempShort = paletteRGB[tempPalette | twoBits]; // Add those 2 bits to palette index to get color (0-3)
                    // uint16_t tempShort = paletteRGB[(*tilePointer >> 8) | twoBits];	//Add those 2 bits to palette index to get color (0-3)
                    *pointer++ = tempShort; //...twice (fat pixels)
                    *pointer++ = tempShort;
                }
                else
                {                     // Else draw sprite pixel...
                    *pointer++ = *sp; //..twice (fat pixels)
                    *pointer++ = *sp;
                }
                sp++;
                temp <<= 2; // Shift for next 2 bit color pair

                if (++fineXPointer == 8)
                { // Advance fine scroll count and jump to next char if needed
                    fineXPointer = 0;
                    tilePointer++; // Advance tile pointer in memory
                    if (++winXtemp == 32)
                    {                      // Rollover edge of tiles X?
                        tilePointer -= 32; // Roll tile X pointer back 32
                    }
                    temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer]; // Fetch next line from pattern table
                    tempPalette = (*tilePointer & 0x700) >> 6;                          // Palette is lower 3 bits of upper word. Mask and shift 6 to the right to get the palette index 0bxxxPPPbb P = palette pointer b = bits (the 4 colors per palette)
                }
            }
        }

        if (++fineYsubCount > 1)
        {
            fineYsubCount = 0;
            if (++fineYpointer == 8)
            {                     // Did we pass a character edge with the fine Y scroll?
                fineYpointer = 0; // Reset scroll and advance character
                if (++coarseY > winYrollover)
                {
                    coarseY = winYreset;
                }
            }
        }
    }

    // gpio_put(27, 0);

    // Row drawn, now wait for DMA to open up
}

void LCDsetDrawFlag()
{ // Call this to tell the LCD core to draw next frame

    localFrameDrawFlag = true;
}

// Plays a 11025Hz 8-bit mono WAVE file from file system using the PWM function and DMA on GPIO14 (gamebadge channel 4)
void playAudio(const char *path, int newPriority)
{
}

void stopAudio()
{
}

void serviceAudio()
{ // This is called every game frame to see if PCM audio buffers need re-loading
}

bool fillAudioBuffer(int whichOne)
{
    return true;
}

// File system stuff-----------for loading/saving levels etc--------------------------------
bool checkFile(const char *path)
{
    if ((file = fopen(path, "r")))
    {
        fclose(file);
        return true;
    }
    return false;
}

void saveFile(const char *path)
{ // Opens a file for saving. Deletes the file if it already exists (write-over)

    fileActive = true;
}

bool loadFile(const char *path)
{ // Opens a file for loading

    fileActive = true;
    file = fopen(path, "rb");
    if (!file)
    {
        printf("Unable to open palette file!\n");
        return false;
    }

    return true;
}

void writeByte(uint8_t theByte)
{ // Writes a byte to current file
}

void writeBool(bool state)
{

    if (state == true)
    {
        writeByte(1);
    }
    else
    {
        writeByte(0);
    }
}

uint8_t readByte()
{ // Reads a byte from the file
    uint8_t c;
    fread(&c, sizeof(c), 1, file);
    return c;
}

bool readBool()
{
    if (readByte() == 1)
    {
        return true;
    }

    return false;
}

void closeFile()
{ // Closes the active file
    fclose(file);
    fileActive = false;
}

void eraseFile(const char *path)
{
}

int rnd(int min, int max)
{
    int r = rand() % (max - min);
    return min + r;
}
/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example demo how to expose on-board external Flash as USB Mass Storage.
 * Following library is required
 *   - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
 *   - SdFat https://github.com/adafruit/SdFat
 *
 * Note: Adafruit fork of SdFat enabled ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT
 * in SdFatConfig.h, which is needed to run SdFat on external flash. You can use original
 * SdFat library and manually change those macros
 *
 * Note2: If your flash is not formatted as FAT12 previously, you could format it using
 * follow sketch https://github.com/adafruit/Adafruit_SPIFlash/tree/master/examples/SdFat_format
 */
