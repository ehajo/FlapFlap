/******************************************************************************
 * @file     EasyMain.c
 * @brief    Uses a system timer to toggle the  ports P0.5, P0.6, P1.5 and P1.6 with 200ms frequency.
 *			 LEDs that are connected to these ports will toggle respectively.
 * 			 In addition it uses the UART of USIC Channel 1 to send a message every 2s to a terminal
 *			 emulator. The communication settings are 57.6kbps/8N1
 *			 P1.3 is configured as input (RXD) and P1.2 is configured as output(TXD)
 *			 This project runs without modifications on the XMC1100 kit for ARDUINO
 * @version  V1.0
 * @date     20. February 2015
 * @note
 * Copyright (C) 2015 Infineon Technologies AG. All rights reserved.
 ******************************************************************************
 * @par
 * Infineon Technologies AG (Infineon) is supplying this software for use with
 * Infineon�s microcontrollers.
 * This file can be freely distributed within development tools that are
 * supporting such microcontrollers.
 * @par
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *****************************************************************************/

#include <string.h>

#include "XMC1100.h"
#include "xmc_uart.h"
#include "xmc_gpio.h"
#include "xmc_ccu4.h"
#include "xmc_scu.h"

#include "GPIO.h"

#include "port.h"
#include "mbport.h"
#include "mb.h"

#include "modbus_if.h"

#define  DSEL_DXnA ( 0 )
#define  DSEL_DXnB ( 1 )
#define  DSEL_DXnC ( 2 )
#define  DSEL_DXnD ( 3 )
#define  DSEL_DXnE ( 4 )
#define  DSEL_DXnF ( 5 )
#define  DSEL_DXnG ( 6 )
#define  DSEL_ALWAYS_ONE ( 7 )

/*********************************************************************************************************************
 * MACROS
 *********************************************************************************************************************/
#define SLICE_PTR         CCU40_CC40
#define MODULE_PTR        CCU40
#define MODULE_NUMBER     (0U)
#define SLICE_NUMBER      (0U)
#define CCU_OUTPUT		  P0_5
/* We need 3.5T as 3.5 Char Time */
/* We use 19200 Baud meaning 52us per Bit */
/* With 8e1 and 1 stopbit and 1 startbit we got 11 Bits per char */
/* times 3.5 menas 38.5 bits -> 38.5*50us = 2.028 ms timeout.... */

/* Period 62499 -> 64000000 means (64Mhz/1024 -1) resulting in 1 secong */
/* We need a 2.028ms timer here with 1 tick = 16us */
/* Resulting in 127,75 -> 128 ticks -1 - > 127 */


#define PWMPERIOD		  (127U) //PWM period calculated based on PCLK = 64MHz
#define DUTYCYCLE		  (127U)



XMC_GPIO_CONFIG_t rx_pin_config;
XMC_GPIO_CONFIG_t tx_pin_config;


/* Config for Systick timer */
#define TICKS_PER_SECOND 1000

XMC_CCU4_SLICE_COMPARE_CONFIG_t compare_config =
{
  .timer_mode 		   = (uint32_t) XMC_CCU4_SLICE_TIMER_COUNT_MODE_EA,
  .monoshot   		   = (uint32_t) true,
  .shadow_xfer_clear   = (uint32_t) 0,
  .dither_timer_period = (uint32_t) 0,
  .dither_duty_cycle   = (uint32_t) 0,
  .prescaler_mode	   = (uint32_t) XMC_CCU4_SLICE_PRESCALER_MODE_NORMAL,
  .mcm_enable		   = (uint32_t) 0,
  .prescaler_initval   = (uint32_t) 10,
  .float_limit		   = (uint32_t) 0,
  .dither_limit		   = (uint32_t) 0,
  .passive_level 	   = (uint32_t) XMC_CCU4_SLICE_OUTPUT_PASSIVE_LEVEL_LOW,
  .timer_concatenation = (uint32_t) 0
};

volatile bool boService=true;
extern void ff_check_position( void );

volatile uint32_t Delay_x =0;
void SysTick_Handler(void)
{
	if(Delay_x>0){
		Delay_x--;
	}
	if(boService==false){
		ff_check_position();
	}


}


