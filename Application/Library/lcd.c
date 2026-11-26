#include "lcd.h"
#include <stdio.h>
#include "utility.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usbd_cdc_if.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
volatile uint32_t lcdTransferErrors, lcdRecoveries;
static uint8_t lcdFault;
static uint32_t lcdRecoveryTick;
#define LCD_IO_TIMEOUT_MS 100u

uint8_t LCD_HasFault(void) { return lcdFault; }
static void LCD_TransferFailed(void)
{
  if (!lcdFault) lcdTransferErrors++;
  lcdFault = 1; /* Skip the rest of this frame, rather than time out per pixel. */
}
static uint8_t LCD_Transmit(uint8_t *data, uint16_t count)
{
  if (lcdFault) return 0;
  if (HAL_SPI_Transmit(&LCD_HANDLE, data, count, LCD_IO_TIMEOUT_MS) != HAL_OK) {
    LCD_TransferFailed();
    return 0;
  }
  return 1;
}
static uint8_t LCD_WaitDMA(void)
{
  uint32_t started = HAL_GetTick();
  while (HAL_DMA_GetState(LCD_HANDLE.hdmatx) != HAL_DMA_STATE_READY ||
         HAL_SPI_GetState(&LCD_HANDLE) != HAL_SPI_STATE_READY) {
    if (HAL_DMA_GetError(LCD_HANDLE.hdmatx) || HAL_SPI_GetError(&LCD_HANDLE) ||
        (uint32_t)(HAL_GetTick() - started) >= LCD_IO_TIMEOUT_MS) {
      LCD_TransferFailed();
      return 0;
    }
  }
  if (HAL_DMA_GetError(LCD_HANDLE.hdmatx) || HAL_SPI_GetError(&LCD_HANDLE)) {
    LCD_TransferFailed();
    return 0;
  }
  return 1;
}

static void LCD_StreamFrame(void);

volatile uint8_t lcdProbeState;
uint8_t lcdProbeBytes[16];

static void LCD_ProbeDelay(void)
{
  for (volatile uint32_t i = 0; i < 40; i++) { __NOP(); }
}

/* Read the bidirectional SDA line at a deliberately slow SPI clock. */
static void LCD_ProbeCommand(uint8_t command, uint8_t *data, uint16_t count)
{
  GPIO_InitTypeDef pin = {0};
  pin.Pin = GPIO_PIN_15;
  pin.Mode = GPIO_MODE_OUTPUT_PP;
  pin.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &pin);
  LCD_PIN(LCD_CS, RESET);
  LCD_PIN(LCD_DC, RESET);
  for (uint8_t bit = 0; bit < 8; bit++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, (command & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    LCD_ProbeDelay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    LCD_ProbeDelay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    command <<= 1;
  }
  /* Release SDA before clocking data out of the controller. */
  pin.Mode = GPIO_MODE_INPUT;
  pin.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &pin);
  LCD_PIN(LCD_DC, SET);
  for (uint16_t i = 0; i < count; i++)
  {
    data[i] = 0;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      LCD_ProbeDelay();
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
      LCD_ProbeDelay();
      data[i] = (data[i] << 1) | (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    }
  }
  LCD_PIN(LCD_CS, SET);
}

