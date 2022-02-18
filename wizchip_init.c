/*
 * wizchip_init.c
 */

#include "wizchip_init.h"

//uint8_t destip_src[4] = { 169, 254, 153, 204 };		// IP
//uint8_t destip_dst[4] = { 169, 254, 153, 204 };		// IP
//uint8_t ip_src[4] = { 169, 254, 153, 204 };			// IP SRC
//uint8_t ip_dst[4] = { 169, 254, 153, 205 };			// IP DST
//
//uint16_t port_src = 3000;						// port SRC
//uint16_t port_dst = 3001;						// port DST
//uint16_t destport_src;
//uint16_t destport_dst;

/* This needs to get ip and port, when send answer */
udp_NetInfo udpNetInfo_src = { .destip = { 169, 254, 153, 204 }};
udp_NetInfo udpNetInfo_dst = { .destip = { 169, 254, 153, 204 }};

/* Mac, IP ... W5500*/
static wiz_NetInfo gWIZNETINFO_src = { .mac = { 0x18, 0xCF, 0x5E, 0x53, 0xBF,
		0x30 }, .ip = { 169, 254, 153, 204 }, .sn = { 255, 255, 0, 0 }, .gw = {
		0, 0, 0, 0 }, .dns = { 0, 0, 0, 0 }, .dhcp = NETINFO_STATIC };

/* Mac, IP ... W5500*/
static wiz_NetInfo gWIZNETINFO_dst = { .mac = { 0x18, 0xCF, 0x5E, 0x53, 0xBF,
		0x3E }, //DST
		.ip = { 169, 254, 153, 205 }, .sn = { 255, 255, 0, 0 }, .gw = { 0, 0, 0,
				0 }, .dns = { 0, 0, 0, 0 }, .dhcp = NETINFO_STATIC };

void W5500_Reset()
{
	/* Reset 1st module W5500 */
	GPIOA->BSRR = GPIO_BSRR_BR15;	//Rst Pin (active High)
	HAL_Delay(100);
	GPIOA->BSRR = GPIO_BSRR_BS15;

	/* Reset 2nd module W5500 */
	GPIOC->BSRR = GPIO_BSRR_BR5;
	HAL_Delay(100);
	GPIOC->BSRR = GPIO_BSRR_BS5;

	HAL_Delay(200);
}
void WizchIP_main() {

	uint8_t gDATABUF[DATA_BUF_SIZE];

	uint8_t tmp_src = 0;
	uint8_t tmp_dst = 0;
	int32_t ret = 0;
	uint8_t memsize[2][8] = { { 16, 0, 0, 0, 0, 0, 0, 0 }, { 16, 0, 0, 0, 0, 0, 0, 0 } };
	uint8_t memsize_dst[2][8] = { { 16, 0, 0, 0, 0, 0, 0, 0 }, { 16, 0, 0, 0, 0, 0, 0, 0 } };

	/* RESET Wiznet. Need to the start*/
	W5500_Reset();

	////////////////////////////////////////////////////////////////////////////////////////////////////
	/* Critical section callback - Function to protect against events such as interupts and task switching while WIZCHIP is accessing */
	reg_wizchip_cris_cbfunc(critical_enter, critical_exit, SRC);
	reg_wizchip_cris_cbfunc(critical_enter, critical_exit, DST);

	/* Chip selection call back */
#if   _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_VDM_
	reg_wizchip_cs_cbfunc(wizchip_select_src, wizchip_deselect_src, SRC);
	reg_wizchip_cs_cbfunc(wizchip_select_dst, wizchip_deselect_dst, DST);

#elif _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_FDM_
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_select); // CS must be tried with LOW.
#else
#if (_WIZCHIP_IO_MODE_ & _WIZCHIP_IO_MODE_SIP_) != _WIZCHIP_IO_MODE_SIP_
#error "Unknown _WIZCHIP_IO_MODE_"
#else
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
#endif
#endif
	/* SPI Read & Write callback function */

	reg_wizchip_spi_cbfunc(wizchip_read_src, wizchip_write_src, SRC);
	reg_wizchip_spiburst_cbfunc(wizchip_read_burst_src, wizchip_write_burst_src, SRC);

	reg_wizchip_spi_cbfunc(wizchip_read_dst, wizchip_write_dst, DST);
	reg_wizchip_spiburst_cbfunc(wizchip_read_burst_dst, wizchip_write_burst_dst,
			DST);
	/* WIZCHIP SOCKET Buffer initialize */

	if (ctlwizchip(CW_INIT_WIZCHIP, (void*) memsize, SRC) == -1) {
#ifdef DEBUG_W5500
		UART_Printf("WIZCHIP SRC Initialized fail.\r\n");
#endif
		while (1);
	}

	if (ctlwizchip(CW_INIT_WIZCHIP, (void*) memsize_dst, DST) == -1) {
#ifdef DEBUG_W5500
		UART_Printf("WIZCHIP DST Initialized fail.\r\n");
#endif
		while (1);
	}

	uint8_t ret_version_src = 0, ret_version_dst = 0;

	ret_version_src = getVERSIONR(SRC);
	ret_version_dst = getVERSIONR(DST);

	if (ret_version_src != 0x4 && ret_version_dst != 0x4) {
		//Error read SPIs
		while (1);
	}

	volatile uint8_t src_connected = false;
	volatile uint8_t dst_connected = false;
	uint32_t time_to_connect = HAL_GetTick();

	while (1) {
		if (!src_connected) {
			ctlwizchip(CW_GET_PHYLINK, (void*) &tmp_src, SRC);
			if (tmp_src == PHYCFGR_LNK_ON)
				src_connected = 1;
		}

		if (!dst_connected) {
			ctlwizchip(CW_GET_PHYLINK, (void*) &tmp_dst, DST);
			if (tmp_dst == PHYCFGR_LNK_ON)
				dst_connected = 1;
		}
		if (src_connected && dst_connected)
			break;

		if ((HAL_GetTick() - time_to_connect) > 1000) {
			if (src_connected)
				break;
		}

	}

