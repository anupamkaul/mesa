/*
 * Based exclusively from reading the talloc documentation.
 */

#ifndef _TALLOC_H_
#define _TALLOC_H_

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "halloc.h"

#define talloc_autofree_context h_autofree_context

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

#define talloc_parent(_ptr) h_get_parent(_ptr)

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

#define talloc_set_destructor(_ctx, _des) h_set_destructor(_ctx, _des)

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

static inline int
talloc_unlink(const void *ctx, const void *ptr) {
   /* XXX check ctx is parent */
   hattach((void*)ptr, NULL);
   return 0;
}

static inline char *
talloc_strdup(const void *ctx, const char *p) {
   char *ptr;

   if (!p)
      return NULL;

   ptr = h_strdup(p);
   if (ptr)
      hattach(ptr, (void *)ctx);

   return ptr;
}

static inline char *
talloc_strdup_append(char *s, const char *p) {
   size_t old_size;
   size_t p_size;

   if (!p)
      return s;

   if (!s)
      return h_strdup(p);

   p_size = strlen(p);
   old_size = strlen(s);

   s = (char*)talloc_realloc_size(NULL, s, old_size + p_size + 1);

   memcpy(&s[old_size], p, p_size);
   s[old_size + p_size] = 0;

   return s;
}

static inline char *
talloc_strndup(const void *t, const char *p, size_t n) {
   char *ret;
   size_t len;

   if (!p)
      return NULL;

   len = strlen(p);

   /* sigh no min */
   if (len < n)
      n = len;

   ret = (char *)talloc_size(t, n + 1);
   memcpy(ret, p, n);
   ret[n] = 0;

   return ret;
}

static inline char *
talloc_strndup_append(char *s, const char *p, size_t n) {
   size_t old_size = 0;
   size_t p_size;

   if (!p)
      return s;

   p_size = strlen(p);

   /* sigh no min */
   if (n < p_size)
      p_size = n;

   if (s)
      old_size = strlen(s);

   s = (char*)talloc_realloc_size(NULL, s, old_size + p_size + 1);

   memcpy(&s[old_size], p, p_size);
   s[old_size + p_size] = 0;

   return s;
}

static inline char *
talloc_vasprintf(const void *t, const char *fmt, va_list ap) {
   char *ret;
   char *tmp = NULL;

   vasprintf(&tmp, fmt, ap);

   ret = talloc_strdup(t, tmp);
   free(tmp);

   return ret;
}

static inline char *
talloc_vasprintf_append(char *s, const char *fmt, va_list ap) {
   char *ret;
   char *tmp = NULL;

   vasprintf(&tmp, fmt, ap);

   ret = talloc_strdup_append(s, tmp);
   free(tmp);

   return ret;
}

static inline char *
talloc_asprintf(const void *t, const char *fmt, ...) {
   char *ret;
   va_list ap;

   va_start(ap, fmt);
   ret = talloc_vasprintf(t, fmt, ap);
   va_end(ap);

   return ret;
}

static inline char *
talloc_asprintf_append(char *s, const char *fmt, ...) {
   char *ret;
   va_list ap;

   va_start(ap, fmt);
   ret = talloc_vasprintf_append(s, fmt, ap);
   va_end(ap);

   return ret;
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
