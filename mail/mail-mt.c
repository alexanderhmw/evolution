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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <libedataserver/e-flag.h>

#include <e-util/e-alert-sink.h>
#include <shell/e-shell-view.h>

#include "mail-mt.h"

/*#define MALLOC_CHECK*/
#define d(x)

/* XXX This is a dirty hack on a dirty hack.  We really need
 *     to rework or get rid of the functions that use this. */
const gchar *shell_builtin_backend = "mail";

static guint mail_msg_seq; /* sequence number of each message */

/* Table of active messages.  Must hold mail_msg_lock to access. */
static GHashTable *mail_msg_active_table;
static GMutex *mail_msg_lock;
static GCond *mail_msg_cond;

static void
mail_msg_cancelled (CamelOperation *operation,
                    gpointer user_data)
{
	mail_msg_cancel (GPOINTER_TO_UINT (user_data));
}

static gboolean
mail_msg_submit (EActivity *activity)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (
		shell, shell_builtin_backend);

	e_shell_backend_add_activity (shell_backend, activity);

	return FALSE;
}

gpointer
mail_msg_new (MailMsgInfo *info)
{
	MailMsg *msg;
	GCancellable *cancellable;

	g_mutex_lock (mail_msg_lock);

	msg = g_slice_alloc0 (info->size);
	msg->info = info;
	msg->ref_count = 1;
	msg->seq = mail_msg_seq++;
	msg->activity = e_activity_new ();

	cancellable = camel_operation_new ();

	e_activity_set_percent (msg->activity, 0.0);
	e_activity_set_cancellable (msg->activity, cancellable);

	g_signal_connect (
		cancellable, "cancelled",
		G_CALLBACK (mail_msg_cancelled),
		GINT_TO_POINTER (msg->seq));

	g_object_unref (cancellable);

	g_hash_table_insert (
		mail_msg_active_table, GINT_TO_POINTER (msg->seq), msg);

	d(printf("New message %p\n", msg));

	g_mutex_unlock (mail_msg_lock);

	return msg;
}

#ifdef MALLOC_CHECK
#include <mcheck.h>

static void
checkmem (gpointer p)
{
	if (p) {
		gint status = mprobe (p);

		switch (status) {
		case MCHECK_HEAD:
			printf("Memory underrun at %p\n", p);
			abort ();
		case MCHECK_TAIL:
			printf("Memory overrun at %p\n", p);
			abort ();
		case MCHECK_FREE:
			printf("Double free %p\n", p);
			abort ();
		}
	}
}
#endif

static gboolean
mail_msg_free (MailMsg *mail_msg)
{
	/* This is an idle callback. */

	if (mail_msg->activity != NULL)
		g_object_unref (mail_msg->activity);

	if (mail_msg->error != NULL)
		g_error_free (mail_msg->error);

	g_slice_free1 (mail_msg->info->size, mail_msg);

	return FALSE;
}

gpointer
mail_msg_ref (gpointer msg)
{
	MailMsg *mail_msg = msg;

	g_return_val_if_fail (mail_msg != NULL, msg);
	g_return_val_if_fail (mail_msg->ref_count > 0, msg);

	g_atomic_int_add (&mail_msg->ref_count, 1);
	return msg;
}

void
mail_msg_unref (gpointer msg)
{
	MailMsg *mail_msg = msg;

	g_return_if_fail (mail_msg != NULL);
	g_return_if_fail (mail_msg->ref_count > 0);

	if (g_atomic_int_add (&mail_msg->ref_count, -1) > 1)
		return;

#ifdef MALLOC_CHECK
	checkmem (mail_msg);
	checkmem (mail_msg->cancel);
	checkmem (mail_msg->priv);
#endif
	d(printf("Free message %p\n", msg));

	if (mail_msg->info->free)
		mail_msg->info->free (mail_msg);

	g_mutex_lock (mail_msg_lock);

	g_hash_table_remove (
		mail_msg_active_table,
		GINT_TO_POINTER (mail_msg->seq));
	g_cond_broadcast (mail_msg_cond);

	g_mutex_unlock (mail_msg_lock);

	/* Destroy the message from an idle callback
	 * so we know we're in the main loop thread. */
	g_idle_add ((GSourceFunc) mail_msg_free, mail_msg);
}

