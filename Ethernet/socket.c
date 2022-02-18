//*****************************************************************************
//
//! \file socket.c
//! \brief SOCKET APIs Implements file.
//! \details SOCKET APIs like as Berkeley Socket APIs. 
//! \version 1.0.3
//! \date 2013/10/21
//! \par  Revision history
//!       <2015/02/05> Notice
//!        The version history is not updated after this point.
//!        Download the latest version directly from GitHub. Please visit the our GitHub repository for ioLibrary.
//!        >> https://github.com/Wiznet/ioLibrary_Driver
//!       <2014/05/01> V1.0.3. Refer to M20140501
//!         1. Implicit type casting -> Explicit type casting.
//!         2. replace 0x01 with PACK_REMAINED in recvfrom()
//!         3. Validation a destination ip in connect() & sendto(): 
//!            It occurs a fatal error on converting unint32 address if uint8* addr parameter is not aligned by 4byte address.
//!            Copy 4 byte addr value into temporary uint32 variable and then compares it.
//!       <2013/12/20> V1.0.2 Refer to M20131220
//!                    Remove Warning.
//!       <2013/11/04> V1.0.1 2nd Release. Refer to "20131104".
//!                    In sendto(), Add to clear timeout interrupt status (Sn_IR_TIMEOUT)
//!       <2013/10/21> 1st Release
//! \author MidnightCow
//! \copyright
//!
//! Copyright (c)  2013, WIZnet Co., LTD.
//! All rights reserved.
//! 
//! Redistribution and use in source and binary forms, with or without 
//! modification, are permitted provided that the following conditions 
//! are met: 
//! 
//!     * Redistributions of source code must retain the above copyright 
//! notice, this list of conditions and the following disclaimer. 
//!     * Redistributions in binary form must reproduce the above copyright
//! notice, this list of conditions and the following disclaimer in the
//! documentation and/or other materials provided with the distribution. 
//!     * Neither the name of the <ORGANIZATION> nor the names of its 
//! contributors may be used to endorse or promote products derived 
//! from this software without specific prior written permission. 
//! 
//! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
//! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
//! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
//! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
//! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
//! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
//! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
//! THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************
#include "socket.h"

//M20150401 : Typing Error
//#define SOCK_ANY_PORT_NUM  0xC000;
#define SOCK_ANY_PORT_NUM  0xC000

static uint16_t sock_any_port = SOCK_ANY_PORT_NUM;
static uint16_t sock_io_mode = 0;
static uint16_t sock_is_sending = 0;

static uint16_t sock_remained_size[_WIZCHIP_SOCK_NUM_] = {0,0,};

//M20150601 : For extern decleation
//static uint8_t  sock_pack_info[_WIZCHIP_SOCK_NUM_] = {0,};
uint8_t  sock_pack_info[_WIZCHIP_SOCK_NUM_] = {0,};
//

#if _WIZCHIP_ == 5200
   static uint16_t sock_next_rd[_WIZCHIP_SOCK_NUM_] ={0,};
#endif

//A20150601 : For integrating with W5300
#if _WIZCHIP_ == 5300
   uint8_t sock_remained_byte[_WIZCHIP_SOCK_NUM_] = {0,}; // set by wiz_recv_data()
#endif


#define CHECK_SOCKNUM()   \
   do{                    \
      if(sn > _WIZCHIP_SOCK_NUM_) return SOCKERR_SOCKNUM;   \
   }while(0);             \

#define CHECK_SOCKMODE(mode, flag_spi)  \
   do{                     \
      if((getSn_MR(sn, flag_spi) & 0x0F) != mode) return SOCKERR_SOCKMODE;  \
   }while(0);              \

#define CHECK_SOCKINIT(flag_spi)   \
   do{                     \
      if((getSn_SR(sn, flag_spi) != SOCK_INIT)) return SOCKERR_SOCKINIT; \
   }while(0);              \

#define CHECK_SOCKDATA()   \
   do{                     \
      if(len == 0) return SOCKERR_DATALEN;   \
   }while(0);              \



