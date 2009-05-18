
/* As long as 'upgrade' doesn't change the type of the existing
 * attribute, upgrading a vertex is fairly straight-forward.
 */
static INLINE void upgrade_vertex( char *dest,
                                   const char *src,
                                   unsigned oldsize,
                                   const char *inject,
                                   unsigned inject_size
                                   unsigned inject_offset )
{
   memcpy(dest,
          src,
          inject_offset);

   memcpy(dest + inject_offset,
          inject,
          inject_size);

   if (inject_offset != oldsize) {
      memcpy(dest + inject_offset + inject_size
             src + inject_offset,
             oldsize - inject_offset);
   }
}

static INLINE void upgrade_attrib( struct st_exec *exec,
                                   unsigned attrib,
                                   unsigned new_attrib_size )
{
   unsigned old_attrib_size = exec->attrib[attrib].size;
   unsigned extra_bytes = (new_attrib_size - old_attrib_size) * sizeof(float);
   unsigned new_vertex_size = exec->vertex_size + extra_bytes;

   for (i = 0; i < 4; i++) {
      char *new_vtx = MALLOC( new_vertex_size );

      upgrade_vertex( new_vtx,
                      exec->slot[i].vertex,
                      exec->vertex_size,
                      (const char *)&identity[old_attrib_size],
                      extra_bytes,
                      exec->attrib[attrib].offset );

      FREE( exec->slot[i].vertex );
      exec->slot[i].vertex = new_vtx;
   }

   exec->attrib[attrib].size = new_attrib_size;

   for (i = attrib + 1; i < exec->nr_attribs; i++)
      exec->attrib[i].offset += extra_bytes;

   for (i = 0; i < exec->nr_attribs; i++)
      exec->attrib[i].ptr += exec->vertex + exec->attrib[i].offset;

   exec->vertex_size = new_vertex_size;
}


static INLINE void new_attrib( struct st_exec *exec,
                               unsigned attrib_name,
                               unsigned size )
{
   unsigned i = exec->nr_attribs;
   exec->attrib[i].name = attrib_name;
   exec->attrib[i].offset = exec->vertex_size;
   exec->attrib[i].size = 0;
   exec->attrib[i].ptr = exec->vertex + exec->attrib[i].offset;
   exec->nr_attribs++;

   upgade_attrib( exec, i, size );
}


static INLINE char *new_prim( struct st_exec *exec,
                              GLenum mode,
                              unsigned verts )
{

   assert(exec->mode == GL_POLYGON+1);
   exec->prim.mode = mode;
   exec->prim.verts = 0;
   exec->prim.start =


   retval = extend_prim( exec, verts );
   assert(retval);
   return retval;
}


static INLINE char *extend_prim( struct st_exec *exec,
                                 unsigned verts )
{
   unsigned bytes = verts * exec->vertex_size;
   char *retval;

   assert(exec->mode != GL_POLYGON+1);

   if (exec->used + bytes > exec->total)
      return NULL;

   retval = exec->ptr + exec->used;
   exec->used += bytes;
   exec->prim.verts += verts;
   return retval;
}


/* POINTS
 */
static void emit_point_subsequent_slot_zero( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POINTS, 1 );
   }

   emit_vertex( exec, dest, 0 );
   exec->slotnr = 0;
}

static void emit_point_first_slot_zero( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_POINTS, 1 );
   emit_vertex( exec, dest, 0 );
   exec->slotnr = 0;
   exec->slot[0].func = emit_point_subsequent_slot_zero;
}


/* LINES
 */
static void emit_line_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINES, 2 );
   }

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   exec->slotnr = 0;
}

static void emit_line_first_slot_one( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_LINES, 2 );
   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   exec->slotnr = 0;
   exec->slot[1].func = emit_line_subsequent_slot_one;
}

/* LINE_STRIP
 */
static void emit_linestrip_subsequent_slot_zero( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 0 );
   exec->slotnr = 1;
}


static void emit_linestrip_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, dest, 0 );
   }

   emit_vertex( exec, dest, 1 );
   exec->slotnr = 0;
}


static void emit_linestrip_first_slot_one( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_LINE_STRIP, 2 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   exec->slotnr = 1;
   exec->slot[0].func = emit_linestrip_subsequent_slot_zero;
   exec->slot[1].func = emit_linestrip_subsequent_slot_one;
}


/* LINE_LOOP
 */
static void emit_lineloop_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, dest, 2 );
   }

   emit_vertex( exec, dest, 1 );
   exec->slotnr = 2;
}


static void emit_lineloop_subsequent_slot_two( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 2 );
   exec->slotnr = 1;
}


static void emit_lineloop_first_slot_one( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_LINE_STRIP, 2 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit->slotnr = 2;
   exec->slot[1].func = emit_lineloop_subsequent_slot_one;
   exec->slot[2].func = emit_lineloop_subsequent_slot_two;
   exec->slot[2].end = emit_lineloop_end_slot_two;
   exec->slot[1].end = emit_lineloop_end_slot_one;
}




