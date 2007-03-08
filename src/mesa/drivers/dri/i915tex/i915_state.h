#define DST_BLND_FACT(f) ((f)<<S6_CBUF_DST_BLEND_FACT_SHIFT)
#define SRC_BLND_FACT(f) ((f)<<S6_CBUF_SRC_BLEND_FACT_SHIFT)
#define DST_ABLND_FACT(f) ((f)<<IAB_DST_FACTOR_SHIFT)
#define SRC_ABLND_FACT(f) ((f)<<IAB_SRC_FACTOR_SHIFT)



static GLuint
translate_blend_equation(GLenum mode)
{
   switch (mode) {
   case GL_FUNC_ADD:
      return BLENDFUNC_ADD;
   case GL_MIN:
      return BLENDFUNC_MIN;
   case GL_MAX:
      return BLENDFUNC_MAX;
   case GL_FUNC_SUBTRACT:
      return BLENDFUNC_SUBTRACT;
   case GL_FUNC_REVERSE_SUBTRACT:
      return BLENDFUNC_REVERSE_SUBTRACT;
   default:
      return 0;
   }
}