int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag, uint8_t flag_spi)
{
	CHECK_SOCKNUM();
	switch(protocol)
	{
      case Sn_MR_TCP :
         {
            //M20150601 : Fixed the warning - taddr will never be NULL
		    /*
            uint8_t taddr[4];
            getSIPR(taddr);
            */
            uint32_t taddr;
            getSIPR((uint8_t*)&taddr, flag_spi);
            if(taddr == 0) return SOCKERR_SOCKINIT;
         }
      case Sn_MR_UDP :
      case Sn_MR_MACRAW :
	  case Sn_MR_IPRAW :
         break;
   #if ( _WIZCHIP_ < 5200 )
      case Sn_MR_PPPoE :
         break;
   #endif
      default :
         return SOCKERR_SOCKMODE;
	}
	//M20150601 : For SF_TCP_ALIGN & W5300
	//if((flag & 0x06) != 0) return SOCKERR_SOCKFLAG;
	if((flag & 0x04) != 0) return SOCKERR_SOCKFLAG;
#if _WIZCHIP_ == 5200
   if(flag & 0x10) return SOCKERR_SOCKFLAG;
#endif
	   
	if(flag != 0)
	{
   	switch(protocol)
   	{
   	   case Sn_MR_TCP:
   		  //M20150601 :  For SF_TCP_ALIGN & W5300
          #if _WIZCHIP_ == 5300
   		     if((flag & (SF_TCP_NODELAY|SF_IO_NONBLOCK|SF_TCP_ALIGN))==0) return SOCKERR_SOCKFLAG;
          #else
   		     if((flag & (SF_TCP_NODELAY|SF_IO_NONBLOCK))==0) return SOCKERR_SOCKFLAG;
          #endif

   	      break;
   	   case Sn_MR_UDP:
   	      if(flag & SF_IGMP_VER2)
   	      {
   	         if((flag & SF_MULTI_ENABLE)==0) return SOCKERR_SOCKFLAG;
   	      }
   	      #if _WIZCHIP_ == 5500
      	      if(flag & SF_UNI_BLOCK)
      	      {
      	         if((flag & SF_MULTI_ENABLE) == 0) return SOCKERR_SOCKFLAG;
      	      }
   	      #endif
   	      break;
   	   default:
   	      break;
   	}
   }
	close(sn, flag_spi);
	//M20150601
	#if _WIZCHIP_ == 5300
	   setSn_MR(sn, ((uint16_t)(protocol | (flag & 0xF0))) | (((uint16_t)(flag & 0x02)) << 7), flag_spi );
    #else
	   setSn_MR(sn, (protocol | (flag & 0xF0)), flag_spi);
    #endif
	if(!port)
	{
	   port = sock_any_port++;
	   if(sock_any_port == 0xFFF0) sock_any_port = SOCK_ANY_PORT_NUM;
	}
   setSn_PORT(sn,port, flag_spi);
   setSn_CR(sn,Sn_CR_OPEN, flag_spi);
   while(getSn_CR(sn,flag_spi));
   //A20150401 : For release the previous sock_io_mode
   sock_io_mode &= ~(1 <<sn);

	sock_io_mode |= ((flag & SF_IO_NONBLOCK) << sn);   
   sock_is_sending &= ~(1<<sn);
   sock_remained_size[sn] = 0;
   //M20150601 : repalce 0 with PACK_COMPLETED
   //sock_pack_info[sn] = 0;
   sock_pack_info[sn] = PACK_COMPLETED;
   //
   while(getSn_SR(sn, flag_spi) == SOCK_CLOSED);
   return (int8_t)sn;
}	   

int8_t close(uint8_t sn, uint8_t flag_spi)
{
	CHECK_SOCKNUM();
//A20160426 : Applied the erratum 1 of W5300
#if   (_WIZCHIP_ == 5300) 
   //M20160503 : Wrong socket parameter. s -> sn 
   //if( ((getSn_MR(s)& 0x0F) == Sn_MR_TCP) && (getSn_TX_FSR(s) != getSn_TxMAX(s)) ) 
   if( ((getSn_MR(sn, flag_spi)& 0x0F) == Sn_MR_TCP) && (getSn_TX_FSR(sn, flag_spi) != getSn_TxMAX(sn, flag_spi)) )
   { 
      uint8_t destip[4] = {0, 0, 0, 1};
      // TODO
      // You can wait for completing to sending data;
      // wait about 1 second;
      // if you have completed to send data, skip the code of erratum 1
      // ex> wait_1s();
      //     if (getSn_TX_FSR(s) == getSn_TxMAX(s)) continue;
      // 
      //M20160503 : The socket() of close() calls close() itself again. It occures a infinite loop - close()->socket()->close()->socket()-> ~
      //socket(s,Sn_MR_UDP,0x3000,0);
      //sendto(s,destip,1,destip,0x3000); // send the dummy data to an unknown destination(0.0.0.1).
      setSn_MR(sn,Sn_MR_UDP,flag_spi);
      setSn_PORTR(sn, 0x3000,flag_spi);
      setSn_CR(sn,Sn_CR_OPEN,flag_spi);
      while(getSn_CR(sn,flag_spi) != 0);
      while(getSn_SR(sn,flag_spi) != SOCK_UDP);
      sendto(sn,destip,1,destip,0x3000); // send the dummy data to an unknown destination(0.0.0.1).
   };   
#endif 
	setSn_CR(sn,Sn_CR_CLOSE,flag_spi);
   /* wait to process the command... */
	while( getSn_CR(sn,flag_spi) );
	/* clear all interrupt of the socket. */
	setSn_IR(sn, 0xFF,flag_spi);
	//A20150401 : Release the sock_io_mode of socket n.
	sock_io_mode &= ~(1<<sn);
	//
	sock_is_sending &= ~(1<<sn);
	sock_remained_size[sn] = 0;
	sock_pack_info[sn] = 0;
	while(getSn_SR(sn,flag_spi) != SOCK_CLOSED);
	return SOCK_OK;
}

int8_t listen(uint8_t sn, uint8_t flag_spi)
{
	CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
	CHECK_SOCKINIT(flag_spi);
	setSn_CR(sn,Sn_CR_LISTEN,flag_spi);
	while(getSn_CR(sn,flag_spi));
   while(getSn_SR(sn,flag_spi) != SOCK_LISTEN)
   {
         close(sn,flag_spi);
         return SOCKERR_SOCKCLOSED;
   }
   return SOCK_OK;
}


