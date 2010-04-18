/*  This file is part of the GtkHTML library.
 *
 *  Copyright 2002 Ximian, Inc.
 *
 *  Author: Radek Doulik
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "gtkhtml.h"
#include "htmlengine.h"
#include "htmlobject.h"

#include "object.h"
#include "paragraph.h"
#include "utils.h"
#include "text.h"
#include <glib/gi18n-lib.h>

static void gtk_html_a11y_class_init (GtkHTMLA11YClass *klass);
static void gtk_html_a11y_init       (GtkHTMLA11Y *a11y);

static GtkAccessibleClass *parent_class = NULL;

static gint
get_n_actions (AtkAction *action)
{
	return 1;
}

static G_CONST_RETURN gchar *
get_description (AtkAction *action, gint i)
{
	if (i == 0)
		return _("grab focus");

	return NULL;
}

static G_CONST_RETURN gchar *
action_get_name (AtkAction *action, gint i)
{
	if (i == 0)
		return _("grab focus");

	return NULL;
}

static gboolean
do_action (AtkAction * action, gint i)
{
	GtkWidget *widget;
	gboolean return_value = TRUE;

#if GTK_CHECK_VERSION(2,21,0)
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
#else
	widget = GTK_ACCESSIBLE (action)->widget;
#endif

	if (widget == NULL) {
	/*
	* State is defunct
	*/
	return FALSE;
	}

	if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
		return FALSE;

	switch (i) {
	case 0:
		gtk_widget_grab_focus (widget);
	default:
		return_value = FALSE;
		break;
	}
	return return_value;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = do_action;
	iface->get_n_actions = get_n_actions;
	iface->get_description = get_description;
	iface->get_name = action_get_name;
}

GType
gtk_html_a11y_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (GtkHTMLA11YClass),
			NULL,                                            /* base init */
			NULL,                                            /* base finalize */
			(GClassInitFunc) gtk_html_a11y_class_init,       /* class init */
			NULL,                                            /* class finalize */
			NULL,                                            /* class data */
			sizeof (GtkHTMLA11Y),                            /* instance size */
			0,                                               /* nb preallocs */
			(GInstanceInitFunc) gtk_html_a11y_init,          /* instance init */
			NULL                                             /* value table */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		/*
		 * Figure out the size of the class and instance
		 * we are deriving from
		 */
		AtkObjectFactory *factory;
		GTypeQuery query;
		GType derived_atk_type;

		factory = atk_registry_get_factory (atk_get_default_registry (), GTK_TYPE_WIDGET);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);
		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type, "GtkHTMLA11Y", &tinfo, 0);

		g_type_add_interface_static (type, ATK_TYPE_ACTION, &atk_action_info);
	}

	return type;
}