//	        /* PHY link status check for SRC*/
//	        do
//	        {
//	        	if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp_src, DST) == -1)
//
//	        	   UART_Printf("Unknown PHY Link status.\r\n");
//
//	        }while(tmp_src == PHY_LINK_OFF);
//	        dst_connected = 1;
//
//	        /* PHY link status check for DST*/
//	        do
//	        {
//	           if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp_src, SRC) == -1)
//
//	        	   UART_Printf("Unknown PHY Link status.\r\n");
//
//	        }while(tmp_src == PHY_LINK_OFF);
//	        src_connected = 1;

	network_init(src_connected, dst_connected);

	while (1) {


//		if( (ret = loopback_tcps(SOCK_TCPS, SOCK_TCPS, gDATABUF, 5000, src_connected, dst_connected)) < 0)
//		{
//			UART_Printf("SOCKET ERROR : %ld\r\n", ret);
//		}
		if ((ret = loopback_udp(SOCK_UDPS, SOCK_UDPS, gDATABUF, 3000,
				src_connected, dst_connected)) < 0) {
#ifdef DEBUG_W5500
			UART_Printf("SOCKET ERROR : %ld\r\n", ret);
#endif
		}
	}
}

void wizchip_select_dst(void) {
	GPIOD->BSRR = GPIO_BSRR_BR2;
}

void wizchip_deselect_dst(void) {

	GPIOD->BSRR = GPIO_BSRR_BS2;
}
void wizchip_write_dst(uint8_t wb)    //Write SPI
{
	HAL_SPI_Transmit(SPI_WIZCHIP_dst, &wb, 1, HAL_MAX_DELAY);
}

uint8_t wizchip_read_dst() //Read SPI
{
	uint8_t spi_read_buf;
	HAL_SPI_Receive(SPI_WIZCHIP_dst, &spi_read_buf, 1, HAL_MAX_DELAY);
	return spi_read_buf;
}

void wizchip_read_burst_dst(uint8_t* pBuf, uint16_t len) //Read SPI
{
	HAL_SPI_Receive(SPI_WIZCHIP_dst, pBuf, len, HAL_MAX_DELAY);
}

void wizchip_write_burst_dst(uint8_t* pBuf, uint16_t len) //Read SPI
{
	HAL_SPI_Transmit(SPI_WIZCHIP_dst, pBuf, len, HAL_MAX_DELAY);
}

void wizchip_select_src(void) {
	GPIOA->BSRR = GPIO_BSRR_BR4;
}

void wizchip_deselect_src(void) {
	//HAL_GPIO_WritePin(GPIOx_SPI_CS, GPIO_PIN_SET); //GPIOA, GPIO_PIN_4
	GPIOA->BSRR = GPIO_BSRR_BS4;
}

