/* $XFree86$ */
/**************************************************************************

Copyright 2002 Tungsten Graphics Inc., Cedar Park, Texas.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

#ifndef __TNL_VTX_API_H__
#define __TNL_VTX_API_H__

#include "t_context.h"

extern void _tnl_UpdateVtxfmt( GLcontext *ctx );
extern void _tnl_InitVtxfmt( GLcontext *ctx );
extern void _tnl_InvalidateVtxfmt( GLcontext *ctx );
extern void _tnl_DestroyVtxfmt( GLcontext *ctx );

typedef void (*p4f)( GLfloat, GLfloat, GLfloat, GLfloat );
typedef void (*p3f)( GLfloat, GLfloat, GLfloat );
typedef void (*p2f)( GLfloat, GLfloat );
typedef void (*p1f)( GLfloat );
typedef void (*pe2f)( GLenum, GLfloat, GLfloat );
typedef void (*pe1f)( GLenum, GLfloat );
typedef void (*pfv)( const GLfloat * );
typedef void (*pefv)( GLenum, const GLfloat * );



/* t_vtx_dlist.c
 */
extern void _tnl_BeginCallList( GLcontext *ctx, GLuint list );
extern void _tnl_EndCallList( GLcontext *ctx );
extern void _tnl_EndList( GLcontext *ctx );
extern void _tnl_NewList( GLcontext *ctx, GLuint list, GLenum mode );
extern void _tnl_FlushVertices( GLcontext *ctx, GLuint flags );


#endif