void SetUpGPIO(){

	/* ID Switch */
	/*
	 * P2.4
	 * P2.5
	 * P2.6
	 * P2.7
	 * P2.8
	 * P2.9
	 * P2.10
	 * P2.11
	 *
	 */

	XMC_GPIO_CONFIG_t pin_input_config_pullup =
		{
		   .mode                = XMC_GPIO_MODE_INPUT_PULL_UP,
		   .input_hysteresis    = XMC_GPIO_INPUT_HYSTERESIS_STANDARD,
		   .output_level        = XMC_GPIO_OUTPUT_LEVEL_LOW,
		};
	XMC_GPIO_Init(P2_4, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_5, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_6, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_7, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_8, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_9, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_10, &pin_input_config_pullup);
	XMC_GPIO_Init(P2_11, &pin_input_config_pullup);

	XMC_GPIO_CONFIG_t pin_output_config_push_pull =
			{
			   .mode                = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
			   .input_hysteresis    = XMC_GPIO_INPUT_HYSTERESIS_STANDARD,
			   .output_level        = XMC_GPIO_OUTPUT_LEVEL_HIGH,
			};

	/* Output for Waveform Generation Flap Flap */
	XMC_GPIO_Init(P0_13, &pin_output_config_push_pull); /* !AD    */
	XMC_GPIO_Init(P0_1, &pin_output_config_push_pull); /* !COL    */
	XMC_GPIO_Init(P0_2, &pin_output_config_push_pull); /* !Start    */

	/* LEDs */
   XMC_GPIO_Init(XMC_GPIO_PORT0,4, &pin_output_config_push_pull);
   XMC_GPIO_Init(XMC_GPIO_PORT0,3, &pin_output_config_push_pull);


	/* Encoder Input */
	XMC_GPIO_CONFIG_t pin_input_config_inv_pullup =
		{
		   .mode                = XMC_GPIO_MODE_INPUT_INVERTED_PULL_UP,
		   .input_hysteresis    = XMC_GPIO_INPUT_HYSTERESIS_STANDARD,
		   .output_level        = XMC_GPIO_OUTPUT_LEVEL_LOW,
		};

	XMC_GPIO_Init(P1_0, &pin_input_config_inv_pullup);
	XMC_GPIO_Init(P1_1, &pin_input_config_inv_pullup);
	XMC_GPIO_Init(P1_2, &pin_input_config_inv_pullup);
	XMC_GPIO_Init(P1_3, &pin_input_config_inv_pullup);
	XMC_GPIO_Init(P1_4, &pin_input_config_inv_pullup);
	XMC_GPIO_Init(P1_5, &pin_input_config_inv_pullup);


	/* TX Init */
	XMC_GPIO_Init(P2_2, &pin_output_config_push_pull);



	/* Disable Drive */
	XMC_GPIO_SetOutputHigh(P0_13);
	XMC_GPIO_SetOutputHigh(P0_1);
	XMC_GPIO_SetOutputHigh(P0_2);

	/* Disable TX  */
	XMC_GPIO_SetOutputLow(P0_3);
	XMC_GPIO_SetOutputHigh(P0_4);

	ff_setup();
}

const XMC_SCU_CLOCK_CONFIG_t clock_config =
{
  .pclk_src = XMC_SCU_CLOCK_PCLKSRC_DOUBLE_MCLK, /*PCLK = 2*MCLK*/
  .rtc_src = XMC_SCU_CLOCK_RTCCLKSRC_DCO2,
  .fdiv = 0,  /**< Fractional divider */
  .idiv = 1,  /**MCLK = 32MHz */
};

unsigned char ReverseBits(uint8_t v)
{
    v = ((v >> 1) & 0x55) | ((v & 0x55) << 1);
    v = ((v >> 2) & 0x33) | ((v & 0x33) << 2);
    v = ((v >> 4) & 0x0F) | ((v & 0x0F) << 4);
    return v;
}

uint8_t ReadDipSwitch( void ){

uint8_t Value = ((PORT2->IN & 0x00000FF0)>>4);
/* Change bits as someone may need to read more */
Value=ReverseBits(Value);
Value=~Value;
return Value;

}