/* TRIANGLES
 */
static void emit_triangle_subsequent_slot_two( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLES, 2 );
   }

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   exec->slotnr = 0;
}

static void emit_first_slot_two( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLES, 2 );
   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   exec->slotnr = 0;
   exec->slot[2].func = emit_triangle_subsequent_slot_two;
}

/* TRIANGLE_STRIP
 */

static void emit_tristrip_subsequent_slot_zero( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, dest, 2 );
      emit_vertex( exec, dest, 3 );
   }

   emit_vertex( exec, dest, 0 );
   exec->slotnr = 1;
}

static void emit_tristrip_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 3 );
   }

   emit_vertex( exec, dest, 1 );
   exec->slotnr = 2;
}

static void emit_tristrip_subsequent_slot_two( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 2 );
   exec->slotnr = 3;
}


static void emit_tristrip_subsequent_slot_three( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, dest, 2 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 3 );
   exec->slotnr = 0;
}


static void emit_tristrip_first_slot_two( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLE_STRIP, 3 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   exec->slotnr = 3;
   exec->slot[0].func = emit_tristrip_subsequent_slot_zero;
   exec->slot[1].func = emit_tristrip_subsequent_slot_one;
   exec->slot[2].func = emit_tristrip_subsequent_slot_two;
   exec->slot[3].func = emit_tristrip_subsequent_slot_three;
}



/* TRIANGLE_FAN
 */
static void emit_trifan_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_FAN, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 2 );
   }

   emit_vertex( exec, dest, 1 );
   exec->slotnr = 2;
}

static void emit_trifan_subsequent_slot_two( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_FAN, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 2 );
   exec->slotnr = 1;
}


static void emit_trifan_first_slot_two( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLE_STRIP, 3 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   exec->slotnr = 1;
   emit->slot[1].func = emit_trifan_subsequent_slot_one;
   emit->slot[2].func = emit_trifan_subsequent_slot_two;
}



/* QUADS
 */
static void emit_quad_subsequent_slot_three( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 4 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
   }

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   emit_vertex( exec, dest, 4 );
   exec->slotnr = 0;
}


static void emit_quad_first_slot_three( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_QUAD_STRIP, 4 );
   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   emit_vertex( exec, dest, 4 );
   exec->slotnr = 0;
   exec->slot[3].func = emit_quad_subsequent_slot_three;
}

/* QUADSTRIP
 */

static void emit_quadstrip_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
      emit_vertex( exec, dest, 2 );
      emit_vertex( exec, dest, 3 );
   }

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   exec->slotnr = 2;
}

static void emit_quadstrip_subsequent_slot_three( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 2 );
   emit_vertex( exec, dest, 3 );
   exec->slotnr = 0;
}


static void emit_quadstrip_first_slot_three( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_QUAD_STRIP, 4 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   emit_vertex( exec, dest, 3 );
   exec->slotnr = 0;
   exec->slot[1].func = emit_quadstrip_subsequent_slot_one;
   exec->slot[3].func = emit_quadstrip_subsequent_slot_three;
}

/* POLYGON
 */
static void emit_polygon_subsequent_slot_one( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POLYGON, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 2 );
   }

   emit_vertex( exec, dest, 1 );
   exec->slotnr = 2;
}

static void emit_polygon_subsequent_slot_two( struct st_exec *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POLYGON, 3 );
      emit_vertex( exec, dest, 0 );
      emit_vertex( exec, dest, 1 );
   }

   emit_vertex( exec, dest, 2 );
   exec->slotnr = 1;
}


static void emit_polygon_first_slot_two( struct st_exec *exec )
{
   char *dest = new_prim( exec, GL_POLYGON, 3 );

   emit_vertex( exec, dest, 0 );
   emit_vertex( exec, dest, 1 );
   emit_vertex( exec, dest, 2 );
   exec->slotnr = 1;
   emit->slot[1].func = emit_polygon_subsequent_slot_one;
   emit->slot[2].func = emit_polygon_subsequent_slot_two;
}

/* Noop
 */
static void emit_noop( struct st_exec *exec )
{
   exec->slotnr++;
}

static emit_func[GL_POLYGON+1][4] =
{
   { emit_point_first_slot_zero,
     NULL,
     NULL,
     NULL },

   { emit_noop,
     emit_line_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_lineloop_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_linestrip_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_noop,
     emit_triangle_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_tristrip_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_trifan_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_noop,
     emit_quad_first_slot_three },

   { emit_noop,
     emit_noop,
     emit_noop,
     emit_quadstrip_first_slot_three },

   { emit_noop,
     emit_noop,
     emit_polygon_first_slot_two,
     NULL }
};