int8_t connect(uint8_t sn, uint8_t * addr, uint16_t port, uint8_t flag_spi)
{
   CHECK_SOCKNUM();
   CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
   CHECK_SOCKINIT(flag_spi);
   //M20140501 : For avoiding fatal error on memory align mismatched
   if( *((uint32_t*)addr) == 0xFFFFFFFF || *((uint32_t*)addr) == 0) return SOCKERR_IPINVALID;
   {
      uint32_t taddr;
      taddr = ((uint32_t)addr[0] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[1] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[2] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[3] & 0x000000FF);
      if( taddr == 0xFFFFFFFF || taddr == 0) return SOCKERR_IPINVALID;
   }
   //
	
	if(port == 0) return SOCKERR_PORTZERO;
	setSn_DIPR(sn,addr, flag_spi);
	setSn_DPORT(sn,port,flag_spi);
	setSn_CR(sn,Sn_CR_CONNECT,flag_spi);
   while(getSn_CR(sn, flag_spi));
   if(sock_io_mode & (1<<sn)) return SOCK_BUSY;
   while(getSn_SR(sn,flag_spi) != SOCK_ESTABLISHED)
   {
		if (getSn_IR(sn,flag_spi) & Sn_IR_TIMEOUT)
		{
			setSn_IR(sn, Sn_IR_TIMEOUT,flag_spi);
            return SOCKERR_TIMEOUT;
		}

		if (getSn_SR(sn, flag_spi) == SOCK_CLOSED)
		{
			return SOCKERR_SOCKCLOSED;
		}
	}
   
   return SOCK_OK;
}

int8_t disconnect(uint8_t sn, uint8_t flag_spi)
{
   CHECK_SOCKNUM();
   CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
   setSn_CR(sn,Sn_CR_DISCON,flag_spi);
	/* wait to process the command... */
	while(getSn_CR(sn, flag_spi));
	sock_is_sending &= ~(1<<sn);
   if(sock_io_mode & (1<<sn)) return SOCK_BUSY;
	while(getSn_SR(sn,flag_spi) != SOCK_CLOSED)
	{
	   if(getSn_IR(sn,flag_spi) & Sn_IR_TIMEOUT)
	   {
	      close(sn,flag_spi);
	      return SOCKERR_TIMEOUT;
	   }
	}
	return SOCK_OK;
}

int32_t send(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t flag_spi)
{
   uint8_t tmp=0;
   uint16_t freesize=0;
   
   CHECK_SOCKNUM();
   CHECK_SOCKMODE(Sn_MR_TCP,flag_spi);
   CHECK_SOCKDATA();

   tmp = getSn_SR(sn,flag_spi); //������ ��������� ������
   if(tmp != SOCK_ESTABLISHED && tmp != SOCK_CLOSE_WAIT) return SOCKERR_SOCKSTATUS;
//   if( sock_is_sending & (1<<sn) )
//   {
//      tmp = getSn_IR(sn,flag_spi);
//      if(tmp & Sn_IR_SENDOK)
//      {
//         setSn_IR(sn, Sn_IR_SENDOK,flag_spi);
//         //M20150401 : Typing Error
//         //#if _WZICHIP_ == 5200
//
//         sock_is_sending &= ~(1<<sn);
//      }
//      else if(tmp & Sn_IR_TIMEOUT)
//      {
//         close(sn,flag_spi);
//         return SOCKERR_TIMEOUT;
//      }
//      else return SOCK_BUSY;
//   }
   freesize = getSn_TxMAX(sn,flag_spi);
   if (len > freesize) len = freesize; // check size not to exceed MAX size.
   while(1)
   {
      freesize = getSn_TX_FSR(sn,flag_spi);
      tmp = getSn_SR(sn,flag_spi);
      if ((tmp != SOCK_ESTABLISHED) && (tmp != SOCK_CLOSE_WAIT))
      {
         close(sn,flag_spi);
         return SOCKERR_SOCKSTATUS;
      }
      if( (sock_io_mode & (1<<sn)) && (len > freesize) ) return SOCK_BUSY;
      if(len <= freesize) break;
   }

   wiz_send_data(sn, buf, len, flag_spi);
   #if _WIZCHIP_ == 5200
      sock_next_rd[sn] = getSn_TX_RD(sn) + len;
   #endif

   #if _WIZCHIP_ == 5300
      setSn_TX_WRSR(sn,len, flag_spi);
   #endif
   
   setSn_CR(sn,Sn_CR_SEND,flag_spi);
   /* wait to process the command... */
   while(getSn_CR(sn,flag_spi));
   //sock_is_sending |= (1 << sn);
   //M20150409 : Explicit Type Casting
   //return len;
   return (int32_t)len;
}

