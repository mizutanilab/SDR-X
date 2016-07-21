/**********************************************************************
* © 2005 Microchip Technology Inc.
*
* FileName:        main.c
* Dependencies:    Header (.h) files if applicable, see below
* Processor:       dsPIC33Fxxxx/PIC24Hxxxx
* Compiler:        MPLAB® C30 v3.00 or higher
*
* SOFTWARE LICENSE AGREEMENT:
* Microchip Technology Incorporated ("Microchip") retains all ownership and 
* intellectual property rights in the code accompanying this message and in all 
* derivatives hereto.  You may use this code, and any derivatives created by 
* any person or entity by or on your behalf, exclusively with Microchip's
* proprietary products.  Your acceptance and/or use of this code constitutes 
* agreement to the terms and conditions of this notice.
*
* CODE ACCOMPANYING THIS MESSAGE IS SUPPLIED BY MICROCHIP "AS IS".  NO 
* WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED 
* TO, IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A 
* PARTICULAR PURPOSE APPLY TO THIS CODE, ITS INTERACTION WITH MICROCHIP'S 
* PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION. 
*
* YOU ACKNOWLEDGE AND AGREE THAT, IN NO EVENT, SHALL MICROCHIP BE LIABLE, WHETHER 
* IN CONTRACT, WARRANTY, TORT (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), 
* STRICT LIABILITY, INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, 
* PUNITIVE, EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF 
* ANY KIND WHATSOEVER RELATED TO THE CODE, HOWSOEVER CAUSED, EVEN IF MICROCHIP HAS BEEN 
* ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT 
* ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO 
* THIS CODE, SHALL NOT EXCEED THE PRICE YOU PAID DIRECTLY TO MICROCHIP SPECIFICALLY TO 
* HAVE THIS CODE DEVELOPED.
*
* You agree that you are solely responsible for testing the code and 
* determining its suitability.  Microchip has no obligation to modify, test, 
* certify, or support the code.
*
* REVISION HISTORY:
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Author            Date      Comments on this revision
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Settu D 			03/09/06  First release of source file
* MC				09/09/11  Updated auto sample time to 2*Tad
*							  Replaced pin toggle with C30 built in function
*							  Updated Configuration settings
*							  Other misc corrections
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~**
*
* ADDITIONAL NOTES:
* Code Tested on:
* Explorer16 Demo board with dsPIC33FJ256GP710A controller 
* The Processor starts with the Internal oscillator without PLL enabled and then the Clock is switched to PLL Mode.

**********************************************************************/

#if defined(__dsPIC33F__)
#include "p33Fxxxx.h"
#elif defined(__PIC24H__)
#include "p24Hxxxx.h"
#endif

#include "adcDrv2.h"

//Macros for Configuration Fuse Registers:
//Invoke macros to set up  device configuration fuse registers.
//The fuses will select the oscillator source, power-up timers, watch-dog
//timers etc. The macros are defined within the device
//header files. The configuration fuse registers reside in Flash memory.

_FGS(GWRP_OFF & GSS_OFF & GCP_OFF);
_FOSCSEL(FNOSC_FRC & IESO_OFF);
//_FOSC(POSCMD_XT & OSCIOFNC_OFF & FCKSM_CSECMD);
_FOSC(POSCMD_NONE & OSCIOFNC_ON & FCKSM_CSECMD);
_FWDT(FWDTEN_OFF);
_FICD(JTAGEN_OFF & ICS_PGD1);

#pragma config BOR = OFF

#define PI2 6.2831853072
#define NFRAC 32
#define NFRACP 5
#include "math.h"

//audio FFT
#define NDIM 16
#define NDIMP 4
int iw0r[NDIM/2]; int iw0i[NDIM/2];
int ix1r[NDIM]; int ix1i[NDIM];
int ifr[NDIM]; int ifi[NDIM];
int n0, np0;

//FM FFT
#define NFDIM 32
#define NFDIMP 5
int ifw0r[NFDIM/2]; int ifw0i[NFDIM/2];
int ifx1r[NFDIM]; int ifx1i[NFDIM];
int iffr[NFDIM]; int iffi[NFDIM];
int nf0, nfp0;

//disp FFT
#define NDDIM 128
#define NDDIMP 7
int idw0r[NDDIM/2]; int idw0i[NDDIM/2];
int idfr[NDDIM]; int idfi[NDDIM];
int idx1r[NDDIM]; int idx1i[NDDIM];
int nd0, ndp0;
int itraj[NDDIM];//legacy
char cspect[NDDIM];

#define AFOUT 3
#define FRQSTEP 4.17

#define NUMSAMP	NDDIM*2
int BufferA[NUMSAMP] __attribute__((space(dma)));
//int BufferB[NUMSAMP] __attribute__((space(dma)));

void initDma0(void)
{
	DMA0CONbits.SIZE = 0;//Data transfer size = word
	DMA0CONbits.DIR = 0;//Read from peripheral
	DMA0CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
//	DMA0CONbits.MODE  = 2;			// Configure DMA for Continuous Ping-Pong mode
	DMA0CONbits.MODE  = 0;			// Configure DMA for Continuous Ping-Pong mode disabled

	DMA0PAD=(int)&ADC1BUF0;
	DMA0CNT=(NUMSAMP-1);				
	
	DMA0REQ=13;	
	
	DMA0STA = __builtin_dmaoffset(BufferA);		
//	DMA0STB = __builtin_dmaoffset(BufferB);

	IFS0bits.DMA0IF = 0;			//Clear the DMA interrupt flag bit
    IEC0bits.DMA0IE = 0;			//Clear the DMA interrupt enable bit
	
	DMA0CONbits.CHEN=1;


}

void wait(unsigned int imicrosec) {
	unsigned int i;
	unsigned int imax = (int)(22.69*imicrosec);
	for (i=0; i<imax; i++) {asm("NOP");}
}

void wait2ms(void) {//2.00 msec
	int i, j;
	for (j=0; j<10; j++) {
		for (i=0; i<4554; i++) {asm("NOP");}
	}
}

void FFTinit() {
	np0 = NDIMP;
	n0 = NDIM; 
	int i;
	for (i=0; i<(n0 / 2); i++) {
		iw0r[i] = (int)(cos(PI2 * i / n0) * NFRAC);
		iw0i[i] = (int)(sin(PI2 * i / n0) * NFRAC);
	}
}

void FFTFinit() {
	nfp0 = NFDIMP;
	nf0 = NFDIM; 
	int i;
	for (i=0; i<(nf0 / 2); i++) {
		ifw0r[i] = (int)(cos(PI2 * i / nf0) * NFRAC);
		ifw0i[i] = (int)(sin(PI2 * i / nf0) * NFRAC);
	}
}

void FFTDinit() {
	ndp0 = NDDIMP;
	nd0 = NDDIM; 
	int i;
	for (i=0; i<(nd0 / 2); i++) {
		idw0r[i] = (int)(cos(PI2 * i / nd0) * NFRAC);
		idw0i[i] = (int)(sin(PI2 * i / nd0) * NFRAC);
	}
}