void __delay_ms(uint32_t d){

	while(Delay_x!=d){
		  Delay_x=d;
	  }

	  while(Delay_x>0){
		  __NOP();
	  }

}
volatile uint8_t u8Address=0;
int main(void)
{
	uint32_t Delay = 0xFFFF;
	uint8_t u8DIPSwitch=0;
	/* Ensure clock frequency is set at 64MHz (MCLK) */
	XMC_SCU_CLOCK_Init(&clock_config);

	/* Ensure fCCU reaches CCU40 */
		XMC_CCU4_SetModuleClock(MODULE_PTR, XMC_CCU4_CLOCK_SCU);
		XMC_CCU4_Init(MODULE_PTR, XMC_CCU4_SLICE_MCMS_ACTION_TRANSFER_PR_CR);

		/* Get the slice out of idle mode */
		XMC_CCU4_EnableClock(MODULE_PTR, SLICE_NUMBER);

		/* Start the prescaler and restore clocks to slices */
		XMC_CCU4_StartPrescaler(MODULE_PTR);

		/* Initialize the Slice */
		XMC_CCU4_SLICE_CompareInit(SLICE_PTR, &compare_config);

		/* Enable compare match events */
		XMC_CCU4_SLICE_EnableEvent(SLICE_PTR, XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP);

	    /* Connect compare match event to SR0 */
		XMC_CCU4_SLICE_SetInterruptNode(SLICE_PTR, XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP, XMC_CCU4_SLICE_SR_ID_0);

		/* Configure NVIC */
		/* Set priority */
		NVIC_SetPriority(CCU40_0_IRQn, 3U);

		/* Enable IRQ */
		NVIC_EnableIRQ(CCU40_0_IRQn);

		/* Program duty cycle = 50% at 1Hz frequency */
		XMC_CCU4_SLICE_SetTimerCompareMatch(SLICE_PTR, DUTYCYCLE);
		XMC_CCU4_SLICE_SetTimerPeriodMatch(SLICE_PTR, PWMPERIOD);

		/* Enable shadow transfer */
		XMC_CCU4_EnableShadowTransfer(MODULE_PTR, (uint32_t)(XMC_CCU4_SHADOW_TRANSFER_SLICE_0|XMC_CCU4_SHADOW_TRANSFER_PRESCALER_SLICE_0));

		/*Enable CCU4 output*/
		/*XMC_GPIO_CONFIG_t ccu_output_config =
		{
		   .mode                = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT4,
		   .input_hysteresis    = XMC_GPIO_INPUT_HYSTERESIS_STANDARD,
		   .output_level        = XMC_GPIO_OUTPUT_LEVEL_LOW,
		};

		XMC_GPIO_Init(CCU_OUTPUT, &ccu_output_config);
		*/





  SetUpGPIO();


  MB_register_UART(XMC_UART0_CH0);
  SysTick_Config(SystemCoreClock / TICKS_PER_SECOND);
  /* Address of 0 disabels the device */
  __delay_ms(250);
  do{
	  u8DIPSwitch = ReadDipSwitch();
  }
  while(0==(u8DIPSwitch&0x3F));
  if(0!=(u8DIPSwitch&0x80)){
	  /* We enter the Testmode Position will be set by DIP Switch */

	  /* If bit 6 is used we just set the pins to dsiplay the address */
	  if(u8DIPSwitch&0x40){
		  voService(); /* Set the IO's */
		  while(1==1){
		  __NOP(); /* We do nothing */
		  }
	  } else{ /* Okay we now will use the DIP ones for position */
		  boService=false;
		  while(1==1){
			  /* We Read the Dipswitch and Write the Position */
			  u8DIPSwitch = ReadDipSwitch();
			  voManualSetPosition((u8DIPSwitch&0x3F));
		  }
	  }


  } else {
	  /* We enter normal operation */
	  boService=false;
	  u8Address = u8DIPSwitch&0x3F; /* We use the bits 0-5 for address */
	  eMBInit(MB_RTU,u8Address,0,115200,MB_PAR_NONE); /* And 115k2 for serial speed */
	  eMBEnable(); /* We enable the Stack */
	  NVIC_ClearPendingIRQ(USIC0_0_IRQn); /* We clear any pending ire for ter uart */
	  NVIC_EnableIRQ(USIC0_0_IRQn); /* And enable it's interrupts */
	  while(1 == 1)
	  {
		/* Infinite loop */
		  if(Delay>0){
			  Delay--;
		  }else{
			  XMC_GPIO_ToggleOutput(XMC_GPIO_PORT0,4);
			  Delay=0xFFFF;
		  }
		  mb_stack_task(); /* Serve the Stack *7
	  }
	}
}
void Default_handler( void ){
/* We shall never end here */
}

void USIC0_0_IRQHandler ( void ){
/* This is the IRQ Handler for the USCI ( USART in the Atmel World ) */


/* This can be called by diffetent Sources therfore we need to determine witch */
uint32_t status = XMC_UART_CH_GetStatusFlag( XMC_UART0_CH0 );

    // Did we receive data?
    if ((status & (XMC_UART_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION | XMC_UART_CH_STATUS_FLAG_RECEIVE_INDICATION)) != 0U)
    {
    	/* We need to add the correct callback here */
    	MB_RxHandler();
    	XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, XMC_UART_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION );
    	XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, XMC_UART_CH_STATUS_FLAG_RECEIVE_INDICATION);
    }

    /* Transmission in Progress ? */
    if (( status & ( XMC_UART_CH_STATUS_FLAG_TRANSMIT_BUFFER_INDICATION)) != 0U){
    	/* We can if requiered refill data to the buffer */
    	 XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, XMC_UART_CH_STATUS_FLAG_TRANSMIT_BUFFER_INDICATION);

    }

    /* Transmitt finished */
    if ((status & ( XMC_UART_CH_STATUS_FLAG_TRANSMIT_SHIFT_INDICATION)) != 0U){
    	/* We can if requiered refill data to the buffer */
    	XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, XMC_UART_CH_STATUS_FLAG_TRANSMIT_SHIFT_INDICATION);
    	MB_TxHandler();
    }

	if ((status & ( XMC_UART_CH_STATUS_FLAG_TRANSMITTER_FRAME_FINISHED)) != 0U){
	    	/* We can if requiered refill data to the buffer */
	    	XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, XMC_UART_CH_STATUS_FLAG_TRANSMITTER_FRAME_FINISHED);

	}

}




























