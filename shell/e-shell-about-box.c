/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-about-box.c
 *
 * Copyright (C) 2001, 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-about-box.h"

#include <gal/util/e-util.h>

#include <gtk/gtkeventbox.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#define PARENT_TYPE gtk_event_box_get_type ()
static GtkEventBoxClass *parent_class = NULL;

static const char *text[] = {
	"",
	"Evolution " VERSION,
	"Copyright 1999 - 2002 Ximian, Inc.",
	"",
	N_("Brought to you by"),
	"",
	"Seth Alves",
	"Jacob Berkman",
	"Kevin Breit",
	"Anders Carlsson",
	"Damon Chaplin",
	"Zbigniew Chyla",
	"Clifford R. Conover",
	"Anna Dirks",
	"Bob Doan",
	"Miguel de Icaza",
	"Radek Doulik",
	"Arturo Espinoza",
	"Larry Ewing",
	"Nat Friedman",
	"Alex Graveley",
	"Bertrand Guiheneuf",
	"Heath Harrelson",
	"Iain Holmes",
	"Mike Kestner",
	"Tuomas Kuosmanen",
	"Christopher J. Lahey",
	"Miles Lane",
	"Jason Leach",
	"Timothy Lee",
	"Matthew Loper",
	"Michael MacDonald",
	"Kjartan Maraas",
	"Gerardo Marin",
	"Michael Meeks",
	"Federico Mena",
	"Michael M. Morrison",
	"Rodrigo Moya",
	"Eskil Heyn Olsen",
	"Gediminas Paulauskas",
	"Jesse Pavel",
	"Ettore Perazzoli",
	"JP Rosevear",
	"Jeffrey Stedfast",
        "Jakub Steiner",
	"Russell Steinthal",
	"Peter Teichman",
	"Chris Toshok",
	"Jon Trowbridge",
	"Luis Villa",
	"Aaron Weber",
	"Peter Williams",
	"Dan Winship",
	"Michael Zucchi"
};
#define NUM_TEXT_LINES (sizeof (text) / sizeof (*text))

struct _EShellAboutBoxPrivate {
	GdkPixmap *pixmap;
	GdkPixmap *text_background_pixmap;
	GdkGC *clipped_gc;
	int text_y_offset;
	int timeout_id;
	const gchar **permuted_text;
};


#define ANIMATION_DELAY 40

#define WIDTH  400
#define HEIGHT 200

#define TEXT_Y_OFFSET 57
#define TEXT_X_OFFSET 60
#define TEXT_WIDTH    (WIDTH - 2 * TEXT_X_OFFSET)
#define TEXT_HEIGHT   90

#define IMAGE_PATH  EVOLUTION_IMAGES "/about-box.png"



static void
permute_names (EShellAboutBox *about_box)
{
	EShellAboutBoxPrivate *priv = about_box->priv;
	gint i, j;

	srandom (time (NULL));
	
	for (i = 6; i < NUM_TEXT_LINES-1; ++i) {
		const gchar *tmp;
		j = i + random () % (NUM_TEXT_LINES - i);
		if (i != j) {
			tmp = priv->permuted_text[i];
			priv->permuted_text[i] = priv->permuted_text[j];
			priv->permuted_text[j] = tmp;
		}
	}
}

/* The callback.  */

static int
timeout_callback (void *data)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;
	GdkRectangle redraw_rect;
	GtkWidget *widget;
	GdkFont *font;
	int line_height;
	int first_line;
	int y;
	int i;

	about_box = E_SHELL_ABOUT_BOX (data);
	priv = about_box->priv;

	widget = GTK_WIDGET (about_box);

	font = gtk_style_get_font (widget->style);
	line_height = font->ascent + font->descent;

	if (priv->text_y_offset < TEXT_HEIGHT) {
		y = TEXT_Y_OFFSET + (TEXT_HEIGHT - priv->text_y_offset);
		first_line = 0;
	} else {
		y = TEXT_Y_OFFSET - ((priv->text_y_offset - TEXT_HEIGHT) % line_height);
		first_line = (priv->text_y_offset - TEXT_HEIGHT) / line_height;
	}

	gdk_draw_pixmap (priv->pixmap, priv->clipped_gc, priv->text_background_pixmap,
			 0, 0,
			 TEXT_X_OFFSET, TEXT_Y_OFFSET, TEXT_WIDTH, TEXT_HEIGHT);

	for (i = 0; i < TEXT_HEIGHT / line_height + 3; i ++) {
		const char *line;
		int x;

		if (first_line + i >= NUM_TEXT_LINES)
			break;

		if (*priv->permuted_text[first_line + i] == '\0')
			line = "";
		else
			line = _(priv->permuted_text[first_line + i]);

		x = TEXT_X_OFFSET + (TEXT_WIDTH - gdk_string_width (font, line)) / 2;

		gdk_draw_string (priv->pixmap, font, priv->clipped_gc, x, y, line);

		y += line_height;
	}

	redraw_rect.x      = TEXT_X_OFFSET;
	redraw_rect.y      = TEXT_Y_OFFSET;
	redraw_rect.width  = TEXT_WIDTH;
	redraw_rect.height = TEXT_HEIGHT;
	gtk_widget_draw (widget, &redraw_rect);

	priv->text_y_offset ++;
	if (priv->text_y_offset > line_height * NUM_TEXT_LINES + TEXT_HEIGHT) {
		priv->text_y_offset = 0;
		permute_names (about_box);
	}

	return TRUE;
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;

	about_box = E_SHELL_ABOUT_BOX (object);
	priv = about_box->priv;

	if (priv->pixmap != NULL)
		gdk_pixmap_unref (priv->pixmap);

	if (priv->text_background_pixmap != NULL)
		gdk_pixmap_unref (priv->text_background_pixmap);

	if (priv->clipped_gc != NULL)
		gdk_gc_unref (priv->clipped_gc);

	if (priv->timeout_id != -1)
		g_source_remove (priv->timeout_id);

	g_free (priv->permuted_text);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* GtkWidget methods.  */

