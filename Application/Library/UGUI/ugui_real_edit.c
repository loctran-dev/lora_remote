#include "ugui_real_edit.h"
#include <stdlib.h>
#include <string.h>
/* -------------------------------------------------------------------------------- */
/* -- Textbox FUNCTIONS                                                          -- */
/* -------------------------------------------------------------------------------- */

/* Static functions */
static void _UG_RealEditUpdate(UG_WINDOW* wnd, UG_OBJECT* obj);
static int _UG_RealEditIntPow(int base, int exponent);
static UG_RESULT _UG_RealEditFloatToFixed(float value, UG_U8 precision, UG_S32* fixedValue);
static float _UG_RealEditFixedToFloat(UG_S32 fixedValue, UG_U8 precision);

static int _UG_RealEditIntPow(int base, int exponent)
{
   int result = 1;

   if ( exponent < 0 ) return 0;

   while ( exponent > 0 )
   {
      result *= base;
      exponent--;
   }

   return result;
}

static UG_RESULT _UG_RealEditFloatToFixed(float value, UG_U8 precision, UG_S32* fixedValue)
{
   int scale;
   double scaledValue;

   if ( precision > 9 ) return UG_RESULT_FAIL;
   scale = _UG_RealEditIntPow(10, precision);
   scaledValue = (double)value * scale;

   if ( scaledValue > 2147483647.0 || scaledValue < -2147483648.0 )
      return UG_RESULT_FAIL;

   *fixedValue = (UG_S32)(scaledValue + (scaledValue >= 0.0 ? 0.5 : -0.5));
   return UG_RESULT_OK;
}

static float _UG_RealEditFixedToFloat(UG_S32 fixedValue, UG_U8 precision)
{
   int scale;

   if ( precision > 9 ) return 0.0f;
   scale = _UG_RealEditIntPow(10, precision);
   return (float)fixedValue / (float)scale;
}

