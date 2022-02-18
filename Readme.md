# ioLibrary Driver
Драйвер библиотеки был взят с оффициального сайта  и видоизменен[W5500](http://wizwiki.net/wiki/doku.php?id=products:w5500:start).
Видоизменения коснулись модификации работы с двумя W5500 на одном микроконтроллере(в данном случае STM32).
## Интерфейсы:
- Internet :
  - DHCP client
  - DNS client
  - FTP client
  - FTP server
  - SNMP agent/trap
  - SNTP client
  - TFTP client
  - HTTP server
  - MQTT Client
  
  ##Описание
  Вся настройка W5500 осуществляется в файле wizchip_init.c
  В этой библиотеке выбор w5500 осуществляется выбором параметра структуры, определенной в файле ```c #include "wizchip_conf.h" ``` и имеет вид:
  ```c
	typedef enum Choose_WizNet{
		SRC,
		DST
	}CH_WIZNET;
```
  
  ### Фунуции приема-передачи данных
  Для передачи данных необходимо объявить макрос в соотвествии с выбранной шиной микроконтроллера, т.е. изменить в файле #include "wizchip_init.h" SPI_WIZCHIP_src и SPI_WIZCHIP_dst
  ```c
	extern SPI_HandleTypeDef hspi1;
	#define SPI_WIZCHIP_src  			&hspi1

	extern SPI_HandleTypeDef hspi2;
	#define SPI_WIZCHIP_dst				&hspi2
  ```
  
  Так общение с микросхемой W5500 идет по SPI, необходимо определить две функции cheap select. Далее все функции для микросхем будут написано с макросом xxx, Где вместо xxx src или dst, в зависимости от выбора W5500.
  ```c
void wizchip_select_xxx(void) {
	GPIOA->BSRR = GPIO_BSRR_BR4;
}

void wizchip_deselect_xxx(void) {
	GPIOA->BSRR = GPIO_BSRR_BS4;
}
```
Вместо GPIO_BSRR_BS4 написать свой PIN CS.
 Для приема и отправки данных необходимо определить функции для отправки 1 байти и для множества байт:
 
 ```c 
 void wizchip_write_xxx(uint8_t wb)    //Write SPI
{
	HAL_SPI_Transmit(SPI_WIZCHIP_xxx, &wb, 1, HAL_MAX_DELAY);
}

uint8_t wizchip_read_xxx() //Read SPI
{
	uint8_t spi_read_buf;
	HAL_SPI_Receive(SPI_WIZCHIP_xxx, &spi_read_buf, 1, HAL_MAX_DELAY);
	return spi_read_buf;
}

void wizchip_read_burst_src(uint8_t* pBuf, uint16_t len) //Read SPI
{
#ifdef SPI_DMA
	HAL_SPI_ReceiveDMA(SPI_WIZCHIP_xxx, pBuf, len, HAL_MAX_DELAY);
	while(HAL_SPI_GetState(SPI_WIZCHIP_xxx) == HAL_SPI_STATE_BUSY_RX);
#else
	HAL_SPI_Receive(SPI_WIZCHIP_xxx, pBuf, len, HAL_MAX_DELAY);
#endif
}

void wizchip_write_burst_src(uint8_t* pBuf, uint16_t len) //Read SPI
{
#ifdef SPI_DMA
	HAL_SPI_Transmit_DMA(SPI_WIZCHIP_xxx, pBuf, len);
	while(HAL_SPI_GetState(SPI_WIZCHIP_xxx) == HAL_SPI_STATE_BUSY_TX);
#else
	HAL_SPI_Transmit(SPI_WIZCHIP_xxx, pBuf, len, HAL_MAX_DELAY);
#endif
}
```
Чтобы использовать для передачи данных DMA, необходимо определить макрос #define SPI_DMA

Функции определяются вызовом API:

```c
	/* Chip selection call back */
	reg_wizchip_cs_cbfunc(wizchip_select_src, wizchip_deselect_src, SRC);

	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_select); // CS must be tried with LOW.
	/* SPI Read & Write callback function */

	reg_wizchip_spi_cbfunc(wizchip_read_src, wizchip_write_src, SRC);
	reg_wizchip_spiburst_cbfunc(wizchip_read_burst_src, wizchip_write_burst_src, SRC);
	```

### Критические секции
Критические секции необхоидмы, чтобы избежать нарушения логики работы W5500 в отвественных местах:
Функции:
```c 
void critical_enter(void)
{
  __disable_irq();
}

void critical_exit(void)
{
  __enable_irq();
}
```
Эти функции могут не использоваться!!!
```c
reg_wizchip_cris_cbfunc(critical_enter, critical_exit, SRC);
```

## Отладка
Для отладки и вывода информации на экран необходимо использовать макрос #define DEBUG_W5500
Это будет вызывать функцию:
```c 
void UART_Printf(const char* fmt, ...) {
	char buff[256];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buff, sizeof(buff), fmt, args);
	HAL_UART_Transmit(UART_WIZCHIP, (uint8_t*) buff, strlen(buff), 10);
	va_end(args);
}
```
Отладка осуществляется по UART. ЧТобы использовать стандартную функцию языка C printf, Нужно использовать для отладки SWO (об этом должен позаботиться разводящий плату).

##Network
Сетевая насйтрока задается в структуре:

```c
/* Mac, IP ... W5500*/
static wiz_NetInfo gWIZNETINFO_src = { .mac = { 0x18, 0xCF, 0x5E, 0x53, 0xBF,
		0x30 }, .ip = { 169, 254, 153, 204 }, .sn = { 255, 255, 0, 0 }, .gw = {
		0, 0, 0, 0 }, .dns = { 0, 0, 0, 0 }, .dhcp = NETINFO_STATIC };
```

В структуре udp_NetInfo сохраняются IP и порт отправителя при UDP:
```c
udp_NetInfo udpNetInfo_dst = { .destip = { 169, 254, 153, 204 }};
```