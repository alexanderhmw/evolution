/* Evolution calendar - Scheduling page
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <glade/glade.h>
#include <gal/e-table/e-cell-combo.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include <widgets/misc/e-dateedit.h>
#include <e-util/e-dialog-widgets.h>
#include "../calendar-config.h"
#include "../e-meeting-time-sel.h"
#include "../itip-utils.h"
#include "comp-editor-util.h"
#include "e-delegate-dialog.h"
#include "schedule-page.h"



/* Private part of the SchedulePage structure */
struct _SchedulePagePrivate {	
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	/* Model */
	EMeetingModel *model;
	
	/* Selector */
	EMeetingTimeSelector *sel;
	
	/* The timezone we use. Note that we use the same timezone for the
	   start and end date. We convert the end date if it is passed in in
	   another timezone. */
	icaltimezone *zone;

	gboolean updating;
};



static void schedule_page_class_init (SchedulePageClass *class);
static void schedule_page_init (SchedulePage *spage);
static void schedule_page_destroy (GtkObject *object);

static GtkWidget *schedule_page_get_widget (CompEditorPage *page);
static void schedule_page_focus_main_widget (CompEditorPage *page);
static void schedule_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static gboolean schedule_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void schedule_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static void times_changed_cb (GtkWidget *widget, gpointer data);

static CompEditorPageClass *parent_class = NULL;



/**
 * schedule_page_get_type:
 * 
 * Registers the #SchedulePage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #SchedulePage class.
 **/
GtkType
schedule_page_get_type (void)
{
	static GtkType schedule_page_type;

	if (!schedule_page_type) {
		static const GtkTypeInfo schedule_page_info = {
			"SchedulePage",
			sizeof (SchedulePage),
			sizeof (SchedulePageClass),
			(GtkClassInitFunc) schedule_page_class_init,
			(GtkObjectInitFunc) schedule_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		schedule_page_type = 
			gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
					 &schedule_page_info);
	}

	return schedule_page_type;
}

/* Class initialization function for the schedule page */
static void
schedule_page_class_init (SchedulePageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = schedule_page_get_widget;
	editor_page_class->focus_main_widget = schedule_page_focus_main_widget;
	editor_page_class->fill_widgets = schedule_page_fill_widgets;
	editor_page_class->fill_component = schedule_page_fill_component;
	editor_page_class->set_summary = NULL;
	editor_page_class->set_dates = schedule_page_set_dates;

	object_class->destroy = schedule_page_destroy;
}

/* Object initialization function for the schedule page */
static void
schedule_page_init (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	priv = g_new0 (SchedulePagePrivate, 1);
	spage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;

	priv->zone = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the schedule page */
static void
schedule_page_destroy (GtkObject *object)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SCHEDULE_PAGE (object));

	spage = SCHEDULE_PAGE (object);
	priv = spage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	gtk_object_unref (GTK_OBJECT (priv->model));

	g_free (priv);
	spage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the schedule page */
static GtkWidget *
schedule_page_get_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	return priv->main;
}

/* focus_main_widget handler for the schedule page */
static void
schedule_page_focus_main_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	gtk_widget_grab_focus (GTK_WIDGET (priv->sel));
}

/* Set date/time */
static void
update_time (SchedulePage *spage, CalComponentDateTime *start_date, CalComponentDateTime *end_date) 
{
	SchedulePagePrivate *priv;
	struct icaltimetype start_tt, end_tt;
	icaltimezone *start_zone = NULL, *end_zone = NULL;
	CalClientGetStatus status;
	gboolean all_day;

	priv = spage->priv;
	
	/* Note that if we are creating a new event, the timezones may not be
	   on the server, so we try to get the builtin timezone with the TZID
	   first. */
	start_zone = icaltimezone_get_builtin_timezone_from_tzid (start_date->tzid);
	if (!start_zone) {
		status = cal_client_get_timezone (COMP_EDITOR_PAGE (spage)->client,
						  start_date->tzid,
						  &start_zone);
		/* FIXME: Handle error better. */
		if (status != CAL_CLIENT_GET_SUCCESS)
			g_warning ("Couldn't get timezone from server: %s",
				   start_date->tzid ? start_date->tzid : "");
	}

	end_zone = icaltimezone_get_builtin_timezone_from_tzid (end_date->tzid);
	if (!end_zone) {
		status = cal_client_get_timezone (COMP_EDITOR_PAGE (spage)->client,
						  end_date->tzid,
						  &end_zone);
		/* FIXME: Handle error better. */
		if (status != CAL_CLIENT_GET_SUCCESS)
		  g_warning ("Couldn't get timezone from server: %s",
			     end_date->tzid ? end_date->tzid : "");
	}

	start_tt = *start_date->value;
	end_tt = *end_date->value;
	
	/* If the end zone is not the same as the start zone, we convert it. */
	priv->zone = start_zone;
	if (start_zone != end_zone) {
		icaltimezone_convert_time (&end_tt, end_zone, start_zone);
	}
	e_meeting_model_set_zone (priv->model, priv->zone);
	
	all_day = (start_tt.is_date && end_tt.is_date) ? TRUE : FALSE;

	/* For All Day Events, if DTEND is after DTSTART, we subtract 1 day
	   from it. */
	if (all_day) {
		if (icaltime_compare_date_only (end_tt, start_tt) > 0) {
			icaltime_adjust (&end_tt, -1, 0, 0, 0);
		}
	}

	e_meeting_time_selector_set_all_day (priv->sel, all_day);
	
	e_date_edit_set_date (E_DATE_EDIT (priv->sel->start_date_edit), start_tt.year,
			      start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->sel->start_date_edit),
				     start_tt.hour, start_tt.minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->sel->end_date_edit), end_tt.year,
			      end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->sel->end_date_edit),
				     end_tt.hour, end_tt.minute);

}

		
/* Fills the widgets with default values */
static void
clear_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv;
	
	priv = spage->priv;
}

