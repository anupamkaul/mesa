#include "pipe/p_context.h"
#include "util/u_rect.h"
#include "util/u_inlines.h"
#include "util/u_transfer.h"

/* One-shot transfer operation with data supplied in a user
 * pointer.  XXX: strides??
 */
void u_transfer_inline_write( struct pipe_context *pipe,
			       struct pipe_resource *resource,
			       struct pipe_subresource sr,
			       enum pipe_transfer_usage usage,
			       const struct pipe_box *box,
			       const void *data )
{
   struct pipe_transfer *transfer = NULL;
   uint8_t *map = NULL;

   transfer = pipe->get_transfer(pipe, 
				 resource,
				 sr,
				 usage,
				 box );
   if (transfer == NULL)
      goto out;

   map = pipe_transfer_map(pipe, transfer);
   if (map == NULL)
      goto out;

   assert(box->depth == 1);	/* XXX: fix me */
   
   util_copy_rect(map,
		  resource->format,
		  transfer->stride, /* bytes? */
		  0, 0,
		  box->width,
		  box->height,
		  data,
		  box->width,	/* bytes? texels? */
		  0, 0);

out:
   if (map)
      pipe_transfer_unmap(pipe, transfer);

   if (transfer)
      pipe_transfer_destroy(pipe, transfer);
}



/* One-shot read transfer operation with data returned in a user
 * pointer.  XXX: strides??
 */
void u_transfer_inline_read( struct pipe_context *pipe,
			      struct pipe_resource *resource,
			      struct pipe_subresource sr,
			      enum pipe_transfer_usage usage,
			      const struct pipe_box *box,
			      void *data )
{
   struct pipe_transfer *transfer = NULL;
   uint8_t *map = NULL;

   transfer = pipe->get_transfer(pipe, 
				 resource,
				 sr,
				 usage,
				 box );
   if (transfer == NULL)
      goto out;

   map = pipe_transfer_map(pipe, transfer);
   if (map == NULL)
      goto out;

   assert(box->depth == 1);	/* XXX: fix me */
   
   util_copy_rect(data,
		  resource->format,
		  transfer->stride, /* bytes? */
		  0, 0,
		  box->width,
		  box->height,
		  map,
		  box->width,	/* bytes? texels? */
		  0, 0);


out:
   if (map)
      pipe_transfer_unmap(pipe, transfer);

   if (transfer)
      pipe_transfer_destroy(pipe, transfer);
}

