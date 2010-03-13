#include "rm.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"

struct rm_transfer {
   struct pipe_transfer base;
   struct pipe_tex_transfer *tex_transfer;
   struct pipe_buffer *buffer;
   unsigned buffer_usage;
};

static INLINE struct rm_transfer *rm_transfer( struct pipe_transfer *t )
{
   return (struct rm_transfer *)t;
}

struct pipe_transfer *rm_get_transfer(struct rm_context *rm_ctx,
				      struct pipe_resource *resource,
				      struct pipe_subresource sr,
				      enum pipe_transfer_usage usage,
				      const struct pipe_box *box)
{
   struct rm_transfer *transfer;
   struct rm_resource *rmr = rm_resource(resource);

   transfer = CALLOC_STRUCT(rm_transfer);
   if (transfer == NULL)
      return NULL;

   transfer->base.resource = resource;

   if (rmr->texture && box->depth == 1) {
      transfer->tex_transfer = rm_ctx->cb.get_tex_transfer(rm_ctx->pipe,
							   rmr->texture,
							   sr.face,
							   sr.level,
							   box->z,
							   usage,
							   box->x,
							   box->y,
							   box->width,
							   box->height);
   }
   else if (rmr->texture && box->depth > 1) {
      /* TBD.  Nobody is using this yet, hopefully can migrate drivers
       * to new interfaces before I have to implement some sort of a
       * workaround here.
       */
      assert(0);
   }
   else {
      transfer->buffer = rm_resource(resource)->buffer;
      transfer->buffer_usage = buffer_usage(usage);
   }

   assert(transfer->buffer || transfer->tex_transfer);
   return &transfer->base;
}

void rm_transfer_destroy(struct rm_context *rm_ctx,
			 struct pipe_transfer *transfer)
{
   struct rm_transfer *rmt = rm_transfer(transfer);

   if (rmt->tex_transfer)
      rm_ctx->cb.transfer_destroy( rm_ctx->pipe, rmt->tex_transfer );

   FREE(rmt);
}

void *rm_transfer_map( struct rm_context *rm_ctx,
		       struct pipe_transfer *transfer )
{
   struct rm_transfer *rmt = rm_transfer(transfer);
   
   if (rmt->tex_transfer) {
      return rm_ctx->cb.transfer_map( rm_ctx->pipe,
				      rmt->tex_transfer );
   }
   else {
      return rm_ctx->rm_screen->cb.buffer_map( rm_ctx->rm_screen->screen,
					       rmt->buffer,
					       rmt->buffer_usage );
   }
}

/* If transfer was created with WRITE|FLUSH_EXPLICIT, only the
 * regions specified with this call are guaranteed to be written to
 * the resource.
 */
void rm_transfer_flush_region( struct rm_context *rm_ctx,
			       struct pipe_transfer *transfer,
			       const struct pipe_box *box)
{
   struct rm_transfer *rmt = rm_transfer(transfer);

   if (rmt->tex_transfer) {
      /* no such action */
   }
   else if (rm_ctx->rm_screen->cb.buffer_flush_mapped_range) {
      rm_ctx->rm_screen->cb.buffer_flush_mapped_range( rm_ctx->rm_screen->screen,
						    rmt->buffer,
						    box->x,
						    box->width);
   }
   else {
      /* no such action */
   }
}

void rm_transfer_unmap( struct rm_context *rm_ctx,
			struct pipe_transfer *transfer )
{
   struct rm_transfer *rmt = rm_transfer(transfer);

   if (rmt->tex_transfer) {
      rm_ctx->cb.transfer_unmap( rm_ctx->pipe,
				   rmt->tex_transfer );
   }
   else {
      struct pipe_buffer *buffer = rmt->buffer;
      
      rm_ctx->rm_screen->cb.buffer_unmap( rm_ctx->rm_screen->screen,
				       buffer );
   }
}


/* One-shot transfer operation with data supplied in a user
 * pointer.  XXX: strides??
 */
void rm_transfer_inline_write( struct rm_context *rm_ctx,
			       struct pipe_resource *resource,
			       struct pipe_subresource sr,
			       enum pipe_transfer_usage usage,
			       const struct pipe_box *box,
			       const void *data )
{
   struct pipe_transfer *transfer = NULL;
   char *map = NULL;

   transfer = rm_get_transfer(rm_ctx, 
			 resource,
			 sr,
			 usage,
			 box );
   if (transfer == NULL)
      goto out;

   map = rm_transfer_map(rm_ctx, transfer);
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
      rm_transfer_unmap(rm_ctx, transfer);

   if (transfer)
      rm_transfer_destroy(rm_ctx, transfer);
}



/* One-shot read transfer operation with data returned in a user
 * pointer.  XXX: strides??
 */
void rm_transfer_inline_read( struct rm_context *rm_ctx,
			      struct pipe_resource *resource,
			      struct pipe_subresource sr,
			      enum pipe_transfer_usage usage,
			      const struct pipe_box *box,
			      void *data )
{
   struct pipe_transfer *transfer = NULL;
   char *map = NULL;

   transfer = rm_get_transfer(rm_ctx, 
			      resource,
			      sr,
			      usage,
			      box );
   if (transfer == NULL)
      goto out;

   map = rm_transfer_map(rm_ctx, transfer);
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
      rm_transfer_unmap(rm_ctx, transfer);

   if (transfer)
      rm_transfer_destroy(rm_ctx, transfer);
}


struct rm_context *
rm_context_create( struct pipe_context *pipe,
		   struct rm_screen *rm_screen,
		   struct rm_context_callbacks *cb)
{
   struct rm_context *rm_context;

   rm_context = CALLOC_STRUCT(rm_context);
   if (rm_context == NULL)
      return NULL;

   rm_context->cb = *cb;
   rm_context->rm_screen = rm_screen;
   rm_context->pipe = pipe;

   return rm_context;
}
