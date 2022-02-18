/*
 * wizchip_init.h
 *      Author: Alexander Baranov
 */

#ifndef WIZCHIP_INIT_H_
#define WIZCHIP_INIT_H_

#include "socket.h"
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include "wizchip_conf.h"
///////////////////////////////////////////////
//Select the library for your stm32fxxx_hal.h//
///////////////////////////////////////////////
#include "stm32f4xx_hal.h"
//////////////////////////////////////////////////
//Write  GPIOx peripheral and specifies the port//
//bit GPIO_PINx to be written for SPI CS 		//
//////////////////////////////////////////////////

extern SPI_HandleTypeDef hspi1;
#define SPI_WIZCHIP_src  			&hspi1

extern SPI_HandleTypeDef hspi2;
#define SPI_WIZCHIP_dst				&hspi2

#define SOCK_TCPS        			0
#define SOCK_UDPS					0

#define DATA_BUF_SIZE				4016

////////////////////////////////////////////////

/* Variables --------------------------------------------------------*/
typedef struct udp_NetInfo_{
	uint8_t destip[4];
	uint16_t destport;
}udp_NetInfo;

void  wizchip_select_src(void);
void  wizchip_select_dst(void);

void  wizchip_deselect_src(void);
void wizchip_deselect_dst(void);

void  wizchip_write_src(uint8_t wb);
void  wizchip_write_dst(uint8_t wb);

uint8_t wizchip_read_src();
uint8_t wizchip_read_dst();

void wizchip_read_burst_src(uint8_t* pBuf, uint16_t len);
void wizchip_write_burst_src(uint8_t* pBuf, uint16_t len);

void wizchip_read_burst_dst(uint8_t* pBuf, uint16_t len);
void wizchip_write_burst_dst(uint8_t* pBuf, uint16_t len);

void UART_Printf(const char* fmt, ...);
void W5500_Reset();

/* Main function */
void WizchIP_main();

/* Init network information about w5550's (ip, mac ... )*/
void network_init(const uint8_t src_con, const uint8_t dst_con);

/*Critical functions. Disable irq, when you are here. You can disable this */
void critical_enter();
void critical_exit();

/*Main func for TCP*/
int32_t loopback_tcps(uint8_t sn_src,uint8_t sn_dst, uint8_t* buf, uint16_t port, const uint8_t src_con, const uint8_t dst_con);

/*Main func for UDP*/
int32_t loopback_udp(uint8_t sn_src,uint8_t sn_dst, uint8_t* buf, uint16_t port, const uint8_t src_con, const uint8_t dst_con);

/*The function will check if there are clients*/
int8_t keep_alive(uint8_t socket_numb, uint8_t* time,CH_WIZNET base );

#endif /* WIZCHIP_INIT_H_ */