void FFT(void) {
	const int n2 = n0 >> 1;
	int al = 1; int bt = n2; int jal, jal2, abinc, idx;
	int scrr, scri, sc2r, sc2i; 

	abinc = bt;
	int j, l, k;
	for (j=0; j<bt; j++) {
		scrr = ifr[j + abinc]; scri = ifi[j + abinc];
		sc2r = ifr[j]; sc2i = ifi[j];
		ix1r[j << 1] = (sc2r + scrr);// >> NFRACP2;
		ix1i[j << 1] = (sc2i + scri);// >> NFRACP2;
		ix1r[(j << 1) + 1] = (sc2r - scrr);// >> NFRACP2;
		ix1i[(j << 1) + 1] = (sc2i - scri);// >> NFRACP2;
	}
	al = al << 1; bt = bt >> 1;
	int jal3;
	for (l=1; l<(np0-1); l++) {//x[j,k]=x[j*alpha + k]
		if (l & 0x01) {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = (jal << 1);
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (ix1r[jal3] * iw0r[k] - ix1i[jal3] * iw0i[k]) >> NFRACP;
					scri = (ix1r[jal3] * iw0i[k] + ix1i[jal3] * iw0r[k]) >> NFRACP;
					ifr[jal2+al] = (ix1r[jal] - scrr);// >> NFRACP2;
					ifi[jal2+al] = (ix1i[jal] - scri);// >> NFRACP2;
					ifr[jal2] = (ix1r[jal] + scrr);// >> NFRACP2;
					ifi[jal2] = (ix1i[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		} else {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = jal << 1;
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (ifr[jal3] * iw0r[k] - ifi[jal3] * iw0i[k]) >> NFRACP;
					scri = (ifr[jal3] * iw0i[k] + ifi[jal3] * iw0r[k]) >> NFRACP;
					ix1r[jal2+al] = (ifr[jal] - scrr);// >> NFRACP2;
					ix1i[jal2+al] = (ifi[jal] - scri);// >> NFRACP2;
					ix1r[jal2] = (ifr[jal] + scrr);// >> NFRACP2;
					ix1i[jal2] = (ifi[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		}
		al = al << 1; bt = bt >> 1;
	}
	//always (al = n0/2, bt = 1) at here
	if (np0 & 0x01) {
		for (k=0; k<n0; k++) {ix1r[k] = ifr[k]; ix1i[k] = ifi[k];}
	}
	//l=np-1
	abinc = al;
		scrr = ix1r[n2]; scri = ix1i[n2];
		ifr[0] = ix1r[0] + scrr; ifi[0] = ix1i[0] + scri;
		ifr[abinc] = ix1r[0] - scrr; ifi[abinc] = ix1i[0] - scri;
	idx = abinc - 1;
	for (k=1; k<al; k++) {
		jal3 = n2+k; //kbt = k * bt;
		scrr = (ix1r[jal3] * iw0r[k] - ix1i[jal3] * iw0i[k]) >> NFRACP;
		scri = (ix1r[jal3] * iw0i[k] + ix1i[jal3] * iw0r[k]) >> NFRACP;
		ifr[idx+abinc] = ix1r[k] + scrr; ifi[idx+abinc] = ix1i[k] + scri;
		ifr[idx] = ix1r[k] - scrr; ifi[idx] = ix1i[k] - scri;
		idx--;
	}
	return;
}

void FFTF(void) {//fft for FM
	const int n2 = nf0 >> 1;
	int al = 1; int bt = n2; int jal, jal2, abinc, idx;
	int scrr, scri, sc2r, sc2i; 

	abinc = bt;
	int j, l, k;
	for (j=0; j<bt; j++) {
		scrr = iffr[j + abinc]; scri = iffi[j + abinc];
		sc2r = iffr[j]; sc2i = iffi[j];
		ifx1r[j << 1] = (sc2r + scrr);// >> NFRACP2;
		ifx1i[j << 1] = (sc2i + scri);// >> NFRACP2;
		ifx1r[(j << 1) + 1] = (sc2r - scrr);// >> NFRACP2;
		ifx1i[(j << 1) + 1] = (sc2i - scri);// >> NFRACP2;
	}
	al = al << 1; bt = bt >> 1;
	int jal3;
	for (l=1; l<(nfp0-1); l++) {//x[j,k]=x[j*alpha + k]
		if (l & 0x01) {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = (jal << 1);
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (ifx1r[jal3] * ifw0r[k] - ifx1i[jal3] * ifw0i[k]) >> NFRACP;
					scri = (ifx1r[jal3] * ifw0i[k] + ifx1i[jal3] * ifw0r[k]) >> NFRACP;
					iffr[jal2+al] = (ifx1r[jal] - scrr);// >> NFRACP2;
					iffi[jal2+al] = (ifx1i[jal] - scri);// >> NFRACP2;
					iffr[jal2] = (ifx1r[jal] + scrr);// >> NFRACP2;
					iffi[jal2] = (ifx1i[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		} else {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = jal << 1;
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (iffr[jal3] * ifw0r[k] - iffi[jal3] * ifw0i[k]) >> NFRACP;
					scri = (iffr[jal3] * ifw0i[k] + iffi[jal3] * ifw0r[k]) >> NFRACP;
					ifx1r[jal2+al] = (iffr[jal] - scrr);// >> NFRACP2;
					ifx1i[jal2+al] = (iffi[jal] - scri);// >> NFRACP2;
					ifx1r[jal2] = (iffr[jal] + scrr);// >> NFRACP2;
					ifx1i[jal2] = (iffi[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		}
		al = al << 1; bt = bt >> 1;
	}
	//always (al = n0/2, bt = 1) at here
	if (nfp0 & 0x01) {
		for (k=0; k<nf0; k++) {ifx1r[k] = iffr[k]; ifx1i[k] = iffi[k];}
	}
	//l=np-1
	abinc = al;
		scrr = ifx1r[n2]; scri = ifx1i[n2];
		iffr[0] = ifx1r[0] + scrr; iffi[0] = ifx1i[0] + scri;
		iffr[abinc] = ifx1r[0] - scrr; iffi[abinc] = ifx1i[0] - scri;
	idx = abinc - 1;
	for (k=1; k<al; k++) {
		jal3 = n2+k; //kbt = k * bt;
		scrr = (ifx1r[jal3] * ifw0r[k] - ifx1i[jal3] * ifw0i[k]) >> NFRACP;
		scri = (ifx1r[jal3] * ifw0i[k] + ifx1i[jal3] * ifw0r[k]) >> NFRACP;
		iffr[idx+abinc] = ifx1r[k] + scrr; iffi[idx+abinc] = ifx1i[k] + scri;
		iffr[idx] = ifx1r[k] - scrr; iffi[idx] = ifx1i[k] - scri;
		idx--;
	}
	return;
}

void FFTD(void) {//fft for disp
	const int n2 = nd0 >> 1;
	int al = 1; int bt = n2; int jal, jal2, abinc, idx;
	int scrr, scri, sc2r, sc2i; 

	abinc = bt;
	int j, l, k;
	for (j=0; j<bt; j++) {
		scrr = idfr[j + abinc]; scri = idfi[j + abinc];
		sc2r = idfr[j]; sc2i = idfi[j];
		idx1r[j << 1] = (sc2r + scrr);// >> NFRACP2;
		idx1i[j << 1] = (sc2i + scri);// >> NFRACP2;
		idx1r[(j << 1) + 1] = (sc2r - scrr);// >> NFRACP2;
		idx1i[(j << 1) + 1] = (sc2i - scri);// >> NFRACP2;
	}
	al = al << 1; bt = bt >> 1;
	int jal3;
	for (l=1; l<(ndp0-1); l++) {//x[j,k]=x[j*alpha + k]
		if (l & 0x01) {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = (jal << 1);
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (idx1r[jal3] * idw0r[k] - idx1i[jal3] * idw0i[k]) >> NFRACP;
					scri = (idx1r[jal3] * idw0i[k] + idx1i[jal3] * idw0r[k]) >> NFRACP;
					idfr[jal2+al] = (idx1r[jal] - scrr);// >> NFRACP2;
					idfi[jal2+al] = (idx1i[jal] - scri);// >> NFRACP2;
					idfr[jal2] = (idx1r[jal] + scrr);// >> NFRACP2;
					idfi[jal2] = (idx1i[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		} else {
			for (j=0; j<bt; j++) {
				jal = j * al; jal2 = jal << 1;
				for (k=0; k<abinc; k+=bt) {
					jal3 = jal + abinc;
					scrr = (idfr[jal3] * idw0r[k] - idfi[jal3] * idw0i[k]) >> NFRACP;
					scri = (idfr[jal3] * idw0i[k] + idfi[jal3] * idw0r[k]) >> NFRACP;
					idx1r[jal2+al] = (idfr[jal] - scrr);// >> NFRACP2;
					idx1i[jal2+al] = (idfi[jal] - scri);// >> NFRACP2;
					idx1r[jal2] = (idfr[jal] + scrr);// >> NFRACP2;
					idx1i[jal2] = (idfi[jal] + scri);// >> NFRACP2;
					jal++; jal2++;
				}
			}
		}
		al = al << 1; bt = bt >> 1;
	}
	//always (al = n0/2, bt = 1) at here
	if (ndp0 & 0x01) {
		for (k=0; k<nd0; k++) {idx1r[k] = idfr[k]; idx1i[k] = idfi[k];}
	}
	//l=np-1
	abinc = al;
		scrr = idx1r[n2]; scri = idx1i[n2];
		idfr[0] = idx1r[0] + scrr; idfi[0] = idx1i[0] + scri;
		idfr[abinc] = idx1r[0] - scrr; idfi[abinc] = idx1i[0] - scri;
	idx = abinc - 1;
	for (k=1; k<al; k++) {
		jal3 = n2+k; //kbt = k * bt;
		scrr = (idx1r[jal3] * idw0r[k] - idx1i[jal3] * idw0i[k]) >> NFRACP;
		scri = (idx1r[jal3] * idw0i[k] + idx1i[jal3] * idw0r[k]) >> NFRACP;
		idfr[idx+abinc] = idx1r[k] + scrr; idfi[idx+abinc] = idx1i[k] + scri;
		idfr[idx] = idx1r[k] - scrr; idfi[idx] = idx1i[k] - scri;
		idx--;
	}
	return;
}

const unsigned char cchar[43][5] = {
	{0x7C,0xA2,0x92,0x8A,0x7C},		// 30	0
	{0x00,0x84,0xFE,0x80,0x00},		// 31	1
	{0xE4,0x92,0x92,0x92,0x8C},		// 32	2
	{0x44,0x82,0x92,0x92,0x6C},		// 33	3
	{0x30,0x28,0x24,0xFE,0x20},		// 34	4
	{0x4E,0x8A,0x8A,0x8A,0x72},		// 35	5
	{0x78,0x94,0x92,0x92,0x60},		// 36	6
	{0x02,0xE2,0x12,0x0A,0x06},		// 37	7
	{0x6C,0x92,0x92,0x92,0x6C},		// 38	8
	{0x0C,0x92,0x92,0x52,0x3C},		// 39	9
	{0x00,0x00,0x00,0x00,0x00},		// 3A	blank(20)
	{0x00,0xB0,0x70,0x00,0x00},		// 3B	,	//instead of ";"
//	{0xB8,0x64,0x5C,0x44,0x3A},		// 3B	phi	//instead of ";"
	{0x10,0x28,0x44,0x82,0x00},		// 3C	<
	{0x28,0x28,0x28,0x28,0x28},		// 3D	=
	{0x82,0x44,0x28,0x10,0x00},		// 3E	>
	{0x40,0x20,0x10,0x08,0x04},		// 3F	/	//instead of "?"
	{0x00,0xC0,0xC0,0x00,0x00},		// 40	.	//instaed of "@"
	{0xF8,0x24,0x22,0x24,0xF8},		// 41	A 
	{0x82,0xFE,0x92,0x92,0x6C},		// 42	B 
	{0x7C,0x82,0x82,0x82,0x44},		// 43	C 
	{0x82,0xFE,0x82,0x82,0x7C},		// 44	D 
	{0xFE,0x92,0x92,0x82,0x82},		// 45	E 
	{0xFE,0x12,0x12,0x02,0x02},		// 46	F 
	{0x7C,0x82,0x92,0x92,0x74},		// 37	G 
	{0xFE,0x10,0x10,0x10,0xFE},		// 48	H 
	{0x00,0x82,0xFE,0x82,0x00},		// 49	I
	{0x40,0x80,0x82,0x7E,0x02},		// 4A	J 
	{0xFE,0x10,0x28,0x44,0x82},		// 4B	K 
	{0xFE,0x80,0x80,0x80,0x80},		// 4C	L 
	{0xFE,0x04,0x18,0x04,0xFE},		// 4D	M 
	{0xFE,0x04,0x08,0x10,0xFE},		// 4E	N 
	{0x7C,0x82,0x82,0x82,0x7C},		// 4F	O 
	{0xFE,0x12,0x12,0x12,0x0C},		// 50	P 
	{0x7C,0x82,0xA2,0x42,0xBC},		// 51	Q
	{0xFE,0x12,0x32,0x52,0x8C},		// 52	R 
	{0x4C,0x92,0x92,0x92,0x64},		// 53	S 
	{0x02,0x02,0xFE,0x02,0x02},		// 54	T 
	{0x7E,0x80,0x80,0x80,0x7E},		// 55	U 
	{0x0E,0x30,0xC0,0x30,0x0E},		// 56	V 
	{0xFE,0x40,0x30,0x40,0xFE},		// 57	W 
	{0xC6,0x28,0x10,0x28,0xC6},		// 58	X 
	{0x06,0x08,0xF0,0x08,0x06},		// 59	Y 
	{0xC2,0xA2,0x92,0x8A,0x86}		// 5A	Z 
};

const unsigned char csmallchar[10][3] = {
	{0x0e,0x11,0x0e},		// 30	0
	{0x02,0x1f,0x00},		// 31	1
	{0x1d,0x15,0x16},		// 32	2
	{0x15,0x15,0x0e},		// 33	3
	{0x07,0x04,0x1f},		// 34	4
	{0x17,0x15,0x0d},		// 35	5
	{0x1e,0x15,0x1d},		// 36	6
	{0x01,0x19,0x07},		// 37	7
	{0x1f,0x15,0x1f},		// 38	8
	{0x17,0x15,0x0f}		// 39	9
};

void lcd_ports(void) {
//	AD1PCFGL=0xFFFF;	// Set as digital
	LATBbits.LATB7 = 0;
	LATBbits.LATB6 = 1;
	LATBbits.LATB5 = 1;
	LATAbits.LATA4 = 1;
	LATBbits.LATB4 = 1;
	LATAbits.LATA3 = 1;
//config RB7 as output: VSS/SCK
	ODCBbits.ODCB7 = 0;//disable open drain	
	TRISBbits.TRISB7 = 0;//set as output
//config RB6 as output: SDO/SDA
	ODCBbits.ODCB6 = 0;	
	TRISBbits.TRISB6 = 0;
//config RB5 as output: RESET/CS
	ODCBbits.ODCB5 = 0;	
	TRISBbits.TRISB5 = 0;
//config RA4 as output: RS/RESET
	TRISAbits.TRISA4 = 0;
//config RB4 as output: SCK/A0
	TRISBbits.TRISB4 = 0;
//config RA3 as output: CS/GND
	TRISAbits.TRISA3 = 0;
//reset
	LATBbits.LATB5 = 0;
	wait2ms();
	LATBbits.LATB5 = 1;	
}

//#define LCD_VSS LATBbits.LATB7
#define LCD_SDO_IO LATBbits.LATB6
//#define LCD_RESET_IO LATBbits.LATB5
#define LCD_RS_IO LATAbits.LATA4
#define LCD_SCK_IO LATBbits.LATB4
#define LCD_CS_IO LATAbits.LATA3
void lcd_write(int idata, int irs) {
    LCD_CS_IO = 0;
	if (irs) LCD_RS_IO = 1; else LCD_RS_IO = 0;//RS

	unsigned int i;
    for(i=0; i<8; i++) {
		asm("nop");
		LCD_SDO_IO = ((idata & 0x80) != 0);
		asm("nop");
		LCD_SCK_IO = 0;
		//asm("nop");
        LCD_SCK_IO = 1;
		asm("nop");
        idata <<= 1;
    }
    LCD_CS_IO = 1;
	return;
}

void lcd_init(void) {
	lcd_write(0xAE, 0);//LCD off
	lcd_write(0xA0, 0);//ADC normal
	lcd_write(0xC8, 0);//Common output 0xC8:reverse 0xC0:normal(upside down)
	lcd_write(0xA3, 0);//LCD bias 1/7
	lcd_write(0x2C, 0);//power ctrl mode=4
	wait2ms();
	lcd_write(0x2E, 0);//power ctrl mode=6
	wait2ms();
	lcd_write(0x2F, 0);//power ctrl mode=7
	lcd_write(0x23, 0);//resister ratio 3
	lcd_write(0x81, 0);//set Vo volume
	lcd_write(0x1C, 0);// volume=0x1c
	lcd_write(0xA4, 0);//A4: not all-on; A5: all dots on
	lcd_write(0x40, 0);//Display start line = 0
	lcd_write(0xA6, 0);//Display polarity = normal
	lcd_write(0xAF, 0);//LCD on
	wait2ms();
}

void lcd_pos(unsigned char xpos, unsigned char ypos){
    lcd_write(0xB0 | ypos, 0);
    lcd_write(0x10 | (xpos >> 4), 0);
    lcd_write(xpos & 0x0F, 0);
}

void lcd_char(char cdata, char cinv){
	int i;
	if (cdata == 0x20) cdata = 0x3a;
	cdata -= 0x30;
	if (cinv) {
		for(i=0; i<5; i++) {lcd_write(~(cchar[cdata][i]), 1);}
		lcd_write(0xff, 1);
	} else {
		for(i=0; i<5; i++) {lcd_write(cchar[cdata][i], 1);}
		lcd_write(0x00, 1);
	}
}

void lcd_string(char* pcstr, char cinv) {
    while (*pcstr) {lcd_char(*pcstr++, cinv);}
}

void lcd_printHex(unsigned int iout, char idigit) {
	char buf;
	signed char i;
	for (i=3; i>=0; i--) {
		buf = (iout >> (i * 4)) & 0x000f;
		if (buf < 10) buf += 0x30; else buf += 0x37;
		if (i < idigit) lcd_char(buf, 0);
	}
}

void lcd_printDec2(unsigned char cout, char cinv) {
	//char 8 bit ==> 2 decimal digits
	char buf0, buf1;
	buf0 = cout / 10;
	buf1 = cout - buf0 * 10;
	lcd_char(buf0 + 0x30, cinv);
	lcd_char(buf1 + 0x30, cinv);
}

void lcd_printDec3(unsigned int iout, char csel, char comit) {
	//int 16 bit ==> 3 decimal digits
	int i;
	int idiv = 100;
	for (i=0; i<3; i++) {
		char buf0 = iout / idiv;
		char cinv  = (csel == i) ? 1 : 0;
		if (comit == 2) {
			if (buf0) {lcd_char(buf0 + 0x30, cinv); comit = 0;}
			else lcd_char(' ', cinv);
		} else if (comit == 1) {
			if (i != 2) {
				if (buf0) {lcd_char(buf0 + 0x30, cinv); comit = 0;}
				else lcd_char(' ', cinv);
			} else {
				lcd_char(buf0 + 0x30, cinv);
			}
		} else {
			lcd_char(buf0 + 0x30, cinv);
		}
		if (i==2) break;
		iout = iout - buf0 * idiv;
		idiv /= 10;
	}
}

void lcd_printDec5(unsigned int iout, int* pidigit, char csel) {
	//int 16 bit ==> 5 decimal digits
	int i;
	int idiv = 10000;
	int iomit = 1;
	*pidigit = 0;
	for (i=0; i<5; i++) {
		char buf0 = iout / idiv;
		char cinv  = (csel == i) ? 1 : 0;
		if (iomit && (i != 4)) {
			if (buf0) {lcd_char(buf0 + 0x30, cinv); iomit = 0; (*pidigit)++;}
			else lcd_char(' ', cinv);
		} else {
			lcd_char(buf0 + 0x30, cinv);
			(*pidigit)++;
		}
		if (i==4) break;
		iout = iout - buf0 * idiv;
		idiv /= 10;
	}
}

void lcd_clear(void){
	unsigned char i,j;
	wait2ms();
	for(j=0; j<8; j++) {
		lcd_pos(0, j);
		for(i=0; i<128; i++) {lcd_write(0, 1); wait(100);}
	}
}

void lcd_spectrum(unsigned char cDiv, unsigned char cTraj) {
	int ix = 0; int iy = 0;//disp origin
	unsigned char cdisp[NDDIM*3];
	int i, j, k;
	for (i=0; i<NDDIM*3; i++) {cdisp[i] = 0;}
	for (i=0; i<NDDIM; i++) {
		j = ((i - (NDDIM>>1)) & (NDDIM-1));
		//f: 0,1,2,...15 -16,-15,-14,...-1
		int iSignal = (abs(idfr[j]) + abs(idfi[j])) >> cDiv;
		if (cTraj) {
			itraj[i] = itraj[i] << 1;
			if (iSignal >= 8) {itraj[i] |= 0x01;}
		}
		for (k=2; k>=0; k--) {
			switch (iSignal) {
				case 0: {cdisp[i + k*NDDIM] = 0x00; break;}
				case 1: {cdisp[i + k*NDDIM] = 0x80; break;}
				case 2: {cdisp[i + k*NDDIM] = 0xC0; break;}
				case 3: {cdisp[i + k*NDDIM] = 0xE0; break;}
				case 4: {cdisp[i + k*NDDIM] = 0xF0; break;}
				case 5: {cdisp[i + k*NDDIM] = 0xF8; break;}
				case 6: {cdisp[i + k*NDDIM] = 0xFC; break;}
				case 7: {cdisp[i + k*NDDIM] = 0xFE; break;}
				default: {cdisp[i + k*NDDIM] = 0xFF; break;}
			}
			iSignal -= 8;
			if (iSignal < 0) break;
		}
	}
	for (k=0; k<=2; k++) {
		lcd_pos(ix, iy + k);
		for (i=0; i<NDDIM; i++) {
			int idata = cdisp[i + k * NDDIM];
			if (k == 0) {//disp arrow head
				switch (i) {
					case (62+AFOUT): {idata |= 0x01; break;}
					case (63+AFOUT): {idata |= 0x03; break;}
					case (64+AFOUT): {idata |= 0x07; break;}
					case (65+AFOUT): {idata |= 0x03; break;}
					case (66+AFOUT): {idata |= 0x01; break;}
				}
			}
			lcd_write(idata, 1);
		}
	}
}

void lcd_traj(void) {
	int ix = 0; int iy = 3;//disp origin
	int i;
	lcd_pos(ix, iy);
	for (i=0; i<NDDIM; i++) {
		unsigned char cbit = (((itraj[i] & 0x3f) << 2) | 0x01);
		lcd_write(cbit, 1);
	}
	lcd_pos(ix, iy+1);
	for (i=0; i<NDDIM; i++) {
		const unsigned char cbit = (itraj[i] >> 6) & 0xff;
		lcd_write(cbit, 1);
	}
}

#define VOLMAX 16
void dispParam(int iMode, int iAtt, char cDemod) {
	char cinv;
	//volume
	lcd_pos(0,5);
	lcd_string("VOL", 0);
	cinv = (iMode == 0) ? 1 : 0;
	lcd_printDec2(VOLMAX+1 - iAtt, cinv);
	//Demodulation
	lcd_pos(32,5);
	cinv = (iMode == 1) ? 1 : 0;
	switch (cDemod) {
		case 0: {lcd_string("AM", cinv); break;}
		case 1: {lcd_string("FM", cinv); break;}
	}
//	lcd_pos(108,5);
//	lcd_string(";", 0);//phi
//	cinv = (iMode == 9) ? 1 : 0;
//	lcd_printDec2(iphase_offset, cinv);
}

const unsigned char ccharKHz[12] = 
	{0xFE, 0x20, 0xD0, 0x00, 0xFE, 0x10, 0xFE, 0x00, 0x90, 0xD0, 0xB0, 0x90};

void dispFreq(double dfreq, char csel) {
	dfreq += AFOUT * FRQSTEP;
	const long lfreq = (long)dfreq;
	const int ifreqMHz = lfreq / 1000;
	const int ifreqKHz = lfreq - (ifreqMHz * 1000);
	lcd_pos(46,5);
	lcd_printDec3(ifreqMHz, csel, 2);
	if (ifreqMHz) {
		lcd_char(0x3B, 0);//comma
		lcd_printDec3(ifreqKHz, csel, 0);
	} else {
		lcd_char(' ', 0);
		lcd_printDec3(ifreqKHz, csel, 1);
	}
	lcd_char(0x40, 0);//decimal point
	char cinv = (csel == 5) ? 1 : 0;
	int ifrac1 = (int)((dfreq - lfreq) * 10);
	lcd_char(ifrac1 + 0x30, cinv);
//	cinv = (csel == 6) ? 1 : 0;
//	int ifrac2 = (int)((dfreq - lfreq - ifrac1*0.1) * 100);
//	lcd_char(ifrac2 + 0x30, cinv);

	//disp "kHz"
	int i;
	for (i=0; i<12; i++) {lcd_write(ccharKHz[i], 1);}
}

void clcd_ports(void) {
//	AD1PCFGL=0xFFFF;	// Set as digital
	LATBbits.LATB7 = 0;//SCK
	LATBbits.LATB6 = 1;
	LATBbits.LATB5 = 1;
	LATAbits.LATA4 = 1;
	LATBbits.LATB4 = 1;
	LATAbits.LATA3 = 0;//RA3 output is disabled
	LATBbits.LATB10 = 1;//LED low
//config RB7 as output: SCK
	ODCBbits.ODCB7 = 0;//disable open drain	
	TRISBbits.TRISB7 = 0;//set as output
//config RB6 as output: SDA
	ODCBbits.ODCB6 = 0;	
	TRISBbits.TRISB6 = 0;
//config RB5 as output: CS
	ODCBbits.ODCB5 = 0;	
	TRISBbits.TRISB5 = 0;
//config RA4 as output: RESET
	TRISAbits.TRISA4 = 0;
//config RB4 as output: A0
	TRISBbits.TRISB4 = 0;
//disable RA3 output: jumper to GND
	TRISAbits.TRISA3 = 1;
//config RB10 as output: LED
	ODCBbits.ODCB10 = 0;	
	TRISBbits.TRISB10 = 0;
}

#define CLCD_SCK_IO LATBbits.LATB7
#define CLCD_SDA_IO LATBbits.LATB6
#define CLCD_CS_IO LATBbits.LATB5
#define CLCD_RESET_IO LATAbits.LATA4
#define CLCD_A0_IO LATBbits.LATB4
#define CLCD_LED_IO LATBbits.LATB10
#define CLCD_LED_TRIS TRISBbits.TRISB10
void clcd_write(char cdata, char ca0) {//command: a0=0, data: a0=1
    CLCD_CS_IO = 0;
	if (ca0) CLCD_A0_IO = 1; else CLCD_A0_IO = 0;

	CLCD_SDA_IO = ((cdata & 0x80) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x40) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x20) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x10) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x08) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x04) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x02) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
	CLCD_SDA_IO = ((cdata & 0x01) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;

//	unsigned int i;
//	for(i=0; i<8; i++) {
//		CLCD_SDA_IO = ((cdata & 0x80) != 0);
//		CLCD_SCK_IO = 0; CLCD_SCK_IO = 1;
//		cdata <<= 1;
//	}
    CLCD_CS_IO = 1;
}

void clcd_init(void) {
	int i;
	CLCD_CS_IO = 0;
//reset
	CLCD_RESET_IO = 1;
	wait(20);
	CLCD_RESET_IO = 0;	
	wait(20);
	CLCD_RESET_IO = 1;
	for (i=0; i<60; i++) {wait2ms();}//120 msec

	clcd_write(0x11, 0);//SLPOUT
	for (i=0; i<60; i++) {wait2ms();}//120 msec
	clcd_write(0x3a, 0);//COLMOD
		clcd_write(0x05, 1);//16-bit color
	clcd_write(0xb1, 0);//FRMCTR1
		clcd_write(0x00, 1); clcd_write(0x06, 1); clcd_write(0x03, 1);
	clcd_write(0x36, 0);//MADCTL
		clcd_write(0x60, 1);
	clcd_write(0xb6, 0);//DISSET5
		clcd_write(0x15, 1); clcd_write(0x02, 1);
	clcd_write(0xb4, 0);//INVCTR
		clcd_write(0x00, 1);
	clcd_write(0xc0, 0);//PWCTR1
		clcd_write(0x02, 1); clcd_write(0x70, 1);
	wait2ms();
	clcd_write(0xc1, 0);//PWCTR2
		clcd_write(0x05, 1);
	clcd_write(0xc2, 0);//PWCTR3
		clcd_write(0x01, 1); clcd_write(0x02, 1);
	clcd_write(0xc5, 0);//VMCTR1
		clcd_write(0x3C, 1); clcd_write(0x38, 1);
 	wait2ms();
	clcd_write(0xfc, 0);//PWCTR6
		clcd_write(0x11, 1); clcd_write(0x15, 1);
	clcd_write(0xe0, 0);//GMCTRP1
		clcd_write(0x09, 1); clcd_write(0x16, 1); clcd_write(0x09, 1); clcd_write(0x20, 1);
		clcd_write(0x21, 1); clcd_write(0x1b, 1); clcd_write(0x13, 1); clcd_write(0x19, 1);
		clcd_write(0x17, 1); clcd_write(0x15, 1); clcd_write(0x1e, 1); clcd_write(0x2b, 1);
		clcd_write(0x04, 1); clcd_write(0x05, 1); clcd_write(0x02, 1); clcd_write(0x0e, 1);
	clcd_write(0xe1, 0);//GMCTRN1
		clcd_write(0x0b, 1); clcd_write(0x14, 1); clcd_write(0x08, 1); clcd_write(0x1e, 1);
		clcd_write(0x22, 1); clcd_write(0x1d, 1); clcd_write(0x18, 1); clcd_write(0x1e, 1);
		clcd_write(0x1b, 1); clcd_write(0x1a, 1); clcd_write(0x24, 1); clcd_write(0x2b, 1);
		clcd_write(0x06, 1); clcd_write(0x06, 1); clcd_write(0x02, 1); clcd_write(0x0f, 1);
	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(0x00, 1);
		clcd_write(0x00, 1); clcd_write(0x7f, 1);
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(0x00, 1);
		clcd_write(0x00, 1); clcd_write(0x7f, 1);
	clcd_write(0x13, 0);//NORON
	clcd_write(0x29, 0);//DISPON
}

#define CLCD_WIDTH 128
#define CLCD_HEIGHT 128
void clcd_clear(int icolor){
	char cColLow = icolor & 0x00ff;
	char cColHigh = (icolor >> 8) & 0xff;

	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(0x00, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(CLCD_WIDTH-1, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(0x00, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(CLCD_HEIGHT-1, 1);// YEND
	clcd_write(0x2c, 0);//RAMWR

	int i, j;
	for(i=0; i<CLCD_HEIGHT; i++){
		for(j=0; j<CLCD_WIDTH; j++){
			clcd_write(cColLow, 1); clcd_write(cColHigh, 1);
		}
	}
}

void clcd_led_off(void) {CLCD_LED_TRIS = 1;}

void clcd_led_high(void) {CLCD_LED_IO = 0; CLCD_LED_TRIS = 0;}

void clcd_led_low(void) {CLCD_LED_IO = 1; CLCD_LED_TRIS = 0;}

char cxcrd = 0;
char cycrd = 0;
char cRotEnc = 0;
#define FONTCOLORH 0xff
#define FONTCOLORL 0xff
#define BACKCOLORH 0x00
#define BACKCOLORL 0x00
#define SELCOLORH 0x00
#define SELCOLORL 0x1f
#define MOVCOLORH 0x66
#define MOVCOLORL 0x00
void clcd_char(char cdata, char cback){
	char cBackColorL = BACKCOLORL;
	char cBackColorH = BACKCOLORH;
	if (cback) {
		if (cRotEnc) {
			cBackColorL = SELCOLORL;
			cBackColorH = SELCOLORH;
		} else {
			cBackColorL = MOVCOLORL;
			cBackColorH = MOVCOLORH;
		}
	}
	if (cdata == 0x20) cdata = 0x3a;
	cdata -= 0x30;

	T2CONbits.TON = 0;//stop timer2 (interrupt)

	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(cxcrd, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(cxcrd + 5, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(cycrd, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(cycrd + 7, 1);// YEND
	cxcrd += 6;
	clcd_write(0x2c, 0);//RAMWR

	char cmask = 0x01;
	int i, j;
	for(i=0; i<8; i++){
		for(j=0; j<5; j++){
			if (cmask & (cchar[cdata][j])) {
				clcd_write(FONTCOLORH, 1); clcd_write(FONTCOLORL, 1);
			} else {
				clcd_write(cBackColorH, 1); clcd_write(cBackColorL, 1);
			}
		}
		clcd_write(cBackColorH, 1); clcd_write(cBackColorL, 1);
		cmask <<= 1;
	}
	T2CONbits.TON = 1;//start timer2 (interrupt)
}

void clcd_smallchar(char cdata, int icolor){
	char cBackColorL = BACKCOLORL;
	char cBackColorH = BACKCOLORH;
	cdata -= 0x30;

	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(cxcrd, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(cxcrd + 3, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(cycrd, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(cycrd + 4, 1);// YEND
	cxcrd += 4;
	clcd_write(0x2c, 0);//RAMWR

	char cmask = 0x01;
	int i, j;
	for(i=0; i<5; i++){
		for(j=0; j<3; j++){
			if (cmask & (csmallchar[cdata][j])) {
				clcd_write(icolor >> 8, 1); clcd_write(icolor, 1);
			} else {
				clcd_write(cBackColorH, 1); clcd_write(cBackColorL, 1);
			}
		}
		clcd_write(cBackColorH, 1); clcd_write(cBackColorL, 1);
		cmask <<= 1;
	}
}

void clcd_xline(char cy, char cxstart, char cxend, int icolor) {
	char cColLow = icolor & 0x00ff;
	char cColHigh = (icolor >> 8) & 0xff;
	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(cxstart, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(cxend, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(cy, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(cy, 1);// YEND
	clcd_write(0x2c, 0);//RAMWR

	int j;
	for(j=cxstart; j<=cxend; j++){
		clcd_write(cColHigh, 1); clcd_write(cColLow, 1);
	}
}

void clcd_yline(char cx, char cystart, char cyend, int icolor) {
	char cColLow = icolor & 0x00ff;
	char cColHigh = (icolor >> 8) & 0xff;
	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(cx, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(cx, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(cystart, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(cyend, 1);// YEND
	clcd_write(0x2c, 0);//RAMWR

	int j;
	for(j=cystart; j<=cyend; j++){
		clcd_write(cColHigh, 1); clcd_write(cColLow, 1);
	}
}

const unsigned int iColorGrad[] = {
	0xF800, 0xF820, 0xF860, 0xF8A0, 0xF8E0, 0xF920, 0xF960, 0xF9A0,
	0xF9E0, 0xFA20, 0xFA60, 0xFAA0, 0xFAE0, 0xFB20, 0xFB60, 0xFBA0,
	0xFBE0, 0xFC20, 0xFC60, 0xFCA0, 0xFCE0, 0xFD20, 0xFD60, 0xFDA0,
	0xFDE0, 0xFE20, 0xFE60, 0xFEA0, 0xFEE0, 0xFF20, 0xFF60, 0xFFA0,
	0xFFE0, 0xF7E0, 0xEFE0, 0xE7E0, 0xDFE0, 0xD7E0, 0xCFE0, 0xC7E0,
	0xBFE0, 0xB7E0, 0xAFE0, 0xA7E0, 0x9FE0, 0x97E0, 0x8FE0, 0x87E0,
	0x7FE0, 0x77E0, 0x6FE0, 0x67E0, 0x5FE0, 0x57E0, 0x4FE0, 0x47E0,
	0x3FE0, 0x37E0, 0x2FE0, 0x27E0, 0x1FE0, 0x17E0, 0x0FE0, 0x07E0,
	0x07E1, 0x07E2, 0x07E3, 0x07E4, 0x07E5, 0x07E6, 0x07E7, 0x07E8,
	0x07E9, 0x07EA, 0x07EB, 0x07EC, 0x07ED, 0x07EE, 0x07EF, 0x07F0,
	0x07F1, 0x07F2, 0x07F3, 0x07F4, 0x07F5, 0x07F6, 0x07F7, 0x07F8,
	0x07F9, 0x07FA, 0x07FB, 0x07FC, 0x07FD, 0x07FE, 0x07FF, 0x07DF,
	0x079F, 0x075F, 0x071F, 0x06DF, 0x069F, 0x065F, 0x061F, 0x05DF,
	0x059F, 0x055F, 0x051F, 0x04DF, 0x049F, 0x045F, 0x041F, 0x03DF,
	0x039F, 0x035F, 0x031F, 0x02DF, 0x029F, 0x025F, 0x021F, 0x01DF,
	0x019F, 0x015F, 0x011F, 0x00DF, 0x009F, 0x005F, 0x001F, 0x001F
};
void clcd_gradyline(char cx, char cystart, char cyend) {
	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(cx, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(cx, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(cystart, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(cyend, 1);// YEND
	clcd_write(0x2c, 0);//RAMWR

	int j;
	for(j=cystart; j<=cyend; j++){
		int icolor = iColorGrad[127 - (j << 1)];
		char cColLow = icolor & 0x00ff;
		char cColHigh = (icolor >> 8) & 0xff;
		clcd_write(cColHigh, 1); clcd_write(cColLow, 1);
	}
}

#define TRAJMAX 48
__attribute__((far)) char ctraj[NDDIM*TRAJMAX];
char ctrajblk = 0;
void clcd_spectrum(unsigned char cDiv, unsigned char cTraj) {
	int i, j, k;
	for (i=0; i<NDDIM; i++) {
		j = ((i - (NDDIM>>1)) & (NDDIM-1));
		//f: 0,1,2,...15 -16,-15,-14,...-1
		int iSignal = (abs(idfr[j]) + abs(idfi[j])) >> cDiv;
		if (iSignal > 63) iSignal = 63;
		if (cspect[i] < iSignal) {
			clcd_gradyline(i, 63-iSignal, 63-cspect[i]);
		} else if (cspect[i] > iSignal) {
			clcd_yline(i, 63-cspect[i], 63-iSignal, 0x0000);
		}
		cspect[i] = (char)iSignal;
		if (cTraj) {
			ctraj[ctrajblk * NDDIM + i] = iSignal;
		}
	}
	if (cTraj) {
		ctrajblk++;
		if (ctrajblk >= TRAJMAX) ctrajblk = 0;
	}
}

void clcd_traj(void) {
	clcd_write(0x2a, 0);//CASET
		clcd_write(0x00, 1); clcd_write(0, 1);// XSTART
		clcd_write(0x00, 1); clcd_write(NDDIM-1, 1);// XEND
	clcd_write(0x2b, 0);//RASET
		clcd_write(0x00, 1); clcd_write(72, 1);// YSTART
		clcd_write(0x00, 1); clcd_write(72+TRAJMAX-1, 1);// YEND
	clcd_write(0x2c, 0);//RAMWR

	int i, j;
	int jy = (ctrajblk ? (ctrajblk-1) : (TRAJMAX-1)) * NDDIM;
	CLCD_CS_IO = 0;
	CLCD_A0_IO = 1;
	for (j=0; j<TRAJMAX; j++) {
		for (i=0; i<NDDIM; i++) {
			int icolor = iColorGrad[(ctraj[i + jy] << 1)];
			char cColLow = icolor;// & 0x00ff;
			char cColHigh = (icolor >> 8);// & 0xff;
			//clcd_write(cColHigh, 1);
			CLCD_SDA_IO = ((cColHigh & 0x80) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x40) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x20) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x10) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x08) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x04) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x02) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColHigh & 0x01) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			//clcd_write(cColLow, 1);
			CLCD_SDA_IO = ((cColLow & 0x80) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x40) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x20) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x10) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x08) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x04) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x02) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
			CLCD_SDA_IO = ((cColLow & 0x01) != 0); CLCD_SCK_IO = 0; asm("nop"); CLCD_SCK_IO = 1;
		}
		jy -= NDDIM;
		if (jy < 0) jy = (TRAJMAX-1) * NDDIM;
	}
    CLCD_CS_IO = 1;
}

void clcd_pos(char cx, char cy) {
	cxcrd = cx; cycrd = cy * 8;
}

void clcd_string(char* pcstr, char cback) {
    while (*pcstr) {clcd_char(*pcstr++, cback);}
}

void clcd_smallstring(char* pcstr, int icolor) {
    while (*pcstr) {clcd_smallchar(*pcstr++, icolor);}
}

void clcd_printDec2(unsigned char cout, char cback) {
	//char 8 bit ==> 2 decimal digits
	char buf0, buf1;
	buf0 = cout / 10;
	buf1 = cout - buf0 * 10;
	clcd_char(buf0 + 0x30, cback);
	clcd_char(buf1 + 0x30, cback);
}

void clcd_printDec3(unsigned int iout, char csel, char comit) {
	//int 16 bit ==> 3 decimal digits
	int i;
	int idiv = 100;
	for (i=0; i<3; i++) {
		char buf0 = iout / idiv;
		char cinv  = (csel == i) ? 1 : 0;
		if (comit == 2) {
			if (buf0) {clcd_char(buf0 + 0x30, cinv); comit = 0;}
			else clcd_char(' ', cinv);
		} else if (comit == 1) {
			if (i != 2) {
				if (buf0) {clcd_char(buf0 + 0x30, cinv); comit = 0;}
				else clcd_char(' ', cinv);
			} else {
				clcd_char(buf0 + 0x30, cinv);
			}
		} else {
			clcd_char(buf0 + 0x30, cinv);
		}
		if (i==2) break;
		iout = iout - buf0 * idiv;
		idiv /= 10;
	}
}

void clcd_printHex(unsigned int iout, char idigit) {
	char buf;
	signed char i;
	for (i=3; i>=0; i--) {
		buf = (iout >> (i * 4)) & 0x000f;
		if (buf < 10) buf += 0x30; else buf += 0x37;
		if (i < idigit) clcd_char(buf, 0);
	}
}

void cdispParam(int iMode, int iAtt, char cDemod) {
	char cinv;
	//volume
	clcd_pos(0,15);
	clcd_string("VOL", 0);
	cinv = (iMode == 0) ? 1 : 0;
	clcd_printDec2(VOLMAX+1 - iAtt, cinv);
	//Demodulation
	clcd_pos(32,15);
	cinv = (iMode == 1) ? 1 : 0;
	switch (cDemod) {
		case 0: {clcd_string("AM", cinv); break;}
		case 1: {clcd_string("FM", cinv); break;}
	}
}

void cdispFreq(double dfreq, char csel) {
	dfreq += AFOUT * FRQSTEP;
	const long lfreq = (long)dfreq;
	const int ifreqMHz = lfreq / 1000;
	const int ifreqKHz = lfreq - (ifreqMHz * 1000);
	clcd_pos(48,15);
	clcd_printDec3(ifreqMHz, ((csel<0) ? -1 : csel+1), 2);
	if (ifreqMHz) {
		clcd_char(0x3B, 0);//comma
		clcd_printDec3(ifreqKHz, csel-2, 0);
	} else {
		clcd_char(' ', 0);
		clcd_printDec3(ifreqKHz, csel-2, 1);
	}
	clcd_char(0x40, 0);//decimal point
	char cinv = (csel == 5) ? 1 : 0;
	int ifrac1 = (int)((dfreq - lfreq) * 10);
	clcd_char(ifrac1 + 0x30, cinv);
//	cinv = (csel == 6) ? 1 : 0;
//	int ifrac2 = (int)((dfreq - lfreq - ifrac1*0.1) * 100);
//	lcd_char(ifrac2 + 0x30, cinv);

	cxcrd += 2;
	clcd_string("KHZ", 0);

	//disp "kHz"
//	int i;
//	for (i=0; i<12; i++) {clcd_write(ccharKHz[i], 1);}
}

#define I2C_SCL_OUT LATBbits.LATB8
#define I2C_SCL_TRIS TRISBbits.TRISB8
#define I2C_SCL_IN PORTBbits.RB8
#define I2C_SDA_OUT LATBbits.LATB9
#define I2C_SDA_IN PORTBbits.RB9
#define I2C_SDA_TRIS TRISBbits.TRISB9
#define I2C_WAIT 10
void i2c_init(void) {
	CNPU2bits.CN21PUE = 1;//SDA weak pull-up (ca.15kOhm)
//	CNPU2bits.CN22PUE = 1;//SCL weak pull-up (ca.15kOhm)
	I2C_SCL_OUT = 1;
	I2C_SDA_OUT = 1;
//config RB8 as output: SCL
	ODCBbits.ODCB8 = 0;//disable open drain	
	I2C_SCL_TRIS = 0;//set as output
//config RB9 as output: SDA
	ODCBbits.ODCB9 = 0;//disable open drain	
	I2C_SDA_TRIS = 0;//set as output
}

void i2c_write(unsigned char cdata) {
	char i;
	for(i=0; i<8; i++){//master==>slave data
		I2C_SDA_OUT = ((cdata & 0x80) != 0);
		asm("NOP");
		I2C_SCL_OUT = 1;
		wait(I2C_WAIT);
		I2C_SCL_OUT = 0;
		I2C_SDA_OUT = 0;
//		I2C_SCL_TRIS = 1;//clock stretch
//		while (I2C_SCL_IN == 0) {wait(I2C_WAIT);}
//		I2C_SCL_TRIS = 0;
		wait(I2C_WAIT);
		cdata <<= 1;
	}
	I2C_SDA_TRIS = 1;//slave==>master ack
	while (I2C_SDA_IN) {}
	I2C_SCL_OUT = 1;
	wait(I2C_WAIT);
	I2C_SCL_OUT = 0;
	I2C_SDA_TRIS = 0;
}

void si5351_byte(unsigned char reg, unsigned char data){
	I2C_SDA_TRIS = 0;
	I2C_SCL_OUT = 1;//master start
	I2C_SDA_OUT = 1;
	wait(I2C_WAIT);
	I2C_SDA_OUT = 0;
	wait(I2C_WAIT);
	I2C_SCL_OUT = 0;

	i2c_write(0xC0);//address, write
	i2c_write(reg);
	i2c_write(data);

	I2C_SDA_OUT = 0;//master stop
	wait(I2C_WAIT);
	I2C_SCL_OUT = 1;
	wait(I2C_WAIT);
	I2C_SDA_OUT = 1;
	wait(I2C_WAIT);
}

void si5351_burst(unsigned char reg, unsigned char* pcdata, unsigned char ndata){
	I2C_SDA_TRIS = 0;
	I2C_SCL_OUT = 1;//master start
	I2C_SDA_OUT = 1;
	wait(I2C_WAIT);
	I2C_SDA_OUT = 0;
	wait(I2C_WAIT);
	I2C_SCL_OUT = 0;

	i2c_write(0xC0);//address, write
	i2c_write(reg);
	int i;
	for (i=0; i<ndata; i++) {i2c_write(pcdata[i]);}

	I2C_SDA_OUT = 0;//master stop
	wait(I2C_WAIT);
	I2C_SCL_OUT = 1;
	wait(I2C_WAIT);
	I2C_SDA_OUT = 1;
	wait(I2C_WAIT);
}

#define SI5351XTAL 25.0
void si5351_PLLs(double dfvco){//fxtal=25 MHz
	double dratio = dfvco / SI5351XTAL;
	unsigned int iratio = (int)(dratio);
	double dfrac = (dratio - iratio) * 128.0;
	unsigned int ifrac = (int)(dfrac);
	unsigned long lp1 = (iratio << 7) - 512 + ifrac;
	unsigned long lp2 = (long)((dfrac - ifrac) * 524288.0);
//	unsigned long lp3 = 524288;//2^19
	unsigned char pcdata[8];
	pcdata[0] = 0;// reg26 <MSNA_P3[15:8]>
	pcdata[1] = 0;// reg27 <MSNA_P3[7:0]>
	pcdata[2] = ((lp1 >> 16) & 0x03);// reg28 <MSNA_P1[17:16]>
	pcdata[3] = (lp1 >> 8) & 0xff;// reg29 <MSNA_P1[15:8]> fvco=900 MHz, 36x
	pcdata[4] = lp1 & 0xff;// reg30 <MSNA_P1[7:0]>
	pcdata[5] = ((lp2 >> 16) & 0x0f) | 0x80;// reg31 <MSNA_P3[19:16], MSNA_P2[19:16]>
	pcdata[6] = (lp2 >> 8) & 0xff;// reg32 <MSNA_P2[15:8]>
	pcdata[7] = lp2 & 0xff;// reg33 <MSNA_P2[7:0]>
//	pcdata[0] = 0;	// reg26 <MSNA_P3[15:8]>
//	pcdata[1] = 1;	// reg27 <MSNA_P3[7:0]>
//	pcdata[2] = 0;	// reg28 <MSNA_P1[17:16]>
//	pcdata[3] = 0x10; 	// reg29 <MSNA_P1[15:8]> fvco=900 MHz, 36x
//	pcdata[4] = 0;	// reg30 <MSNA_P1[7:0]>
//	pcdata[5] = 0;	// reg31 <MSNA_P3[19:16], MSNA_P2[19:16]>
//	pcdata[6] = 0;	// reg32 <MSNA_P2[15:8]>
//	pcdata[7] = 0;	// reg33 <MSNA_P2[7:0]>
	si5351_burst(26, pcdata, 8);
	si5351_burst(34, pcdata, 8);
}

#define SI5351PHASE 0.00007
void si5351_freq(double dfreq){
	double dfvco = 900.0;
	char cdiv = 0x00;
	int idiv = 1;
	char cphase = 0x00;
	double doffset = 0.0;
	si5351_byte(16, 0x0d);//CLK0_PON, Fractional, PLLA, not invert, MSynth0, 4 mA
	si5351_byte(18, 0x0d);//CLK2_PON, Fractional, PLLA, not invert, MSynth2, 4 mA
	unsigned char pcdata[8];

	if (dfreq < 0.2) {idiv = 128; cdiv = 0x70; doffset = SI5351PHASE * idiv;}
	else if (dfreq < 1.8) {idiv = 16; cdiv = 0x40; doffset = SI5351PHASE * idiv;}
	else if (dfreq < 7.1) {doffset = SI5351PHASE;}
	else if (dfreq < 110.0) {cphase = (char)(dfvco / dfreq);}
	else if (dfreq < 150.0) {
		dfvco = dfreq * 6.0;
		cphase = 6;
		si5351_byte(16, 0x4d);//CLK0_PON, Integer, PLLA, not invert, MSynth0, 4 mA
		si5351_byte(18, 0x4d);//CLK2_PON, Integer, PLLA, not invert, MSynth2, 4 mA
	} else {
		dfvco = dfreq * 4.0;
		cphase = 4;
		si5351_byte(16, 0x4d);//CLK0_PON, Integer, PLLA, not invert, MSynth0, 4 mA
		si5351_byte(18, 0x4d);//CLK2_PON, Integer, PLLA, not invert, MSynth2, 4 mA
		pcdata[0] = 0;//reg42 <MS0_P3[15:8]>
		pcdata[1] = 1;//reg43 <MS0_P3[7:0]>
		pcdata[2] = 0x0c;//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
		pcdata[3] = 0;//reg45 <MS0_P1[15:8]>
		pcdata[4] = 0;//reg46 <MS0_P1[7:0]>
		pcdata[5] = 0;//reg47 <MS0_P3[19:16], MS0_P2[19:16]>
		pcdata[6] = 0;//reg48 <MS0_P2[15:8]>
		pcdata[7] = 0;//reg49 <MS0_P2[7:0]>
		si5351_burst(42, pcdata, 8);//MSynth0
		si5351_burst(58, pcdata, 8);//MSynth2
		si5351_byte(167, cphase);//MSynth2 phase
		si5351_byte(177,0xA0);//PLL soft reset is needed to align phases
		return;
	}
//PLL
	si5351_PLLs(dfvco);
//set freq
	double dratio = dfvco / (dfreq * idiv - doffset);
	unsigned int iratio = (int)(dratio);
	double dfrac = (dratio - iratio) * 128.0;
	unsigned int ifrac = (int)(dfrac);
	unsigned long lp1 = (iratio << 7) - 512 + ifrac;
	unsigned long lp2 = (long)((dfrac - ifrac) * 524288.0);
//	unsigned long lp3 = 524288;//2^19
	pcdata[0] = 0;//reg42 <MS0_P3[15:8]>
	pcdata[1] = 0;//reg43 <MS0_P3[7:0]>
	pcdata[2] = ((lp1 >> 16) & 0x03) | cdiv;//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
//	pcdata[2] = ((lp1 >> 16) & 0x03) | 0x40;//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
//	pcdata[2] = ((lp1 >> 16) & 0x03) | 0x20;//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
//	pcdata[2] = ((lp1 >> 16) & 0x03);//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
	pcdata[3] = (lp1 >> 8) & 0xff;//reg45 <MS0_P1[15:8]>
	pcdata[4] = lp1 & 0xff;//reg46 <MS0_P1[7:0]>
	pcdata[5] = ((lp2 >> 16) & 0x0f) | 0x80;//reg47 <MS0_P3[19:16], MS0_P2[19:16]>
	pcdata[6] = (lp2 >> 8) & 0xff;//reg48 <MS0_P2[15:8]>
	pcdata[7] = lp2 & 0xff;//reg49 <MS0_P2[7:0]>
	si5351_burst(42, pcdata, 8);//MSynth0
	si5351_burst(58, pcdata, 8);//MSynth2
	si5351_byte(167, cphase);//MSynth2 phase
	si5351_byte(177,0xA0);//PLL soft reset is needed to align phases
/*
	T2CONbits.TON = 0;
	clcd_pos(0,14);
	clcd_printHex(pcdata[0], 2); cxcrd += 2;
	clcd_printHex(pcdata[1], 2); cxcrd += 2;
	clcd_printHex(pcdata[2], 2); cxcrd += 2;
	clcd_printHex(pcdata[3], 2); cxcrd += 2;
	clcd_printHex(pcdata[4], 2); cxcrd += 2;
	clcd_printHex(pcdata[5], 2); cxcrd += 2;
	clcd_printHex(pcdata[6], 2); cxcrd += 2;
	clcd_printHex(pcdata[7], 2);
	T2CONbits.TON = 1;
///*///
	if (doffset == 0.0) return;
//set phase by changing freq
	dratio = dfvco / (dfreq * idiv);
	iratio = (int)(dratio);
	dfrac = (dratio - iratio) * 128.0;
	ifrac = (int)(dfrac);
	lp1 = (iratio << 7) - 512 + ifrac;
	lp2 = (long)((dfrac - ifrac) * 524288.0);
	pcdata[0] = 0;//reg42 <MS0_P3[15:8]>
	pcdata[1] = 0;//reg43 <MS0_P3[7:0]>
	pcdata[2] = ((lp1 >> 16) & 0x03) | cdiv;//reg44 <0, R0_DIV[2:0], MS0_DIVBY4[1:0], MS0\P1[17:16]>
	pcdata[3] = (lp1 >> 8) & 0xff;//reg45 <MS0_P1[15:8]>
	pcdata[4] = lp1 & 0xff;//reg46 <MS0_P1[7:0]>
	pcdata[5] = ((lp2 >> 16) & 0x0f) | 0x80;//reg47 <MS0_P3[19:16], MS0_P2[19:16]>
	pcdata[6] = (lp2 >> 8) & 0xff;//reg48 <MS0_P2[15:8]>
	pcdata[7] = lp2 & 0xff;//reg49 <MS0_P2[7:0]>
	si5351_burst(42, pcdata, 8);//MSynth0
	si5351_burst(58, pcdata, 8);//MSynth2
	si5351_byte(167, 0x00);//MSynth2 phase
}

void si5351_init() {
	i2c_init();
	si5351_byte(3, 0xff);//disable outputs
	si5351_byte(183,0b10010010);//Xtal load cap = 8 pF
	unsigned char pcdata[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
	si5351_burst(16, pcdata, 8);//turn off all outputs
	si5351_byte(149, 0x00);//disable spread spectrum
	si5351_PLLs(900.0);//PLLA and PLLB
	si5351_byte(177, 0xA0);//PLL soft reset
	si5351_byte(165, 0x00);//MSynth0 phase
	si5351_byte(167, 0x00);//MSynth2 phase
	si5351_byte(187, 0x00);//disable MSynth0 fanout
	si5351_freq(2.0);
	si5351_byte(16, 0x0d);//CLK0_PON, Fractional, PLLA, not invert, MSynth0, 4 mA
//	si5351_byte(16, 0x0c);//CLK0_PON, Fractional, PLLA, not invert, MSynth0, 2 mA
	si5351_byte(18, 0x0d);//CLK2_PON, Fractional, PLLA, not invert, MSynth2, 4 mA
	si5351_byte(3, 0xfa);//enable CLK0+CLK2 outputs
//	si5351_byte(177,0xA0);	//PLL soft reset
}

unsigned int DACbuffer[1] __attribute__((space(dma)));

void initDAC(void) {
	DACbuffer[0] = 0x8000;
	//init DMA1
	DMA1CONbits.SIZE = 0;//Data transfer size = word
	DMA1CONbits.DIR = 1;//Write to peripheral
	DMA1CONbits.AMODE = 1;	// Configure DMA for Register indirect w/o post increment
	DMA1CONbits.MODE  = 0;	// Configure DMA for Continuous, Ping-Pong mode disabled
	DMA1PAD=(int)&DAC1LDAT;
	DMA1CNT=0;	
	DMA1REQ=79;
	DMA1STA = __builtin_dmaoffset(DACbuffer);
	IFS0bits.DMA1IF = 0;	//Clear the DMA interrupt flag bit
	IEC0bits.DMA1IE = 0;	//Disable DMA interrupt
	DMA1CONbits.CHEN=1;

	//set auxiliary clock for DAC operation
	ACLKCONbits.SELACLK = 0;	//PLL output ==> auxiliary clk source
	ACLKCONbits.APSTSCLR = 7; //auxiliary clk divider: divided by 1

	//init DAC
	DAC1STATbits.LOEN = 1;	//Left channel DAC output enabled
	DAC1STATbits.LITYPE = 1;	//Interrupt if FIFP is empty (interrupt is controlled with DMA1IE)
	DAC1CONbits.AMPON = 0;	//Amplifier is disabled during sleep/idle mode
	DAC1CONbits.DACFDIV = 31;	//Divide clock by 32 (19.52 kHz sampling if Fcy = 40 MHz)
	DAC1CONbits.FORM = 0;	//Data format is unsigned
	DAC1CONbits.DACEN = 1;	//DAC1 module enabled
	DAC1DFLT = 0x8000;
}

#define BIASR 0x0200
#define BIASI 0x0200
#define AVGDIM 256
#define AVGDIMP 8
int iavgr[AVGDIM]; int iavgi[AVGDIM];

#define AM_DACSCL 6
#define DISP_RANGE 4
#define FRQMAX 225000
#define FRQMIN 7
#define NDISP_LEDHIGH 100
#define NDISP_LEDLOW 500
#define DEMODMAX 1
int iAtt = 10;//11;
int ibiasr = BIASR;
int ibiasi = BIASI;
unsigned int icyc = 0;
int iMode = 0;
char cDemod = 1;//AM-FM
double dfreq = 82500 - AFOUT * FRQSTEP;
int idisp = 0;
unsigned int iPrevRB11 = 1;
unsigned int iPrevRB12 = 1;
float fprev = 0;
int iamp = 0;

void __attribute__((interrupt, no_auto_psv)) _T2Interrupt(void)
{
	IEC0bits.T2IE = 0;

	icyc++;
	int iADCdiv = iAtt >> 1;

	//audio
	if (cDemod == 0) {
		//AM
		//FFT
		int i = DMA0CNT >> 1;
		int j=NDIM-1;
		while (j >= 0) {
			int isumr = 0; int isumi = 0; int k;
			for (k=0; k<8; k++) {
				isumr += BufferA[i*2+1];
				isumi += BufferA[i*2];
				i--; if (i<0) i=NDDIM-1;
			}
			ifr[j] = ((isumr >> 3) - ibiasr) >> iADCdiv;
			ifi[j] = ((isumi >> 3) - ibiasi) >> iADCdiv;
			if (iAtt & 0x01) {
				ifr[j] = (ifr[j] * 11) >> 4;//*0.7
				ifi[j] = (ifi[j] * 11) >> 4;//*0.7
			}
			j--;
		}
		FFT();
		iamp = (int)sqrt((long)(ifr[AFOUT]) * ifr[AFOUT] + (long)ifi[AFOUT] * ifi[AFOUT]);
		iamp <<= AM_DACSCL;
	} else if (cDemod == 1) {
		//FM
		/*
		int i = DMA0CNT >> 1;
		int j=128;
		long lsum1 = 0; long lsum0 = 0;
		while (j >= 0) {
			lsum0 += (BufferA[i*2] - ibiasr);
			lsum1 += (BufferA[i*2+1] - ibiasi);
			i--; if (i<0) i=NDDIM-1;
			j--;
		}
		float famp = lsum1 ? atan((float)lsum0 / lsum1) : 1.57;
		int ipow = 0x2000 >> (iAtt >> 1);
		if (iAtt & 0x01) iamp = (int)((famp - fprev) * 3.535534 * ipow + 0x8000);
		else iamp = (int)((famp - fprev) * 5.0 * ipow + 0x8000);
		fprev = famp;
		///*///
		int i = DMA0CNT >> 1;
		int j=NFDIM-1;
		while (j >= 0) {
			int isumr = 0; int isumi = 0; int k;
			for (k=0; k<2; k++) {
				isumr += BufferA[i*2+1];
				isumi += BufferA[i*2];
				i--; if (i<0) i=NDDIM-1;
			}
			iffr[j] = ((isumr >> 1) - ibiasr) >> iADCdiv;
			iffi[j] = ((isumi >> 1) - ibiasi) >> iADCdiv;
			j--;
		}
		FFTF();
		long lsum = 0; float fcrd = 0;
		for (i=AFOUT*4; i<NFDIM-AFOUT*3; i++) {
			j = ((i - (NFDIM>>1)) & (NFDIM-1));
			//f: 0,1,2,...15 -16,-15,-14,...-1
			long lnorm = ((long)iffr[j] * iffr[j] + (long)iffi[j] * iffi[j]);
			if (lnorm > 100) fcrd += lnorm * (i-(NFDIM>>1)+AFOUT);
			lsum += lnorm;
		}
		float famp = fcrd / lsum;
		int ipow = 0x2000 >> (iAtt >> 1);
		if (iAtt & 0x01) iamp = (int)((famp - fprev * 0.8) * 3.535534 * ipow + 0x8000);
		else iamp = (int)((famp - fprev * 0.8) * 5.0 * ipow + 0x8000);
		fprev = famp;
		///*///
	}
	DACbuffer[0] = iamp;

	//switches
	if ((icyc & 0xff) == 0) {
		if (PORTBbits.RB11 == 0) {//push sw
			if (iPrevRB11) {
				idisp = 0;
				if (idisp < NDISP_LEDLOW) {
					cRotEnc = cRotEnc ? 0 : 1;
//					iMode++;
//					if (iMode > 7) iMode = 0;
				}
			}
			iPrevRB11 = 0;
		} else {
			iPrevRB11 = 1;
		}
	}
	if ((icyc & 0xf) == 0) {
		if (PORTBbits.RB12 == 0) {//rotary encoder
			if (iPrevRB12) {
				idisp = 0;
				if (idisp < NDISP_LEDLOW) {
					if (cRotEnc) {
						if (PORTBbits.RB13 == 0) {
							switch (iMode) {
								case 0: {if (iAtt < VOLMAX) iAtt++; break;}
								case 1: {if (cDemod > 0) cDemod--; else cDemod=DEMODMAX; break;}
								case 2: {if (dfreq-10000 >= FRQMIN) dfreq-=10000; break;}
								case 3: {if (dfreq-1000 >= FRQMIN) dfreq-=1000; break;}
								case 4: {if (dfreq-100 >= FRQMIN) dfreq-=100; break;}
								case 5: {if (dfreq-10 >= FRQMIN) dfreq-=10; break;}
								case 6: {if (dfreq-1 >= FRQMIN) dfreq-=1; break;}
								case 7: {if (dfreq-0.1 >= FRQMIN) dfreq-=0.1; break;}
							}
						} else {
							switch (iMode) {
								case 0: {if (iAtt > 0) iAtt--; break;}
								case 1: {if (cDemod < DEMODMAX) cDemod++; else cDemod=0; break;}
								case 2: {if (dfreq+10000 <= FRQMAX) dfreq+=10000; break;}
								case 3: {if (dfreq+1000 <= FRQMAX) dfreq+=1000; break;}
								case 4: {if (dfreq+100 <= FRQMAX) dfreq+=100; break;}
								case 5: {if (dfreq+10 <= FRQMAX) dfreq+=10; break;}
								case 6: {if (dfreq+1 <= FRQMAX) dfreq+=1; break;}
								case 7: {if (dfreq+0.1 <= FRQMAX) dfreq+=0.1; break;}
							}
						}
						if (iMode > 1) {si5351_freq(dfreq * 0.001);}
					} else {
						if (PORTBbits.RB13 == 0) {
							iMode--;
							if (iMode < 0) iMode = 7;
						} else {
							iMode++;
							if (iMode > 7) iMode = 0;
						}
					}//(cRotEnc)
				}//(idisp < NDISP_LEDLOW)
			}
			iPrevRB12 = 0;
		} else {
			iPrevRB12 = 1;
		}
	}

	IFS0bits.T2IF = 0;
	IEC0bits.T2IE = 1;
}

int main (void)
{

// Configure Oscillator to operate the device at 40Mhz
// Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
// Fosc= 6M*80/(2*3)=80Mhz for 6M input clock
//	PLLFBD=78;					// M=80
// Fosc= 7.3728M*65/(2*3)=79.872Mhz for FRC 1/1 input clock
	PLLFBD=63;					// M=65
	CLKDIVbits.PLLPOST=0;		// N1=2
	CLKDIVbits.PLLPRE=1;		// N2=3
	OSCTUN=0;					// Tune FRC oscillator, if FRC is used
	CLKDIVbits.FRCDIV = 0;	//FRC divided by 1

// Disable Watch Dog Timer
	RCONbits.SWDTEN=0;

// clock switching to incorporate PLL
	__builtin_write_OSCCONH(0x01);		// Initiate Clock Switch to FRC with PLL (NOSC=0x01)										
//	__builtin_write_OSCCONH(0x03);		// Initiate Clock Switch to Primary Oscillator with PLL (NOSC=0x03)										
	__builtin_write_OSCCONL(OSCCON || 0x01);		// Start clock switching
	while (OSCCONbits.COSC != 0x01);	// Wait for Clock switch to occur
//	while (OSCCONbits.COSC != 0x03);	// Wait for Clock switch to occur
	while(OSCCONbits.LOCK!=1);		// Wait for PLL to lock

//Set ports as digital
	AD1PCFGL=0xFFFF;

//config timer2
	T2CONbits.T32 = 0;
//	T2CONbits.TCKPS = 3;//prescale 1/256
	T2CONbits.TCKPS = 2;//prescale 1/64
	T2CONbits.TCS = 0;//use internal FCY
	T2CONbits.TGATE = 0;
//	PR2 = 49999;//timer2 preset: FCY(40MHz) / 256 / 50000 = 3.125Hz
	PR2 = 77;//timer2 preset: FCY(40MHz) / 64 / 78 = 8013Hz
	IFS0bits.T2IF = 0;
	IEC0bits.T2IE = 1;
//	T2CONbits.TON = 1;//start timer2

//wait for 200 ms
//	int i, j;
//	for (i=0; i<100; i++) {wait2ms();}

//switches
	TRISBbits.TRISB13 = 1;
	TRISBbits.TRISB12 = 1;
	TRISBbits.TRISB11 = 1;//push sw
	CNPU1bits.CN13PUE = 1;//enable pull ups
	CNPU1bits.CN14PUE = 1;
	CNPU1bits.CN15PUE = 1;

//lcd
	//lcd_ports();
	//lcd_init();
	//lcd_clear();
	clcd_ports();
	clcd_init();
	clcd_clear(0x0000);
	clcd_led_high();

//x axis
//	lcd_pos(0,3);
//	for (i=0; i<NDDIM; i++) {lcd_write(0x01, 1);}
	clcd_xline(64, 0, 127, 0x8410);
	clcd_yline(64+AFOUT, 65, 68, 0x8410);
	cxcrd = 64+AFOUT+2; cycrd = 66; clcd_smallstring("0", 0x8410);
	clcd_yline(64+AFOUT+(100/FRQSTEP), 65, 68, 0x8410);
	cxcrd = 64+AFOUT+(100/FRQSTEP)+2; clcd_smallstring("100", 0x8410);
	clcd_yline(64+AFOUT-(100/FRQSTEP), 65, 68, 0x8410);
	cxcrd = 64+AFOUT-(100/FRQSTEP)-12; clcd_smallstring("100", 0x8410);
	clcd_yline(64+AFOUT+(200/FRQSTEP), 65, 68, 0x8410);
	cxcrd = 64+AFOUT+(200/FRQSTEP)+2; clcd_smallstring("200", 0x8410);
	clcd_yline(64+AFOUT-(200/FRQSTEP), 65, 68, 0x8410);
	cxcrd = 64+AFOUT-(200/FRQSTEP)-12; clcd_smallstring("200", 0x8410);

//local osc
	si5351_init();

//disp
	cdispParam(iMode, iAtt, cDemod);
	cdispFreq(dfreq, iMode-2);

	int i;
	for (i=0; i<NDDIM; i++) {itraj[i] = 0; cspect[i] = 0;}
	for (i=0; i<NDDIM*TRAJMAX; i++) {ctraj[i] = 0;}
	clcd_traj();
	for (i=0; i<AVGDIM; i++) {iavgr[i] = BIASR; iavgi[i] = BIASI;}
	FFTinit();
	FFTDinit();
	FFTFinit();

	unsigned int icyc2 = 0;

	initDma0();	// Initialise the DMA controller to buffer ADC data
   	initAdc1();	// Initialize the A/D converter
	initDAC();
	si5351_freq(dfreq * 0.001);
	cdispFreq(dfreq, iMode-2);
	cdispParam(iMode, iAtt, cDemod);

	T2CONbits.TON = 1;//start timer2
	while (1) {
		//led
		if (idisp < NDISP_LEDHIGH) {clcd_led_high(); idisp++;}
		else if (idisp < NDISP_LEDLOW) {clcd_led_low(); idisp++;}
		else {clcd_led_off();}

		//params
		if (idisp < NDISP_LEDLOW) {
			cdispParam(iMode, iAtt, cDemod);
			cdispFreq(dfreq, iMode-2);
		}

		//adjust DC level
		icyc2++;
		int j = icyc2 & (AVGDIM - 1);
		long lavgr = 0; long lavgi = 0;
		for (i=0; i<NDDIM; i++) {
			lavgr += BufferA[i*2+1]; lavgi += BufferA[i*2];
		}
		iavgr[j] = lavgr >> NDDIMP; iavgi[j] = lavgi >> NDDIMP;
		long lbiasr = 0; long lbiasi = 0;
		for (i=0; i<AVGDIM; i++) {lbiasr += iavgr[i]; lbiasi += iavgi[i];}
		ibiasr = lbiasr >> AVGDIMP; ibiasi = lbiasi >> AVGDIMP;

		//show spectrum and trajectory
		j=NDDIM-1;
		i = DMA0CNT >> 1;
		int iADCdiv = iAtt >> 1;
		while (j >= 0) {
			idfr[j] = (BufferA[i*2+1] - ibiasr) >> iADCdiv;
			idfi[j] = (BufferA[i*2] - ibiasi) >> iADCdiv;
			if (iAtt & 0x01) {
				idfr[j] = (idfr[j] * 11) >> 4;
				idfi[j] = (idfi[j] * 11) >> 4;
			}
			i--; if (i<0) i=NDDIM-1;
			j--;
		}
		FFTD();
		clcd_spectrum(DISP_RANGE, 1);
		clcd_traj();
//		unsigned char ctraj = ((icyc & 0x03fff) == 0);
//		lcd_spectrum(DISP_RANGE, ctraj);
//		if (ctraj) {lcd_traj();}
	}
}