void LCD_ProcessProbe(void)
{
  if (lcdProbeState == 3)
  {
    LCD_StreamFrame();
    lcdProbeState = 4;
    return;
  }
  if (lcdProbeState != 1) { return; }
  /* Called between complete renders, with no DMA transfer outstanding. */
  taskENTER_CRITICAL();
  uint32_t savedCR1 = LCD_HANDLE.Instance->CR1;
  uint32_t savedCRH = GPIOB->CRH;
  uint32_t savedODR = GPIOB->ODR;
  GPIO_InitTypeDef pin = {0};
  __HAL_SPI_DISABLE(&LCD_HANDLE);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
  pin.Pin = GPIO_PIN_13;
  pin.Mode = GPIO_MODE_OUTPUT_PP;
  pin.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &pin);
  LCD_ProbeCommand(0x04, &lcdProbeBytes[0], 5);
  LCD_ProbeCommand(0x09, &lcdProbeBytes[5], 6);
  LCD_ProbeCommand(0x0C, &lcdProbeBytes[11], 3);
  LCD_ProbeCommand(0x0B, &lcdProbeBytes[14], 2);
  GPIOB->ODR = savedODR;
  GPIOB->CRH = savedCRH;
  LCD_HANDLE.Instance->CR1 = savedCR1;
  taskEXIT_CRITICAL();
  lcdProbeState = 2;
}
extern SystemInfo_t systemInfo;
extern DeviceInfo_t devicesInfo[MAXIMUM_DEVICE];
/* Arg count, CMD, Args if any */
#if defined USE_ST7735
const uint8_t init_cmd[] = {
    0,  CMD_SLPOUT,
//  3,  CMD_FRMCTR1, 0x01, 0x2C, 0x2D,                     // Standard frame rate
//  3,  CMD_FRMCTR2, 0x01, 0x2C, 0x2D,                     // Standard frame rate
//  6,  CMD_FRMCTR3, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,   // Standard frame rate
    3,  CMD_FRMCTR1, 0x01, 0x01, 0x01,                     // Max
    3,  CMD_FRMCTR2, 0x01, 0x01, 0x01,                     // Max
    6,  CMD_FRMCTR3, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,   // Max frame rate
    1,  CMD_INVCTR,  0x07,
    3,  CMD_PWCTR1,  0xA2, 0x02, 0x84,
    1,  CMD_PWCTR2,  0xC5,
    2,  CMD_PWCTR3,  0x0A, 0x00,
    2,  CMD_PWCTR4,  0x8A, 0x2A,
    2,  CMD_PWCTR5,  0x8A, 0xEE,
    1,  CMD_VMCTR1,  0x0E,
    1,  CMD_INVOFF,  0x00,
    1,  CMD_COLMOD,  0x05,
    2,  CMD_CASET,   0x00, LCD_WIDTH-1,
    2,  CMD_RASET,   0x00, LCD_HEIGHT-1,
    1,  CMD_MADCTL,  LCD_ROTATION_CMD,
    16, CMD_GMCTRP1, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    16, CMD_GMCTRN1, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    0,  CMD_NORON,
};
#elif defined USE_ST7789
const uint8_t init_cmd[] = {
    0,  CMD_SLPOUT,
    1,  CMD_COLMOD,  CMD_COLOR_MODE_16bit,
    5,  CMD_PORCTRL, 0x0C, 0x0C, 0x00, 0x33, 0x33,   // Standard porch
  //5,  CMD_PORCTRL, 0x01, 0x01, 0x00, 0x11, 0x11,   // Minimum porch (7% faster screen refresh rate)
    1,  CMD_GCTRL,   0x35,                           // Gate Control, Default value
    1,  CMD_VCOMS,   0x19,                           // VCOM setting 0.725v (default 0.75v for 0x20)
    1,  CMD_LCMCTRL, 0X2C,                           // LCMCTRL, Default value
    1,  CMD_VDVVRHEN,0x01,                           // VDV and VRH command Enable, Default value
    1,  CMD_VRHS,    0x12,                           // VRH set, +-4.45v (default +-4.1v for 0x0B)
    1,  CMD_VDVS,    0x20,                           // VDV set, Default value
    1,  CMD_FRCTRL2, 0x0F,                           // Frame rate control in normal mode, Default refresh rate (60Hz)
  //1,  CMD_FRCTRL2, 0x01,                           // Frame rate control in normal mode, Max refresh rate (111Hz)
    2,  CMD_PWCTRL1, 0xA4, 0xA1,
    1,  CMD_MADCTL,  LCD_ROTATION_CMD,
    14, CMD_GMCTRP1, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,
    14, CMD_GMCTRN1, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,
    0,  CMD_INVON,
    0,  CMD_NORON
};
#endif

char str[32];

static void LCD_Update(void);
typedef struct{
  int8_t spi_sz;
  int8_t dma_sz;
  int8_t dma_mem_inc;
}config_t;

config_t config = {
    .spi_sz = -1,
    .dma_sz = -1,
    .dma_mem_inc = -1,
};

