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
#include <atk/atkcomponent.h>
#include <atk/atktext.h>
#include <atk/atkhypertext.h>
#include <atk/atkhyperlink.h>
#include <glib/gi18n-lib.h>
#include <pango/pango.h>
#include "gtkhtml.h"
#include "htmlengine.h"
#include "htmlengine-edit.h"
#include "htmlengine-edit-cursor.h"
#include "htmlengine-edit-movement.h"
#include "htmlengine-edit-cut-and-paste.h"
#include "htmlinterval.h"
#include "htmlselection.h"
#include "htmltext.h"
#include "htmltextslave.h"

#include "object.h"
#include "html.h"
#include "text.h"
#include "hyperlink.h"

static void html_a11y_text_class_init    (HTMLA11YTextClass *klass);
static void html_a11y_text_init          (HTMLA11YText *a11y_text);
static void atk_component_interface_init (AtkComponentIface *iface);
static void atk_text_interface_init      (AtkTextIface *iface);

static void html_a11y_text_get_extents   (AtkComponent *component,
					  gint *x, gint *y, gint *width, gint *height, AtkCoordType coord_type);
static void html_a11y_text_get_size      (AtkComponent *component, gint *width, gint *height);
static gboolean html_a11y_text_grab_focus (AtkComponent *comp);

static gchar * html_a11y_text_get_text (AtkText *text, gint start_offset, gint end_offset);
static gchar * html_a11y_text_get_text_after_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						     gint *start_offset, gint *end_offset);
static gchar * html_a11y_text_get_text_at_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						  gint *start_offset, gint *end_offset);
static gunichar html_a11y_text_get_character_at_offset (AtkText *text, gint offset);
static gchar * html_a11y_text_get_text_before_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						      gint *start_offset, gint *end_offset);
static gint html_a11y_text_get_character_count (AtkText *text);
static gint html_a11y_text_get_n_selections (AtkText *text);
static gchar *html_a11y_text_get_selection (AtkText *text, gint selection_num, gint *start_offset, gint *end_offset);
static gboolean html_a11y_text_add_selection (AtkText *text, gint start_offset, gint end_offset);
static gboolean html_a11y_text_remove_selection (AtkText *text, gint selection_num);
static gboolean html_a11y_text_set_selection (AtkText *text, gint selection_num, gint start_offset, gint end_offset);
static gint html_a11y_text_get_caret_offset (AtkText *text);
static gboolean html_a11y_text_set_caret_offset (AtkText *text, gint offset);
static void html_a11y_text_get_character_extents (AtkText *text, gint offset,
				gint *x, gint *y, gint *width, gint *height,
				AtkCoordType coords);
static gint html_a11y_text_get_offset_at_point (AtkText *text, gint x, gint y,
					AtkCoordType coords);

/* Hyperlink interface */
static void atk_hyper_text_interface_init (AtkHypertextIface *iface);

static AtkHyperlink * html_a11y_text_get_link (AtkHypertext *hypertext, gint link_index);
static gint html_a11y_text_get_n_links (AtkHypertext *hypertext);
static gint html_a11y_text_get_link_index (AtkHypertext *hypertext, gint char_index);

/* Editable text interface. */
static void	atk_editable_text_interface_init      (AtkEditableTextIface *iface);
static void	html_a11y_text_set_text_contents	(AtkEditableText      *text,
							 const gchar          *string);
static void	html_a11y_text_insert_text	(AtkEditableText      *text,
						 const gchar          *string,
						 gint                 length,
						 gint                 *position);
static void	html_a11y_text_copy_text	(AtkEditableText      *text,
						 gint                 start_pos,
						 gint                 end_pos);
static void	html_a11y_text_cut_text		(AtkEditableText      *text,
						 gint                 start_pos,
						 gint                 end_pos);
static void	html_a11y_text_delete_text	(AtkEditableText      *text,
						 gint                 start_pos,
						 gint                 end_pos);
static void	html_a11y_text_paste_text	(AtkEditableText      *text,
						 gint                 position);

static AtkStateSet* html_a11y_text_ref_state_set	(AtkObject	*accessible);

static AtkAttributeSet* html_a11y_text_get_run_attributes	(AtkText *text,
								 gint offset,
								 gint *start_offset,
								 gint *end_offset);

