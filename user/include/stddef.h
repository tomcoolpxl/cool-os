/*
 * stddef.h - Standard definitions
 */

#ifndef _STDDEF_H
#define _STDDEF_H

/* Size type (unsigned) */
typedef unsigned long       size_t;

/* Signed size type */
typedef long                ssize_t;

/* Pointer difference type */
typedef long                ptrdiff_t;

/* Null pointer constant */
#define NULL                ((void *)0)

/* Offset of member in structure */
#define offsetof(type, member)  __builtin_offsetof(type, member)

#endif /* _STDDEF_H */
