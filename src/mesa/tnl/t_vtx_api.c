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
 */
#include "mtypes.h"
#include "colormac.h"
#include "simple_list.h"
#include "vtxfmt.h"

#include "tnl_vtx_api.h"

/* Fallback versions of all the entrypoints for situations where
 * codegen isn't available.  This is slowed significantly by all the
 * gumph necessary to get to the tnl pointer.
 */
#define ATTRF( ATTR, N, A, B, C, D )				\
{								\
   GET_CURRENT_CONTEXT( ctx );					\
   TNLcontext *tnl = TNL_CONTEXT(ctx);				\
								\
   if ((ATTR) == 0) {						\
      int i;							\
								\
      if (N>0) tnl->vbptr[0].f = A;				\
      if (N>1) tnl->vbptr[1].f = B;				\
      if (N>2) tnl->vbptr[2].f = C;				\
      if (N>3) tnl->vbptr[3].f = D;				\
								\
      for (i = N; i < tnl->vertex_size; i++)			\
	 *tnl->vbptr[i].i = tnl->vertex[i].i;			\
								\
      tnl->vbptr += tnl->vertex_size;				\
								\
      if (--tnl->counter == 0)					\
	 tnl->notify();						\
   }								\
   else {					\
      GLfloat *dest = tnl->attrptr[ATTR];			\
      if (N>0) dest[0] = A;					\
      if (N>1) dest[1] = B;					\
      if (N>2) dest[2] = C;					\
      if (N>3) dest[3] = D;					\
   }								\
}

#define ATTR4F( ATTR, A, B, C, D )  ATTRF( ATTR, 4, A, B, C, D )
#define ATTR3F( ATTR, A, B, C, D )  ATTRF( ATTR, 3, A, B, C, 1 )
#define ATTR2F( ATTR, A, B, C, D )  ATTRF( ATTR, 2, A, B, 0, 1 )
#define ATTR1F( ATTR, A, B, C, D )  ATTRF( ATTR, 1, A, 0, 0, 1 )

#define ATTRS( ATTRIB )						\
static void attrib_##ATTRIB##_1_0( GLfloat s )			\
{								\
   ATTR1F( ATTRIB, s );						\
}								\
								\
static void attrib_##ATTRIB##_1_1( const GLfloat *v )		\
{								\
   ATTR1F( ATTRIB, v[0] );					\
}								\
								\
static void attrib_##ATTRIB##_2_0( GLfloat s, GLfloat t )	\
{								\
   ATTR2F( ATTRIB, s, t );					\
}								\
								\
static void attrib_##ATTRIB##_2_1( const GLfloat *v )		\
{								\
   ATTR2F( ATTRIB, v[0], v[1] );				\
}								\
								\
static void attrib_##ATTRIB##_3_0( GLfloat s, GLfloat t, 	\
				   GLfloat r )			\
{								\
   ATTR3F( ATTRIB, s, t, r );					\
}								\
								\
static void attrib_##ATTRIB##_3_1( const GLfloat *v )		\
{								\
   ATTR3F( ATTRIB, v[0], v[1], v[2] );				\
}								\
								\
static void attrib_##ATTRIB##_4_0( GLfloat s, GLfloat t,	\
				   GLfloat r, GLfloat q )	\
{								\
   ATTR4F( ATTRIB, s, t, r, q );				\
}								\
								\
static void attrib_##ATTRIB##_4_1( const GLfloat *v )		\
{								\
   ATTR4F( ATTRIB, v[0], v[1], v[2], v[3] );			\
}

/* Generate a lot of functions:
 */
ATTRS( 0 )
ATTRS( 1 )
ATTRS( 2 )
ATTRS( 3 )
ATTRS( 4 )
ATTRS( 5 )
ATTRS( 6 )
ATTRS( 7 )
ATTRS( 8 )
ATTRS( 9 )
ATTRS( 10 )
ATTRS( 11 )
ATTRS( 12 )
ATTRS( 13 )
ATTRS( 14 )
ATTRS( 15 )


