/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmemory.h
 CREATOR: eric 30 June 1999


 $Id$
 $Locker$

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is icalmemory.h
 The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 1999 The Software Studio. 
 http://www.softwarestudio.org

======================================================================*/

#ifndef ICALMEMORY_H
#define ICALMEMORY_H

#include <sys/types.h> /* for size_t */

/* Tmp buffers are managed by ical. References can be returned to the caller, although the caller will not own the memory. */

void* icalmemory_tmp_buffer(size_t size);
char* icalmemory_tmp_copy(char* str);

/* Add an externally allocated buffer to the ring. */
void  icalmemory_add_tmp_buffer(void*);


/* Free all memory used in the ring */
void icalmemory_free_ring();

/* Non-tmp buffers must be freed. These are mostly wrappers around
 * malloc, etc, but are used so the caller can change the memory
 * allocators in a future version of the library */

void* icalmemory_new_buffer(size_t size);
void* icalmemory_resize_buffer(void* buf, size_t size);
void icalmemory_free_buffer(void* buf);

/* icalmemory_append_string will copy the string 'string' to the
   buffer 'buf' starting at position 'pos', reallocing 'buf' if it is
   too small. 'buf_size' is the size of 'buf' and will be changed if
   'buf' is reallocated. 'pos' will point to the last byte of the new
   string in 'buf', usually a '\0' */

/* THESE ROUTINES CAN NOT BE USED ON TMP BUFFERS. Only use them on
   normally allocated memory, or on buffers created from
   icalmemory_new_buffer, never with buffers created by
   icalmemory_tmp_buffer. If icalmemory_append_string has to resize a
   buffer on the ring, the ring will loose track of it an you will
   have memory problems. */

void icalmemory_append_string(char** buf, char** pos, size_t* buf_size, 
			      char* string);

/*  icalmemory_append_char is similar, but is appends a character instead of a string */
void icalmemory_append_char(char** buf, char** pos, size_t* buf_size, 
			      char ch);

#endif /* !ICALMEMORY_H */