/* Called in lcdTask with UI ownership. Recover the bus, not the whole board;
   do not reinitialize the current screen or discard unsaved editor contents. */
uint8_t LCD_Recover(void)
{
  if (!lcdFault || (uint32_t)(HAL_GetTick() - lcdRecoveryTick) < 1000u) return 0;
  lcdRecoveryTick = HAL_GetTick();
  HAL_SPI_Abort(&LCD_HANDLE);
  HAL_DMA_Abort(LCD_HANDLE.hdmatx);
  if (HAL_DMA_Init(LCD_HANDLE.hdmatx) != HAL_OK) return 0;
  LCD_PIN(LCD_CS, SET);
  __HAL_RCC_SPI2_FORCE_RESET();
  __HAL_RCC_SPI2_RELEASE_RESET();
  LCD_HANDLE.Init.DataSize = SPI_DATASIZE_8BIT;
  config.spi_sz = config.dma_sz = config.dma_mem_inc = -1;
  if (HAL_SPI_Init(&LCD_HANDLE) != HAL_OK) return 0;
  lcdFault = 0;
  lcdRecoveries++;
  return 1;
}

#ifdef LCD_LOCAL_FB
static uint16_t fb[LCD_WIDTH*LCD_HEIGHT];
#endif

static UG_GUI gui;
static UG_DEVICE device = {
    .x_dim = LCD_WIDTH,
    .y_dim = LCD_HEIGHT,
#ifdef LCD_LOCAL_FB
    .pset = LCD_DrawPixelFB,
#else
    .pset = LCD_DrawPixel,
#endif
    .flush = LCD_Update,
};

#define mode_16bit    1
#define mode_8bit     0
/*
 * @brief Sets SPI interface word size (0=8bit, 1=16 bit)
 * @param none
 * @return none
 */

static void setSPI_Size(int8_t size){
  if (lcdFault) return;
  if(config.spi_sz!=size){
    __HAL_SPI_DISABLE(&LCD_HANDLE);
    config.spi_sz=size;
    if(size==mode_16bit){
      LCD_HANDLE.Init.DataSize = SPI_DATASIZE_16BIT;
      LCD_HANDLE.Instance->CR1 |= SPI_CR1_DFF;
    }
    else{
      LCD_HANDLE.Init.DataSize = SPI_DATASIZE_8BIT;
      LCD_HANDLE.Instance->CR1 &= ~(SPI_CR1_DFF);
    }
  }
}


#ifdef USE_DMA
#define DMA_Min_Pixels    32             // Don't use DMA for small transfers? Setting this to 1 will always use DMA
#define mem_increase      1
#define mem_fixed         0

/**
 * @brief Configures DMA/ SPI interface
 * @param memInc Enable/disable memory address increase
 * @param mode16 Enable/disable 16 bit mode (disabled = 8 bit)
 * @return none
 */