void wizchip_write_src(uint8_t wb)    //Write SPI
{
	HAL_SPI_Transmit(SPI_WIZCHIP_src, &wb, 1, HAL_MAX_DELAY);
}

uint8_t wizchip_read_src() //Read SPI
{
	uint8_t spi_read_buf;
	HAL_SPI_Receive(SPI_WIZCHIP_src, &spi_read_buf, 1, HAL_MAX_DELAY);
	return spi_read_buf;
}

void wizchip_read_burst_src(uint8_t* pBuf, uint16_t len) //Read SPI
{
#ifdef SPI_DMA
	HAL_SPI_ReceiveDMA(SPI_WIZCHIP, pBuf, len, HAL_MAX_DELAY);
	while(HAL_SPI_GetState(SPI_WIZCHIP) == HAL_SPI_STATE_BUSY_RX);
#else
	HAL_SPI_Receive(SPI_WIZCHIP_src, pBuf, len, HAL_MAX_DELAY);
#endif
}

void wizchip_write_burst_src(uint8_t* pBuf, uint16_t len) //Read SPI
{
#ifdef SPI_DMA
	HAL_SPI_Transmit_DMA(SPI_WIZCHIP, pBuf, len);
	while(HAL_SPI_GetState(SPI_WIZCHIP) == HAL_SPI_STATE_BUSY_TX);
#else
	HAL_SPI_Transmit(SPI_WIZCHIP_src, pBuf, len, HAL_MAX_DELAY);
#endif
}

void network_init(const uint8_t src_con, const uint8_t dst_con) {
	uint8_t rx_tx_buff_sizes_src[] = { 16, 0, 0, 0, 0, 0, 0, 0 };
	uint8_t rx_tx_buff_sizes_dst[] = { 16, 0, 0, 0, 0, 0, 0, 0 };
	uint8_t tmpstr_src[6] = { 0, };
	uint8_t tmpstr_dst[6] = { 0, };

	if (src_con) {
		wizchip_init(rx_tx_buff_sizes_src, rx_tx_buff_sizes_src, SRC);
		ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO_src, SRC);
		// Display Network Information
		ctlwizchip(CW_GET_ID, (void*) tmpstr_src, SRC);
	}

	if (dst_con) {
		wizchip_init(rx_tx_buff_sizes_dst, rx_tx_buff_sizes_dst, DST);
		ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO_dst, DST);
		// Display Network Information
		ctlwizchip(CW_GET_ID, (void*) tmpstr_dst, DST);
	}
}

