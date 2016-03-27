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

// RNDIS to USB Mapping https://msdn.microsoft.com/en-us/library/windows/hardware/ff570657(v=vs.85).aspx

#include "usbd_conf.h"
#include "usbd_rndis.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"

/*******************************************************************************
Private constants
*******************************************************************************/

const uint32_t OIDSupportedList[] = 
{
  OID_GEN_SUPPORTED_LIST,
  OID_GEN_HARDWARE_STATUS,
  OID_GEN_MEDIA_SUPPORTED,
  OID_GEN_MEDIA_IN_USE,
  //    OID_GEN_MAXIMUM_LOOKAHEAD,
  OID_GEN_MAXIMUM_FRAME_SIZE,
  OID_GEN_LINK_SPEED,
  //    OID_GEN_TRANSMIT_BUFFER_SPACE,
  //    OID_GEN_RECEIVE_BUFFER_SPACE,
  OID_GEN_TRANSMIT_BLOCK_SIZE,
  OID_GEN_RECEIVE_BLOCK_SIZE,
  OID_GEN_VENDOR_ID,
  OID_GEN_VENDOR_DESCRIPTION,
  OID_GEN_VENDOR_DRIVER_VERSION,
  OID_GEN_CURRENT_PACKET_FILTER,
  //    OID_GEN_CURRENT_LOOKAHEAD,
  //    OID_GEN_DRIVER_VERSION,
  OID_GEN_MAXIMUM_TOTAL_SIZE,
  OID_GEN_PROTOCOL_OPTIONS,
  OID_GEN_MAC_OPTIONS,
  OID_GEN_MEDIA_CONNECT_STATUS,
  OID_GEN_MAXIMUM_SEND_PACKETS,
  OID_802_3_PERMANENT_ADDRESS,
  OID_802_3_CURRENT_ADDRESS,
  OID_802_3_MULTICAST_LIST,
  OID_802_3_MAXIMUM_LIST_SIZE,
  OID_802_3_MAC_OPTIONS
};

#define OID_LIST_LENGTH (sizeof(OIDSupportedList) / sizeof(*OIDSupportedList))
#define ENC_BUF_SIZE    (OID_LIST_LENGTH * 4 + 32)

/*******************************************************************************
Private function definitions
*******************************************************************************/
void response_available(USBD_HandleTypeDef *pdev);

