//ST7735 alt driver
//220205
//https://users.ece.utexas.edu/~valvano/arm/ST7735.c

#if defined(__dsPIC33F__)
#include "p33Fxxxx.h"
#elif defined(__PIC24H__)
#include "p24Hxxxx.h"
#endif

#include "st7735b.h"

void wait10ms(void) {//10 msec
	int i, j;
	for (j=0; j<50; j++) {
		for (i=0; i<4554; i++) {asm("NOP");}
	}
}

#define CLCD_SCK_IO LATBbits.LATB7
#define CLCD_SDA_IO LATBbits.LATB6
#define CLCD_CS_IO LATBbits.LATB5
#define CLCD_RESET_IO LATAbits.LATA4
#define CLCD_A0_IO LATBbits.LATB4
#define CLCD_LED_IO LATBbits.LATB10
#define CLCD_LED_TRIS TRISBbits.TRISB10
void clcd_write2(char cdata, char ca0) {//command: a0=0, data: a0=1
    CLCD_CS_IO = 0;
	if (ca0) CLCD_A0_IO = 1; else CLCD_A0_IO = 0;

	CLCD_SDA_IO = ((cdata & 0x80) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x40) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x20) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x10) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x08) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x04) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x02) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x01) != 0); CLCD_SCK_IO = 0; asm("nop"); asm("nop"); asm("nop"); CLCD_SCK_IO = 1;

//	unsigned int i;
//	for(i=0; i<8; i++) {
//		CLCD_SDA_IO = ((cdata & 0x80) != 0);
//		CLCD_SCK_IO = 0; CLCD_SCK_IO = 1;
//		cdata <<= 1;
//	}
    CLCD_CS_IO = 1;
}

void writecommand(char cdata) {
	clcd_write2(cdata, 0);
}
void writedata(char cdata) {
	clcd_write2(cdata, 1);
}

#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E

#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define INITR_GREENTAB 1
#define INITR_BLACKTAB 2
#define INITR_REDTAB 3

void clcd_init2(void) {
//------------ST7735_InitR------------
// Initialization for ST7735R screens (green or red tabs).
// Input: option one of the enumerated options depending on tabs
// Output: none
//void ST7735_InitR(enum initRFlags option) {
	int i;
	int ioption = INITR_REDTAB;
	//int ioption = INITR_BLACKTAB;
	//int ioption = INITR_GREENTAB;
	//commonInit(Rcmd1);
	CLCD_RESET_IO = 1;
	wait(20);
	CLCD_RESET_IO = 0;	
	wait(20);
	CLCD_RESET_IO = 1;
	for (i=0; i<25; i++) {wait10ms();}//250 msec

    writecommand(ST7735_SLPOUT);
	for (i=0; i<25; i++) {wait10ms();}//250 msec
    writecommand(ST7735_FRMCTR1);
	writedata(0x01); writedata(0x2C); writedata(0x2D);//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    writecommand(ST7735_FRMCTR2);//  4: Frame rate control - idle mode, 3 args:
	writedata(0x01); writedata(0x2C); writedata(0x2D);//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
	writecommand(ST7735_FRMCTR3);//  5: Frame rate ctrl - partial mode, 6 args:
	writedata(0x01); writedata(0x2C); writedata(0x2D);//     Dot inversion mode
	writedata(0x01); writedata(0x2C); writedata(0x2D);//     Line inversion mode
    writecommand(ST7735_INVCTR);//  6: Display inversion ctrl, 1 arg, no delay:
	writedata(0x07);//     No inversion
    writecommand(ST7735_PWCTR1);//  7: Power control, 3 args, no delay:
	writedata(0xA2); 
	writedata(0x02);//     -4.6V
	writedata(0x84);//     AUTO mode
    writecommand(ST7735_PWCTR2);//  8: Power control, 1 arg, no delay:
	writedata(0xC5);//     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    writecommand(ST7735_PWCTR3);//  9: Power control, 2 args, no delay:
	writedata(0x0A);//     Opamp current small
	writedata(0x00);//     Boost frequency
    writecommand(ST7735_PWCTR4);// 10: Power control, 2 args, no delay:
	writedata(0x8A);//     BCLK/2, Opamp current small & Medium low
	writedata(0x2A);
    writecommand(ST7735_PWCTR5);// 11: Power control, 2 args, no delay:
	writedata(0x8A);
	writedata(0xEE);
    writecommand(ST7735_VMCTR1);// 12: Power control, 1 arg, no delay:
	writedata(0x0E);
    writecommand(ST7735_INVOFF);// 13: Don't invert display, no args, no delay
    writecommand(ST7735_MADCTL);// 14: Memory access control (directions), 1 arg:
	writedata(0xC8);//     row addr/col addr, bottom to top refresh
	//writedata(0xC0);//     RGB<>BGR
    writecommand(ST7735_COLMOD);// 15: set color mode, 1 arg, no delay:
	writedata(0x05);//     16-bit color

	if(ioption == INITR_GREENTAB) {
		//commandList(Rcmd2green);
    	writecommand(ST7735_CASET);//  1: Column addr set, 4 args, no delay:
		writedata(0x00); writedata(0x02);//     XSTART = 0
		writedata(0x00); writedata(0x7F+0x02);//     XEND = 127
    	writecommand(ST7735_RASET);//  2: Row addr set, 4 args, no delay:
		writedata(0x00); writedata(0x01);//     XSTART = 0
		writedata(0x00); writedata(0x9F+0x01);//     XEND = 159
		//ColStart = 2;
		//RowStart = 1;
	} else {
		// colstart, rowstart left at default '0' values
		//commandList(Rcmd2red);
    	writecommand(ST7735_CASET);//  1: Column addr set, 4 args, no delay:
		writedata(0x00); writedata(0x00);//     XSTART = 0
		writedata(0x00); writedata(0x7F);//     XEND = 127
    	writecommand(ST7735_RASET);//  2: Row addr set, 4 args, no delay:
		writedata(0x00); writedata(0x00);//     XSTART = 0
		writedata(0x00); writedata(0x9F);//     XEND = 159
	}
	//commandList(Rcmd3);
    writecommand(ST7735_GMCTRP1);//  1: Magical unicorn dust, 16 args, no delay:
		writedata(0x02); writedata(0x1c); writedata(0x07); writedata(0x12);
		writedata(0x37); writedata(0x32); writedata(0x29); writedata(0x2d);
		writedata(0x29); writedata(0x25); writedata(0x2B); writedata(0x39);
		writedata(0x00); writedata(0x01); writedata(0x03); writedata(0x10);
    writecommand(ST7735_GMCTRN1);//  2: Sparkles and rainbows, 16 args, no delay:
		writedata(0x03); writedata(0x1d); writedata(0x07); writedata(0x06);
		writedata(0x2E); writedata(0x2C); writedata(0x29); writedata(0x2D);
		writedata(0x2E); writedata(0x2E); writedata(0x37); writedata(0x3F);
		writedata(0x00); writedata(0x00); writedata(0x02); writedata(0x10);
    writecommand(ST7735_NORON);//  3: Normal display on, no args, w/delay
	wait10ms();
    writecommand(ST7735_DISPON);//  4: Main screen turn on, no args w/delay
	for (i=0; i<25; i++) {wait10ms();}//250 msec

	// if black, change MADCTL color filter
	if (ioption == INITR_BLACKTAB) {
		writecommand(ST7735_MADCTL);
		writedata(0xC0);
	}
	//TabColor = option;
	//ST7735_SetCursor(0,0);
	//StTextColor = ST7735_YELLOW;
	//ST7735_FillScreen(0);                 // set screen to black
}


