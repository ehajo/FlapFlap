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
#include "port.h"
#include "mbutils.h"
#include "mb.h"
#include "GPIO.h"
/****************************************************************
* MACROS AND DEFINES
***************************************************************/
/* Input Register Definition (16 bit; Read-Only) */
/* Input Register Start Address */
#define REG_INPUT_START_ADDR 1U
/* No of Input Registers*/
#define REG_INPUT_COUNT 4U

/* Holding Register Definition (16 bit; Read-Write) */
/* Holding Register Start Address */
#define REG_HOLDING_START_ADDR 0U
/* No of Holding Registers */
#define REG_HOLDING_COUNT 4U

/* Coil Register Definition (1 bit; Read-Write) */
/* Coil Register Start Address */
#define REG_COILS_START_ADDR 1000U
/* No of Coil Registers*/
#define REG_COILS_COUNT 8U

/* Discrete Inputs Definition (1 bit; Read-Only) */
/* Discrete Inputs Start Address */
#define REG_DISC_START_ADDR 2000U
/* No of Discrete Inputs */
#define REG_DISC_COUNT 8U

/****************************************************************
* LOCAL DATA
***************************************************************/
/* Allocate buffer for Input Register (16bit; Read-Only) */
static uint16_t reg_input_buffer[REG_INPUT_COUNT];
/* Allocate buffer for Holding Register (16bit; Read-Write) */
static volatile uint16_t reg_holding_buffer[REG_HOLDING_COUNT];
/* Allocate buffer for discrete input register (1bit; Read-Only) */
static uint8_t reg_discrete_input_buffer[ (((uint16_t)REG_DISC_COUNT-1U) / 8U) + 1U];
/* Allocate buffer for coil register (1bit; Read-Write) */
static uint8_t reg_coils_buffers[ (((uint16_t)REG_COILS_COUNT-1U) / 8U) + 1U];

/****************************************************************
* API PROTOTYPES
***************************************************************/
void Rx_Cb(void);
void Tx_Cb(void);

typedef enum {
	FF_PIN_ADC=0,
	FF_PIN_LINE,
	FF_PIN_START
}FF_CNTRL_PIN_t;

/* Shall be called every 1ms from somewhere */
typedef enum {
	FF_FSM_IDLE,
	FF_FSM_START_POS_READ,
	FF_FSM_POS_REG_COMP,
	FF_FSM_START_MOVE,
	FF_FSM_START_MOVE_WAIT_START,
	FF_FSM_START_STOP,
	FF_FSM_TOGGLE_START,
}FF_FSM_STATE_t;



void clr_pin(FF_CNTRL_PIN_t pin);
void set_pin(FF_CNTRL_PIN_t pin);

/****************************************************************
* API IMPLEMENTATION
***************************************************************/
/**
 * @eMBRegInputCB
 *
 * Callback function called by the protocol stack for reading the 16bit value of input
 * register(s)
 *
 * @input :  - buffer  : Pointer to a buffer which is used to return the
 *                        current value of the modbus input registers to the stack.
 *           - address : Starting address of the registers.
 *           - count   : Number of registers to be returned.
 *
 * @output : - buffer  : Buffer which is updated with the modbus input registers.
 *
 * @return : MB_ENOERR if success
 *           MB_ENOREG if failure (illegal register access)
 *
 * */
eMBErrorCode eMBRegInputCB( uint8_t *buffer, uint16_t address, uint16_t count )
{
  eMBErrorCode status = MB_ENOERR;
  uint16_t     register_index;

  if (( address >= (uint16_t)REG_INPUT_START_ADDR )
     && ( (uint16_t)(address + count) <= (uint16_t)(REG_INPUT_START_ADDR + REG_INPUT_COUNT) ))
  {
    register_index = ( uint16_t )( address - REG_INPUT_START_ADDR );
    while( count > 0U )
    {
      /* Pass current register values to the protocol stack. */
      *buffer = ( uint8_t )( reg_input_buffer[register_index] >> 8 );
      buffer++;
      *buffer = ( uint8_t )( reg_input_buffer[register_index] & 0xFFU );
      buffer++;
      register_index++;
      count--;
    }
  }
  else
  {
    status = MB_ENOREG;
  }
  return status;
}