static AtkObjectClass *parent_class = NULL;

static gint
get_n_actions (AtkAction *action)
{
        return 1;
}

static G_CONST_RETURN gchar *
action_get_name (AtkAction *action, gint      i)
{
        if (i == 0)
                return _("grab focus");

        return NULL;
}

static gboolean
do_action (AtkAction * action, gint i)
{
        switch (i) {
        case 0:
		return html_a11y_text_grab_focus (ATK_COMPONENT (action));
        default:
                return FALSE;
        }
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
        g_return_if_fail (iface != NULL);

        iface->do_action = do_action;
        iface->get_n_actions = get_n_actions;
        iface->get_name = action_get_name;
}

GType
html_a11y_text_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo tinfo = {
			sizeof (HTMLA11YTextClass),
			NULL,                                                      /* base init */
			NULL,                                                      /* base finalize */
			(GClassInitFunc) html_a11y_text_class_init,                /* class init */
			NULL,                                                      /* class finalize */
			NULL,                                                      /* class data */
			sizeof (HTMLA11YText),                                     /* instance size */
			0,                                                         /* nb preallocs */
			(GInstanceInitFunc) html_a11y_text_init,                   /* instance init */
			NULL                                                       /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc) atk_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_editable_text_info =
		{
			(GInterfaceInitFunc) atk_editable_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_hyper_text_info = {
			(GInterfaceInitFunc) atk_hyper_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_HTML_A11Y, "HTMLA11YText", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);
		g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);
		g_type_add_interface_static (type, ATK_TYPE_ACTION, &atk_action_info);
		g_type_add_interface_static (type, ATK_TYPE_HYPERTEXT, &atk_hyper_text_info);
	}

	return type;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = html_a11y_text_get_extents;
	iface->get_size = html_a11y_text_get_size;
	iface->grab_focus = html_a11y_text_grab_focus;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_text = html_a11y_text_get_text;
	iface->get_text_after_offset = html_a11y_text_get_text_after_offset;
	iface->get_text_before_offset = html_a11y_text_get_text_before_offset;
	iface->get_text_at_offset = html_a11y_text_get_text_at_offset;
	iface->get_character_at_offset = html_a11y_text_get_character_at_offset;
	iface->get_character_count = html_a11y_text_get_character_count;
	iface->get_n_selections = html_a11y_text_get_n_selections;
	iface->get_selection = html_a11y_text_get_selection;
	iface->remove_selection = html_a11y_text_remove_selection;
	iface->set_selection = html_a11y_text_set_selection;
	iface->add_selection = html_a11y_text_add_selection;
	iface->get_caret_offset = html_a11y_text_get_caret_offset;
	iface->set_caret_offset = html_a11y_text_set_caret_offset;
	iface->get_character_extents = html_a11y_text_get_character_extents;
	iface->get_offset_at_point = html_a11y_text_get_offset_at_point;
	iface->get_run_attributes = html_a11y_text_get_run_attributes;
}

static void
html_a11y_text_finalize (GObject *obj)
{
        HTMLA11YText *ato = HTML_A11Y_TEXT (obj);

        if (ato->util != NULL) {
                g_object_unref (ato->util);
                ato->util = NULL;
        }

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
html_a11y_text_initialize (AtkObject *obj, gpointer data)
{
	GtkTextBuffer *buffer;
	HTMLText *to;
	HTMLA11YText *ato;

	/* printf ("html_a11y_text_initialize\n"); */

	if (ATK_OBJECT_CLASS (parent_class)->initialize)
		ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

	to = HTML_TEXT (data);
	ato = HTML_A11Y_TEXT (obj);

	buffer = gtk_text_buffer_new (NULL);
	ato->util = gail_text_util_new ();
	gtk_text_buffer_set_text (buffer, to->text, -1);
	gail_text_util_buffer_setup (ato->util, buffer);
	g_object_unref (buffer);
}

static void
html_a11y_text_class_init (HTMLA11YTextClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	atk_class->initialize = html_a11y_text_initialize;
	atk_class->ref_state_set = html_a11y_text_ref_state_set;
	gobject_class->finalize = html_a11y_text_finalize;
}

static void
html_a11y_text_init (HTMLA11YText *a11y_text)
{
}

AtkObject*
html_a11y_text_new (HTMLObject *html_obj)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (HTML_IS_TEXT (html_obj), NULL);

	object = g_object_new (G_TYPE_HTML_A11Y_TEXT, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, html_obj);

	accessible->role = ATK_ROLE_TEXT;

	/* printf ("created new html accessible text object\n"); */

	return accessible;
}

