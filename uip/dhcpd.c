/*
 * Copyright (c) 2015, mleaf mleaf90@gmail.com or 350983773@qq.com
 * All rights reserved.
 */

#include <linux/string.h>
#include <common.h>
#include <command.h>
#include <net.h>
#include <malloc.h>

#include "uip.h"
#include "dhcpd.h"
#include "timer.h"
#include "pt.h"

struct dhcp_client *head_dhcpd;
typedef struct dhcp_client NODE;

#define TRUE    1  
#define FALSE   0  

#define STATE_INITIAL         0
#define STATE_SENDING         1
#define STATE_DHCP_DISCOVER   2
#define STATE_DHCP_REQUEST    3
#define STATE_DHCP_OVER       4
#define STATE_DHCP_DISCOVER_OVER       5     

static struct dhcpd_state s;

struct dhcpd_msg {
  u8_t op, htype, hlen, hops;
  u8_t xid[4];//DHCP REQUEST 时产生的数值，以作 DHCPREPLY 时的依据
  u16_t secs;//Client 端启动时间（秒）
  u16_t flags;//从 0 到 15 共 16 bits ，最左一 bit 为 1 时表示 server 将以广播方式传送封包给 client ，其余尚未使用。
  u8_t ciaddr[4];//要是 client 端想继续使用之前取得之 IP 地址，则列于这里。
  u8_t yiaddr[4];//从 server 送回 client 之 DHCP OFFER 与 DHCPACK封包中，此栏填写分配给 client 的 IP 地址。
  u8_t siaddr[4];//若 client 需要透过网络开机，从 server 送出之 DHCP OFFER、DHCPACK、DHCPNACK封包中，此栏填写开机程序代码所在 server 之地址。
  u8_t giaddr[4];//若需跨网域进行 DHCP 发放，此栏为 relay agent 的地址，否则为 0。
  u8_t chaddr[16];//Client 硬件地址。
  u8_t sname[64];//Server 名称字符串，以 0x00 结尾。
  u8_t file[128];//若 client 需要透过网络开机，此栏将指出开机程序名称，稍后以 TFTP 传送。
  u8_t options[312];
};
/*
OP:
若是 client 送给 server 的封包，设为 1 ，反向为 2。
HTYPE:
硬件类别，Ethernet 为 1。
HLEN:
硬件地址长度， Ethernet 为 6。
HOPS:
若封包需经过 router 传送，每站加 1 ，若在同一网内，为 0。

options:
允许厂商定议选项（Vendor-Specific Area)，以提供更多的设定信息（如：Netmask、Gateway、DNS、等等）。
其长度可变，同时可携带多个选项，每一选项之第一个 byte 为信息代码，其后一个 byte 为该项数据长度，最后为项目内容。
CODE LEN VALUE 此字段完全兼容 BOOTP ，同时扩充了更多选项。其中，DHCP封包可利用编码为 0x53 之选项来设定封包类别：
项值类别:
1 DHCP DISCOVER
2 DHCP OFFER
3 DHCP REQUEST
4 DHCPDECLINE
5 DHCPACK
6 DHCPNACK
7 DHCPRELEASE
*/

extern char dhcpd_end;
#define BOOTP_BROADCAST 0x8000

#define DHCP_REQUEST        1
#define DHCP_REPLY          2
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6
#define DHCP_MSG_LEN      236

#define DHCPC_SERVER_PORT  67
#define DHCPC_CLIENT_PORT  68

#define DHCPDISCOVER  1
#define DHCPOFFER     2
#define DHCPREQUEST   3
#define DHCPDECLINE   4
#define DHCPACK       5
#define DHCPNAK       6
#define DHCPRELEASE   7
#define DHCPINFORM   8


#define DHCP_OPTION_SUBNET_MASK   1
#define DHCP_OPTION_ROUTER        3
#define DHCP_OPTION_DNS_SERVER    6
#define DHCP_OPTION_DOMAIN_NAME  15
#define DHCP_OPTION_REQ_IPADDR   50
#define DHCP_OPTION_LEASE_TIME   51
#define DHCP_OPTION_MSG_TYPE     53
#define DHCP_OPTION_SERVER_ID    54
#define DHCP_OPTION_REQ_LIST     55
#define DHCP_OPTION_END         255
#define DHCP_OPTION_OVER		0x34

static  u32_t uip_server_ipaddr = (192<<24)|(168<<16)|(1<<8)|(1);
static  u32_t uip_dhcpd_ipaddr = (1<<24)|(1<<16)|(168<<8)|(192);
static  u32_t uip_server_netmask = (255<<24)|(255<<16)|(255<<8)|(0);
static  u32_t uip_dhcp_leasetime = 1800;	//30min

