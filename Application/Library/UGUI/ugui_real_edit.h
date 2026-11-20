#ifndef __UGUI_REAL_EDIT_H
#define __UGUI_REAL_EDIT_H

#include "ugui.h"

/* -------------------------------------------------------------------------------- */
/* -- TEXTBOX OBJECT                                                             -- */
/* -------------------------------------------------------------------------------- */
/* Textbox structure */
typedef struct
{
   char str[30];
   UG_S32 val;
   UG_S32 min;
   UG_S32 max;
   UG_U8 fieldWidth;
   UG_U8 fieldPrecision;
   UG_U8 charWidth;
   UG_FONT *font; 
   UG_U8 style;
   UG_COLOR fc;
   UG_COLOR bc;
   UG_U8 align;
   UG_S8 h_space;
   UG_S8 v_space;
   UG_U8 cursorVisible;
   UG_U16 cursorPos;
   UG_COLOR cursorColor;
   UG_COLOR cfc;
   UG_COLOR cbc;
} UG_REALEDIT;

/* Object type */
#define OBJ_TYPE_REAL_EDIT 7

/* Default textbox IDs */
#define REDT_ID_0 OBJ_ID_0
#define REDT_ID_1 OBJ_ID_1
#define REDT_ID_2 OBJ_ID_2
#define REDT_ID_3 OBJ_ID_3
#define REDT_ID_4 OBJ_ID_4
#define REDT_ID_5 OBJ_ID_5
#define REDT_ID_6 OBJ_ID_6
#define REDT_ID_7 OBJ_ID_7
#define REDT_ID_8 OBJ_ID_8
#define REDT_ID_9 OBJ_ID_9
#define REDT_ID_10 OBJ_ID_10
#define REDT_ID_11 OBJ_ID_11
#define REDT_ID_12 OBJ_ID_12
#define REDT_ID_13 OBJ_ID_13
#define REDT_ID_14 OBJ_ID_14
#define REDT_ID_15 OBJ_ID_15
#define REDT_ID_16 OBJ_ID_16
#define REDT_ID_17 OBJ_ID_17
#define REDT_ID_18 OBJ_ID_18
#define REDT_ID_19 OBJ_ID_19

/* Textbox functions */
UG_RESULT UG_RealEditCreate(UG_WINDOW *wnd, UG_REALEDIT *rEdt, UG_U8 id, UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye,
                            UG_U8 fieldWidth, UG_U8 fieldPrecision);
UG_RESULT UG_RealEditDelete(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditShow(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditHide(UG_WINDOW *wnd, UG_U8 id);
UG_U8 UG_RealEditCheckVisible(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditSetForeColor(UG_WINDOW *wnd, UG_U8 id, UG_COLOR fc);
UG_RESULT UG_RealEditSetBackColor(UG_WINDOW *wnd, UG_U8 id, UG_COLOR bc);
UG_RESULT UG_RealEditSetText(UG_WINDOW *wnd, UG_U8 id, char *str);
UG_RESULT UG_RealEditSetValue(UG_WINDOW *wnd, UG_U8 id, float value);
float UG_RealEditGetValue(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditSetRange(UG_WINDOW *wnd, UG_U8 id, float min, float max);
UG_RESULT UG_RealEditGetRange(UG_WINDOW *wnd, UG_U8 id, float *min, float *max);
UG_RESULT UG_RealEditSetFont(UG_WINDOW *wnd, UG_U8 id, UG_FONT *font);
UG_RESULT UG_RealEditSetHSpace(UG_WINDOW *wnd, UG_U8 id, UG_S8 hs);
UG_RESULT UG_RealEditSetVSpace(UG_WINDOW *wnd, UG_U8 id, UG_S8 vs);
UG_RESULT UG_RealEditSetAlignment(UG_WINDOW *wnd, UG_U8 id, UG_U8 align);
UG_COLOR UG_RealEditGetForeColor(UG_WINDOW *wnd, UG_U8 id);
UG_COLOR UG_RealEditGetBackColor(UG_WINDOW *wnd, UG_U8 id);
char *UG_RealEditGetText(UG_WINDOW *wnd, UG_U8 id);
UG_FONT *UG_RealEditGetFont(UG_WINDOW *wnd, UG_U8 id);
UG_S8 UG_RealEditGetHSpace(UG_WINDOW *wnd, UG_U8 id);
UG_S8 UG_RealEditGetVSpace(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditShowCursor(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditHideCursor(UG_WINDOW *wnd, UG_U8 id);
UG_RESULT UG_RealEditSetCursorForeColor(UG_WINDOW *wnd, UG_U8 id, UG_COLOR cfc);
UG_RESULT UG_RealEditSetCursorBackColor(UG_WINDOW *wnd, UG_U8 id, UG_COLOR cbc);
UG_RESULT UG_RealEditSetCharWidth(UG_WINDOW *wnd, UG_U8 id, UG_U8 charWidth);
UG_RESULT UG_RealEditSetCursorPosition(UG_WINDOW *wnd, UG_U8 id, UG_U16 pos);
#endif // __UGUI_REAL_EDIT_H
