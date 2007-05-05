#ifndef INTEL_UTILS_H
#define INTEL_UTILS_H

#include "imports.h"

static INLINE int align(int x, int align)
{
   return (x + align - 1) & ~(align - 1);
}

/* ================================================================
 * From linux kernel i386 header files, copes with odd sizes better
 * than COPY_DWORDS would:
 * XXX Put this in src/mesa/main/imports.h ???
 */
#if defined(i386) || defined(__i386__)
static INLINE void *
__memcpy(void *to, const void *from, size_t n)
{
   int d0, d1, d2;
   __asm__ __volatile__("rep ; movsl\n\t"
                        "testb $2,%b4\n\t"
                        "je 1f\n\t"
                        "movsw\n"
                        "1:\ttestb $1,%b4\n\t"
                        "je 2f\n\t"
                        "movsb\n" "2:":"=&c"(d0), "=&D"(d1), "=&S"(d2)
                        :"0"(n / 4), "q"(n), "1"((long) to), "2"((long) from)
                        :"memory");
   return (to);
}
#else
#define __memcpy(a,b,c) memcpy(a,b,c)
#endif

union fi { 
   GLfloat f;
   GLuint u; 
   GLint i;
};

static INLINE GLuint count_bits( GLuint mask )
{
   GLuint i, nr = 0;

   for (i = 1; mask >= i; i <<= 1) 
      if (mask & i)
	 nr++;

   return nr;
}


static INLINE GLuint page_space( void *ptr )
{
   return 4096 - (((unsigned long)ptr) & (4096-1));
}

#endif