/* atkobject.h */

static AtkStateSet*
html_a11y_text_ref_state_set (AtkObject *accessible)
{
	AtkStateSet *state_set;
	GtkHTML * html;
	GtkHTMLA11Y *html_a11y;

	state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (accessible);
	html_a11y = html_a11y_get_gtkhtml_parent (HTML_A11Y (accessible));
	if (!html_a11y)
		return state_set;

	html = GTK_HTML_A11Y_GTKHTML (html_a11y);
	if (!html || !html->engine)
		return state_set;

	if (html_engine_get_editable(html->engine))
		atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);

	atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);
	atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

	return state_set;
}

/*
 * AtkComponent interface
 */

static void
html_a11y_text_get_extents (AtkComponent *component, gint *x, gint *y, gint *width, gint *height, AtkCoordType coord_type)
{
	HTMLObject *obj = HTML_A11Y_HTML (component);
	GtkHTMLA11Y *top_html_a11y;
	HTMLEngine *top_engine;
	gint min_x, min_y, max_x, max_y;
	HTMLObject *next;
	gint sx, sy;

	g_return_if_fail (obj);
	top_html_a11y = html_a11y_get_top_gtkhtml_parent (HTML_A11Y (component));
	g_return_if_fail (top_html_a11y);

	if (obj->y <obj->ascent)
		obj->y = obj->ascent;

	atk_component_get_extents (ATK_COMPONENT (top_html_a11y), x, y, width, height, coord_type);

	/* we need to get a max rect here */
	html_object_calc_abs_position (obj, &min_x, &min_y);
	max_x = min_x + obj->width;
	max_y = min_y + obj->descent;
	min_y -= obj->ascent;

	next = obj->next;
	while (next && HTML_IS_TEXT_SLAVE (next)) {
		html_object_calc_abs_position (next, &sx, &sy);
		min_x = MIN (min_x, sx);
		min_y = MIN (min_y, sy - next->ascent);
		max_x = MAX (max_x, sx + next->width);
		max_y = MAX (max_y, sy + next->descent);

		next = next->next;
	}

	*x += min_x;
	*width = max_x - min_x;
	*y += min_y;
	*height = max_y - min_y;

	/* scroll window */
	top_engine = GTK_HTML_A11Y_GTKHTML (top_html_a11y)->engine;
	*x -=  top_engine->x_offset;
	*y -=  top_engine->y_offset;
}

static void
html_a11y_text_get_size (AtkComponent *component, gint *width, gint *height)
{
	gint x, y;

	html_a11y_get_extents (component, &x, &y, width, height, ATK_XY_WINDOW);
}

static gboolean
html_a11y_text_grab_focus (AtkComponent *comp)
{
	GtkHTML *html;

	html = GTK_HTML_A11Y_GTKHTML (html_a11y_get_gtkhtml_parent (HTML_A11Y (comp)));
	g_return_val_if_fail (html && html->engine && html_engine_get_editable (html->engine), FALSE);

	html_engine_jump_to_object (html->engine, HTML_A11Y_HTML (comp), HTML_TEXT (HTML_A11Y_HTML (comp))->text_len);
	g_signal_emit_by_name (html, "grab_focus");

        return TRUE;
}

/*
 * AtkText interface
 */

static gchar *
html_a11y_text_get_text (AtkText *text, gint start_offset, gint end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));
	gchar *str;

	g_return_val_if_fail (to, NULL);

	/* printf ("%d - %d\n", start_offset, end_offset); */
	if (end_offset == -1)
		end_offset = to->text_len;

	if (start_offset > end_offset)
		return NULL;
	if (start_offset < 0)
		return NULL;
	if (start_offset > to->text_len)
		return NULL;
	if (end_offset > to->text_len)
		return NULL;

	str = html_text_get_text (to, start_offset);

	return g_strndup (str, g_utf8_offset_to_pointer (str, end_offset - start_offset) - str);
}