int32_t send_dma(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t flag_spi)
{
   uint8_t tmp=0;
   uint16_t freesize=0;

   CHECK_SOCKNUM();
   CHECK_SOCKMODE(Sn_MR_TCP,flag_spi);
   CHECK_SOCKDATA();

   tmp = getSn_SR(sn,flag_spi); //������ ��������� ������
   if(tmp != SOCK_ESTABLISHED && tmp != SOCK_CLOSE_WAIT) return SOCKERR_SOCKSTATUS;

   freesize = getSn_TxMAX(sn,flag_spi);
   if (len > freesize) len = freesize; // check size not to exceed MAX size.
   while(1)
   {
      freesize = getSn_TX_FSR(sn,flag_spi);
      tmp = getSn_SR(sn,flag_spi);
      if ((tmp != SOCK_ESTABLISHED) && (tmp != SOCK_CLOSE_WAIT))
      {
         close(sn,flag_spi);
         return SOCKERR_SOCKSTATUS;
      }
      if( (sock_io_mode & (1<<sn)) && (len > freesize) ) return SOCK_BUSY;
      if(len <= freesize) break;
   }

   wiz_send_data_dma(sn, buf, len, flag_spi);
   setSn_CR(sn,Sn_CR_SEND,flag_spi);
   /* wait to process the command... */
   while(getSn_CR(sn,flag_spi));
   /* Send len + HANDSHAKE */
   return (int32_t)len;
}

int32_t recv(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t flag_spi)
{
   uint8_t  tmp = 0;
   uint16_t recvsize = 0;
//A20150601 : For integarating with W5300
#if   _WIZCHIP_ == 5300
   uint8_t head[2];
   uint16_t mr;
#endif
//
   CHECK_SOCKNUM();
   CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
   CHECK_SOCKDATA();
   
   recvsize = getSn_RxMAX(sn, flag_spi);
   if(recvsize < len) len = recvsize;
      
//A20150601 : For Integrating with W5300
#if _WIZCHIP_ == 5300
   //sock_pack_info[sn] = PACK_COMPLETED;    // for clear      
   if(sock_remained_size[sn] == 0)
   {
#endif
//
      while(1)
      {
         recvsize = getSn_RX_RSR(sn,flag_spi);
         tmp = getSn_SR(sn, flag_spi);
         if (tmp != SOCK_ESTABLISHED)
         {
            if(tmp == SOCK_CLOSE_WAIT)
            {
               if(recvsize != 0) break;
               else if(getSn_TX_FSR(sn, flag_spi) == getSn_TxMAX(sn,flag_spi))
               {
                  close(sn, flag_spi);
                  return SOCKERR_SOCKSTATUS;
               }
            }
            else
            {
               close(sn, flag_spi);
               return SOCKERR_SOCKSTATUS;
            }
         }
         if((sock_io_mode & (1<<sn)) && (recvsize == 0)) return SOCK_BUSY;
         if(recvsize != 0) break;
      };
#if _WIZCHIP_ == 5300
   }
#endif

//A20150601 : For integrating with W5300
#if _WIZCHIP_ == 5300
   if((sock_remained_size[sn] == 0) || (getSn_MR(sn) & Sn_MR_ALIGN))
   {
      mr = getMR();
      if((getSn_MR(sn,flag_spi) & Sn_MR_ALIGN)==0)
      {
         wiz_recv_data(sn,head,2,flag_spi);
         if(mr & MR_FS)
            recvsize = (((uint16_t)head[1]) << 8) | ((uint16_t)head[0]);
         else
            recvsize = (((uint16_t)head[0]) << 8) | ((uint16_t)head[1]);
         sock_pack_info[sn] = PACK_FIRST;
      }
      sock_remained_size[sn] = recvsize;
   }
   if(len > sock_remained_size[sn]) len = sock_remained_size[sn];
   recvsize = len;   
   if(sock_pack_info[sn] & PACK_FIFOBYTE)
   {
      *buf = sock_remained_byte[sn];
      buf++;
      sock_pack_info[sn] &= ~(PACK_FIFOBYTE);
      recvsize -= 1;
      sock_remained_size[sn] -= 1;
   }
   if(recvsize != 0)
   {
      wiz_recv_data(sn, buf, recvsize,flag_spi);
      setSn_CR(sn,Sn_CR_RECV,flag_spi);
      while(getSn_CR(sn,flag_spi));
   }
   sock_remained_size[sn] -= recvsize;
   if(sock_remained_size[sn] != 0)
   {
      sock_pack_info[sn] |= PACK_REMAINED;
      if(recvsize & 0x1) sock_pack_info[sn] |= PACK_FIFOBYTE;
   }
   else sock_pack_info[sn] = PACK_COMPLETED;
   if(getSn_MR(sn,flag_spi) & Sn_MR_ALIGN) sock_remained_size[sn] = 0;
   //len = recvsize;
#else   
   if(recvsize < len) len = recvsize;   
   wiz_recv_data(sn, buf, len,flag_spi);
   setSn_CR(sn,Sn_CR_RECV,flag_spi);
   while(getSn_CR(sn,flag_spi));
#endif
     
   //M20150409 : Explicit Type Casting
   //return len;
   return (int32_t)len;
}

