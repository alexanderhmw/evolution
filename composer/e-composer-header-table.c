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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-composer-header-table.h"

#include <glib/gi18n-lib.h>

#include <shell/e-shell.h>

#include "e-msg-composer.h"
#include "e-composer-private.h"
#include "e-composer-from-header.h"
#include "e-composer-name-header.h"
#include "e-composer-post-header.h"
#include "e-composer-spell-header.h"
#include "e-composer-text-header.h"

#define E_COMPOSER_HEADER_TABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTablePrivate))

#define HEADER_TOOLTIP_TO \
	_("Enter the recipients of the message")
#define HEADER_TOOLTIP_CC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message")
#define HEADER_TOOLTIP_BCC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message without appearing " \
	  "in the recipient list of the message")

struct _EComposerHeaderTablePrivate {
	EComposerHeader *headers[E_COMPOSER_NUM_HEADERS];
	GtkWidget *signature_label;
	GtkWidget *signature_combo_box;
	ENameSelector *name_selector;
	ESourceRegistry *registry;
	EShell *shell;
};

enum {
	PROP_0,
	PROP_DESTINATIONS_BCC,
	PROP_DESTINATIONS_CC,
	PROP_DESTINATIONS_TO,
	PROP_IDENTITY_UID,
	PROP_POST_TO,
	PROP_REGISTRY,
	PROP_REPLY_TO,
	PROP_SHELL,
	PROP_SIGNATURE_COMBO_BOX,
	PROP_SIGNATURE_UID,
	PROP_SUBJECT
};

G_DEFINE_TYPE (
	EComposerHeaderTable,
	e_composer_header_table,
	GTK_TYPE_TABLE)

static void
g_value_set_destinations (GValue *value,
                          EDestination **destinations)
{
	GPtrArray *array;
	gint ii;

	/* Preallocate some reasonable number. */
	array = g_ptr_array_new_full (64, g_object_unref);

	for (ii = 0; destinations[ii] != NULL; ii++) {
		g_ptr_array_add (array, e_destination_copy (destinations[ii]));
	}

	g_value_take_boxed (value, array);
}

static EDestination **
g_value_dup_destinations (const GValue *value)
{
	EDestination **destinations;
	const EDestination *dest;
	GPtrArray *array;
	guint ii;

	array = g_value_get_boxed (value);
	destinations = g_new0 (EDestination *, array->len + 1);

	for (ii = 0; ii < array->len; ii++) {
		dest = g_ptr_array_index (array, ii);
		destinations[ii] = e_destination_copy (dest);
	}

	return destinations;
}

static void
g_value_set_string_list (GValue *value,
                         GList *list)
{
	GPtrArray *array;

	array = g_ptr_array_new_full (g_list_length (list), g_free);

	while (list != NULL) {
		g_ptr_array_add (array, g_strdup (list->data));
		list = list->next;
	}

	g_value_take_boxed (value, array);
}

static GList *
g_value_dup_string_list (const GValue *value)
{
	GPtrArray *array;
	GList *list = NULL;
	gint ii;

	array = g_value_get_boxed (value);

	for (ii = 0; ii < array->len; ii++) {
		const gchar *element = g_ptr_array_index (array, ii);
		list = g_list_prepend (list, g_strdup (element));
	}

	return g_list_reverse (list);
}

static void
composer_header_table_notify_header (EComposerHeader *header,
                                     const gchar *property_name)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (header->input_widget);
	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (parent));
	g_object_notify (G_OBJECT (parent), property_name);
}

static void
composer_header_table_notify_widget (GtkWidget *widget,
                                     const gchar *property_name)
{
	EShell *shell;
	GtkWidget *parent;

	/* FIXME Pass this in somehow. */
	shell = e_shell_get_default ();

	if (e_shell_get_small_screen_mode (shell)) {
		parent = gtk_widget_get_parent (widget);
		parent = g_object_get_data (G_OBJECT (parent), "pdata");
	} else
		parent = gtk_widget_get_parent (widget);
	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (parent));
	g_object_notify (G_OBJECT (parent), property_name);
}