static gint
html_a11y_text_get_caret_offset(AtkText * text)
{
	HTMLObject * p;
	HTMLEngine * e;
	GtkHTML * html;

	g_return_val_if_fail(text, 0);

	p= HTML_A11Y_HTML(text);
	g_return_val_if_fail(p && HTML_IS_TEXT(p), 0);

	html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));

	g_return_val_if_fail(html && GTK_IS_HTML(html) && html->engine, 0);

	e = html_engine_get_top_html_engine(html->engine);

	g_return_val_if_fail(e && e->cursor && e->cursor->object == p, 0);

	return e->cursor->offset;
}

static gboolean
html_a11y_text_set_caret_offset(AtkText * text, gint offset)
{
	GtkHTML * html;
	HTMLEngine * e;
	HTMLObject * obj = HTML_A11Y_HTML(text);

	html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));

	g_return_val_if_fail(obj && html && html->engine, FALSE);

	e = html->engine;
	html_engine_jump_to_object(e, obj, offset);

	return TRUE;
}

static gchar *
html_a11y_text_get_text_after_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				      gint *start_offset, gint *end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));
	HTMLTextSlave *start_slave, *end_slave;

	g_return_val_if_fail (to, NULL);
	g_return_val_if_fail (start_offset && end_offset, NULL);

	switch (boundary_type) {
	case ATK_TEXT_BOUNDARY_LINE_START:
		end_slave = html_text_get_slave_at_offset (to, NULL, offset);
		g_return_val_if_fail (end_slave, NULL);
		start_slave = (HTMLTextSlave *) HTML_OBJECT (end_slave)->next;

		if (start_slave && HTML_IS_TEXT_SLAVE (start_slave)) {
			*start_offset = start_slave->posStart;
			end_slave = (HTMLTextSlave *) HTML_OBJECT (start_slave)->next;
			if (end_slave && HTML_IS_TEXT_SLAVE (end_slave))
				*end_offset = end_slave->posStart;
			else
				*end_offset = start_slave->posStart + start_slave->posLen;

		} else { /* we are on the last line. */
			*start_offset = *end_offset = html_a11y_text_get_character_count (text);
		}

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	case ATK_TEXT_BOUNDARY_LINE_END:
		start_slave = html_text_get_slave_at_offset (to, NULL, offset);
		g_return_val_if_fail (start_slave, NULL);

		*start_offset = start_slave->posStart + start_slave->posLen;
		end_slave = (HTMLTextSlave *) HTML_OBJECT (start_slave)->next;
		if (end_slave && HTML_IS_TEXT_SLAVE (end_slave))
			*end_offset = end_slave->posStart + end_slave->posLen;
		else
			*end_offset = *start_offset;

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	default:
		gail_text_util_text_setup (HTML_A11Y_TEXT (text)->util, to->text);
		return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_AFTER_OFFSET, boundary_type, offset,
					start_offset, end_offset);
	}
}

static gchar *
html_a11y_text_get_text_at_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				   gint *start_offset, gint *end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));
	HTMLTextSlave *start_slave, *end_slave;

	g_return_val_if_fail (to, NULL);
	g_return_val_if_fail (start_offset && end_offset, NULL);

	switch (boundary_type) {
	case ATK_TEXT_BOUNDARY_LINE_START:
		start_slave = html_text_get_slave_at_offset (to, NULL, offset);

		if (start_slave == NULL) {
			*start_offset = 0;
			*end_offset = to->text_len;
		} else {
			end_slave = (HTMLTextSlave *) HTML_OBJECT (start_slave)->next;

			if (end_slave && HTML_IS_TEXT_SLAVE (end_slave)) {
				*end_offset = end_slave->posStart;
			} else {
				*end_offset = start_slave->posStart + start_slave->posLen;
			}
			*start_offset = start_slave->posStart;
		}

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	case ATK_TEXT_BOUNDARY_LINE_END:
		end_slave = html_text_get_slave_at_offset (to, NULL, offset);

		if (end_slave == NULL) {
			*start_offset = 0;
			*end_offset = to->text_len;
		} else {
			start_slave = (HTMLTextSlave *) HTML_OBJECT (end_slave)->prev;

			if (start_slave && HTML_IS_TEXT_SLAVE (start_slave)) {
				*start_offset = start_slave->posStart + start_slave->posLen;
			} else {
				*start_offset = end_slave->posStart;
			}
			*end_offset = end_slave->posStart + end_slave->posLen;
		}

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	default:
		gail_text_util_text_setup (HTML_A11Y_TEXT (text)->util, to->text);
		return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_AT_OFFSET, boundary_type, offset,
					start_offset, end_offset);
	}

}

