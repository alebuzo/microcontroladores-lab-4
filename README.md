# microcontroladores-lab-4

Para correr este ejemplo es necesario que la carpeta "src" se encuentre dentro de la ruta de ejemplos 
de la libreria libopencm3, de manera especifica para el modelo de tarjeta utilizado que en este caso es
 
** /libopencm3-examples/examples/stm32/f4/stm32f429i-discovery/src*

Luego de esto es necesario correr la linea 

**st-flash --reset write lab4.bin 0x8000000*

En caso de realizar cambios al archivo lab4.c, se deben correr las siguientes 3 lineas en la terminal :

1- make

2 - arm-none-eabi-objcopy -O binary lab4.elf  lab4.bin

3 - st-flash --reset write lab4.bin 0x8000000



Para el script de python, es necesario correr el script en la terminal como 

python script.py
python3 script.py
