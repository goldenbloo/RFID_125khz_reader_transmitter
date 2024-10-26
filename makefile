CC=avr-gcc
OBJCOPY=avr-objcopy
AVRSIZE=avr-size

MCU=atmega8
F_CPU=8000000UL
INCLUDE_DIR=C:/msys64/ucrt64/avr/include
CFLAGS=-mmcu=$(MCU) -DF_CPU=$(F_CPU) -I$(INCLUDE_DIR) -Os -gdwarf-2

.PHONY: all clean rebuild

all: main.hex

main.elf: main.c
	$(CC) $(CFLAGS) -o $@ $^
	$(AVRSIZE) -C --mcu=$(MCU) $@

main.hex: main.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

clean:
	-del main.elf main.hex

rebuild: clean all