static void
composer_header_table_bind_header (const gchar *property_name,
                                   const gchar *signal_name,
                                   EComposerHeader *header)
{
	/* Propagate the signal as "notify::property_name". */

	g_signal_connect (
		header, signal_name,
		G_CALLBACK (composer_header_table_notify_header),
		(gpointer) property_name);
}

static void
composer_header_table_bind_widget (const gchar *property_name,
                                   const gchar *signal_name,
                                   GtkWidget *widget)
{
	/* Propagate the signal as "notify::property_name". */

	g_signal_connect (
		widget, signal_name,
		G_CALLBACK (composer_header_table_notify_widget),
		(gpointer) property_name);
}

static EDestination **
composer_header_table_update_destinations (EDestination **old_destinations,
                                           const gchar * const *auto_addresses)
{
	CamelAddress *address;
	CamelInternetAddress *inet_address;
	EDestination **new_destinations;
	EDestination *destination;
	GQueue queue = G_QUEUE_INIT;
	guint length;
	gint ii;

	/* Include automatic recipients for the selected account. */

	if (auto_addresses == NULL)
		goto skip_auto;

	inet_address = camel_internet_address_new ();
	address = CAMEL_ADDRESS (inet_address);

	/* XXX Calling camel_address_decode() multiple times on the same
	 *     CamelInternetAddress has a cumulative effect, which isn't
	 *     well documented. */
	for (ii = 0; auto_addresses[ii] != NULL; ii++)
		camel_address_decode (address, auto_addresses[ii]);

	for (ii = 0; ii < camel_address_length (address); ii++) {
		const gchar *name, *email;

		if (!camel_internet_address_get (
			inet_address, ii, &name, &email))
			continue;

		destination = e_destination_new ();
		e_destination_set_auto_recipient (destination, TRUE);

		if (name != NULL)
			e_destination_set_name (destination, name);

		if (email != NULL)
			e_destination_set_email (destination, email);

		g_queue_push_tail (&queue, destination);
	}

	g_object_unref (inet_address);

skip_auto:

	/* Include custom recipients for this message. */

	if (old_destinations == NULL)
		goto skip_custom;

	for (ii = 0; old_destinations[ii] != NULL; ii++) {
		if (e_destination_is_auto_recipient (old_destinations[ii]))
			continue;

		destination = e_destination_copy (old_destinations[ii]);
		g_queue_push_tail (&queue, destination);
	}

skip_custom:

	length = g_queue_get_length (&queue);
	new_destinations = g_new0 (EDestination *, length + 1);

	for (ii = 0; ii < length; ii++)
		new_destinations[ii] = g_queue_pop_head (&queue);

	/* Sanity check. */
	g_warn_if_fail (g_queue_is_empty (&queue));

	return new_destinations;
}

static gboolean
from_header_should_be_visible (EComposerHeaderTable *table)
{
	EShell *shell;
	EComposerHeader *header;
	EComposerHeaderType type;
	GtkComboBox *combo_box;
	GtkTreeModel *tree_model;

	shell = e_composer_header_table_get_shell (table);

	/* Always display From in standard mode. */
	if (!e_shell_get_express_mode (shell))
		return TRUE;

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);

	combo_box = GTK_COMBO_BOX (header->input_widget);
	tree_model = gtk_combo_box_get_model (combo_box);

	return (gtk_tree_model_iter_n_children (tree_model, NULL) > 1);
}