int32_t sendto(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port, uint8_t flag_spi)
{
   uint8_t tmp = 0;
   uint16_t freesize = 0;
   uint32_t taddr;

   CHECK_SOCKNUM();
   switch(getSn_MR(sn,flag_spi) & 0x0F)
   {
      case Sn_MR_UDP:
      case Sn_MR_MACRAW:
//         break;
//   #if ( _WIZCHIP_ < 5200 )
      case Sn_MR_IPRAW:
         break;
//   #endif
      default:
         return SOCKERR_SOCKMODE;
   }
   //CHECK_SOCKDATA(); //�����������
   //M20140501 : For avoiding fatal error on memory align mismatched
   //if(*((uint32_t*)addr) == 0) return SOCKERR_IPINVALID;
   //{
      //uint32_t taddr;
      taddr = ((uint32_t)addr[0]) & 0x000000FF;
      taddr = (taddr << 8) + ((uint32_t)addr[1] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[2] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[3] & 0x000000FF);
   //}
   //
   //if(*((uint32_t*)addr) == 0) return SOCKERR_IPINVALID;
   if((taddr == 0) && ((getSn_MR(sn,flag_spi)&Sn_MR_MACRAW) != Sn_MR_MACRAW)) return SOCKERR_IPINVALID;
   if((port  == 0) && ((getSn_MR(sn,flag_spi)&Sn_MR_MACRAW) != Sn_MR_MACRAW)) return SOCKERR_PORTZERO;
   tmp = getSn_SR(sn,flag_spi);
#if ( _WIZCHIP_ < 5200 )
   if((tmp != SOCK_MACRAW) && (tmp != SOCK_UDP) && (tmp != SOCK_IPRAW)) return SOCKERR_SOCKSTATUS;
#else
   if(tmp != SOCK_MACRAW && tmp != SOCK_UDP) return SOCKERR_SOCKSTATUS;
#endif
      
   setSn_DIPR(sn,addr,flag_spi);
   setSn_DPORT(sn,port,flag_spi);
   freesize = getSn_TxMAX(sn,flag_spi);
   if (len > freesize) len = freesize; // check size not to exceed MAX size.
   while(1)
   {
      freesize = getSn_TX_FSR(sn,flag_spi);
      if(getSn_SR(sn,flag_spi) == SOCK_CLOSED) return SOCKERR_SOCKCLOSED;
      //if( (sock_io_mode & (1<<sn)) && (len > freesize) ) return SOCK_BUSY;
      if(len <= freesize) break;
   };
	wiz_send_data(sn, buf, len,flag_spi);

   #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
      getSIPR((uint8_t*)&taddr, flag_spi);
      if(taddr == 0)
      {
         getSUBR((uint8_t*)&taddr, flag_spi);
         setSUBR((uint8_t*)"\x00\x00\x00\x00", flag_spi);
      }
      else taddr = 0;
   #endif

//A20150601 : For W5300
#if _WIZCHIP_ == 5300
   setSn_TX_WRSR(sn, len, flag_spi);
#endif
//   
	setSn_CR(sn,Sn_CR_SEND, flag_spi);
	/* wait to process the command... */
	while(getSn_CR(sn,flag_spi));
   while(1)
   {
      tmp = getSn_IR(sn,flag_spi);
      if(tmp & Sn_IR_SENDOK)
      {
         setSn_IR(sn, Sn_IR_SENDOK,flag_spi);
         break;
      }
      //M:20131104
      //else if(tmp & Sn_IR_TIMEOUT) return SOCKERR_TIMEOUT;
      else if(tmp & Sn_IR_TIMEOUT)
      {
         setSn_IR(sn, Sn_IR_TIMEOUT,flag_spi);
         //M20150409 : Fixed the lost of sign bits by type casting.
         //len = (uint16_t)SOCKERR_TIMEOUT;
         //break;
         #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
            if(taddr) setSUBR((uint8_t*)&taddr, flag_spi);
         #endif
         return SOCKERR_TIMEOUT;
      }
      ////////////
   }
   #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
      if(taddr) setSUBR((uint8_t*)&taddr, flag_spi);
   #endif
   //M20150409 : Explicit Type Casting
   //return len;
   return (int32_t)len;
}
int32_t sendto_dma(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port, uint8_t flag_spi)
{
   uint8_t tmp = 0;
   uint16_t freesize = 0;
   uint32_t taddr;

   CHECK_SOCKNUM();
   switch(getSn_MR(sn,flag_spi) & 0x0F)
   {
      case Sn_MR_UDP:
      case Sn_MR_MACRAW:
//         break;
//   #if ( _WIZCHIP_ < 5200 )
      case Sn_MR_IPRAW:
         break;
//   #endif
      default:
         return SOCKERR_SOCKMODE;
   }
   //CHECK_SOCKDATA(); //�����������
   //M20140501 : For avoiding fatal error on memory align mismatched
   //if(*((uint32_t*)addr) == 0) return SOCKERR_IPINVALID;
   //{
      //uint32_t taddr;
      taddr = ((uint32_t)addr[0]) & 0x000000FF;
      taddr = (taddr << 8) + ((uint32_t)addr[1] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[2] & 0x000000FF);
      taddr = (taddr << 8) + ((uint32_t)addr[3] & 0x000000FF);
   //}
   //
   //if(*((uint32_t*)addr) == 0) return SOCKERR_IPINVALID;
   if((taddr == 0) && ((getSn_MR(sn,flag_spi)&Sn_MR_MACRAW) != Sn_MR_MACRAW)) return SOCKERR_IPINVALID;
   if((port  == 0) && ((getSn_MR(sn,flag_spi)&Sn_MR_MACRAW) != Sn_MR_MACRAW)) return SOCKERR_PORTZERO;
   tmp = getSn_SR(sn,flag_spi);
#if ( _WIZCHIP_ < 5200 )
   if((tmp != SOCK_MACRAW) && (tmp != SOCK_UDP) && (tmp != SOCK_IPRAW)) return SOCKERR_SOCKSTATUS;
#else
   if(tmp != SOCK_MACRAW && tmp != SOCK_UDP) return SOCKERR_SOCKSTATUS;
#endif

   setSn_DIPR(sn,addr,flag_spi);
   setSn_DPORT(sn,port,flag_spi);
   freesize = getSn_TxMAX(sn,flag_spi);
   if (len > freesize) len = freesize; // check size not to exceed MAX size.
   while(1)
   {
      freesize = getSn_TX_FSR(sn,flag_spi);
      if(getSn_SR(sn,flag_spi) == SOCK_CLOSED) return SOCKERR_SOCKCLOSED;
      //if( (sock_io_mode & (1<<sn)) && (len > freesize) ) return SOCK_BUSY;
      if(len <= freesize) break;
   };
	wiz_send_data_dma(sn, buf, len,flag_spi);

   #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
      getSIPR((uint8_t*)&taddr, flag_spi);
      if(taddr == 0)
      {
         getSUBR((uint8_t*)&taddr, flag_spi);
         setSUBR((uint8_t*)"\x00\x00\x00\x00", flag_spi);
      }
      else taddr = 0;
   #endif

//A20150601 : For W5300
#if _WIZCHIP_ == 5300
   setSn_TX_WRSR(sn, len, flag_spi);
#endif
//
	setSn_CR(sn,Sn_CR_SEND, flag_spi);
	/* wait to process the command... */
	while(getSn_CR(sn,flag_spi));
   while(1)
   {
      tmp = getSn_IR(sn,flag_spi);
      if(tmp & Sn_IR_SENDOK)
      {
         setSn_IR(sn, Sn_IR_SENDOK,flag_spi);
         break;
      }
      //M:20131104
      //else if(tmp & Sn_IR_TIMEOUT) return SOCKERR_TIMEOUT;
      else if(tmp & Sn_IR_TIMEOUT)
      {
         setSn_IR(sn, Sn_IR_TIMEOUT,flag_spi);
         //M20150409 : Fixed the lost of sign bits by type casting.
         //len = (uint16_t)SOCKERR_TIMEOUT;
         //break;
         #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
            if(taddr) setSUBR((uint8_t*)&taddr, flag_spi);
         #endif
         return SOCKERR_TIMEOUT;
      }
      ////////////
   }
   #if _WIZCHIP_ < 5500   //M20150401 : for WIZCHIP Errata #4, #5 (ARP errata)
      if(taddr) setSUBR((uint8_t*)&taddr, flag_spi);
   #endif
   //M20150409 : Explicit Type Casting
   //return len;
   return (int32_t)(len+4);
}


