
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : EC200U Debug Firmware
 *
 * USART1 -> EC200U-CN
 * USART6 -> Debug Terminal (PuTTY)
 *
 * STM32F446RE
 ******************************************************************************
 */

#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=========================================================
                    FUNCTION DECLARATIONS
=========================================================*/

void USART1_Init(void);
void USART6_Init(void);

void USART1_WriteChar(char ch);
void USART6_WriteChar(char ch);

void USART1_SendString(char *str);
void USART6_SendString(char *str);

char USART1_ReadChar(void);

void EC200U_SendCommand(char *cmd);

void delay(volatile uint32_t time);

/*=========================================================
                    GLOBAL VARIABLES
=========================================================*/

char rx_buffer[300];

char latitude[20];
char longitude[20];
char json_payload[200]; //Buffer for JSON Format
volatile uint16_t rx_index = 0;

/*===13.05.2026 Add iteration===*/
float latitude_decimal;
float longitude_decimal;



/*=========================================================
        CONVERT GPS TO DECIMAL DEGREES
=========================================================*/


void ConvertToDecimal(char *coord, char direction, int *degree, int *fraction)
{
    long long raw_value;

    long long deg;

    long long minutes_scaled;

    long long decimal_scaled;

    char temp[20];

    int i;
    int j = 0;

    /*
        REMOVE DECIMAL POINT  Example: 1101.4081 becomes 11014081
    */

    for(i = 0; coord[i] != '\0'; i++)
    {
        if(coord[i] != '.')
        {
            temp[j++] = coord[i];
        }
    }

    temp[j] = '\0';

    raw_value = atoll(temp);

    /*===EXTRACT DEGREES Latitude: ddmm.mmmm Longitude: dddmm.mmmm===*/

    deg = raw_value / 1000000;

    /*===EXTRACT MINUTES===*/

    minutes_scaled = raw_value - (deg * 1000000);

    /*=== CONVERT TO DECIMAL DEGREE decimal = minutes / 60===*/

    decimal_scaled = (minutes_scaled * 1000000LL) / 600000LL;

    /*===HANDLE SOUTH/WEST===*/

    if(direction == 'S' || direction == 'W')
    {
        deg = -deg;
    }

    *degree = (int)deg;

    *fraction = (int)decimal_scaled;
}
/*===TIM_2 INIT===*/
void TIM2_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Enable clock for timer 2 peripheral
	/*Timer clock=16MHz, Prescalar = 16-1, Timer Tick = 1MHz 1 Tick = 1us*/
	TIM2->PSC = 16-1;
	TIM2->ARR = 0XFFFFFFFF;//Auto Reload Max
	TIM2->CNT = 0;//Reset the counter
	TIM2->CR1 |= TIM_CR1_CEN;
}
/*===Microsecond Delay===*/
void Delay_us(uint32_t us)
{
	TIM2->CNT = 0;
	while(TIM2->CNT < us);
}
/*===Milli Second Delay===*/
void Delay_ms(uint32_t ms)
{
	while(ms--)
	{
		Delay_us(1000);
	}
}
/*=========================================================
                    MAIN
=========================================================*/

int main(void)
{
    USART1_Init();     // EC200U
    USART6_Init();     // Debug UART
    TIM2_Init();	   // Tim2_Init
    //USART6->SR &= ~(1 << 5);
    USART6_SendString("\r\n================================");
    USART6_SendString("\r\n EC200U DEBUG STARTED ");
    USART6_SendString("\r\n================================\r\n");

    //delay(1000000);
    Delay_ms(250);

    /* MODEM CHECK */
    EC200U_SendCommand("AT\r\n");

    //delay(10000000);
    Delay_ms(2500);

    /* DISABLE ECHO */
    EC200U_SendCommand("ATE0\r\n");

    //delay(10000000);
    Delay_ms(2500);

    /* ENABLE GPS */
    EC200U_SendCommand("AT+QGPS=1\r\n");

    //delay(30000000);
    Delay_ms(5000);
    while(1)
    {
        /* CHECK GPS STATUS */
        EC200U_SendCommand("AT+QGPS?\r\n");

        //delay(10000000);
        Delay_ms(2500);
        /* GET GPS LOCATION */
        EC200U_SendCommand("AT+QGPSLOC=0\r\n");

        //delay(30000000);
        Delay_ms(15000);
    }
}

/*=========================================================
                SEND COMMAND + RECEIVE RESPONSE
=========================================================*/