/*********************************************
RNDIS Device library callbacks
*********************************************/
static uint8_t  usbd_rndis_init                         (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  usbd_rndis_deinit                       (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  usbd_rndis_setup                        (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  usbd_rndis_ep0_recv                     (USBD_HandleTypeDef *pdev);
static uint8_t  usbd_rndis_data_in                      (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  usbd_rndis_data_out                     (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  usbd_rndis_sof                          (USBD_HandleTypeDef *pdev);
static uint8_t  rndis_iso_in_incomplete                 (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  rndis_iso_out_incomplete                (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  *usbd_rndis_GetDeviceQualifierDesc      (uint16_t *length);
static uint8_t  *usbd_rndis_GetCfgDesc                  (uint16_t *length);

/*******************************************************************************
Private variables
*******************************************************************************/
USBD_HandleTypeDef *pDev;

uint8_t station_hwaddr[6] = { STATION_HWADDR };
uint8_t permanent_hwaddr[6] = { PERMANENT_HWADDR };
usb_eth_stat_t usb_eth_stat = { 0, 0, 0, 0 };
rndis_state_t rndis_state = rndis_uninitialized;
uint32_t oid_packet_filter = 0x0000000;
uint8_t encapsulated_buffer[ENC_BUF_SIZE];

uint8_t *rndis_tx_ptr = NULL;
uint16_t rndis_tx_data_size = 0;
bool rndis_tx_transmitting = false;
bool rndis_tx_ZLP = false;
uint8_t rndis_rx_buffer[RNDIS_RX_BUFFER_SIZE];
uint16_t rndis_rx_data_size = 0;
bool rndis_rx_started = false;

USBD_ClassTypeDef usbd_rndis =
{
  usbd_rndis_init,
  usbd_rndis_deinit,
  usbd_rndis_setup,
  NULL,
  usbd_rndis_ep0_recv,
  usbd_rndis_data_in,
  usbd_rndis_data_out,
  usbd_rndis_sof,
  rndis_iso_in_incomplete,
  rndis_iso_out_incomplete,
  usbd_rndis_GetCfgDesc,
  usbd_rndis_GetCfgDesc, 
  usbd_rndis_GetCfgDesc,
  usbd_rndis_GetDeviceQualifierDesc,
};

__ALIGN_BEGIN static uint8_t usbd_rndis_CfgDesc[RNDIS_TOTAL_CONFIG_DESC_SIZE]  __ALIGN_END =
{
  // Configuration descriptor           https://msdn.microsoft.com/en-US/library/ee482887(v=winembedded.60).aspx
  RNDIS_CONFIG_DESC_SIZE,               /* bLength */
  USB_DESC_TYPE_CONFIGURATION,          /* bDescriptorType */
  LOBYTE(RNDIS_TOTAL_CONFIG_DESC_SIZE), /* wTotalLength  109 bytes*/
  HIBYTE(RNDIS_TOTAL_CONFIG_DESC_SIZE),      
  0x02,                                 /* bNumInterfaces */
  0x01,                                 /* bConfigurationValue */
  0x00,                                 /* iConfiguration */
  0x80,                                 /* bmAttributes  BUS Powred*/
  0x64,                                 /* bMaxPower = 100 mA*/
  /* 09 byte*/
  
  //  Communication Class INTERFACE descriptor          https://msdn.microsoft.com/en-US/library/ee485851(v=winembedded.60).aspx
  RNDIS_INTERFACE_DESC_SIZE,            /* bLength */
  USB_DESC_TYPE_INTERFACE,              /* bDescriptorType */
  0x00,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x01,                                 /* bNumEndpoints */                     
  USB_DEVICE_CLASS_RNDIS,               /* bInterfaceClass */
  RNDIS_SUBCLASS,                       /* bInterfaceSubClass */
  RNDIS_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
    
  //  Functional Descriptors for Communication Class Interface per RNDIS spec.
  
  // Header Functional Descriptor
  0x05,                              // bFunctionLength
  0x24,                              // bDescriptorType = CS Interface
  0x00,                              // bDescriptorSubtype
  0x10,                              // bcdCDC = 1.10
  0x01,                              // bcdCDC = 1.10
  
  // Call Management Functional Descriptor
  0x05,                              // bFunctionLength
  0x24,                              // bDescriptorType = CS Interface
  0x01,                              // bDescriptorSubtype = Call Management
  0x00,                              // bmCapabilities
  0x01,                              // bDataInterface
  
  // Abstract Control Management Functional Descriptor
  0x04,                              // bFunctionLength
  0x24,                              // bDescriptorType = CS Interface
  0x02,                              // bDescriptorSubtype = Abstract Control Management
  0x00,                              // bmCapabilities = Requests/notifications not supported
  
  // Union Functional Descriptor
  0x05,                              // bFunctionLength
  0x24,                              // bDescriptorType = CS Interface
  0x06,                              // bDescriptorSubtype = Union
  0x00,                              // bControlInterface = "RNDIS Communications Control"
  0x01,                              // bSubordinateInterface0 = "RNDIS Ethernet Data"
  
  // Endpoint descriptors for Communication Class Interface     https://msdn.microsoft.com/en-US/library/ee482509(v=winembedded.60).aspx
  
  RNDIS_ENDPOINT_DESC_SIZE,             //  bLength         = 7 bytes
  USB_DESC_TYPE_ENDPOINT,               //  bDescriptorType = ENDPOINT
  RNDIS_NOTIFICATION_IN_EP,             //  bEndpointAddr   = IN - EP1
  USBD_EP_TYPE_INTR,                    //  bmAttributes    = Interrupt endpoint
  LOBYTE(RNDIS_NOTIFICATION_IN_SZ),     //  wMaxPacketSize
  HIBYTE(RNDIS_NOTIFICATION_IN_SZ),
  1,                                    //  bInterval       = 1 ms polling from host
  
  //  Data Class INTERFACE descriptor           https://msdn.microsoft.com/en-US/library/ee481260(v=winembedded.60).aspx
  
  RNDIS_INTERFACE_DESC_SIZE,            //  bLength         = 9 bytes
  USB_DESC_TYPE_INTERFACE,              //  bDescriptorType = INTERFACE
  0x01,                                 //  bInterfaceNo    = 1
  0x00,                                 //  bAlternateSet   = 0
  0x02,                                 //  bNumEndPoints   = 2 (RNDIS spec)
  0x0A,                                 //  bInterfaceClass = Data if class (RNDIS spec)
  0x00,                                 //  bIfSubClass     = unused
  0x00,                                 //  bIfProtocol     = unused
  0x00,                                 //  iInterface      = unused
  
  // Endpoint descriptors for Data Class Interface
  // IN Endpoint descriptor     https://msdn.microsoft.com/en-US/library/ee484483(v=winembedded.60).aspx
  RNDIS_ENDPOINT_DESC_SIZE,             //  bLength         = 7 bytes.
  USB_DESC_TYPE_ENDPOINT,               //  bDescriptorType = ENDPOINT [IN]
  RNDIS_DATA_IN_EP,                     //  bEndpointAddr   = IN EP
  USBD_EP_TYPE_BULK,                    //  bmAttributes    = BULK
  LOBYTE(RNDIS_DATA_IN_SZ),             //  wMaxPacketSize
  HIBYTE(RNDIS_DATA_IN_SZ),
  0,                                    //  bInterval       = ignored for BULK.
  
  // OUT Endpoint descriptor     https://msdn.microsoft.com/en-US/library/ee482464(v=winembedded.60).aspx
  RNDIS_ENDPOINT_DESC_SIZE,             //  bLength         = 7 bytes.
  USB_DESC_TYPE_ENDPOINT,               //  bDescriptorType = ENDPOINT [OUT]
  RNDIS_DATA_OUT_EP,                    //  bEndpointAddr   = OUT EP
  USBD_EP_TYPE_BULK,                    //  bmAttributes    = BULK
  LOBYTE(RNDIS_DATA_OUT_SZ),             //  wMaxPacketSize
  HIBYTE(RNDIS_DATA_OUT_SZ),
  0                                     //  bInterval       = ignored for BULK
};

__ALIGN_BEGIN uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};



/*******************************************************************************
                            API functions 
*******************************************************************************/
__weak void rndis_initialized_cb(void)
{
}

bool rndis_rx_start(void)
{
  if (rndis_rx_started)
    return false;
  
  rndis_rx_started = true;
  USBD_LL_PrepareReceive(pDev, 
                         RNDIS_DATA_OUT_EP,                                      
                         rndis_rx_buffer,
                         RNDIS_RX_BUFFER_SIZE);
  return true;
}

uint8_t *rndis_rx_data(void)
{
  if (rndis_rx_size())
    return rndis_rx_buffer + RNDIS_HEADER_SIZE;
  else
    return NULL;
}

uint16_t rndis_rx_size(void)
{
  if (!rndis_rx_started)
    return rndis_rx_data_size;
  else
    return 0;
}

__weak void rndis_rx_ready_cb(void)
{
}

bool rndis_tx_start(uint8_t *data, uint16_t size)
{
uint8_t sended;
static uint8_t first[RNDIS_DATA_IN_SZ];
rndis_data_packet_t *hdr;

  //if tx buffer is already transfering or has incorrect length
  if ((rndis_tx_transmitting) || (size > ETH_MAX_PACKET_SIZE) || (size == 0))
  {
    usb_eth_stat.txbad++;
    return false;
  }


  rndis_tx_transmitting = true;
  rndis_tx_ptr = data;
  rndis_tx_data_size = size;

  hdr = (rndis_data_packet_t *)first;
  memset(hdr, 0, RNDIS_HEADER_SIZE);
  hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
  hdr->MessageLength = RNDIS_HEADER_SIZE + rndis_tx_data_size;
  hdr->DataOffset = RNDIS_HEADER_SIZE - offsetof(rndis_data_packet_t, DataOffset);
  hdr->DataLength = rndis_tx_data_size;

  sended = RNDIS_DATA_IN_SZ - RNDIS_HEADER_SIZE;
  if (sended > rndis_tx_data_size)
    sended = rndis_tx_data_size;
  memcpy(first + RNDIS_HEADER_SIZE, rndis_tx_ptr, sended);
  rndis_tx_ptr += sended;
  rndis_tx_data_size -= sended;
  
  //http://habrahabr.ru/post/248729/
  if (hdr->MessageLength % RNDIS_DATA_IN_SZ == 0)
    rndis_tx_ZLP = true;
  
  __disable_irq();      //TODO IRQ
  USBD_LL_Transmit (pDev, 
                    RNDIS_DATA_IN_EP,                                      
                    (uint8_t *)first,
                    RNDIS_DATA_IN_SZ);
  __enable_irq(); 

  //Increment error counter and then decrement in data_in if OK
  usb_eth_stat.txbad++;
    
  return true;
}

bool rndis_tx_started(void)
{
  return rndis_tx_transmitting;
}

__weak void rndis_tx_ready_cb(void)
{
}
/*******************************************************************************
                            /API functions 
*******************************************************************************/

static uint8_t usbd_rndis_init(USBD_HandleTypeDef  *pdev, uint8_t cfgidx)
{
  USBD_LL_OpenEP(pdev,
                 RNDIS_NOTIFICATION_IN_EP,
                 USBD_EP_TYPE_INTR,
                 RNDIS_NOTIFICATION_IN_SZ);
  USBD_LL_OpenEP(pdev,
                 RNDIS_DATA_IN_EP,
                 USBD_EP_TYPE_BULK,
                 RNDIS_DATA_IN_SZ);
  USBD_LL_OpenEP(pdev,
                 RNDIS_DATA_OUT_EP,
                 USBD_EP_TYPE_BULK,
                 RNDIS_DATA_OUT_SZ);
  
  /*USBD_LL_PrepareReceive(pdev,
                         RNDIS_DATA_OUT_EP,
                         rndis_rx_buffer,                        
                         RNDIS_RX_BUFFER_SIZE);*/    
  pDev = pdev;

  rndis_rx_start();
  return USBD_OK;
}

static uint8_t  usbd_rndis_deinit(USBD_HandleTypeDef  *pdev, uint8_t cfgidx)
{
  USBD_LL_CloseEP(pdev,
              RNDIS_NOTIFICATION_IN_EP);
  USBD_LL_CloseEP(pdev,
              RNDIS_DATA_IN_EP);
  USBD_LL_CloseEP(pdev,
              RNDIS_DATA_OUT_EP);
  return USBD_OK;
}

static uint8_t usbd_rndis_setup(USBD_HandleTypeDef  *pdev, USBD_SetupReqTypedef *req)
{
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :
    if (req->wLength != 0) // is data setup packet?
    {
      /* Check if the request is Device-to-Host */
      if (req->bmRequest & 0x80)
      {
        USBD_CtlSendData (pdev, 
                          encapsulated_buffer,
                          ((rndis_generic_msg_t *)encapsulated_buffer)->MessageLength);
      }
      else /* Host-to-Device requeset */
      {
        USBD_CtlPrepareRx (pdev,
                           encapsulated_buffer,                                  
                           req->wLength);    
      }
    }
    return USBD_OK;
    
  default:
    return USBD_OK;
  }
}

#define INFBUF ((uint32_t *)(encapsulated_buffer + sizeof(rndis_query_cmplt_t)))

void rndis_query_cmplt32(USBD_HandleTypeDef  *pdev, int status, uint32_t data)
{
  rndis_query_cmplt_t *c;
  c = (rndis_query_cmplt_t *)encapsulated_buffer;
  c->MessageType = REMOTE_NDIS_QUERY_CMPLT;
  c->MessageLength = sizeof(rndis_query_cmplt_t) + 4;
  c->InformationBufferLength = 4;
  c->InformationBufferOffset = 16;
  c->Status = status;
  *(uint32_t *)(c + 1) = data;
  response_available(pdev);
}

void rndis_query_cmplt(USBD_HandleTypeDef  *pdev, int status, const void *data, int size)
{
  rndis_query_cmplt_t *c;
  c = (rndis_query_cmplt_t *)encapsulated_buffer;
  c->MessageType = REMOTE_NDIS_QUERY_CMPLT;
  c->MessageLength = sizeof(rndis_query_cmplt_t) + size;
  c->InformationBufferLength = size;
  c->InformationBufferOffset = 16;
  c->Status = status;
  memcpy(c + 1, data, size);
  response_available(pdev);
}

#define MAC_OPT NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | \
NDIS_MAC_OPTION_RECEIVE_SERIALIZED  | \
  NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  | \
    NDIS_MAC_OPTION_NO_LOOPBACK
      
      static const char *rndis_vendor = RNDIS_VENDOR;

void rndis_query(USBD_HandleTypeDef  *pdev)
{
  switch (((rndis_query_msg_t *)encapsulated_buffer)->Oid)
  {
  case OID_GEN_SUPPORTED_LIST:         rndis_query_cmplt(pdev, RNDIS_STATUS_SUCCESS, OIDSupportedList, 4 * OID_LIST_LENGTH); return;
  case OID_GEN_VENDOR_DRIVER_VERSION:  rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0x00001000);  return;
  case OID_802_3_CURRENT_ADDRESS:      rndis_query_cmplt(pdev, RNDIS_STATUS_SUCCESS, &station_hwaddr, 6); return;
  case OID_802_3_PERMANENT_ADDRESS:    rndis_query_cmplt(pdev, RNDIS_STATUS_SUCCESS, &permanent_hwaddr, 6); return;
  case OID_GEN_MEDIA_SUPPORTED:        rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
  case OID_GEN_MEDIA_IN_USE:           rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
  case OID_GEN_PHYSICAL_MEDIUM:        rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
  case OID_GEN_HARDWARE_STATUS:        rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  case OID_GEN_LINK_SPEED:             rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, ETH_LINK_SPEED / 100); return;
  case OID_GEN_VENDOR_ID:              rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0x00FFFFFF); return;
  case OID_GEN_VENDOR_DESCRIPTION:     rndis_query_cmplt(pdev, RNDIS_STATUS_SUCCESS, rndis_vendor, strlen(rndis_vendor) + 1); return;
  case OID_GEN_CURRENT_PACKET_FILTER:  rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, oid_packet_filter); return;
  case OID_GEN_MAXIMUM_FRAME_SIZE:     rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE - ETH_HEADER_SIZE); return;
  case OID_GEN_MAXIMUM_TOTAL_SIZE:     rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
  case OID_GEN_TRANSMIT_BLOCK_SIZE:    rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
  case OID_GEN_RECEIVE_BLOCK_SIZE:     rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
  case OID_GEN_MEDIA_CONNECT_STATUS:   rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, NDIS_MEDIA_STATE_CONNECTED); return;
  //	case OID_GEN_CURRENT_LOOKAHEAD:      rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, RNDIS_RX_BUFFER_SIZE); return;
  case OID_GEN_RNDIS_CONFIG_PARAMETER: rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  case OID_802_3_MAXIMUM_LIST_SIZE:    rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 1); return;
  case OID_802_3_MULTICAST_LIST:       rndis_query_cmplt32(pdev, RNDIS_STATUS_NOT_SUPPORTED, 0); return;
  case OID_802_3_MAC_OPTIONS:          rndis_query_cmplt32(pdev, RNDIS_STATUS_NOT_SUPPORTED, 0); return;
  case OID_GEN_MAC_OPTIONS:            rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, /*MAC_OPT*/ 0); return;
  case OID_802_3_RCV_ERROR_ALIGNMENT:  rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  case OID_802_3_XMIT_ONE_COLLISION:   rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  case OID_802_3_XMIT_MORE_COLLISIONS: rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  case OID_GEN_XMIT_OK:                rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, usb_eth_stat.txok); return;
  case OID_GEN_RCV_OK:                 rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, usb_eth_stat.rxok); return;
  case OID_GEN_RCV_ERROR:              rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, usb_eth_stat.rxbad); return;
  case OID_GEN_XMIT_ERROR:             rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, usb_eth_stat.txbad); return;
  case OID_GEN_RCV_NO_BUFFER:          rndis_query_cmplt32(pdev, RNDIS_STATUS_SUCCESS, 0); return;
  default:                             rndis_query_cmplt(pdev, RNDIS_STATUS_FAILURE, NULL, 0); return;
  }
}

