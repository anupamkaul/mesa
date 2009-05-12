/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 **************************************************************************/


/**
 * API/dispatch management functions.
 *
 * These replace Mesa's regular glapi.c file.
 *
 * Note that we don't use an API dispatch table at all for the OpenGL
 * ES state trackers.  glEnable(), for example, directly calls the
 * corresponding _mesa_Enable() function without going through a jump table.
 *
 * So the dispatch table stuff here is no-op'd.
 *
 * At this time, the current context pointer is _not_ thread safe.
 */


#include "glapi/glapi.h"


void *_glapi_Context = NULL;

struct _glapi_table *_glapi_Dispatch = NULL;


void
_glapi_noop_enable_warnings(GLboolean enable)
{
   /* no-op */
}

void
_glapi_set_warning_func(_glapi_warning_func func)
{
   /* no-op */
}


void
_glapi_check_multithread(void)
{
   /* no-op */
}


void
_glapi_set_context(void *context)
{
   _glapi_Context = context;
}


void *
_glapi_get_context(void)
{
   return _glapi_Context;
}

void
_glapi_set_dispatch(struct _glapi_table *dispatch)
{
   /* no-op */
}


struct _glapi_table *
_glapi_get_dispatch(void)
{
   return NULL;
}


GLuint
_glapi_get_dispatch_table_size(void)
{
   return 0;
}


int
_glapi_add_dispatch( const char * const * function_names,
		     const char * parameter_signature )
{
   /* we don't support run-time generation of new GL entrypoints */
   return -1;
}