static void *lookup_or_generate( GLuint attr, GLuint sz, GLuint v,
				 void *fallback_attr_func )
{ 
   GET_CURRENT_CONTEXT( ctx ); 
   TNLcontext *tnl = TNL_CONTEXT(ctx); 
   void *ptr = 0;

   if (tnl->vertex_active[attr] != sz)
      tnl_fixup_vertex( ctx, attr, sz );

   if (ptr == 0)
      ptr = tnl->generated[attr][sz-1][v];
   
   if (ptr == 0 && attr == 0)
      ptr = tnl->codegen.vertex[sz-1][v]( ctx );

   if (ptr == 0 && attr != 0)
      ptr = tnl->codegen.attr[sz-1][v]( ctx, attr );

   if (ptr == 0)
      ptr = fallback_attr_func;

   ctx->Driver.NeedFlush |= FLUSH_UPDATE_CURRENT;

   tnl->tabf[v][sz-1][attr] = ptr;

   if (dispatch_entry[attr][sz-1][v]) 
      ((void **)ctx->Exec)[dispatch_entry[attr][sz-1][v]] = ptr;

   return ptr;
}


/* These functions choose one of the ATTR's generated above (or from
 * codegen).  Like the ATTR functions, they live in the GL dispatch
 * table and in the second-level dispatch table for MultiTexCoord,
 * AttribNV, etc.
 */