static gunichar
html_a11y_text_get_character_at_offset (AtkText *text, gint offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));

	g_return_val_if_fail (to && offset <= to->text_len, 0);

	return html_text_get_char (to, offset);
}

static gchar *
html_a11y_text_get_text_before_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				       gint *start_offset, gint *end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));
	HTMLTextSlave *start_slave, *end_slave;

	g_return_val_if_fail (to, NULL);
	g_return_val_if_fail (start_offset && end_offset, NULL);

	switch (boundary_type) {
	case ATK_TEXT_BOUNDARY_LINE_START:
		end_slave = html_text_get_slave_at_offset (to, NULL, offset);
		g_return_val_if_fail (end_slave, NULL);
		start_slave = (HTMLTextSlave *) HTML_OBJECT (end_slave)->prev;

		*end_offset = end_slave->posStart;
		if (start_slave && HTML_IS_TEXT_SLAVE (start_slave)) {
			*start_offset = start_slave->posStart;
		} else
			*start_offset = *end_offset;

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	case ATK_TEXT_BOUNDARY_LINE_END:
		start_slave = html_text_get_slave_at_offset (to, NULL, offset);
		g_return_val_if_fail (start_slave, NULL);
		end_slave = (HTMLTextSlave *) HTML_OBJECT (start_slave)->prev;

		if (end_slave && HTML_IS_TEXT_SLAVE (end_slave)) {
			*end_offset = end_slave->posStart + end_slave->posLen;
			start_slave = (HTMLTextSlave *) HTML_OBJECT (end_slave)->prev;
			if (start_slave && HTML_IS_TEXT_SLAVE (start_slave))
				*start_offset = start_slave->posStart + start_slave->posLen;
			else
				*start_offset = end_slave->posStart;

		} else {
			*start_offset = *end_offset = 0;	/* on the first line */
		}

		return html_a11y_text_get_text (text, *start_offset, *end_offset);

	default:
		gail_text_util_text_setup (HTML_A11Y_TEXT (text)->util, to->text);
		return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_BEFORE_OFFSET, boundary_type, offset,
					start_offset, end_offset);
	}
}

static gint
html_a11y_text_get_character_count (AtkText *text)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));

	g_return_val_if_fail (to, 0);
	return to->text_len;
}

static gint
html_a11y_text_get_n_selections (AtkText *text)
{
	HTMLObject *to = HTML_A11Y_HTML (text);

	g_return_val_if_fail (to, 0);
	return to->selected ? 1 : 0;
}

static gchar *
html_a11y_text_get_selection (AtkText *text, gint selection_num, gint *start_offset, gint *end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));

	if (!to || !HTML_OBJECT (to)->selected || selection_num > 0)
		return NULL;

	*start_offset = to->select_start;
	*end_offset = to->select_start + to->select_length;

	return html_a11y_text_get_text (text, *start_offset, *end_offset);
}

static gboolean
html_a11y_text_add_selection (AtkText *text, gint start_offset, gint end_offset)
{
	GtkHTML *html = GTK_HTML_A11Y_GTKHTML (html_a11y_get_gtkhtml_parent (HTML_A11Y (text)));
	HTMLObject *obj = HTML_A11Y_HTML (text);
	HTMLInterval *i;

	g_return_val_if_fail(html && html->engine, FALSE);

	if (html_engine_is_selection_active (html->engine))
		return FALSE;

	i = html_interval_new (obj, obj, start_offset, end_offset);
	html_engine_select_interval (html->engine, i);

	return TRUE;
}

