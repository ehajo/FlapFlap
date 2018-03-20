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
/**
* @file port.h
* @date 11 May,2016
* @version 1.0.0
*
* @brief Port layer defines and API function prototype definitions
*
* History
*
* 11 May 2016 Version 1.0
* Initial revision
*/

#ifndef _PORT_H
#define _PORT_H

/****************************************************************
* HEADER FILES
***************************************************************/

#include "port_config.h"

/* include for stdtypes */
#include <stdint.h>
/* include for the MCU Type UART for the XMC1100 */
#include "xmc_usic.h"
/****************************************************************
* MACROS AND DEFINES
***************************************************************/
#define	INLINE
#define PR_BEGIN_EXTERN_C extern "C" {
#define	PR_END_EXTERN_C }
#define ENTER_CRITICAL_SECTION( )	__disable_irq()
#define EXIT_CRITICAL_SECTION( )    __enable_irq()
#define assert(exp) ((void) 0)

#ifndef TRUE
#define TRUE     true
#endif

#ifndef FALSE
#define FALSE    false
#endif

/****************************************************************
* DATA STRUCTURES
***************************************************************/
typedef uint8_t  BOOL;
typedef uint8_t  UCHAR ;
typedef int8_t   CHAR;
typedef uint16_t USHORT;
typedef int16_t  SHORT;
typedef uint32_t ULONG;
typedef int32_t  LONG;

/****************************************************************
* API PROTOTYPES
***************************************************************/
#if ((MB_RTU_ENABLED|MB_ASCII_ENABLED) > 0U)

/**
* @brief Function to register UART interface to be used by modbus.
* Must be called before modbus is enabled.
* @param *uart pointer to structure defining the uart interface
* @retval none
*
* <b>Details of function</b><br>
* Function to register UART interface to be used by modbus.
*
* <b>Example Usage:</b>
* @code
* UART_t uart;
* MB_register_UART(&uart);
* @endcode
*/
void MB_register_UART(XMC_USIC_CH_t *USIC);

/**
* @brief Function called when new data is received on UART.
* @param none
* @retval none
*
* <b>Details of function</b><br>
* Function called when new data is received on UART.
*
* <b>Example Usage:</b>
* @code
* MB_RxHandler();
* @endcode
*/
void MB_RxHandler(void);

/**
* @brief Function called when new data was transmitted on UART.
* @param none
* @retval none
*
* <b>Details of function</b><br>
* Function called when new data was transmitted on UART.
*
* <b>Example Usage:</b>
* @code
* MB_TxHandler();
* @endcode
*/
void MB_TxHandler(void);

/**
* @brief Function called to disable interrupts before critical section is entered.
* @param none
* @retval none
*
* <b>Details of function</b><br>
* Function called to disable interrupt before criticla sectionis entered.
*
* <b>Example Usage:</b>
* @code
* EnterCriticalSection();
* @endcode
*/
void EnterCriticalSection( void );

/**
* @brief Function called to enable interrupts after critical section is eexit.
* @param none
* @retval none
*
* <b>Details of function</b><br>
* Function called to disable interrupt before criticla sectionis entered.
*
* <b>Example Usage:</b>
* @code
* EnterCriticalSection();
* @endcode
*/void ExitCriticalSection( void );

#endif
#endif