#define CHOOSE( FNTYPE, ATTR, SZ, V, ARGS1, ARGS2 )	\
static void choose_##ATTR##_##SZ##_##V ARGS1		\
{							\
   void *ptr = choose(ctx, ATTR, SZ, V, 		\
		      attrib_##ATTR##_##SZ##_##V );	\
   							\
   (FN_TYPE) ptr ARGS2;					\
}

#define CHOOSERS( ATTR )						\
CHOOSE( ATTR, 1, 1, pfv, (const GLfloat *v), (v))			\
CHOOSE( ATTR, 2, 1, pfv, (const GLfloat *v), (v))			\
CHOOSE( ATTR, 3, 1, pfv, (const GLfloat *v), (v))			\
CHOOSE( ATTR, 4, 1, pfv, (const GLfloat *v), (v))			\
CHOOSE( ATTR, 1, 0, p1f, (GLfloat a), (a))				\
CHOOSE( ATTR, 2, 0, p2f, (GLfloat a, GLfloat b), (a,b))		\
CHOOSE( ATTR, 3, 0, p3f, (GLfloat a, GLfloat c), (a,b,c))		\
CHOOSE( ATTR, 4, 0, p4f, (GLfloat a, GLfloat c,GLfloat d), (a,b,c,d))


CHOOSERS( 0 )
CHOOSERS( 1 )
CHOOSERS( 2 )
CHOOSERS( 3 )
CHOOSERS( 4 )
CHOOSERS( 5 )
CHOOSERS( 6 )
CHOOSERS( 7 )
CHOOSERS( 8 )
CHOOSERS( 9 )
CHOOSERS( 10 )
CHOOSERS( 11 )
CHOOSERS( 12 )
CHOOSERS( 13 )
CHOOSERS( 14 )
CHOOSERS( 15 )






/* Second level dispatch table for MultiTexCoord, Material and 
 * VertexAttribNV.
 *
 * Need this because we want to track things like vertex attribute
 * sizes, presence/otherwise of attribs in recorded vertices, etc, by
 * manipulating the state of dispatch tables.  Need therefore a
 * dispatch slot for each value of 'index' or 'unit' in VertexAttribNV
 * and MultiTexCoordARB.  Also need a mechnism for keeping this data
 * consistent with what's coming in via the Vertex/Normal/etc api
 * above (where aliasing exists with the traditional entrypoints).
 * Note that MultiTexCoordARB aliases with TexCoord when unit==0.
 *
 * Need presence tracking for material components, too, but not size
 * tracking or help with aliasing.  Could move material to seperate
 * dispatch without the "*4" below, or even do the checks every time.
 */
struct attr_dispatch_tab {
   void (*tabfv[4][32])( const GLfloat * );

   int swapcount;
   int installed;		/* bitmap */
   int installed_sizes[32];	/* active sizes */
};

#define DISPATCH_ATTRFV( ATTR, COUNT, P ) tnl->vb.tabfv[COUNT-1][ATTR]( P )
#define DISPATCH_ATTR1FV( ATTR, V ) tnl->vb.tabfv[0][attr]( V )
#define DISPATCH_ATTR2FV( ATTR, V ) tnl->vb.tabfv[1][attr]( V )
#define DISPATCH_ATTR3FV( ATTR, V ) tnl->vb.tabfv[2][attr]( V )
#define DISPATCH_ATTR4FV( ATTR, V ) tnl->vb.tabfv[3][attr]( V )

#define DISPATCH_ATTR1F( ATTR, S ) tnl->vb.tabfv[0][attr]( &S )
#ifdef USE_X86_ASM
/* Naughty cheat:
 */
#define DISPATCH_ATTR2F( ATTR, S,T ) tnl->vb.tabfv[1][attr]( &S )
#define DISPATCH_ATTR3F( ATTR, S,T,R ) tnl->vb.tabfv[2][attr]( &S )
#define DISPATCH_ATTR4F( ATTR, S,T,R,Q ) tnl->vb.tabfv[3][attr]( &S )
#else
/* Safe:
 */
#define DISPATCH_ATTR2F( ATTR, S,T ) 		\
do { 						\
   GLfloat v[2]; 				\
   v[0] = S; v[1] = T;				\
   tnl->vb.tabfv[1][attr]( v );			\
} while (0)
#define DISPATCH_ATTR3F( ATTR, S,T,R ) 		\
do { 						\
   GLfloat v[3]; 				\
   v[0] = S; v[1] = T; v[2] = R;		\
   tnl->vb.tabfv[2][attr]( v );			\
} while (0)
#define DISPATCH_ATTR4F( ATTR, S,T,R,Q )	\
do { 						\
   GLfloat v[4]; 				\
   v[0] = S; v[1] = T; v[2] = R; v[3] = Q;	\
   tnl->vb.tabfv[3][attr]( v );			\
} while (0)
#endif



static void enum_error( void )
{
   GET_CURRENT_CONTEXT( ctx );
   _mesa_error( ctx, GL_INVALID_ENUM, __FUNCTION__ );
}

static void op_error( void )
{
   GET_CURRENT_CONTEXT( ctx );
   _mesa_error( ctx, GL_INVALID_OPERATION, __FUNCTION__ );
}


/* First level for MultiTexcoord:  Send through second level dispatch.
 * These are permanently installed in the toplevel dispatch.
 *
 * Assembly can optimize the generation of arrays by using &s instead
 * of building 'v'.
 */
static void tnl_MultiTexCoord1f( GLenum target, GLfloat s  )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR1FV( attr, &s );
}

static void tnl_MultiTexCoord1fv( GLenum target, const GLfloat *v )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR1FV( attr, v );
}

static void tnl_MultiTexCoord2f( GLenum target, GLfloat s, GLfloat t )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR2F( attr, s, t );
}

static void tnl_MultiTexCoord2fv( GLenum target, const GLfloat *v )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR2FV( attr, v );
}

static void tnl_MultiTexCoord3f( GLenum target, GLfloat s, GLfloat t,
				    GLfloat r)
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR3F( attr, s, t, r );
}

static void tnl_MultiTexCoord3fv( GLenum target, const GLfloat *v )
{
   GLuint attr = (target & 0x7);
   DISPATCH_ATTR3FV( attr, v );
}

static void tnl_MultiTexCoord4f( GLenum target, GLfloat s, GLfloat t,
				    GLfloat r, GLfloat q )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR4F( attr, s, t, r, q );
}