static void
composer_header_table_setup_mail_headers (EComposerHeaderTable *table)
{
	GSettings *settings;
	gint ii;

	settings = g_settings_new ("org.gnome.evolution.mail");

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;
		gboolean sensitive;
		gboolean visible;

		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_BCC:
				key = "composer-show-bcc";
				break;

			case E_COMPOSER_HEADER_CC:
				key = "composer-show-cc";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = "composer-show-reply-to";
				break;

			default:
				key = NULL;
				break;
		}

		if (key != NULL)
			g_settings_unbind (header, "visible");

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				sensitive = TRUE;
				visible = from_header_should_be_visible (table);
				break;

			case E_COMPOSER_HEADER_BCC:
			case E_COMPOSER_HEADER_CC:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_SUBJECT:
			case E_COMPOSER_HEADER_TO:
				sensitive = TRUE;
				visible = TRUE;
				break;

			default:
				sensitive = FALSE;
				visible = FALSE;
				break;
		}

		e_composer_header_set_sensitive (header, sensitive);
		e_composer_header_set_visible (header, visible);

		if (key != NULL)
			g_settings_bind (
				settings, key,
				header, "visible",
				G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);
}

static void
composer_header_table_setup_post_headers (EComposerHeaderTable *table)
{
	GSettings *settings;
	gint ii;

	settings = g_settings_new ("org.gnome.evolution.mail");

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;

		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				key = "composer-show-post-from";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = "composer-show-post-reply-to";
				break;

			default:
				key = NULL;
				break;
		}

		if (key != NULL)
			g_settings_unbind (header, "visible");

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
			case E_COMPOSER_HEADER_POST_TO:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_SUBJECT:
				e_composer_header_set_sensitive (header, TRUE);
				e_composer_header_set_visible (header, TRUE);
				break;

			default:  /* this includes TO, CC and BCC */
				e_composer_header_set_sensitive (header, FALSE);
				e_composer_header_set_visible (header, FALSE);
				break;
		}

		if (key != NULL)
			g_settings_bind (
				settings, key,
				header, "visible",
				G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);
}

static gboolean
composer_header_table_show_post_headers (EComposerHeaderTable *table)
{
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *target_uid;
	gboolean show_post_headers = FALSE;

	registry = e_composer_header_table_get_registry (table);
	target_uid = e_composer_header_table_get_identity_uid (table);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_sources (registry, extension_name);

	/* Look for a mail account referencing this mail identity.
	 * If the mail account's backend name is "nntp", show the
	 * post headers.  Otherwise show the mail headers.
	 *
	 * XXX What if multiple accounts use this identity but only
	 *     one is "nntp"?  Maybe it should be indicated by the
	 *     transport somehow?
	 */
	for (link = list; link != NULL; link = link->next) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *backend_name;
		const gchar *identity_uid;

		extension = e_source_get_extension (source, extension_name);

		backend_name = e_source_backend_get_backend_name (
			E_SOURCE_BACKEND (extension));
		identity_uid = e_source_mail_account_get_identity_uid (
			E_SOURCE_MAIL_ACCOUNT (extension));

		if (g_strcmp0 (identity_uid, target_uid) != 0)
			continue;

		if (g_strcmp0 (backend_name, "nntp") != 0)
			continue;

		show_post_headers = TRUE;
		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return show_post_headers;
}

