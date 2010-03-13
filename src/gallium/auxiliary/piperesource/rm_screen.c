
#include "rm.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

static void translate_template( const struct pipe_resource *template,
				struct pipe_texture *tex )
{
   tex->tex_usage = template->tex_usage;
}

/* This module implements the following new-style driver functions in
 * terms of the old ones that are still present in most pipe drivers.
 */
struct pipe_resource *rm_resource_create(struct rm_screen *rm_screen,
					 const struct pipe_resource *template)
{
   struct rm_resource *resource;

   resource = CALLOC_STRUCT(rm_resource);
   if (resource == NULL)
      goto fail;

   pipe_reference_init(&resource->base.reference, 1);
   resource->base.screen = rm_screen->screen;

   if (template->target == PIPE_BUFFER) {
      struct pipe_texture tex_template;
      
      translate_template(template, &tex_template);
      
      resource->texture = rm_screen->cb.texture_create(rm_screen->screen,
							&tex_template);
      if (resource->texture == NULL)
	 goto fail;
   }
   else {
      resource->buffer = rm_screen->cb.buffer_create(rm_screen->screen,
						     rm_screen->default_buffer_alignment,
						     template->usage,
						     template->width0);
      if (resource->buffer == NULL)
	 goto fail;
   }

   return &resource->base;

fail:
   FREE(resource);
   return NULL;
}

struct pipe_resource *rm_resource_from_handle(struct rm_screen *rm_screen,
					       const struct pipe_resource *template,
					       struct winsys_handle *handle)
{
   struct rm_resource *resource;
   struct pipe_texture tex_template;

   assert(template->target != PIPE_BUFFER);
   
   resource = CALLOC_STRUCT(rm_resource);
   if (resource == NULL)
      goto fail;

   translate_template(template, &tex_template);

   pipe_reference_init(&resource->base.reference, 1);
   resource->base.screen = rm_screen->screen;
   resource->texture = rm_screen->cb.texture_from_handle(rm_screen->screen,
							  &tex_template,
							  handle);
   if (resource->texture == NULL)
      goto fail;

   return &resource->base;

fail:
   FREE(resource);
   return NULL;
}

boolean rm_resource_get_handle(struct rm_screen *rm_screen,
			       struct pipe_resource *resource,
			       struct winsys_handle *handle)
{
   struct rm_resource *rmr = rm_resource(resource);
   assert(rmr->texture);
   return rm_screen->cb.texture_get_handle(rm_screen->screen,
					    rmr->texture,
					    handle);
}


void rm_resource_destroy(struct rm_screen *rm_screen,
			 struct pipe_resource *resource)
{
   struct rm_resource *rmr = rm_resource(resource);

   if (rmr->texture) {
      rm_screen->cb.texture_destroy(rm_screen->screen,
				    rmr->texture);
   }

   if (rmr->buffer) {
      rm_screen->cb.buffer_destroy(rm_screen->screen,
				   rmr->buffer);
   }

   FREE(rmr);   
}


struct rm_screen *
rm_screen_create( struct pipe_screen *screen,
		  struct rm_screen_callbacks *cb,
		  unsigned default_buffer_alignment)
{
   struct rm_screen *rm_screen;

   rm_screen = CALLOC_STRUCT(rm_screen);
   if (rm_screen == NULL)
      return NULL;

   rm_screen->cb = *cb;
   rm_screen->screen = screen;
   rm_screen->default_buffer_alignment = default_buffer_alignment;

   return rm_screen;
}