static void tnl_MultiTexCoord4fv( GLenum target, const GLfloat *v )
{
   GLuint attr = (target & 0x7) + VERT_ATTRIB_TEX0;
   DISPATCH_ATTR4FV( attr, v );
}


/* First level for NV_vertex_program:
 *
 * Check for errors & reroute through second dispatch layer to get
 * size tracking per-attribute.
 */
static void tnl_VertexAttrib1fNV( GLuint index, GLfloat s )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR1F( index, s );
   else
      enum_error(); 
}

static void tnl_VertexAttrib1fvNV( GLuint index, const GLfloat *v )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR1FV( index, v );
   else
      enum_error();
}

static void tnl_VertexAttrib2fNV( GLuint index, GLfloat s, GLfloat t )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR2F( index, s, t );
   else
      enum_error();
}

static void tnl_VertexAttrib2fvNV( GLuint index, const GLfloat *v )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR2FV( index, v );
   else
      enum_error();
}

static void tnl_VertexAttrib3fNV( GLuint index, GLfloat s, GLfloat t, 
				  GLfloat r )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR3F( index, s, t, r );
   else
      enum_error();
}

static void tnl_VertexAttrib3fvNV( GLuint index, const GLfloat *v )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR3FV( index, v );
   else
      enum_error();
}

static void tnl_VertexAttrib4fNV( GLuint index, GLfloat s, GLfloat t,
				  GLfloat r, GLfloat q )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR4F( index, s, t, r, q );
   else
      enum_error();
}

static void tnl_VertexAttrib4fvNV( GLuint index, const GLfloat *v )
{
   if (index < MAX_VERT_ATTRS)
      DISPATCH_ATTR4FV( index, v );
   else
      enum_error();
}


/* Materials:  
 * 
 * These are treated as per-vertex attributes, at indices above where
 * the NV_vertex_program leaves off.  There are a lot of good things
 * about treating materials this way.  
 *
 * However: I don't want to double the number of generated functions
 * just to cope with this, so I unroll the 'C' varients of CHOOSE and
 * ATTRF into this function, and dispense with codegen and
 * second-level dispatch.
 */
#define MAT_ATTR( A, N, params )			\
do {							\
   if (tnl->vertex_active[A] != N) {			\
      tnl_fixup_vertex( ctx, A, N );			\
   }							\
							\
   {							\
      GLfloat *dest = tnl->attrptr[A];			\
      if (N>0) dest[0] = params[0];			\
      if (N>1) dest[1] = params[1];			\
      if (N>2) dest[2] = params[2];			\
      if (N>3) dest[3] = params[3];			\
      ctx->Driver.NeedFlush |= FLUSH_UPDATE_CURRENT;	\
   }							\
} while (0)


#define MAT( ATTR, N, face, params )		\
do {						\
   if (face != GL_BACK)				\
      MAT_ATTR( ATTR, N, params ); /* front */	\
   if (face != GL_FRONT)			\
      MAT_ATTR( ATTR+7, N, params ); /* back */	\
} while (0)


/* NOTE: Have to remove/dealwith colormaterial crossovers, probably
 * later on - in the meantime just store everything.  
 */
