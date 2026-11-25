#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "config_store.h"

static long budget = -1;   /* flash operations allowed before a simulated power loss */
static int ops;
static void tick(void) { ops++; }
static int lost(void) { if (budget < 0) return 0; if (budget == 0) return 1; budget--; return 0; }

HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }
uint32_t HAL_FLASH_GetError(void) { return 1; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *init, uint32_t *error)
{
    tick();
    if (lost()) { /* half-erased page */ memset((void *)(uintptr_t)init->PageAddress, 0xFF, 512); return HAL_ERROR; }
    memset((void *)(uintptr_t)init->PageAddress, 0xFF, init->NbPages * FLASH_PAGE_SIZE);
    (void)error; return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t data)
{
    uint32_t *p = (uint32_t *)(uintptr_t)address;
    tick(); (void)type;
    if (*p != 0xFFFFFFFFu) { printf("FAIL: program over non-erased word at %08x\n", address); exit(1); }
    if (lost()) { *p = (uint32_t)data | 0x0F0F0F0Fu; return HAL_ERROR; } /* torn word */
    *p = (uint32_t)data; return HAL_OK;
}

static uint8_t *flash;
static int failures;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

static void erase_all(void) { memset(flash, 0xFF, 0x1000); }
static void boot(void) { memset(&storageData, 0xAA, sizeof(storageData)); Config_Load(); }
static uint32_t seq(int slot) { return *(uint32_t *)(flash + slot * 0x800); }

int main(void)
{
    SystemStorageData_t saved;
    flash = mmap((void *)CONFIG_SLOT_A, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (flash != (void *)CONFIG_SLOT_A) { perror("mmap"); return 2; }

    /* 1. Blank flash: defaults, saved to slot A. */
    erase_all(); boot();
    CHECK(storageData.netIdConfig == 1 && storageData.frequencyConfig == 433 && storageData.sfConfig == 7, "defaults");
    CHECK(storageData.tUpdate == 5 && storageData.tSleepUpdate == 300 && storageData.DeviceConfig[3].isDelete == 1, "defaults 2");
    CHECK(seq(0) == 1, "defaults saved to A with sequence 1");

    /* 2. Alternating saves; reload picks the newest. */
    storageData.tUpdate = 42; CHECK(Config_Save() == 0, "save B");
    CHECK(seq(1) == 2, "B sequence 2");
    storageData.tUpdate = 43; CHECK(Config_Save() == 0, "save A");
    CHECK(seq(0) == 3, "A sequence 3");
    boot(); CHECK(storageData.tUpdate == 43, "newest wins");
    storageData.tUpdate = 44; CHECK(Config_Save() == 0, "save after reload goes to B"); CHECK(seq(1) == 4, "B sequence 4");
    boot(); CHECK(storageData.tUpdate == 44, "reload 44");

    /* 3. Power loss at every flash operation of a save keeps the last committed copy. */
    for (long k = 0; k < 400; k++)
    {
        boot(); memcpy(&saved, &storageData, sizeof(saved));
        storageData.tUpdate = 1000 + k; storageData.MotorConfig[5].name[0] = 'Z';
        ops = 0; budget = k;
        uint32_t r = Config_Save();
        budget = -1;
        boot();
        if (r == 0) { CHECK(storageData.tUpdate == 1000 + k, "completed save visible"); }
        else { CHECK(memcmp(&storageData, &saved, sizeof(saved)) == 0, "interrupted save keeps previous copy"); }
        if (r == 0) break;
    }

    /* 4. Old single-copy format (raw ConfigV2_t, 885 bytes, one settings set per device) is
          converted and written to slot B; slot A is kept until the next save. */
    erase_all();
    uint8_t legacy[885];
    int32_t freq = 868; uint32_t update = 7, sleepUpdate = 77;
    memset(legacy, 0, sizeof(legacy));
    legacy[0] = 9; memcpy(legacy + 1, &freq, 4); legacy[5] = 9;        /* net, frequency, SF */
    memcpy(legacy + 7, &update, 4); memcpy(legacy + 11, &sleepUpdate, 4);
    for (int d = 0; d < MAXIMUM_DEVICE; d++) legacy[15 + d * 41 + 40] = 1;  /* isDelete */
    legacy[15 + 2 * 41 + 0] = 33; legacy[15 + 2 * 41 + 40] = 0;        /* device 2: node 33, paired */
    memcpy(legacy + 15 + 2 * 41 + 10, "Pump", 5);                       /* its name */
    legacy[835] = 1;                                                     /* lastWriteSuccess */
    memcpy(flash, legacy, sizeof(legacy));
    boot(); CHECK(storageData.netIdConfig == 9 && storageData.DeviceConfig[2].nodeIdConfig == 33 && storageData.tSleepUpdate == 77, "legacy read");
    CHECK(Config_Motor(2, 0) != NULL && strcmp(Config_Motor(2, 0)->name, "Pump") == 0, "legacy name moved to motor 0");
    CHECK(Config_Motor(3, 0) == NULL, "deleted device has no motor");
    /* interrupted first save leaves the legacy copy usable */
    budget = 3; storageData.tUpdate = 8; CHECK(Config_Save() != 0, "interrupted migration"); budget = -1;
    boot(); CHECK(storageData.tUpdate == 7 && storageData.netIdConfig == 9, "legacy survives interrupted migration");
    storageData.tUpdate = 8; CHECK(Config_Save() == 0, "migration save");
    CHECK(seq(1) == 1, "migration went to B");
    CHECK(memcmp(flash, legacy, sizeof(legacy)) == 0, "legacy page untouched by migration");
    boot(); CHECK(storageData.tUpdate == 8 && storageData.netIdConfig == 9, "reload after migration");
    storageData.tUpdate = 10; CHECK(Config_Save() == 0, "next save to A"); CHECK(seq(0) == 2, "A sequence 2");
    boot(); CHECK(storageData.tUpdate == 10, "reload 10");

    /* 5. Both slots corrupt and no legacy marker: defaults. */
    flash[0x10 + 3] ^= 1; flash[0x800 + 0x10 + 3] ^= 1;
    boot(); CHECK(storageData.netIdConfig == 1 && storageData.tUpdate == 5, "corrupt -> defaults");

    /* 6. Sequence wrap: 0 is newer than 0xFFFFFFFF. */
    erase_all(); boot();                             /* A seq 1 */
    storageData.tUpdate = 11; Config_Save();          /* B seq 2 */
    /* forge sequences: rewrite headers with wrapped values (CRC covers only the payload) */
    *(uint32_t *)(flash) = 0xFFFFFFFFu; *(uint32_t *)(flash + 0x800) = 0;
    boot(); CHECK(storageData.tUpdate == 11, "wrapped sequence 0 beats 0xFFFFFFFF");

    /* 7. Sanitising out-of-range values. */
    storageData.sfConfig = 3; storageData.frequencyConfig = 50; storageData.tUpdate = 0; storageData.sleepMode = 5;
    Config_Save(); boot();
    CHECK(storageData.sfConfig == 7 && storageData.frequencyConfig == 433 && storageData.tUpdate == 5 && storageData.sleepMode == 0, "sanitised");

    printf("%s (%d failures), sizeof(SystemStorageData_t) = %zu\n", failures ? "FAILED" : "ALL PASSED", failures, sizeof(SystemStorageData_t));
    return failures != 0;
}
