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

#ifndef __RADEON_VTXFMT_H__
#define __RADEON_VTXFMT_H__

#ifdef GLX_DIRECT_RENDERING

#include "_tnl__context.h"

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

/* Want to keep a cache of these around.  Each is parameterized by
 * only a single value which has only a small range.  Only expect a
 * few, so just rescan the list each time?
 */
struct dynfn {
   struct dynfn *next, *prev;
   int key;
   char *code;
};

struct dfn_lists {
   struct dynfn Attr1f;
   struct dynfn Attr1fv;
   struct dynfn Attr2f;
   struct dynfn Attr2fv;
   struct dynfn Attr3f;
   struct dynfn Attr3fv;
   struct dynfn Attr4f;
   struct dynfn Attr4fv;
   struct dynfn Vertex1f;
   struct dynfn Vertex1fv;
   struct dynfn Vertex2f;
   struct dynfn Vertex2fv;
   struct dynfn Vertex3f;
   struct dynfn Vertex3fv;
   struct dynfn Vertex4f;
   struct dynfn Vertex4fv;
};

struct _vb;

struct dfn_generators {
   struct dynfn *(*Attr1f)( struct _vb *, int );
   struct dynfn *(*Attr1fv)( struct _vb *, int );
   struct dynfn *(*Attr2f)( struct _vb *, int );
   struct dynfn *(*Attr2fv)( struct _vb *, int );
   struct dynfn *(*Attr3f)( struct _vb *, int );
   struct dynfn *(*Attr3fv)( struct _vb *, int );
   struct dynfn *(*Attr4f)( struct _vb *, int );
   struct dynfn *(*Attr4fv)( struct _vb *, int );
   struct dynfn *(*Vertex1f)( struct _vb *, int );
   struct dynfn *(*Vertex1fv)( struct _vb *, int );
   struct dynfn *(*Vertex2f)( struct _vb *, int );
   struct dynfn *(*Vertex2fv)( struct _vb *, int );
   struct dynfn *(*Vertex3f)( struct _vb *, int );
   struct dynfn *(*Vertex3fv)( struct _vb *, int );
   struct dynfn *(*Vertex4f)( struct _vb *, int );
   struct dynfn *(*Vertex4fv)( struct _vb *, int );
};

struct prim {
   GLuint start;
   GLuint end;
   GLuint prim;
};

#define _TNL__MAX_PRIMS 64



struct tnl_vbinfo {
   /* Keep these first: referenced from codegen templates:
    */
   GLint counter;
   GLint *dmaptr;
   void (*notify)( void );
   union { float f; int i; GLubyte ub4[4]; } vertex[16*4];

   GLfloat *attrptr[16];
   GLuint size[16];

   GLenum *prim;		/* &ctx->Driver.CurrentExecPrimitive */
   GLuint primflags;

   GLboolean installed;
   GLboolean recheck;

   GLint vertex_size;
   GLint initial_counter;
   GLint nrverts;
   GLuint vertex_format;

   GLuint installed_vertex_format;

   struct prim primlist[RADEON_MAX_PRIMS];
   int nrprims;

   struct dfn_lists dfn_cache;
   struct dfn_generators codegen;
   GLvertexformat vtxfmt;
};


extern void _tnl_InitVtxfmtChoosers( GLvertexformat *vfmt );


#define FIXUP( CODE, OFFSET, CHECKVAL, NEWVAL )	\
do {						\
   int *icode = (int *)(CODE+OFFSET);		\
   assert (*icode == CHECKVAL);			\
   *icode = (int)NEWVAL;			\
} while (0)


/* Useful for figuring out the offsets:
 */
#define FIXUP2( CODE, OFFSET, CHECKVAL, NEWVAL )		\
do {								\
   while (*(int *)(CODE+OFFSET) != CHECKVAL) OFFSET++;		\
   fprintf(stderr, "%s/%d CVAL %x OFFSET %d\n", __FUNCTION__,	\
	   __LINE__, CHECKVAL, OFFSET);				\
   *(int *)(CODE+OFFSET) = (int)NEWVAL;				\
   OFFSET += 4;							\
} while (0)

/* 
 */
void _tnl_InitCodegen( struct dfn_generators *gen );
void _tnl_InitX86Codegen( struct dfn_generators *gen );
void _tnl_InitSSECodegen( struct dfn_generators *gen );

void _tnl_copy_to_current( GLcontext *ctx );


/* Defined in tnl_vtxfmt_c.c.
 */
struct dynfn *tnl_makeX86Vertex2f( TNLcontext *, int );
struct dynfn *tnl_makeX86Vertex2fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Vertex3f( TNLcontext *, int );
struct dynfn *tnl_makeX86Vertex3fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Vertex4f( TNLcontext *, int );
struct dynfn *tnl_makeX86Vertex4fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr4f( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr4fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr3f( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr3fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr2f( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr2fv( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr1f( TNLcontext *, int );
struct dynfn *tnl_makeX86Attr1fv( TNLcontext *, int );




#endif
#endif