int32_t recvfrom(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t *port, uint8_t flag_spi)
{
//M20150601 : For W5300   
#if _WIZCHIP_ == 5300
   uint16_t mr;
   uint16_t mr1;
#else   
   uint8_t  mr;
#endif
//   
   uint8_t  head[8];
	uint16_t pack_len=0;

   CHECK_SOCKNUM();
   // CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
//A20150601
#if _WIZCHIP_ == 5300
   mr1 = getMR(flag_spi);
#endif   

   switch((mr=getSn_MR(sn,flag_spi)) & 0x0F)
   {
      case Sn_MR_UDP:
	  case Sn_MR_IPRAW:
      case Sn_MR_MACRAW:
         break;
   #if ( _WIZCHIP_ < 5200 )         
      case Sn_MR_PPPoE:
         break;
   #endif
      default:
         return SOCKERR_SOCKMODE;
   }
   CHECK_SOCKDATA();
   if(sock_remained_size[sn] == 0)
   {
      while(1)
      {
         pack_len = getSn_RX_RSR(sn,flag_spi);
         if(getSn_SR(sn,flag_spi) == SOCK_CLOSED) return SOCKERR_SOCKCLOSED;
         if( (sock_io_mode & (1<<sn)) && (pack_len == 0) ) return SOCK_BUSY;
         if(pack_len != 0) break;
      };
   }
//D20150601 : Move it to bottom
// sock_pack_info[sn] = PACK_COMPLETED;
	switch (mr & 0x07)
	{
	   case Sn_MR_UDP :
	      if(sock_remained_size[sn] == 0)
	      {
   			wiz_recv_data(sn, head, 8, flag_spi);
   			setSn_CR(sn,Sn_CR_RECV,flag_spi);
   			while(getSn_CR(sn,flag_spi));
   			// read peer's IP address, port number & packet length
   	   //A20150601 : For W5300
   		#if _WIZCHIP_ == 5300
   		   if(mr1 & MR_FS)
   		   {
   		      addr[0] = head[1];
   		      addr[1] = head[0];
   		      addr[2] = head[3];
   		      addr[3] = head[2];
   		      *port = head[5];
   		      *port = (*port << 8) + head[4];
      			sock_remained_size[sn] = head[7];
      			sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[6];
   		   }
            else
            {
         #endif
               addr[0] = head[0];
      			addr[1] = head[1];
      			addr[2] = head[2];
      			addr[3] = head[3];
      			*port = head[4];
      			*port = (*port << 8) + head[5];
      			sock_remained_size[sn] = head[6];
      			sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[7];
         #if _WIZCHIP_ == 5300
            }
         #endif
   			sock_pack_info[sn] = PACK_FIRST;
   	   }
			if(len < sock_remained_size[sn]) pack_len = len;
			else pack_len = sock_remained_size[sn];
			//A20150601 : For W5300
			len = pack_len;
			#if _WIZCHIP_ == 5300
			   if(sock_pack_info[sn] & PACK_FIFOBYTE)
			   {
			      *buf++ = sock_remained_byte[sn];
			      pack_len -= 1;
			      sock_remained_size[sn] -= 1;
			      sock_pack_info[sn] &= ~PACK_FIFOBYTE;
			   }
			#endif
			//
			// Need to packet length check (default 1472)
			//
   		wiz_recv_data(sn, buf, pack_len,flag_spi); // data copy.
			break;
	   case Sn_MR_MACRAW :
	      if(sock_remained_size[sn] == 0)
	      {
   			wiz_recv_data(sn, head, 2,flag_spi);
   			setSn_CR(sn,Sn_CR_RECV,flag_spi);
   			while(getSn_CR(sn,flag_spi));
   			// read peer's IP address, port number & packet length
    			sock_remained_size[sn] = head[0];
   			sock_remained_size[sn] = (sock_remained_size[sn] <<8) + head[1] -2;
   			#if _WIZCHIP_ == W5300
   			if(sock_remained_size[sn] & 0x01)
   				sock_remained_size[sn] = sock_remained_size[sn] + 1 - 4;
   			else
   				sock_remained_size[sn] -= 4;
			#endif
   			if(sock_remained_size[sn] > 1514) 
   			{
   			   close(sn,flag_spi);
   			   return SOCKFATAL_PACKLEN;
   			}
   			sock_pack_info[sn] = PACK_FIRST;
   	   }
			if(len < sock_remained_size[sn]) pack_len = len;
			else pack_len = sock_remained_size[sn];
			wiz_recv_data(sn,buf,pack_len,flag_spi);
		   break;
   //#if ( _WIZCHIP_ < 5200 )
		case Sn_MR_IPRAW:
		   if(sock_remained_size[sn] == 0)
		   {
   			wiz_recv_data(sn, head, 6,flag_spi);
   			setSn_CR(sn,Sn_CR_RECV,flag_spi);
   			while(getSn_CR(sn,flag_spi));
   			addr[0] = head[0];
   			addr[1] = head[1];
   			addr[2] = head[2];
   			addr[3] = head[3];
   			sock_remained_size[sn] = head[4];
   			//M20150401 : For Typing Error
   			//sock_remaiend_size[sn] = (sock_remained_size[sn] << 8) + head[5];
   			sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[5];
   			sock_pack_info[sn] = PACK_FIRST;
         }
			//
			// Need to packet length check
			//
			if(len < sock_remained_size[sn]) pack_len = len;
			else pack_len = sock_remained_size[sn];
   		wiz_recv_data(sn, buf, pack_len, flag_spi); // data copy.
			break;
   //#endif
      default:
         wiz_recv_ignore(sn, pack_len,flag_spi); // data copy.
         sock_remained_size[sn] = pack_len;
         break;
   }
	setSn_CR(sn,Sn_CR_RECV,flag_spi);
	/* wait to process the command... */
	while(getSn_CR(sn,flag_spi)) ;
	sock_remained_size[sn] -= pack_len;
	//M20150601 : 
	//if(sock_remained_size[sn] != 0) sock_pack_info[sn] |= 0x01;
	if(sock_remained_size[sn] != 0)
	{
	   sock_pack_info[sn] |= PACK_REMAINED;
   #if _WIZCHIP_ == 5300	   
	   if(pack_len & 0x01) sock_pack_info[sn] |= PACK_FIFOBYTE;
   #endif	      
	}
	else sock_pack_info[sn] = PACK_COMPLETED;
#if _WIZCHIP_ == 5300	   
   pack_len = len;
#endif
   //
   //M20150409 : Explicit Type Casting
   //return pack_len;
   return (int32_t)pack_len;
}