static void _tnl_Materialfv( GLenum face, GLenum pname, 
			       const GLfloat *params )
{
   GET_CURRENT_CONTEXT( ctx ); 
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   switch (pname) {
   case GL_EMISSION:
      MAT( VERT_ATTRIB_MAT_FRONT_EMMISSION, 4, face, params );
      break;
   case GL_AMBIENT:
      MAT( VERT_ATTRIB_MAT_FRONT_AMBIENT, 4, face, params );
      break;
   case GL_DIFFUSE:
      MAT( VERT_ATTRIB_MAT_FRONT_DIFFUSE, 4, face, params );
      break;
   case GL_SPECULAR:
      MAT( VERT_ATTRIB_MAT_FRONT_SPECULAR, 4, face, params );
      break;
   case GL_SHININESS:
      MAT( VERT_ATTRIB_MAT_FRONT_SHININESS, 1, face, params );
      break;
   case GL_COLOR_INDEXES:
      MAT( VERT_ATTRIB_MAT_FRONT_INDEXES, 3, face, params ); /* ??? */
      break;
   case GL_AMBIENT_AND_DIFFUSE:
      MAT( VERT_ATTRIB_MAT_FRONT_AMBIENT, 4, face, params );
      MAT( VERT_ATTRIB_MAT_FRONT_DIFFUSE, 4, face, params );
      break;
   default:
      _mesa_error( ctx, GL_INVALID_ENUM, where );
      return;
   }
}






/* These functions are the initial state for dispatch entries for all
 * entrypoints except those requiring double-dispatch (multitexcoord,
 * material, vertexattrib).
 *
 * These may provoke a vertex-upgrade where the existing vertex buffer
 * is flushed and a new element is added to the active vertex layout.
 * This can happen between begin/end pairs.
 */
static float id[4] = { 0, 0, 0, 1 };

static void _tnl_fixup_vertex( GLcontext *ctx, GLuint attr, GLuint sz )
{
   if (tnl->vertex_present[ATTR] < SZ) {
      tnl_upgrade_vertex( tnl, ATTR, SZ );
   }
   else {
      int i;

      /* Just clean the bits that won't be touched otherwise:
       */
      for (i = SZ ; i < tnl->vertex_present[ATTR] ; i++)
	 tnl->attrptr[ATTR][i] = id[i];

      if (ctx->Driver.NeedFlush & FLUSH_UPDATE_CURRENT) {
	 _tnl_reset_attr_dispatch_tab( ctx );
	 _mesa_install_exec_vtxfmt( ctx, &tnl->chooser );
      }
   }
}



/* EvalCoord needs special treatment as ususal:
 */
static void evalcoord( GLfloat a, GLfloat b, GLuint type ) 
{
   GET_CURRENT_CONTEXT( ctx );					
   TNLcontext *tnl = TNL_CONTEXT(ctx);				
   
   /* Initialize the list of eval fixups:
    */
   if (!tnl->evalptr) {
      init_eval_ptr( ctx );
   }

   /* Note that this vertex will need to be fixed up:
    */
   tnl->evalptr[0].vert = tnl->initial_counter - tnl->counter;
   tnl->evalptr[0].type = type;

   /* Now emit the vertex with eval data in obj coordinates:
    */
   ATTRF( 0, 2, a, b, 0, 1 );
}


static void _tnl_EvalCoord1f( GLfloat u )
{
   evalcoord( u, 0, _TNL_EVAL_COORD1 );
}

static void _tnl_EvalCoord1fv( const GLfloat *v )
{
   evalcoord( v[0], 0, _TNL_EVAL_COORD1 );
}

static void _tnl_EvalCoord2f( GLfloat u, GLfloat v )
{
   evalcoord( u, v, _TNL_EVAL_COORD2 );
}

static void _tnl_EvalCoord2fv( const GLfloat *u )
{
   evalcoord( v[0], v[1], _TNL_EVAL_COORD2 );
}

static void _tnl_EvalPoint1( GLint i )
{
   evalcoord( (GLfloat)i, 0, _TNL_EVAL_POINT1 );
}

static void _tnl_EvalPoint2( GLint i, GLint j )
{
   evalcoord( (GLfloat)i, (GLfloat)j, _TNL_EVAL_POINT2 );
}








