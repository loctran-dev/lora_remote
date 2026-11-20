#ifndef __IMAGE_DATA_H__
#define __IMAGE_DATA_H__

#include <stdint.h>
#include "ugui.h"
 typedef struct {
     const uint16_t *data;
     uint16_t width;
     uint16_t height;
     uint8_t dataSize;
     } tImage;
 
extern const UG_BMP bmp_alarm_hoz;
extern const UG_BMP bmp_alarm_ver;
extern const UG_BMP* hourglassGIF[];
extern const UG_BMP bmp_battery_0;
extern const UG_BMP bmp_battery_1;
extern const UG_BMP bmp_battery_2;
extern const UG_BMP bmp_battery_3;
extern const UG_BMP bmp_battery_4;
extern const UG_BMP* batteryGIF[];
extern const UG_BMP bmp_icon_on; 
extern const UG_BMP bmp_icon_off;;
extern const UG_BMP bmp_signal_status_5;

#endif // __IMAGE_DATA_H__
