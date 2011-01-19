/*
 * Based exclusively from reading the talloc documentation.
 */

#ifndef _TALLOC_H_
#define _TALLOC_H_

#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "halloc.h"

static inline char *
talloc_asprintf(const void *t, const char *fmt, ...) {
   assert(0);
   return NULL;
}

static inline char *
talloc_asprintf_append(char *s, const char *fmt, ...) {
   assert(0);
   return NULL;
}

static inline void *
talloc_autofree_context(void) {
   assert(0);
   return NULL;
}

static inline int
talloc_free(void *ptr) {
   halloc(ptr, 0);
   return 0;
}
   
static inline void *
talloc_init(const char *fmt, ...) {
   /* XXX can't create empty allocations */
   return halloc(NULL, 1);
}

static inline void *
talloc_parent(const void *ptr) {
   assert(0);
   return NULL;
}

static inline void *
talloc_realloc_size(const void *ctx, void *ptr, size_t size) {
   /* talloc_realloc works like c realloc and matches halloc behaviour */
   void *ret = halloc(ptr, size);

   if (!ptr)
      hattach(ret, (void *)ctx);

   return ret;
}

static inline void *
talloc_reference(const void *ctx, const void *ptr) {
   assert(0);
   return NULL;
}

static inline void 
talloc_set_destructor(void *ctx, int (*destructor)(void*)) {
   assert(0);
}

static inline void *
talloc_size(const void *ctx, size_t size) {
   void *ptr;

   /* XXX can't create empty allocations */
   if (size == 0)
      size = 1;

   ptr = halloc(NULL, size);
   if (!ptr)
      return NULL;

   hattach(ptr, (void*)ctx);

   return ptr;
}

#define talloc_new(_ctx) talloc_strdup(_ctx, __FILE__ )

#define talloc(_ctx, _type) (_type *)talloc_size(_ctx, sizeof(_type))
#define talloc_array_size(_ctx, _size, _count) talloc_size(_ctx, _size * _count)
#define talloc_array(_ctx, _type, _count) (_type *)talloc_size(_ctx, sizeof(_type) * _count)
#define talloc_realloc(_ctx, _ptr, _type, _count) (_type *)talloc_realloc_size(_ctx, _ptr, sizeof(_type) * _count)

static inline void *
talloc_steal(const void *new_ctx, const void *ptr)
{
   /* halloc will assert if new_ctx != NULL */
   if (!ptr)
      return NULL;

   hattach((void*)ptr, (void*)new_ctx);
   return (void*)ptr;
}

static inline char *
talloc_strdup(const void *ctx, const char *p) {
   char *ptr = h_strdup(p);
   if (ptr)
      hattach(ptr, (void *)ctx);
   return ptr;
}

static inline char *
talloc_strdup_append(const void *t, const char *p) {
   assert(0);
   return NULL;
}

static inline char *
talloc_strndup(const void *t, const char *p, size_t n) {
   assert(0);
   return NULL;
}

static inline char *
talloc_strndup_append(char *s, const char *p, size_t n) {
   assert(0);
   return NULL;
}

static inline int
talloc_unlink(const void *ctx, const void *ptr) {
   /* XXX check ctx is parent */
   hattach((void*)ptr, NULL);
   return 0;
}

static inline char *
talloc_vasprintf_append(char *s, const char *fmt, va_list ap) {
   assert(0);
   return NULL;
}

static inline void *
talloc_zero_size(const void *ctx, size_t size) {
   void *ptr = talloc_size(ctx, size);
   if (ptr) {
      memset(ptr, 0, size);
   }
   return ptr;
}

#define talloc_zero(_ctx, _type) (_type *)talloc_zero_size(_ctx, sizeof(_type))
#define talloc_zero_array(_ctx, _type, _count) (_type *)talloc_zero_size(_ctx, sizeof(_type) * _count)

#endif
