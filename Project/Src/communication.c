//******************************************************************************
/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "communication.h"
#include "lwip/init.h"
#include "lwip/tcp_impl.h"
#include "netif/etharp.h"
#include "usbd_rndis.h"
#include "httpd.h"
#include "dhserver.h"
#include "dnserver.h"
#include "time.h"

/* Private types -------------------------------------------------------------*/
struct netif netif_data;
const char *state_cgi_handler(int index, int n_params, char *params[], char *values[]);
const char *ctl_cgi_handler(int index, int n_params, char *params[], char *values[]);

dhcp_entry_t entries[DHCP_ENTRIES_QNT] = DHCP_ENTRIES;
dhcp_config_t dhcp_config = DHCP_CONFIG;

static const tCGI cgi_uri_table[] = CGI_URI_TABLE;
static const char *ssi_tags_table[] = SSI_TAGS_TABLE;


/* Private variables ---------------------------------------------------------*/
const uint8_t ipaddr[4]  = IPADDR;

/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
err_t netif_init_cb(struct netif *netif);
err_t linkoutput_fn(struct netif *netif, struct pbuf *p);
err_t output_fn(struct netif *netif, struct pbuf *p, ip_addr_t *ipaddr);
bool dns_query_proc(const char *name, ip_addr_t *addr);
static uint16_t ssi_handler(int index, char *insert, int ins_len);

// Transceiving Ethernet packets
err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
int i;
struct pbuf *q;
uint8_t *pbuffer;
static uint8_t Data[RNDIS_RX_BUFFER_SIZE];                                      
uint16_t size = 0;

  for (i = 0; i < 200; i++)
  {
    if (!rndis_tx_started()) break;
    msleep(1);
  }
  if (rndis_tx_started())
    return ERR_USE;
  
  /*while (!rndis_rx_data())    //TODO
  {}*/
  
  pbuffer = Data;
  for(q = p; q != NULL; q = q->next)                                            //TODO
  {
    /*if (size + q->len > ETH_MAX_PACKET_SIZE)
      return ERR_ARG;*/
    memcpy(pbuffer + size, (char *)q->payload, q->len);
    size += q->len;
  }
  rndis_tx_start(pbuffer, size);
  return ERR_OK;
}

// Receiving Ethernet packets
void usb_polling(void)
{
  if (!rndis_rx_data()) 
    return;
  
  struct pbuf *frame;
  frame = pbuf_alloc(PBUF_RAW, rndis_rx_size(), PBUF_POOL);
  if (frame == NULL) 
    return;
  memcpy(frame->payload, rndis_rx_data(), rndis_rx_size());
  frame->len = rndis_rx_size();
  rndis_rx_start();
  ethernet_input(frame, &netif_data);
  pbuf_free(frame);
}

err_t output_fn(struct netif *netif, struct pbuf *p, ip_addr_t *ipaddr)
{
  return etharp_output(netif, p, ipaddr);
}

err_t netif_init_cb(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = ETH_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn;
  netif->output = output_fn;
  return ERR_OK;
}

TIMER_PROC(tcp_timer, TCP_TMR_INTERVAL * 1000, 1, NULL)
{
  tcp_tmr();
}

bool dns_query_proc(const char *name, ip_addr_t *addr)
{
  if (strcmp(name, "run.stm") == 0 || strcmp(name, "www.run.stm") == 0)
  {
    addr->addr = *(uint32_t *)ipaddr;
    return true;
  }
  return false;
}

void init_lwip()
{
uint8_t hwaddr[6]  = HWADDR;
uint8_t netmask[4] = NETMASK;
uint8_t gateway[4] = GATEWAY;

  struct netif  *netif = &netif_data;
  
  lwip_init();
  netif->hwaddr_len = 6;
  memcpy(netif->hwaddr, hwaddr, 6);
  
  netif = netif_add(netif, PADDR(ipaddr), PADDR(netmask), PADDR(gateway), NULL, netif_init_cb, ip_input);
  netif_set_default(netif);
  
  stmr_add(&tcp_timer);
}

void init_netif(void)
{
  while (!netif_is_up(&netif_data));
}