/* fill_widgets handler for the schedule page */
static void
schedule_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	CalComponentDateTime start_date, end_date;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	priv->updating = TRUE;

	/* Clean the screen */
	clear_widgets (spage);

	/* Start and end times */
	cal_component_get_dtstart (comp, &start_date);
	cal_component_get_dtend (comp, &end_date);
	update_time (spage, &start_date, &end_date);
	
	cal_component_free_datetime (&start_date);
	cal_component_free_datetime (&end_date);
	
	priv->updating = FALSE;
}

/* fill_component handler for the schedule page */
static gboolean
schedule_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	
	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	return TRUE;
}

static void
schedule_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	
	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	priv->updating = TRUE;
	
	update_time (spage, dates->start, dates->end);

	priv->updating = FALSE;
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (SchedulePage *spage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (spage);
	SchedulePagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = spage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("schedule-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (GTK_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

#undef GW

	return TRUE;
}

static gboolean
init_widgets (SchedulePage *spage) 
{
	SchedulePagePrivate *priv;

	priv = spage->priv;

	gtk_signal_connect (GTK_OBJECT (priv->sel), 
			    "changed", times_changed_cb, spage);

	return TRUE;
	
}



/**
 * schedule_page_construct:
 * @spage: An schedule page.
 * 
 * Constructs an schedule page by loading its Glade data.
 * 
 * Return value: The same object as @spage, or NULL if the widgets could not 
 * be created.
 **/
SchedulePage *
schedule_page_construct (SchedulePage *spage, EMeetingModel *emm)
{
	SchedulePagePrivate *priv;
	
	priv = spage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/schedule-page.glade", NULL, NULL);
	if (!priv->xml) {
		g_message ("schedule_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (spage)) {
		g_message ("schedule_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	/* Model */
	gtk_object_ref (GTK_OBJECT (emm));
	priv->model = emm;
	
	/* Selector */
	priv->sel = E_MEETING_TIME_SELECTOR (e_meeting_time_selector_new (emm));
	e_meeting_time_selector_set_working_hours (priv->sel,
						   calendar_config_get_day_start_hour (),
						   calendar_config_get_day_start_minute (),
						   calendar_config_get_day_end_hour (),
						   calendar_config_get_day_end_minute ());
	gtk_widget_show (GTK_WIDGET (priv->sel));
	gtk_box_pack_start (GTK_BOX (priv->main), GTK_WIDGET (priv->sel), TRUE, TRUE, 2);

	if (!init_widgets (spage)) {
		g_message ("schedule_page_construct(): " 
			   "Could not initialize the widgets!");
		return NULL;
	}

	return spage;
}

/**
 * schedule_page_new:
 * 
 * Creates a new schedule page.
 * 
 * Return value: A newly-created schedule page, or NULL if the page could
 * not be created.
 **/
SchedulePage *
schedule_page_new (EMeetingModel *emm)
{
	SchedulePage *spage;

	spage = gtk_type_new (TYPE_SCHEDULE_PAGE);
	if (!schedule_page_construct (spage, emm)) {
		gtk_object_unref (GTK_OBJECT (spage));
		return NULL;
	}

	return spage;
}

static void
times_changed_cb (GtkWidget *widget, gpointer data)
{
	SchedulePage *spage = data;
	SchedulePagePrivate *priv;
	CompEditorPageDates dates;
	CalComponentDateTime start_dt, end_dt;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();
	
	priv = spage->priv;

	if (priv->updating)
		return;
	
	e_date_edit_get_date (E_DATE_EDIT (priv->sel->start_date_edit),
			      &start_tt.year,
			      &start_tt.month,
			      &start_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->sel->start_date_edit),
				     &start_tt.hour,
				     &start_tt.minute);
	e_date_edit_get_date (E_DATE_EDIT (priv->sel->end_date_edit),
			      &end_tt.year,
			      &end_tt.month,
			      &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->sel->end_date_edit),
				     &end_tt.hour,
				     &end_tt.minute);

	start_dt.value = &start_tt;
	end_dt.value = &end_tt;

	if (e_date_edit_get_show_time (E_DATE_EDIT (priv->sel->start_date_edit))) {
		/* We set the start and end to the same timezone. */
		start_dt.tzid = icaltimezone_get_tzid (priv->zone);
		end_dt.tzid = start_dt.tzid;
	} else {
		/* For All-Day Events, we set the timezone to NULL, and add
		   1 day to DTEND. */
		start_dt.value->is_date = TRUE;
		start_dt.tzid = NULL;
		end_dt.value->is_date = TRUE;
		icaltime_adjust (&end_tt, 1, 0, 0, 0);
		end_dt.tzid = NULL;
	}

	dates.start = &start_dt;
	dates.end = &end_dt;	
	dates.due = NULL;
	dates.complete = NULL;

	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (spage),
					       &dates);
}