UG_RESULT UG_RealEditCreate( UG_WINDOW* wnd, UG_REALEDIT* rEdt, UG_U8 id, UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye,
                            UG_U8 fieldWidth, UG_U8 fieldPrecision )
{
   UG_OBJECT* obj;

   obj = _UG_GetFreeObject( wnd );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   /* Initialize object-specific parameters */
   //rEdt->str = NULL;
   rEdt->font = UG_GetGUI() != NULL ? (UG_GetGUI()->font) : NULL;
   rEdt->style = 0; /* reserved */
   rEdt->fc = wnd->fc;
   rEdt->bc = wnd->bc;
   rEdt->align = ALIGN_CENTER;
   rEdt->h_space = 0;
   rEdt->v_space = 0;
   rEdt->fieldWidth = fieldWidth;
   rEdt->fieldPrecision = fieldPrecision;
   rEdt->charWidth = 15;
   rEdt->val = 0;
   rEdt->min = 0;
   rEdt->max = _UG_RealEditIntPow(10, fieldWidth) - 1;
	rEdt->cursorVisible = 0x01u;
   rEdt->cursorPos = 0;
   rEdt->cfc = wnd->fc;
   rEdt->cbc = wnd->bc;
   rEdt->str[0] = '\0';

   /* Initialize standard object parameters */
   obj->update = _UG_RealEditUpdate;
   #ifdef UGUI_USE_TOUCH
   obj->touch_state = OBJ_TOUCH_STATE_INIT;
   #endif
   obj->type = OBJ_TYPE_REAL_EDIT;
   obj->event = OBJ_EVENT_NONE;
   obj->a_rel.xs = xs;
   obj->a_rel.ys = ys;
   obj->a_rel.xe = xe;
   obj->a_rel.ye = ye;
   obj->a_abs.xs = -1;
   obj->a_abs.ys = -1;
   obj->a_abs.xe = -1;
   obj->a_abs.ye = -1;
   obj->id = id;
   obj->state |= OBJ_STATE_VISIBLE | OBJ_STATE_REDRAW | OBJ_STATE_VALID;
   obj->data = (void*)rEdt;

   /* Update function: Do your thing! */
   obj->state &= ~OBJ_STATE_FREE;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditDelete( UG_WINDOW* wnd, UG_U8 id )
{
   return _UG_DeleteObject( wnd, OBJ_TYPE_REAL_EDIT, id );
}

UG_RESULT UG_RealEditShow( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   obj->state |= OBJ_STATE_VISIBLE;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditHide( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   obj->state &= ~OBJ_STATE_VISIBLE;
   obj->state |= OBJ_STATE_UPDATE;

   return UG_RESULT_OK;
}

UG_U8 UG_RealEditCheckVisible( UG_WINDOW* wnd, UG_U8 id )
{
	UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return 0u;

   return obj->state & OBJ_STATE_VISIBLE;;
}

UG_RESULT UG_RealEditSetForeColor( UG_WINDOW* wnd, UG_U8 id, UG_COLOR fc )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	 if (rEdt->fc != fc)
	 {
		 rEdt->fc = fc;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetBackColor( UG_WINDOW* wnd, UG_U8 id, UG_COLOR bc )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   rEdt->bc = bc;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetText( UG_WINDOW* wnd, UG_U8 id, char* str )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	if (strcmp(rEdt->str, str) != 0)
	{
	 strcpy(rEdt->str, str);
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	}

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetValue( UG_WINDOW* wnd, UG_U8 id, float value )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_S32 fixedValue;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   if ( _UG_RealEditFloatToFixed(value, rEdt->fieldPrecision, &fixedValue) != UG_RESULT_OK )
      return UG_RESULT_FAIL;

   if (fixedValue < rEdt->min) 
      fixedValue = rEdt->min;
   if (fixedValue > rEdt->max) 
      fixedValue = rEdt->max;

   if ( rEdt->val != fixedValue )
   {
      rEdt->val = fixedValue;
      obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
   }

   return UG_RESULT_OK;
}

float UG_RealEditGetValue( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return 0.0f;

   rEdt = (UG_REALEDIT*)(obj->data);
   return _UG_RealEditFixedToFloat(rEdt->val, rEdt->fieldPrecision);
}

UG_RESULT UG_RealEditSetRange( UG_WINDOW* wnd, UG_U8 id, float min, float max )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_S32 fixedMin;
   UG_S32 fixedMax;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   if ( _UG_RealEditFloatToFixed(min, rEdt->fieldPrecision, &fixedMin) != UG_RESULT_OK )
      return UG_RESULT_FAIL;
   if ( _UG_RealEditFloatToFixed(max, rEdt->fieldPrecision, &fixedMax) != UG_RESULT_OK )
      return UG_RESULT_FAIL;
   if ( fixedMin > fixedMax ) return UG_RESULT_FAIL;

   rEdt->min = fixedMin;
   rEdt->max = fixedMax;
   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditGetRange( UG_WINDOW* wnd, UG_U8 id, float* min, float* max )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   if ( min == NULL || max == NULL ) return UG_RESULT_FAIL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   *min = _UG_RealEditFixedToFloat(rEdt->min, rEdt->fieldPrecision);
   *max = _UG_RealEditFixedToFloat(rEdt->max, rEdt->fieldPrecision);
   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetFont( UG_WINDOW* wnd, UG_U8 id, UG_FONT* font )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   rEdt->font = font;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetHSpace( UG_WINDOW* wnd, UG_U8 id, UG_S8 hs )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   rEdt->h_space = hs;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetVSpace( UG_WINDOW* wnd, UG_U8 id, UG_S8 vs )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   rEdt->v_space = vs;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetAlignment( UG_WINDOW* wnd, UG_U8 id, UG_U8 align )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   rEdt->align = align;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_COLOR UG_RealEditGetForeColor( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_COLOR c = C_BLACK;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      c = rEdt->fc;
   }
   return c;
}

UG_COLOR UG_RealEditGetBackColor( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_COLOR c = C_BLACK;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      c = rEdt->bc;
   }
   return c;
}

char *UG_RealEditGetText(UG_WINDOW *wnd, UG_U8 id)
{
   UG_OBJECT* obj = NULL;
   UG_REALEDIT* rEdt = NULL;
   char *str = NULL;

   obj = _UG_SearchObject(wnd, OBJ_TYPE_REAL_EDIT, id);
   if (obj != NULL)
   {
      rEdt = (UG_REALEDIT *)(obj->data);
      str = (char *)malloc(strlen(rEdt->str) + 1);
      strcpy(str, rEdt->str);
   }
   return str;
}

UG_FONT* UG_RealEditGetFont( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_FONT* font = NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      font = (UG_FONT*)rEdt->font;
   }
   return font;
}

UG_S8 UG_RealEditGetHSpace( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_S8 hs = 0;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      hs = rEdt->h_space;
   }
   return hs;
}

UG_S8 UG_RealEditGetVSpace( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_S8 vs = 0;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      vs = rEdt->v_space;
   }
   return vs;
}

UG_U8 UG_RealEditGetAlignment( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;
   UG_U8 align = 0;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj != NULL )
   {
      rEdt = (UG_REALEDIT*)(obj->data);
      align = rEdt->align;
   }
   return align;
}

UG_RESULT UG_RealEditShowCursor( UG_WINDOW* wnd, UG_U8 id )
{
	 UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);

   rEdt->cursorVisible = 0x01;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}