static void setDMAMemMode(uint8_t memInc, uint8_t size)
{
  if (lcdFault) return;
  setSPI_Size(size);
  if(config.dma_sz!=size || config.dma_mem_inc!=memInc){
    config.dma_sz =size;
    config.dma_mem_inc = memInc;
    __HAL_DMA_DISABLE(LCD_HANDLE.hdmatx);;
#ifdef DMA_SxCR_EN
    uint32_t budget = 1000;
    while((LCD_HANDLE.hdmatx->Instance->CR & DMA_SxCR_EN) != RESET && --budget) {}
    if (!budget) { LCD_TransferFailed(); return; }
#elif defined DMA_CCR_EN
    uint32_t budget = 1000;
    while((LCD_HANDLE.hdmatx->Instance->CCR & DMA_CCR_EN) != RESET && --budget) {}
    if (!budget) { LCD_TransferFailed(); return; }
#endif
    if(memInc==mem_increase){
      LCD_HANDLE.hdmatx->Init.MemInc = DMA_MINC_ENABLE;
#ifdef DMA_SxCR_EN
      LCD_HANDLE.hdmatx->Instance->CR |= DMA_SxCR_MINC;
#elif defined DMA_CCR_EN
      LCD_HANDLE.hdmatx->Instance->CCR |= DMA_CCR_MINC;
#endif
    }
    else{
      LCD_HANDLE.hdmatx->Init.MemInc = DMA_MINC_DISABLE;
#ifdef DMA_SxCR_EN
      LCD_HANDLE.hdmatx->Instance->CR &= ~(DMA_SxCR_MINC);
#elif defined DMA_CCR_EN
      LCD_HANDLE.hdmatx->Instance->CCR &= ~(DMA_CCR_MINC);
#endif
    }

    if(size==mode_16bit){
      LCD_HANDLE.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
      LCD_HANDLE.hdmatx->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
#ifdef DMA_SxCR_EN
      LCD_HANDLE.hdmatx->Instance->CR = (LCD_HANDLE.hdmatx->Instance->CR & ~(DMA_SxCR_PSIZE_Msk | DMA_SxCR_MSIZE_Msk)) |
                                                   (1<<DMA_SxCR_PSIZE_Pos | 1<<DMA_SxCR_MSIZE_Pos);
#elif defined DMA_CCR_EN
      LCD_HANDLE.hdmatx->Instance->CCR = (LCD_HANDLE.hdmatx->Instance->CCR & ~(DMA_CCR_PSIZE_Msk | DMA_CCR_MSIZE_Msk)) |
                                                   (1<<DMA_CCR_PSIZE_Pos | 1<<DMA_CCR_MSIZE_Pos);
#endif

    }
    else{
      LCD_HANDLE.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
      LCD_HANDLE.hdmatx->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
#ifdef DMA_SxCR_EN
      LCD_HANDLE.hdmatx->Instance->CR = (LCD_HANDLE.hdmatx->Instance->CR & ~(DMA_SxCR_PSIZE_Msk | DMA_SxCR_MSIZE_Msk));
#elif defined DMA_CCR_EN
      LCD_HANDLE.hdmatx->Instance->CCR = (LCD_HANDLE.hdmatx->Instance->CCR & ~(DMA_CCR_PSIZE_Msk | DMA_CCR_MSIZE_Msk));
#endif
    }
  }
}
#endif

/**
 * @brief Write command to ST7735 controller
 * @param cmd -> command to write
 * @return none
 */
static void LCD_WriteCommand(uint8_t *cmd, uint8_t argc)
{
  if (lcdFault) return;
  setSPI_Size(mode_8bit);
  LCD_PIN(LCD_DC,RESET);
#ifdef LCD_CS
  LCD_PIN(LCD_CS,RESET);
#endif
  uint8_t sent = LCD_Transmit(cmd, 1);
  if(argc && sent){
    LCD_PIN(LCD_DC,SET);
    LCD_Transmit(cmd+1, argc);
  }
#ifdef LCD_CS
  LCD_PIN(LCD_CS,SET);
#endif
}

/**
 * @brief Write data to ST7735 controller
 * @param buff -> pointer of data buffer
 * @param buff_size -> size of the data buffer
 * @return none
 */
static void LCD_WriteData(uint8_t *buff, size_t buff_size)
{
  if (lcdFault) return;
  LCD_PIN(LCD_DC,SET);
#ifdef LCD_CS
  LCD_PIN(LCD_CS,RESET);
#endif

  // split data in small chunks because HAL can't send more than 64K at once

  while (buff_size > 0) {
    uint16_t chunk_size = buff_size > 65535 ? 65535 : buff_size;
#ifdef USE_DMA
    if(buff_size>DMA_Min_Pixels){
      if (HAL_SPI_Transmit_DMA(&LCD_HANDLE, buff, chunk_size) != HAL_OK) LCD_TransferFailed();
      if (lcdFault || !LCD_WaitDMA()) break;
      if(config.dma_mem_inc==mem_increase){
        if(config.dma_sz==mode_16bit)
          buff += chunk_size*2;
        else
          buff += chunk_size;
      }
    }
    else{
      if (!LCD_Transmit(buff, chunk_size)) break;
      if(config.spi_sz==mode_16bit)
        buff += chunk_size*2;
      else
        buff += chunk_size;
    }
#else
    if (!LCD_Transmit(buff, chunk_size)) break;
    buff += chunk_size * (config.spi_sz == mode_16bit ? 2 : 1);
#endif
    buff_size -= chunk_size;
  }
#ifdef LCD_CS
  LCD_PIN(LCD_CS,SET);
#endif
}