static void
impl_size_request (GtkWidget *widget,
		   GtkRequisition *requisition)
{
	requisition->width = WIDTH;
	requisition->height = HEIGHT;
}

static void
impl_realize (GtkWidget *widget)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;
	GdkPixbuf *background_pixbuf;
	GdkRectangle clip_rectangle;

	(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	about_box = E_SHELL_ABOUT_BOX (widget);
	priv = about_box->priv;

	background_pixbuf = gdk_pixbuf_new_from_file (IMAGE_PATH, NULL);
	g_assert (background_pixbuf != NULL);
	g_assert (gdk_pixbuf_get_width (background_pixbuf) == WIDTH);
	g_assert (gdk_pixbuf_get_height (background_pixbuf) == HEIGHT);

	g_assert (priv->pixmap == NULL);
	priv->pixmap = gdk_pixmap_new (widget->window, WIDTH, HEIGHT, -1);

	gdk_pixbuf_render_to_drawable (background_pixbuf, priv->pixmap, widget->style->black_gc,
				       0, 0, 0, 0, WIDTH, HEIGHT,
				       GDK_RGB_DITHER_MAX, 0, 0);

	g_assert (priv->clipped_gc == NULL);
	priv->clipped_gc = gdk_gc_new (widget->window);
	gdk_gc_copy (priv->clipped_gc, widget->style->black_gc);

	clip_rectangle.x      = TEXT_X_OFFSET;
	clip_rectangle.y      = TEXT_Y_OFFSET;
	clip_rectangle.width  = TEXT_WIDTH;
	clip_rectangle.height = TEXT_HEIGHT;
	gdk_gc_set_clip_rectangle (priv->clipped_gc, & clip_rectangle);

	priv->text_background_pixmap = gdk_pixmap_new (widget->window, clip_rectangle.width, clip_rectangle.height, -1);
	gdk_pixbuf_render_to_drawable (background_pixbuf, priv->text_background_pixmap, widget->style->black_gc,
				       TEXT_X_OFFSET, TEXT_Y_OFFSET,
				       0, 0, TEXT_WIDTH, TEXT_HEIGHT,
				       GDK_RGB_DITHER_MAX, 0, 0);

	g_assert (priv->timeout_id == -1);
	priv->timeout_id = g_timeout_add (ANIMATION_DELAY, timeout_callback, about_box);

	gdk_pixbuf_unref (background_pixbuf);
}

static void
impl_unrealize (GtkWidget *widget)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;

	about_box = E_SHELL_ABOUT_BOX (widget);
	priv = about_box->priv;

	(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

	g_assert (priv->clipped_gc != NULL);
	gdk_gc_unref (priv->clipped_gc);
	priv->clipped_gc = NULL;

	g_assert (priv->pixmap != NULL);
	gdk_pixmap_unref (priv->pixmap);
	priv->pixmap = NULL;

	if (priv->timeout_id != -1) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = -1;
	}
}

static int
impl_expose_event (GtkWidget *widget,
		   GdkEventExpose *event)
{
	EShellAboutBoxPrivate *priv;

	if (! GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	priv = E_SHELL_ABOUT_BOX (widget)->priv;

	gdk_draw_pixmap (widget->window, widget->style->black_gc,
			 priv->pixmap,
			 event->area.x, event->area.y,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

	return TRUE;
}


static void
class_init (GObjectClass *object_class)
{
	GtkWidgetClass *widget_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->finalize = impl_finalize;

	widget_class = GTK_WIDGET_CLASS (object_class);
	widget_class->size_request = impl_size_request;
	widget_class->realize      = impl_realize;
	widget_class->unrealize    = impl_unrealize;
	widget_class->expose_event = impl_expose_event;
}

static void
init (EShellAboutBox *shell_about_box)
{
	EShellAboutBoxPrivate *priv;
	gint i;

	priv = g_new (EShellAboutBoxPrivate, 1);
	priv->pixmap                 = NULL;
	priv->text_background_pixmap = NULL;
	priv->clipped_gc             = NULL;
	priv->timeout_id             = -1;
	priv->text_y_offset          = 0;

	priv->permuted_text = g_new (const gchar *, NUM_TEXT_LINES);
	for (i = 0; i < NUM_TEXT_LINES; ++i) {
		priv->permuted_text[i] = text[i];
	}

	shell_about_box->priv = priv;

	permute_names (shell_about_box);
}


GtkWidget *
e_shell_about_box_new (void)
{
	EShellAboutBox *about_box;

	about_box = g_object_new (e_shell_about_box_get_type (), NULL);

	return GTK_WIDGET (about_box);
}


E_MAKE_TYPE (e_shell_about_box, "EShellAboutBox", EShellAboutBox, class_init, init, GTK_TYPE_EVENT_BOX)
