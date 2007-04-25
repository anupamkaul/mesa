struct prim_clip_stage {
   struct prim_pipeline_stage base;

   GLfloat plane[12][4];
};


struct prim_pipeline_stage *intel_prim_clip( struct prim_pipeline *pipe )
{
   struct clip_stage *clip = MALLOC(sizeof(*clip));
   GLuint i;

   if (clip) {
      prim_init_stage( pipe, 
		       &clip->base, 
		       MAX_CLIPPED_VERTICES );

      clip->base.point = clip_point;
      clip->base.line = clip_line;
      clip->base.tri = clip_tri;
   }
   
   return clip;
}



static void interp_attr( const struct vf_attr *a,
			 GLubyte *vdst,
			 GLfloat t,
			 const GLubyte *vin,
			 const GLubyte *vout )
{
   GLuint offset = a->vertoffset;
   GLfloat fin[4], fout[4], fdst[4];
   
   a->extract( a, fin, vin + offset );
   a->extract( a, fout, vout + offset );

   INTERP_F( t, fdst[3], fout[3], fin[3] );
   INTERP_F( t, fdst[2], fout[2], fin[2] );
   INTERP_F( t, fdst[1], fout[1], fin[1] );
   INTERP_F( t, fdst[0], fout[0], fin[0] );

   a->insert[4-1]( a, vdst + offset, fdst );
}




/* Interpolate between two vertices to produce a third.  Delves
 * slightly into the internals of the vf struct, but assumes too much
 * about the layout of the vertex to be included in the vf code
 * itself.
 */
static void interp( struct vertex_fetch *vf,
		    GLubyte *vdst,
		    GLfloat t,
		    const GLubyte *vin, 
		    const GLubyte *vout )
{
   const struct vertex_fetch_attr *a = vf->attr;
   const GLuint attr_count = vf->attr_count;
   GLuint j;

   /* Vertex header - leave undefined.
    */
   assert(a[0].attrib == VF_ATTRIB_VERTEX_HEADER);

   /* Clip coordinates:  interpolate normally
    */
   assert(a[1].format = EMIT_4F);
   interp_attr(&a[1], vin, vout, vdst);

   /* Projected/viewport coordinates:
    */
   {
      GLfloat out[4];

      /* We do the projective divide:
       */
      out[3] = 1.0 / clip[3];
      out[0] = clip[0] * out[3];
      out[1] = clip[1] * out[3];
      out[2] = clip[2] * out[3];

      /* vf module handles the viewport application:
       */
      a[2].insert[4-1]( &a[2], vdst + a[2].vertoffset, out );
   }
   
   /* Other attributes
    */
   for (j = 3; j < attr_count; j++) {
      interp_attr(&a[j], vin, vout, vdst);
   }
}




/* Clip a triangle against the viewport and user clip planes.
 */
