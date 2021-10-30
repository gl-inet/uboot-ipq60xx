/*
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000, 2001 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include <asm/byteorder.h>
#include "httpd.h"

#include "../httpd/uipopt.h"
#include "../httpd/uip.h"
#include "../httpd/uip_arp.h"
#include <gl_api.h>

// extern flash_info_t flash_info[];

static int arptimer = 0;

struct in_addr net_httpd_ip;

// start http daemon
void HttpdStart(void){
	struct uip_eth_addr eaddr;
	unsigned short int ip[2];
	ulong tmp_ip_addr = ntohl(net_ip.s_addr);
	printf( "HTTP server is starting at IP: %ld.%ld.%ld.%ld\n", ( tmp_ip_addr & 0xff000000  ) >> 24, ( tmp_ip_addr & 0x00ff0000  ) >> 16, ( tmp_ip_addr & 0x0000ff00  ) >> 8, ( tmp_ip_addr & 0x000000ff));
	eaddr.addr[0] = net_ethaddr[0];
	eaddr.addr[1] = net_ethaddr[1];
	eaddr.addr[2] = net_ethaddr[2];
	eaddr.addr[3] = net_ethaddr[3];
	eaddr.addr[4] = net_ethaddr[4];
	eaddr.addr[5] = net_ethaddr[5];
	// set MAC address
	uip_setethaddr(eaddr);

	uip_init();
	httpd_init();

	ip[0] =  htons((tmp_ip_addr & 0xFFFF0000) >> 16);
	ip[1] = htons(tmp_ip_addr & 0x0000FFFF);

	uip_sethostaddr(ip);

	printf("done set host addr 0x%x 0x%x\n", uip_hostaddr[0], uip_hostaddr[1]);
	// set network mask (255.255.255.0 -> local network)
	ip[0] = htons((0xFFFFFF00 & 0xFFFF0000) >> 16);
	ip[1] = htons(0xFFFFFF00 & 0x0000FFFF);

	net_netmask.s_addr = 0x00FFFFFF;
	uip_setnetmask(ip);

	// should we also set default router ip address?
	//uip_setdraddr();
	do_http_progress(WEBFAILSAFE_PROGRESS_START);
	webfailsafe_is_running = 1;
}

void HttpdStop(void){
	webfailsafe_is_running = 0;
	webfailsafe_ready_for_upgrade = 0;
	webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;
	do_http_progress(WEBFAILSAFE_PROGRESS_UPGRADE_FAILED);
}
void HttpdDone(void){
	webfailsafe_is_running = 0;
	webfailsafe_ready_for_upgrade = 0;
	webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;
	do_http_progress(WEBFAILSAFE_PROGRESS_UPGRADE_READY);
}

#ifdef CONFIG_MD5
#include <u-boot/md5.h>

void printChecksumMd5(int address,unsigned int size)
{
	void *buf = (void*)(address);
	int i = 0;
	u8 output[16];
	md5_wd(buf, size, output, CHUNKSZ_MD5);
	printf("md5 for %08x ... %08x ==> ", address, address + size);
	for (i = 0; i < 16; i++)
		printf("%02x", output[i] & 0xFF);
	printf("\n");
}
#else
void printChecksumMd5(int address,unsigned int size)
{
}
#endif

int do_http_upgrade(const ulong size, const int upgrade_type){
	char buf[96];
	//printf checksum if defined
	printChecksumMd5(WEBFAILSAFE_UPLOAD_RAM_ADDRESS,size);

	if(upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_UBOOT){
		printf("\n\n****************************\n*     U-BOOT UPGRADING     *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		if(qca_smem_flash_info.flash_type==6)
			sprintf(buf,
				"sf probe && sf update 0x%lx 0x%lx 0x%lx",
				(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
				(unsigned long int)WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS,
				(unsigned long int)size);
		else if(qca_smem_flash_info.flash_type==2)
			sprintf(buf,
				"nand erase 0x%lx 0x%lx; nand write 0x%lx 0x%lx 0x%lx",
				(unsigned long int)WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS_NAND,
				(unsigned long int)WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES_NAND,
				(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
				(unsigned long int)WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS_NAND,
				(unsigned long int)((size/131072+(size%131072!=0))*131072));
	} else if(upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE){
		if(check_fw_type((void *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS)==FW_TYPE_NOR){
		//固件在nor的情况，不会发生
		printf("\n\n****************************\n*    FIRMWARE UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		sprintf(buf,
				"sf probe && sf update 0x%lx 0x%lx 0x%lx",
				(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
				(unsigned long int)WEBFAILSAFE_UPLOAD_FW_ADDRESS,
				(unsigned long int)size);
		}else if(check_fw_type((void *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS)==FW_TYPE_EMMC){
		//固件为emmc mbr分区的情况，不会发生
			printf("\n\n****************************\n*    FIRMWARE UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
			sprintf(buf,
					"mmc dev 0 && mmc erase 0 0x109800 && mmc write 0x%lx 0x%lx 0x%lx",
					(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
					(unsigned long int)0x0,
					(unsigned long int)(size/512+1));
		}else if(check_fw_type((void *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS)==FW_TYPE_QSDK){
			printf("\n\n****************************\n*    FIRMWARE UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
			sprintf(buf, "sf probe; imgaddr=0x%lx && source $imgaddr:script", (unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS);
		}else if(check_fw_type((void *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS)==FW_TYPE_UBI){
			printf("\n\n****************************\n*    FIRMWARE UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
			sprintf(buf, "nand erase 0xa00000 0x7300000; nand write 0x%lx 0xa00000 0x%lx", (unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS, (unsigned long int)size);
		}else{
			return(-1);
		}
	} else if(upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_ART){
		// TODO: add option to change ART partition offset,
		// for those who want to use OFW on router with replaced/bigger FLASH
		printf("\n\n****************************\n*      ART  UPGRADING      *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		if(qca_smem_flash_info.flash_type==6)
			sprintf(buf,
				"sf probe && sf update 0x%lx 0x%lx 0x%lx",
				(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
				(unsigned long int)WEBFAILSAFE_UPLOAD_ART_ADDRESS,
				(unsigned long int)size);
		else
			sprintf(buf,
				"nand erase 0x%lx 0x%lx; nand write 0x%lx 0x%lx 0x%lx",
				(unsigned long int)WEBFAILSAFE_UPLOAD_ART_ADDRESS_NAND,
				(unsigned long int)WEBFAILSAFE_UPLOAD_ART_SIZE_IN_BYTES_NAND,
				(unsigned long int)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
				(unsigned long int)WEBFAILSAFE_UPLOAD_ART_ADDRESS_NAND,
				(unsigned long int)((size/131072+(size%131072!=0))*131072));
	}
	else {
		return(-1);
	}

	printf("Executing: %s\n\n", buf);
	return(run_command(buf, 0));
}

// info about current progress of failsafe mode
int do_http_progress(const int state){
	//unsigned char i = 0;

	/* toggle LED's here */
	switch(state){
		case WEBFAILSAFE_PROGRESS_START:

			// // blink LED fast 10 times
			// for(i = 0; i < 10; ++i){
			// 	all_led_on();
			// 	milisecdelay(25);
			// 	all_led_off();
			// 	milisecdelay(25);
			// }

			printf("HTTP server is ready!\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_TIMEOUT:
			//printf("Waiting for request...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPLOAD_READY:
			printf("HTTP upload is done! Upgrading...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_READY:
			printf("HTTP ugrade is done! Rebooting...\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_FAILED:
			printf("## Error: HTTP ugrade failed!\n\n");

			// // blink LED fast for 4 sec
			// for(i = 0; i < 80; ++i){
			// 	all_led_on();
			// 	milisecdelay(25);
			// 	all_led_off();
			// 	milisecdelay(25);
			// }

			// wait 1 sec
			// milisecdelay(1000);

			break;
	}

	return(0);
}

void NetSendHttpd(void){

	volatile uchar *tmpbuf = net_tx_packet;
	int i;

	for(i = 0; i < 40 + UIP_LLH_LEN; i++){

		tmpbuf[i] = uip_buf[i];
	}

	for(; i < uip_len; i++){

		tmpbuf[i] = uip_appdata[i - 40 - UIP_LLH_LEN];
	}

	eth_send(net_tx_packet, uip_len);
}

void NetReceiveHttpd(volatile uchar * inpkt, int len){

	memcpy(uip_buf, (const void *)inpkt, len);
	uip_len = len;
	struct uip_eth_hdr * tmp = (struct uip_eth_hdr *)&uip_buf[0];
	if(tmp->type == htons(UIP_ETHTYPE_IP)){

		uip_arp_ipin();
		uip_input();

		if(uip_len > 0){

			uip_arp_out();
			NetSendHttpd();
		}
	} else if(tmp->type == htons(UIP_ETHTYPE_ARP)){

		uip_arp_arpin();

		if(uip_len > 0){

			NetSendHttpd();
		}
	}
}

void HttpdHandler(void){
	int i;

	for(i = 0; i < UIP_CONNS; i++){
		uip_periodic(i);

		if(uip_len > 0){
			uip_arp_out();
			NetSendHttpd();
		}
	}

	if(++arptimer == 20){
		uip_arp_timer();
		arptimer = 0;
	}
}