static void
gtk_html_a11y_finalize (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gtk_html_a11y_initialize (AtkObject *obj, gpointer data)
{
	/* printf ("gtk_html_a11y_initialize\n"); */

	if (ATK_OBJECT_CLASS (parent_class)->initialize)
		ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

	g_object_set_data (G_OBJECT (obj), GTK_HTML_ID, data);
}

static gint
gtk_html_a11y_get_n_children (AtkObject *accessible)
{
	HTMLObject *clue;
	gint n_children = 0;
	AtkStateSet *ss;

	if (GTK_HTML_A11Y_GTKHTML (accessible)->engine->parsing)
		return 0;

	ss = atk_object_ref_state_set (accessible);
	if (atk_state_set_contains_state (ss, ATK_STATE_DEFUNCT)) {
		g_object_unref (ss);
		return 0;
	}
	g_object_unref (ss);

	clue = GTK_HTML_A11Y_GTKHTML (accessible)->engine->clue;
	if (clue) {
		AtkObject *atk_clue = html_utils_get_accessible (clue, NULL);
		if (atk_clue) {
			AtkStateSet *ss_clue = atk_object_ref_state_set (atk_clue);
			if (atk_state_set_contains_state (ss_clue, ATK_STATE_DEFUNCT)) {
				g_object_unref (ss_clue);
				return 0;
			}
			g_object_unref (ss_clue);
		}

		n_children = html_object_get_n_children (GTK_HTML_A11Y_GTKHTML (accessible)->engine->clue);
	}

	/* printf ("gtk_html_a11y_get_n_children resolves to %d\n", n_children); */

	return n_children;
}

static AtkObject *
gtk_html_a11y_ref_child (AtkObject *accessible, gint index)
{
	HTMLObject *child;
	AtkObject *accessible_child = NULL;
	AtkStateSet *ss;

	if (GTK_HTML_A11Y_GTKHTML (accessible)->engine->parsing)
		return NULL;

	ss = atk_object_ref_state_set (accessible);
	if (atk_state_set_contains_state (ss, ATK_STATE_DEFUNCT)) {
		g_object_unref (ss);
		return NULL;
	}
	g_object_unref (ss);

	if (GTK_HTML_A11Y_GTKHTML (accessible)->engine->clue) {
		AtkObject *atk_clue = html_utils_get_accessible (GTK_HTML_A11Y_GTKHTML (accessible)->engine->clue, NULL);
		if (atk_clue) {
			AtkStateSet *ss_clue = atk_object_ref_state_set (atk_clue);
			if (atk_state_set_contains_state (ss_clue, ATK_STATE_DEFUNCT)) {
				g_object_unref (ss_clue);
				return NULL;
			}
			g_object_unref (ss_clue);
		}

		child = html_object_get_child (GTK_HTML_A11Y_GTKHTML (accessible)->engine->clue, index);
		if (child) {
			accessible_child = html_utils_get_accessible (child, accessible);
			if (accessible_child)
				g_object_ref (accessible_child);
		}
	}

	/* printf ("gtk_html_a11y_ref_child %d resolves to %p\n", index, accessible_child); */

	return accessible_child;
}

static G_CONST_RETURN gchar *
gtk_html_a11y_get_name (AtkObject *obj)
{
	if (obj->name != NULL) {
		return obj->name;
	}

	return _("Panel containing HTML");
}

static void
gtk_html_a11y_class_init (GtkHTMLA11YClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	atk_class->initialize = gtk_html_a11y_initialize;
	atk_class->get_n_children = gtk_html_a11y_get_n_children;
	atk_class->ref_child = gtk_html_a11y_ref_child;
	atk_class->get_name = gtk_html_a11y_get_name;

	gobject_class->finalize = gtk_html_a11y_finalize;
}

static void
gtk_html_a11y_init (GtkHTMLA11Y *a11y)
{
}

static AtkObject *
gtk_html_a11y_get_focus_object (GtkWidget * widget)
{
	GtkHTML * html;
	HTMLObject * htmlobj = NULL;
        AtkObject *obj = NULL;
	gint offset;

	html = GTK_HTML(widget);

	g_return_val_if_fail (html && html->engine, NULL);

	if (!html->engine->caret_mode && !gtk_html_get_editable (html))
		htmlobj = html_engine_get_focus_object (html->engine, &offset);
	else if (html->engine && html->engine->cursor)
		htmlobj = html->engine->cursor->object;

	if (htmlobj)
		obj = html_utils_get_accessible (htmlobj, NULL);

	return obj;
}

static AtkObject * gtk_html_a11y_focus_object = NULL;

static void
gtk_html_a11y_grab_focus_cb(GtkWidget * widget)
{
        AtkObject *focus_object, *obj, *clue;

	focus_object = gtk_html_a11y_get_focus_object (widget);
	if (focus_object == NULL)
		return;

	obj = gtk_widget_get_accessible (widget);

	clue = html_utils_get_accessible(GTK_HTML(widget)->engine->clue, obj);
	atk_object_set_parent(clue, obj);

	gtk_html_a11y_focus_object = focus_object;
	atk_focus_tracker_notify (focus_object);
}

static void
gtk_html_a11y_cursor_changed_cb (GtkWidget *widget)
{
	AtkObject *focus_object;

	focus_object = gtk_html_a11y_get_focus_object (widget);
	g_return_if_fail (focus_object != NULL);

	if (gtk_html_a11y_focus_object != focus_object) {
		gtk_html_a11y_focus_object = focus_object;
		atk_focus_tracker_notify (focus_object);
	} else {
		if (G_IS_HTML_A11Y_TEXT(focus_object)) {
			gint offset;

			offset = (GTK_HTML(widget))->engine->cursor->offset;
			g_signal_emit_by_name(focus_object, "text_caret_moved",offset);
		}
	}
}

static void
gtk_html_a11y_insert_object_cb (GtkWidget * widget, gint pos, gint len)
{
	AtkObject * a11y;

	HTMLCursor *cursor = GTK_HTML (widget)->engine->cursor;

	a11y = gtk_html_a11y_get_focus_object (widget);
	g_return_if_fail (a11y != NULL);

	if (gtk_html_a11y_focus_object != a11y) {
		gtk_html_a11y_focus_object = a11y;
		atk_focus_tracker_notify (a11y);
	}

	if (G_IS_HTML_A11Y_TEXT(a11y)) {
		g_signal_emit_by_name (a11y, "text_changed::insert", cursor->offset - len, len);

	}
}

static void
gtk_html_a11y_delete_object_cb (GtkWidget * widget, gint pos, gint len)
{
	AtkObject * a11y;

	a11y = gtk_html_a11y_get_focus_object (widget);
	g_return_if_fail (a11y != NULL);

	if (gtk_html_a11y_focus_object != a11y) {
		gtk_html_a11y_focus_object = a11y;
		atk_focus_tracker_notify (a11y);
	}

	if (G_IS_HTML_A11Y_TEXT(a11y)) {
		g_signal_emit_by_name (a11y, "text_changed::delete", pos, len);
	}
}

AtkObject*
gtk_html_a11y_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible, *focus_object;

	g_return_val_if_fail (GTK_IS_HTML (widget), NULL);

	object = g_object_new (G_TYPE_GTK_HTML_A11Y, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	accessible->role = ATK_ROLE_PANEL;
	g_signal_connect (widget, "grab_focus",
			G_CALLBACK (gtk_html_a11y_grab_focus_cb),
			NULL);
	g_signal_connect (widget, "cursor_changed",
			G_CALLBACK(gtk_html_a11y_cursor_changed_cb),
			NULL);
	g_signal_connect_after (widget, "object_inserted",
			G_CALLBACK(gtk_html_a11y_insert_object_cb),
			NULL);
	g_signal_connect_after (widget, "object_delete",
			G_CALLBACK(gtk_html_a11y_delete_object_cb),
			NULL);

	if (GTK_HTML(widget)->engine->clue)
		html_utils_get_accessible(GTK_HTML(widget)->engine->clue, accessible);

	/* printf ("created new gtkhtml accessible object\n"); */

	focus_object = gtk_html_a11y_get_focus_object (widget);

	if (focus_object && gtk_html_a11y_focus_object != focus_object) {
		gtk_html_a11y_focus_object = focus_object;
		atk_focus_tracker_notify (focus_object);
	}

	return accessible;
}
