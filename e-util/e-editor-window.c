/*
 * e-editor-window.h
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
 */

#ifndef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-window.h"

G_DEFINE_TYPE (
	EEditorWindow,
	e_editor_window,
	GTK_TYPE_WINDOW)

struct _EEditorWindowPrivate {
	EEditor *editor;
	GtkGrid *main_layout;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;

	gint editor_row;
};

enum {
	PROP_0,
	PROP_EDITOR
};

static void
editor_window_get_property (GObject *object,
			    guint property_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_set_object (
				value, E_EDITOR_WINDOW (object)->priv->editor);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_window_class_init (EEditorWindowClass *klass)
{
	GObjectClass *object_class;

	e_editor_window_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorWindowPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = editor_window_get_property;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
		        NULL,
		       	NULL,
		        E_TYPE_EDITOR,
		        G_PARAM_READABLE));
}

static void
e_editor_window_init (EEditorWindow *window)
{
	EEditorWindowPrivate *priv;
	GtkWidget *widget;

	window->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		window, E_TYPE_EDITOR_WINDOW, EEditorWindowPrivate);

	priv = window->priv;
	priv->editor = E_EDITOR (e_editor_new ());

	priv->main_layout = GTK_GRID (gtk_grid_new ());
	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (priv->main_layout), GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (priv->main_layout));
	gtk_widget_show (GTK_WIDGET (priv->main_layout));

	widget = e_editor_get_managed_widget (priv->editor, "/main-menu");
	gtk_grid_attach (priv->main_layout, widget, 0, 0, 1, 1);
	gtk_widget_set_hexpand (widget, TRUE);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (priv->editor, "/main-toolbar");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (
		priv->main_layout, widget, 0, 1, 1, 1);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	gtk_widget_set_hexpand (GTK_WIDGET (priv->editor), TRUE);
	gtk_grid_attach (
		priv->main_layout, GTK_WIDGET (priv->editor),
		0, 2, 1, 1);
	gtk_widget_show (GTK_WIDGET (priv->editor));
	priv->editor_row = 2;
}

GtkWidget *
e_editor_window_new (GtkWindowType type)
{
	return g_object_new (
		E_TYPE_EDITOR_WINDOW,
		"type", type,
		NULL);
}

EEditor *
e_editor_window_get_editor (EEditorWindow *window)
{
	g_return_val_if_fail (E_IS_EDITOR_WINDOW (window), NULL);

	return window->priv->editor;
}

void
e_editor_window_pack_above (EEditorWindow *window,
			    GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR_WINDOW (window));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_insert_row (
		window->priv->main_layout, window->priv->editor_row);
	window->priv->editor_row++;

	gtk_grid_attach_next_to (
		window->priv->main_layout, child,
		GTK_WIDGET (window->priv->editor),
		GTK_POS_TOP, 1, 1);
}

void
e_editor_window_pack_below (EEditorWindow *window,
			  GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR_WINDOW (window));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_attach_next_to (
		window->priv->main_layout, child,
		NULL, GTK_POS_BOTTOM, 1, 1);
}