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
#include "mbconfig.h"

#if (MB_TCP_ENABLED > 0U)
#include <stdio.h>

#include "port.h"

/* ----------------------- lwIP includes ------------------------------------*/
#include "DAVE.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"


#ifndef  NETCONN_COPY
#define NETCONN_COPY 1
#endif

/* ----------------------- MBAP Header --------------------------------------*/
#define MB_TCP_UID          6
#define MB_TCP_LEN          4
#define MB_TCP_FUNC         7

/* ----------------------- Defines  -----------------------------------------*/
#define MB_TCP_BUF_SIZE     ( 256 + 7 ) /* Must hold a complete Modbus TCP frame. */

/* ----------------------- Prototypes ---------------------------------------*/
void vMBPortEventClose( void );
#ifdef MB_TCP_DEBUG
void vMBPortLog( eMBPortLogLevel eLevel, const CHAR * szModule,
                const CHAR * szFmt, ... );
#endif

/* ----------------------- Static variables ---------------------------------*/
static struct tcp_pcb *pxPCBListen;
static struct tcp_pcb *pxPCBClient;

static UCHAR    aucTCPBuf[MB_TCP_BUF_SIZE];
static USHORT   usTCPBufPos;

/* ----------------------- Static functions ---------------------------------*/
static err_t    prvxMBTCPPortAccept( void *pvArg, struct tcp_pcb *pxPCB, err_t xErr );
static err_t    prvxMBTCPPortReceive( void *pvArg, struct tcp_pcb *pxPCB, struct pbuf *p,
                                      err_t xErr );
static void     prvvMBTCPPortError( void *pvArg, err_t xErr );

/* ----------------------- Begin implementation -----------------------------*/
BOOL xMBTCPPortInit( USHORT usTCPPort )
{
  struct tcp_pcb *pxPCBListenNew, *pxPCBListenOld;
  BOOL            bOkay = false;
  USHORT          usPort;

  if( usTCPPort == 0 )
  {
    usPort = MB_TCP_DEFAULT_PORT;
  }
  else
  {
    usPort = ( USHORT ) usTCPPort;
  }

  if( ( pxPCBListenNew = pxPCBListenOld = tcp_new(  ) ) == NULL )
  {
    /* Can't create TCP socket. */
  bOkay = false;
  }
  else if( tcp_bind( pxPCBListenNew, IP_ADDR_ANY, ( u16_t ) usPort ) != ERR_OK )
  {
    /* Bind failed - Maybe illegal port value or in use. */
    ( void )tcp_close( pxPCBListenOld );
    bOkay = false;
  }
  else if( ( pxPCBListenNew = tcp_listen( pxPCBListenNew ) ) == NULL )
  {
    ( void )tcp_close( pxPCBListenOld );
    bOkay = false;
  }
  else
  {
    /* Register callback function for new clients. */
    tcp_accept( pxPCBListenNew, prvxMBTCPPortAccept );

    /* Everything okay. Set global variable. */
    pxPCBListen = pxPCBListenNew;

#ifdef MB_TCP_DEBUG
    vMBPortLog( MB_LOG_DEBUG, "MBTCP-ACCEPT", "Protocol stack ready.\r\n" );
#endif
  }
  bOkay = true;
  return bOkay;
}

void prvvMBPortReleaseClient( struct tcp_pcb *pxPCB )
{
  if( pxPCB != NULL )
  {
    if( tcp_close( pxPCB ) != ERR_OK )
    {
      tcp_abort( pxPCB );
    }
    if( pxPCB == pxPCBClient )
    {
#ifdef MB_TCP_DEBUG
      vMBPortLog( MB_LOG_DEBUG, "MBTCP-CLOSE", "Closed connection to %d.%d.%d.%d.\r\n",
                  ip4_addr1( &( pxPCB->remote_ip ) ),
                  ip4_addr2( &( pxPCB->remote_ip ) ),
                  ip4_addr3( &( pxPCB->remote_ip ) ), ip4_addr4( &( pxPCB->remote_ip ) ) );
#endif
      pxPCBClient = NULL;
    }
    if( pxPCB == pxPCBListen )
    {
      pxPCBListen = NULL;
    }
  }
}
void vMBTCPPortClose(  )
{
  /* Shutdown any open client sockets. */
  prvvMBPortReleaseClient( pxPCBClient );

  /* Shutdown or listening socket. */
  prvvMBPortReleaseClient( pxPCBListen );

  /* Release resources for the event queue. */
  vMBPortEventClose(  );
}

void vMBTCPPortDisable( void )
{
  prvvMBPortReleaseClient( pxPCBClient );
}