#undef INFBUF
#define INFBUF ((uint32_t *)((uint8_t *)&(m->RequestId) + m->InformationBufferOffset))
#define CFGBUF ((rndis_config_parameter_t *) INFBUF)
#define PARMNAME  ((uint8_t *)CFGBUF + CFGBUF->ParameterNameOffset)
#define PARMVALUE ((uint8_t *)CFGBUF + CFGBUF->ParameterValueOffset)
#define PARMVALUELENGTH	CFGBUF->ParameterValueLength
#define PARM_NAME_LENGTH 25 /* Maximum parameter name length */

void rndis_handle_config_parm(const char *data, int keyoffset, int valoffset, int keylen, int vallen)
{
  //	if (strncmp(parmname, "rawmode", 7) == 0) 
  //	{
  //		if (parmvalue[0] == '0') 
  //		{
  //			usbstick_mode.raw = 0;
  //		} 
  //		else 
  //		{
  //			usbstick_mode.raw = 1;
  //		}
  //	}
}

void rndis_packetFilter(uint32_t newfilter)
{
  if (newfilter & NDIS_PACKET_TYPE_PROMISCUOUS) 
  {
    //		USB_ETH_HOOK_SET_PROMISCIOUS_MODE(true);
  } 
  else 
  {
    //		USB_ETH_HOOK_SET_PROMISCIOUS_MODE(false);
  }
}

