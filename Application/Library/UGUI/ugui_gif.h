#ifndef __UGUI_GIF_H
#define __UGUI_GIF_H

#include "ugui.h"

/* -------------------------------------------------------------------------------- */
/* -- IMAGE OBJECT                                                               -- */
/* -------------------------------------------------------------------------------- */
/* Image structure */
typedef struct
{
   UG_U8 type;
	 const UG_BMP** frames;    
	 uint16_t numFrames;     
	 uint16_t currentFrame;  
	 uint16_t delayMs;       
	 uint32_t lastTime; 
	 UG_U8 loadBMPState;
} UG_GIF;

/* Object type */
#define OBJ_TYPE_GIF                                6

/* Default image IDs */
#define GIF_ID_0                                      OBJ_ID_0
#define GIF_ID_1                                      OBJ_ID_1
#define GIF_ID_2                                      OBJ_ID_2
#define GIF_ID_3                                      OBJ_ID_3
#define GIF_ID_4                                      OBJ_ID_4
#define GIF_ID_5                                      OBJ_ID_5
#define GIF_ID_6                                      OBJ_ID_6
#define GIF_ID_7                                      OBJ_ID_7
#define GIF_ID_8                                      OBJ_ID_8
#define GIF_ID_9                                      OBJ_ID_9
#define GIF_ID_10                                     OBJ_ID_10
#define GIF_ID_11                                     OBJ_ID_11
#define GIF_ID_12                                     OBJ_ID_12
#define GIF_ID_13                                     OBJ_ID_13
#define GIF_ID_14                                     OBJ_ID_14
#define GIF_ID_15                                     OBJ_ID_15
#define GIF_ID_16                                     OBJ_ID_16
#define GIF_ID_17                                     OBJ_ID_17
#define GIF_ID_18                                     OBJ_ID_18
#define GIF_ID_19                                     OBJ_ID_19

/* Image types */
#define GIF_TYPE_BMP                                  (1<<0)
#define GIF_STATE_RUN                                  (0)
#define GIF_STATE_STOP                                  (0)
/* Image functions */
UG_RESULT UG_GifCreate( UG_WINDOW* wnd, UG_GIF* img, UG_U8 id, UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye );
UG_RESULT UG_GifDelete( UG_WINDOW* wnd, UG_U8 id );
UG_RESULT UG_GifShow( UG_WINDOW* wnd, UG_U8 id );
UG_RESULT UG_GifHide( UG_WINDOW* wnd, UG_U8 id );
UG_U8 UG_GifCheckVisible( UG_WINDOW* wnd, UG_U8 id );
UG_RESULT UG_GifSetFrames( UG_WINDOW* wnd, UG_U8 id, const UG_BMP* bmp[], uint16_t numFrames);
UG_RESULT UG_GifStart(UG_WINDOW* wnd, UG_U8 id, int32_t startFrame);
UG_RESULT UG_GifStop(UG_WINDOW* wnd, UG_U8 id, int32_t stopFrame);
UG_RESULT UG_GifSetDelayTime(UG_WINDOW* wnd, UG_U8 id, uint16_t delayMs);
UG_RESULT UG_GifLoadBMP(UG_WINDOW* wnd, UG_U8 id, uint32_t currentTime);
#endif // __UGUI_GIF_H