void
mail_msg_check_error (gpointer msg)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window = NULL;
	EShellContent *shell_content;
	MailMsg *m = msg;
	gchar *what;
	GList *list, *iter;

#ifdef MALLOC_CHECK
	checkmem (m);
	checkmem (m->cancel);
	checkmem (m->priv);
#endif

	if (e_activity_handle_cancellation (m->activity, m->error))
		return;

	e_activity_set_state (m->activity, E_ACTIVITY_COMPLETED);

	if (m->error == NULL)
		return;

	/* XXX Hmm, no explanation of why this is needed.  It looks like
	 *     a lame hack and will be removed at some point, if only to
	 *     reintroduce whatever issue made this necessary so we can
	 *     document it the source code this time. */
	if (g_error_matches (m->error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID_UID))
		return;

	shell = e_shell_get_default ();

	/* Find the most recently used EShellWindow. */
	list =  e_shell_get_watched_windows (shell);
	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		if (E_IS_SHELL_WINDOW (iter->data)) {
			shell_window = E_SHELL_WINDOW (iter->data);
			break;
		}
	}

	/* If we can't find an EShellWindow then... well, screw it. */
	if (shell_window == NULL)
		return;

	shell_view = e_shell_window_get_shell_view (
		shell_window, shell_builtin_backend);
	shell_content = e_shell_view_get_shell_content (shell_view);

	if (m->info->desc && (what = m->info->desc (m))) {
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"mail:async-error", what,
			m->error->message, NULL);
		g_free (what);
	} else
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"mail:async-error-nodescribe",
			m->error->message, NULL);
}

void
mail_msg_cancel (guint msgid)
{
	MailMsg *msg;
	GCancellable *cancellable = NULL;

	g_mutex_lock (mail_msg_lock);

	msg = g_hash_table_lookup (
		mail_msg_active_table, GINT_TO_POINTER (msgid));

	/* Hold a reference to the GCancellable so it doesn't finalize
	 * itself on us between unlocking the mutex and cancelling. */
	if (msg != NULL) {
		cancellable = e_activity_get_cancellable (msg->activity);
		if (g_cancellable_is_cancelled (cancellable))
			cancellable = NULL;
		else
			g_object_ref (cancellable);
	}

	g_mutex_unlock (mail_msg_lock);

	if (cancellable != NULL) {
		camel_operation_cancel (CAMEL_OPERATION (cancellable));
		g_object_unref (cancellable);
	}
}

gboolean
mail_msg_active (void)
{
	gboolean active;

	g_mutex_lock (mail_msg_lock);
	active = g_hash_table_size (mail_msg_active_table) > 0;
	g_mutex_unlock (mail_msg_lock);

	return active;
}

/* **************************************** */

static GHookList cancel_hook_list;

GHook *
mail_cancel_hook_add (GHookFunc func, gpointer data)
{
	GHook *hook;

	g_mutex_lock (mail_msg_lock);

	if (!cancel_hook_list.is_setup)
		g_hook_list_init (&cancel_hook_list, sizeof (GHook));

	hook = g_hook_alloc (&cancel_hook_list);
	hook->func = func;
	hook->data = data;

	g_hook_append (&cancel_hook_list, hook);

	g_mutex_unlock (mail_msg_lock);

	return hook;
}

void
mail_cancel_hook_remove (GHook *hook)
{
	g_mutex_lock (mail_msg_lock);

	g_return_if_fail (cancel_hook_list.is_setup);
	g_hook_destroy_link (&cancel_hook_list, hook);

	g_mutex_unlock (mail_msg_lock);
}

