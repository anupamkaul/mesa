
#include "pipe/p_debug.h"
#include "u_linear.h"

void
pipe_linear_to_tile(size_t src_stride, void *src_ptr,
		    struct pipe_tile_info *t, void *dst_ptr)
{
   unsigned x, y, offset;
   char *ptr, *dst;
   unsigned rows = t->rows, cols = t->cols;
   size_t bytes = t->cols * t->block.size;


   assert(pipe_linear_check_tile(t));

   /* lets write lineary to the tiled buffer */
   for (x = t->left; x < t->right; x += cols) {
      cols = t->cols - x % t->cols;
      if (x + cols > t->right)
         cols = t->right - x;
      ptr = (char*)src_ptr + (x - t->left) * t->block.size;
      offset = x / t->cols * t->tile.size + (x % t->cols) * t->block.size;
      for (y = t->top; y < t->bottom; y++) {
         dst = (char*)dst_ptr + offset
                + y / t->rows * t->stride * t->rows
                + (y % t->rows) * bytes;
         memcpy(dst, ptr, cols * t->block.size);
         ptr += src_stride;
      }
   }
}

void pipe_linear_from_tile(struct pipe_tile_info *t, void  *src_ptr,
			   size_t dst_stride, void *dst_ptr)
{
   unsigned x, y, offset;
   unsigned rows = t->rows, cols = t->cols;
   char *ptr, *src;
   size_t bytes = t->cols * t->block.size;

   /* lets write lineary to the tiled buffer */
   for (x = t->left; x < t->right; x += cols) {
      cols = t->cols - x % t->cols;
      if (x + cols > t->right)
         cols = t->right - x;
      ptr = (char*)dst_ptr + (x - t->left) * t->block.size;
      offset = x / t->cols * t->tile.size + (x % t->cols) * t->block.size;
      for (y = t->top; y < t->bottom; y++) {
         src = (char*)src_ptr + offset
                + y / t->rows * t->stride * t->rows
                + (y % t->rows) * bytes;
         memcpy(ptr, src, cols * t->block.size);
         ptr += dst_stride;
      }
   }
}

void
pipe_linear_fill_info(struct pipe_tile_info *t,
		      struct pipe_format_block *block,
		      unsigned tile_width, unsigned tile_height,
		      unsigned tiles_x, unsigned tiles_y,
		      unsigned left, unsigned top,
		      unsigned right, unsigned bottom)
{
   t->block = *block;

   t->tile.width = tile_width;
   t->tile.height = tile_height;
   t->cols = t->tile.width / t->block.width;
   t->rows = t->tile.height / t->block.height;
   t->tile.size = t->cols * t->rows * t->block.size;

   t->tiles_x = tiles_x;
   t->tiles_y = tiles_y;
   t->stride = t->cols * t->tiles_x * t->block.size;
   t->size = t->tiles_x * t->tiles_y * t->tile.size;

   t->left = left;
   t->top = top;
   t->right = right;
   t->bottom = bottom;
}