void rndis_handle_set_msg(USBD_HandleTypeDef  *pdev)
{
  rndis_set_cmplt_t *c;
  rndis_set_msg_t *m;
  rndis_Oid_t oid;
  
  c = (rndis_set_cmplt_t *)encapsulated_buffer;
  m = (rndis_set_msg_t *)encapsulated_buffer;
  
  // Never have longer parameter names than PARM_NAME_LENGTH
  /*
  char parmname[PARM_NAME_LENGTH+1];
  
  uint8_t i;
  int8_t parmlength;
  */
  
  // The parameter name seems to be transmitted in uint16_t, but
  // we want this in uint8_t. Hence have to throw out some info...
  
  /*
  if (CFGBUF->ParameterNameLength > (PARM_NAME_LENGTH*2))
  {
  parmlength = PARM_NAME_LENGTH * 2;
}
	else
  {
  parmlength = CFGBUF->ParameterNameLength;
}
  
  i = 0;
  while (parmlength > 0)
  {
  // Convert from uint16_t to char array. 
  parmname[i] = (char)*(PARMNAME + 2*i); // FSE! FIX IT!
  parmlength -= 2;
  i++;
}
  */
  
  oid = m->Oid;
  c->MessageType = REMOTE_NDIS_SET_CMPLT;
  c->MessageLength = sizeof(rndis_set_cmplt_t);
  c->Status = RNDIS_STATUS_SUCCESS;
  
  switch (oid)
  {
    // Parameters set up in 'Advanced' tab
  case OID_GEN_RNDIS_CONFIG_PARAMETER:
    {
      char *ptr = (char *)m;
      ptr += sizeof(rndis_generic_msg_t);
      ptr += m->InformationBufferOffset;
      rndis_config_parameter_t *p = (rndis_config_parameter_t *)ptr;
      rndis_handle_config_parm(ptr, p->ParameterNameOffset, p->ParameterValueOffset, p->ParameterNameLength, p->ParameterValueLength);
    }
    break;
    
    /* Mandatory general OIDs */
  case OID_GEN_CURRENT_PACKET_FILTER:
    oid_packet_filter = *INFBUF;
    if (oid_packet_filter)
    {
      rndis_packetFilter(oid_packet_filter);
      rndis_state = rndis_data_initialized;
    } 
    else 
    {
      rndis_state = rndis_initialized;
      rndis_initialized_cb();
    }
    break;
    
  case OID_GEN_CURRENT_LOOKAHEAD:
    break;
    
  case OID_GEN_PROTOCOL_OPTIONS:
    break;
    
    /* Mandatory 802_3 OIDs */
  case OID_802_3_MULTICAST_LIST:
    break;
    
    /* Power Managment: fails for now */
  case OID_PNP_ADD_WAKE_UP_PATTERN:
  case OID_PNP_REMOVE_WAKE_UP_PATTERN:
  case OID_PNP_ENABLE_WAKE_UP:
  default:
    c->Status = RNDIS_STATUS_FAILURE;
    break;
  }
  
  //c->MessageID is same as before
  
  response_available(pdev);

  return;
}

