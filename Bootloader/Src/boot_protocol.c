#include "main.h"
#include "boot_layout.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Stop-and-wait: IRQ only owns one USB packet; flash runs in main context. */
static uint8_t rx[64], tx[128], frame[276];
static volatile uint16_t rxLength;
static uint16_t frameLength;
static uint32_t receivedAt, imageSize, imageCRC, written;
static uint8_t session;

static uint32_t u32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int8_t cdc_init(void)
{
  rxLength = frameLength = 0;
  session = 0;
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx);
  return USBD_OK;
}
static int8_t cdc_deinit(void) { session = 0; return USBD_OK; }
static int8_t cdc_control(uint8_t command, uint8_t *data, uint16_t length)
{
  (void)length;
  if (command == CDC_GET_LINE_CODING)
  {
    const uint8_t coding[7] = {0x00, 0xC2, 0x01, 0, 0, 0, 8};
    memcpy(data, coding, 7);
  }
  return USBD_OK;
}
static int8_t cdc_receive(uint8_t *data, uint32_t *length)
{
  if (*length == 0)
  {
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
  }
  if (*length <= sizeof(rx))
  {
    if (data != rx) memcpy(rx, data, *length);
    __DMB();
    rxLength = (uint16_t)*length;
  }
  return USBD_OK;
}
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
  cdc_init, cdc_deinit, cdc_control, cdc_receive
};

static int reply(const char *format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  int size = vsnprintf((char*)tx, sizeof(tx), format, arguments);
  va_end(arguments);
  if (size < 0 || size >= sizeof(tx)) return 0;
  USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
  if (!cdc || hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || cdc->TxState) return 0;
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx, size);
  if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK) return 0;
  uint32_t start = HAL_GetTick();
  while (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
         hUsbDeviceFS.pClassData && ((USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData)->TxState)
    if (HAL_GetTick() - start > 3000) return 0;
  return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

static int erase(uint32_t address, uint32_t count)
{
  FLASH_EraseInitTypeDef request = {0};
  uint32_t error;
  request.TypeErase = FLASH_TYPEERASE_PAGES;
  request.PageAddress = address;
  request.NbPages = count;
  return HAL_FLASHEx_Erase(&request, &error) == HAL_OK;
}

static int program(uint32_t address, const uint8_t *data, uint32_t size)
{
  for (uint32_t i = 0; i < size; i += 2)
  {
    uint16_t halfword = data[i] | ((uint16_t)(i + 1 < size ? data[i + 1] : 0xFF) << 8);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + i, halfword) != HAL_OK ||
        *(const uint16_t*)(address + i) != halfword) return 0;
  }
  return 1;
}

static void execute(void)
{
  uint32_t command = u32(frame + 4), argument = u32(frame + 8), size = u32(frame + 12);
  const uint8_t *data = frame + 16;
  if (command == 1 && size == 0 && argument == 0)
    reply("OK INFO APP=%08lX MAX=%lu VALID=%u\n", (unsigned long)BOOT_APP_START,
      (unsigned long)BOOT_APP_MAX, Boot_ApplicationValid());
  else if (command == 2 && size == 4 && argument >= 8 && argument <= BOOT_APP_MAX)
  {
    session = 0;
    imageSize = argument;
    imageCRC = u32(data);
    written = 0;
    HAL_FLASH_Unlock();
    /* Invalidate before touching the running image. Never erase boot/config. */
    int ok = erase(BOOT_META_ADDRESS, 1) &&
      erase(BOOT_APP_START, (imageSize + BOOT_PAGE_SIZE - 1) / BOOT_PAGE_SIZE);
    HAL_FLASH_Lock();
    if (ok) { session = 1; reply("OK BEGIN\n"); }
    else reply("ERR ERASE\n");
  }
  else if (command == 3 && session && size && size <= 256 && argument == written &&
           (argument & 1) == 0 && size <= imageSize - written &&
           ((size & 1) == 0 || written + size == imageSize))
  {
    HAL_FLASH_Unlock();
    int ok = program(BOOT_APP_START + written, data, size);
    HAL_FLASH_Lock();
    if (ok) { written += size; reply("OK WRITE %lu\n", (unsigned long)written); }
    else { session = 0; reply("ERR PROGRAM\n"); }
  }
  else if (command == 4 && !argument && !size && session && written == imageSize)
  {
    session = 0;
    if (Boot_CRC32((const void*)BOOT_APP_START, imageSize) != imageCRC)
    { reply("ERR IMAGE_CRC\n"); return; }
    BootMetadata metadata = {imageSize, imageCRC, ~imageSize, BOOT_MAGIC};
    HAL_FLASH_Unlock();
    int ok = program(BOOT_META_ADDRESS, (const uint8_t*)&metadata, sizeof(metadata));
    HAL_FLASH_Lock();
    if (ok && Boot_ApplicationValid()) reply("OK END\n");
    else reply("ERR IMAGE_INVALID\n");
  }
  else if (command == 5 && !argument && !size && !session && Boot_ApplicationValid())
  {
    if (reply("OK RUN\n")) { HAL_Delay(30); NVIC_SystemReset(); }
  }
  else reply("ERR COMMAND_OR_STATE\n");
}

void Boot_ProcessUSB(void)
{
  if (frameLength && HAL_GetTick() - receivedAt > 3000)
  { frameLength = 0; reply("ERR FRAME_TIMEOUT\n"); }
  uint16_t size = rxLength;
  if (!size) return;
  receivedAt = HAL_GetTick();
  if (size > sizeof(frame) - frameLength)
  { frameLength = 0; reply("ERR FRAME_SIZE\n"); }
  else
  {
    memcpy(frame + frameLength, rx, size);
    frameLength += size;
    if (frameLength >= 16)
    {
      uint32_t payload = u32(frame + 12);
      if (memcmp(frame, "UPD1", 4) || payload > 256 || frameLength > 20 + payload)
      { frameLength = 0; reply("ERR FRAME_HEADER\n"); }
      else if (frameLength == 20 + payload)
      {
        if (Boot_CRC32(frame, 16 + payload) != u32(frame + 16 + payload)) reply("ERR FRAME_CRC\n");
        else execute();
        frameLength = 0;
      }
    }
  }
  rxLength = 0;
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx);
  if (hUsbDeviceFS.pClassData) USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}
