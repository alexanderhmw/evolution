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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "e-cal-config.h"

#define E_CAL_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_CONFIG, ECalConfigPrivate))

struct _ECalConfigPrivate {
	guint source_changed_id;
};

G_DEFINE_TYPE (ECalConfig, e_cal_config, E_TYPE_CONFIG)

static void
ecp_target_free (EConfig *ec,
                 EConfigTarget *t)
{
	ECalConfigPrivate *p = E_CAL_CONFIG (ec)->priv;

	if (ec->target == t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;

			if (p->source_changed_id) {
				g_signal_handler_disconnect (
					s->source, p->source_changed_id);
				p->source_changed_id = 0;
			}
			break; }
		case EC_CONFIG_TARGET_PREFS: {
			break; }
		}
	}

	switch (t->type) {
	case EC_CONFIG_TARGET_SOURCE: {
		ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;
		if (s->source)
			g_object_unref (s->source);
		break; }
	case EC_CONFIG_TARGET_PREFS: {
		ECalConfigTargetPrefs *s = (ECalConfigTargetPrefs *) t;
		if (s->settings)
			g_object_unref (s->settings);
		break; }
	}

	((EConfigClass *) e_cal_config_parent_class)->target_free (ec, t);
}

static void
ecp_source_changed (ESource *source,
                    EConfig *ec)
{
	e_config_target_changed (ec, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
ecp_set_target (EConfig *ec,
                EConfigTarget *t)
{
	ECalConfigPrivate *p = E_CAL_CONFIG_GET_PRIVATE (ec);

	((EConfigClass *) e_cal_config_parent_class)->set_target (ec, t);

	if (t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;

			p->source_changed_id = g_signal_connect (
				s->source, "changed",
				G_CALLBACK (ecp_source_changed), ec);
			break; }
		case EC_CONFIG_TARGET_PREFS: {
			/* ECalConfigTargetPrefs *s = (ECalConfigTargetPrefs *)t; */
			break; }
		}
	}
}

static void
e_cal_config_class_init (ECalConfigClass *class)
{
	EConfigClass *config_class;

	g_type_class_add_private (class, sizeof (ECalConfigPrivate));

	config_class = E_CONFIG_CLASS (class);
	config_class->set_target = ecp_set_target;
	config_class->target_free = ecp_target_free;
}

static void
e_cal_config_init (ECalConfig *cfg)
{
	cfg->priv = E_CAL_CONFIG_GET_PRIVATE (cfg);
}

ECalConfig *
e_cal_config_new (gint type,
                  const gchar *menuid)
{
	ECalConfig *ecp = g_object_new (e_cal_config_get_type (), NULL);
	e_config_construct (&ecp->config, type, menuid);
	return ecp;
}

ECalConfigTargetSource *
e_cal_config_target_new_source (ECalConfig *ecp,
                                ESource *source)
{
	ECalConfigTargetSource *t;

	t = e_config_target_new (
		&ecp->config, EC_CONFIG_TARGET_SOURCE, sizeof (*t));

	t->source = g_object_ref (source);

	return t;
}

ECalConfigTargetPrefs *
e_cal_config_target_new_prefs (ECalConfig *ecp)
{
	ECalConfigTargetPrefs *t;

	t = e_config_target_new (
		&ecp->config, EC_CONFIG_TARGET_PREFS, sizeof (*t));

	t->settings = g_settings_new ("org.gnome.evolution.calendar");

	return t;
}