// Control Channel      https://msdn.microsoft.com/en-us/library/windows/hardware/ff546124(v=vs.85).aspx
static uint8_t usbd_rndis_ep0_recv(USBD_HandleTypeDef  *pdev)
{
  switch (((rndis_generic_msg_t *)encapsulated_buffer)->MessageType)
  {
  case REMOTE_NDIS_INITIALIZE_MSG:
    {
      rndis_initialize_cmplt_t *m;
      m = ((rndis_initialize_cmplt_t *)encapsulated_buffer);
      //m->MessageID is same as before
      m->MessageType = REMOTE_NDIS_INITIALIZE_CMPLT;
      m->MessageLength = sizeof(rndis_initialize_cmplt_t);
      m->MajorVersion = RNDIS_MAJOR_VERSION;
      m->MinorVersion = RNDIS_MINOR_VERSION;
      m->Status = RNDIS_STATUS_SUCCESS;
      m->DeviceFlags = RNDIS_DF_CONNECTIONLESS;
      m->Medium = RNDIS_MEDIUM_802_3;
      m->MaxPacketsPerTransfer = 1;
      m->MaxTransferSize = RNDIS_RX_BUFFER_SIZE;
      m->PacketAlignmentFactor = 0;
      m->AfListOffset = 0;
      m->AfListSize = 0;
      rndis_state = rndis_initialized;
      response_available(pdev);
    }
    break;
    
  case REMOTE_NDIS_QUERY_MSG:
    rndis_query(pdev);
    break;
    
  case REMOTE_NDIS_SET_MSG:
    rndis_handle_set_msg(pdev);
    break;
    
  case REMOTE_NDIS_RESET_MSG:
    {
      rndis_reset_cmplt_t * m;
      m = ((rndis_reset_cmplt_t *)encapsulated_buffer);
      rndis_state = rndis_uninitialized;
      m->MessageType = REMOTE_NDIS_RESET_CMPLT;
      m->MessageLength = sizeof(rndis_reset_cmplt_t);
      m->Status = RNDIS_STATUS_SUCCESS;
      m->AddressingReset = 1; /* Make it look like we did something */
      //	m->AddressingReset = 0; //Windows halts if set to 1 for some reason
      response_available(pdev);
    }
    break;
    
  case REMOTE_NDIS_KEEPALIVE_MSG:
    {
      rndis_keepalive_cmplt_t * m;
      m = (rndis_keepalive_cmplt_t *)encapsulated_buffer;
      m->MessageType = REMOTE_NDIS_KEEPALIVE_CMPLT;
      m->MessageLength = sizeof(rndis_keepalive_cmplt_t);
      m->Status = RNDIS_STATUS_SUCCESS;
    }
    // We have data to send back
    response_available(pdev);
    break;
    
  default:
    break;
  }
  return USBD_OK;
}