void init_dnserv(void)
{
uint8_t ipaddr[4]  = IPADDR;
  while (dnserv_init(PADDR(ipaddr), 53, dns_query_proc) != ERR_OK) ;
}

void init_dhserv(void)
{
  while (dhserv_init(&dhcp_config) != ERR_OK) ;
}

void init_htserv(void)
{
  http_set_cgi_handlers(cgi_uri_table, sizeof(cgi_uri_table) / sizeof(tCGI));
  http_set_ssi_handler(ssi_handler, ssi_tags_table, sizeof(ssi_tags_table) / sizeof(char *));
  httpd_init();
}

uint8_t PORTC[8];

const char *state_cgi_handler(int index, int n_params, char *params[], char *values[])
{
  return "/state.shtml";
}

const char *ctl_cgi_handler(int index, int n_params, char *params[], char *values[])
{
  int i;
  for (i = 0; i < n_params; i++)
  {
    if (strcmp(params[i], "PA0") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA1") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA2") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA3") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA4") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA5") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA6") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    if (strcmp(params[i], "PA7") == 0) 
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, (*values[i] == '1' ? GPIO_PIN_SET : GPIO_PIN_RESET));
    //if (strcmp(params[i], "r") == 0) led_r = *values[i] == '1';
  }
    
  return "/state.shtml";
}

#ifdef TX_ZLP_TEST
static uint16_t ssi_handler(int index, char *insert, int ins_len)
{
  int res;
  static uint8_t i;
  static uint8_t c;
  
  if (ins_len < 32) return 0;
  
  if (c++ == 10)
  {
    i++;
    i &= 0x07;
    c = 0;
  }
  switch (index)
  {
  case 0: // systick
    res = snprintf(insert, ins_len, "%s", "1234");
    break;
  case 1: // PORTC
    {
      res = snprintf(insert, ins_len, "%u, %u, %u, %u, %u, %u, %u, %u", 10, 10, 10, 10, 10, 10, 10, 10);
      break;
    }
  case 2: // PA0
    *insert = '0' + (i == 0);
    res = 1;
    break;
  case 3: // PA1
    *insert = '0' + (i == 1);
    res = 1;
    break;
  case 4: // PA2
    *insert = '0' + (i == 2);
    res = 1;
    break;
  case 5: // PA3
    *insert = '0' + (i == 3);
    res = 1;
    break;
  case 6: // PA4
    *insert = '0' + (i == 4);
    res = 1;
    break;
  case 7: // PA5
    *insert = '0' + (i == 5);
    res = 1;
    break;
  case 8: // PA6
    *insert = '0' + (i == 6);
    res = 1;
    break;
  case 9: // PA7
    *insert = '0' + (i == 7);
    res = 1;
    break;
  }
  
  return res;
}
#else
static uint16_t ssi_handler(int index, char *insert, int ins_len)
{
  int res;
  
  if (ins_len < 32) return 0;
  
  switch (index)
  {
  case 0: // systick
    res = snprintf(insert, ins_len, "%u", (unsigned)mtime());
    break;
  case 1: // PORTC
    {
      PORTC[0] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_SET;
      PORTC[1] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_SET;
      PORTC[2] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2) == GPIO_PIN_SET;
      PORTC[3] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_3) == GPIO_PIN_SET;
      PORTC[4] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_SET;
      PORTC[5] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET;
      PORTC[6] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_SET;
      PORTC[7] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7) == GPIO_PIN_SET;
      
      res = snprintf(insert, ins_len, "%i, %i, %i, %i, %i, %i, %i, %i", PORTC[0], PORTC[1], PORTC[2], PORTC[3], PORTC[4], PORTC[5], PORTC[6], PORTC[7]);
      break;
    }
  case 2: // PA0
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);
    res = 1;
    break;
  case 3: // PA1
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET);
    res = 1;
    break;
  case 4: // PA2
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET);
    res = 1;
    break;
  case 5: // PA3
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET);
    res = 1;
    break;
  case 6: // PA4
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET);
    res = 1;
    break;
  case 7: // PA5
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET);
    res = 1;
    break;
  case 8: // PA6
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET);
    res = 1;
    break;
  case 9: // PA7
    *insert = '0' + (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET);
    res = 1;
    break;
  }
  
  return res;
}
#endif
