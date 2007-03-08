
struct i915_fp_compile {
   GLcontext *ctx;

   struct i915_fragment_program *fp;

   GLuint declarations[I915_PROGRAM_SIZE];
   GLuint program[I915_PROGRAM_SIZE];

   GLfloat constant[I915_MAX_CONSTANT][4];
   GLuint constant_flags[I915_MAX_CONSTANT];
   GLuint nr_constants;

   GLuint *csr;                 /* Cursor, points into program.
                                 */

   GLuint *decl;                /* Cursor, points into declarations.
                                 */

   GLuint decl_s;               /* flags for which s regs need to be decl'd */
   GLuint decl_t;               /* flags for which t regs need to be decl'd */

   GLuint temp_flag;            /* Tracks temporary regs which are in
                                 * use.
                                 */

   GLuint utemp_flag;           /* Tracks TYPE_U temporary regs which are in
                                 * use.
                                 */



   /* Helpers for i915_fragprog.c:
    */
   GLuint wpos_tex;

   struct
   {
      GLuint reg;               /* Hardware constant idx */
      const GLfloat *values;    /* Pointer to tracked values */
   } param[I915_MAX_CONSTANT];
   GLuint nr_params;

};
