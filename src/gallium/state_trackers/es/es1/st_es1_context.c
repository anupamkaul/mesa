/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 **************************************************************************/


/**
 * Extra per-context init for OpenGL ES 1.x only.
 */


#include "main/context.h"


/**
 * Called from _mesa_initialize_context()
 */
void
_mesa_initialize_context_extra(GLcontext *ctx)
{
   /* nothing for ES 1.x */
}
