
#ifndef U_TRANSFER_H
#define U_TRANSFER_H

/* Fallback implementations for inline read/writes which just go back
 * to the regular transfer behaviour.
 */
#include "pipe/p_state.h"

struct pipe_context;


void u_transfer_inline_write( struct pipe_context *pipe,
			       struct pipe_resource *resource,
			       struct pipe_subresource sr,
			       enum pipe_transfer_usage usage,
			       const struct pipe_box *box,
			      const void *data );
void u_transfer_inline_read( struct pipe_context *pipe,
			      struct pipe_resource *resource,
			      struct pipe_subresource sr,
			      enum pipe_transfer_usage usage,
			      const struct pipe_box *box,
			     void *data );
#endif