/*---------------------------------------------------------------------------*/

/**
 * Convert an u32_t from host- to network byte order.
 *
 * @param n u32_t in host byte order
 * @return n in network byte order
 */
u32_t
uip_htonl(u32_t n)
{
      return ((n & 0xff) << 24) |
        ((n & 0xff00) << 8) |
        ((n & 0xff0000UL) >> 8) |
        ((n & 0xff000000UL) >> 24);
}

/**
 * Convert an u32_t from network- to host byte order.
 *
 * @param n u32_t in network byte order
 * @return n in host byte order
 */
u32_t
uip_ntohl(u32_t n)
{
  return uip_htonl(n);
}

/**
*add dhcp msg type
*/

static u8_t *
add_msg_type(u8_t *optptr, u8_t type)
{
  *optptr++ = DHCP_OPTION_MSG_TYPE;
  *optptr++ = 1;
  *optptr++ = type;
  return optptr;
}
/**
*add dhcp server id
*/

static u8_t *
add_dhcpd_server_id(u8_t *optptr)
{
  u32_t server_id;
  *optptr++ = DHCP_OPTION_SERVER_ID;
  *optptr++ = 4;
  server_id=uip_ntohl(uip_server_ipaddr);
  memcpy(optptr, &server_id, 4);
  return optptr + 4;
}
/**
*add dhcp default router
*/

static u8_t *
add_dhcpd_default_router(u8_t *optptr)
{
  u32_t default_router;
  *optptr++ = DHCP_OPTION_ROUTER;
  *optptr++ = 4;
  default_router=uip_ntohl(uip_server_ipaddr);
  memcpy(optptr, &default_router, 4);
  return optptr + 4;
}
/**
*add dhcp dns server
*/
static u8_t *
add_dhcpd_dns_server(u8_t *optptr)
{
  u32_t dns;
  *optptr++ = DHCP_OPTION_DNS_SERVER;
  *optptr++ = 4;
  dns=uip_ntohl(uip_server_ipaddr);
  memcpy(optptr, &dns, 4);
  return optptr + 4;
}
/**
*add dhcp domain name
*/
static u8_t *
add_dhcpd_domain_name(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_DOMAIN_NAME;
  *optptr++ = 4;
  memcpy(optptr, "GLnet", 6);
  return optptr + 4;
}
/**
*add dhcp lease time
*/
static u8_t *
add_dhcpd_lease_time(u8_t *optptr)
{
	u32_t leasetime;

	*optptr++ = DHCP_OPTION_LEASE_TIME;
	*optptr++ = 4;
	leasetime=uip_ntohl(uip_dhcp_leasetime);
	optptr = memcpy(optptr, &leasetime, 4);
	
  return optptr + 4;
}
/**
*add dhcp subnet mask
*/
static u8_t *
add_dhcpd_subnet_mask(u8_t *optptr)
{
  u32_t subnet_mask;
  *optptr++ = DHCP_OPTION_SUBNET_MASK;
  *optptr++ = 4;
  subnet_mask=uip_ntohl(uip_server_netmask);
  memcpy(optptr, &subnet_mask, 4);
  return optptr + 4;
}
/**
*add dhcp end msg
*/
static u8_t *
add_end(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_END;
  return optptr;
}

static u8_t *
add_end1(u8_t *optptr)
{
 char  pend[27]={0x1c ,0x04 ,0xc0 ,0xa8, 0x01 ,0xff ,0x03 ,0x04 ,0xc0 ,0xa8 ,0x01 ,0x01 ,0x06 ,0x04 ,0xc0,0xa8 ,0x01 ,0x01 ,0x0f ,0x03 ,0x6c ,0x61 ,0x6e ,0xff ,0x00 ,0x00 ,0x00};
 memcpy(optptr, pend, 27);
  return optptr +27 ;

}
/**
*show dhcp list
*/
void display_list(NODE *head) {
    NODE *p;
    for (p=head->next; p!=NULL; p=p->next)
	{
        printf("ipaddr: %d.%d.%d.%d\n",p->next->ipaddr.addr&0x000000ff,(p->next->ipaddr.addr&0x0000ff00)>>8,(p->next->ipaddr.addr&0x00ff0000)>>16,(p->next->ipaddr.addr&0xff000000)>>24);
	}
    printf("\n");
}