int32_t loopback_tcps(uint8_t sn_src, uint8_t sn_dst, uint8_t* buf,
		uint16_t port, const uint8_t src_con, const uint8_t dst_con) {
	int32_t ret;
	uint16_t size = 0;
	// uint8_t packet[BUFSIZE+1]={0,};
	/* ------------------ SRC ------------------------*/
	if (src_con) {
		switch (getSn_SR(sn_src, SRC)) {
		case SOCK_ESTABLISHED:

			if (getSn_IR(sn_src, SRC) & Sn_IR_CON) {
			#ifdef DEBUG_W5500
				UART_Printf("%d:Connected\r\n",sn_src);
			#endif
				setSn_IR(sn_src, Sn_IR_CON, SRC);
			}

			if ((size = getSn_RX_RSR(sn_src, SRC)) > 0) {
				if (size > DATA_BUF_SIZE)
					size = DATA_BUF_SIZE;
				ret = recv(sn_src, buf, size, SRC);	//get length packet
				if (ret <= 0)
					return ret;
			}

			break;
		case SOCK_CLOSE_WAIT:
#ifdef DEBUG_W5500
			UART_Printf("%d:CloseWait\r\n",sn_src);
#endif
			if ((ret = disconnect(sn_src, SRC)) != SOCK_OK)
				return ret;

			if (dst_con) {
				if ((ret = disconnect(sn_dst, DST)) != SOCK_OK)
					return ret;	//disconnect client
			}
#ifdef DEBUG_W5500
			UART_Printf("%d:Closed\r\n", sn_src);
#endif

			break;
		case SOCK_INIT:
#ifdef DEBUG_W5500
			UART_Printf("%d:Listen, port [%d]\r\n",sn_src, port);
#endif
			if ((ret = listen(sn_src, SRC)) != SOCK_OK)
				return ret;

			break;
		case SOCK_CLOSED:
#ifdef DEBUG_W5500
			UART_Printf("%d:LBTStart\r\n",sn_src);
#endif
			setSn_KPALVTR(sn_src, 10, SRC); //KEEP ALIVE every 50sec

			if ((ret = socket(sn_src, Sn_MR_TCP, port, 0x00, SRC)) != sn_src)
				return ret;
			//UART_Printf("%d:Opened\r\n",sn_src);
			break;
		default:
			break;
		}
	}

	if (dst_con) {
		switch (getSn_SR(sn_dst, DST)) {
		case SOCK_ESTABLISHED:

			if (getSn_IR(sn_dst, DST) & Sn_IR_CON) {
#ifdef DEBUG_W5500
				UART_Printf("%d:Connected\r\n",sn);
#endif
				setSn_IR(sn_dst, Sn_IR_CON, DST);
			}
			if ((size = getSn_RX_RSR(sn_dst, DST)) > 0) {

				if (size > DATA_BUF_SIZE)
					size = DATA_BUF_SIZE;
				ret = recv(sn_dst, buf, size, DST);
				if (ret <= 0)
					return ret; //

				ret = send(sn_src, buf, ret, SRC);
				if (ret < 0) {
					close(sn_dst, DST);
					return -1;
				}
			}

			break;
		case SOCK_CLOSE_WAIT:
#ifdef DEBUG_W5500
			UART_Printf("%d:CloseWait\r\n",sn_dst);
#endif
			if ((ret = disconnect(sn_dst, DST)) != SOCK_OK)
				return ret;
#ifdef DEBUG_W5500
			UART_Printf("%d:Closed\r\n",sn_dst);
#endif

			break;
		case SOCK_INIT:
#ifdef DEBUG_W5500
			UART_Printf("%d:Connect, port [%d]\r\n",sn_dst, port);
#endif
			if ((ret = connect(sn_dst, gWIZNETINFO_src.ip, port, DST))
					!= SOCK_OK) {
				return ret;
			}
#ifdef DEBUG_W5500
			UART_Printf("%d:Connect, port [%d]\r\n", sn_dst, port);
#endif
			break;
		case SOCK_CLOSED:
#ifdef DEBUG_W5500
			UART_Printf("%d:LBTStart\r\n", DST);
#endif
			setSn_KPALVTR(sn_dst, 10, DST);

			if ((ret = socket(sn_dst, Sn_MR_TCP, 10800, 0x00, DST)) != sn_dst)
				return ret;
#ifdef DEBUG_W5500
			UART_Printf("%d:Opened\r\n", sn_dst);
#endif
			//setSn_KPALVTR(sn, 10, DST);
			break;
		default:
			break;
		}
	} /*else {
	 uint8_t tmp_dst = 0;
	 if (ctlwizchip(CW_GET_PHYLINK, (void*) &tmp_dst, DST) == -1)
	 UART_Printf("Unknown PHY Link status.\r\n"); //Р СњР ВµР С‘Р В·Р Р†Р ВµРЎРѓРЎвЂљР Р…РЎвЂ№Р в„– РЎРѓРЎвЂљР В°РЎвЂљРЎС“РЎРѓ
	 if (tmp_dst == PHYCFGR_LNK_ON)
	 dst_connected = 1;
	 else
	 return 1;

	 uint8_t rx_tx_buff_sizes_dst[] = { 16, 0, 0, 0, 0, 0, 0, 0 };
	 uint8_t tmpstr_dst[6] = { 0, };

	 if (dst_con) {
	 wizchip_init(rx_tx_buff_sizes_dst, rx_tx_buff_sizes_dst, DST);
	 ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO_dst, DST);
	 // Display Network Information
	 ctlwizchip(CW_GET_ID, (void*) tmpstr_dst, DST);
	 }
	 }*/
	return 1;
}