int sended = 0;

// Data Channel         https://msdn.microsoft.com/en-us/library/windows/hardware/ff546305(v=vs.85).aspx
//                      https://msdn.microsoft.com/en-us/library/windows/hardware/ff570635(v=vs.85).aspx
static uint8_t usbd_rndis_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  epnum &= 0x0F;
  if (epnum == (RNDIS_DATA_IN_EP & 0x0F))
  {
    if (rndis_tx_data_size && rndis_tx_ptr)
    {
      USBD_LL_Transmit (pdev, 
                        RNDIS_DATA_IN_EP,                                      
                        rndis_tx_ptr,
                        rndis_tx_data_size);
      rndis_tx_data_size = 0;
      rndis_tx_ptr = NULL;
    }
    else
      if (rndis_tx_ZLP)
      {
        //__disable_irq();          //TODO IRQ
        USBD_LL_Transmit (pdev, 
                          RNDIS_DATA_IN_EP,                                      
                          NULL,
                          0);
        //__enable_irq();   //TODO 
        rndis_tx_ZLP = false;
      }
      else
      {
        usb_eth_stat.txbad--;
        usb_eth_stat.txok++;
        rndis_tx_transmitting = false;
        rndis_tx_ready_cb();
      }
  }
  return USBD_OK;
}