/**
 * @brief Write data to ST7735 controller, simplify for 8bit data.
 * data -> data to write
 * @return none
 */
/*
static void LCD_ReadCmd(uint8_t cmd, uint8_t *data, uint8_t count)
{
  setSPI_Size(mode_8bit);
  LCD_PIN(LCD_CS,RESET);
  LCD_PIN(LCD_DC,RESET);
  HAL_SPI_Transmit(&LCD_HANDLE, &cmd, sizeof(cmd), HAL_MAX_DELAY);
  LCD_PIN(LCD_DC,SET);
  HAL_SPI_Receive(&LCD_HANDLE, data, count, HAL_MAX_DELAY);
  LCD_PIN(LCD_CS,SET);
}
*/

/**
 * @brief Set the rotation direction of the display
 * @param m -> rotation parameter(please refer it in ST7735.h)
 * @return none
 */
void LCD_SetRotation(uint8_t m)
{
  uint8_t cmd[] = { CMD_MADCTL, 0};

  m = m % 4; // can't be higher than 3

  switch (m)
  {
  case 0:
#if LCD_IS_160X80
    cmd[1] = CMD_MADCTL_MX | CMD_MADCTL_MY | CMD_MADCTL_BGR;
#else
    cmd[1] = CMD_MADCTL_MX | CMD_MADCTL_MY | CMD_MADCTL_RGB;
#endif
    break;
  case 1:
#if CMD_IS_160X80
    cmd[1] = CMD_MADCTL_MY | CMD_MADCTL_MV | CMD_MADCTL_BGR;
#else
    cmd[1] = CMD_MADCTL_MY | CMD_MADCTL_MV | CMD_MADCTL_RGB;
#endif
    break;
  case 2:
#if CMD_IS_160X80
    cmd[1] = CMD_MADCTL_BGR;
#else
    cmd[1] = CMD_MADCTL_RGB;
#endif
    break;
  case 3:
#if CMD_IS_160X80
    cmd[1] = CMD_MADCTL_MX | CMD_MADCTL_MV | CMD_MADCTL_BGR;
#else
    cmd[1] = CMD_MADCTL_MX | CMD_MADCTL_MV | CMD_MADCTL_RGB;
#endif
    break;
  }
  LCD_WriteCommand(cmd, sizeof(cmd)-1);
}


/**
 * @brief Set address of DisplayWindow
 * @param xi&yi -> coordinates of window
 * @return none
 */
static void LCD_SetAddressWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  int16_t x_start = x0 + LCD_X_SHIFT, x_end = x1 + LCD_X_SHIFT;
  int16_t y_start = y0 + LCD_Y_SHIFT, y_end = y1 + LCD_Y_SHIFT;

  /* Column Address set */
  {
    uint8_t cmd[] = { CMD_CASET, x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF };
    LCD_WriteCommand(cmd, sizeof(cmd)-1);
  }
  /* Row Address set */
  {
    uint8_t cmd[] = { CMD_RASET, y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF };
    LCD_WriteCommand(cmd, sizeof(cmd)-1);
  }
  {
  /* Write to RAM */
    uint8_t cmd[] = { CMD_RAMWR };
    LCD_WriteCommand(cmd, sizeof(cmd)-1);
  }
}


/**
 * @brief Address and draw a Pixel
 * @param x&y -> coordinate to Draw
 * @param color -> color of the Pixel
 * @return none
 */
static uint8_t LCD_SendFrameBlock(uint8_t *data, uint16_t length)
{
  uint32_t started = HAL_GetTick();
  uint8_t status;
  do
  {
    status = CDC_Transmit_FS(data, length);
    if (HAL_GetTick() - started > 2000) { return 0; }
  } while (status == USBD_BUSY);
  if (status != USBD_OK) { return 0; }
  /* The buffer must remain unchanged until USB IN completes. */
  USBD_CDC_HandleTypeDef *cdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  while (cdc && cdc->TxState)
  {
    if (HAL_GetTick() - started > 2000) { return 0; }
    cdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  }
  return cdc != NULL;
}