void EC200U_SendCommand(char *cmd)
{
    char data;
    uint32_t timeout = 0;

    /* CLEAR BUFFER */
    memset(rx_buffer, 0, sizeof(rx_buffer));

    rx_index = 0;

    /* PRINT TX COMMAND */
    USART6_SendString("\r\nTX -> ");
    USART6_SendString(cmd);

    /* SEND COMMAND TO MODEM */
    USART1_SendString(cmd);

    /* WAIT COMPLETE TRANSMISSION */
    //while(!(USART1->SR & USART_SR_TC));

    /* MODEM RESPONSE DELAY */
    //(5000000);

    USART6_SendString("\r\nRX <- ");

    /*=========================================
            RECEIVE RESPONSE
    =========================================*/

    while(timeout < 500000) //500000
    {
        if(USART1->SR & USART_SR_RXNE)
        {
            /* READ DATA */
            data = USART1_ReadChar();

            /* STORE IN BUFFER */
            rx_buffer[rx_index++] = data;

            /* NULL TERMINATE */
            rx_buffer[rx_index] = '\0';

            /* PRINT ON DEBUG UART */
            USART6_WriteChar(data);

            /* RESET TIMEOUT */
            timeout = 0;

            /* PREVENT OVERFLOW */
            if(rx_index >= sizeof(rx_buffer)-1)
            {
                break;
            }
        }
        else
        {
            timeout++;
        }
    }

    USART6_SendString("\r\n");

    /*=========================================
                GPS PARSING
    =========================================*/

//    if(strstr(rx_buffer, "+QGPSLOC"))
//    {
//        sscanf(rx_buffer,
//               "%*[^:]: %*[^,],%[^,],%[^,]",
//               latitude,
//               longitude);
//
//        USART6_SendString("\r\n====================");
//        USART6_SendString("\r\nPARSED GPS DATA");
//        USART6_SendString("\r\n====================");
//
//        USART6_SendString("\r\nLatitude  : ");
//        USART6_SendString(latitude);
//
//        USART6_SendString("\r\nLongitude : ");
//        USART6_SendString(longitude);
//
//        USART6_SendString("\r\n====================\r\n");
//    }

    if(strstr(rx_buffer, "+QGPSLOC"))
    {
        char lat_dir;
        char lon_dir;

        char lat_string[20] = {0};
        char lon_string[20] = {0};

        char print_buffer[100];

        int lat_deg;
        int lat_frac;

        int lon_deg;
        int lon_frac;

        sscanf(rx_buffer,
               "%*[^:]: %*[^,],%15[0-9.]%c,%15[0-9.]%c",
               lat_string,
               &lat_dir,
               lon_string,
               &lon_dir);

        USART6_SendString("\r\n====================");
        USART6_SendString("\r\nRAW GPS DATA");
        USART6_SendString("\r\n====================");

        USART6_SendString("\r\nLatitude String  : ");
        USART6_SendString(lat_string);

        USART6_SendString("\r\nLongitude String : ");
        USART6_SendString(lon_string);

        /*=========================================
                CONVERT GPS FORMAT
        =========================================*/

        ConvertToDecimal(lat_string,lat_dir,&lat_deg,&lat_frac);

        ConvertToDecimal(lon_string,lon_dir,&lon_deg,&lon_frac);

        /*=========================================
                    PRINT DATA
        =========================================*/

        USART6_SendString("\r\n\r\n====================");
        USART6_SendString("\r\nDECIMAL GPS DATA");
        USART6_SendString("\r\n====================");

        sprintf(print_buffer,"\r\nLatitude  : %d.%06d", lat_deg, lat_frac);

        USART6_SendString(print_buffer);

        sprintf(print_buffer, "\r\nLongitude : %d.%06d", lon_deg, lon_frac);

        USART6_SendString(print_buffer);

        USART6_SendString("\r\n====================\r\n");

        /*===Json Payload===*/
        sprintf(json_payload, "{\"latitude\":\"%d.%06d\",""\"longitude\":\"%d.%06d\"}",lat_deg, lat_frac, lon_deg, lon_frac);

        USART6_SendString("\r\n====================");
        USART6_SendString("\r\nJSON PAYLOAD");
        USART6_SendString("\r\n====================\r\n");

        USART6_SendString(json_payload);

        USART6_SendString("\r\n====================\r\n");
    }
}

/*=========================================================
                    USART1 INIT
                    PA9  -> TX
                    PA10 -> RX
=========================================================*/

void USART1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->MODER &= ~(0xF << 18);

    GPIOA->MODER |= (0xA << 18);

    GPIOA->AFR[1] &= ~(0xFF << 4);

    GPIOA->AFR[1] |= (0x77 << 4);

    /* 9600 Baud @16MHz */ //115200
    USART1->BRR = 0x008B;

    USART1->CR1 |= USART_CR1_TE;
    USART1->CR1 |= USART_CR1_RE;

    USART1->CR1 |= USART_CR1_UE;
}

/*=========================================================
                    USART6 INIT
                    PC6 -> TX
                    PC7 -> RX
=========================================================*/

void USART6_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;

    GPIOC->MODER &= ~(0xF << 12);

    GPIOC->MODER |= (0xA << 12);

    GPIOC->AFR[0] &= ~(0xFF << 24);

    GPIOC->AFR[0] |= (0x88 << 24);

    /* 9600 Baud @16MHz */
    USART6->BRR = 0x008B;

    USART6->CR1 |= USART_CR1_TE;
    USART6->CR1 |= USART_CR1_RE;

    USART6->CR1 |= USART_CR1_UE;
}

/*=========================================================
                    USART1 WRITE
=========================================================*/

void USART1_WriteChar(char ch)
{
    while(!(USART1->SR & USART_SR_TXE));

    USART1->DR = ch;
}

/*=========================================================
                    USART6 WRITE
=========================================================*/

void USART6_WriteChar(char ch)
{
    while(!(USART6->SR & USART_SR_TXE));

    USART6->DR = ch;
}

/*=========================================================
                    USART1 SEND STRING
=========================================================*/

void USART1_SendString(char *str)
{
    while(*str)
    {
        USART1_WriteChar(*str++);

        while(!(USART1->SR & USART_SR_TC));
    }
}

/*=========================================================
                    USART6 SEND STRING
=========================================================*/

void USART6_SendString(char *str)
{
    while(*str)
    {
        USART6_WriteChar(*str++);
    }
}

/*=========================================================
                    USART1 READ
=========================================================*/

char USART1_ReadChar(void)
{
    while(!(USART1->SR & USART_SR_RXNE));

    /* HANDLE OVERRUN ERROR */

    if(USART1->SR & USART_SR_ORE)
    {
        volatile char temp;

        temp = USART1->DR;

        (void)temp;
    }

    return USART1->DR;
}

/*=========================================================
                        DELAY
=========================================================*/

void delay(volatile uint32_t time)
{
    while(time--);
}
