//     Universidad de Costa Rica
// Laboratorio de Microcontroladores
// Laboratorio 4 - Sismografo
// Estudiantes;  Raquel Corrales Marín B92378
//               Alexa Carmona Buzo B91643        
// Febrero 2022.


// IMPORTANTE MENCIONAR QUE EL DESARROLLO E IMPLEMENTACION DE LA SOLUCION FUE REALIZADA, 
// TOMANDO POR PUNTO DE PARTIDA LOS EJEMPLOS A DISPOSICION EN LA LIBRERIA LIBOPENCM3
// PRINCIPALMENTE EL SPI Y LCD-SPI, ASI COMO EL BLINKING 

// SE DECALRAN LAS BIBLIOTECAS Y ARCHIVOS REQUERIDOS
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "clock.h"
#include "console.h"
#include "sdram.h"
#include "lcd-spi.h"
#include <errno.h>
#include "gfx.h"
#include "rcc.h"
#include "adc.h"
#include "dac.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"
#include <errno.h>
#include <unistd.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>


//PARAMETROS DEL GIROSCOPIO SIGUIENDO LOS EJEMPLOS DE LA LIBRERIA (STM32/F4/STM32F3-DISCOVERY/SPI)
#define GYR_RNW			(1 << 7) /* Write when zero */
#define GYR_MNS			(1 << 6) /* Multiple reads when 1 */
#define GYR_WHO_AM_I		0x0F
#define GYR_OUT_TEMP		0x26
#define GYR_STATUS_REG		0x27
#define GYR_CTRL_REG1		0x20
#define GYR_CTRL_REG1_PD	(1 << 3)
#define GYR_CTRL_REG1_XEN	(1 << 1)
#define GYR_CTRL_REG1_YEN	(1 << 0)
#define GYR_CTRL_REG1_ZEN	(1 << 2)
#define GYR_CTRL_REG1_BW_SHIFT	4
#define GYR_CTRL_REG4		0x23
#define GYR_CTRL_REG4_FS_SHIFT	4

#define GYR_OUT_X_L		0x28
#define GYR_OUT_X_H		0x29
#define GYR_OUT_Y_L		0x2A
#define GYR_OUT_Y_H		0x2B
#define GYR_OUT_Z_L		0x2C
#define GYR_OUT_Z_H		0x2D

#define L3GD20_SENSITIVITY_250DPS  (0.00875F)  
//SE DEFINEN LAS VARIABLES DE LOS EJES X, Y Y Z
typedef struct Gyro {
  int16_t x;
  int16_t y;
  int16_t z;
} gyro;

int print_decimal(int);
gyro read_xyz(void);

static void spi_setup(void)
{
	rcc_periph_clock_enable(RCC_SPI5);
	/* For spi signal pins */
    	rcc_periph_clock_enable(RCC_GPIOC);
    	/* For spi mode select on the l3gd20 */
	rcc_periph_clock_enable(RCC_GPIOF);

   	/* Setup GPIOE3 pin for spi mode l3gd20 select. */
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPIO1);
	gpio_set(GPIOC, GPIO1);
	gpio_mode_setup(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE,GPIO7 | GPIO8 | GPIO9);   
	gpio_set_af(GPIOF, GPIO_AF5, GPIO7 | GPIO8 | GPIO9);
    	//spi initialization;
    	spi_set_master_mode(SPI5);
	spi_set_baudrate_prescaler(SPI5, SPI_CR1_BR_FPCLK_DIV_64);
	spi_set_clock_polarity_0(SPI5);
	spi_set_clock_phase_0(SPI5);
	spi_set_full_duplex_mode(SPI5);
	spi_set_unidirectional_mode(SPI5); /* bidirectional but in 3-wire */
	spi_enable_software_slave_management(SPI5);
	spi_send_msb_first(SPI5);
	spi_set_nss_high(SPI5);
    	SPI_I2SCFGR(SPI5) &= ~SPI_I2SCFGR_I2SMOD;  
	spi_enable(SPI5);
	
	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_CTRL_REG1); 
	spi_read(SPI5);
	spi_send(SPI5, GYR_CTRL_REG1_PD | GYR_CTRL_REG1_XEN | GYR_CTRL_REG1_YEN | GYR_CTRL_REG1_ZEN | (3 << GYR_CTRL_REG1_BW_SHIFT));
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1); 
    	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_CTRL_REG4);
	spi_read(SPI5);
	spi_send(SPI5, (1 << GYR_CTRL_REG4_FS_SHIFT));
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);
}