/**
 * @eMBRegHoldingCB
 *
 * Callback function called by the protocol stack for reading/writing the 16bit value of
 * holding register(s)
 *
 * @input :  - buffer  : Pointer to a buffer which is used to exchange (read/write)
 *                       current value of the modbus holding registers with the protocol stack.
 *           - address : Starting address of the register(s).
 *           - count   : Number of registers to be exchanged (read/write).
 *           - mode    : MB_REG_WRITE Protocol stack is writing input register(s).
 *                       MB_REG_READ  Protocol stack is reading input register(s).
 *
 * @output : - buffer  : Buffer which is updated with the modbus holding registers.
 *
 * @return:  MB_ENOERR if success
 *           MB_ENOREG if failure (illegal register access)
 *
 * */
eMBErrorCode eMBRegHoldingCB( uint8_t *buffer, uint16_t address, uint16_t count, eMBRegisterMode mode )
{
  eMBErrorCode status = MB_ENOERR;
  uint16_t register_index;

  if ( ( address >= REG_HOLDING_START_ADDR ) &&
     ( (uint16_t)(address + count) <= (uint16_t)(REG_HOLDING_START_ADDR + REG_HOLDING_COUNT)) )
  {
    register_index = ( uint16_t )( address - REG_HOLDING_START_ADDR );
    switch ( mode )
    {
      /* Pass current register values to the protocol stack. */
      case MB_REG_READ:
    	if(register_index>0){
    		register_index--;
    	}
        while( count > 0U )
        {
          *buffer = (uint8_t)( reg_holding_buffer[register_index] >> 8 );
          buffer++;
          *buffer = (uint8_t)( reg_holding_buffer[register_index] & 0xFFU );
          buffer++;
          register_index++;
          count--;
        }
        break;
      /* Update current register values with new values from the
       * protocol stack. */
      case MB_REG_WRITE:
        while( count > 0U )
        {
          if(register_index>0){
        	  register_index--;
          }
          reg_holding_buffer[register_index] = (uint16_t)((uint16_t)*buffer << 8);
          buffer++;
          reg_holding_buffer[register_index] |= *buffer;
          buffer++;
          register_index++;
          count--;
        }
        break;
      default:
        status = MB_ENOREG;
        break;
    }
  }
  else
  {
      status = MB_ENOREG;
  }
  return status;
}

/**
 * @eMBRegCoilsCB
 *
 * Callback function called by the protocol stack for reading/writing the 1bit value of coil register(s)
 *
 * @input :  - buffer  : Pointer to a buffer which is used to exchange (read/write)
 *                       current value of the modbus coil regitser(s) with the protocol stack.
 *           - address : Starting address of the register(s).
 *           - count   : Number of registers bits to be exchanged (read/write).
 *           - mode    : MB_REG_WRITE Protocol stack is writing coil register(s).
 *                       MB_REG_READ  Protocol stack is reading coil register(s).
 *
 * @output : - buffer  : Buffer which is updated with the modbus coil register(s).
 *
 * @return:  MB_ENOERR if success
 *           MB_ENOREG if failure (illegal register access)
 *
 * */
