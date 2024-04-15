// Game & graphics driver for gameBadgePico (MGC 2023)
#ifndef _CATSKILLGFX_H
#define _CATSKILLGFX_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>

#define ROWS 240
#define COLS 240
#define BYTES_PER_PIXEL 3

extern uint8_t playfield[ROWS * COLS * BYTES_PER_PIXEL];

void initGfx();
void setButton(uint16_t, bool);
void setButtonDebounce(int which, bool useDebounce, uint8_t frames);

bool button(uint8_t which);
void serviceDebounce();
bool loadRGB(const char *path);
void loadPalette(const char *path);
void loadPattern(const char *path, uint16_t start, uint16_t length);

void setWinYjump(int jumpFrom, int nextRow);
void clearWinYjump(int whichRow);

void setSpriteWindow(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1);
void setWindow(uint8_t x, uint8_t y);
void setWindowSlice(int whichRow, uint8_t x);
void setCoarseYRollover(int topRow, int bottomRow);

// NEW 3-27-23
void pauseLCD(bool state);
bool getRenderStatus();
void RenderRow();
void LCDsetDrawFlag();

//bool isDMAbusy(int whatChannel);

void drawLineOfPlayfield(const uint16_t *data, int whatSize);
void drawPlayfield();

void drawTile(int xPos, int yPos, uint16_t whatTile, char whatPalette, int flags);
void drawTileXY(int xPos, int yPos, uint16_t tileX, uint16_t tileY, char whatPalette, int flags);
void setTileType(int tileX, int tileY, int flags);
void tileDirect(int tileX, int tileY, uint16_t theData);
void setTilePalette(int tileX, int tileY, int whatPalette);
int getTileType(int tileX, int tileY);
int getTileValue(int tileX, int tileY);
void fillTiles(int startX, int startY, int endX, int endY, uint16_t whatTile, char whatPalette);
void fillTilesXY(int startX, int startY, int endX, int endY, uint16_t patternX, uint16_t patternY, char whatPalette);
void setASCIIbase(uint16_t whatBase);
void setTextMargins(int left, int right);
void drawText(const char *text, uint8_t x, uint8_t y, bool doWrap);
void drawSpriteText(const char *text, uint8_t x, uint8_t y, int whatPalette);
void drawDecimalXY(int32_t theValue, uint8_t x, uint8_t y);
void drawDecimal(int32_t theValue, uint8_t y);
void drawSpriteDecimal(int32_t theValue, uint8_t x, uint8_t y, int whatPalette);
void drawSpriteDecimalRight(int32_t theValue, uint8_t xRight, uint8_t y, int whatPalette);

void drawSpriteTile(int xPos, int yPos, uint16_t whichTile, uint8_t whichPalette, bool hFlip, bool vFlip);
void drawSpriteRange(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint16_t xWide, uint16_t yHigh, uint8_t whichPalette, bool hFlip, bool vFlip);
void drawSpriteSingle(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint8_t whichPalette, bool hFlip, bool vFlip);

void clearSprite();
void updatePalette(int position, int theIndex);
void updatePaletteRGB(int position, char r, char g, char b);
void convertBitplanePattern(uint16_t position, unsigned char *lowBitP, unsigned char *highBitP);

void playAudio(const char *path, int newPriority);
void stopAudio();
void serviceAudio();
bool fillAudioBuffer(int whichOne);
void initAudio();

bool checkFile(const char *path);
void saveFile(const char *path);
bool loadFile(const char *path);
void writeByte(uint8_t theByte);
void writeBool(bool state);
uint8_t readByte();
bool readBool();
void closeFile();
void eraseFile(const char *path);
int rnd(int min, int max);

// GPIO connections:
//  #define C_but			2
//  #define B_but			3
//  #define A_but			4
//  #define start_but		5
//  #define select_but		21
//  #define up_but 			7
//  #define down_but			11
//  #define left_but			9
//  #define right_but		13

// Buttons to index #: (these are the defines your game will use)
#define up_but 0
#define down_but 1
#define left_but 2
#define right_but 3
#define select_but 4
#define start_but 5
#define A_but 6
#define B_but 7
#define C_but 8
#define ESC_but 9
#define no_but 10

#endif