void handle_packet(rndis_data_packet_t *pheader, int size)
{
  if (size < RNDIS_HEADER_SIZE) 
  {
    usb_eth_stat.rxbad++;
    return;
  }
  //To exclude Rx ZLP bug
  if ((pheader->MessageType != REMOTE_NDIS_PACKET_MSG) || 
      ((pheader->MessageLength != size) && (pheader->MessageLength != size - 1))) 
  { 
    usb_eth_stat.rxbad++;
    return;
  }
  size = pheader->MessageLength;
  if (pheader->DataOffset + offsetof(rndis_data_packet_t, DataOffset) + pheader->DataLength != size)
  {
    usb_eth_stat.rxbad++;
    return;
  }
  if (!rndis_rx_started)
  {
    usb_eth_stat.rxbad++;
    return;
  }
  rndis_rx_data_size = pheader->DataLength;
  rndis_rx_started = false;
  
  usb_eth_stat.rxok++;
  rndis_rx_ready_cb();
}

// Data Channel         https://msdn.microsoft.com/en-us/library/windows/hardware/ff546305(v=vs.85).aspx
static uint8_t usbd_rndis_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if (epnum == RNDIS_DATA_OUT_EP)
  {
    PCD_EPTypeDef *ep = &((PCD_HandleTypeDef*)pdev->pData)->OUT_ep[epnum];
    handle_packet((rndis_data_packet_t*)rndis_rx_buffer, RNDIS_RX_BUFFER_SIZE - ep->xfer_len - RNDIS_DATA_OUT_SZ + ep->xfer_count);
  }
  return USBD_OK;
}

// Start Of Frame event management
static uint8_t usbd_rndis_sof(USBD_HandleTypeDef *pdev)
{
  //rndis_send();
  return USBD_OK;
}

static uint8_t rndis_iso_in_incomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  return USBD_OK;
}

static uint8_t rndis_iso_out_incomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  return USBD_OK;
}

/**
* @brief  DeviceQualifierDescriptor 
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
static uint8_t  *usbd_rndis_GetDeviceQualifierDesc (uint16_t *length)
{
  *length = sizeof (USBD_DeviceQualifierDesc);
  return USBD_DeviceQualifierDesc;
}

/**
  * @brief  usbd_rndis_GetCfgDesc 
  *         return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *usbd_rndis_GetCfgDesc (uint16_t *length)
{
  *length = sizeof (usbd_rndis_CfgDesc);
  return usbd_rndis_CfgDesc;
}

void response_available(USBD_HandleTypeDef *pdev)
{
  __disable_irq();
  USBD_LL_Transmit (pdev, 
                    RNDIS_NOTIFICATION_IN_EP,                                      
                    (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00",
                    RNDIS_NOTIFICATION_IN_SZ);
  __enable_irq();   //TODO  
}