void _tnl_InitVtxfmtChoosers( GLvertexformat *vfmt )
{


   vfmt->Color3f = choose_Color3f;
   vfmt->Color3fv = choose_Color3fv;
   vfmt->Color4f = choose_Color4f;
   vfmt->Color4fv = choose_Color4fv;
   vfmt->SecondaryColor3fEXT = choose_SecondaryColor3fEXT;
   vfmt->SecondaryColor3fvEXT = choose_SecondaryColor3fvEXT;
   vfmt->MultiTexCoord1fARB = dd_MultiTexCoord1f;
   vfmt->MultiTexCoord1fvARB = dd_MultiTexCoord1fv;
   vfmt->MultiTexCoord2fARB = dd_MultiTexCoord2f;
   vfmt->MultiTexCoord2fvARB = dd_MultiTexCoord2fv;
   vfmt->MultiTexCoord3fARB = dd_MultiTexCoord3f;
   vfmt->MultiTexCoord3fvARB = dd_MultiTexCoord3fv;
   vfmt->MultiTexCoord4fARB = dd_MultiTexCoord4f;
   vfmt->MultiTexCoord4fvARB = dd_MultiTexCoord4fv;
   vfmt->Normal3f = choose_Normal3f;
   vfmt->Normal3fv = choose_Normal3fv;
   vfmt->TexCoord1f = choose_TexCoord1f;
   vfmt->TexCoord1fv = choose_TexCoord1fv;
   vfmt->TexCoord2f = choose_TexCoord2f;
   vfmt->TexCoord2fv = choose_TexCoord2fv;
   vfmt->TexCoord3f = choose_TexCoord3f;
   vfmt->TexCoord3fv = choose_TexCoord3fv;
   vfmt->TexCoord4f = choose_TexCoord4f;
   vfmt->TexCoord4fv = choose_TexCoord4fv;
   vfmt->Vertex2f = choose_Vertex2f;
   vfmt->Vertex2fv = choose_Vertex2fv;
   vfmt->Vertex3f = choose_Vertex3f;
   vfmt->Vertex3fv = choose_Vertex3fv;
   vfmt->Vertex4f = choose_Vertex4f;
   vfmt->Vertex4fv = choose_Vertex4fv;
   vfmt->FogCoordfvEXT = choose_FogCoordfvEXT;
   vfmt->FogCoordfEXT = choose_FogCoordfEXT;
   vfmt->EdgeFlag = choose_EdgeFlag;
   vfmt->EdgeFlagv = choose_EdgeFlagv;
   vfmt->Indexi = choose_Indexi;
   vfmt->Indexiv = choose_Indexiv;
   vfmt->EvalCoord1f = choose_EvalCoord1f;
   vfmt->EvalCoord1fv = choose_EvalCoord1fv;
   vfmt->EvalCoord2f = choose_EvalCoord2f;
   vfmt->EvalCoord2fv = choose_EvalCoord2fv;
   vfmt->Materialfv = dd_Materialfv;
}


static struct dynfn *codegen_noop( struct _vb *vb, int key )
{
   (void) vb; (void) key;
   return 0;
}

void _tnl_InitCodegen( struct dfn_generators *gen )
{
   gen->attr[0][1] = codegen_noop;
   gen->attr[0][0] = codegen_noop;
   gen->attr[1][1] = codegen_noop;
   gen->attr[1][0] = codegen_noop;
   gen->attr[2][1] = codegen_noop;
   gen->attr[2][0] = codegen_noop;
   gen->attr[3][1] = codegen_noop;
   gen->attr[3][0] = codegen_noop;

   gen->vertex[1][1] = codegen_noop;
   gen->vertex[1][0] = codegen_noop;
   gen->vertex[2][1] = codegen_noop;
   gen->vertex[2][0] = codegen_noop;
   gen->vertex[3][1] = codegen_noop;
   gen->vertex[3][0] = codegen_noop;
   

   if (!getenv("MESA_NO_CODEGEN")) {
#if defined(USE_X86_ASM)
      _tnl_InitX86Codegen( gen );
#endif

#if defined(USE_SSE_ASM)
      _tnl_InitSSECodegen( gen );
#endif

#if defined(USE_3DNOW_ASM)
#endif

#if defined(USE_SPARC_ASM)
#endif
   }
}
