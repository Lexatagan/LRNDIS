/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 by Sergey Fetisov <fsenok@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * version: 1.0 demo (7.02.2015)
 */

#ifndef __USB_CDC_CORE_H_
#define __USB_CDC_CORE_H_

#include  "usbd_ioreq.h"
//#include <stdint.h>
#include <stdbool.h>
//#include <string.h>
#include <stddef.h>
#include "rndis_protocol.h"

#define ETH_LINK_SPEED                                  250000                         // bits per sec
#define RNDIS_VENDOR                                    "fetisov"                      // NIC vendor name
#define STATION_HWADDR                                  0x20,0x89,0x84,0x6A,0x96,0xAA  // station MAC
#define PERMANENT_HWADDR                                0x20,0x89,0x84,0x6A,0x96,0xAA  // permanent MAC

#define ETH_HEADER_SIZE                                 14
#define ETH_MTU                                         1500                                                    // MTU value
#define ETH_MIN_PACKET_SIZE                             60
#define ETH_MAX_PACKET_SIZE                             (ETH_HEADER_SIZE + ETH_MTU)
#define RNDIS_HEADER_SIZE                               sizeof(rndis_data_packet_t)
#define RNDIS_TX_BUFFER_SIZE                            (ETH_MAX_PACKET_SIZE + RNDIS_HEADER_SIZE)               //TODO
#define RNDIS_RX_BUFFER_SIZE                            (ETH_MAX_PACKET_SIZE + RNDIS_HEADER_SIZE)               //TODO

#define RNDIS_CONFIG_DESC_SIZE                          0x09
#define RNDIS_TOTAL_CONFIG_DESC_SIZE                    0x0043                  /*TODO 0x3E according to MSDN*/
#define RNDIS_INTERFACE_DESC_SIZE                       0x09
#define RNDIS_ENDPOINT_DESC_SIZE                        0x07

#define USB_DEVICE_CLASS_RNDIS                          0x02
#define RNDIS_SUBCLASS                                  0x02
#define RNDIS_PROTOCOL_UNDEFINED                        0xff

#define RNDIS_CONTROL_OUT_EP                            0x00
#define RNDIS_CONTROL_IN_EP                             0x80
#define RNDIS_NOTIFICATION_IN_EP                        0x81
#define RNDIS_DATA_IN_EP                                0x82
#define RNDIS_DATA_OUT_EP                               0x03
#define RNDIS_NOTIFICATION_IN_SZ                        0x0008
#define RNDIS_DATA_IN_SZ                                0x0040
#define RNDIS_DATA_OUT_SZ                               0x0040

#define RNDIS_CONTROL_OUT_PMAADDRESS                    0x08 * 4                //8 bytes per EP
#define RNDIS_CONTROL_IN_PMAADDRESS                     RNDIS_CONTROL_OUT_PMAADDRESS + USB_MAX_EP0_SIZE
#define RNDIS_NOTIFICATION_IN_PMAADDRESS                RNDIS_CONTROL_IN_PMAADDRESS + USB_MAX_EP0_SIZE
#define RNDIS_DATA_IN_PMAADDRESS                        RNDIS_NOTIFICATION_IN_PMAADDRESS + RNDIS_NOTIFICATION_IN_SZ
#define RNDIS_DATA_OUT_PMAADDRESS                       RNDIS_DATA_IN_PMAADDRESS + RNDIS_DATA_IN_SZ

typedef  struct  rndis_ethernet_frame 
{
  rndis_data_packet_t rndis_header;                      
  uint8_t data[ETH_MAX_PACKET_SIZE];
  uint16_t data_size;
  bool ZLP;
  bool wrprotected;                                                                   //if true - buffer is write protected
}rndis_EthernetFrameTypeDef;

extern USBD_ClassTypeDef  usbd_rndis;
extern usb_eth_stat_t usb_eth_stat;
extern rndis_state_t rndis_state;

bool rndis_send(void);
bool rndis_can_send(void);
bool rndis_received(void);
void rndis_continue_receiving(void);
uint8_t *rndis_get_tx_buffer(void);
uint8_t *rndis_get_rx_buffer(void);
void rndis_set_tx_size(uint16_t size);
uint16_t rndis_get_rx_size(void);

#endif
