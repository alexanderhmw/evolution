/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* Simple macros to set up storage allocation and basic types for libart
   functions. */

#ifndef __ART_MISC_H__
#define __ART_MISC_H__

#include <glib.h>
#include <stdlib.h> /* for malloc, etc. */

#ifdef __cplusplus
extern "C" {
#endif
gpointer art_alloc(gsize size);
void art_free(gpointer ptr);
gpointer art_realloc(gpointer ptr, gsize size);
#ifdef __cplusplus
}
#endif /* __cplusplus */

/* These aren't, strictly speaking, configuration macros, but they're
   damn handy to have around, and may be worth playing with for
   debugging. */
#define art_new(type, n) ((type *)art_alloc ((n) * sizeof(type)))

#define art_renew(p, type, n) ((type *)art_realloc (p, (n) * sizeof(type)))

/* This one must be used carefully - in particular, p and max should
   be variables. They can also be pstruct->el lvalues. */
#define art_expand(p, type, max) \
	do { if(max) { p = art_renew (p, type, max <<= 1); } \
	else { max = 1; p = art_new(type, 1); } } while (0)

typedef gint art_boolean;
#define ART_FALSE 0
#define ART_TRUE 1

/* define pi */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif  /*  M_PI  */

#ifndef M_SQRT2
#define M_SQRT2         1.41421356237309504880  /* sqrt(2) */
#endif  /* M_SQRT2 */

/* Provide macros to feature the GCC function attribute.
 */
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4))
#define ART_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#define ART_GNUC_NORETURN                         \
  __attribute__((__noreturn__))
#else   /* !__GNUC__ */
#define ART_GNUC_PRINTF( format_idx, arg_idx )
#define ART_GNUC_NORETURN
#endif  /* !__GNUC__ */

#ifdef __cplusplus
extern "C" {
#endif

typedef guint8 art_u8;
typedef guint16 art_u16;
typedef guint32 art_u32;

void ART_GNUC_NORETURN
art_die (const gchar *fmt, ...) ART_GNUC_PRINTF (1, 2);

void
art_warn (const gchar *fmt, ...) ART_GNUC_PRINTF (1, 2);

#ifdef __cplusplus
}
#endif

#define ART_USE_NEW_INTERSECTOR

#endif /* __ART_MISC_H__ */