static gboolean
html_a11y_text_remove_selection (AtkText *text, gint selection_num)
{
	GtkHTML *html = GTK_HTML_A11Y_GTKHTML (html_a11y_get_gtkhtml_parent (HTML_A11Y (text)));
	HTMLObject *obj = HTML_A11Y_HTML (text);

	if (!obj->selected || selection_num)
		return FALSE;

	html_engine_unselect_all (html->engine);

	return TRUE;
}

static gboolean
html_a11y_text_set_selection (AtkText *text, gint selection_num, gint start_offset, gint end_offset)
{
	if (selection_num)
		return FALSE;

	return html_a11y_text_add_selection (text, start_offset, end_offset);
}

/* Most of the code of the following function is copied from */
/* libgail-util/gailmisc.c gail_misc_layout_get_run_attributes */

static AtkAttributeSet *
html_a11y_text_get_run_attributes (AtkText *text,
				   gint offset,
				   gint *start_offset,
				   gint *end_offset)
{
	PangoAttrIterator *iter;
	PangoAttrList *attr;
	PangoAttrString *pango_string;
	PangoAttrInt *pango_int;
	PangoAttrColor *pango_color;
	PangoAttrLanguage *pango_lang;
	PangoAttrFloat *pango_float;
	gint index, start_index, end_index;
	gboolean is_next = TRUE;
	gchar *value = NULL;
	glong len;
	gchar *textstring;
	AtkAttributeSet *attrib_set = NULL;
	GtkHTMLA11Y *a11y;
	GtkHTML *html;
	HTMLEngine *e;
	HTMLText *t = HTML_TEXT (HTML_A11Y_HTML (text));

	g_return_val_if_fail (t, NULL);
	textstring = t->text;
	g_return_val_if_fail (textstring, NULL);

	a11y = html_a11y_get_top_gtkhtml_parent (HTML_A11Y (text));
	g_return_val_if_fail (a11y, NULL);

	html = GTK_HTML_A11Y_GTKHTML (a11y);
	g_return_val_if_fail (html && GTK_IS_HTML(html) && html->engine, NULL);

	e = html->engine;
	attr = html_text_prepare_attrs (t, e->painter);
	g_return_val_if_fail (attr, NULL);

	len = g_utf8_strlen (textstring, -1);
	iter = pango_attr_list_get_iterator (attr);

	/* Get invariant range offsets */
	/* If offset out of range, set offset in range */
	if (offset > len)
		offset = len;
	else if (offset < 0)
		offset = 0;

	index = g_utf8_offset_to_pointer (textstring, offset) - textstring;
	pango_attr_iterator_range (iter, &start_index, &end_index);

	while (is_next) {
		if (index >= start_index && index < end_index) {
			*start_offset = g_utf8_pointer_to_offset (textstring, textstring + start_index);
			if (end_index == G_MAXINT)
			/* Last iterator */
			end_index = len;

			*end_offset = g_utf8_pointer_to_offset (textstring, textstring + end_index);
			break;
		}
		is_next = pango_attr_iterator_next (iter);
		pango_attr_iterator_range (iter, &start_index, &end_index);
	}

	/* Get attributes */
	if ((pango_string = (PangoAttrString*) pango_attr_iterator_get (iter,
						PANGO_ATTR_FAMILY)) != NULL) {
		value = g_strdup_printf("%s", pango_string->value);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_FAMILY_NAME,
				value);
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_STYLE)) != NULL) {
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_STYLE,
				g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE,
									pango_int->value)));
	}

	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_WEIGHT)) != NULL) {
		value = g_strdup_printf("%i", pango_int->value);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_WEIGHT,
				value);
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_VARIANT)) != NULL) {
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_VARIANT,
				g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT,
									pango_int->value)));
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_STRETCH)) != NULL) {
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_STRETCH,
				g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH,
									pango_int->value)));
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_SIZE)) != NULL) {
		value = g_strdup_printf("%i", pango_int->value / PANGO_SCALE);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_SIZE,
				value);
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_UNDERLINE)) != NULL) {
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_UNDERLINE,
				g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE,
									pango_int->value)));
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_STRIKETHROUGH)) != NULL) {
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_STRIKETHROUGH,
				g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH,
									pango_int->value)));
	}
	if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
						PANGO_ATTR_RISE)) != NULL) {
		value = g_strdup_printf("%i", pango_int->value);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_RISE,
				value);
	}
	if ((pango_lang = (PangoAttrLanguage*) pango_attr_iterator_get (iter,
						PANGO_ATTR_LANGUAGE)) != NULL) {
		value = g_strdup( pango_language_to_string( pango_lang->value));
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_LANGUAGE,
				value);
	}
	if ((pango_float = (PangoAttrFloat*) pango_attr_iterator_get (iter,
						PANGO_ATTR_SCALE)) != NULL) {
		value = g_strdup_printf("%g", pango_float->value);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_SCALE,
				value);
	}
	if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter,
						PANGO_ATTR_FOREGROUND)) != NULL) {
		value = g_strdup_printf ("%u,%u,%u",
				pango_color->color.red,
				pango_color->color.green,
				pango_color->color.blue);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_FG_COLOR,
				value);
	}
	if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter,
						PANGO_ATTR_BACKGROUND)) != NULL) {
		value = g_strdup_printf ("%u,%u,%u",
				pango_color->color.red,
				pango_color->color.green,
				pango_color->color.blue);
		attrib_set = gail_misc_add_attribute (attrib_set,
				ATK_TEXT_ATTR_BG_COLOR,
				value);
	}

	pango_attr_iterator_destroy (iter);
	pango_attr_list_unref (attr);
	return attrib_set;

}

