
static void
check_wpos(struct i915_fragment_program *p)
{
   GLuint inputs = p->FragProg.Base.InputsRead;
   GLint i;

   p->wpos_tex = -1;

   for (i = 0; i < p->ctx->Const.MaxTextureCoordUnits; i++) {
      if (inputs & FRAG_BIT_TEX(i))
         continue;
      else if (inputs & FRAG_BIT_WPOS) {
         p->wpos_tex = i;
         inputs &= ~FRAG_BIT_WPOS;
      }
   }

   if (inputs & FRAG_BIT_WPOS) {
      i915_program_error(p, "No free texcoord for wpos value");
   }
}


static void brw_merge_inputs( struct brw_context *brw,
		       const struct gl_client_array *arrays[])
{
   struct brw_vertex_element *inputs = brw->vb.inputs;
   struct brw_vertex_info old = brw->vb.info;
   GLuint i;

   memset(inputs, 0, sizeof(*inputs));
   memset(&brw->vb.info, 0, sizeof(brw->vb.info));

   for (i = 0; i < VERT_ATTRIB_MAX; i++) {
      brw->vb.inputs[i].glarray = arrays[i];

      /* XXX: metaops passes null arrays */
      if (arrays[i]) {
	 if (arrays[i]->StrideB != 0)
	    brw->vb.info.varying |= 1 << i;

	 brw->vb.info.sizes[i/16] |= (inputs[i].glarray->Size - 1) << ((i%16) * 2);
      }
   }

   /* Raise statechanges if input sizes and varying have changed: 
    */
   if (memcmp(brw->vb.info.sizes, old.sizes, sizeof(old.sizes)) != 0)
      brw->state.dirty.brw |= BRW_NEW_INPUT_DIMENSIONS;

   if (brw->vb.info.varying != old.varying)
      brw->state.dirty.brw |= BRW_NEW_INPUT_VARYING;
}






   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   const GLuint inputsRead = p->FragProg.Base.InputsRead;



   intel->vertex_attr_count = 0;
   intel->wpos_offset = 0;
   intel->wpos_size = 0;
   intel->coloroffset = 0;
   intel->specoffset = 0;

   if (inputsRead & FRAG_BITS_TEX_ANY) {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_4F_VIEWPORT, S4_VFMT_XYZW, 16);
   }
   else {
      EMIT_ATTR(_TNL_ATTRIB_POS, EMIT_3F_VIEWPORT, S4_VFMT_XYZ, 12);
   }

   if (inputsRead & FRAG_BIT_COL0) {
      intel->coloroffset = offset / 4;
      EMIT_ATTR(_TNL_ATTRIB_COLOR0, EMIT_4UB_4F_BGRA, S4_VFMT_COLOR, 4);
   }

   if (inputsRead & (FRAG_BIT_COL1 | FRAG_BIT_FOGC)) {

      if (inputsRead & FRAG_BIT_COL1) {
         intel->specoffset = offset / 4;
         EMIT_ATTR(_TNL_ATTRIB_COLOR1, EMIT_3UB_3F_BGR, S4_VFMT_SPEC_FOG, 3);
      }
      else
         EMIT_PAD(3);

      if (inputsRead & FRAG_BIT_FOGC)
         EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1UB_1F, S4_VFMT_SPEC_FOG, 1);
      else
         EMIT_PAD(1);
   }

   if (inputsRead & FRAG_BIT_FOGC) {
      EMIT_ATTR(_TNL_ATTRIB_FOG, EMIT_1F, S4_VFMT_FOG_PARAM, 4);
   }

   for (i = 0; i < p->ctx->Const.MaxTextureCoordUnits; i++) {
      if (inputsRead & FRAG_BIT_TEX(i)) {
         int sz = VB->TexCoordPtr[i]->size;

         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(sz));

         EMIT_ATTR(_TNL_ATTRIB_TEX0 + i, EMIT_SZ(sz), 0, sz * 4);
      }
      else if (i == p->wpos_tex) {

         /* If WPOS is required, duplicate the XYZ position data in an
          * unused texture coordinate:
          */
         s2 &= ~S2_TEXCOORD_FMT(i, S2_TEXCOORD_FMT0_MASK);
         s2 |= S2_TEXCOORD_FMT(i, SZ_TO_HW(3));

         intel->wpos_offset = offset;
         intel->wpos_size = 3 * sizeof(GLuint);

         EMIT_PAD(intel->wpos_size);
      }
   }



   GLuint s4 = i915->state.Ctx[I915_CTXREG_LIS4] & ~S4_VFMT_MASK;
   GLuint s2 = S2_TEXCOORD_NONE;
   if (s2 != i915->state.Ctx[I915_CTXREG_LIS2] ||
       s4 != i915->state.Ctx[I915_CTXREG_LIS4]) {
      int k;

      I915_STATECHANGE(i915, I915_UPLOAD_CTX);

      /* Must do this *after* statechange, so as not to affect
       * buffered vertices reliant on the old state:
       */
      intel->vertex_size = _tnl_install_attrs(&intel->ctx,
                                              intel->vertex_attrs,
                                              intel->vertex_attr_count,
                                              intel->ViewportMatrix.m, 0);

      intel->vertex_size >>= 2;

      i915->state.Ctx[I915_CTXREG_LIS2] = s2;
      i915->state.Ctx[I915_CTXREG_LIS4] = s4;
   }