static void LCD_StreamFrame(void)
{
  static uint8_t raw[LCD_WIDTH * 3 + 1];
  static uint8_t pixels[LCD_WIDTH * 2];
  static uint8_t header[12] = {'L','C','F','1', LCD_WIDTH, 0, LCD_HEIGHT, 0,
    (LCD_WIDTH * LCD_HEIGHT * 2) & 255,
    ((LCD_WIDTH * LCD_HEIGHT * 2) >> 8) & 255, 0, 0};
  /* Freeze UI tasks while retaining USB/DMA interrupts. */
  vTaskSuspendAll();
  if (LCD_SendFrameBlock(header, sizeof(header)))
  {
    for (uint16_t y = 0; y < LCD_HEIGHT; y++)
    {
      LCD_SetAddressWindow(0, y, LCD_WIDTH - 1, y);
      taskENTER_CRITICAL();
      uint32_t savedCR1 = LCD_HANDLE.Instance->CR1;
      uint32_t savedCRH = GPIOB->CRH;
      uint32_t savedODR = GPIOB->ODR;
      GPIO_InitTypeDef pin = {0};
      __HAL_SPI_DISABLE(&LCD_HANDLE);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
      pin.Pin = GPIO_PIN_13;
      pin.Mode = GPIO_MODE_OUTPUT_PP;
      pin.Speed = GPIO_SPEED_FREQ_LOW;
      HAL_GPIO_Init(GPIOB, &pin);
      LCD_ProbeCommand(CMD_RAMRD, raw, sizeof(raw));
      GPIOB->ODR = savedODR;
      GPIOB->CRH = savedCRH;
      LCD_HANDLE.Instance->CR1 = savedCR1;
      taskEXIT_CRITICAL();
      for (uint16_t x = 0; x < LCD_WIDTH; x++)
      {
        uint16_t rgb565 = ((raw[1 + x * 3] & 0xF8) << 8) |
          ((raw[2 + x * 3] & 0xFC) << 3) | (raw[3 + x * 3] >> 3);
        pixels[x * 2] = rgb565 & 255;
        pixels[x * 2 + 1] = rgb565 >> 8;
      }
      if (!LCD_SendFrameBlock(pixels, sizeof(pixels))) { break; }
    }
  }
  xTaskResumeAll();
}

void LCD_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
  if (lcdFault) return;
  if ((x < 0) || (x > LCD_WIDTH-1) ||
     (y < 0) || (y > LCD_HEIGHT-1))
    return;

  uint8_t data[2] = {color >> 8, color & 0xFF};

  LCD_SetAddressWindow(x, y, x, y);

  LCD_PIN(LCD_DC,SET);
#ifdef LCD_CS
  LCD_PIN(LCD_CS,RESET);
#endif
  LCD_Transmit(data, sizeof(data));
#ifdef LCD_CS
  LCD_PIN(LCD_CS,SET);
#endif
}

#ifdef LCD_LOCAL_FB
void LCD_DrawPixelFB(int16_t x, int16_t y, uint16_t color)
{
  if ((x < 0) || (x >= LCD_WIDTH) ||
     (y < 0) || (y >= LCD_HEIGHT)) return;

  fb[x+(y*LCD_WIDTH)] = color;
}
#endif

void LCD_FillPixels(uint32_t pixels, uint16_t color){
#ifdef USE_DMA
  if(pixels>DMA_Min_Pixels)
    LCD_WriteData((uint8_t*)&color, pixels);
  else{
#endif
    uint16_t fill[DMA_Min_Pixels];                                                                // Use a pixel buffer for faster filling, removes overhead.
    for(uint32_t t=0;t<(pixels<DMA_Min_Pixels ? pixels : DMA_Min_Pixels);t++){                     // Fill the buffer with the color
      fill[t]=color;
    }
    while(pixels){                                                                                // Send 64 pixel blocks
      uint32_t sz = (pixels<DMA_Min_Pixels ? pixels : DMA_Min_Pixels);
      LCD_WriteData((uint8_t*)fill, sz);
      pixels-=sz;
    }
#ifdef USE_DMA
  }
#endif
}

