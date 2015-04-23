#!/bin/sh

filename=$1
shift
MCU=$1
CHIP=`echo $MCU | tr '[:lower:]' '[:upper:]'`
shift
name="${filename%.*}"
ext="${filename##*.}"
echo "FILE=${name}"
echo "Chip=$CHIP"
echo msp430-gcc -Wall -g -ggdb -O2 -mmcu=${MCU} -DCHIP=__${CHIP}__ $filename -o $name.elf $@
	 msp430-gcc -Wall -g -ggdb -O2 -mmcu=${MCU} -DCHIP=__${CHIP}__ $filename -o $name.elf $@
if [ -e "$name.elf" ]; then
	msp430-objdump -S -D $name.elf > $name.lst
fi