eMBErrorCode eMBRegCoilsCB( uint8_t *buffer, uint16_t address, uint16_t count, eMBRegisterMode mode )
{
  eMBErrorCode status = MB_ENOERR;
  int16_t signed_count = (int16_t)count;
  uint16_t bit_offset;

  /* Check if we have registers mapped at this block. */
  if( ( address >= REG_COILS_START_ADDR ) &&
      ( (uint16_t)(address + count) <= (uint16_t)(REG_COILS_START_ADDR + REG_COILS_COUNT)) )
  {
    bit_offset = ( uint16_t )( address - REG_COILS_START_ADDR );
    switch ( mode )
    {
      /* Read current values and pass to protocol stack. */
      case MB_REG_READ:
        while( signed_count > 0 )
        {
          if (signed_count > 8)
          {
            *buffer = xMBUtilGetBits( reg_coils_buffers, bit_offset, 8U);
          }
          else
          {
            *buffer = xMBUtilGetBits( reg_coils_buffers, bit_offset, (uint8_t)signed_count);
          }
          buffer++;
          signed_count -= 8;
          bit_offset += 8U;
        }
        break;
      /* Update current register values. */
      case MB_REG_WRITE:
        while( signed_count > 0 )
        {
          if (signed_count > 8)
          {
            xMBUtilSetBits( reg_coils_buffers, bit_offset, 8U, *buffer );
          }
          else
          {
            xMBUtilSetBits( reg_coils_buffers, bit_offset, (uint8_t)signed_count, *buffer );
          }
          buffer++;
          signed_count -= 8;
          bit_offset += (uint8_t)8;
        }
        break;
      default:
        status = MB_ENOREG;
        break;
    }
  }
  else
  {
    status = MB_ENOREG;
  }
  return status;
}

/**
 * @eMBRegDiscreteCB
 *
 * Callback function called by the protocol stack for reading the 1bit value of discrete register(s)
 *
 * @input :  - buffer  : Pointer to a buffer which is used to return the
 *                       current value of the modbus discrete registers to the stack.
 *           - address : Starting address of the registers.
 *           - count   : Number of register bits to be returned.
 *
 * @output : - buffer  : Buffer which is updated with the modbus discrete registers.
 *
 * @return : MB_ENOERR if success
 *           MB_ENOREG if failure (illegal register access)
 *
 * */
eMBErrorCode eMBRegDiscreteCB( uint8_t *buffer, uint16_t address, uint16_t count )
{
  eMBErrorCode status = MB_ENOERR;
  int16_t signed_count = (int16_t)count;
  uint16_t bit_offset;

  /* Check if we have registers mapped at this block. */
  if( ( address >= REG_DISC_START_ADDR ) &&
    ( (uint16_t)(address + count) <= (uint16_t)(REG_DISC_START_ADDR + REG_DISC_COUNT)) )
  {
    bit_offset = (uint16_t)( address - REG_DISC_START_ADDR );
    while( signed_count > 0 )
    {
      if (signed_count > 8)
      {
         *buffer = xMBUtilGetBits( reg_discrete_input_buffer, bit_offset, 8U );
      }
      else
      {
         *buffer = xMBUtilGetBits( reg_discrete_input_buffer, bit_offset, (uint8_t)signed_count);
      }
      buffer++;
      signed_count -= 8;
      bit_offset += (uint8_t)8;
    }
  }
  else
  {
    status = MB_ENOREG;
  }
  return status;
}

void mb_stack_task (void){
/* May every 8.6uS there be a new char */
/* Process modbus protocol stack */
(void)eMBPoll();
/* All other housekeeping regarding modbus here */

 	
}

/* Callback handler of UART receiving */
void Rx_Cb(void)
{
  MB_RxHandler();
}

/* Callback handler of UART transmitting */
void Tx_Cb(void)
{
  MB_TxHandler();
}

/* FF Control */



void ff_setup() {


  set_pin(FF_PIN_START);
  set_pin(FF_PIN_LINE);
  set_pin(FF_PIN_ADC);

}