int8_t  ctlsocket(uint8_t sn, ctlsock_type cstype, void* arg, uint8_t flag_spi)
{
   uint8_t tmp = 0;
   CHECK_SOCKNUM();
   switch(cstype)
   {
      case CS_SET_IOMODE:
         tmp = *((uint8_t*)arg);
         if(tmp == SOCK_IO_NONBLOCK)  sock_io_mode |= (1<<sn);
         else if(tmp == SOCK_IO_BLOCK) sock_io_mode &= ~(1<<sn);
         else return SOCKERR_ARG;
         break;
      case CS_GET_IOMODE:   
         //M20140501 : implict type casting -> explict type casting
         //*((uint8_t*)arg) = (sock_io_mode >> sn) & 0x0001;
         *((uint8_t*)arg) = (uint8_t)((sock_io_mode >> sn) & 0x0001);
         //
         break;
      case CS_GET_MAXTXBUF:
         *((uint16_t*)arg) = getSn_TxMAX(sn,flag_spi);
         break;
      case CS_GET_MAXRXBUF:    
         *((uint16_t*)arg) = getSn_RxMAX(sn,flag_spi);
         break;
      case CS_CLR_INTERRUPT:
         if( (*(uint8_t*)arg) > SIK_ALL) return SOCKERR_ARG;
         setSn_IR(sn,*(uint8_t*)arg, flag_spi);
         break;
      case CS_GET_INTERRUPT:
         *((uint8_t*)arg) = getSn_IR(sn,flag_spi);
         break;
   #if _WIZCHIP_ != 5100
      case CS_SET_INTMASK:  
         if( (*(uint8_t*)arg) > SIK_ALL) return SOCKERR_ARG;
         setSn_IMR(sn,*(uint8_t*)arg,flag_spi);
         break;
      case CS_GET_INTMASK:   
         *((uint8_t*)arg) = getSn_IMR(sn,flag_spi);
         break;
   #endif
      default:
         return SOCKERR_ARG;
   }
   return SOCK_OK;
}