/*
  AtkAttributeSet* (* get_default_attributes)     (AtkText	    *text);
*/

static gint
html_a11y_text_get_offset_at_point (AtkText *text, gint x, gint y,
					AtkCoordType coords)
{
	GtkHTML *top_html;
	GtkHTMLA11Y *top_a11y;
	HTMLEngine *top_e;
	HTMLObject *obj, *return_obj;
	gint offset = -1;
	gint html_x, html_y, html_height, html_width;
	gint text_x, text_y, text_height, text_width;

	obj = HTML_A11Y_HTML (text);
	g_return_val_if_fail(obj && html_object_is_text(obj), -1);

	atk_component_get_extents (ATK_COMPONENT (text), &text_x, &text_y, &text_width, &text_height, coords);

	/* check whether in range */
	if (x < text_x || x > text_x + text_width
		|| y < text_y || y > text_y + text_height)
		return -1;

	top_a11y = html_a11y_get_top_gtkhtml_parent (HTML_A11Y (text));
	g_return_val_if_fail (top_a11y, -1);

	top_html = GTK_HTML_A11Y_GTKHTML (top_a11y);
	g_return_val_if_fail (top_html && GTK_IS_HTML(top_html) && top_html->engine, -1);
	top_e = top_html->engine;

	atk_component_get_extents (ATK_COMPONENT (top_a11y), &html_x, &html_y, &html_width, &html_height, coords);

	x -= html_x;
	y -= html_y;

	return_obj = html_engine_get_object_at (top_e, x, y, (guint *) &offset, FALSE);

	if (obj == return_obj)
		return offset;
	else
		/*since the point is in range, need to return a valid value */
		return 0;
}

static void
html_a11y_text_get_character_extents (AtkText *text, gint offset,
				gint *x, gint *y, gint *width, gint *height,
				AtkCoordType coords)
{
	HTMLObject *obj;
	GtkHTML *html;
	HTMLEngine *e;
	gint x1, x2, y1, y2;
	GtkHTMLA11Y *a11y;

	obj = HTML_A11Y_HTML (text);
	g_return_if_fail(obj && html_object_is_text(obj));

	a11y = html_a11y_get_top_gtkhtml_parent (HTML_A11Y (text));
	g_return_if_fail (a11y);

	html = GTK_HTML_A11Y_GTKHTML (a11y);
	g_return_if_fail(html && GTK_IS_HTML(html) && html->engine);
	e = html->engine;

	atk_component_get_extents (ATK_COMPONENT (a11y), x, y, width, height, coords);
	html_object_get_cursor (obj, e->painter, offset, &x1, &y1, &x2, &y2);

	*x += x1;
	*y += y1;
	*height = y2 - y1;
	/* a reasonable guess */
	*width = *height / 2;

	/* scroll window */
	*x -=  e->x_offset;
	*y -=  e->y_offset;
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->set_text_contents = html_a11y_text_set_text_contents;
	iface->insert_text = html_a11y_text_insert_text;
	iface->copy_text = html_a11y_text_copy_text;
	iface->cut_text = html_a11y_text_cut_text;
	iface->delete_text = html_a11y_text_delete_text;
	iface->paste_text = html_a11y_text_paste_text;
	iface->set_run_attributes = NULL;
}