void
mail_cancel_all (void)
{
	camel_operation_cancel (NULL);

	g_mutex_lock (mail_msg_lock);

	if (cancel_hook_list.is_setup)
		g_hook_list_invoke (&cancel_hook_list, FALSE);

	g_mutex_unlock (mail_msg_lock);
}

static guint idle_source_id = 0;
G_LOCK_DEFINE_STATIC (idle_source_id);
static GAsyncQueue *main_loop_queue = NULL;
static GAsyncQueue *msg_reply_queue = NULL;
static GThread *main_thread = NULL;

static gboolean
mail_msg_idle_cb (void)
{
	MailMsg *msg;

	g_return_val_if_fail (main_loop_queue != NULL, FALSE);
	g_return_val_if_fail (msg_reply_queue != NULL, FALSE);

	G_LOCK (idle_source_id);
	idle_source_id = 0;
	G_UNLOCK (idle_source_id);
	/* check the main loop queue */
	while ((msg = g_async_queue_try_pop (main_loop_queue)) != NULL) {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (msg->activity);

		g_idle_add_full (
			G_PRIORITY_DEFAULT,
			(GSourceFunc) mail_msg_submit,
			g_object_ref (msg->activity),
			(GDestroyNotify) g_object_unref);
		if (msg->info->exec != NULL)
			msg->info->exec (msg, cancellable, &msg->error);
		if (msg->info->done != NULL)
			msg->info->done (msg);
		mail_msg_unref (msg);
	}

	/* check the reply queue */
	while ((msg = g_async_queue_try_pop (msg_reply_queue)) != NULL) {
		if (msg->info->done != NULL)
			msg->info->done (msg);
		mail_msg_check_error (msg);
		mail_msg_unref (msg);
	}
	return FALSE;
}

static void
mail_msg_proxy (MailMsg *msg)
{
	GCancellable *cancellable;

	cancellable = e_activity_get_cancellable (msg->activity);

	if (msg->info->desc != NULL) {
		gchar *text = msg->info->desc (msg);
		camel_operation_push_message (cancellable, "%s", text);
		g_free (text);
	}

	g_idle_add_full (
		G_PRIORITY_DEFAULT,
		(GSourceFunc) mail_msg_submit,
		g_object_ref (msg->activity),
		(GDestroyNotify) g_object_unref);

	if (msg->info->exec != NULL)
		msg->info->exec (msg, cancellable, &msg->error);

	if (msg->info->desc != NULL)
		camel_operation_pop_message (cancellable);

	g_async_queue_push (msg_reply_queue, msg);

	G_LOCK (idle_source_id);
	if (idle_source_id == 0)
		idle_source_id = g_idle_add (
			(GSourceFunc) mail_msg_idle_cb, NULL);
	G_UNLOCK (idle_source_id);
}

void
mail_msg_init (void)
{
	mail_msg_lock = g_mutex_new ();
	mail_msg_cond = g_cond_new ();

	main_loop_queue = g_async_queue_new ();
	msg_reply_queue = g_async_queue_new ();

	mail_msg_active_table = g_hash_table_new (NULL, NULL);
	main_thread = g_thread_self ();
}

static gint
mail_msg_compare (const MailMsg *msg1, const MailMsg *msg2)
{
	gint priority1 = msg1->priority;
	gint priority2 = msg2->priority;

	if (priority1 == priority2)
		return 0;

	return (priority1 < priority2) ? 1 : -1;
}

static gpointer
create_thread_pool (gpointer data)
{
	GThreadPool *thread_pool;
	gint max_threads = GPOINTER_TO_INT (data);

	/* once created, run forever */
	thread_pool = g_thread_pool_new (
		(GFunc) mail_msg_proxy, NULL, max_threads, FALSE, NULL);
	g_thread_pool_set_sort_function (
		thread_pool, (GCompareDataFunc) mail_msg_compare, NULL);

	return thread_pool;
}