err_t prvxMBTCPPortAccept( void *pvArg, struct tcp_pcb *pxPCB, err_t xErr )
{
  err_t error;

  if( xErr != ERR_OK )
  {
    return xErr;
  }

  /* We can handle only one client. */
  if( pxPCBClient == NULL )
  {
    /* Register the client. */
    pxPCBClient = pxPCB;

    /* Set up the receive function prvxMBTCPPortReceive( ) to be called when data
     * arrives.
     */
    tcp_recv( pxPCB, prvxMBTCPPortReceive );

    /* Register error handler. */
    tcp_err( pxPCB, prvvMBTCPPortError );

    /* Set callback argument later used in the error handler. */
    tcp_arg( pxPCB, pxPCB );

    /* Reset the buffers and state variables. */
    usTCPBufPos = 0;

#ifdef MB_TCP_DEBUG
    vMBPortLog( MB_LOG_DEBUG, "MBTCP-ACCEPT", "Accepted new client %d.%d.%d.%d\r\n",
                ip4_addr1( &( pxPCB->remote_ip ) ),
                ip4_addr2( &( pxPCB->remote_ip ) ),
                ip4_addr3( &( pxPCB->remote_ip ) ), ip4_addr4( &( pxPCB->remote_ip ) ) );
#endif

    error = ERR_OK;
  }
  else
  {
    prvvMBPortReleaseClient( pxPCB );
    error = ERR_OK;
  }
  return error;
}

/* Called in case of an unrecoverable error. In any case we drop the client
 * connection. */
void prvvMBTCPPortError( void *pvArg, err_t xErr )
{
  struct tcp_pcb *pxPCB = pvArg;

  if( pxPCB != NULL )
  {
#ifdef MB_TCP_DEBUG
    vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR", "Error with client connection! Droping it.\r\n" );
#endif
    prvvMBPortReleaseClient( pxPCB );
  }
}

err_t prvxMBTCPPortReceive( void *pvArg, struct tcp_pcb *pxPCB, struct pbuf *p, err_t xErr )
{
  USHORT          usLength;
  err_t           error = ERR_OK;

  if( xErr != ERR_OK )
  {
    return xErr;
  }

  /* If pbuf is NULL then remote end has closed connection. */
  if( p == NULL )
  {
    prvvMBPortReleaseClient( pxPCB );
    return ERR_OK;
  }

  /* Acknowledge that we have received the data bytes. */
  tcp_recved( pxPCB, p->len );

  /* Check for internal buffer overflow. In case of an error drop the
   * client. */
  if( ( usTCPBufPos + p->len ) >= MB_TCP_BUF_SIZE )
  {
    prvvMBPortReleaseClient( pxPCB );
    error = ERR_OK;
  }
  else
  {
    memcpy( &aucTCPBuf[usTCPBufPos], p->payload, p->len );
    usTCPBufPos += p->len;

    /* If we have received the MBAP header we can analyze it and calculate
     * the number of bytes left to complete the current request. If complete
     * notify the protocol stack.
     */
    if( usTCPBufPos >= MB_TCP_FUNC )
    {
      /* Length is a byte count of Modbus PDU (function code + data) and the
       * unit identifier. */
      usLength = aucTCPBuf[MB_TCP_LEN] << 8U;
      usLength |= aucTCPBuf[MB_TCP_LEN + 1];

      /* Is the frame already complete. */
      if( usTCPBufPos < ( MB_TCP_UID + usLength ) )
      {
      }
      else if( usTCPBufPos == ( MB_TCP_UID + usLength ) )
      {
#ifdef MB_TCP_DEBUG
        prvvMBTCPLogFrame( "MBTCP-RECV", &aucTCPBuf[0], usTCPBufPos );
#endif
        (void)xMBPortEventPost( EV_FRAME_RECEIVED );
      }
      else
      {
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR",
                   "Received to many bytes! Droping client.\r\n" );
#endif
                /* This should not happen. We can't deal with such a client and
                 * drop the connection for security reasons.
                 */
         prvvMBPortReleaseClient( pxPCB );
         error = ERR_MEM;
      }
    }
  }
  pbuf_free( p );
  return error;
}

BOOL xMBTCPPortGetRequest( UCHAR ** ppucMBTCPFrame, USHORT * usTCPLength )
{
    *ppucMBTCPFrame = &aucTCPBuf[0];
    *usTCPLength = usTCPBufPos;

    /* Reset the buffer. */
    usTCPBufPos = 0;
    return true;
}

BOOL xMBTCPPortSendResponse( const UCHAR * pucMBTCPFrame, USHORT usTCPLength )
{
  BOOL bFrameSent = false;

  if( pxPCBClient )
  {
    /* Make sure we can send the packet. */
    assert( tcp_sndbuf( pxPCBClient ) >= usTCPLength );

    if( tcp_write( pxPCBClient, pucMBTCPFrame, ( u16_t ) usTCPLength, NETCONN_COPY ) == ERR_OK )
    {
#ifdef MB_TCP_DEBUG
      prvvMBTCPLogFrame( "MBTCP-SENT", &aucTCPBuf[0], usTCPLength );
#endif
      /* Make sure data gets sent immediately. */
      (void )tcp_output( pxPCBClient );
      bFrameSent = true;
    }
    else
    {
      /* Drop the connection in case of an write error. */
      prvvMBPortReleaseClient( pxPCBClient );
    }
  }
  return bFrameSent;
}
void   vMBPortEventClose( void )
{
}
#endif