static void
composer_header_table_from_changed_cb (EComposerHeaderTable *table)
{
	ESource *source = NULL;
	ESource *mail_account = NULL;
	ESourceRegistry *registry;
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;
	EComposerTextHeader *text_header;
	EDestination **old_destinations;
	EDestination **new_destinations;
	const gchar *reply_to = NULL;
	const gchar * const *bcc = NULL;
	const gchar * const *cc = NULL;
	const gchar *uid;

	/* Keep "Post-To" and "Reply-To" synchronized with "From" */

	registry = e_composer_header_table_get_registry (table);
	uid = e_composer_header_table_get_identity_uid (table);

	if (uid != NULL)
		source = e_source_registry_ref_source (registry, uid);

	/* Make sure this is really a mail identity source. */
	if (source != NULL) {
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		if (!e_source_has_extension (source, extension_name)) {
			g_object_unref (source);
			source = NULL;
		}
	}

	if (source != NULL) {
		ESourceMailIdentity *mi;
		ESourceMailComposition *mc;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		mi = e_source_get_extension (source, extension_name);

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		mc = e_source_get_extension (source, extension_name);

		reply_to = e_source_mail_identity_get_reply_to (mi);
		bcc = e_source_mail_composition_get_bcc (mc);
		cc = e_source_mail_composition_get_cc (mc);

		g_object_unref (source);
	}

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);
	e_composer_post_header_set_mail_account (post_header, mail_account);

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);
	e_composer_text_header_set_text (text_header, reply_to);

	/* Update automatic CC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_cc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, cc);
	e_composer_header_table_set_destinations_cc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	/* Update automatic BCC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_bcc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, bcc);
	e_composer_header_table_set_destinations_bcc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	if (composer_header_table_show_post_headers (table))
		composer_header_table_setup_post_headers (table);
	else
		composer_header_table_setup_mail_headers (table);
}

static void
composer_header_table_set_registry (EComposerHeaderTable *table,
                                    ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (table->priv->registry == NULL);

	table->priv->registry = g_object_ref (registry);
}

static void
composer_header_table_set_shell (EComposerHeaderTable *table,
                                 EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (table->priv->shell == NULL);

	table->priv->shell = g_object_ref (shell);
}

static void
composer_header_table_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	EDestination **destinations;
	GList *list;

	switch (property_id) {
		case PROP_DESTINATIONS_BCC:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_bcc (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_CC:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_cc (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_TO:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_to (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_IDENTITY_UID:
			e_composer_header_table_set_identity_uid (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_POST_TO:
			list = g_value_dup_string_list (value);
			e_composer_header_table_set_post_to_list (
				E_COMPOSER_HEADER_TABLE (object), list);
			g_list_foreach (list, (GFunc) g_free, NULL);
			g_list_free (list);
			return;

		case PROP_REGISTRY:
			composer_header_table_set_registry (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_REPLY_TO:
			e_composer_header_table_set_reply_to (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_SHELL:
			composer_header_table_set_shell (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SIGNATURE_UID:
			e_composer_header_table_set_signature_uid (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_SUBJECT:
			e_composer_header_table_set_subject (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_table_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	EDestination **destinations;
	GList *list;

	switch (property_id) {
		case PROP_DESTINATIONS_BCC:
			destinations =
				e_composer_header_table_get_destinations_bcc (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_CC:
			destinations =
				e_composer_header_table_get_destinations_cc (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_TO:
			destinations =
				e_composer_header_table_get_destinations_to (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_IDENTITY_UID:
			g_value_set_string (
				value,
				e_composer_header_table_get_identity_uid (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_POST_TO:
			list = e_composer_header_table_get_post_to (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_string_list (value, list);
			g_list_foreach (list, (GFunc) g_free, NULL);
			g_list_free (list);
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_composer_header_table_get_registry (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_REPLY_TO:
			g_value_set_string (
				value,
				e_composer_header_table_get_reply_to (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SHELL:
			g_value_set_object (
				value,
				e_composer_header_table_get_shell (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE_COMBO_BOX:
			g_value_set_object (
				value,
				e_composer_header_table_get_signature_combo_box (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE_UID:
			g_value_set_string (
				value,
				e_composer_header_table_get_signature_uid (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SUBJECT:
			g_value_set_string (
				value,
				e_composer_header_table_get_subject (
				E_COMPOSER_HEADER_TABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_table_dispose (GObject *object)
{
	EComposerHeaderTablePrivate *priv;
	gint ii;

	priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (object);

	for (ii = 0; ii < G_N_ELEMENTS (priv->headers); ii++) {
		if (priv->headers[ii] != NULL) {
			g_object_unref (priv->headers[ii]);
			priv->headers[ii] = NULL;
		}
	}

	if (priv->signature_combo_box != NULL) {
		g_object_unref (priv->signature_combo_box);
		priv->signature_combo_box = NULL;
	}

	if (priv->name_selector != NULL) {
		e_name_selector_cancel_loading (priv->name_selector);
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->shell != NULL) {
		g_object_unref (priv->shell);
		priv->shell = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_header_table_parent_class)->dispose (object);
}

static void
composer_header_table_constructed (GObject *object)
{
	EComposerHeaderTable *table;
	ENameSelector *name_selector;
	ESourceRegistry *registry;
	EComposerHeader *header;
	GtkWidget *widget;
	EShell *shell;
	guint ii;
	gint row_padding;
	gboolean small_screen_mode;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_header_table_parent_class)->
		constructed (object);

	table = E_COMPOSER_HEADER_TABLE (object);
	shell = e_composer_header_table_get_shell (table);
	registry = e_composer_header_table_get_registry (table);

	small_screen_mode = e_shell_get_small_screen_mode (shell);

	name_selector = e_name_selector_new (registry);
	table->priv->name_selector = name_selector;

	header = e_composer_from_header_new (registry, _("Fr_om:"));
	composer_header_table_bind_header ("identity-uid", "changed", header);
	g_signal_connect_swapped (
		header, "changed", G_CALLBACK (
		composer_header_table_from_changed_cb), table);
	table->priv->headers[E_COMPOSER_HEADER_FROM] = header;

	header = e_composer_text_header_new_label (registry, _("_Reply-To:"));
	composer_header_table_bind_header ("reply-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_REPLY_TO] = header;

	header = e_composer_name_header_new (
		registry, _("_To:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_TO);
	composer_header_table_bind_header ("destinations-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_TO] = header;

	header = e_composer_name_header_new (
		registry, _("_Cc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_CC);
	composer_header_table_bind_header ("destinations-cc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_CC] = header;

	header = e_composer_name_header_new (
		registry, _("_Bcc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_BCC);
	composer_header_table_bind_header ("destinations-bcc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_BCC] = header;

	header = e_composer_post_header_new (registry, _("_Post To:"));
	composer_header_table_bind_header ("post-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_POST_TO] = header;

	header = e_composer_spell_header_new_label (registry, _("S_ubject:"));
	composer_header_table_bind_header ("subject", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_SUBJECT] = header;

	widget = e_mail_signature_combo_box_new (registry);
	composer_header_table_bind_widget ("signature-uid", "changed", widget);
	table->priv->signature_combo_box = g_object_ref_sink (widget);

	widget = gtk_label_new_with_mnemonic (_("Si_gnature:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), table->priv->signature_combo_box);
	table->priv->signature_label = g_object_ref_sink (widget);

	/* Use "ypadding" instead of "row-spacing" because some rows may
	 * be invisible and we don't want spacing around them. */

	/* For small screens, pack the table's rows closely together. */
	row_padding = small_screen_mode ? 0 : 3;

	for (ii = 0; ii < G_N_ELEMENTS (table->priv->headers); ii++) {
		gtk_table_attach (
			GTK_TABLE (object),
			table->priv->headers[ii]->title_widget, 0, 1,
			ii, ii + 1, GTK_FILL, GTK_FILL, 0, row_padding);
		gtk_table_attach (
			GTK_TABLE (object),
			table->priv->headers[ii]->input_widget, 1, 4,
			ii, ii + 1, GTK_FILL | GTK_EXPAND, 0, 0, row_padding);
	}

	ii = E_COMPOSER_HEADER_FROM;

	/* Leave room in the "From" row for signature stuff. */
	gtk_container_child_set (
		GTK_CONTAINER (object),
		table->priv->headers[ii]->input_widget,
		"right-attach", 2, NULL);

	g_object_bind_property (
		table->priv->headers[ii]->input_widget, "visible",
		table->priv->signature_label, "visible",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		table->priv->headers[ii]->input_widget, "visible",
		table->priv->signature_combo_box, "visible",
		G_BINDING_SYNC_CREATE);

	/* Now add the signature stuff. */
	if (!small_screen_mode) {
		gtk_table_attach (
			GTK_TABLE (object),
			table->priv->signature_label,
			2, 3, ii, ii + 1, 0, 0, 0, row_padding);
		gtk_table_attach (
			GTK_TABLE (object),
			table->priv->signature_combo_box,
			3, 4, ii, ii + 1, 0, 0, 0, row_padding);
	} else {
		GtkWidget *box = gtk_hbox_new (FALSE, 0);

		gtk_box_pack_start (
			GTK_BOX (box),
			table->priv->signature_label,
			FALSE, FALSE, 4);
		gtk_box_pack_end (
			GTK_BOX (box),
			table->priv->signature_combo_box,
			TRUE, TRUE, 0);
		g_object_set_data (G_OBJECT (box), "pdata", object);
		gtk_table_attach (
			GTK_TABLE (object), box,
			3, 4, ii, ii + 1, GTK_FILL, 0, 0, row_padding);
		gtk_widget_hide (box);
	}

	/* Initialize the headers. */
	composer_header_table_from_changed_cb (table);
}