int32_t loopback_udp(uint8_t sn_src, uint8_t sn_dst, uint8_t* buf,
		uint16_t port, const uint8_t src_con, const uint8_t dst_con) {
	int32_t ret;
	uint16_t size;

	if (src_con) {
		switch (getSn_SR(sn_src, SRC)) {
		case SOCK_UDP:
			//ret = sendto(sn_src,tmp_buf_adc_full,4,gWIZNETINFO_dst.ip,destport_dst, SRC);
			if ((size = getSn_RX_RSR(sn_src, SRC)) > 0) {
				if (size > DATA_BUF_SIZE)
					size = DATA_BUF_SIZE;
				ret = recvfrom(sn_src, buf, size, udpNetInfo_src.destip,
						(uint16_t*) &udpNetInfo_src.destport, SRC);
				if (ret <= 0) {
#ifdef DEBUG_W5500
					UART_Printf("%d: recvfrom error. %ld\r\n",sn,ret);
#endif
					return ret;
				}
			}
			break;
		case SOCK_CLOSED:
#ifdef DEBUG_W5500
			UART_Printf("%d:LBTStart\r\n", sn_src);
#endif
			//setSn_KPALVTR(sn_src, 10, SRC); //KEEP ALIVE every 50sec
			if ((ret = socket(sn_src, Sn_MR_UDP, port, 0x00, SRC)) != sn_src)
				return ret;
#ifdef DEBUG_W5500
			UART_Printf("%d:Opened, port [%d]\r\n",sn, port);
#endif
			break;
		default:
			break;
		}
	}

	if (dst_con) {
		switch (getSn_SR(sn_dst, DST)) {
		case SOCK_UDP:

			if ((size = getSn_RX_RSR(sn_dst, DST)) > 0) {
				if (size > DATA_BUF_SIZE)
					size = DATA_BUF_SIZE;
				ret = recvfrom(sn_dst, buf, size, udpNetInfo_dst.destip,
						(uint16_t*) &udpNetInfo_dst.destport, DST);
				if (ret <= 0)
					return ret;
				ret = sendto(sn_src, buf, ret, udpNetInfo_src.destip, udpNetInfo_src.destport, SRC);
				if (ret < 0) {
					return ret;
				}
			}

			break;

		case SOCK_CLOSED:
			UART_Printf("%d:LBTStart\r\n", DST);
			//Р РЋР С•Р В·Р Т‘Р В°Р Р…Р С‘Р Вµ РЎРѓР С•Р С”Р ВµРЎвЂљР В°
			if ((ret = socket(sn_dst, Sn_MR_UDP, port + 1, 0x00, DST))
					!= sn_dst)
				return ret;
			UART_Printf("%d:Opened\r\n", sn_dst);
			break;
		default:
			break;
		}
	} //else {
//		uint8_t tmp_dst = 0;
//		if (ctlwizchip(CW_GET_PHYLINK, (void*) &tmp_dst, DST) == -1)
//			UART_Printf("Unknown PHY Link status.\r\n"); //Р СњР ВµР С‘Р В·Р Р†Р ВµРЎРѓРЎвЂљР Р…РЎвЂ№Р в„– РЎРѓРЎвЂљР В°РЎвЂљРЎС“РЎРѓ
//		if (tmp_dst == PHYCFGR_LNK_ON)
//			dst_connected = 1;
//		else
//			return 1;
//
//		uint8_t rx_tx_buff_sizes_dst[] = { 16, 0, 0, 0, 0, 0, 0, 0 };
//		uint8_t tmpstr_dst[6] = { 0, };
//
//		if (dst_con) {
//			wizchip_init(rx_tx_buff_sizes_dst, rx_tx_buff_sizes_dst, DST);
//			ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO_dst, DST);
//			// Display Network Information
//			ctlwizchip(CW_GET_ID, (void*) tmpstr_dst, DST);
//		}
//	}
	return 1;

}

void critical_enter(void)
{
  __disable_irq();
}

void critical_exit(void)
{
  __enable_irq();
}

void UART_Printf(const char* fmt, ...) {
	char buff[256];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buff, sizeof(buff), fmt, args);
	//HAL_UART_Transmit(UART_WIZCHIP, (uint8_t*) buff, strlen(buff), 10);
	va_end(args);
}

/* time = 5*time (ex: time =0x1, keep-alive every 5 sec*/
int8_t keep_alive(uint8_t socket_numb, uint8_t* time,CH_WIZNET base ) {
	int8_t reter = 0;
	setSn_CR(socket_numb, Sn_CR_SEND_KEEP, base);
	getSn_CR(socket_numb, base);
	if (getSn_IR(socket_numb,base) & Sn_IR_TIMEOUT) {
		setSn_IR(socket_numb, Sn_IR_TIMEOUT, base);
		return SOCKERR_TIMEOUT;
	}
	//if(time == 0) reter = setsockopt(socket_numb, 6, (uint8_t*)0);
	//else if(time!=0) reter = setsockopt(socket_numb, SO_KEEPALIVEAUTO, (uint8_t*)time);

	return reter;
}