int find_vaild_hwaddr(NODE* head,char *HwAddress)
{
 NODE *p;
 p =head;
 while((p->next)&&(strcmp(p->next->hwaddr,HwAddress)))
     p = p->next;
 if(p->next)
 {
     printf("Found match ipaddr: %d.%d.%d.%d\n",p->next->ipaddr.addr&0x000000ff,(p->next->ipaddr.addr&0x0000ff00)>>8,(p->next->ipaddr.addr&0x00ff0000)>>16,(p->next->ipaddr.addr&0xff000000)>>24);
	 return TRUE;
 }
 else
 {
     printf("Could not find vaild ipaddr\n");
	 return FALSE;
 }

}

int list_add2(NODE **rootp, int ipaddr, u8_t *hw_addr)  
{  
    NODE *newNode;
    NODE *previous;
    NODE *current;
  
    current = *rootp;  
    previous = NULL;  
  
    while (current != NULL && current->ipaddr.addr < ipaddr)  
    {  
        previous = current;  
        current = current->next;  
    }  
  
    newNode = (NODE *)malloc(sizeof(NODE));  
    if (newNode == NULL) { 
        return FALSE; 
	} 
	newNode->ipaddr.addr=ipaddr;
	memcpy(newNode->hwaddr,hw_addr,DHCPD_CHADDR_LEN);

    newNode->next = current;  
    if (previous == NULL){
		
        *rootp = newNode;
    }
    else{
		
        previous->next = newNode;  
    }
  
    return TRUE;  
}  

static int search_vaild_bitmap(u8_t *bitmap,int size)
{
	int i=0,index=0;
	for(index=0;index<size;index++,bitmap++){
		i=0;
		while(i<8){
			if((*bitmap)&(1<<i++))
				continue;
			*bitmap=(*bitmap)|(1<<(--i));
			return ((index*8)+i);
		}
	}
	return -1;
}
static u32_t allocate_new_ipaddr(NODE *head)
{
	struct dhcp_client*dhcp=head;
	int ret;
	u32_t ipaddr;
	ret=search_vaild_bitmap(dhcp->ip_bitmap,IP_BITMAP_SIZE);
    //printf("ret : %d \n");
	if((ret<0)||((ret+DHCPD_ADDR_START)>DHCPD_ADDR_END)){
		printf("allocate ip vaild bitmap failed.\n");
		return FALSE;
	}
	ipaddr=uip_ntohl(uip_dhcpd_ipaddr);
	ipaddr+=(ret+DHCPD_ADDR_START);
	return uip_htonl(ipaddr);
}

/*---------------------------------------------------------------------------*/

static u8_t
parse_dhcp_options(u8_t *optptr, int len)
{
  u8_t *end = optptr + len;
  u8_t type = 0;

  while(optptr < end) {
    switch(*optptr) {
    case DHCP_OPTION_SUBNET_MASK:
      memcpy(s.netmask, optptr + 2, 4);
      break;
    case DHCP_OPTION_ROUTER:
      memcpy(s.default_router, optptr + 2, 4);
      break;
    case DHCP_OPTION_DNS_SERVER:
      memcpy(s.dnsaddr, optptr + 2, 4);
      break;
    case DHCP_OPTION_MSG_TYPE:
      type = *(optptr + 2);
      break;
    case DHCP_OPTION_SERVER_ID:
      memcpy(s.serverid, optptr + 2, 4);
      break;
    case DHCP_OPTION_LEASE_TIME:
      memcpy(s.lease_time, optptr + 2, 4);
      break;
	case DHCP_OPTION_REQ_IPADDR:
      memcpy(s.ipaddr, optptr + 2, 4);
      break;
    case DHCP_OPTION_END:
      return type;
    }

    optptr += optptr[1] + 2;
  }
  return type;
}