static void usart_setup(void)
{
	/* Setup GPIO pin GPIO_USART2_TX/GPIO9 on GPIO port A for transmit. */
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);	
	gpio_set_af(GPIOA, GPIO_AF7, GPIO9);
	
	/* Setup UART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	
	/* Finally enable the USART. */
	usart_enable(USART1);
}


// FUNCION PARA LEER LAS COORDENADAS DEL GIROSCOPIO
gyro read_xyz(void)
{
	gyro lectura;
	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_WHO_AM_I | 0x80);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_STATUS_REG | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_TEMP | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_X_L | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.x = spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_X_H | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.x |=spi_read(SPI5) << 8;
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_Y_L | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.y =spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_Y_H | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.y|=spi_read(SPI5) << 8;
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_Z_L | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.z=spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);

	gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, GYR_OUT_Z_H | GYR_RNW);
	spi_read(SPI5);
	spi_send(SPI5, 0);
	lectura.z|=spi_read(SPI5) << 8;
	gpio_set(GPIOC, GPIO1);

	lectura.x = lectura.x*L3GD20_SENSITIVITY_250DPS;
	lectura.y = lectura.y*L3GD20_SENSITIVITY_250DPS;
	lectura.z = lectura.z*L3GD20_SENSITIVITY_250DPS;
	return lectura;
}

static void gpio_setup(void)
{
	//Se habilitan los relojes
	rcc_periph_clock_enable(RCC_GPIOG);

	rcc_periph_clock_enable(RCC_GPIOA);

	/* Set GPIO0 (in GPIO port A) to 'input open-drain'. */
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);
	/* Set GPIO13 (in GPIO port G) to 'output push-pull'. */
	gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
			/* Set GPIO14 (in GPIO port G) to 'output push-pull'. */
	gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
}

int print_decimal(int num)
{
	/*
	int len = print_decimal(int value)
	Funcion para emprimir enteros en consola
	*/
	int		ndx = 0;
	char	buf[10];
	int		len = 0;
	char	is_signed = 0;

	if (num < 0) {
		is_signed++;
		num = 0 - num;
	}
	buf[ndx++] = '\000';
	do {
		buf[ndx++] = (num % 10) + '0';
		num = num / 10;
	} while (num != 0);
	ndx--;
	if (is_signed != 0) {
		console_putc('-');
		len++;
	}
	while (buf[ndx] != '\000') {
		console_putc(buf[ndx--]);
		len++;
	}
	return len; /* number of characters printed */
}


// SE TOMO COMO REFERENCIA EL EJEMPLO CONTENIDO EN 
//libopencm3-examples/examples/stm32/f4/stm32f429i-discovery/adc-dac-printf/adc-dac-printf.c

static void adc_setup(void)
{
	//SE DECLARA EL PUERTO PA3 DONDE SE TOMA LA LECTURA DE LA BATERIA 
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO3);
	adc_power_off(ADC1);
	adc_disable_scan_mode(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_3CYC);
	adc_power_on(ADC1);

}

static uint16_t read_adc_naiive(uint8_t channel)
{
	uint8_t channel_array[16];
	channel_array[0] = channel;
	adc_set_regular_sequence(ADC1, 1, channel_array);
	adc_start_conversion_regular(ADC1);
	while (!adc_eoc(ADC1));
	uint16_t reg16 = adc_read_regular(ADC1);
	return reg16;
}