int8_t  setsockopt(uint8_t sn, sockopt_type sotype, void* arg, uint8_t flag_spi)
{
 // M20131220 : Remove warning
 //uint8_t tmp;
   CHECK_SOCKNUM();
   switch(sotype)
   {
      case SO_TTL:
         setSn_TTL(sn,*(uint8_t*)arg, flag_spi);
         break;
      case SO_TOS:
         setSn_TOS(sn,*(uint8_t*)arg,flag_spi);
         break;
      case SO_MSS:
         setSn_MSSR(sn,*(uint16_t*)arg, flag_spi);
         break;
      case SO_DESTIP:
         setSn_DIPR(sn, (uint8_t*)arg, flag_spi);
         break;
      case SO_DESTPORT:
         setSn_DPORT(sn, *(uint16_t*)arg, flag_spi);
         break;
#if _WIZCHIP_ != 5100
      case SO_KEEPALIVESEND:
    	  CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
         #if _WIZCHIP_ > 5200
            if(getSn_KPALVTR(sn, flag_spi) != 0) return SOCKERR_SOCKOPT;
         #endif
            setSn_CR(sn,Sn_CR_SEND_KEEP, flag_spi);
            while(getSn_CR(sn, flag_spi) != 0)
            {
               // M20131220
         		//if ((tmp = getSn_IR(sn)) & Sn_IR_TIMEOUT)
               if (getSn_IR(sn, flag_spi) & Sn_IR_TIMEOUT)
         		{
         			setSn_IR(sn, Sn_IR_TIMEOUT, flag_spi);
                  return SOCKERR_TIMEOUT;
         		}
            }
         break;
   #if !( (_WIZCHIP_ == 5100) || (_WIZCHIP_ == 5200) )
      case SO_KEEPALIVEAUTO:
    	  CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
         setSn_KPALVTR(sn,*(uint8_t*)arg, flag_spi);
         break;
   #endif      
#endif   
      default:
         return SOCKERR_ARG;
   }   
   return SOCK_OK;
}

int8_t  getsockopt(uint8_t sn, sockopt_type sotype, void* arg, uint8_t flag_spi)
{
   CHECK_SOCKNUM();
   switch(sotype)
   {
      case SO_FLAG:
         *(uint8_t*)arg = getSn_MR(sn,flag_spi) & 0xF0;
         break;
      case SO_TTL:
         *(uint8_t*) arg = getSn_TTL(sn,flag_spi);
         break;
      case SO_TOS:
         *(uint8_t*) arg = getSn_TOS(sn,flag_spi);
         break;
      case SO_MSS:   
         *(uint16_t*) arg = getSn_MSSR(sn,flag_spi);
         break;
      case SO_DESTIP:
         getSn_DIPR(sn, (uint8_t*)arg,flag_spi);
         break;
      case SO_DESTPORT:  
         *(uint16_t*) arg = getSn_DPORT(sn,flag_spi);
         break;
   #if _WIZCHIP_ > 5200   
      case SO_KEEPALIVEAUTO:
    	 CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
         *(uint16_t*) arg = getSn_KPALVTR(sn,flag_spi);
         break;
   #endif      
      case SO_SENDBUF:
         *(uint16_t*) arg = getSn_TX_FSR(sn,flag_spi);
         break;
      case SO_RECVBUF:
         *(uint16_t*) arg = getSn_RX_RSR(sn,flag_spi);
         break;
      case SO_STATUS:
         *(uint8_t*) arg = getSn_SR(sn,flag_spi);
         break;
      case SO_REMAINSIZE:
         if(getSn_MR(sn,flag_spi) & Sn_MR_TCP)
            *(uint16_t*)arg = getSn_RX_RSR(sn,flag_spi);
         else
            *(uint16_t*)arg = sock_remained_size[sn];
         break;
      case SO_PACKINFO:
         // CHECK_SOCKMODE(Sn_MR_TCP, flag_spi);
#if _WIZCHIP_ != 5300
         if((getSn_MR(sn,flag_spi) == Sn_MR_TCP))
             return SOCKERR_SOCKMODE;
#endif
         *(uint8_t*)arg = sock_pack_info[sn];
         break;
      default:
         return SOCKERR_SOCKOPT;
   }
   return SOCK_OK;
}

