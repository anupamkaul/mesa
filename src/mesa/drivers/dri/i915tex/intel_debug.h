/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#ifndef INTEL_DEBUG_H
#define INTEL_DEBUG_H

/* ================================================================
 * Debugging:
 */
#if defined(DEBUG)
#define DO_DEBUG 1
extern int INTEL_DEBUG;
#else
#define INTEL_DEBUG		0
#endif

#define DEBUG_TEXTURE	  0x1
#define DEBUG_STATE	  0x2
#define DEBUG_IOCTL	  0x4
#define DEBUG_BLIT	  0x8
#define DEBUG_MIPTREE     0x10
#define DEBUG_FALLBACKS	  0x20
#define DEBUG_VERBOSE     0x40
#define DEBUG_BATCH       0x80
#define DEBUG_PIXEL       0x100
#define DEBUG_BUFMGR      0x200
#define DEBUG_REGION      0x400
#define DEBUG_FBO         0x800
#define DEBUG_LOCK        0x1000
#define DEBUG_IDX         0x2000
#define DEBUG_RENDER      0x4000
#define DEBUG_ALWAYS_SYNC 0x8000
#define DEBUG_VBO         0x10000
#define DEBUG_FRAME       0x20000

#define DBG(...)  do { if (INTEL_DEBUG & FILE_DEBUG_FLAG) _mesa_printf(__VA_ARGS__); } while(0)


struct debug_stream 
{
   GLuint offset;		/* current gtt offset */
   GLubyte *ptr;			/* pointer to gtt offset zero */
   GLboolean print_addresses;
};



#endif
