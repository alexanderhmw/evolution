/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *   Peter Williams <peterw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/* mail-folder-cache.h: Stores information about open folders */

#ifndef _MAIL_FOLDER_CACHE_H
#define _MAIL_FOLDER_CACHE_H

#include <glib-object.h>
#include <camel/camel-store.h>

G_BEGIN_DECLS

/* Stores information about open folders */

#define MAIL_TYPE_FOLDER_CACHE            mail_folder_cache_get_type()
#define MAIL_FOLDER_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCache))
#define MAIL_FOLDER_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MAIL_TYPE_FOLDER_CACHE, MailFolderCacheClass))
#define MAIL_IS_FOLDER_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAIL_TYPE_FOLDER_CACHE))
#define MAIL_IS_FOLDER_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MAIL_TYPE_FOLDER_CACHE))
#define MAIL_FOLDER_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCacheClass))

typedef struct _MailFolderCache MailFolderCache;
typedef struct _MailFolderCacheClass MailFolderCacheClass;
typedef struct _MailFolderCachePrivate MailFolderCachePrivate;

struct _MailFolderCache
{
	GObject parent;

	MailFolderCachePrivate *priv;
};

struct _MailFolderCacheClass
{
	GObjectClass parent_class;
};

GType mail_folder_cache_get_type (void) G_GNUC_CONST;

MailFolderCache *mail_folder_cache_new (void);

MailFolderCache *mail_folder_cache_get_default (void);

typedef gboolean (*NoteDoneFunc)(CamelStore *store, CamelFolderInfo *info, gpointer data);
/* Add a store whose folders should appear in the shell
   The folders are scanned from the store, and/or added at
   runtime via the folder_created event.
   The 'done' function returns if we can free folder info. */
void mail_folder_cache_note_store (MailFolderCache *self,
				 CamelStore *store, CamelOperation *op,
				 NoteDoneFunc func, gpointer data);

/* de-note a store */
void mail_folder_cache_note_store_remove (MailFolderCache *self, CamelStore *store);

/* When a folder has been opened, notify it for watching.
   The folder must have already been created on the store (which has already been noted)
   before the folder can be opened
 */
void mail_folder_cache_note_folder (MailFolderCache *self, CamelFolder *folder);

/* Returns true if a folder is available (yet), and also sets *folderp (if supplied)
   to a (referenced) copy of the folder if it has already been opened */
gboolean mail_folder_cache_get_folder_from_uri (MailFolderCache *self, const gchar *uri, CamelFolder **folderp);
gboolean mail_folder_cache_get_folder_info_flags (MailFolderCache *self, CamelFolder *folder, gint *flags);

G_END_DECLS
#endif