void ff_check_position(){

/* Modified to a Statemachine to keep other tasks running */

  static FF_FSM_STATE_t FSM_STATE = FF_FSM_IDLE;
  static uint32_t u32Delay=0;
  uint16_t position = 0;


  switch(FSM_STATE){

  case FF_FSM_IDLE:{
	  FSM_STATE=FF_FSM_START_POS_READ;
	  u32Delay=0;
  }break;

  case FF_FSM_START_POS_READ:{
   clr_pin(FF_PIN_ADC);
   clr_pin(FF_PIN_LINE);
   if(u32Delay>0){
	   u32Delay--;
   } else {
   FSM_STATE=FF_FSM_POS_REG_COMP;
   }
  }break;

  case FF_FSM_POS_REG_COMP:{
   /* This can be optimised by reading the port register */
   reg_holding_buffer[3] = (uint16_t)(PORT1->IN & 0x0000003F);
   position =reg_holding_buffer[3]- 1;
   reg_holding_buffer[1]=position & 0x0000003F;
  	   if((reg_holding_buffer[1] & 0x003F) != (reg_holding_buffer[0]&0x0003F) ){
		   /* We need to move on step and check again */
		   FSM_STATE = FF_FSM_START_MOVE;
	   } else {
		   FSM_STATE = FF_FSM_IDLE;
	   }
  	set_pin(FF_PIN_ADC);
    set_pin(FF_PIN_LINE);
  }break;

  case FF_FSM_START_MOVE:{
  /* Compare Position with register */

	  clr_pin(FF_PIN_START);
	  clr_pin(FF_PIN_LINE);
	  clr_pin(FF_PIN_ADC);
	  u32Delay=100;
	  FSM_STATE = FF_FSM_START_MOVE_WAIT_START;
  } break;

  case FF_FSM_START_MOVE_WAIT_START:{
	  if(u32Delay<=0){
		  set_pin(FF_PIN_LINE);
		  set_pin(FF_PIN_ADC);
		  u32Delay=30;
		  /* Next State */
		  FSM_STATE = FF_FSM_START_STOP;
	  } else {
		  u32Delay--;
		  FSM_STATE = FF_FSM_START_MOVE_WAIT_START;
	  }

  }break;

  case FF_FSM_START_STOP:{
	  if(u32Delay<=0){
		  set_pin(FF_PIN_START);
		  u32Delay=125;
		  /* Next State */
		  FSM_STATE = FF_FSM_TOGGLE_START;
	  } else {
		  u32Delay--;
		  FSM_STATE = FF_FSM_START_STOP;

	  }

  }break;

  case FF_FSM_TOGGLE_START:{
	  /* Toggle FF_PIN_ADC */
	  /* This will be done for 500ms ( 1000 * 0.5ms) */
	  if(u32Delay>0){
		  if(u32Delay%2==0){
			  clr_pin(FF_PIN_ADC);
		  } else {
			  set_pin(FF_PIN_ADC);
		  }
		  u32Delay--;
		  FSM_STATE = FF_FSM_TOGGLE_START;
	  }else{
	   /* Change State */
	     FSM_STATE = FF_FSM_IDLE;
	  /* Move to idle */
	  }
  /* May the FF Display stops now */
  }break;

  default:{
	FSM_STATE = FF_FSM_IDLE;
  }
 }

}




void clr_pin(FF_CNTRL_PIN_t pin)
{
  switch(pin){
	case FF_PIN_ADC:{
		P0_13_reset(); /* !AD    */
	} break;

	case FF_PIN_LINE:{
		P0_1_reset(); /* !COL   */
	}break;

	case FF_PIN_START:{
		P0_2_reset(); /* !Start */
	} break;

	default:{
		/* Unsuppoted pin */
	} break;
  }

}

void set_pin(FF_CNTRL_PIN_t pin)
{
	switch(pin){
		case FF_PIN_ADC:{
			P0_13_set(); /* !AD    */
		} break;

		case FF_PIN_LINE:{
			P0_1_set(); /* !COL   */
		}break;

		case FF_PIN_START:{
			P0_2_set(); /* !Start */
		} break;

		default:{
			/* Unsuppoted pin */
		} break;
	}
}

void voManualSetPosition(uint8_t Position){
	reg_holding_buffer[0] = Position & 0x003F;
}
void voService(void){
	clr_pin(FF_PIN_ADC);
	clr_pin(FF_PIN_LINE);
	clr_pin(FF_PIN_START);
}
