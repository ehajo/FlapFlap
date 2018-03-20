/******************************************************************************
* Software License Agreement
*
* Copyright (c) 2016, Infineon Technologies AG
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* Neither the name of the copyright holders nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission

* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* To improve the quality of the software, users are encouraged to share
* modifications, enhancements or bug fixes with Infineon Technologies AG
* (dave@infineon.com).
*
*****************************************************************************/

/****************************************************************
* HEADER FILES
***************************************************************/
/* Enter MCU Includes here */
#include "XMC1100.h"
#include "xmc_uart.h"
#include "xmc_gpio.h"
#include "GPIO.h"

#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "mbconfig.h"

#if ((MB_RTU_ENABLED|MB_ASCII_ENABLED) > 0U)
/****************************************************************
* LOCAL DATA
***************************************************************/

static XMC_USIC_CH_t *ptr_uart = 0;

/****************************************************************
* API IMPLEMENTATION
***************************************************************/
void MB_register_UART(XMC_USIC_CH_t *uart)
{
	ptr_uart = uart;
}

void  vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
	/* We check if only TX or RX is active else we have a problem */
	if( ( TRUE == xRxEnable ) && ( TRUE == xTxEnable ) ){ /* COVERS TX=TRUE and RX=TRUE */
		/* This is an unsupported configuration */
		/* We fall back to RX only and if supported report an error */
	} else if(TRUE==xTxEnable ){ /* Covers TX = TRUE and RX =FALSE */
		/* Set the RS485 Transreceiver to TX */
		XMC_GPIO_SetOutputHigh(P0_3);
		XMC_GPIO_SetOutputLow(P0_4);



		(void)pxMBFrameCBTransmitterEmpty();
	} else if(TRUE==xRxEnable){ /* COVERS TX = FALSE and RX = TRUE */
		XMC_GPIO_SetOutputLow(P0_3);
		XMC_GPIO_SetOutputHigh(P0_4);


	} else if( ( FALSE == xRxEnable ) && ( FALSE == xTxEnable ) ){ /* COVERS TX = FALSE and RX = FALSE */
		XMC_GPIO_SetOutputLow(P0_3);
		XMC_GPIO_SetOutputHigh(P0_4);


	} else {
	  /* Shall not happen */
	}
}

void
vMBPortClose( void )
{
}

BOOL
xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
  BOOL return_value=false;

  if (ptr_uart == XMC_UART0_CH0)
  {

	  XMC_UART_CH_CONFIG_t uart_config;
	  uart_config.data_bits = ucDataBits;
	  uart_config.frame_length=0;
	  uart_config.stop_bits = 1U; /* As we don't use ASCII 7Bit stopbits will be one */
	  /* eParity */
	  switch(eParity){

		  case MB_PAR_NONE:{
			  uart_config.parity_mode = XMC_USIC_CH_PARITY_MODE_NONE;
		  } break;

		  case MB_PAR_EVEN:{
			  uart_config.parity_mode = XMC_USIC_CH_PARITY_MODE_EVEN;
		  } break;

		  case MB_PAR_ODD:{
			  uart_config.parity_mode = XMC_USIC_CH_PARITY_MODE_ODD;
		  } break;

		  default:{
			  uart_config.parity_mode = XMC_USIC_CH_PARITY_MODE_NONE;
		  } break;

	  }

	  uart_config.baudrate = ulBaudRate;
	  /* Configure UART channel */
	  XMC_UART_CH_Init(XMC_UART0_CH0, &uart_config);


	  /* Configure RX pin */
	  XMC_GPIO_CONFIG_t rx_pin_config;
	  rx_pin_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;
	  XMC_GPIO_Init(XMC_GPIO_PORT2,1, &rx_pin_config);
	  XMC_UART_CH_SetInputSource(XMC_UART0_CH0,XMC_UART_CH_INPUT_RXD,USIC0_C0_DX0_P2_1);
	  /* Configure TX pin */
	  XMC_GPIO_CONFIG_t tx_pin_config;
	  tx_pin_config.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT6;
	  XMC_GPIO_Init(XMC_GPIO_PORT2,0, &tx_pin_config);
	  /* Start UART channel */
	  XMC_UART_CH_Start(XMC_UART0_CH0);

	  XMC_UART_CH_EnableEvent(XMC_UART0_CH0,(XMC_UART_CH_EVENT_STANDARD_RECEIVE | XMC_UART_CH_EVENT_ALTERNATIVE_RECEIVE | XMC_UART_CH_EVENT_TRANSMIT_SHIFT | XMC_UART_CH_EVENT_TRANSMIT_BUFFER | XMC_UART_CH_STATUS_FLAG_TRANSMITTER_FRAME_FINISHED));

	  return_value=true;
  }

  return return_value;
}

BOOL
xMBPortSerialPutByte( CHAR ucByte )
{
  BOOL return_value=false;

  if (ptr_uart != 0)
  {
	  XMC_UART_CH_Transmit(ptr_uart,(uint8_t)ucByte);
	  //UART_TransmitWord(ptr_uart,(uint8_t)ucByte);
    return_value=true;
  }

  return return_value;
}
BOOL
xMBPortSerialGetByte( CHAR * pucByte )
{
  BOOL return_value=false;

  if (ptr_uart != 0)
  {
	  XMC_UART_CH_ClearStatusFlag(XMC_UART0_CH0, (XMC_UART_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION |
	                                     XMC_UART_CH_STATUS_FLAG_RECEIVE_INDICATION));
	  *pucByte=(int8_t)XMC_UART_CH_GetReceivedData(XMC_UART0_CH0);
      return_value=true;
  }

  return return_value;
}

void MB_RxHandler(void)
{
  (void)pxMBFrameCBByteReceived();
}
void MB_TxHandler(void)
{
  (void)pxMBFrameCBTransmitterEmpty();
}
#endif