int main(void)
{	

	gyro lectura;
	float bateria_lvl;
	char data[10];
	char print_x[5];
	char print_y[5];
	char print_z[5];
	char print_bat[5];
	bool enviar = false; 
	uint16_t input_adc0;
	console_setup(115200);
	clock_setup();
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_ADC1);
	gpio_setup();
	adc_setup();
	sdram_init();
	usart_setup();
	spi_setup();
	lcd_spi_init();
	gfx_init(lcd_draw_pixel, 240, 320);

	while (1) {
	
		
		// SE AGREGA FORMATO A LA INFORMACION QUE VA A SER DESPLEGADA Y ENVIADA
		sprintf(print_x, "%s", "X:");
		sprintf(data, "%d", lectura.x);
		strcat(print_x, data);
		sprintf(print_y, "%s", "Y:");
		sprintf(data, "%d", lectura.y);
		strcat(print_y, data);
		sprintf(print_z, "%s", "Z:");
		sprintf(data, "%d", lectura.z);
		strcat(print_z, data);
		sprintf(print_bat, "%s", "");
		sprintf(data, "%f", bateria_lvl);
		strcat(print_bat, data);
		
		// SE MUESTRA LA INFORMACION EN LA PANTALLA
		gfx_fillScreen(LCD_WHITE);
		gfx_setTextColor(LCD_BLACK, LCD_WHITE);
		gfx_setTextSize(2);			
		gfx_setCursor(23, 30);
		gfx_puts(" Sismografo ");		

		// SE IMPRIMEN LOS EJES
		gfx_setCursor(20, 80);
		gfx_puts("Eje ");
		gfx_puts(print_x);
		
		gfx_setCursor(20, 120);
		gfx_puts("Eje ");
		gfx_puts(print_y);

		gfx_setCursor(20, 160);
		gfx_puts("Eje ");
		gfx_puts(print_z);


		// SE IMPRIME LA INFORMACION DE LA BATERIA
		gfx_setCursor(5, 240);
		gfx_puts("Bateria: ");
		gfx_setCursor(5, 270);
		gfx_puts(print_bat);
		gfx_puts(" V");

		// SE IMPRIME UNA LINEA PARA INDICAR SI SE ESTA TRANSMITIENDO O NO
		gfx_setCursor(3, 200);
		gfx_puts("Trasmitiendo: ");

		// SE INDICA EN PANTALLA SI LA TRANSMISION ESTA HABILITADA O NO
		if (enviar){
			gfx_setCursor(205, 200);
			gfx_puts("Si");
		}
		else{
			gfx_setCursor(205, 200);
			gfx_puts("No");
		}
		lcd_show_frame();
		
		
		//Enviar datos
		lectura = read_xyz();
		gpio_set(GPIOC, GPIO1);
		// SE LEE EL PUERTO PA3 
		input_adc0 = read_adc_naiive(3);
		
		// sE CALCULA EL NIVEL DE LA BATERIA
		bateria_lvl = (input_adc0*9)/4095;
		
		// SI LA TRANSMISION ESTA HABILITADA SE ENVIA LA INFORMACION
		if (enviar)
		{
			print_decimal(lectura.x);
			console_puts("\t");
       	 		print_decimal(lectura.y);
			console_puts("\t");
        		print_decimal(lectura.z); 
			console_puts("\t");
			print_decimal(bateria_lvl); 
			console_puts("\n");
			gpio_toggle(GPIOG, GPIO13); //SE HACE BLINKING EN EL LED DE LA TRANSMISION
		}
		else{
			gpio_clear(GPIOG, GPIO13); //SE APAGA EL LED DE LA TRANSMISION
		}

		// SE INCLUYO EL LED PARA INDICAR EL NIVEL DE LA BATERIA, SI 
		// ESTE ES MAYOR A 7V se apaga, caso contrario hace blinking
		if (bateria_lvl<7)
		{
			gpio_toggle(GPIOG, GPIO14); // SE HACE BLINKING EN EL LED DE LA BATERIA
		}

		else gpio_clear(GPIOG, GPIO14); //  LED DE LA BATERIA APAGADO
		
		if (gpio_get(GPIOA, GPIO0)) {
			if (enviar) {
				enviar = false;
				gpio_clear(GPIOG, GPIO13); //SE APAGA EL LED DE LA TRANSMISION
				}
			else enviar = true;
		}

		int i;
		for (i = 0; i < 80000; i++)    /* Wait a bit. */
			__asm__("nop");
		
	}
	return 0;
}
