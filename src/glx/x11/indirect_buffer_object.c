/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * \file indirect_buffer_object.c
 * Client-side implementation of hand-coded buffer object related protocol.
 * 
 * \author Ian Romanick <ian.d.romanick@intel.com>
 */

#include <GL/gl.h>
#include "indirect.h"
#include "glxclient.h"
#include "indirect_size.h"
#include "dispatch.h"
#include "glapi.h"
#include "glthread.h"
#include <GL/glxproto.h>


#define X_GLrop_BindBufferARB 235
void
__indirect_glBindBufferARB(GLenum target, GLuint buffer)
{
   __GLXcontext *const gc = __glXGetCurrentContext();
   __GLXattribute *const state = (__GLXattribute *)(gc->client_state_private);
    const GLuint cmdlen = 12;


    /* Update local buffer binding state.
     */
    switch (target) {
    case GL_ARRAY_BUFFER:
        state->buffer_bindings.array = buffer;
        break;

    case GL_ELEMENT_ARRAY_BUFFER:
        state->buffer_bindings.element = buffer;
        break;

    default:
        break;
    }


    emit_header(gc->pc, X_GLrop_BindBufferARB, cmdlen);
    (void) memcpy((void *) (gc->pc + 4), (void *) (&target), 4);
    (void) memcpy((void *) (gc->pc + 8), (void *) (&buffer), 4);
    gc->pc += cmdlen;
    if (__builtin_expect(gc->pc > gc->limit, 0)) {
        (void) __glXFlushRenderBuffer(gc, gc->pc);
    }
}