static u8_t
parse_dhcp_msg(void)
{
  struct dhcpd_msg *m = (struct dhcpd_msg *)uip_appdata;
  //判断是否是DHCP请求
  if(m->op == DHCP_REQUEST) {
    return parse_dhcp_options(&m->options[4], uip_datalen());
  }
  return 0;
}
static int dhcpd_relay_offer(NODE* head)
{
	u8_t *end;
    u32_t addr;
	NODE *p;
	p =head;
	struct dhcpd_msg *m = (struct dhcpd_msg *)uip_appdata;
 	while((p->next)&&(strcmp(p->next->hwaddr,m->chaddr)))
     	p = p->next;
	if(p->next)
	{
        addr = 0x0201a8c0;
		//addr=p->next->ipaddr.addr;
        printf("offer used ipaddr: %d.%d.%d.%d\n",p->next->ipaddr.addr&0x000000ff,(p->next->ipaddr.addr&0x0000ff00)>>8,(p->next->ipaddr.addr&0x00ff0000)>>16,(p->next->ipaddr.addr&0xff000000)>>24);
	}
	else
	{
		//addr = allocate_new_ipaddr(head);
        addr = 0x0201a8c0;
        printf("new  ip addr: %d.%d.%d.%d\n",addr&0x000000ff,(addr&0x0000ff00)>>8,(addr&0x00ff0000)>>16,((addr&0xff000000)>>24));
		list_add2(&head,addr,m->chaddr);
	}

	m->op = DHCP_REPLY;
	memcpy(m->yiaddr,&addr, sizeof(m->yiaddr));//设置CLINET IP地址

	end = add_msg_type(&m->options[4], DHCPOFFER);
	end = add_dhcpd_server_id(end);
	end = add_dhcpd_default_router(end);
    end = add_dhcpd_dns_server(end);
	end = add_dhcpd_lease_time(end);
	end = add_dhcpd_subnet_mask(end);
//	end = add_end(end);
	end = add_end1(end);
	my_uip_send(uip_appdata, end - (u8_t *)uip_appdata);
	return 1;
	
}
char NACK_flag = 0;
unsigned int clinet_ipaddr = 0;
static int dhcpd_replay_ask(NODE* head)
{
	u8_t *end;
    u32_t addr;
	int IfAsk=1;
	NODE *p;
	p =head;

	struct dhcpd_msg *m = (struct dhcpd_msg *)uip_appdata;

 	while((p->next)&&(strcmp(p->next->hwaddr,m->chaddr)))
     	p = p->next;
	if(p->next)
	{
	//	printf("Find available ipaddr from list\n");
		addr=p->next->ipaddr.addr;
		IfAsk = 1;
	}
	else
	{
	//	printf("Could not find available ipaddr from list\n");
		IfAsk = 0;
	}
	

	m->op = DHCP_REPLY;

	if(IfAsk){
        ask:    
        addr = 0x0201a8c0;
        printf("dhcpd set client ipaddr: %d.%d.%d.%d\n",addr&0x000000ff,(addr&0x0000ff00)>>8,(addr&0x00ff0000)>>16,((addr&0xff000000)>>24));
        clinet_ipaddr = addr;
		memcpy(m->yiaddr, &addr, sizeof(m->yiaddr));//设置CLINET IP地址
		end = add_msg_type(&m->options[4], DHCPACK);
		end = add_dhcpd_server_id(end);
		end = add_dhcpd_default_router(end);
		end = add_dhcpd_dns_server(end);
		end = add_dhcpd_lease_time(end);
		end = add_dhcpd_subnet_mask(end);
		
        dhcpd_end = 1;
	}
	else{
        NACK_flag ++ ;
        if(NACK_flag>=3){
            NACK_flag = 0;
            goto ask;
        }
		end = add_msg_type(&m->options[4], DHCPNAK);
	  	end = add_dhcpd_server_id(end);

        dhcpd_end = 0;
	}
	end = add_end(end);
	my_uip_send(uip_appdata, end - (u8_t *)uip_appdata);

	return 1;

}

void 
handle_dhcpd(void)
{
	unsigned char state;

	if(uip_newdata()){
		state=parse_dhcp_msg();

		if (state == DHCPDISCOVER){
			
			dhcpd_relay_offer(head_dhcpd);

		}
        else if(state == DHCPREQUEST){
			dhcpd_replay_ask(head_dhcpd);

		}
        else if(state == DHCPINFORM){
			dhcpd_replay_ask(head_dhcpd);
		}
	}
	return;
}


int
dhcpd_init(void)
{

	head_dhcpd=(NODE *)malloc(sizeof(NODE));
	if (head_dhcpd==NULL) {
		printf("allocated head_dhcpd failed.\n");
		return -1;
	}
	printf("dhcpd start\n");	
	head_dhcpd->next=NULL;

	uip_ipaddr_t addr;

	s.mac_len  = DHCP_HLEN_ETHERNET;

	uip_ipaddr(addr, 255,255,255,255);
	s.conn = uip_udp_new(&addr, HTONS(DHCPC_CLIENT_PORT));

	if(s.conn != NULL) {
		uip_udp_bind(s.conn, HTONS(DHCPC_SERVER_PORT));
    }
	return TRUE;
}

void
dhcpd_udp_appcall(void)
{
	struct uip_udp_conn *udp_conn = uip_udp_conn;
	u16_t lport, rport;

	lport=HTONS(udp_conn->lport);
	rport=HTONS(udp_conn->rport);
    if (DHCPC_SERVER_PORT == lport)
	{
		handle_dhcpd();
	}

	return;
}