static void
do_clip_tri( struct prim_pipeline_stage *stage, 
	     struct vertex_header *v0,
	     struct vertex_header *v1,
	     struct vertex_header *v2,
	     GLuint clipmask );
{
   struct clip_stage *clip = clip_stage( stage );
   struct vertex_header *a[MAX_CLIPPED_VERTICES];
   struct vertex_header *b[MAX_CLIPPED_VERTICES];
   struct vertex_header *inlist = a;
   struct vertex_header *outlist = b;
   GLuint n = 3;

   inlist[0] = v0;
   inlist[1] = v1;
   inlist[2] = v2;

   /* XXX: Note stupid hack to deal with tnl's 8-bit clipmask.  Remove
    * this once we correctly use 16bit masks for userclip planes.
    */
   if (clipmask & CLIP_USER_BIT) 
      clipmask |= clip->active_user_planes;

   while (clipmask && n >= 3) {
      GLuint plane_idx = ffs(clipmask)+1;
      const GLfloat *plane = clip->plane[plane_idx];
      struct vertex_header *vert_prev = inlist[0];
      GLfloat dp_prev = draw->clip.dotprod( vert_prev->data, plane );
      GLuint outcount = 0;
      GLuint i;

      clipmask &= ~(1<<plane_idx);

      inlist[n] = inlist[0]; /* prevent rotation of vertices */

      for (i = 1; i <= n; i++) {
	 struct vertex_header *vert = inlist[i];

	 GLfloat dp = draw->clip.dotprod( vert->data, plane );

	 if (!IS_NEGATIVE(dp_prev)) {
	    outlist[outcount++] = vert_prev;
	 }

	 if (DIFFERENT_SIGNS(dp, dp_prev)) {
	    struct vertex_header *new_vert = get_new_vertex( draw );
	    outlist[outcount++] = new_vert;

	    if (IS_NEGATIVE(dp)) {
	       /* Going out of bounds.  Avoid division by zero as we
		* know dp != dp_prev from DIFFERENT_SIGNS, above.
		*/
	       GLfloat t = dp / (dp - dp_prev);
	       draw->clip.interp( new_vert, t, vert, vert_prev );
	       
	       /* Force edgeflag true in this case:
		*/
	       new_vert->edgeflag = 1;
	    } else {
	       /* Coming back in.
		*/
	       GLfloat t = dp_prev / (dp_prev - dp);
	       draw->clip.interp( new_vert, t, vert_prev, vert );

	       /* Copy starting vert's edgeflag:
		*/
	       new_vert->edgeflag = vert_prev->edgeflag;
	    }
	 }

	 vert_prev = vert;
	 dp_prev = dp;
      }

      {
	 GLuint *tmp = inlist;
	 inlist = outlist;
	 outlist = tmp;
	 n = outcount;
      }
   }

   /* Emit the polygon as triangles to the setup stage:
    */
   for (i = 2; i < n; i++) {
      stage->next->tri( stage->next, 
			inlist[0],
			inlist[i-1],
			inlist[i] );
   }
}



/* Clip a line against the viewport and user clip planes.
 */
static void
do_clip_line( struct prim_pipeline_stage *stage, 
	      struct vertex_header *v0,
	      struct vertex_header *v1,
	      GLuint clipmask )
{
   struct clip_stage *clip = clip_stage( stage );

   /* XXX: Note stupid hack to deal with tnl's 8-bit clipmask.  Remove
    * this once we correctly use 16bit masks for userclip planes.
    */
   if (clipmask & CLIP_USER_BIT) 
      clipmask |= clip->active_user_planes;

   while (clipmask) {
      GLuint plane_idx = ffs(clipmask)+1;
      const GLfloat *plane = clip->plane[plane_idx];

      clipmask &= ~(1<<plane_idx);

      {
	 const GLfloat dp1 = DP4( v1->ndc, plane );
	 if (dp1 < 0) {
	    GLfloat t = dp1 / (dp1 - dp0);
	    if (t > t1) t1 = t;
	 } 
      }
      
      {
	 const GLfloat dp0 = DP4( v0->ndc, plane );
	 if (dp0 < 0) {
	    GLfloat t = dp0 / (dp0 - dp1);
	    if (t > t0) t0 = t;
	 }
      }

      if (t0 + t1 >= 1.0)
	 return; /* discard */
   }

   {
      struct vertex_header *newv0 = clip_interp( t0, v0, v1, GL_FALSE );
      struct vertex_header *newv1 = clip_interp( t1, v1, v0, GL_FALSE );
   }

   stage->next.line( stage->next, newv0, newv1 );
}



static void
clip_tri( struct prim_pipeline_stage *stage, 
	  struct vertex_header *v0,
	  struct vertex_header *v1,
	  struct vertex_header *v2 )
{
   GLuint clipmask = v0->clipmask | v1->clipmask | v2->clipmask;
   
   if (clipmask == 0) 
      stage->next->tri( stage->next, v0, v1, v2 );
   else if ((v0->clipmask & v1->clipmask & v2->clipmask) != 0)
      clip_tri(stage, v0, v1, v2, clipmask);
}

      
static void
clip_line( struct prim_pipeline_stage *stage, 
	   struct vertex_header *v0,
	   struct vertex_header *v1 )
{
   GLuint clipmask = v0->clipmask | v1->clipmask;
   
   if (clipmask == 0) 
      stage->next->line( stage->next, v0, v1 );
   else if ((v0->clipmask & v1->clipmask) != 0)
      clip_line(stage, clipmask, v0, v1);
}

static void
clip_point( struct prim_pipeline_stage *stage, 
	    struct vertex_header *v0 )
{
   if (v0->clipmask == 0) 
      stage->next->point( stage->next, v0 );
}