/**
 * @brief Set address of DisplayWindow and returns raw pixel draw for uGUI driver acceleration
 * @param xi&yi -> coordinates of window
 * @return none
 */
void(*LCD_FillArea(int16_t x0, int16_t y0, int16_t x1, int16_t y1))(uint32_t, uint16_t){
  if(x0==-1){
#ifdef USE_DMA
    setDMAMemMode(mem_increase, mode_8bit);
#else
    setSPI_Size(mode_8bit);                                                          // Set SPI to 8 bit
#endif
    return NULL;
  }
  LCD_SetAddressWindow(x0,y0,x1,y1);
#ifdef USE_DMA
    setDMAMemMode(mem_fixed, mode_16bit);
#else
    setSPI_Size(mode_16bit);                                                          // Set SPI to 16 bit
#endif
  LCD_PIN(LCD_DC,SET);
  return LCD_FillPixels;
}


/**
 * @brief Fill an Area with single color
 * @param xSta&ySta -> coordinate of the start point
 * @param xEnd&yEnd -> coordinate of the end point
 * @param color -> color to Fill with
 * @return none
 */
int8_t LCD_Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
  uint32_t pixels = (uint32_t)(xEnd-xSta+1)*(yEnd-ySta+1);
  LCD_SetAddressWindow(xSta, ySta, xEnd, yEnd);
#ifdef USE_DMA
    setDMAMemMode(mem_fixed, mode_16bit);
#else
    setSPI_Size(mode_16bit);
#endif
  LCD_FillPixels(pixels, color);
#ifdef USE_DMA
  setDMAMemMode(mem_increase, mode_8bit);
#else
  setSPI_Size(mode_8bit);
#endif
  return UG_RESULT_OK;
}


/**
 * @brief Draw an Image on the screen
 * @param x&y -> start point of the Image
 * @param w&h -> width & height of the Image to Draw
 * @param data -> pointer of the Image array
 * @return none
 */
void LCD_DrawImage(uint16_t x, uint16_t y, UG_BMP* bmp)
{
  uint16_t w = bmp->width;
  uint16_t h = bmp->height;
  if ((x > LCD_WIDTH-1) || (y > LCD_HEIGHT-1))
    return;
  if ((x + w - 1) > LCD_WIDTH-1)
    return;
  if ((y + h - 1) > LCD_HEIGHT-1)
    return;
  if(bmp->bpp!=BMP_BPP_16)
    return;
  LCD_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  #ifdef USE_DMA
  setDMAMemMode(mem_increase, mode_16bit);                                                            // Set SPI and DMA to 16 bit, enable memory increase
  #else
  setSPI_Size(mode_16bit);                                                                            // Set SPI to 16 bit
  #endif
  LCD_WriteData((uint8_t*)bmp->p, w*h);
#ifdef USE_DMA
setDMAMemMode(mem_increase, mode_8bit);                                                            // Set SPI and DMA to 16 bit, enable memory increase
#else
setSPI_Size(mode_8bit);                                                                            // Set SPI to 16 bit
#endif
  }

/**
 * @brief Accelerated line draw using filling (Only for vertical/horizontal lines)
 * @param x1&y1 -> coordinate of the start point
 * @param x2&y2 -> coordinate of the end point
 * @param color -> color of the line to Draw
 * @return none
 */
int8_t LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {

  if(x0==x1){                                   // If horizontal
    if(y0>y1) swap(y0,y1);
  }
  else if(y0==y1){                              // If vertical
    if(x0>x1) swap(x0,x1);
  }
  else{                                         // Else, return fail, draw using software
    return UG_RESULT_FAIL;
  }

  LCD_Fill(x0,y0,x1,y1,color);               // Draw using acceleration
  return UG_RESULT_OK;
}
void LCD_PutChar(uint16_t x, uint16_t y, char ch, UG_FONT* font, uint16_t color, uint16_t bgcolor){
  UG_FontSelect(font);
  UG_PutChar(ch, x, y, color, bgcolor);
}

void LCD_PutStr(uint16_t x, uint16_t y,  char *str, UG_FONT* font, uint16_t color, uint16_t bgcolor){
  UG_FontSelect(font);
  UG_SetForecolor(color);
  UG_SetBackcolor(bgcolor);
  UG_PutString(x, y, str);
}
/**
 * @brief Invert Fullscreen color
 * @param invert -> Whether to invert
 * @return none
 */
