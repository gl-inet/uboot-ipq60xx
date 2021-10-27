/*
 * Control GPIO pins on the fly
 *
 * Copyright (c) 2008-2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <asm/io.h>
#include <common.h>
#include <asm/types.h>
#include <asm/arch-qca-common/gpio.h>

int do_gpio(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	unsigned int config_val = 0;
	unsigned int val = 0;
	int i = 0;
	unsigned int *addr;
	for(i = 0; i < 90; i++){
		addr = (unsigned int *)GPIO_CONFIG_ADDR(i);
		config_val=readl(addr);
		//if((config_val&(1<<2))==0){ 
		addr = (unsigned int *)GPIO_IN_OUT_ADDR(i);
		val = readl(addr);
		printf("gpio%2d, config reg: %8x, gpio_func: %d, oe: %d, value: %d\n",i, config_val, (config_val>>2)&3, (config_val>>9)&1, val);
		//}
	}
	return 0;
}

U_BOOT_CMD(
	gpio,	CONFIG_SYS_MAXARGS,	0,	do_gpio,
	"print gpio direction and value",
	"\n"
);