static void
html_a11y_text_set_text_contents (AtkEditableText *text,
				  const gchar     *string)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk set text contents called text %p\n", text);*/
	g_return_if_fail(string);

        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

        html_engine_hide_cursor (html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), 0);
	html_engine_set_mark(html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), t->text_len);
	html_engine_update_selection_if_necessary (html->engine);
	html_engine_paste_text(html->engine, string, -1);
        html_engine_show_cursor (html->engine);

        g_signal_emit_by_name(html, "grab_focus");
}

static void
html_a11y_text_insert_text (AtkEditableText *text,
			    const gchar     *string,
			    gint            length,
			    gint            *position)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk insert text called \n"); */

	g_return_if_fail(string && (length > 0));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), *position);
	html_engine_paste_text(html->engine, string, -1);
}

static void
html_a11y_text_copy_text	(AtkEditableText *text,
				 gint            start_pos,
				 gint            end_pos)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk copy text called \n"); */
        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

        html_engine_hide_cursor (html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), start_pos);
	html_engine_set_mark(html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), end_pos);
	html_engine_update_selection_if_necessary (html->engine);

	html_engine_copy(html->engine);
        html_engine_show_cursor (html->engine);
}

static void
html_a11y_text_cut_text (AtkEditableText *text,
			 gint            start_pos,
			 gint            end_pos)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk cut text called.\n"); */
        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

        html_engine_hide_cursor (html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), start_pos);
	html_engine_set_mark(html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), end_pos);
	html_engine_update_selection_if_necessary (html->engine);
	html_engine_cut(html->engine);
	html_engine_show_cursor (html->engine);

        g_signal_emit_by_name(html, "grab_focus");
}

static void
html_a11y_text_delete_text	(AtkEditableText *text,
			 gint            start_pos,
			 gint            end_pos)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk delete text called.\n"); */
        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), start_pos);
	html_engine_delete_n(html->engine, end_pos-start_pos, TRUE);
        g_signal_emit_by_name(html, "grab_focus");
}

static void
html_a11y_text_paste_text	(AtkEditableText *text,
				 gint            position)
{
	GtkHTML * html;
	HTMLText *t;

	/* fprintf(stderr, "atk paste text called.\n"); */

        html = GTK_HTML_A11Y_GTKHTML(html_a11y_get_gtkhtml_parent(HTML_A11Y(text)));
	g_return_if_fail(html && html->engine && html_engine_get_editable(html->engine));
	t = HTML_TEXT(HTML_A11Y_HTML(text));
	g_return_if_fail (t);

        html_engine_show_cursor (html->engine);
	html_cursor_jump_to(html->engine->cursor, html->engine, HTML_OBJECT(t), position);
	html_engine_paste(html->engine);
        html_engine_show_cursor (html->engine);

        g_signal_emit_by_name(html, "grab_focus");
}

static void
atk_hyper_text_interface_init (AtkHypertextIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_link = html_a11y_text_get_link;
	iface->get_n_links = html_a11y_text_get_n_links;
	iface->get_link_index = html_a11y_text_get_link_index;
}

/*
 * AtkHyperLink interface
 */

static AtkHyperlink *
html_a11y_text_get_link (AtkHypertext *hypertext, gint link_index)
{
	return html_a11y_hyper_link_new (HTML_A11Y (hypertext), link_index);
}

static gint
html_a11y_text_get_n_links (AtkHypertext *hypertext)
{
	HTMLText *text = HTML_TEXT (HTML_A11Y_HTML (hypertext));
	if (!text || !HTML_IS_TEXT (text))
		return 0;
	return g_slist_length (text->links);
}

static gint
html_a11y_text_get_link_index (AtkHypertext *hypertext, gint char_index)
{
	HTMLObject *obj = HTML_A11Y_HTML (hypertext);
	Link *link = html_text_get_link_at_offset (HTML_TEXT (obj), char_index);

	return link ? link->start_offset : -1;
}