UG_RESULT UG_RealEditHideCursor( UG_WINDOW* wnd, UG_U8 id )
{
	 UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	 if (rEdt->cursorVisible == 0x01)
	 {
		 rEdt->cursorVisible = 0x00;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }

   return UG_RESULT_OK;
}
UG_RESULT UG_RealEditSetCursorForeColor( UG_WINDOW* wnd, UG_U8 id, UG_COLOR cfc )
{
	   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	 if (rEdt->cfc != cfc)
	 {
		 rEdt->cfc = cfc;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }

   return UG_RESULT_OK;
}
UG_RESULT UG_RealEditSetCursorBackColor( UG_WINDOW* wnd, UG_U8 id, UG_COLOR cbc )
{
	 UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	 if (rEdt->cbc != cbc)
	 {
		 rEdt->cbc = cbc;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetCharWidth( UG_WINDOW* wnd, UG_U8 id, UG_U8 charWidth )
{
   UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
   if ( rEdt->charWidth != charWidth )
   {
      rEdt->charWidth = charWidth;
      obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
   }

   return UG_RESULT_OK;
}

UG_RESULT UG_RealEditSetCursorPosition( UG_WINDOW* wnd, UG_U8 id, UG_U16 pos )
{
	 UG_OBJECT* obj=NULL;
   UG_REALEDIT* rEdt=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_REAL_EDIT, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   rEdt = (UG_REALEDIT*)(obj->data);
	 if (rEdt->cursorPos != pos)
	 {
		 rEdt->cursorPos = pos;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }

   return UG_RESULT_OK;
}
static void _UG_RealEditUpdate(UG_WINDOW* wnd, UG_OBJECT* obj)
{
   UG_REALEDIT* rEdt;
   UG_AREA a;
   UG_TEXT txt = {0};
   UG_U16 xs, w;


   /* Get object-specific data */
   rEdt = (UG_REALEDIT*)(obj->data);

   /* -------------------------------------------------- */
   /* Object update section                              */
   /* -------------------------------------------------- */
   if ( obj->state & OBJ_STATE_UPDATE )
   {
      if ( obj->state & OBJ_STATE_VISIBLE )
      {
         /* Full redraw necessary? */
         if ( obj->state & OBJ_STATE_REDRAW )
         {
            UG_WindowGetArea(wnd,&a);
            obj->a_abs.xs = obj->a_rel.xs + a.xs;
            obj->a_abs.ys = obj->a_rel.ys + a.ys;
            obj->a_abs.xe = obj->a_rel.xe + a.xs;
            obj->a_abs.ye = obj->a_rel.ye + a.ys;
            if ( obj->a_abs.ye > wnd->ye ) return;
            if ( obj->a_abs.xe > wnd->xe ) return;
#ifdef UGUI_USE_PRERENDER_EVENT
            _UG_SendObjectPrerenderEvent(wnd, obj);
#endif

            


            UG_FillFrame(obj->a_abs.xs, obj->a_abs.ys, obj->a_abs.xe, obj->a_abs.ye, rEdt->bc);

            /* Draw Textbox text */
            w = rEdt->fieldWidth * (rEdt->charWidth + rEdt->h_space) - rEdt->h_space;
            xs = obj->a_abs.xs + (obj->a_abs.xe - obj->a_abs.xs - w) / 2;

            char charToStr[2];
            for (int charNo = 0; charNo < rEdt->fieldWidth; charNo++)
            {
               if (charNo == rEdt->fieldWidth - rEdt->fieldPrecision - 1) // Decimal point
               {
                  charToStr[0] = '.';
               }
               else
               {
                  int exponent = rEdt->fieldWidth - charNo - 1;
                  if (charNo < rEdt->fieldWidth - rEdt->fieldPrecision - 1) exponent--;
                  charToStr[0] = rEdt->val / _UG_RealEditIntPow(10, exponent) % 10 + '0';
               }
               charToStr[1] = '\0';

               txt.a.xs = xs + charNo * (rEdt->charWidth + rEdt->h_space);
               txt.a.ys = obj->a_abs.ys;
               txt.a.xe = txt.a.xs + rEdt->charWidth - 1;
               txt.a.ye = obj->a_abs.ye;
               txt.align = ALIGN_CENTER;
               txt.font = rEdt->font;
               txt.h_space = rEdt->h_space;
               txt.v_space = rEdt->v_space;
               txt.str = charToStr;
               if (rEdt->cursorVisible == 0x01 && charNo == rEdt->cursorPos)
               {
                  txt.fc = rEdt->cfc;
                  txt.bc = rEdt->cbc;
                  UG_FillFrame(txt.a.xs, txt.a.ys, txt.a.xe, txt.a.ye, txt.bc);
               }
               else
               {
                  txt.fc = rEdt->fc;
                  txt.bc = rEdt->bc;
               }
               _UG_PutText(&txt);
            }
            obj->state &= ~OBJ_STATE_REDRAW;
#ifdef UGUI_USE_POSTRENDER_EVENT
            _UG_SendObjectPostrenderEvent(wnd, obj);
#endif
         }
      }
      else
      {
         UG_FillFrame(obj->a_abs.xs, obj->a_abs.ys, obj->a_abs.xe, obj->a_abs.ye, wnd->bc);
      }
      obj->state &= ~OBJ_STATE_UPDATE;
   }
}
