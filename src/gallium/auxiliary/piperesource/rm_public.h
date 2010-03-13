#ifndef RM_PUBLIC_H
#define RM_PUBLIC_H

#include "pipe/p_state.h"

/* Here are the old pipe_buffer and pipe_texture structs, almost as
 * they were.  Old pre-resources driver code that creates and
 * manipulates these structs can be used to implement the new
 * pipe_resource behaviour using this adaptor module.
 */
struct pipe_buffer
{
   struct pipe_reference  reference;
   struct rm_screen      *screen; /* Note! */
   unsigned               size;
   unsigned               alignment;
   unsigned               usage;
};


struct pipe_texture
{ 
   struct pipe_reference    reference;
   struct rm_screen        *screen;	/* Note! */
   enum pipe_texture_target target; 
   enum pipe_format         format;         
   unsigned                 width0;
   unsigned                 height0;
   unsigned                 depth0;
   unsigned                 last_level:8;
   unsigned                 nr_samples:8;
   unsigned                 tex_usage;       /**< bitmask of PIPE_TEXTURE_USAGE_* */
};

/* Have to give the legacy transfer struct a new name to avoid
 * conflicts.
 */
struct pipe_tex_transfer
{
   unsigned x;                   /**< x offset from start of texture image */
   unsigned y;                   /**< y offset from start of texture image */
   unsigned width;               /**< logical width in pixels */
   unsigned height;              /**< logical height in pixels */
   unsigned stride;              /**< stride in bytes between rows of blocks */
   enum pipe_transfer_usage usage; /**< PIPE_TRANSFER_*  */

   struct pipe_texture *texture; /**< texture to transfer to/from  */
   unsigned face;
   unsigned level;
   unsigned zslice;
};


struct rm_resource {
   struct pipe_resource base;
   struct pipe_buffer *buffer;
   struct pipe_texture *texture;
};

static INLINE struct rm_resource *
rm_resource( struct pipe_resource *resource )
{
   return (struct rm_resource *)resource;
}

static INLINE struct pipe_texture *
rm_texture( struct pipe_resource *resource )
{
   return rm_resource(resource)->texture;
}

static INLINE struct pipe_buffer *
rm_buffer( struct pipe_resource *resource )
{
   return rm_resource(resource)->buffer;
}


struct rm_screen_callbacks {
   struct pipe_texture * (*texture_create)(struct pipe_screen *,
                                           const struct pipe_texture *templat);

   struct pipe_texture * (*texture_from_handle)(struct pipe_screen *,
                                                const struct pipe_texture *templat,
                                                struct winsys_handle *handle);

   boolean (*texture_get_handle)(struct pipe_screen *,
                                 struct pipe_texture *tex,
                                 struct winsys_handle *handle);


   void (*texture_destroy)(struct pipe_screen *,
			   struct pipe_texture *pt);

   struct pipe_surface *(*get_tex_surface)(struct pipe_screen *,
                                           struct pipe_texture *texture,
                                           unsigned face, unsigned level,
                                           unsigned zslice,
                                           unsigned usage );

   void (*tex_surface_destroy)(struct pipe_surface *);
   

   /* Buffer functions:
    */
   struct pipe_buffer *(*buffer_create)( struct pipe_screen *screen,
                                         unsigned alignment,
                                         unsigned usage,
                                         unsigned size );

   struct pipe_buffer *(*user_buffer_create)(struct pipe_screen *screen,
                                             void *ptr,
                                             unsigned bytes);

   void *(*buffer_map)( struct pipe_screen *screen,
			struct pipe_buffer *buf,
			unsigned usage );

   void *(*buffer_map_range)( struct pipe_screen *screen,
                              struct pipe_buffer *buf,
                              unsigned offset,
                              unsigned length,
                              unsigned usage);

   void (*buffer_flush_mapped_range)( struct pipe_screen *screen,
                                      struct pipe_buffer *buf,
                                      unsigned offset,
                                      unsigned length);

   void (*buffer_unmap)( struct pipe_screen *screen,
                         struct pipe_buffer *buf );

   void (*buffer_destroy)( struct pipe_screen *screen,
			   struct pipe_buffer *buf );
};


struct rm_context_callbacks {
   unsigned int (*is_texture_referenced)(struct pipe_context *pipe,
					 struct pipe_texture *texture,
					 unsigned face, unsigned level);

   unsigned int (*is_buffer_referenced)(struct pipe_context *pipe,
					struct pipe_buffer *buf);

   struct pipe_tex_transfer *(*get_tex_transfer)(struct pipe_context *,
					   struct pipe_texture *texture,
					   unsigned face, unsigned level,
					   unsigned zslice,
					   enum pipe_transfer_usage usage,
					   unsigned x, unsigned y,
					   unsigned w, unsigned h);

   void (*transfer_destroy)(struct pipe_context *,
                                struct pipe_tex_transfer *);
   
   void *(*transfer_map)( struct pipe_context *,
                          struct pipe_tex_transfer *transfer );

   void (*transfer_unmap)( struct pipe_context *,
                           struct pipe_tex_transfer *transfer );
};



struct rm_screen *
rm_screen_create( struct pipe_screen *screen,
		  struct rm_screen_callbacks *callbacks,
		  unsigned default_buffer_alignment);

struct rm_context *
rm_context_create( struct pipe_context *context,
		   struct rm_screen *rm_screen,
		   struct rm_context_callbacks *callbacks );


/* This module implements the following new-style driver functions in
 * terms of the old ones that are still present in most pipe drivers.
 */
struct pipe_resource *rm_resource_create(struct rm_screen *,
					  const struct pipe_resource *template);

struct pipe_resource *rm_resource_from_handle(struct rm_screen *,
					       const struct pipe_resource *template,
					       struct winsys_handle *handle);

boolean rm_resource_get_handle(struct rm_screen *,
			       struct pipe_resource *tex,
			       struct winsys_handle *handle);


void rm_resource_destroy(struct rm_screen *,
			 struct pipe_resource *pt);

struct pipe_transfer *rm_get_transfer(struct rm_context *,
				      struct pipe_resource *resource,
				      struct pipe_subresource,
				      enum pipe_transfer_usage,
				      const struct pipe_box *);

void rm_transfer_destroy(struct rm_context *,
                                struct pipe_transfer *);

void *rm_transfer_map( struct rm_context *,
			struct pipe_transfer *transfer );

void rm_transfer_flush_region( struct rm_context *,
			       struct pipe_transfer *transfer,
			       const struct pipe_box *);

void rm_transfer_unmap( struct rm_context *,
			struct pipe_transfer *transfer );


void rm_transfer_inline_write( struct rm_context *,
			       struct pipe_resource *,
			       struct pipe_subresource,
			       enum pipe_transfer_usage,
			       const struct pipe_box *,
			       const void *data );

void rm_transfer_inline_read( struct rm_context *,
			      struct pipe_resource *,
			      struct pipe_subresource,
			      enum pipe_transfer_usage,
			      const struct pipe_box *,
			      void *data );

#endif
