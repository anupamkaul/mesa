#ifndef RM_PRIVATE_H
#define RM_PRIVATE_H

#include "rm_public.h"

struct rm_screen {
   struct pipe_screen *screen;
   struct rm_screen_callbacks cb;
   unsigned default_buffer_alignment;
};

struct rm_context {
   struct pipe_context *pipe;
   struct rm_screen *rm_screen;	/* ? */
   struct rm_context_callbacks cb;
};

#endif