void LCD_InvertColors(uint8_t invert)
{
  uint8_t cmd[] = { (invert ? CMD_INVON /* INVON */ : CMD_INVOFF /* INVOFF */) };
  LCD_WriteCommand(cmd, sizeof(cmd)-1);
}

/*
 * @brief Open/Close tearing effect line
 * @param tear -> Whether to tear
 * @return none
 */
void LCD_TearEffect(uint8_t tear)
{
  uint8_t cmd[] = { (tear ? 0x35 /* TEON */ : 0x34 /* TEOFF */) };
  LCD_WriteCommand(cmd, sizeof(cmd)-1);
}

void LCD_setPower(uint8_t power)
{
  uint8_t cmd[] = { (power ? CMD_DISPON /* TEON */ : CMD_DISPOFF /* TEOFF */) };
  LCD_WriteCommand(cmd, sizeof(cmd)-1);
}

void LCD_Backlight_On(void)
{
#ifdef LCD_BL
  LCD_PIN(LCD_BL,SET);
#endif
}
void LCD_Backlight_Off(void)
{
#ifdef LCD_BL
  LCD_PIN(LCD_BL,RESET);
#endif
}

static void LCD_Update(void)
{
#ifdef LCD_LOCAL_FB
  setSPI_Size(mode_8bit);
  LCD_SetAddressWindow(0,0,LCD_WIDTH-1,LCD_HEIGHT-1);
  #ifdef USE_DMA
  setDMAMemMode(mem_increase, mode_16bit);                                                            // Set SPI and DMA to 16 bit, enable memory increase
  #else
  setSPI_Size(mode_16bit);                                                                            // Set SPI to 16 bit
  #endif
  LCD_WriteData((uint8_t*)fb, LCD_WIDTH*LCD_HEIGHT);
#endif
  #ifdef USE_DMA
  setDMAMemMode(mem_increase, mode_8bit);                                                            // Set SPI and DMA to 16 bit, enable memory increase
  #else
  setSPI_Size(mode_8bit);                                                                            // Set SPI to 16 bit
  #endif
}
/**
 * @brief Initialize ST7735 controller
 * @param none
 * @return none
 */

void LCD_init(void)
{
#ifdef LCD_CS
  LCD_PIN(LCD_CS,SET);
#endif
#ifdef LCD_RST
  LCD_PIN(LCD_RST,RESET);
  HAL_Delay(1);
  LCD_PIN(LCD_RST,SET);
  HAL_Delay(200);
#endif
  UG_Init(&gui, &device);
#ifndef LCD_LOCAL_FB
  UG_DriverRegister(DRIVER_DRAW_LINE, LCD_DrawLine);
  UG_DriverRegister(DRIVER_FILL_FRAME, LCD_Fill);
  UG_DriverRegister(DRIVER_FILL_AREA, LCD_FillArea);
  UG_DriverRegister(DRIVER_DRAW_BMP, LCD_DrawImage);
#endif
  UG_FontSetHSpace(0);
  UG_FontSetVSpace(0);
  for(uint16_t i=0; i<sizeof(init_cmd); ){
    LCD_WriteCommand((uint8_t*)&init_cmd[i+1], init_cmd[i]);
    i += init_cmd[i]+2;
  }
  UG_FillScreen(C_BLACK);               //  Clear screen
  LCD_setPower(ENABLE);
  UG_Update();
}


#define DEFAULT_FONT FONT_6X8

static uint32_t draw_time = 0;
static void clearTime(void){
  draw_time = HAL_GetTick();
}
static void printTime(void){
  char str[8];
  uint8_t t = UG_FontGetTransparency();
  sprintf(str,"%dums", HAL_GetTick()-draw_time);
  UG_FontSelect(DEFAULT_FONT);
  UG_SetForecolor(C_YELLOW);
  UG_SetBackcolor(C_BLACK);
  UG_FontSetTransparency(0);
  UG_PutString(10, 15, str);
  UG_FontSetTransparency(t);
  UG_Update();
}

