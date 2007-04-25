

/* Offset tri.  i915 can handle this, but not i830, and neither when
 * unfilled rendering.
 */
static void offset_tri( struct prim_pipeline_stage *stage,
			struct vertex_header *v0,
			struct vertex_header *v1,
			struct vertex_header *v2 )
{
   GLfloat det = calc_det(v0, v1, v2);

   /* Should have been detected in a cull stage??
    */
   if (FABSF(det) > 1e-8) {
      GLfloat offset = ctx->Polygon.OffsetUnits * DEPTH_SCALE;
      GLfloat inv_det = 1.0 / det;

      GLfloat z0 = v0->data[2].f;
      GLfloat z1 = v1->data[2].f;
      GLfloat z2 = v2->data[2].f;

      GLfloat ez = z0 - z2;
      GLfloat fz = z1 - z2;
      GLfloat a	= ey*fz - ez*fy;
      GLfloat b	= ez*fx - ex*fz;
      GLfloat ac = a * inv_det;
      GLfloat bc = b * inv_det;

      if ( ac < 0.0f ) ac = -ac;
      if ( bc < 0.0f ) bc = -bc;

      offset += MAX2( ac, bc ) * ctx->Polygon.OffsetFactor;
      offset *= ctx->DrawBuffer->_MRD;

      v0 = dup_vert(stage, v0);
      v1 = dup_vert(stage, v1);
      v2 = dup_vert(stage, v2);

      v0->data[2] += offset;
      v1->data[2] += offset;
      v2->data[2] += offset;

      stage->next->tri( stage->next, v0, v1, v2 );
   }
}
