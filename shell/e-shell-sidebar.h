/*
 * e-shell-sidebar.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-sidebar
 * @short_description: the left side of the main window
 * @include: shell/e-shell-sidebar.h
 **/

#ifndef E_SHELL_SIDEBAR_H
#define E_SHELL_SIDEBAR_H

#include <shell/e-shell-common.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_SIDEBAR \
	(e_shell_sidebar_get_type ())
#define E_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_SIDEBAR, EShellSidebar))
#define E_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_SIDEBAR, EShellSidebarClass))
#define E_IS_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_SIDEBAR))
#define E_IS_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SHELL_SIDEBAR))
#define E_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_SIDEBAR, EShellSidebarClass))

G_BEGIN_DECLS

/* Avoid including <e-shell-view.h> */
struct _EShellView;

typedef struct _EShellSidebar EShellSidebar;
typedef struct _EShellSidebarClass EShellSidebarClass;
typedef struct _EShellSidebarPrivate EShellSidebarPrivate;

/**
 * EShellSidebar:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellSidebar {
	GtkBin parent;
	EShellSidebarPrivate *priv;
};

struct _EShellSidebarClass {
	GtkBinClass parent_class;
};

GType		e_shell_sidebar_get_type	(void);
GtkWidget *	e_shell_sidebar_new		(struct _EShellView *shell_view);
struct _EShellView *
		e_shell_sidebar_get_shell_view	(EShellSidebar *shell_sidebar);
const gchar *	e_shell_sidebar_get_primary_text(EShellSidebar *shell_sidebar);
void		e_shell_sidebar_set_primary_text(EShellSidebar *shell_sidebar,
						 const gchar *primary_text);
const gchar *	e_shell_sidebar_get_secondary_text
						(EShellSidebar *shell_sidebar);
void		e_shell_sidebar_set_secondary_text
						(EShellSidebar *shell_sidebar,
						 const gchar *secondary_text);

G_END_DECLS

#endif /* E_SHELL_SIDEBAR_H */
