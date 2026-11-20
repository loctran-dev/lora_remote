#ifndef BOOT_CDC_IF_H
#define BOOT_CDC_IF_H
#include "usbd_cdc.h"
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
void Boot_ProcessUSB(void);
#endif