static void
e_composer_header_table_class_init (EComposerHeaderTableClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EComposerHeaderTablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_header_table_set_property;
	object_class->get_property = composer_header_table_get_property;
	object_class->dispose = composer_header_table_dispose;
	object_class->constructed = composer_header_table_constructed;

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_BCC,
		g_param_spec_boxed (
			"destinations-bcc",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_CC,
		g_param_spec_boxed (
			"destinations-cc",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_TO,
		g_param_spec_boxed (
			"destinations-to",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_UID,
		g_param_spec_string (
			"identity-uid",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_POST_TO,
		g_param_spec_boxed (
			"post-to",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			NULL,
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REPLY_TO,
		g_param_spec_string (
			"reply-to",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			NULL,
			NULL,
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_COMBO_BOX,
		g_param_spec_string (
			"signature-combo-box",
			NULL,
			NULL,
			NULL,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_UID,
		g_param_spec_string (
			"signature-uid",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SUBJECT,
		g_param_spec_string (
			"subject",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
composer_header_table_realize_cb (EComposerHeaderTable *table)
{
	g_return_if_fail (table != NULL);
	g_return_if_fail (table->priv != NULL);

	g_signal_handlers_disconnect_by_func (
		table, composer_header_table_realize_cb, NULL);

	e_name_selector_load_books (table->priv->name_selector);
}

static void
e_composer_header_table_init (EComposerHeaderTable *table)
{
	gint rows;

	table->priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (table);

	rows = G_N_ELEMENTS (table->priv->headers);
	gtk_table_resize (GTK_TABLE (table), rows, 4);
	gtk_table_set_row_spacings (GTK_TABLE (table), 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	/* postpone name_selector loading, do that only when really needed */
	g_signal_connect (
		table, "realize",
		G_CALLBACK (composer_header_table_realize_cb), NULL);
}

GtkWidget *
e_composer_header_table_new (EShell *shell,
                             ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_HEADER_TABLE,
		"shell", shell, "registry", registry, NULL);
}

EShell *
e_composer_header_table_get_shell (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return table->priv->shell;
}

ESourceRegistry *
e_composer_header_table_get_registry (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return table->priv->registry;
}

EComposerHeader *
e_composer_header_table_get_header (EComposerHeaderTable *table,
                                    EComposerHeaderType type)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);
	g_return_val_if_fail (type < E_COMPOSER_NUM_HEADERS, NULL);

	return table->priv->headers[type];
}

EMailSignatureComboBox *
e_composer_header_table_get_signature_combo_box (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return E_MAIL_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
}

EDestination **
e_composer_header_table_get_destinations (EComposerHeaderTable *table)
{
	EDestination **destinations;
	EDestination **to, **cc, **bcc;
	gint total, n_to, n_cc, n_bcc;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	to = e_composer_header_table_get_destinations_to (table);
	for (n_to = 0; to != NULL && to[n_to] != NULL; n_to++);

	cc = e_composer_header_table_get_destinations_cc (table);
	for (n_cc = 0; cc != NULL && cc[n_cc] != NULL; n_cc++);

	bcc = e_composer_header_table_get_destinations_bcc (table);
	for (n_bcc = 0; bcc != NULL && bcc[n_bcc] != NULL; n_bcc++);

	total = n_to + n_cc + n_bcc;
	destinations = g_new0 (EDestination *, total + 1);

	while (n_bcc > 0 && total > 0)
		destinations[--total] = g_object_ref (bcc[--n_bcc]);

	while (n_cc > 0 && total > 0)
		destinations[--total] = g_object_ref (cc[--n_cc]);

	while (n_to > 0 && total > 0)
		destinations[--total] = g_object_ref (to[--n_to]);

	/* Counters should all be zero now. */
	g_assert (total == 0 && n_to == 0 && n_cc == 0 && n_bcc == 0);

	e_destination_freev (to);
	e_destination_freev (cc);
	e_destination_freev (bcc);

	return destinations;
}

EDestination **
e_composer_header_table_get_destinations_bcc (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_bcc (EComposerHeaderTable *table,
                                              EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_composer_header_table_set_destinations_bcc (EComposerHeaderTable *table,
                                              EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_cc (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_cc (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_composer_header_table_set_destinations_cc (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_to (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);
}

void
e_composer_header_table_set_destinations_to (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);
}

const gchar *
e_composer_header_table_get_identity_uid (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_active_id (from_header);
}

void
e_composer_header_table_set_identity_uid (EComposerHeaderTable *table,
                                          const gchar *identity_uid)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	e_composer_from_header_set_active_id (from_header, identity_uid);
}

GList *
e_composer_header_table_get_post_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	return e_composer_post_header_get_folders (post_header);
}

void
e_composer_header_table_set_post_to_base (EComposerHeaderTable *table,
                                          const gchar *base_url,
                                          const gchar *folders)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	e_composer_post_header_set_folders_base (post_header, base_url, folders);
}

void
e_composer_header_table_set_post_to_list (EComposerHeaderTable *table,
                                          GList *folders)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	e_composer_post_header_set_folders (post_header, folders);
}

const gchar *
e_composer_header_table_get_reply_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_reply_to (EComposerHeaderTable *table,
                                      const gchar *reply_to)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, reply_to);

	if (reply_to != NULL && *reply_to != '\0')
		e_composer_header_set_visible (header, TRUE);
}

const gchar *
e_composer_header_table_get_signature_uid (EComposerHeaderTable *table)
{
	EMailSignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	return gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
}

void
e_composer_header_table_set_signature_uid (EComposerHeaderTable *table,
                                           const gchar *signature_uid)
{
	EMailSignatureComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), signature_uid);
}

const gchar *
e_composer_header_table_get_subject (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_SUBJECT;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_subject (EComposerHeaderTable *table,
                                     const gchar *subject)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_SUBJECT;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, subject);
}

void
e_composer_header_table_set_header_visible (EComposerHeaderTable *table,
                                            EComposerHeaderType type,
                                            gboolean visible)
{
	EComposerHeader *header;

	header = e_composer_header_table_get_header (table, type);
	e_composer_header_set_visible (header, visible);

	/* Signature widgets track the "From" header. */
	if (type == E_COMPOSER_HEADER_FROM) {
		if (visible) {
			gtk_widget_show (table->priv->signature_label);
			gtk_widget_show (table->priv->signature_combo_box);
		} else {
			gtk_widget_hide (table->priv->signature_label);
			gtk_widget_hide (table->priv->signature_combo_box);
		}
	}
}
