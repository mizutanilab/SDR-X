/**********************************************************************
* © 2005 Microchip Technology Inc.
*
* FileName:        adcDrv2.c
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
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~**
*
* ADDITIONAL NOTES:
* This file contains two functions - initAdc1(), initDma0 and _DMA0Interrupt().
*
**********************************************************************/

#if defined(__dsPIC33F__)
#include "p33Fxxxx.h"
#elif defined(__PIC24H__)
#include "p24Hxxxx.h"
#endif

#include "adcDrv2.h"

// User Defines
#define FCY		40000000  			// User must calculate and enter FCY here
#define Dly_Time (20E-6 * FCY)      // ADC Off-to-On delay

//#define NUMSAMP	64
//int BufferA[NUMSAMP] __attribute__((space(dma)));
//int BufferB[NUMSAMP] __attribute__((space(dma)));

//void ProcessADCSamples(int * AdcBuffer);



//Functions:
//initAdc1() is used to configure A/D to convert AN0 using CH0 and CH1 sample/hold in sequencial mode 
//at 1.1MHz throughput rate. ADC clock is configured at 13.3Mhz or Tad=75ns
void initAdc1(void)
{
	AD1CON1bits.ADSIDL = 1;//Discontinue module operation when device enters Idle mode
	AD1CON1bits.FORM   = 0;		// Integer (DOUT = 0000 00dd dddd dddd)
//		AD1CON1bits.FORM = 3;		// Data Output Format: Signed Fraction (Q15 format)
		AD1CON1bits.SSRC = 7;		// Internal counter ends sampling and starts conversion (auto-convert)
		AD1CON1bits.ASAM = 0;		// Sampling begins when SAMP bit is set (for now)
		AD1CON1bits.AD12B = 0;		// 10-bit ADC operation
		AD1CON1bits.ADDMABM = 1; 	// DMA buffers are built in conversion order mode
	AD1CON1bits.SIMSAM = 1;//Samples CH0 and CH1 simultaneously (when CHPS<1:0> = 01)
		
	AD1CON2bits.VCFG = 0;//Converter Voltage Reference: AVDD-AVSS
	AD1CON2bits.CSCNA = 0;//Scan Input Selections for CH0+ during Sample A bit = Do not scan inputs
	AD1CON2bits.ALTS = 0;//Always uses channel input selects for Sample A
		AD1CON2bits.SMPI  = 0;		// SMPI must be 0
		AD1CON2bits.CHPS  = 1;		// Converts CH0/CH1
     
		AD1CON3bits.ADRC = 0;		// ADC Clock is derived from Systems Clock
	AD1CON3bits.SAMC=0; 		// Auto Sample Time = 0*Tad is sufficient
//		AD1CON3bits.SAMC = 2;		// Auto Sample Time = 2*Tad		
		AD1CON3bits.ADCS = 2;		// ADC Conversion Clock Tad=Tcy*(ADCS+1)= (1/40M)*3 = 75ns (13.3Mhz)
									// ADC Conversion Time for 10-bit Tc=12*Tad =  900ns (1.1MHz)
									// Total: 1875ns = 900ns + 900ns + 75ns unknown interval (inherent sampling?)

//	AD1CON4bits.DMABL = 6;//64 word//////////////

  	//AD1CHS0/AD1CHS123: A/D Input Select Register
        AD1CHS0bits.CH0SA = 1;		// MUXA +ve input selection (AN1) for CH0
//        AD1CHS0bits.CH0SA = 0;		// MUXA +ve input selection (AN0) for CH0
		AD1CHS0bits.CH0NA = 0;		// MUXA -ve input selection (Vref-) for CH0

        AD1CHS123bits.CH123SA = 0;	// MUXA +ve input selection (AN0) for CH1
		AD1CHS123bits.CH123NA = 0;	// MUXA -ve input selection (Vref-) for CH1


	//AD1PCFGH/AD1PCFGL: Port Configuration Register
//160211		AD2PCFGL=0xFFFF;	// Set as digital
		AD1PCFGL=0xFFFF;	// Set as digital
//160211		AD1PCFGH=0xFFFF;	// Set as digital
		AD1PCFGLbits.PCFG0 = 0;		// AN0 as Analog Input
		AD1PCFGLbits.PCFG1 = 0;		// AN1 as Analog Input


        IFS0bits.AD1IF = 0;			// Clear the A/D interrupt flag bit
//        IEC0bits.AD1IE = 1;			// Enable A/D interrupt 
        IEC0bits.AD1IE = 0;			// Do Not Enable A/D interrupt 
        
        AD1CON1bits.ADON = 1;		// Turn on the A/D converter	
        
        Dly_Time;			// Delay for 20uS to allow ADC to settle (25nS * 0x320 = 20uS)
        
        AD1CON1bits.ASAM = 1;		// Sampling begins immediately after last conversion. SAMP bit is auto-set
 

}

// DMA0 configuration
// Direction: Read from peripheral address 0-x300 (ADC1BUF0) and write to DMA RAM 
// AMODE: Register indirect with post increment
// MODE: Continuous, Ping-Pong Mode
// IRQ: ADC Interrupt
// ADC stores results stored alternatively between DMA_BASE[0]/DMA_BASE[16] on every 16th DMA request 

/*
void initDma0(void)
{
	// Initialize pin for toggling in DMA interrupt
//160211	_TRISA6 = 0;
//160211	_LATA6 = 0;

//	DMA0CONbits.SIZE = 0;//Data transfer size = word
//	DMA0CONbits.DIR = 0;//Read from peripheral
	DMA0CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
	DMA0CONbits.MODE  = 2;			// Configure DMA for Continuous Ping-Pong mode

	DMA0PAD=(int)&ADC1BUF0;
	DMA0CNT=(NUMSAMP-1);				
	
	DMA0REQ=13;	
	
	DMA0STA = __builtin_dmaoffset(BufferA);		
	DMA0STB = __builtin_dmaoffset(BufferB);

	IFS0bits.DMA0IF = 0;			//Clear the DMA interrupt flag bit
    IEC0bits.DMA0IE = 1;			//Set the DMA interrupt enable bit
	
	DMA0CONbits.CHEN=1;


}*/





/*=============================================================================
_DMA0Interrupt(): ISR name is chosen from the device linker script.
=============================================================================*/
/*unsigned int DmaBuffer = 0;

void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void)
{

		if(DmaBuffer == 0)
		{
			ProcessADCSamples(BufferA);
		}
		else
		{
			ProcessADCSamples(BufferB);

		}

		DmaBuffer ^= 1;

		__builtin_btg((unsigned int *)&LATA, 6); // Toggle RA6	
        IFS0bits.DMA0IF = 0;					 //Clear the DMA0 Interrupt Flag
}



void ProcessADCSamples(int * AdcBuffer)
{
	// Do something with ADC Samples
}*/




