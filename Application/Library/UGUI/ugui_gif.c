#include "ugui_gif.h"

/* -------------------------------------------------------------------------------- */
/* -- IMAGE FUNCTIONS                                                            -- */
/* -------------------------------------------------------------------------------- */

/* Static functions */
static void _UG_GifUpdate(UG_WINDOW* wnd, UG_OBJECT* obj);

UG_RESULT UG_GifCreate( UG_WINDOW* wnd, UG_GIF* gif, UG_U8 id, UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye )
{
   UG_OBJECT* obj;

   obj = _UG_GetFreeObject( wnd );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   /* Initialize object-specific parameters */
   gif->frames = NULL;
   gif->type = IMG_TYPE_BMP;
	 gif->numFrames = 0;
	 gif->currentFrame = 0;
	 gif->delayMs = 0;
	 gif->loadBMPState = GIF_STATE_STOP;

   /* Initialize standard object parameters */
   obj->update = _UG_GifUpdate;
   #ifdef UGUI_USE_TOUCH
   obj->touch_state = OBJ_TOUCH_STATE_INIT;
   #endif
   obj->type = OBJ_TYPE_GIF;
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
   obj->data = (void*)gif;

   /* Update function: Do your thing! */
   obj->state &= ~OBJ_STATE_FREE;

   return UG_RESULT_OK;
}

UG_RESULT UG_GifDelete( UG_WINDOW* wnd, UG_U8 id )
{
   return _UG_DeleteObject( wnd, OBJ_TYPE_GIF, id );
}

UG_RESULT UG_GifShow( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   obj->state |= OBJ_STATE_VISIBLE;
   obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;

   return UG_RESULT_OK;
}

UG_RESULT UG_GifHide( UG_WINDOW* wnd, UG_U8 id )
{
   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   obj->state &= ~OBJ_STATE_VISIBLE;
   obj->state |= OBJ_STATE_UPDATE;

   return UG_RESULT_OK;
}
UG_U8 UG_GifCheckVisible( UG_WINDOW* wnd, UG_U8 id )
{
	   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   return obj->state & OBJ_STATE_VISIBLE;;
}
UG_RESULT UG_GifSetFrames( UG_WINDOW* wnd, UG_U8 id, const UG_BMP* frames[], uint16_t numFrames )
{
   UG_OBJECT* obj=NULL;
   UG_GIF* gif=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   gif = (UG_GIF*)(obj->data);

	 gif->frames = frames;
	 gif->numFrames = numFrames;
	 gif->type = IMG_TYPE_BMP;
	 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
   return UG_RESULT_OK;
}
UG_RESULT UG_GifStart(UG_WINDOW* wnd, UG_U8 id, int32_t startFrame)
{
	 UG_OBJECT* obj=NULL;
   UG_GIF* gif=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   gif = (UG_GIF*)(obj->data);
	 
	 if (gif->loadBMPState != GIF_STATE_RUN)
	 {
		 gif->loadBMPState = GIF_STATE_RUN;
		 if (startFrame > 0 && startFrame < gif->numFrames)
			 gif->currentFrame = startFrame;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }
	 
   return UG_RESULT_OK;
}
UG_RESULT UG_GifStop(UG_WINDOW* wnd, UG_U8 id, int32_t stopFrame)
{
	 UG_OBJECT* obj=NULL;
   UG_GIF* gif=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   gif = (UG_GIF*)(obj->data);

	 gif->loadBMPState = GIF_STATE_STOP;
	 if (stopFrame != gif->currentFrame && stopFrame > 0 && stopFrame < gif->numFrames)
	 {
		 gif->currentFrame = stopFrame;
		 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	 }
   return UG_RESULT_OK;
}
UG_RESULT UG_GifSetDelayTime(UG_WINDOW* wnd, UG_U8 id, uint16_t delayMs)
{
	 UG_OBJECT* obj=NULL;
   UG_GIF* gif=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   gif = (UG_GIF*)(obj->data);

	 gif->delayMs = delayMs;
	 obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
   return UG_RESULT_OK;
}

UG_RESULT UG_GifLoadBMP(UG_WINDOW* wnd, UG_U8 id, uint32_t currentTime)
{
	 UG_OBJECT* obj=NULL;
   UG_GIF* gif=NULL;

   obj = _UG_SearchObject( wnd, OBJ_TYPE_GIF, id );
   if ( obj == NULL ) return UG_RESULT_FAIL;

   gif = (UG_GIF*)(obj->data);
	 
   if (currentTime - gif->lastTime >= gif->delayMs && gif->loadBMPState == GIF_STATE_RUN) 
	 {
			gif->lastTime = currentTime;
			if (gif->currentFrame < gif->numFrames - 1) 
			{
				gif->currentFrame++;
					
			}
			else
			{
				gif->currentFrame = 0;
			}
			obj->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
   }
   return UG_RESULT_OK;
}
static void _UG_GifUpdate(UG_WINDOW* wnd, UG_OBJECT* obj)
{
   UG_GIF* gif;
   UG_AREA a;

   /* Get object-specific data */
   gif = (UG_GIF*)(obj->data);

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
            /* ToDo: more/better image features */
            obj->a_abs.xs = obj->a_rel.xs + a.xs;
            obj->a_abs.ys = obj->a_rel.ys + a.ys;
            obj->a_abs.xe = obj->a_rel.xs + ((UG_BMP*)gif->frames[gif->currentFrame])->width + a.xs;
            obj->a_abs.ye = obj->a_rel.ys + ((UG_BMP*)gif->frames[gif->currentFrame])->height + a.ys;
            if ( obj->a_abs.ye > wnd->ye ) return;
            if ( obj->a_abs.xe > wnd->xe ) return;

            /* Draw Image */
            if ( (gif->frames[gif->currentFrame] != NULL) && (gif->type & IMG_TYPE_BMP) )
            {
               UG_DrawBMP(obj->a_abs.xs,obj->a_abs.ys,(UG_BMP*)gif->frames[gif->currentFrame]);
            }

            obj->state &= ~OBJ_STATE_REDRAW;
         }
      }
      else
      {
         UG_FillFrame(obj->a_abs.xs, obj->a_abs.ys, obj->a_abs.xe, obj->a_abs.ye, wnd->bc);
      }
      obj->state &= ~OBJ_STATE_UPDATE;
   }
}
