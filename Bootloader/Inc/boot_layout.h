#ifndef BOOT_LAYOUT_H
#define BOOT_LAYOUT_H
#include <stdint.h>
#define BOOT_APP_START 0x08008000u
#define BOOT_META_ADDRESS 0x0803E800u
#define BOOT_CONFIG_START 0x0803F000u
#define BOOT_APP_MAX (BOOT_META_ADDRESS - BOOT_APP_START)
#define BOOT_PAGE_SIZE 2048u
#define BOOT_MAGIC 0x57425043u
#define BOOT_REQUEST 0xB007u
typedef struct { uint32_t length, crc, inverted_length, magic; } BootMetadata;
uint32_t Boot_CRC32(const void *data, uint32_t size);
int Boot_ApplicationValid(void);
void Boot_RunApplication(void);
#endif