void
mail_msg_main_loop_push (gpointer msg)
{
	g_async_queue_push_sorted (main_loop_queue, msg,
		(GCompareDataFunc) mail_msg_compare, NULL);

	G_LOCK (idle_source_id);
	if (idle_source_id == 0)
		idle_source_id = g_idle_add (
			(GSourceFunc) mail_msg_idle_cb, NULL);
	G_UNLOCK (idle_source_id);
}

void
mail_msg_unordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (10));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
}

void
mail_msg_fast_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
}

void
mail_msg_slow_ordered_push (gpointer msg)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, (GThreadFunc) create_thread_pool, GINT_TO_POINTER (1));

	g_thread_pool_push ((GThreadPool *) once.retval, msg, NULL);
}

gboolean
mail_in_main_thread (void)
{
	return (g_thread_self () == main_thread);
}

/* ********************************************************************** */

struct _call_msg {
	MailMsg base;

	mail_call_t type;
	MailMainFunc func;
	gpointer ret;
	va_list ap;
	EFlag *done;
};

static void
do_call (struct _call_msg *m,
         GCancellable *cancellable,
         GError **error)
{
	gpointer p1, *p2, *p3, *p4, *p5;
	gint i1;
	va_list ap;

	G_VA_COPY (ap, m->ap);

	switch (m->type) {
	case MAIL_CALL_p_p:
		p1 = va_arg (ap, gpointer );
		m->ret = m->func (p1);
		break;
	case MAIL_CALL_p_pp:
		p1 = va_arg (ap, gpointer );
		p2 = va_arg (ap, gpointer );
		m->ret = m->func (p1, p2);
		break;
	case MAIL_CALL_p_ppp:
		p1 = va_arg (ap, gpointer );
		p2 = va_arg (ap, gpointer );
		p3 = va_arg (ap, gpointer );
		m->ret = m->func (p1, p2, p3);
		break;
	case MAIL_CALL_p_pppp:
		p1 = va_arg (ap, gpointer );
		p2 = va_arg (ap, gpointer );
		p3 = va_arg (ap, gpointer );
		p4 = va_arg (ap, gpointer );
		m->ret = m->func (p1, p2, p3, p4);
		break;
	case MAIL_CALL_p_ppppp:
		p1 = va_arg (ap, gpointer );
		p2 = va_arg (ap, gpointer );
		p3 = va_arg (ap, gpointer );
		p4 = va_arg (ap, gpointer );
		p5 = va_arg (ap, gpointer );
		m->ret = m->func (p1, p2, p3, p4, p5);
		break;
	case MAIL_CALL_p_ppippp:
		p1 = va_arg (ap, gpointer );
		p2 = va_arg (ap, gpointer );
		i1 = va_arg (ap, gint);
		p3 = va_arg (ap, gpointer );
		p4 = va_arg (ap, gpointer );
		p5 = va_arg (ap, gpointer );
		m->ret = m->func (p1, p2, i1, p3, p4, p5);
		break;
	}

	e_activity_set_state (
		m->base.activity,
		g_cancellable_is_cancelled (cancellable) ?
		E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	if (m->done != NULL)
		e_flag_set (m->done);
}

static MailMsgInfo mail_call_info = {
	sizeof (struct _call_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) do_call,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) NULL
};

gpointer
mail_call_main (mail_call_t type, MailMainFunc func, ...)
{
	GCancellable *cancellable;
	struct _call_msg *m;
	gpointer ret;
	va_list ap;

	va_start (ap, func);

	m = mail_msg_new (&mail_call_info);
	m->type = type;
	m->func = func;
	G_VA_COPY (m->ap, ap);

	cancellable = e_activity_get_cancellable (m->base.activity);

	if (mail_in_main_thread ())
		do_call (m, cancellable, &m->base.error);
	else {
		mail_msg_ref (m);
		m->done = e_flag_new ();
		mail_msg_main_loop_push (m);
		e_flag_wait (m->done);
		e_flag_free (m->done);
	}

	va_end (ap);

	ret = m->ret;
	mail_msg_unref (m);

	return ret;
}

void
mail_mt_set_backend (gchar *backend)
{
	shell_builtin_backend = backend;
}

