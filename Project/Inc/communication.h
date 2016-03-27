/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
#define PADDR(ptr) ((ip_addr_t *)ptr)

/* Exported constants --------------------------------------------------------*/

/* LAN */
#define HWADDR                          {0x20,0x89,0x84,0x6A,0x96,0x34}
#define IPADDR                          {192, 168, 7, 1}
#define NETMASK                         {255, 255, 255, 0}
#define GATEWAY                         {0, 0, 0, 0}

#define DHCP_ENTRIES_QNT                3
#define DHCP_ENTRIES                    {\
                                        { {0}, {192, 168, 7, 2}, {255, 255, 255, 0}, 24 * 60 * 60 }, \
                                        { {0}, {192, 168, 7, 3}, {255, 255, 255, 0}, 24 * 60 * 60 }, \
                                        { {0}, {192, 168, 7, 4}, {255, 255, 255, 0}, 24 * 60 * 60 } \
                                        }
#define DHCP_CONFIG                     { \
                                        IPADDR, 67, \
                                        IPADDR, \
                                        "stm", \
                                        DHCP_ENTRIES_QNT, \
                                        entries \
                                        }

#define CGI_URI_TABLE                   { \
                                        { "/state.cgi", state_cgi_handler }, \
                                        { "/ctl.cgi",   ctl_cgi_handler }, \
                                        }
#define SSI_TAGS_TABLE                  { \
                                        "systick", \
                                        "PORTC", \
                                        "PA0", \
                                        "PA1", \
                                        "PA2", \
                                        "PA3", \
                                        "PA4", \
                                        "PA5", \
                                        "PA6", \
                                        "PA7"  \
                                        }

//#define TX_ZLP_TEST

/* Exported macro ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void init_lwip();
void init_htserv(void);
void init_dhserv(void);
void init_dnserv(void);
void init_netif(void);
void usb_polling(void);

#endif /* __COMMUNICATION_H */
