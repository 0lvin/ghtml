/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright 1999, 2000 Helix Code, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "../a11y/factory.h"

#include "htmlcolorset.h"
#include "htmlcluev.h"
#include "htmlcursor.h"
#include "htmldrawqueue.h"
#include "htmlengine-edit.h"
#include "htmlengine-edit-clueflowstyle.h"
#include "htmlengine-edit-cut-and-paste.h"
#include "htmlengine-edit-fontstyle.h"
#include "htmlengine-edit-rule.h"
#include "htmlengine-edit-movement.h"
#include "htmlengine-edit-cursor.h"
#include "htmlengine-edit-table.h"
#include "htmlengine-edit-tablecell.h"
#include "htmlengine-edit-text.h"
#include "htmlengine-edit-selection-updater.h"
#include "htmlengine-print.h"
#include "htmlengine-save.h"
#include "htmlform.h"
#include "htmlframe.h"
#include "htmlframeset.h"
#include "htmliframe.h"
#include "htmlimage.h"
#include "htmlinterval.h"
#include "htmlmarshal.h"
#include "htmlplainpainter.h"
#include "htmlsettings.h"
#include "htmltable.h"
#include "htmltext.h"
#include "htmltextslave.h"
#include "htmlselection.h"
#include "htmlundo.h"

#include "gtkhtml.h"
#include "gtkhtml-embedded.h"
#include "gtkhtml-keybinding.h"
#include "gtkhtml-search.h"
#include "gtkhtml-stream.h"
#include "gtkhtml-private.h"
#include "gtkhtml-properties.h"
#include "gtkhtmldebug.h"
#include "math.h"

enum DndTargetType {
	DND_TARGET_TYPE_MESSAGE_RFC822,
	DND_TARGET_TYPE_X_UID_LIST,
	DND_TARGET_TYPE_TEXT_URI_LIST,
	DND_TARGET_TYPE_MOZILLA_URL,
	DND_TARGET_TYPE_TEXT_VCARD,
	DND_TARGET_TYPE_TEXT_CALENDAR,
	DND_TARGET_TYPE_TEXT_HTML,
	DND_TARGET_TYPE_UTF8_STRING,
	DND_TARGET_TYPE_TEXT_PLAIN,
	DND_TARGET_TYPE_STRING
};

static GtkTargetEntry dnd_link_sources [] = {
	{ (gchar *) "message/rfc822", 0, DND_TARGET_TYPE_MESSAGE_RFC822 },
	{ (gchar *) "x-uid-list", 0, DND_TARGET_TYPE_X_UID_LIST },
	{ (gchar *) "text/uri-list", 0, DND_TARGET_TYPE_TEXT_URI_LIST },
	{ (gchar *) "_NETSCAPE_URL", 0, DND_TARGET_TYPE_MOZILLA_URL },
	{ (gchar *) "text/x-vcard", 0, DND_TARGET_TYPE_TEXT_VCARD },
	{ (gchar *) "text/calendar", 0, DND_TARGET_TYPE_TEXT_CALENDAR },
	{ (gchar *) "text/html", 0, DND_TARGET_TYPE_TEXT_HTML },
	{ (gchar *) "UTF8_STRING", 0, DND_TARGET_TYPE_UTF8_STRING },
	{ (gchar *) "text/plain", 0, DND_TARGET_TYPE_TEXT_PLAIN },
	{ (gchar *) "STRING", 0, DND_TARGET_TYPE_STRING },
};
#define DND_LINK_SOURCES sizeof (dnd_link_sources) / sizeof (GtkTargetEntry)

enum _TargetInfo {
	TARGET_HTML,
	TARGET_UTF8_STRING,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_TEXT
};

typedef enum _TargetInfo TargetInfo;

static const GtkTargetEntry selection_targets[] = {
	{ (gchar *) "text/html", GTK_TARGET_SAME_APP, TARGET_HTML },
	{ (gchar *) "UTF8_STRING", 0, TARGET_UTF8_STRING },
	{ (gchar *) "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
	{ (gchar *) "STRING", 0, TARGET_STRING },
	{ (gchar *) "TEXT", 0, TARGET_TEXT }
};

static const guint n_selection_targets = G_N_ELEMENTS (selection_targets);

typedef struct _ClipboardContents ClipboardContents;

struct _ClipboardContents {
	gchar *html_text;
	gchar *plain_text;
};

#define d_s(x)
#define D_IM(x)

static GtkLayoutClass *parent_class = NULL;

#ifdef HAVE_GCONF
static GError      *gconf_error  = NULL;
#endif

enum {
	TITLE_CHANGED,
	URL_REQUESTED,
	LOAD_DONE,
	LINK_CLICKED,
	SET_BASE,
	SET_BASE_TARGET,
	ON_URL,
	REDIRECT,
	SUBMIT,
	OBJECT_REQUESTED,
	CURRENT_PARAGRAPH_STYLE_CHANGED,
	CURRENT_PARAGRAPH_INDENTATION_CHANGED,
	CURRENT_PARAGRAPH_ALIGNMENT_CHANGED,
	INSERTION_FONT_STYLE_CHANGED,
	INSERTION_COLOR_CHANGED,
	SIZE_CHANGED,
	IFRAME_CREATED,
	/* keybindings signals */
	SCROLL,
	CURSOR_MOVE,
	COMMAND,
	CURSOR_CHANGED,
	OBJECT_INSERTED,
	OBJECT_DELETE,
	/* now only last signal */
	LAST_SIGNAL
};

/* #define USE_PROPS */
#ifdef USE_PROPS
enum {
	PROP_0,
	PROP_EDITABLE,
	PROP_TITLE,
	PROP_DOCUMENT_BASE,
	PROP_TARGET_BASE,
};

static void     gtk_html_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void     gtk_html_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
#endif

static guint signals [LAST_SIGNAL] = { 0 };

static void
gtk_html_update_scrollbars_on_resize (GtkHTML *html,
				      gdouble old_doc_width, gdouble old_doc_height,
				      gdouble old_width, gdouble old_height,
				      gboolean *changed_x, gboolean *changed_y);

static void update_primary_selection    (GtkHTML *html);
static void clipboard_paste_received_cb (GtkClipboard     *clipboard,
					 GtkSelectionData *selection_data,
					 gpointer          user_data);
static gint motion_notify_event (GtkWidget *widget, GdkEventMotion *event);

/* keybindings signal hadlers */
static void     scroll                 (GtkHTML *html, GtkOrientation orientation, GtkScrollType scroll_type, gfloat position);
static void     cursor_move            (GtkHTML *html, GtkDirectionType dir_type, GtkHTMLCursorSkipType skip);
static gboolean command                (GtkHTML *html, GtkHTMLCommandType com_type);
static gint     mouse_change_pos       (GtkWidget *widget, GdkWindow *window, gint x, gint y, gint state);
static void     add_bindings           (GtkHTMLClass *klass);
static const gchar * get_value_nick    (GtkHTMLCommandType com_type);
static void	gtk_html_adjust_cursor_position (GtkHTML *html);
static gboolean any_has_cursor_moved (GtkHTML *html);
static gboolean any_has_skip_update_cursor (GtkHTML *html);

/* Interval for scrolling during selection.  */
#define SCROLL_TIMEOUT_INTERVAL 10


GtkHTMLParagraphStyle
clueflow_style_to_paragraph_style (HTMLClueFlowStyle style, HTMLListType item_type)
{
	switch (style) {
	case HTML_CLUEFLOW_STYLE_NORMAL:
		return GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	case HTML_CLUEFLOW_STYLE_H1:
		return GTK_HTML_PARAGRAPH_STYLE_H1;
	case HTML_CLUEFLOW_STYLE_H2:
		return GTK_HTML_PARAGRAPH_STYLE_H2;
	case HTML_CLUEFLOW_STYLE_H3:
		return GTK_HTML_PARAGRAPH_STYLE_H3;
	case HTML_CLUEFLOW_STYLE_H4:
		return GTK_HTML_PARAGRAPH_STYLE_H4;
	case HTML_CLUEFLOW_STYLE_H5:
		return GTK_HTML_PARAGRAPH_STYLE_H5;
	case HTML_CLUEFLOW_STYLE_H6:
		return GTK_HTML_PARAGRAPH_STYLE_H6;
	case HTML_CLUEFLOW_STYLE_ADDRESS:
		return GTK_HTML_PARAGRAPH_STYLE_ADDRESS;
	case HTML_CLUEFLOW_STYLE_PRE:
		return GTK_HTML_PARAGRAPH_STYLE_PRE;
	case HTML_CLUEFLOW_STYLE_LIST_ITEM:
		switch (item_type) {
		case HTML_LIST_TYPE_UNORDERED:
			return GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED;
		case HTML_LIST_TYPE_ORDERED_ARABIC:
			return GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT;
		case HTML_LIST_TYPE_ORDERED_LOWER_ROMAN:
		case HTML_LIST_TYPE_ORDERED_UPPER_ROMAN:
			return GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN;
		case HTML_LIST_TYPE_ORDERED_LOWER_ALPHA:
		case HTML_LIST_TYPE_ORDERED_UPPER_ALPHA:
			return GTK_HTML_PARAGRAPH_STYLE_ITEMALPHA;
		default:
			return GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED;
		}
	default:		/* This should not really happen, though.  */
		return GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	}
}

void
paragraph_style_to_clueflow_style (GtkHTMLParagraphStyle style, HTMLClueFlowStyle *flow_style, HTMLListType *item_type)
{
	*item_type = HTML_LIST_TYPE_BLOCKQUOTE;
	*flow_style = HTML_CLUEFLOW_STYLE_LIST_ITEM;

	switch (style) {
	case GTK_HTML_PARAGRAPH_STYLE_NORMAL:
		*flow_style = HTML_CLUEFLOW_STYLE_NORMAL;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H1:
		*flow_style = HTML_CLUEFLOW_STYLE_H1;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H2:
		*flow_style = HTML_CLUEFLOW_STYLE_H2;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H3:
		*flow_style = HTML_CLUEFLOW_STYLE_H3;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H4:
		*flow_style = HTML_CLUEFLOW_STYLE_H4;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H5:
		*flow_style = HTML_CLUEFLOW_STYLE_H5;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_H6:
		*flow_style = HTML_CLUEFLOW_STYLE_H6;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_ADDRESS:
		*flow_style = HTML_CLUEFLOW_STYLE_ADDRESS;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_PRE:
		*flow_style = HTML_CLUEFLOW_STYLE_PRE;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED:
		*item_type = HTML_LIST_TYPE_UNORDERED;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN:
		*item_type = HTML_LIST_TYPE_ORDERED_UPPER_ROMAN;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMALPHA:
		*item_type = HTML_LIST_TYPE_ORDERED_UPPER_ALPHA;
		break;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT:
		*item_type = HTML_LIST_TYPE_ORDERED_ARABIC;
		break;
	default:		/* This should not really happen, though.  */
		*flow_style = HTML_CLUEFLOW_STYLE_NORMAL;
	}
}

HTMLHAlignType
paragraph_alignment_to_html (GtkHTMLParagraphAlignment alignment)
{
	switch (alignment) {
	case GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT:
		return HTML_HALIGN_LEFT;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT:
		return HTML_HALIGN_RIGHT;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER:
		return HTML_HALIGN_CENTER;
	default:
		return HTML_HALIGN_LEFT;
	}
}

GtkHTMLParagraphAlignment
html_alignment_to_paragraph (HTMLHAlignType alignment)
{
	switch (alignment) {
	case HTML_HALIGN_LEFT:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	case HTML_HALIGN_CENTER:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER;
	case HTML_HALIGN_RIGHT:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT;
	default:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	}
}

void
gtk_html_update_styles (GtkHTML *html)
{
	GtkHTMLParagraphStyle paragraph_style;
	GtkHTMLParagraphAlignment alignment;
	HTMLEngine *engine;
	HTMLClueFlowStyle flow_style;
	HTMLListType item_type;
	guint indentation;

	/* printf ("gtk_html_update_styles called\n"); */

	if (! html_engine_get_editable (html->engine))
		return;

	engine          = html->engine;
	html_engine_get_current_clueflow_style (engine, &flow_style, &item_type);
	paragraph_style = clueflow_style_to_paragraph_style (flow_style, item_type);

	if (paragraph_style != html->priv->paragraph_style) {
		html->priv->paragraph_style = paragraph_style;
		g_signal_emit (html, signals [CURRENT_PARAGRAPH_STYLE_CHANGED], 0, paragraph_style);
	}

	indentation = html_engine_get_current_clueflow_indentation (engine);
	if (indentation != html->priv->paragraph_indentation) {
		html->priv->paragraph_indentation = indentation;
		g_signal_emit (html, signals [CURRENT_PARAGRAPH_INDENTATION_CHANGED], 0, indentation);
	}

	alignment = html_alignment_to_paragraph (html_engine_get_current_clueflow_alignment (engine));
	if (alignment != html->priv->paragraph_alignment) {
		html->priv->paragraph_alignment = alignment;
		g_signal_emit (html, signals [CURRENT_PARAGRAPH_ALIGNMENT_CHANGED], 0, alignment);
	}

	if (html_engine_update_insertion_font_style (engine))
		g_signal_emit (html, signals [INSERTION_FONT_STYLE_CHANGED], 0, engine->insertion_font_style);
	if (html_engine_update_insertion_color (engine))
		g_signal_emit (html, signals [INSERTION_COLOR_CHANGED], 0, engine->insertion_color);

	/* TODO add insertion_url_or_targed_changed signal */
	html_engine_update_insertion_url_and_target (engine);
}


/* GTK+ idle loop handler.  */

static gint
idle_handler (gpointer data)
{
	GtkHTML *html;
	HTMLEngine *engine;
	gboolean also_update_cursor;

	html = GTK_HTML (data);
	engine = html->engine;

	also_update_cursor = any_has_cursor_moved (html) || !any_has_skip_update_cursor (html);

	if (html->engine->thaw_idle_id == 0 && !html_engine_frozen (html->engine))
		html_engine_flush_draw_queue (engine);

	if (also_update_cursor)
		gtk_html_adjust_cursor_position (html);

	html->priv->idle_handler_id = 0;
	html->priv->skip_update_cursor = FALSE;
	html->priv->cursor_moved = FALSE;

	while (html->iframe_parent) {
		html = GTK_HTML (html->iframe_parent);

		if (html) {
			html->priv->skip_update_cursor = FALSE;
			html->priv->cursor_moved = FALSE;
		}

		if (also_update_cursor)
			gtk_html_adjust_cursor_position (html);
	}

	return FALSE;
}

static void
gtk_html_adjust_cursor_position (GtkHTML *html)
{
	HTMLEngine *e;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	e = html->engine;

	if (html->priv->scroll_timeout_id == 0  &&
	    html->engine->thaw_idle_id == 0  &&
	    !html_engine_frozen (html->engine))
		html_engine_make_cursor_visible (e);

	hadjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (html));
	vadjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));

	gtk_adjustment_set_value (hadjustment, (gfloat) e->x_offset);
	gtk_adjustment_set_value (vadjustment, (gfloat) e->y_offset);
	gtk_html_private_calc_scrollbars (html, NULL, NULL);

}

static void
queue_draw (GtkHTML *html)
{
	if (html->priv->idle_handler_id == 0)
		html->priv->idle_handler_id = g_idle_add (idle_handler, html);
}


/* HTMLEngine callbacks.  */

static void
html_engine_title_changed_cb (HTMLEngine *engine, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	g_signal_emit (gtk_html, signals [TITLE_CHANGED], 0, engine->title);
}

static void
html_engine_set_base_cb (HTMLEngine *engine, const gchar *base, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_html_set_base (gtk_html, base);
	g_signal_emit (gtk_html, signals[SET_BASE], 0, base);
}

static void
html_engine_set_base_target_cb (HTMLEngine *engine, const gchar *base_target, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	g_signal_emit (gtk_html, signals[SET_BASE_TARGET], 0, base_target);
}

static void
html_engine_load_done_cb (HTMLEngine *engine, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	g_signal_emit (gtk_html, signals[LOAD_DONE], 0);
}

static void
html_engine_url_requested_cb (HTMLEngine *engine,
			      const gchar *url,
			      GtkHTMLStream *handle,
			      gpointer data)
{
	GtkHTML *gtk_html;
	gchar *expanded = NULL;

	if (!url)
		return;

	if (engine->stopped)
		return;

	gtk_html = GTK_HTML (data);

	expanded = gtk_html_get_url_base_relative (gtk_html, url);
	g_signal_emit (gtk_html, signals[URL_REQUESTED], 0, expanded, handle);
	g_free (expanded);
}

static void
html_engine_draw_pending_cb (HTMLEngine *engine,
			     gpointer data)
{
	GtkHTML *html;

	html = GTK_HTML (data);
	html->priv->skip_update_cursor = TRUE;
	queue_draw (html);
}

static void
html_engine_redirect_cb (HTMLEngine *engine,
			 const gchar *url,
			 gint delay,
			 gpointer data)
{
	GtkHTML *gtk_html;
	gchar *expanded = NULL;

	if (!url)
		return;

	gtk_html = GTK_HTML (data);
	expanded = gtk_html_get_url_base_relative (gtk_html, url);
	g_signal_emit (gtk_html, signals[REDIRECT], 0, expanded, delay);
	g_free (expanded);
}

static void
html_engine_submit_cb (HTMLEngine *engine,
		       const gchar *method,
		       const gchar *url,
		       const gchar *encoding,
		       gpointer data)
{
	GtkHTML *gtk_html;
	gchar *expanded = NULL;

	if (!url)
		return;

	gtk_html = GTK_HTML (data);

	expanded = gtk_html_get_url_base_relative (gtk_html, url);
	g_signal_emit (gtk_html, signals[SUBMIT], 0, method, expanded, encoding);
	g_free (expanded);
}

static gboolean
html_engine_object_requested_cb (HTMLEngine *engine,
		       GtkHTMLEmbedded *eb,
		       gpointer data)
{
	GtkHTML *gtk_html;
	gboolean object_found = FALSE;

	gtk_html = GTK_HTML (data);

	object_found = FALSE;
	g_signal_emit (gtk_html, signals[OBJECT_REQUESTED], 0, eb, &object_found);
	return object_found;
}


/* GtkAdjustment handling.  */

static void
scroll_update_mouse (GtkWidget *widget)
{
	GdkWindow *window;
	GdkWindow *bin_window;
	gint x, y;

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	window = gtk_widget_get_window (widget);
	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

	gdk_window_get_pointer (bin_window, &x, &y, NULL);
	mouse_change_pos (widget, window, x, y, 0);
}

static void
vertical_scroll_cb (GtkAdjustment *adjustment, gpointer data)
{
	GtkHTML *html = GTK_HTML (data);
	gdouble page_increment;
	gdouble value;

	value = gtk_adjustment_get_value (adjustment);
	page_increment = gtk_adjustment_get_page_increment (adjustment);

	/* check if adjustment is valid, it's changed in
	   Layout::size_allocate and we can't do anything about it,
	   because it uses private fields we cannot access, so we have
	   to use it*/
	if (html->engine->keep_scroll || html->engine->height != page_increment)
		return;

	html->engine->y_offset = (gint) value;
	scroll_update_mouse (GTK_WIDGET (data));
}

static void
horizontal_scroll_cb (GtkAdjustment *adjustment, gpointer data)
{
	GtkHTML *html = GTK_HTML (data);
	gdouble page_increment;
	gdouble value;

	value = gtk_adjustment_get_value (adjustment);
	page_increment = gtk_adjustment_get_page_increment (adjustment);

	/* check if adjustment is valid, it's changed in
	   Layout::size_allocate and we can't do anything about it,
	   because it uses private fields we cannot access, so we have
	   to use it*/
	if (html->engine->keep_scroll || html->engine->width != page_increment)
		return;

	html->engine->x_offset = (gint) value;
	scroll_update_mouse (GTK_WIDGET (data));
}

static void
connect_adjustments (GtkHTML *html,
		     GtkAdjustment *hadj,
		     GtkAdjustment *vadj)
{
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	hadjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (html));
	vadjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));

	if (html->hadj_connection != 0)
		g_signal_handler_disconnect (hadjustment, html->hadj_connection);

	if (html->vadj_connection != 0)
		g_signal_handler_disconnect (vadjustment, html->vadj_connection);

	if (vadj != NULL)
		html->vadj_connection =
			g_signal_connect (vadj, "value_changed", G_CALLBACK (vertical_scroll_cb), (gpointer) html);
	else
		html->vadj_connection = 0;

	if (hadj != NULL)
		html->hadj_connection =
			g_signal_connect (hadj, "value_changed", G_CALLBACK (horizontal_scroll_cb), (gpointer) html);
	else
		html->hadj_connection = 0;
}


/* Scroll timeout handling.  */

static void
inc_adjustment (GtkAdjustment *adj, gint doc_width, gint alloc_width, gint inc)
{
	gfloat value;
	gint max;

	value = gtk_adjustment_get_value (adj) + (gfloat) inc;

	if (doc_width > alloc_width)
		max = doc_width - alloc_width;
	else
		max = 0;

	if (value > (gfloat) max)
		value = (gfloat) max;
	else if (value < 0)
		value = 0.0;

	gtk_adjustment_set_value (adj, value);
}

static gint
scroll_timeout_cb (gpointer data)
{
	GtkWidget *widget;
	GdkWindow *window;
	GtkHTML *html;
	HTMLEngine *engine;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gint x_scroll, y_scroll;
	gint x, y;

	GDK_THREADS_ENTER ();

	widget = GTK_WIDGET (data);
	html = GTK_HTML (data);
	engine = html->engine;

	window = gtk_widget_get_window (widget);
	gdk_window_get_pointer (window, &x, &y, NULL);

	if (x < 0) {
		x_scroll = x;
		if (x + engine->x_offset >= 0)
			x = 0;
	} else if (x >= widget->allocation.width) {
		x_scroll = x - widget->allocation.width + 1;
		x = widget->allocation.width;
	} else {
		x_scroll = 0;
	}
	x_scroll /= 2;

	if (y < 0) {
		y_scroll = y;
		if (y + engine->y_offset >= 0)
			y = 0;
	} else if (y >= widget->allocation.height) {
		y_scroll = y - widget->allocation.height + 1;
		y = widget->allocation.height;
	} else {
		y_scroll = 0;
	}
	y_scroll /= 2;

	if (html->in_selection && (x_scroll != 0 || y_scroll != 0))
		html_engine_select_region (engine, html->selection_x1, html->selection_y1,
					   x + engine->x_offset, y + engine->y_offset);

	hadjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (widget));
	vadjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (widget));

	inc_adjustment (hadjustment, html_engine_get_doc_width (html->engine),
			widget->allocation.width, x_scroll);
	inc_adjustment (vadjustment, html_engine_get_doc_height (html->engine),
			widget->allocation.height, y_scroll);

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
setup_scroll_timeout (GtkHTML *html)
{
	if (html->priv->scroll_timeout_id != 0)
		return;

	html->priv->scroll_timeout_id = g_timeout_add (SCROLL_TIMEOUT_INTERVAL,
						   scroll_timeout_cb, html);

	GDK_THREADS_LEAVE();
	scroll_timeout_cb (html);
	GDK_THREADS_ENTER();
}

static void
remove_scroll_timeout (GtkHTML *html)
{
	if (html->priv->scroll_timeout_id == 0)
		return;

	g_source_remove (html->priv->scroll_timeout_id);
	html->priv->scroll_timeout_id = 0;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GtkHTML *html;

	html = GTK_HTML (object);

	g_free (html->pointer_url);
	html->pointer_url = NULL;

	if (html->hand_cursor) {
		gdk_cursor_unref (html->hand_cursor);
		html->hand_cursor = NULL;
	}

	if (html->ibeam_cursor) {
		gdk_cursor_unref (html->ibeam_cursor);
		html->ibeam_cursor = NULL;
	}

	connect_adjustments (html, NULL, NULL);

	if (html->priv) {
		if (html->priv->idle_handler_id != 0) {
			g_source_remove (html->priv->idle_handler_id);
			html->priv->idle_handler_id = 0;
		}

		if (html->priv->scroll_timeout_id != 0) {
			g_source_remove (html->priv->scroll_timeout_id);
			html->priv->scroll_timeout_id = 0;
		}

#ifdef HAVE_GCONF
		if (html->priv->notify_monospace_font_id) {
			gconf_client_notify_remove (
				gconf_client_get_default (),
				html->priv->notify_monospace_font_id);
			html->priv->notify_monospace_font_id = 0;
		}
#endif

		if (html->priv->resize_cursor) {
			gdk_cursor_unref (html->priv->resize_cursor);
			html->priv->resize_cursor = NULL;
		}

		if (html->priv->im_context) {
			g_object_unref (html->priv->im_context);
			html->priv->im_context = NULL;
		}

		g_free (html->priv->caret_first_focus_anchor);
		g_free (html->priv);
		html->priv = NULL;
	}

	if (html->engine) {
		g_object_unref (G_OBJECT (html->engine));
		html->engine = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkHTML *
gtk_html_get_top_html (GtkHTML *html)
{
	while (html->iframe_parent)
		html = GTK_HTML (html->iframe_parent);

	return html;
}

static cairo_font_options_t *
get_font_options (void)
{
#ifdef HAVE_GCONF
	gchar *antialiasing, *hinting, *subpixel_order;
	GConfClient *gconf = gconf_client_get_default ();
#endif
	cairo_font_options_t *font_options = cairo_font_options_create ();
#ifdef HAVE_GCONF

	/* Antialiasing */
	antialiasing = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/antialiasing", NULL);
	if (antialiasing == NULL) {
#endif
		cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);
#ifdef HAVE_GCONF
	} else {
		if (strcmp (antialiasing, "grayscale") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_GRAY);
		else if (strcmp (antialiasing, "rgba") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_SUBPIXEL);
		else if (strcmp (antialiasing, "none") == 0)
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_NONE);
		else
			cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_DEFAULT);
	}
	hinting = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/hinting", NULL);
	if (hinting == NULL) {
#endif
		cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);
#ifdef HAVE_GCONF
	} else {
		if (strcmp (hinting, "full") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_FULL);
		else if (strcmp (hinting, "medium") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_MEDIUM);
		else if (strcmp (hinting, "slight") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_SLIGHT);
		else if (strcmp (hinting, "none") == 0)
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
		else
			cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_DEFAULT);
	}
	subpixel_order = gconf_client_get_string (gconf,
			"/desktop/gnome/font_rendering/rgba_order", NULL);
	if (subpixel_order == NULL) {
#endif
		cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
#ifdef HAVE_GCONF
	} else {
		if (strcmp (subpixel_order, "rgb") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_RGB);
		else if (strcmp (subpixel_order, "bgr") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_BGR);
		else if (strcmp (subpixel_order, "vrgb") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VRGB);
		else if (strcmp (subpixel_order, "vbgr") == 0)
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_VBGR);
		else
			cairo_font_options_set_subpixel_order (font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
	}
	g_free (antialiasing);
	g_free (hinting);
	g_free (subpixel_order);
	g_object_unref (gconf);
#endif
	return font_options;
}

void
gtk_html_set_fonts (GtkHTML *html, HTMLPainter *painter)
{
	GtkWidget *top_level;
	GtkStyle *style;
	PangoFontDescription *fixed_desc = NULL;
	gchar *fixed_name = NULL;
	const gchar *fixed_family = NULL;
	gint  fixed_size = 0;
	gboolean  fixed_points = FALSE;
	const gchar *font_var = NULL;
	gint  font_var_size = 0;
	gboolean  font_var_points = FALSE;
	cairo_font_options_t *font_options;

	top_level = GTK_WIDGET (gtk_html_get_top_html (html));
	style = gtk_widget_get_style (top_level);

	font_var = pango_font_description_get_family (style->font_desc);
	font_var_size = pango_font_description_get_size (style->font_desc);
	font_var_points = !pango_font_description_get_size_is_absolute (style->font_desc);

	gtk_widget_style_get (GTK_WIDGET (top_level), "fixed_font_name", &fixed_name, NULL);
	if (fixed_name) {
		fixed_desc = pango_font_description_from_string (fixed_name);
		if (pango_font_description_get_family (fixed_desc)) {
			fixed_size = pango_font_description_get_size (fixed_desc);
			fixed_points = !pango_font_description_get_size_is_absolute (fixed_desc);
			fixed_family = pango_font_description_get_family (fixed_desc);
		} else {
			g_free (fixed_name);
			fixed_name = NULL;
		}
	}

#ifdef HAVE_GCONF
	if (!fixed_name) {
		GConfClient *gconf;

		gconf = gconf_client_get_default ();
		fixed_name = gconf_client_get_string (gconf, "/desktop/gnome/interface/monospace_font_name", NULL);
		if (fixed_name) {
			fixed_desc = pango_font_description_from_string (fixed_name);
			if (fixed_desc) {
				fixed_size = pango_font_description_get_size (fixed_desc);
				fixed_points = !pango_font_description_get_size_is_absolute (fixed_desc);
				fixed_family = pango_font_description_get_family (fixed_desc);
			} else {
				g_free (fixed_name);
				fixed_name = NULL;
			}
		}
		g_object_unref (gconf);
	}
#endif

	if (!fixed_name) {
		fixed_family = "Monospace";
		fixed_size = font_var_size;
	}

	html_font_manager_set_default (&painter->font_manager,
				       (gchar *)font_var, (gchar *)fixed_family,
				       font_var_size, font_var_points,
				       fixed_size, fixed_points);
	if (fixed_desc)
		pango_font_description_free (fixed_desc);

	font_options = get_font_options ();
	pango_cairo_context_set_font_options (painter->pango_context, font_options);
	cairo_font_options_destroy (font_options);

	g_free (fixed_name);
}

static void
set_caret_mode(HTMLEngine *engine, gboolean caret_mode)
{
	if (engine->editable)
		return;

	if (!caret_mode && engine->blinking_timer_id)
		html_engine_stop_blinking_cursor (engine);

	engine->caret_mode = caret_mode;

	if (caret_mode && !engine->parsing && !engine->timerId == 0)
		gtk_html_edit_make_cursor_visible(engine->widget);

	/* Normally, blink cursor handler is setup in focus in event.
	 * However, in the case focus already in this engine, and user
	 * type F7 to enable cursor, we must setup the handler by
	 * ourselves.
	 */
	if (caret_mode && !engine->blinking_timer_id && engine->have_focus)
		html_engine_setup_blinking_cursor (engine);

	return;
}

/* GtkWidget methods.  */
static void
style_set (GtkWidget *widget, GtkStyle  *previous_style)
{
	HTMLEngine *engine = GTK_HTML (widget)->engine;

	/* we don't need to set font's in idle time so call idle callback directly to avoid
	   recalculating whole document
	*/
	if (engine) {
		gtk_html_set_fonts (GTK_HTML (widget), engine->painter);
		html_engine_refresh_fonts (engine);
	}

	html_colorset_set_style (engine->defaultSettings->color_set, widget);
	html_colorset_set_unchanged (engine->settings->color_set,
				     engine->defaultSettings->color_set);

	/* link and quotation colors might changed => need to refresh pango info in text objects */
	if (engine->clue)
		html_object_change_set_down (engine->clue, HTML_CHANGE_RECALC_PI);
	html_engine_schedule_update (engine);
}

static void
update_mouse_cursor (GtkWidget *widget, guint state)
{
	GdkEventMotion event;

	/* a bit hacky here */
	memset (&event, 0, sizeof (GdkEventMotion));
	event.type = GDK_MOTION_NOTIFY;
	event.window = gtk_widget_get_window (widget);
	event.send_event = FALSE;
	event.state = state;

	motion_notify_event (widget, &event);
}

static void
link_clicked(GtkHTML *gtk_html, gchar *url)
{
	gchar *expanded = NULL;

	if (!url)
		return;

	expanded = gtk_html_get_url_base_relative (gtk_html, url);
	g_signal_emit (gtk_html, signals [LINK_CLICKED], 0, expanded);
	g_free (expanded);
}

static gint
key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	GtkHTML *html = GTK_HTML (widget);
#ifdef HAVE_GCONF
	GtkHTMLClass *html_class = GTK_HTML_CLASS (GTK_WIDGET_GET_CLASS (html));
#endif
	gboolean retval = FALSE, update = TRUE;
	HTMLObject *focus_object;
	gint focus_object_offset;
	gboolean url_test_mode;

	html->binding_handled = FALSE;
	html->priv->update_styles = FALSE;
	html->priv->event_time = event->time;

	if ((event->keyval == GDK_Control_L || event->keyval == GDK_Control_R)
	    && html_engine_get_editable (html->engine))
		url_test_mode = TRUE;
	else
		url_test_mode = FALSE;

	if (html->priv->in_url_test_mode != url_test_mode) {
		html->priv->in_url_test_mode = url_test_mode;
		update_mouse_cursor (widget, event->state);
	}

	if (html_engine_get_editable (html->engine)) {
		if (gtk_im_context_filter_keypress (html->priv->im_context, event)) {
			html_engine_reset_blinking_cursor (html->engine);
			html->priv->need_im_reset = TRUE;
			return TRUE;
		}
	}
	
#ifdef HAVE_GCONF
	if (html_class->use_emacs_bindings && html_class->emacs_bindings && !html->binding_handled)
		gtk_binding_set_activate (html_class->emacs_bindings, event->keyval, event->state, GTK_OBJECT (widget));
#endif

	if (!html->binding_handled) {
		html->priv->in_key_binding = TRUE;
		retval = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
		html->priv->in_key_binding = FALSE;
	}

	retval = retval || html->binding_handled;
	update = html->priv->update_styles;

	if (retval && update)
		gtk_html_update_styles (html);

	html->priv->event_time = 0;

	/* FIXME: use bindings */
	if (!html_engine_get_editable (html->engine)) {
		switch (event->keyval) {
		case GDK_Return:
		case GDK_KP_Enter:
			/* the toplevel gtkhtml's focus object may be a frame or ifame */
			focus_object = html_engine_get_focus_object (html->engine, &focus_object_offset);

			if (focus_object) {
				gchar *url;
				url = html_object_get_complete_url (focus_object, focus_object_offset);
				if (url) {
					/* printf ("link clicked: %s\n", url); */
                                        if (HTML_IS_TEXT(focus_object)) {
						html_text_set_link_visited (HTML_TEXT (focus_object), focus_object_offset, html->engine, TRUE);
					link_clicked (html, url);
					}
					g_free (url);
				}
			}
			break;
		default:
			;
		}
	}

	if (retval && (html_engine_get_editable (html->engine) || html->engine->caret_mode))
		html_engine_reset_blinking_cursor (html->engine);

	/* printf ("retval: %d\n", retval); */

	return retval;
}

static gint
key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	GtkHTML *html = GTK_HTML (widget);

	if (html->priv->in_url_test_mode) {
		html->priv->in_url_test_mode = FALSE;
		update_mouse_cursor (widget, event->state);
	}

	if (!html->binding_handled && html_engine_get_editable (html->engine)) {
		if (gtk_im_context_filter_keypress (html->priv->im_context, event)) {
			html->priv->need_im_reset = TRUE;
			return TRUE;
		}
	}

	return GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);
}

void
gtk_html_drag_dest_set (GtkHTML *html)
{
	if (html_engine_get_editable (html->engine))
		gtk_drag_dest_set (GTK_WIDGET (html), GTK_DEST_DEFAULT_ALL,
				   dnd_link_sources, DND_LINK_SOURCES, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	else
		gtk_drag_dest_unset (GTK_WIDGET (html));
}

static void
realize (GtkWidget *widget)
{
	GtkHTML *html;
	GtkStyle *style;
	GdkWindow *window;
	GdkWindow *bin_window;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));

	html = GTK_HTML (widget);
	hadjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (widget));
	vadjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (widget));

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	window = gtk_widget_get_window (widget);
	bin_window = gtk_layout_get_bin_window (&html->layout);

	style = gtk_widget_get_style (widget);
	style = gtk_style_attach (style, window);
	gtk_widget_set_style (widget, style);

	gdk_window_set_events (bin_window,
			       (gdk_window_get_events (bin_window)
				| GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK
				| GDK_ENTER_NOTIFY_MASK
				| GDK_BUTTON_PRESS_MASK
				| GDK_BUTTON_RELEASE_MASK
				| GDK_VISIBILITY_NOTIFY_MASK
				| GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK));

	html_engine_realize (html->engine, bin_window);

	gdk_window_set_cursor (window, NULL);

	/* This sets the backing pixmap to None, so that scrolling does not
           erase the newly exposed area, thus making the thing smoother.  */
	gdk_window_set_back_pixmap (bin_window, NULL, FALSE);

	/* If someone was silly enough to stick us in something that doesn't
	 * have adjustments, go ahead and create them now since we expect them
	 * and love them and pat them
	 */
	if (hadjustment == NULL) {
		hadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
		gtk_layout_set_hadjustment (GTK_LAYOUT (widget), hadjustment);
	}

	if (vadjustment == NULL) {
		vadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
		gtk_layout_set_vadjustment (GTK_LAYOUT (widget), vadjustment);
	}

	gtk_html_drag_dest_set (html);

	gtk_im_context_set_client_window (html->priv->im_context, window);

	html_image_factory_start_animations (html->engine->image_factory);
}

static void
unrealize (GtkWidget *widget)
{
	GtkHTML *html = GTK_HTML (widget);

	html_engine_unrealize (html->engine);

	gtk_im_context_set_client_window (html->priv->im_context, NULL);

	html_image_factory_stop_animations (html->engine->image_factory);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gboolean
expose (GtkWidget *widget, GdkEventExpose *event)
{
	/* printf ("expose x: %d y: %d\n", GTK_HTML (widget)->engine->x_offset, GTK_HTML (widget)->engine->y_offset); */

	html_engine_expose (GTK_HTML (widget)->engine, event);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
	/* printf ("expose END\n"); */

	return FALSE;
}

static void
gtk_html_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	HTMLEngine *e = GTK_HTML (widget)->engine;
	if (!e->writing) {
		gint old_width, old_height;

		old_width = e->width;
		old_height = e->height;
		e->width = requisition->width;
		e->height = requisition->height;
		html_engine_calc_size (e, NULL);
		requisition->width = html_engine_get_doc_width (e);
		requisition->height = html_engine_get_doc_height (e);
		e->width = old_width;
		e->height = old_height;
		html_engine_calc_size (e, NULL);
	} else {
		requisition->width = html_engine_get_doc_width (e);
		requisition->height = html_engine_get_doc_height (e);
	}
}

static void
child_size_allocate (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (html_object_is_embedded (o)) {
		HTMLEmbedded *eo = HTML_EMBEDDED (o);

		if (eo->widget) {
			GtkAllocation allocation;

			html_object_calc_abs_position_in_frame (o, &allocation.x, &allocation.y);
			allocation.y -= o->ascent;
			allocation.width = o->width;
			allocation.height = o->ascent + o->descent;
			gtk_widget_size_allocate (eo->widget, &allocation);
		}
	}
}

static void
size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkHTML *html;
	GtkLayout *layout;
	gboolean changed_x = FALSE, changed_y = FALSE;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));
	g_return_if_fail (allocation != NULL);

	html = GTK_HTML (widget);
	layout = GTK_LAYOUT (widget);

	/* isolate childs from layout - we want to set them after calc size is performed
	   and we know the children positions */
	children = GTK_LAYOUT (widget)->children;
	GTK_LAYOUT (widget)->children = NULL;
	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);
	GTK_LAYOUT (widget)->children = children;

	if (html->engine->width != allocation->width
	    || html->engine->height != allocation->height) {
		HTMLEngine *e = html->engine;
		gint old_doc_width, old_doc_height, old_width, old_height;

		old_doc_width = html_engine_get_doc_width (html->engine);
		old_doc_height = html_engine_get_doc_height (html->engine);
		old_width = e->width;
		old_height = e->height;

		e->width  = allocation->width;
		e->height = allocation->height;

		html_engine_calc_size (html->engine, NULL);
		gtk_html_update_scrollbars_on_resize (html, old_doc_width, old_doc_height, old_width, old_height,
						      &changed_x, &changed_y);
	}

	if (!html->engine->keep_scroll) {
		GtkAdjustment *adjustment;

		gtk_html_private_calc_scrollbars (html, &changed_x, &changed_y);

		if (changed_x) {
			adjustment = gtk_layout_get_hadjustment (layout);
			gtk_adjustment_value_changed (adjustment);
		}
		if (changed_y) {
			adjustment = gtk_layout_get_vadjustment (layout);
			gtk_adjustment_value_changed (adjustment);
		}
	}

	if (html->engine->clue)
		html_object_forall (html->engine->clue, html->engine, child_size_allocate, NULL);
}

static void
set_pointer_url (GtkHTML *html, const gchar *url)
{
	if (url == html->pointer_url)
		return;

	if (url && html->pointer_url && !strcmp (url, html->pointer_url))
		return;

	g_free (html->pointer_url);
	html->pointer_url = url ? g_strdup (url) : NULL;
	g_signal_emit (html,  signals[ON_URL], 0, html->pointer_url);
}

static void
dnd_link_set (GtkWidget *widget, HTMLObject *o, gint offset)
{
	if (!html_engine_get_editable (GTK_HTML (widget)->engine)) {
		/* printf ("dnd_link_set %p\n", o); */

		gtk_drag_source_set (widget, GDK_BUTTON1_MASK,
				     dnd_link_sources, DND_LINK_SOURCES,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		GTK_HTML (widget)->priv->dnd_object = o;
		GTK_HTML (widget)->priv->dnd_object_offset = offset;
	}
}

static void
dnd_link_unset (GtkWidget *widget)
{
	if (!html_engine_get_editable (GTK_HTML (widget)->engine)) {
		/* printf ("dnd_link_unset\n"); */

		gtk_drag_source_unset (widget);
		GTK_HTML (widget)->priv->dnd_object = NULL;
	}
}

static void
on_object (GtkWidget *widget, GdkWindow *window, HTMLObject *obj, gint offset, gint x, gint y)
{
	GtkHTML *html = GTK_HTML (widget);

	if (obj) {
		gchar *url;

		if (gtk_html_get_editable (html)) {
			if (HTML_IS_IMAGE (obj)) {
				gint ox, oy;

				html_object_calc_abs_position (obj, &ox, &oy);
				if (ox + obj->width - 5 <= x && oy + obj->descent - 5 <= y) {
					gdk_window_set_cursor (window, html->priv->resize_cursor);

					return;
				}
			}
		}

		url = gtk_html_get_url_object_relative (html, obj,
							html_object_get_url (obj, offset));
		if (url != NULL) {
			set_pointer_url (html, url);
			dnd_link_set (widget, obj, offset);

			if (html->engine->editable && !html->priv->in_url_test_mode)
				gdk_window_set_cursor (window, html->ibeam_cursor);
			else {
				gdk_window_set_cursor (window, html->hand_cursor);
			}
		} else {
			set_pointer_url (html, NULL);
			dnd_link_unset (widget);

			if (html_object_is_text (obj) && html->allow_selection)
				gdk_window_set_cursor (window, html->ibeam_cursor);
			else
				gdk_window_set_cursor (window, NULL);
		}

		g_free (url);
	} else {
		set_pointer_url (html, NULL);
		dnd_link_unset (widget);

		gdk_window_set_cursor (window, NULL);
	}
}

#define HTML_DIST(x,y) sqrt(x*x + y*y)

static gint
mouse_change_pos (GtkWidget *widget, GdkWindow *window, gint x, gint y, gint state)
{
	GtkHTML *html;
	HTMLEngine *engine;
	HTMLObject *obj;
	HTMLType type;
	gint offset;

	if (!GTK_WIDGET_REALIZED (widget))
		return FALSE;

	html   = GTK_HTML (widget);
	engine = html->engine;
	obj    = html_engine_get_object_at (engine, x, y, (guint *) &offset, FALSE);

	if ((html->in_selection || html->in_selection_drag) && html->allow_selection) {
		gboolean need_scroll;

		if (obj) {
			type = HTML_OBJECT_TYPE (obj);

			/* FIXME this is broken */

			if (type == HTML_TYPE_BUTTON ||
			    type ==  HTML_TYPE_CHECKBOX ||
			    type ==  HTML_TYPE_EMBEDDED ||
			    type ==  HTML_TYPE_HIDDEN ||
			    type ==  HTML_TYPE_IMAGEINPUT ||
			    type ==  HTML_TYPE_RADIO ||
			    type ==  HTML_TYPE_SELECT ||
			    type ==  HTML_TYPE_TEXTAREA ||
			    type ==  HTML_TYPE_TEXTINPUT ) {
				return FALSE;
			}
		}

		if (HTML_DIST ((x - html->selection_x1), (y  - html->selection_y1))
		    > html_painter_get_space_width (engine->painter, GTK_HTML_FONT_STYLE_SIZE_3, NULL)) {
			html->in_selection = TRUE;
			html->in_selection_drag = TRUE;
		}

		need_scroll = FALSE;

		if (x < html->engine->x_offset) {
			need_scroll = TRUE;
		} else if (x >= widget->allocation.width) {
			need_scroll = TRUE;
		}

		if (y < html->engine->y_offset) {
			need_scroll = TRUE;
		} else if (y >= widget->allocation.height) {
			need_scroll = TRUE;
		}

		if (need_scroll)
			setup_scroll_timeout (html);
		else
			remove_scroll_timeout (html);

		/* This will put the mark at the position of the
                   previous click.  */
		if (engine->mark == NULL && engine->editable)
			html_engine_set_mark (engine);

		html_engine_select_region (engine, html->selection_x1, html->selection_y1, x, y);
	}

	if (html->priv->in_object_resize) {
		HTMLObject *o = html->priv->resize_object;
		gint ox, oy;

		html_object_calc_abs_position (o, &ox, &oy);
		oy -= o->ascent;
		g_assert (HTML_IS_IMAGE (o));
		if (x > ox && y > oy) {
			gint w, h;

			w = x - ox;
			h = y - oy;
			if (!(state & GDK_SHIFT_MASK)) {
				w = MAX (w, h);
				h = -1;
			}
			html_image_set_size (HTML_IMAGE (o), w, h, FALSE, FALSE);
		}
	} else
		on_object (widget, window, obj, offset, x, y);

	return TRUE;
}

static const gchar *
skip_host (const gchar *url)
{
	const gchar *host;

	host = url;
	while (*host && (*host != '/') && (*host != ':'))
	       host++;

	if (*host == ':') {
		host++;

		if (*host == '/')
			host++;

		url = host;

		if (*host == '/') {
			host++;

			while (*host && (*host != '/'))
				host++;

			url = host;
		}
	}

	return url;
}

/* length path in uri*/
static gsize
full_path_len (const gchar *base)
{
	/*skip '?' and '#'*/
	return strcspn(base, "?#");

}

static gsize
current_path_len (const gchar *base, gboolean absolute)
{
	const gchar *last;
	const gchar *start;

	start = last = skip_host (base);

	if (absolute) {
		return last - base;
	} else {
		gsize skip;

		skip = full_path_len(base);

		while (*start  != '\0') {
			start = strchr (start, '/');
			/* '/' not exist */
			if (!start)
				return last - base;
			/* '/' after '#' or '?' */
			if ((start - base) > skip)
				return last - base;
			last = start;
			start++;
		}

		if (start)
			last = start;
	}

	return last - base;
}

#if 0
gchar *
collapse_path (gchar *url)
{
	gchar *start;
	gchar *end;
	gchar *cur;
	gsize len;

	start = skip_host (url);

	cur = start;
	while ((cur = strstr (cur, "/../"))) {
		end = cur + 3;

		/* handle the case of a rootlevel /../ specialy */
		if (cur == start) {
			len = strlen (end);
			memmove (cur, end, len + 1);
		}

		while (cur > start) {
			cur--;
			if ((*cur == '/') || (cur == start)) {
				len = strlen (end);
				memmove (cur, end, len + 1);
				break;
			}
		}
	}
	return url;
}
#endif

static gboolean
url_is_absolute (const gchar *url)
{
	/*
	  URI Syntactic Components

	  The URI syntax is dependent upon the scheme.  In general, absolute
	  URI are written as follows:

	  <scheme>:<scheme-specific-part>

	  scheme        = alpha *( alpha | digit | "+" | "-" | "." )
	*/

	if (!url)
		return FALSE;

	if (!isalpha (*url))
		return FALSE;
	url ++;

	while (*url && (isalnum (*url) || *url == '+' || *url == '-' || *url == '.'))
		url ++;

	return *url && *url == ':';
}

static gchar *
expand_relative (const gchar *base, const gchar *url)
{
	gchar *new_url = NULL;
	gsize base_len, url_len;

	if (!base || url_is_absolute (url)) {
		return g_strdup (url);
	}

	if (*url == '/') {
		base_len = current_path_len (base, TRUE);
	} else if (*url == '?' || *url == '#' ) {
		base_len = full_path_len (base);
	} else {
		base_len = current_path_len (base, FALSE);
	}

	url_len = strlen (url);

	new_url = g_malloc (base_len + url_len + 2);

	if (base_len) {
		memcpy (new_url, base, base_len);

		if (
			base[base_len - 1] != '/' &&
			*url != '/' &&
			*url != '#' &&
			*url != '?'
		)
			new_url[base_len++] = '/';
	}

	memcpy (new_url + base_len, url, url_len);
	new_url[base_len + url_len] = '\0';
	/*
	   g_warning ("path:\n\tbase = %s\n\turl = %s\n\tnew_url = %s\n",
	   base, url, new_url);
	*/
	return new_url;
}

gchar *
gtk_html_get_url_base_relative (GtkHTML *html, const gchar *url)
{
	return expand_relative (gtk_html_get_base (html), url);
}

static gchar *
expand_frame_url (GtkHTML *html, const gchar *url)
{
	gchar *new_url;

	new_url = gtk_html_get_url_base_relative (html, url);
	while (html->iframe_parent) {
		gchar *expanded;

		expanded = gtk_html_get_url_base_relative (GTK_HTML (html->iframe_parent),
						       new_url);
		g_free (new_url);
		new_url = expanded;

		html = GTK_HTML (html->iframe_parent);
	}
	return new_url;
}

gchar *
gtk_html_get_url_object_relative (GtkHTML *html, HTMLObject *o, const gchar *url)
{
	HTMLEngine *e;
	HTMLObject *parent;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	/* start at the top always */
	while (html->iframe_parent)
		html = GTK_HTML (html->iframe_parent);

	parent = o;
	while (parent->parent) {
		parent = parent->parent;
		if ((HTML_OBJECT_TYPE (parent) == HTML_TYPE_FRAME)
		    || (HTML_OBJECT_TYPE (parent) == HTML_TYPE_IFRAME))
			break;
	}

	e = html_object_get_engine (parent, html->engine);

	if (!e) {
		g_warning ("Cannot find object for url");
		return NULL;
	}

	/*
	if (e == html->engine)
		g_warning ("engine matches engine");
	*/
        return url ? expand_frame_url (e->widget, url) : NULL;
}

static GtkWidget *
shift_to_iframe_parent (GtkWidget *widget, gint *x, gint *y)
{
	while (GTK_HTML (widget)->iframe_parent) {
		if (x)
			*x += widget->allocation.x - GTK_HTML (widget)->engine->x_offset;
		if (y)
			*y += widget->allocation.y - GTK_HTML (widget)->engine->y_offset;

		widget = GTK_HTML (widget)->iframe_parent;

	}

	return widget;
}

static gint
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	GdkWindow *window;
	GdkWindow *bin_window;
	HTMLEngine *engine;
	gint x, y;

	g_return_val_if_fail (widget != NULL, 0);
	g_return_val_if_fail (GTK_IS_HTML (widget), 0);
	g_return_val_if_fail (event != NULL, 0);

	/* printf ("motion_notify_event\n"); */

	if (GTK_HTML (widget)->priv->dnd_in_progress)
		return TRUE;

	widget = shift_to_iframe_parent (widget, &x, &y);

	window = gtk_widget_get_window (widget);
	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

	gdk_window_get_pointer (bin_window, &x, &y, NULL);

	if (!mouse_change_pos (widget, window, x, y, event->state))
		return FALSE;

	engine = GTK_HTML (widget)->engine;
	if (GTK_HTML (widget)->in_selection_drag && html_engine_get_editable (engine))
		html_engine_jump_at (engine, x, y);
	return TRUE;
}

static gboolean
toplevel_unmap (GtkWidget *widget, GdkEvent *event, GtkHTML *html)
{
	html_image_factory_stop_animations (html->engine->image_factory);

	return FALSE;
}

static void
hierarchy_changed (GtkWidget *widget,
		   GtkWidget *old_toplevel)
{
	GtkWidget *toplevel;
	GtkHTMLPrivate   *priv = GTK_HTML (widget)->priv;

	if (old_toplevel && priv->toplevel_unmap_handler) {
		g_signal_handler_disconnect (old_toplevel,
					     priv->toplevel_unmap_handler);
		priv->toplevel_unmap_handler = 0;
	}

	toplevel = gtk_widget_get_toplevel (widget);

	if (gtk_widget_is_toplevel (toplevel) && priv->toplevel_unmap_handler == 0) {
		priv->toplevel_unmap_handler = g_signal_connect (G_OBJECT (toplevel), "unmap-event",
								 G_CALLBACK (toplevel_unmap), widget);
	}
}

static gint
visibility_notify_event (GtkWidget *widget,
			 GdkEventVisibility *event)
{
	if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
		html_image_factory_stop_animations (GTK_HTML (widget)->engine->image_factory);
	else
		html_image_factory_start_animations (GTK_HTML (widget)->engine->image_factory);

	return FALSE;
}

static gint
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	GtkHTML *html;
	GtkWidget *orig_widget = widget;
	HTMLEngine *engine;
	gint value, x, y;

	/* printf ("button_press_event\n"); */

	x = event->x;
	y = event->y;

	widget = shift_to_iframe_parent (widget, &x, &y);
	html   = GTK_HTML (widget);
	engine = html->engine;

	if (event->button == 1 || ((event->button == 2 || event->button == 3) && html_engine_get_editable (engine))) {
		html->priv->is_first_focus = FALSE;
		html->priv->skip_update_cursor = TRUE;
		html->priv->cursor_moved = FALSE;
		gtk_widget_grab_focus (widget);
	}

	if (event->type == GDK_BUTTON_PRESS) {
		GtkAdjustment *adjustment;
		gdouble adj_value;
		gdouble lower;
		gdouble upper;
		gdouble page_size;
		gdouble step_increment;

		adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (widget));
		adj_value = gtk_adjustment_get_value (adjustment);
		lower = gtk_adjustment_get_lower (adjustment);
		upper = gtk_adjustment_get_upper (adjustment);
		page_size = gtk_adjustment_get_page_size (adjustment);
		step_increment = gtk_adjustment_get_step_increment (adjustment);

		switch (event->button) {
		case 4:
			/* Mouse wheel scroll up.  */
			if (event->state & GDK_CONTROL_MASK)
				gtk_html_command (html, "zoom-out");
			else {
				value = adj_value - step_increment * 3;

				if (value < lower)
					value = lower;

				gtk_adjustment_set_value (adjustment, value);
			}
			return TRUE;
		case 5:
			/* Mouse wheel scroll down.  */
			if (event->state & GDK_CONTROL_MASK)
				gtk_html_command (html, "zoom-in");
			else {
				value = adj_value + step_increment * 3;

				if (value > (upper - page_size))
					value = upper - page_size;

				gtk_adjustment_set_value (adjustment, value);
			}
			return TRUE;
		case 2:
			if (html_engine_get_editable (engine)) {
				gint type;
				html_engine_disable_selection (html->engine);
				html_engine_jump_at (engine, x, y);
				gtk_html_update_styles (html);
				html->priv->selection_as_cite = event->state & GDK_SHIFT_MASK;
				type = event->state & GDK_CONTROL_MASK ? 1 : 0;
				gtk_clipboard_request_contents (
					gtk_widget_get_clipboard (GTK_WIDGET (html), GDK_SELECTION_PRIMARY),
					gdk_atom_intern (selection_targets[type].target, FALSE),
					clipboard_paste_received_cb, html);
				return TRUE;
			}
			break;
		case 1:
			html->in_selection_drag = TRUE;
			if (html_engine_get_editable (engine)) {
				HTMLObject *obj;

				obj = html_engine_get_object_at (engine, x, y, NULL, FALSE);

				if (obj && HTML_IS_IMAGE (obj)) {
					gint ox, oy;

					html_object_calc_abs_position (obj, &ox, &oy);
					if (ox + obj->width - 5 <= x && oy + obj->descent - 5 <= y) {
						html->priv->in_object_resize = TRUE;
						html->priv->resize_object = obj;
						html->in_selection_drag = FALSE;
					}
				}

				if (html->allow_selection && !html->priv->in_object_resize)
					if (!(event->state & GDK_SHIFT_MASK)
					    || (!engine->mark && event->state & GDK_SHIFT_MASK))
						html_engine_set_mark (engine);
				html_engine_jump_at (engine, x, y);
			} else {
				HTMLObject *obj;
				HTMLEngine *orig_e;
				gint offset;
				gchar *url = NULL;

				orig_e = GTK_HTML (orig_widget)->engine;
				obj = html_engine_get_object_at (engine, x, y, (guint *) &offset, FALSE);
				if (obj && ((HTML_IS_IMAGE (obj) && HTML_IMAGE (obj)->url && *HTML_IMAGE (obj)->url)
					    || (HTML_IS_TEXT (obj) && (url = html_object_get_complete_url (obj, offset))))) {
					g_free (url);
					html_engine_set_focus_object (orig_e, obj, offset);
				} else {
					html_engine_set_focus_object (orig_e, NULL, 0);
					if (orig_e->caret_mode || engine->caret_mode)
						html_engine_jump_at (engine, x, y);
				}
			}
			if (html->allow_selection && !html->priv->in_object_resize) {
				if (event->state & GDK_SHIFT_MASK)
					html_engine_select_region (engine,
								   html->selection_x1, html->selection_y1, x, y);
				else {
					GdkWindow *bin_window;

					bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
					html_engine_disable_selection (engine);
					if (gdk_pointer_grab (bin_window, FALSE,
							      (GDK_BUTTON_RELEASE_MASK
							       | GDK_BUTTON_MOTION_MASK
							       | GDK_POINTER_MOTION_HINT_MASK),
							      NULL, NULL, event->time) == 0) {
						html->selection_x1 = x;
						html->selection_y1 = y;
					}
				}
			}

			engine->selection_mode = FALSE;
			if (html_engine_get_editable (engine))
				gtk_html_update_styles (html);
			break;
		default:
			break;
		}
	} else if (event->button == 1 && html->allow_selection) {
		if (event->type == GDK_2BUTTON_PRESS) {
			html->in_selection_drag = FALSE;
			gtk_html_select_word (html);
			html->in_selection = TRUE;
		}
		else if (event->type == GDK_3BUTTON_PRESS) {
			html->in_selection_drag = FALSE;
			gtk_html_select_line (html);
			html->in_selection = TRUE;
		}
	}

	return FALSE;
}

static gboolean
any_has_cursor_moved (GtkHTML *html)
{
	while (html) {
		if (html->priv->cursor_moved)
			return TRUE;

		html = html->iframe_parent ? GTK_HTML (html->iframe_parent) : NULL;
	}

	return FALSE;
}

static gboolean
any_has_skip_update_cursor (GtkHTML *html)
{
	while (html) {
		if (html->priv->skip_update_cursor)
			return TRUE;

		html = html->iframe_parent ? GTK_HTML (html->iframe_parent) : NULL;
	}

	return FALSE;
}

static gint
button_release_event (GtkWidget *initial_widget,
		      GdkEventButton *event)
{
	GtkWidget *widget;
	GtkHTML *html;
	HTMLEngine *engine;
	gint x, y;
	HTMLObject *focus_object;
	gint focus_object_offset;

	/* printf ("button_release_event\n"); */

	x = event->x;
	y = event->y;
	widget = shift_to_iframe_parent (initial_widget, &x, &y);
	html   = GTK_HTML (widget);

	remove_scroll_timeout (html);
	gtk_grab_remove (widget);
	gdk_pointer_ungrab (event->time);

	engine =  html->engine;

	if (html->in_selection) {
		html_engine_update_selection_active_state (html->engine, html->priv->event_time);
		if (html->in_selection_drag)
			html_engine_select_region (engine, html->selection_x1, html->selection_y1,
						   x, y);
		gtk_html_update_styles (html);
		update_primary_selection (html);
		queue_draw (html);
	}

	if (event->button == 1) {

		if (html->in_selection_drag && html_engine_get_editable (engine))
			html_engine_jump_at (engine, x, y);

		html->in_selection_drag = FALSE;

		if (!html->priv->dnd_in_progress
		    && html->pointer_url != NULL && ! html->in_selection
		    && (!gtk_html_get_editable (html) || html->priv->in_url_test_mode)) {
			link_clicked (GTK_HTML (widget), html->pointer_url);
			focus_object = html_engine_get_focus_object (html->engine, &focus_object_offset);
			if (HTML_IS_TEXT(focus_object)) {
				html_text_set_link_visited (HTML_TEXT(focus_object), focus_object_offset, html->engine, TRUE);
			}

			if (html->priv->in_url_test_mode) {
				GValue arg;
				guint offset;

				memset (&arg, 0, sizeof (GValue));
				g_value_init (&arg, G_TYPE_STRING);
				g_value_set_string (&arg, html->pointer_url);

				gtk_html_editor_event (html, GTK_HTML_EDITOR_EVENT_LINK_CLICKED, &arg);

				g_value_unset (&arg);

				focus_object = html_engine_get_object_at (html->engine, x, y, &offset, TRUE);
				if (HTML_IS_TEXT (focus_object))
					html_text_set_link_visited (HTML_TEXT (focus_object), (gint) offset, html->engine, TRUE);
			}

			html->priv->skip_update_cursor = TRUE;
		}
	}

	html->in_selection = FALSE;
	html->priv->in_object_resize = FALSE;

	return TRUE;
}

static void
gtk_html_keymap_direction_changed (GdkKeymap *keymap, GtkHTML *html)
{
	if (html_engine_get_editable (html->engine)) {
		html_engine_edit_set_direction (html->engine, html_text_direction_pango_to_html (gdk_keymap_get_direction (keymap)));
	}
}

static gboolean
goto_caret_anchor (GtkHTML *html)
{
	gint x = 0, y = 0;

	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	if (!html->priv->is_first_focus)
		return FALSE;

	html->priv->is_first_focus = FALSE;

	if (html->priv->caret_first_focus_anchor && html_object_find_anchor (html->engine->clue, html->priv->caret_first_focus_anchor, &x, &y)) {
		GtkAdjustment *adjustment;
		GtkLayout *layout;
		gdouble page_size;
		gdouble value;

		html_engine_jump_at (html->engine, x, y);

		layout = GTK_LAYOUT (html->engine->widget);
		adjustment = gtk_layout_get_vadjustment (layout);
		page_size = gtk_adjustment_get_page_size (adjustment);
		value = gtk_adjustment_get_value (adjustment);

		/* scroll to the position on screen if not visible */
		if (y < value || y > value + page_size)
			gtk_adjustment_set_value (adjustment, y);

		return TRUE;
	}

	return FALSE;
}

static gint
focus_in_event (GtkWidget *widget,
		GdkEventFocus *event)
{
	GtkHTML *html = GTK_HTML (widget);

	/* printf ("focus in\n"); */
	if (!html->iframe_parent) {
		GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
		if (html->engine->cursor && html->engine->cursor->position == 0 && html->engine->caret_mode)
			goto_caret_anchor (html);
		html_engine_set_focus (html->engine, TRUE);
	} else {
		GtkWidget *window = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
		if (window)
			gtk_window_set_focus (GTK_WINDOW (window), html->iframe_parent);
	}

	html->priv->need_im_reset = TRUE;
	gtk_im_context_focus_in (html->priv->im_context);

	gtk_html_keymap_direction_changed (gdk_keymap_get_for_display (gtk_widget_get_display (widget)),
					   html);
	g_signal_connect (gdk_keymap_get_for_display (gtk_widget_get_display (widget)),
			  "direction_changed", G_CALLBACK (gtk_html_keymap_direction_changed), html);

	return FALSE;
}

static gint
focus_out_event (GtkWidget *widget,
		 GdkEventFocus *event)
{
	GtkHTML *html = GTK_HTML (widget);

	html_painter_set_focus (html->engine->painter, FALSE);
	html_engine_redraw_selection (html->engine);
	/* printf ("focus out\n"); */
	if (!html->iframe_parent) {
		GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
		html_engine_set_focus (html->engine, FALSE);
	}

	html->priv->need_im_reset = TRUE;
	gtk_im_context_focus_out (html->priv->im_context);

	g_signal_handlers_disconnect_by_func (gdk_keymap_get_for_display (gtk_widget_get_display (widget)),
					      gtk_html_keymap_direction_changed, html);

	return FALSE;
}

static gint
enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	GdkWindow *window;
	gint x, y;

	x = event->x;
	y = event->y;
	window = gtk_widget_get_window (widget);
	widget = shift_to_iframe_parent (widget, &x, &y);

	mouse_change_pos (widget, window, x, y, event->state);

	return TRUE;
}

/* X11 selection support.  */

static const gchar *
utf16_order (gboolean swap)
{
	gboolean be;

	/*
	 * FIXME owen tells me this logic probably isn't needed
	 * because smart iconvs will notice the BOM and act accordingly
	 * I don't have as much faith in the various iconv implementations
	 * so I am leaving it in for now
	 */

	be = G_BYTE_ORDER == G_BIG_ENDIAN;
	be = swap ? be : !be;

	if (be)
		return "UTF-16BE";
	else
		return "UTF-16LE";

}

static gchar *
get_selection_string (GtkHTML *html, gint *len, gboolean selection, gboolean primary, gboolean html_format)
{
	HTMLObject *selection_object = NULL;
	gchar *selection_string = NULL;
	gboolean free_object = FALSE;

	if (selection && html_engine_is_selection_active (html->engine)) {
		guint selection_len;
		html_engine_copy_object (html->engine, &selection_object, &selection_len);
		free_object = TRUE;
	} else {
		if (primary) {
			if (html->engine->primary) {
				selection_object = html->engine->primary;
			}
		} else	/* CLIPBOARD */ {
			if (html->engine->clipboard) {
				selection_object = html->engine->clipboard;
			}
		}
	}

	if (html_format) {
		if (selection_object) {
			HTMLEngineSaveState *state;
			GString *buffer;

			state = html_engine_save_buffer_new (html->engine, TRUE);
			buffer = (GString *)state->user_data;

			html_object_save (selection_object, state);
			g_string_append_unichar (buffer, 0x0000);

			if (len)
				*len = buffer->len;
			selection_string = html_engine_save_buffer_free (state, FALSE);
		}
	} else {
		if (selection_object)
			selection_string = html_object_get_selection_string (selection_object, html->engine);
		if (len && selection_string)
			*len = strlen (selection_string);
	}

	if (selection_object && free_object)
		html_object_destroy (selection_object);

	return selection_string;
}

/* returned pointer should be freed with g_free */
gchar *
gtk_html_get_selection_html (GtkHTML *html, gint *len)
{
	return get_selection_string (html, len, TRUE, FALSE, TRUE);
}

/* returned pointer should be freed with g_free */
gchar *
gtk_html_get_selection_plain_text (GtkHTML *html, gint *len)
{
	return get_selection_string (html, len, TRUE, FALSE, FALSE);
}

static gchar *
utf16_to_utf8_with_bom_check (guchar  *data, guint len) {
	const gchar *fromcode = NULL;
	GError  *error = NULL;
	guint16 c;
	gsize read_len, written_len;
	gchar *utf8_ret;

	/*
	 * Unicode Techinical Report 20
	 * ( http://www.unicode.org/unicode/reports/tr20/ ) says to treat an
	 * initial 0xfeff (ZWNBSP) as a byte order indicator so that is
	 * what we do.  If there is no indicator assume it is in the default
	 * order
	 */

	memcpy (&c, data, 2);
	switch (c) {
	case 0xfeff:
	case 0xfffe:
		fromcode = utf16_order (c == 0xfeff);
		data += 2;
		len  -= 2;
		break;
	default:
		fromcode = "UTF-16";
		break;
	}

	utf8_ret = g_convert ((gchar *) data, len, "UTF-8", fromcode, &read_len, &written_len, &error);

	if (error) {
		g_warning ("g_convert error: %s\n", error->message);
		g_error_free (error);
	}
	return (utf8_ret);
}

/* removes useless leading BOM from UTF-8 string if present */
static gchar *
utf8_filter_out_bom (gchar *str) {
	if (!str)
		return NULL;

	/* input is always valid, NUL-terminated UTF-8 sequence, we don't need
	 * to validated it again */
	if (g_utf8_get_char (str) == 0xfeff) {
		gchar *out = g_strdup (g_utf8_next_char (str));
		g_free (str);
		return out;
	}

	return str;
}

static void
set_adjustments (GtkLayout     *layout,
		 GtkAdjustment *hadj,
		 GtkAdjustment *vadj)
{
	GtkHTML *html = GTK_HTML(layout);

	connect_adjustments (html, hadj, vadj);

	if (parent_class->set_scroll_adjustments)
		(* parent_class->set_scroll_adjustments) (layout, hadj, vadj);
}


/* Initialization.  */
static void
set_focus_child (GtkContainer *containter, GtkWidget *w)
{
	HTMLObject *o = NULL;

	while (w && !(o = g_object_get_data (G_OBJECT (w), "embeddedelement")))
		w = gtk_widget_get_parent (w);

	if (o && !html_object_is_frame (o))
		html_engine_set_focus_object (GTK_HTML (containter)->engine, o, 0);

	(*GTK_CONTAINER_CLASS (parent_class)->set_focus_child) (containter, w);
}

static gboolean
focus (GtkWidget *w, GtkDirectionType direction)
{
	HTMLEngine *e = GTK_HTML (w)->engine;

	if (html_engine_get_editable (e)) {
		gboolean rv;

		rv = (*GTK_WIDGET_CLASS (parent_class)->focus) (w, direction);
		html_engine_set_focus (GTK_HTML (w)->engine, rv);
		return rv;
	}

	/* Reset selection. */
	if (e->shift_selection || e->mark) {
		html_engine_disable_selection (e);
		html_engine_edit_selection_updater_schedule (e->selection_updater);
		e->shift_selection = FALSE;
	}

	if (!GTK_WIDGET_HAS_FOCUS (w) && e->caret_mode) {
		if (goto_caret_anchor (GTK_HTML (w))) {
			gtk_widget_grab_focus (w);

			update_primary_selection (GTK_HTML (w));
			g_signal_emit (GTK_HTML (w), signals [CURSOR_CHANGED], 0);

			return TRUE;
		}
	}

	if (((e->focus_object && !(GTK_WIDGET_HAS_FOCUS (w))) || html_engine_focus (e, direction)) && e->focus_object) {
		gint offset;
		HTMLObject *obj = html_engine_get_focus_object (e, &offset);
		gint x1, y1, x2, y2, xo, yo;

		xo = e->x_offset;
		yo = e->y_offset;

		if (HTML_IS_TEXT (obj)) {
			if (!html_text_get_link_rectangle (HTML_TEXT (obj), e->painter, offset, &x1, &y1, &x2, &y2))
				return FALSE;
		} else {
			html_object_calc_abs_position (obj, &x1, &y1);
			y2 = y1 + obj->descent;
			x2 = x1 + obj->width;
			y1 -= obj->ascent;
		}

		/* printf ("child pos: %d,%d x %d,%d\n", x1, y1, x2, y2); */

		if (x2 > e->x_offset + e->width)
			e->x_offset = x2 - e->width;
		if (x1 < e->x_offset)
			e->x_offset = x1;
		if (e->width > 2*RIGHT_BORDER && e->x_offset == x2 - e->width)
			e->x_offset = MIN (x2 - e->width + RIGHT_BORDER + 1,
					   html_engine_get_doc_width (e) - e->width);
		if (e->width > 2*LEFT_BORDER && e->x_offset >= x1)
			e->x_offset = MAX (x1 - LEFT_BORDER, 0);

		if (y2 >= e->y_offset + e->height)
			e->y_offset = y2 - e->height + 1;
		if (y1 < e->y_offset)
			e->y_offset = y1;
		if (e->height > 2*BOTTOM_BORDER && e->y_offset == y2 - e->height + 1)
			e->y_offset = MIN (y2 - e->height + BOTTOM_BORDER + 1,
					   html_engine_get_doc_height (e) - e->height);
		if (e->height > 2*TOP_BORDER && e->y_offset >= y1)
			e->y_offset = MAX (y1 - TOP_BORDER, 0);

		if (e->x_offset != xo) {
			GtkAdjustment *adjustment;

			adjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (w));
			gtk_adjustment_set_value (adjustment, (gfloat) e->x_offset);
		}
		if (e->y_offset != yo) {
			GtkAdjustment *adjustment;

			adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (w));
			gtk_adjustment_set_value (adjustment, (gfloat) e->y_offset);
		}
		/* printf ("engine pos: %d,%d x %d,%d\n",
		   e->x_offset, e->y_offset, e->x_offset + e->width, e->y_offset + e->height); */

		if (!GTK_WIDGET_HAS_FOCUS (w) && !html_object_is_embedded (obj))
			gtk_widget_grab_focus (w);
		if (e->caret_mode) {
			html_engine_jump_to_object (e, obj, offset);
		}

		update_primary_selection (GTK_HTML (w));
		g_signal_emit (GTK_HTML (w), signals [CURSOR_CHANGED], 0);

		return TRUE;
	}

	return FALSE;
}

/* dnd begin */

static void
drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	HTMLInterval *i;
	HTMLObject *o;

	/* printf ("drag_begin\n"); */
	GTK_HTML (widget)->priv->dnd_real_object = o = GTK_HTML (widget)->priv->dnd_object;
	GTK_HTML (widget)->priv->dnd_real_object_offset = GTK_HTML (widget)->priv->dnd_object_offset;
	GTK_HTML (widget)->priv->dnd_in_progress = TRUE;

	i = html_interval_new (o, o, 0, html_object_get_length (o));
	html_engine_select_interval (GTK_HTML (widget)->engine, i);
}

static void
drag_end (GtkWidget *widget, GdkDragContext *context)
{
	GtkHTML *html;

	/* printf ("drag_end\n"); */
	g_return_if_fail (GTK_IS_HTML (widget));

	html = GTK_HTML (widget);
	if (html->priv)
		html->priv->dnd_in_progress = FALSE;
}

static void
drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data, guint info, guint time)
{
	/* printf ("drag_data_get\n"); */
	switch (info) {
	case DND_TARGET_TYPE_MOZILLA_URL:
	case DND_TARGET_TYPE_TEXT_URI_LIST:
		/* printf ("\ttext/uri-list\n"); */
	case DND_TARGET_TYPE_TEXT_HTML:
	case DND_TARGET_TYPE_TEXT_PLAIN:
	case DND_TARGET_TYPE_UTF8_STRING:
	case DND_TARGET_TYPE_STRING: {
		HTMLObject *obj = GTK_HTML (widget)->priv->dnd_real_object;
		gint offset = GTK_HTML (widget)->priv->dnd_real_object_offset;
		const gchar *url, *target;
		gchar *complete_url;

		/* printf ("\ttext/plain\n"); */
		if (obj) {
			/* printf ("obj %p\n", obj); */
			url = html_object_get_url (obj, offset);
			target = html_object_get_target (obj, offset);
			if (url && *url) {

				complete_url = g_strconcat (url, target && *target ? "#" : NULL, target, NULL);

				if (info == DND_TARGET_TYPE_MOZILLA_URL) {
					/* MOZ_URL is in UTF-16 but in format 8. BROKEN!
					 *
					 * The data contains the URL, a \n, then the
					 * title of the web page.
					 */
					gchar *utf16;
					gchar *utf8;
					gsize written_len;
					GdkAtom atom;

					if (HTML_IS_TEXT (obj)) {
						Link *link = html_text_get_link_at_offset (HTML_TEXT (obj), offset);
						gchar *text;

						g_return_if_fail (link);
						text = g_strndup (HTML_TEXT (obj)->text + link->start_index, link->end_index - link->start_index);
						utf8 = g_strconcat (complete_url, "\n", text, NULL);
					} else
						utf8 = g_strconcat (complete_url, "\n", complete_url, NULL);

					utf16 = g_convert (utf8, strlen (utf8), "UTF-16", "UTF-8", NULL, &written_len, NULL);
					atom = gtk_selection_data_get_target (selection_data);
					gtk_selection_data_set (selection_data, atom, 8,
								(guchar *) utf16, written_len);
					g_free (utf8);
					g_free (complete_url);
					GTK_HTML (widget)->priv->dnd_url = utf16;
				} else {
					GdkAtom atom;

					atom = gtk_selection_data_get_target (selection_data);
					gtk_selection_data_set (selection_data, atom, 8,
								(guchar *) complete_url, strlen (complete_url));
					/* printf ("complete URL %s\n", complete_url); */
					GTK_HTML (widget)->priv->dnd_url = complete_url;
				}
			}
		}
	}
	break;
	}
}

static void
drag_data_delete (GtkWidget *widget, GdkDragContext *context)
{
	g_free (GTK_HTML (widget)->priv->dnd_url);
	GTK_HTML (widget)->priv->dnd_url = NULL;
}

static gchar *
next_uri (guchar **uri_list, gint *len, gint *list_len)
{
	guchar *uri, *begin;

	begin = *uri_list;
	*len = 0;
	while (**uri_list && **uri_list != '\n' && **uri_list != '\r' && *list_len) {
		(*uri_list) ++;
		(*len) ++;
		(*list_len) --;
	}

	uri = (guchar *) g_strndup ((gchar *) begin, *len);

	while ((!**uri_list || **uri_list == '\n' || **uri_list == '\r') && *list_len) {
		(*uri_list) ++;
		(*list_len) --;
	}

	return (gchar *) uri;
}

static HTMLObject *
new_img_obj_from_uri (HTMLEngine *e, gchar *uri, gchar *title, gint len)
{
	if (!strncmp (uri, "file:", 5)) {
		if (!HTML_IS_PLAIN_PAINTER(e->painter)) {
			GdkPixbuf *pixbuf = NULL;
			gchar *img_path = g_filename_from_uri (uri, NULL, NULL);
			if (img_path) {
				pixbuf = gdk_pixbuf_new_from_file(img_path, NULL);
				g_free(img_path);
			}
			if (pixbuf) {
				g_object_unref (pixbuf);
				return html_image_new (html_engine_get_image_factory (e), uri,
						       NULL, NULL, -1, -1, FALSE, FALSE, 0,
						       html_colorset_get_color (e->settings->color_set, HTMLTextColor),
						       HTML_VALIGN_BOTTOM, TRUE);
			}
		}
	}
	return NULL;
}

static void
move_before_paste (GtkWidget *widget, gint x, gint y)
{
	HTMLEngine *engine = GTK_HTML (widget)->engine;

	if (html_engine_is_selection_active (engine)) {
		HTMLObject *obj;
		guint offset;

		obj = html_engine_get_object_at (engine, x, y, &offset, FALSE);
		if (!html_engine_point_in_selection (engine, obj, offset)) {
			html_engine_disable_selection (engine);
			html_engine_edit_selection_updater_update_now (engine->selection_updater);
		}
	}
	if (!html_engine_is_selection_active (engine)) {

		html_engine_jump_at (engine, x, y);
		gtk_html_update_styles (GTK_HTML (widget));
	}
}

static void
drag_data_received (GtkWidget *widget, GdkDragContext *context,
		    gint x, gint y, GtkSelectionData *selection_data, guint info, guint time)
{
	HTMLEngine *engine = GTK_HTML (widget)->engine;
	GdkWindow *bin_window;
	gboolean pasted = FALSE;
	const guchar *data;
	gint length;

	/* printf ("drag data received at %d,%d\n", x, y); */

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (!data || length < 0 || !html_engine_get_editable (engine))
		return;

	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_get_pointer (bin_window, &x, &y, NULL);
	move_before_paste (widget, x, y);

	switch (info) {
	case DND_TARGET_TYPE_TEXT_PLAIN:
	case DND_TARGET_TYPE_UTF8_STRING:
	case DND_TARGET_TYPE_STRING:
	case DND_TARGET_TYPE_TEXT_HTML:
		clipboard_paste_received_cb(
			gtk_widget_get_clipboard (GTK_WIDGET (widget), GDK_SELECTION_PRIMARY),
			selection_data, widget);
		pasted = TRUE;
		break;
        case DND_TARGET_TYPE_MOZILLA_URL  :
		break;
        case DND_TARGET_TYPE_TEXT_URI_LIST:
		if (!HTML_IS_PLAIN_PAINTER (engine->painter)) {
                 HTMLObject *obj;
                 gint list_len, len;
                 gchar *uri;
                 html_undo_level_begin (engine->undo, "Dropped URI(s)", "Remove Dropped URI(s)");
                 list_len = length;
                 do {
                         uri = next_uri ((guchar **) &data, &len, &list_len);
                         obj = new_img_obj_from_uri (engine, uri, NULL, -1);
                         if (obj) {
                                 html_engine_paste_object (engine, obj, html_object_get_length (obj));
                                 pasted = TRUE;
                         }
                 } while (list_len);
                 html_undo_level_end (engine->undo);
	}
	break;
	}
	gtk_drag_finish (context, pasted, FALSE, time);
}

static gboolean
drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time)
{
	GdkWindow *window;
	GdkWindow *bin_window;

	if (!gtk_html_get_editable (GTK_HTML (widget)))
		return FALSE;

	window = gtk_widget_get_window (widget);
	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_get_pointer (bin_window, &x, &y, NULL);

	html_engine_disable_selection (GTK_HTML (widget)->engine);
	html_engine_jump_at (GTK_HTML (widget)->engine, x, y);
	html_engine_show_cursor (GTK_HTML (widget)->engine);

	mouse_change_pos (widget, window, x, y, 0);

	return TRUE;
}

/* dnd end */

#ifdef HAVE_GCONF
static void
read_key_theme (GtkHTMLClass *html_class)
{
	gchar *key_theme;

	key_theme = gconf_client_get_string (gconf_client_get_default (), "/desktop/gnome/interface/gtk_key_theme", NULL);
	html_class->use_emacs_bindings = key_theme && !strcmp (key_theme, "Emacs");
	g_free (key_theme);
}

static void
client_notify_key_theme (GConfClient* client, guint cnxn_id, GConfEntry* entry, gpointer data)
{
	read_key_theme ((GtkHTMLClass *) data);
}

static void
client_notify_monospace_font (GConfClient* client, guint cnxn_id, GConfEntry* entry, gpointer data)
{
	GtkHTML *html = (GtkHTML *) data;
	HTMLEngine *e = html->engine;
	if (e && e->painter) {
		gtk_html_set_fonts (html, e->painter);
		html_engine_refresh_fonts (e);
	}
}

static void
client_notify_cursor_blink (GConfClient* client, guint cnxn_id, GConfEntry* entry, gpointer data)
{
	if (gconf_client_get_bool (client, "/desktop/gnome/interface/cursor_blink", NULL))
		html_engine_set_cursor_blink_timeout (gconf_client_get_int (client, "/desktop/gnome/interface/cursor_blink_time", NULL) / 2);
	else
		html_engine_set_cursor_blink_timeout (0);
}
#endif

static void
gtk_html_direction_changed (GtkWidget *widget, GtkTextDirection previous_dir)
{
	GtkHTML *html = GTK_HTML (widget);

	if (html->engine->clue) {
		HTMLDirection old_direction = html_object_get_direction (html->engine->clue);

		switch (gtk_widget_get_direction (widget)) {
		case GTK_TEXT_DIR_NONE:
			HTML_CLUEV (html->engine->clue)->dir = HTML_DIRECTION_DERIVED;
			break;
		case GTK_TEXT_DIR_LTR:
			HTML_CLUEV (html->engine->clue)->dir = HTML_DIRECTION_LTR;
			break;
		case GTK_TEXT_DIR_RTL:
			HTML_CLUEV (html->engine->clue)->dir = HTML_DIRECTION_RTL;
			break;
		}

		if (old_direction != html_object_get_direction (html->engine->clue))
			html_engine_schedule_update (html->engine);
	}

	GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_html_class_init (GtkHTMLClass *klass)
{
#ifdef USE_PROPS
	GObjectClass      *gobject_class;
#endif
	GtkHTMLClass      *html_class;
	GtkWidgetClass    *widget_class;
	GtkObjectClass    *object_class;
	GtkLayoutClass    *layout_class;
	GtkContainerClass *container_class;
#ifdef HAVE_GCONF
	gchar *filename;
	GConfClient *client;
#endif

	html_class = (GtkHTMLClass *) klass;
#ifdef USE_PROPS
	gobject_class = (GObjectClass *) klass;
#endif
	widget_class = (GtkWidgetClass *) klass;
	object_class = (GtkObjectClass *) klass;
	layout_class = (GtkLayoutClass *) klass;
	container_class = (GtkContainerClass *) klass;

	object_class->destroy = destroy;

	parent_class = g_type_class_peek_parent (klass);

	signals [TITLE_CHANGED] =
		g_signal_new ("title_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, title_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	signals [URL_REQUESTED] =
		g_signal_new ("url_requested",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GtkHTMLClass, url_requested),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_POINTER);
	signals [LOAD_DONE] =
		g_signal_new ("load_done",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, load_done),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [LINK_CLICKED] =
		g_signal_new ("link_clicked",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, link_clicked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	signals [SET_BASE] =
		g_signal_new ("set_base",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, set_base),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	signals [SET_BASE_TARGET] =
		g_signal_new ("set_base_target",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, set_base_target),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	signals [ON_URL] =
		g_signal_new ("on_url",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, on_url),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	signals [REDIRECT] =
		g_signal_new ("redirect",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, redirect),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_INT);

	signals [SUBMIT] =
		g_signal_new ("submit",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, submit),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals [OBJECT_REQUESTED] =
		g_signal_new ("object_requested",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GtkHTMLClass, object_requested),
			      NULL, NULL,
			      html_g_cclosure_marshal_BOOL__OBJECT,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_OBJECT);

	signals [CURRENT_PARAGRAPH_STYLE_CHANGED] =
		g_signal_new ("current_paragraph_style_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, current_paragraph_style_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals [CURRENT_PARAGRAPH_INDENTATION_CHANGED] =
		g_signal_new ("current_paragraph_indentation_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, current_paragraph_indentation_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals [CURRENT_PARAGRAPH_ALIGNMENT_CHANGED] =
		g_signal_new ("current_paragraph_alignment_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, current_paragraph_alignment_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals [INSERTION_FONT_STYLE_CHANGED] =
		g_signal_new ("insertion_font_style_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, insertion_font_style_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals [INSERTION_COLOR_CHANGED] =
		g_signal_new ("insertion_color_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, insertion_color_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	signals [SIZE_CHANGED] =
		g_signal_new ("size_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, size_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [IFRAME_CREATED] =
		g_signal_new ("iframe_created",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, iframe_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_HTML);

	signals [SCROLL] =
		g_signal_new ("scroll",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkHTMLClass, scroll),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__ENUM_ENUM_FLOAT,
			      G_TYPE_NONE, 3,
			      GTK_TYPE_ORIENTATION,
			      GTK_TYPE_SCROLL_TYPE, G_TYPE_FLOAT);

	signals [CURSOR_MOVE] =
		g_signal_new ("cursor_move",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkHTMLClass, cursor_move),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__ENUM_ENUM,
			      G_TYPE_NONE, 2, GTK_TYPE_DIRECTION_TYPE, GTK_TYPE_HTML_CURSOR_SKIP);

	signals [COMMAND] =
		g_signal_new ("command",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkHTMLClass, command),
			      NULL, NULL,
			      html_g_cclosure_marshal_BOOL__ENUM,
			      G_TYPE_BOOLEAN, 1, GTK_TYPE_HTML_COMMAND);

	signals [CURSOR_CHANGED] =
		g_signal_new ("cursor_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, cursor_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals [OBJECT_INSERTED] =
		g_signal_new ("object_inserted",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, object_inserted),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_INT);

	signals [OBJECT_DELETE] =
		g_signal_new ("object_delete",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkHTMLClass, object_delete),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_INT);
	object_class->destroy = destroy;

#ifdef USE_PROPS
	gobject_class->get_property = gtk_html_get_property;
	gobject_class->set_property = gtk_html_set_property;

	g_object_class_install_property (gobject_class,
					 PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       _("Whether the html can be edited"),
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (gobject_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
							      _("Document Title"),
							      _("The title of the current document"),
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (gobject_class,
					 PROP_DOCUMENT_BASE,
					 g_param_spec_string ("document_base",
							      _("Document Base"),
							      _("The base URL for relative references"),
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (gobject_class,
					 PROP_TARGET_BASE,
					 g_param_spec_string ("target_base",
							      _("Target Base"),
							      _("The base URL of the target frame"),
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));

#endif

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_string ("fixed_font_name",
								     _("Fixed Width Font"),
								     _("The Monospace font to use for typewriter text"),
								     NULL,
								     G_PARAM_READABLE));

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("link_color",
								     _("New Link Color"),
								     _("The color of new link elements"),
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("vlink_color",
								     _("Visited Link Color"),
								     _("The color of visited link elements"),
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("alink_color",
								     _("Active Link Color"),
								     _("The color of active link elements"),
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("spell_error_color",
								     _("Spelling Error Color"),
								     _("The color of the spelling error markers"),
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("cite_color",
								     _("Cite Quotation Color"),
								     _("The color of the cited text"),
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));

	widget_class->realize = realize;
	widget_class->unrealize = unrealize;
	widget_class->style_set = style_set;
	widget_class->key_press_event = key_press_event;
	widget_class->key_release_event = key_release_event;
	widget_class->expose_event  = expose;
	widget_class->size_request = gtk_html_size_request;
	widget_class->size_allocate = size_allocate;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->visibility_notify_event = visibility_notify_event;
	widget_class->hierarchy_changed = hierarchy_changed;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->focus_in_event = focus_in_event;
	widget_class->focus_out_event = focus_out_event;
	widget_class->enter_notify_event = enter_notify_event;
	widget_class->drag_data_get = drag_data_get;
	widget_class->drag_data_delete = drag_data_delete;
	widget_class->drag_begin = drag_begin;
	widget_class->drag_end = drag_end;
	widget_class->drag_data_received = drag_data_received;
	widget_class->drag_motion = drag_motion;
	widget_class->focus = focus;
	widget_class->direction_changed = gtk_html_direction_changed;

	container_class->set_focus_child = set_focus_child;

	layout_class->set_scroll_adjustments = set_adjustments;

	html_class->scroll            = scroll;
	html_class->cursor_move       = cursor_move;
	html_class->command           = command;
	html_class->properties        = gtk_html_class_properties_new ();

	add_bindings (klass);
	gtk_html_accessibility_init ();

#ifdef HAVE_GCONF
	filename = g_build_filename (PREFIX, "share", GTKHTML_RELEASE_STRING, "keybindingsrc.emacs", NULL);
	gtk_rc_parse (filename);
	g_free (filename);
	html_class->emacs_bindings = gtk_binding_set_find ("gtkhtml-bindings-emacs");
	read_key_theme (html_class);
	
	client = gconf_client_get_default ();

	gconf_client_notify_add (client, "/desktop/gnome/interface/gtk_key_theme",
				 client_notify_key_theme, html_class, NULL, &gconf_error);

	gconf_client_notify_add (client, "/desktop/gnome/interface/cursor_blink", client_notify_cursor_blink, NULL, NULL, NULL);
	gconf_client_notify_add (client, "/desktop/gnome/interface/cursor_blink_time", client_notify_cursor_blink, NULL, NULL, NULL);
	client_notify_cursor_blink (client, 0, NULL, NULL);

	g_object_unref (client);
#endif
}

void
gtk_html_im_reset (GtkHTML *html)
{
	if (!html->priv->im_block_reset) {
		D_IM (printf ("IM reset requested\n");)
		if (html->priv->need_im_reset) {
			if (html->engine->freeze_count == 1)
				html_engine_thaw_idle_flush (html->engine);
			html->priv->need_im_reset = FALSE;
			gtk_im_context_reset (html->priv->im_context);
			D_IM (printf ("IM reset called\n");)
		}
	}
}

static void
gtk_html_im_commit_cb (GtkIMContext *context, const gchar *str, GtkHTML *html)
{
	gboolean state = html->priv->im_block_reset;
	gint pos;

	html->priv->im_block_reset = TRUE;

        if (html->priv->im_pre_len > 0) {
                D_IM (printf ("IM delete last preedit %d + %d\n", html->priv->im_pre_pos, html->priv->im_pre_len);)

                html_undo_freeze (html->engine->undo);
                html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, html->priv->im_pre_pos);
                html_engine_set_mark (html->engine);
                html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, html->priv->im_pre_pos + html->priv->im_pre_len);
                html_engine_delete (html->engine);
                html->priv->im_pre_len = 0;
                html_undo_thaw (html->engine->undo);
        }

	pos = html->engine->cursor->position;
	if (html->engine->mark && html->engine->mark->position > pos)
		pos = html->engine->mark->position;

	D_IM (printf ("IM commit %s\n", str);)
	html_engine_paste_text (html->engine, str, -1);
	html->priv->im_block_reset = state;

	D_IM (printf ("IM commit pos: %d pre_pos: %d\n", pos, html->priv->im_pre_pos);)
	if (html->priv->im_pre_pos >= pos)
		html->priv->im_pre_pos += html->engine->cursor->position - pos;
}

static void
gtk_html_im_preedit_start_cb (GtkIMContext *context, GtkHTML *html)
{
	html->priv->im_pre_len = 0;
}

static void
gtk_html_im_preedit_changed_cb (GtkIMContext *context, GtkHTML *html)
{
	PangoAttrList *attrs;
	gchar *preedit_string;
	gint cursor_pos, initial_position;
	gboolean state = html->priv->im_block_reset;
	gboolean pop_selection = FALSE;
	gint deleted = 0;

	D_IM (printf ("IM preedit changed cb [begin] cursor %d(%p) mark %d(%p) active: %d\n",
		      html->engine->cursor ? html->engine->cursor->position : 0, html->engine->cursor,
		      html->engine->mark ? html->engine->mark->position : 0, html->engine->mark,
		      html_engine_is_selection_active (html->engine));)

	if (!html->engine->cursor) {
		return;
	}

	html->priv->im_block_reset = TRUE;

	if (html->engine->mark && html_engine_is_selection_active (html->engine)) {
		D_IM (printf ("IM push selection\n");)
		html_engine_selection_push (html->engine);
		html_engine_disable_selection (html->engine);
		html_engine_edit_selection_updater_update_now (html->engine->selection_updater);
		pop_selection = TRUE;
	}
	initial_position = html->engine->cursor->position;
	D_IM (printf ("IM initial position %d\n", initial_position);)

	html_undo_freeze (html->engine->undo);

	if (html->priv->im_pre_len > 0) {
		D_IM (printf ("IM delete last preedit %d + %d\n", html->priv->im_pre_pos, html->priv->im_pre_len);)

		html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, html->priv->im_pre_pos);
		html_engine_set_mark (html->engine);
		html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, html->priv->im_pre_pos + html->priv->im_pre_len);
		html_engine_delete (html->engine);
		deleted = html->priv->im_pre_len;
	} else
		html->priv->im_orig_style = html_engine_get_font_style (html->engine);

	gtk_im_context_get_preedit_string (html->priv->im_context, &preedit_string, &attrs, &cursor_pos);

	D_IM (printf ("IM preedit changed to %s\n", preedit_string);)
	html->priv->im_pre_len = g_utf8_strlen (preedit_string, -1);

	if (html->priv->im_pre_len > 0) {
		cursor_pos = CLAMP (cursor_pos, 0, html->priv->im_pre_len);
		html->priv->im_pre_pos = html->engine->cursor->position;
		html_engine_paste_text_with_extra_attributes (html->engine, preedit_string, html->priv->im_pre_len, attrs);
		html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, html->priv->im_pre_pos + cursor_pos);
	} else
		html_engine_set_font_style (html->engine, 0, html->priv->im_orig_style);
	g_free (preedit_string);

	if (pop_selection) {
		gint position= html->engine->cursor->position, cpos, mpos;
		D_IM (printf ("IM pop selection\n");)
		g_assert (html_engine_selection_stack_top (html->engine, &cpos, &mpos));
		if (position < MAX (cpos, mpos) + html->priv->im_pre_len - deleted)
			g_assert (html_engine_selection_stack_top_modify (html->engine, html->priv->im_pre_len - deleted));
		html_engine_selection_pop (html->engine);
	}
	/* that works for now, but idealy we should be able to have cursor positioned outside selection, so that preedit
	   cursor is in the right place */
	if (html->priv->im_pre_len == 0)
		html_cursor_jump_to_position_no_spell (html->engine->cursor, html->engine,
						       initial_position >= html->priv->im_pre_pos + deleted ? initial_position - deleted : initial_position);

	if (html->engine->freeze_count == 1)
		html_engine_thaw_idle_flush (html->engine);
	/* FIXME gtk_im_context_set_cursor_location (im_context, &area); */
	html->priv->im_block_reset = state;

	html_undo_thaw (html->engine->undo);

	D_IM (printf ("IM preedit changed cb [end] cursor %d(%p) mark %d(%p) active: %d\n",
		      html->engine->cursor ? html->engine->cursor->position : 0, html->engine->cursor,
		      html->engine->mark ? html->engine->mark->position : 0, html->engine->mark,
		      html_engine_is_selection_active (html->engine));)
}

static gchar *
get_surrounding_text (HTMLEngine *e, gint *offset)
{
	HTMLObject *o = e->cursor->object;
	HTMLObject *prev;
	gchar *text = NULL;

	if (!html_object_is_text (o)) {
		*offset = 0;
		if (e->cursor->offset == 0) {
			prev = html_object_prev_not_slave (o);
			if (html_object_is_text (prev)) {
				o = prev;
			} else
				return NULL;
		} else if (e->cursor->offset == html_object_get_length (e->cursor->object)) {
			HTMLObject *next;

			next = html_object_next_not_slave (o);
			if (html_object_is_text (next)) {
				o = next;
			} else
				return NULL;
		}
	} else
		*offset = e->cursor->offset;

	while ((prev = html_object_prev_not_slave (o)) && html_object_is_text (prev)) {
		o = prev;
		*offset += HTML_TEXT (o)->text_len;
	}

	while (o) {
		if (html_object_is_text (o)) {
			if (!text)
				text = g_strdup (HTML_TEXT (o)->text);
			else {
				gchar *concat = g_strconcat (text, HTML_TEXT (o)->text, NULL);
				g_free (text);
				text = concat;
			}
		}
		o = html_object_next_not_slave (o);
	}

	return text;
}

static gboolean
gtk_html_im_retrieve_surrounding_cb (GtkIMContext *context, GtkHTML *html)
{
	gint offset = 0;
	gchar *text;

	D_IM (printf ("IM gtk_html_im_retrieve_surrounding_cb\n");)

	text = get_surrounding_text (html->engine, &offset);
	if (text) {
		/* convert gchar offset to byte offset */
		offset = g_utf8_offset_to_pointer (text, offset) - text;
		gtk_im_context_set_surrounding (context, text, -1, offset);
		g_free (text);
	} else
		gtk_im_context_set_surrounding (context, NULL, 0, 0);

	return TRUE;
}

static gboolean
gtk_html_im_delete_surrounding_cb (GtkIMContext *slave, gint offset, gint n_chars, GtkHTML *html)
{
	D_IM (printf ("IM gtk_html_im_delete_surrounding_cb\n");)
	if (html_engine_get_editable (html->engine) && !html_engine_is_selection_active (html->engine)) {
		gint orig_position = html->engine->cursor->position;

		html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, orig_position + offset);
		html_engine_set_mark (html->engine);
		html_cursor_exactly_jump_to_position_no_spell (html->engine->cursor, html->engine, orig_position + offset + n_chars);
		html_engine_delete (html->engine);
		if (offset < 0)
			orig_position -= MIN (n_chars, - offset);
		html_cursor_jump_to_position_no_spell (html->engine->cursor, html->engine, orig_position);
	}
	return TRUE;
}

static void
gtk_html_init (GtkHTML* html)
{
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (html), GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (html), GTK_APP_PAINTABLE);

	html->editor_api = NULL;
	html->debug = FALSE;
	html->allow_selection = TRUE;

	html->pointer_url = NULL;
	html->hand_cursor = gdk_cursor_new (GDK_HAND2);
	html->ibeam_cursor = gdk_cursor_new (GDK_XTERM);
	html->hadj_connection = 0;
	html->vadj_connection = 0;

	html->selection_x1 = 0;
	html->selection_y1 = 0;

	html->in_selection = FALSE;
	html->in_selection_drag = FALSE;

	html->priv = g_new0 (GtkHTMLPrivate, 1);
	html->priv->idle_handler_id = 0;
	html->priv->scroll_timeout_id = 0;
	html->priv->skip_update_cursor = FALSE;
	html->priv->cursor_moved = FALSE;
	html->priv->paragraph_style = GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	html->priv->paragraph_alignment = GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	html->priv->paragraph_indentation = 0;
	html->priv->insertion_font_style = GTK_HTML_FONT_STYLE_DEFAULT;
	html->priv->selection_type = -1;
	html->priv->selection_as_cite = FALSE;
	html->priv->search_input_line = NULL;
	html->priv->in_object_resize = FALSE;
	html->priv->resize_cursor = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
	html->priv->in_url_test_mode = FALSE;
	html->priv->in_key_binding = FALSE;

	html->priv->caret_first_focus_anchor = NULL;
	html->priv->is_first_focus = TRUE;

	/* IM Context */
	html->priv->im_context = gtk_im_multicontext_new ();
	html->priv->need_im_reset = FALSE;
	html->priv->im_block_reset = FALSE;
	html->priv->im_pre_len = 0;

	g_signal_connect (G_OBJECT (html->priv->im_context), "commit",
			  G_CALLBACK (gtk_html_im_commit_cb), html);
	g_signal_connect (G_OBJECT (html->priv->im_context), "preedit_start",
			  G_CALLBACK (gtk_html_im_preedit_start_cb), html);
	g_signal_connect (G_OBJECT (html->priv->im_context), "preedit_changed",
			  G_CALLBACK (gtk_html_im_preedit_changed_cb), html);
	g_signal_connect (G_OBJECT (html->priv->im_context), "retrieve_surrounding",
			  G_CALLBACK (gtk_html_im_retrieve_surrounding_cb), html);
	g_signal_connect (G_OBJECT (html->priv->im_context), "delete_surrounding",
			  G_CALLBACK (gtk_html_im_delete_surrounding_cb), html);

#ifdef HAVE_GCONF
	html->priv->notify_monospace_font_id =
		gconf_client_notify_add (gconf_client_get_default (), "/desktop/gnome/interface/monospace_font_name",
					 client_notify_monospace_font, html, NULL, &gconf_error);
#endif

	gtk_html_construct (html);
}

GType
gtk_html_get_type (void)
{
	static GType html_type = 0;

	if (!html_type) {
		static const GTypeInfo html_info = {
			sizeof (GtkHTMLClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gtk_html_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (GtkHTML),
			1,              /* n_preallocs */
			(GInstanceInitFunc) gtk_html_init,
		};

		html_type = g_type_register_static (GTK_TYPE_LAYOUT, "GtkHTML", &html_info, 0);
	}

	return html_type;
}

/**
 * gtk_html_new:
 * @void:
 *
 * GtkHTML widget contructor. It creates an empty GtkHTML widget.
 *
 * Return value: A GtkHTML widget, newly created and empty.
 **/

GtkWidget *
gtk_html_new (void)
{
	GtkWidget *html;

	html = g_object_new (GTK_TYPE_HTML, NULL);

	return html;
}

/**
 * gtk_html_new_from_string:
 * @str: A string containing HTML source.
 * @len: A length of @str, if @len == -1 then it will be computed using strlen.
 *
 * GtkHTML widget constructor. It creates an new GtkHTML widget and loads HTML source from @str.
 * It is intended for simple creation. For more complicated loading you probably want to use
 * #GtkHTMLStream. See #gtk_html_begin.
 *
 * Return value: A GtkHTML widget, newly created, containing document loaded from input @str.
 **/

GtkWidget *
gtk_html_new_from_string (const gchar *str, gint len)
{
	GtkWidget *html;

	html = g_object_new (GTK_TYPE_HTML, NULL);
	gtk_html_load_from_string (GTK_HTML (html), str, len);

	return html;
}

void
gtk_html_construct (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->engine        = html_engine_new (GTK_WIDGET (html));
	html->iframe_parent = NULL;

	g_signal_connect (G_OBJECT (html->engine), "title_changed",
			  G_CALLBACK (html_engine_title_changed_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "set_base",
			  G_CALLBACK (html_engine_set_base_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "set_base_target",
			  G_CALLBACK (html_engine_set_base_target_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "load_done",
			  G_CALLBACK (html_engine_load_done_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "url_requested",
			  G_CALLBACK (html_engine_url_requested_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "draw_pending",
			  G_CALLBACK (html_engine_draw_pending_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "redirect",
			  G_CALLBACK (html_engine_redirect_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "submit",
			  G_CALLBACK (html_engine_submit_cb), html);
	g_signal_connect (G_OBJECT (html->engine), "object_requested",
			  G_CALLBACK (html_engine_object_requested_cb), html);
}


void
gtk_html_enable_debug (GtkHTML *html,
		       gboolean debug)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->debug = debug;
}


void
gtk_html_allow_selection (GtkHTML *html,
			  gboolean allow)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->allow_selection = allow;
}

/**
 * gtk_html_begin_full:
 * @html: the GtkHTML widget to operate on.
 * @target_frame: the string identifying the frame to load the data into
 * @content_type: the content_type of the data that we will be loading
 * @flags: the GtkHTMLBeginFlags that control the reload behavior handling
 *
 * Opens a new stream of type @content_type to the frame named @target_frame.
 * the flags in @flags allow control over what data is reloaded.
 *
 * Returns: a new GtkHTMLStream to specified frame
 */
GtkHTMLStream *
gtk_html_begin_full (GtkHTML           *html,
		     gchar              *target_frame,
		     const gchar        *content_type,
		     GtkHTMLBeginFlags flags)
{
	GtkHTMLStream *handle;

	g_return_val_if_fail (!gtk_html_get_editable (html), NULL);

	if (flags & GTK_HTML_BEGIN_BLOCK_UPDATES)
		gtk_html_set_blocking (html, TRUE);
	else
		gtk_html_set_blocking (html, FALSE);

	if (flags & GTK_HTML_BEGIN_BLOCK_IMAGES)
		gtk_html_set_images_blocking (html, TRUE);
	else
		gtk_html_set_images_blocking (html, FALSE);

	if (flags & GTK_HTML_BEGIN_KEEP_IMAGES)
		gtk_html_images_ref (html);

	if (flags & GTK_HTML_BEGIN_KEEP_SCROLL)
		html->engine->keep_scroll = TRUE;
	else
		html->engine->keep_scroll = FALSE;

	html->priv->is_first_focus = TRUE;

	handle = html_engine_begin (html->engine, content_type);
	if (handle == NULL)
		return NULL;

	html_engine_parse (html->engine);

	if (flags & GTK_HTML_BEGIN_KEEP_IMAGES)
		gtk_html_images_unref (html);

	if (flags & GTK_HTML_BEGIN_KEEP_SCROLL)
		html->engine->newPage = FALSE;

	/* Enable change content type in engine */
	if (flags & GTK_HTML_BEGIN_CHANGECONTENTTYPE)
		gtk_html_set_default_engine(html, TRUE);

	return handle;
}

/**
 * gtk_html_begin:
 * @html: the html widget to operate on.
 *
 * Opens a new stream to load new content into the GtkHTML widget @html.
 *
 * Returns: a new GtkHTMLStream to store new content.
 **/
GtkHTMLStream *
gtk_html_begin (GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	return gtk_html_begin_full (html, NULL, NULL, 0);
}

/**
 * gtk_html_begin_content:
 * @html: the html widget to operate on.
 * @content_type: a string listing the type of content to expect on the stream.
 *
 * Opens a new stream to load new content of type @content_type into
 * the GtkHTML widget given in @html.
 *
 * Returns: a new GtkHTMLStream to store new content.
 **/
GtkHTMLStream *
gtk_html_begin_content (GtkHTML *html, const gchar *content_type)
{
	g_return_val_if_fail (! gtk_html_get_editable (html), NULL);

	return gtk_html_begin_full (html, NULL, content_type , 0);
}

/**
 * gtk_html_set_href:
 * @html: the GtkHTML widget the stream belongs to (unused)
 * @handle: the GkHTMLStream to write to.
 * @href: url from loaded content.
 *
 * Set @href from loaded content in @stream.
 **/
void
gtk_html_set_href (GtkHTML *html,
		GtkHTMLStream *handle,
		const gchar *href)
{
	g_return_if_fail (href!=NULL);
	g_return_if_fail (handle!=NULL);
	gtk_html_stream_href (handle, href);
}

/**
 * gtk_html_get_types:
 * @html: the GtkHTML widget the stream belongs to (unused)
 * @handle: the GkHTMLStream to write to.
 *
 * Get all content type understanding in this time
 *
 * Returns: all understanding types in this time.
 **/
gchar **
gtk_html_get_types (GtkHTML *html,
		GtkHTMLStream *handle)
{
	return gtk_html_stream_get_types (handle);
}

/**
 * gtk_html_set_mime_type:
 * @html: the GtkHTML widget the stream belongs to (unused)
 * @handle: the GkHTMLStream to write to.
 * @mime_type: stream mime type.
 *
 * Set @mime_type for content in @stream.
 **/
void
gtk_html_set_mime_type (GtkHTML *html,
		GtkHTMLStream *handle,
		const gchar *mime_type)
{
	g_return_if_fail (mime_type!=NULL);
	g_return_if_fail (handle!=NULL);
	gtk_html_stream_mime (handle, mime_type);
}

/**
 * gtk_html_write:
 * @html: the GtkHTML widget the stream belongs to (unused)
 * @handle: the GkHTMLStream to write to.
 * @buffer: the data to write to the stream.
 * @size: the length of data to read from @buffer
 *
 * Writes @size bytes of @buffer to the stream pointed to by @stream.
 **/
void
gtk_html_write (GtkHTML *html,
		GtkHTMLStream *handle,
		const gchar *buffer,
		gsize size)
{
	gtk_html_stream_write (handle, buffer, size);
}

/**
 * gtk_html_end:
 * @html: the GtkHTML widget the stream belongs to.
 * @handle: the GtkHTMLStream to close.
 * @status: the GtkHTMLStreamStatus representing the state of the stream when closed.
 *
 * Close the GtkHTMLStream represented by @stream and notify @html that is
 * should not expect any more content from that stream.
 **/
void
gtk_html_end (GtkHTML *html,
	      GtkHTMLStream *handle,
	      GtkHTMLStreamStatus status)
{
	gtk_html_stream_close (handle, status);
}

/**
 * gtk_html_stop:
 * @html: the GtkHTML widget.
 *
 * Stop requesting any more data by url_requested signal.
 **/
void
gtk_html_stop (GtkHTML *html)
{
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_stop (html->engine);
}


/**
 * gtk_html_get_title:
 * @html: The GtkHTML widget.
 *
 * Retrieve the title of the document currently loaded in the GtkHTML widget.
 *
 * Returns: the title of the current document
 **/
const gchar *
gtk_html_get_title (GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, NULL);
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	if (html->engine->title == NULL)
		return NULL;

	return html->engine->title;
}

/**
 * gtk_html_set_title:
 * @html: The GtkHTML widget.
 *
 * Set the title of the document currently loaded in the GtkHTML widget.
 *
 **/
void
gtk_html_set_title (GtkHTML *html, const gchar *title)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_title (html->engine, title);
}


/**
 * gtk_html_jump_to_anchor:
 * @html: the GtkHTML widget.
 * @anchor: a string containing the name of the anchor.
 *
 * Scroll the document display to show the HTML anchor listed in @anchor
 *
 * Returns: TRUE if the anchor is found, FALSE otherwise.
 **/
gboolean
gtk_html_jump_to_anchor (GtkHTML *html,
			 const gchar *anchor)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return html_engine_goto_anchor (html->engine, anchor);
}


gboolean
gtk_html_save (GtkHTML *html,
	       GtkHTMLSaveReceiverFn receiver,
	       gpointer data)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (receiver != NULL, FALSE);

	return html_engine_save (html->engine, receiver, data);
}

/**
 * gtk_html_dump:
 * @html: the GtkHTML widget
 *
 * Dumped simple object tree to console.
 *
 **/
void
gtk_html_dump (GtkHTML *html)
{
	gtk_html_debug_dump_tree_simple (html->engine->clue, 0);
}

/**
 * gtk_html_export:
 * @html: the GtkHTML widget
 * @content_type: the expected content_type
 * @receiver:
 * @user_data: pointer to maintain user state.
 *
 * Export the current document into the content type given by @content_type,
 * by calling the function listed in @receiver data becomes avaiable.  When @receiver is
 * called @user_data is passed in as the user_data parameter.
 *
 * Returns: TRUE if the export was successfull, FALSE otherwise.
 **/
gboolean
gtk_html_export (GtkHTML *html,
		 const gchar *content_type,
		 GtkHTMLSaveReceiverFn receiver,
		 gpointer user_data)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (receiver != NULL, FALSE);

	if (strcmp (content_type, "text/html") == 0) {
		return html_engine_save (html->engine, receiver, user_data);
	} else if (strcmp (content_type, "text/plain") == 0) {
		return html_engine_save_plain (html->engine, receiver,
					       user_data);
	} else {
		return FALSE;
	}
}



static void
gtk_html_update_scrollbars_on_resize (GtkHTML *html,
				      gdouble old_doc_width, gdouble old_doc_height,
				      gdouble old_width, gdouble old_height,
				      gboolean *changed_x, gboolean *changed_y)
{
	GtkAdjustment *vadj, *hadj;
	gdouble doc_width, doc_height;

	/* printf ("update on resize\n"); */

	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (html));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (html));

	doc_height = html_engine_get_doc_height (html->engine);
	doc_width  = html_engine_get_doc_width (html->engine);

	if (!html->engine->keep_scroll) {
		if (old_doc_width - old_width > 0) {
			gdouble value;

			value = gtk_adjustment_get_value (hadj);
			html->engine->x_offset = (gint) (value * (doc_width - html->engine->width)
							 / (old_doc_width - old_width));

			gtk_adjustment_set_value (hadj, html->engine->x_offset);
		}

		if (old_doc_height - old_height > 0) {
			gdouble value;

			value = gtk_adjustment_get_value (vadj);
			html->engine->y_offset = (gint) (value * (doc_height - html->engine->height)
							 / (old_doc_height - old_height));
			gtk_adjustment_set_value (vadj, html->engine->y_offset);
		}
	}
}

void
gtk_html_private_calc_scrollbars (GtkHTML *html, gboolean *changed_x, gboolean *changed_y)
{
	GtkLayout *layout;
	GtkAdjustment *vadj, *hadj;
	guint layout_width, layout_height;
	gint width, height;
	gdouble value;

	if (!GTK_WIDGET_REALIZED (html))
		return;

	/* printf ("calc scrollbars\n"); */

	height = html_engine_get_doc_height (html->engine);
	width = html_engine_get_doc_width (html->engine);

	layout = GTK_LAYOUT (html);
	hadj = gtk_layout_get_hadjustment (layout);
	vadj = gtk_layout_get_vadjustment (layout);

	gtk_adjustment_set_page_size (vadj, html->engine->height);
	gtk_adjustment_set_step_increment (vadj, 14);  /* FIXME */
	gtk_adjustment_set_page_increment (vadj, html->engine->height);

	value = gtk_adjustment_get_value (vadj);
	if (value > height - html->engine->height) {
		gtk_adjustment_set_value (vadj, height - html->engine->height);
		if (changed_y)
			*changed_y = TRUE;
	}

	gtk_adjustment_set_page_size (hadj, html->engine->width);
	gtk_adjustment_set_step_increment (hadj, 14);  /* FIXME */
	gtk_adjustment_set_page_increment (hadj, html->engine->width);

	gtk_layout_get_size (layout, &layout_width, &layout_height);
	if ((width != layout_width) || (height != layout_height)) {
		g_signal_emit (html, signals [SIZE_CHANGED], 0);
		gtk_layout_set_size (layout, width, height);
	}

	value = gtk_adjustment_get_value (hadj);
	if (value > width - html->engine->width || value > MAX_WIDGET_WIDTH - html->engine->width) {
		gtk_adjustment_set_value (hadj, MIN (width - html->engine->width, MAX_WIDGET_WIDTH - html->engine->width));
		if (changed_x)
			*changed_x = TRUE;
	}

}



#ifdef USE_PROPS
static void
gtk_html_set_property (GObject        *object,
		       guint           prop_id,
		       const GValue   *value,
		       GParamSpec     *pspec)
{
	GtkHTML *html = GTK_HTML (object);

	switch (prop_id) {
	case PROP_EDITABLE:
		gtk_html_set_editable (html, g_value_get_boolean (value));
		break;
	case PROP_TITLE:
		gtk_html_set_title (html, g_value_get_string (value));
		break;
	case PROP_DOCUMENT_BASE:
		gtk_html_set_base (html, g_value_get_string (value));
		break;
	case PROP_TARGET_BASE:
		/* This doesn't do anything yet */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_html_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	GtkHTML *html = GTK_HTML (object);

	switch (prop_id) {
	case PROP_EDITABLE:
		g_value_set_boolean (value, gtk_html_get_editable (html));
		break;
	case PROP_TITLE:
		g_value_set_static_string (value, gtk_html_get_title (html));
		break;
	case PROP_DOCUMENT_BASE:
		g_value_set_static_string (value, gtk_html_get_base (html));
		break;
	case PROP_TARGET_BASE:
		g_value_set_static_string (value, gtk_html_get_base (html));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
#endif

void
gtk_html_set_editable (GtkHTML *html,
		       gboolean editable)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_editable (html->engine, editable);

	if (editable)
		gtk_html_update_styles (html);
}

gboolean
gtk_html_get_editable  (const GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return html_engine_get_editable (html->engine);
}

void
gtk_html_set_inline_spelling (GtkHTML *html,
			      gboolean inline_spell)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->priv->inline_spelling = inline_spell;

	if (gtk_html_get_editable (html) && html->priv->inline_spelling)
		html_engine_spell_check (html->engine);
	else
		html_engine_clear_spell_check (html->engine);
}

gboolean
gtk_html_get_inline_spelling (const GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return html->priv->inline_spelling;
}

void
gtk_html_set_magic_links (GtkHTML *html,
			  gboolean links)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->priv->magic_links = links;
}

gboolean
gtk_html_get_magic_links (const GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return	html->priv->magic_links;
}

void
gtk_html_set_magic_smileys (GtkHTML *html,
			    gboolean smile)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->priv->magic_smileys = smile;
}

gboolean
gtk_html_get_magic_smileys (const GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return	html->priv->magic_smileys;
}

static void
frame_set_animate (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (HTML_IS_FRAME (o)) {
		html_image_factory_set_animate (GTK_HTML (HTML_FRAME (o)->html)->engine->image_factory,
						*(gboolean *)data);
	} else if (HTML_IS_IFRAME (o)) {
		html_image_factory_set_animate (GTK_HTML (HTML_IFRAME (o)->html)->engine->image_factory,
						*(gboolean *)data);
	}
}

void
gtk_html_set_caret_mode(GtkHTML * html, gboolean caret_mode)
{
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (HTML_IS_ENGINE (html->engine));

	set_caret_mode(html->engine, caret_mode);
}

gboolean
gtk_html_get_caret_mode(const GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (HTML_IS_ENGINE (html->engine), FALSE);

	return html->engine->caret_mode;
}

/**
 * gtk_html_set_caret_first_focus_anchor:
 * When setting focus to the GtkHTML first time and is in caret mode,
 * then looks for an anchor of name @param name and tries to set focus
 * just after it. If NULL, then behaves as before.
 *
 * @param html GtkHTML instance.
 * @param name Name of the anchor to be set the first focus just after it,
 *             or NULL to not look for the anchor.
 **/
void
gtk_html_set_caret_first_focus_anchor (GtkHTML *html, const gchar *name)
{
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (html->priv != NULL);

	g_free (html->priv->caret_first_focus_anchor);
	html->priv->caret_first_focus_anchor = g_strdup (name);
}

void
gtk_html_set_animate (GtkHTML *html, gboolean animate)
{
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (HTML_IS_ENGINE (html->engine));

	html_image_factory_set_animate (html->engine->image_factory, animate);
	if (html->engine->clue)
		html_object_forall (html->engine->clue, html->engine, frame_set_animate, &animate);
}

gboolean
gtk_html_get_animate (const GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (HTML_IS_ENGINE (html->engine), FALSE);

	return html_image_factory_get_animate (html->engine->image_factory);
}

void
gtk_html_load_empty (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_load_empty (html->engine);
}

void
gtk_html_load_from_string  (GtkHTML *html, const gchar *str, gint len)
{
	GtkHTMLStream *stream;

	stream = gtk_html_begin_content (html, "text/html; charset=utf-8");
	gtk_html_stream_write (stream, str, (len == -1) ? strlen (str) : len);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
}

void
gtk_html_set_base (GtkHTML *html, const gchar *url)
{
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_href (html->engine, url);

}

const gchar *
gtk_html_get_base (GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	return html_engine_get_href (html->engine);
}


/* Printing.  */
void
gtk_html_print_page (GtkHTML *html, GtkPrintContext *context)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_print (html->engine, context, .0, .0, NULL, NULL, NULL);
}

void
gtk_html_print_page_with_header_footer (GtkHTML *html,
					GtkPrintContext *context,
					gdouble header_height,
					gdouble footer_height,
					GtkHTMLPrintCallback header_print,
					GtkHTMLPrintCallback footer_print,
					gpointer user_data)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_print (
		html->engine, context, header_height, footer_height,
		header_print, footer_print, user_data);
}


/* Editing.  */

void
gtk_html_set_paragraph_style (GtkHTML *html,
			      GtkHTMLParagraphStyle style)
{
	HTMLClueFlowStyle current_style;
	HTMLClueFlowStyle clueflow_style;
	HTMLListType item_type;
	HTMLListType cur_item_type;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	/* FIXME precondition: check if it's a valid style.  */

	paragraph_style_to_clueflow_style (style, &clueflow_style, &item_type);

	html_engine_get_current_clueflow_style (html->engine, &current_style, &cur_item_type);
	if (!html_engine_is_selection_active (html->engine) && current_style == clueflow_style
	    && (current_style != HTML_CLUEFLOW_STYLE_LIST_ITEM || item_type == cur_item_type))
		return;

	if (! html_engine_set_clueflow_style (html->engine, clueflow_style, item_type, 0, 0, NULL,
					      HTML_ENGINE_SET_CLUEFLOW_STYLE, HTML_UNDO_UNDO, TRUE))
		return;

	html->priv->paragraph_style = style;

	g_signal_emit (html, signals[CURRENT_PARAGRAPH_STYLE_CHANGED], 0, style);
	queue_draw (html);
}

GtkHTMLParagraphStyle
gtk_html_get_paragraph_style (GtkHTML *html)
{
	HTMLClueFlowStyle style;
	HTMLListType item_type;

	html_engine_get_current_clueflow_style (html->engine, &style, &item_type);

	return clueflow_style_to_paragraph_style (style, item_type);
}

guint
gtk_html_get_paragraph_indentation (GtkHTML *html)
{
	return html_engine_get_current_clueflow_indentation (html->engine);
}

void
gtk_html_set_indent (GtkHTML *html,
		     GByteArray *levels)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_clueflow_style (html->engine, 0, 0, 0,
					levels ? levels->len : 0,
					levels ? levels->data : NULL,
					HTML_ENGINE_SET_CLUEFLOW_INDENTATION, HTML_UNDO_UNDO, TRUE);

	gtk_html_update_styles (html);
}

static void
gtk_html_modify_indent_by_delta (GtkHTML *html,
				 gint delta, guint8 *levels)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_clueflow_style (html->engine, 0, 0, 0, delta, levels,
					HTML_ENGINE_SET_CLUEFLOW_INDENTATION_DELTA, HTML_UNDO_UNDO, TRUE);

	gtk_html_update_styles (html);
}

void
gtk_html_indent_push_level (GtkHTML *html, HTMLListType level_type)
{
	guint8 type = (guint8)level_type;
	gtk_html_modify_indent_by_delta (html, +1, &type);
}

void
gtk_html_indent_pop_level (GtkHTML *html)
{
	gtk_html_modify_indent_by_delta (html, -1, NULL);
}

void
gtk_html_set_font_style (GtkHTML *html,
			 GtkHTMLFontStyle and_mask,
			 GtkHTMLFontStyle or_mask)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (html_engine_set_font_style (html->engine, and_mask, or_mask))
		g_signal_emit (html, signals [INSERTION_FONT_STYLE_CHANGED], 0, html->engine->insertion_font_style);
}

void
gtk_html_set_color (GtkHTML *html, HTMLColor *color)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (html_engine_set_color (html->engine, color))
		g_signal_emit (html, signals [INSERTION_COLOR_CHANGED], 0, html->engine->insertion_color);
}

void
gtk_html_toggle_font_style (GtkHTML *html,
			    GtkHTMLFontStyle style)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (HTML_IS_PLAIN_PAINTER (html->engine->painter))
		return;

	if (html_engine_toggle_font_style (html->engine, style))
		g_signal_emit (html, signals [INSERTION_FONT_STYLE_CHANGED], 0, html->engine->insertion_font_style);
}

GtkHTMLParagraphAlignment
gtk_html_get_paragraph_alignment (GtkHTML *html)
{
	/* This makes the function return a HTMLHalignType. Should the blow call really be
         * html_alignment_to_paragraph()?
         */

	return paragraph_alignment_to_html (html_engine_get_current_clueflow_alignment (html->engine));
}

void
gtk_html_set_paragraph_alignment (GtkHTML *html,
				  GtkHTMLParagraphAlignment alignment)
{
	HTMLHAlignType align;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	align = paragraph_alignment_to_html (alignment);

	if (html_engine_set_clueflow_style (html->engine, 0, 0, align, 0, NULL,
					    HTML_ENGINE_SET_CLUEFLOW_ALIGNMENT, HTML_UNDO_UNDO, TRUE)) {
		html->priv->paragraph_alignment = alignment;
		g_signal_emit (html,  signals [CURRENT_PARAGRAPH_ALIGNMENT_CHANGED], 0, alignment);
	}
}


/* Clipboard operations.  */

static void
free_contents (ClipboardContents *contents)
{
	if (contents->html_text)
		g_free (contents->html_text);
	if (contents->plain_text)
		g_free (contents->plain_text);

	contents->html_text = NULL;
	contents->plain_text = NULL;

	g_free (contents);
}

static void
clipboard_get_contents_cb (GtkClipboard     *clipboard,
                           GtkSelectionData *selection_data,
                           guint             info,
                           gpointer          data)
{
	ClipboardContents *contents = (ClipboardContents *) data;

	if (info == TARGET_HTML && contents->html_text) {
		gtk_selection_data_set (selection_data,
					gdk_atom_intern ("text/html", FALSE), 8,
					(const guchar *) contents->html_text,
					(gint )strlen (contents->html_text));
	} else if (contents->plain_text) {
		gtk_selection_data_set_text (selection_data,
					     contents->plain_text,
					     (gint )strlen (contents->plain_text));
	}
}

static void
clipboard_clear_contents_cb (GtkClipboard *clipboard,
                             gpointer      data)
{
	ClipboardContents *contents = (ClipboardContents *) data;

	free_contents (contents);
}

static void
clipboard_paste_received_cb (GtkClipboard     *clipboard,
			     GtkSelectionData *selection_data,
			     gpointer          user_data)
{
	gint i = 0;
	GtkWidget *widget = GTK_WIDGET (user_data);
	GdkAtom data_type;
	GdkAtom target;
	gboolean as_cite;
	HTMLEngine *e;
	const guchar *data;
	gint length;

	e = GTK_HTML (widget)->engine;
	as_cite = GTK_HTML (widget)->priv->selection_as_cite;

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);
	target = gtk_selection_data_get_target (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	if (length > 0) {
		gchar *utf8 = NULL;

		if (data_type == gdk_atom_intern (selection_targets[TARGET_HTML].target, FALSE)) {
			if (length > 1 &&
			    !g_utf8_validate ((const gchar *) data, length - 1, NULL)) {
				utf8 = utf16_to_utf8_with_bom_check ((guchar *) data, length);

			} else {
				utf8 = utf8_filter_out_bom (g_strndup ((const gchar *) data, length));
			}

			if (as_cite && utf8) {
				gchar *cite;

				cite = g_strdup_printf ("<br><blockquote type=\"cite\">%s</blockquote>", utf8);

				g_free (utf8);
				utf8 = cite;
			}
			if (utf8)
				gtk_html_insert_html (GTK_HTML (widget), utf8);
			else
				g_warning ("selection was empty");
		} else if ((utf8 = (gchar *) gtk_selection_data_get_text (selection_data))) {
			utf8 = utf8_filter_out_bom (utf8);
			if (as_cite) {
				gchar *encoded;

				encoded = html_encode_entities (utf8, g_utf8_strlen (utf8, -1), NULL);
				g_free (utf8);
				utf8 = g_strdup_printf ("<br><pre><blockquote type=\"cite\">%s</blockquote></pre>",
						encoded);
				g_free (encoded);
				gtk_html_insert_html (GTK_HTML (widget), utf8);
			} else {
				html_engine_paste_text (e, utf8, g_utf8_strlen (utf8, -1));
			}

			if (HTML_IS_TEXT (e->cursor->object))
				html_text_magic_link (HTML_TEXT (e->cursor->object), e, 1);
		}

		if (utf8)
			g_free (utf8);

		return;
	}

	while (i < n_selection_targets - 1) {
		if (target == gdk_atom_intern (selection_targets[i].target, FALSE))
			break;
		i++;
	}

	if (i < n_selection_targets - 1) {
		GTK_HTML (widget)->priv->selection_type = i + 1;
		gtk_clipboard_request_contents (clipboard,
						gdk_atom_intern (selection_targets[i + 1].target, FALSE),
						clipboard_paste_received_cb,
						widget);
	}
}

static ClipboardContents *
create_clipboard_contents (GtkHTML *html)
{
	ClipboardContents *contents;
	gint html_len, text_len;

	contents = g_new0 (ClipboardContents, 1);

	/* set html text */
	contents->html_text = get_selection_string (html, &html_len, FALSE, FALSE, TRUE);

	/* set plain text */
	contents->plain_text = get_selection_string (html, &text_len, FALSE, FALSE, FALSE);

	return contents;
}

void
gtk_html_cut (GtkHTML *html)
{
	GtkClipboard *clipboard;
	ClipboardContents *contents;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_cut (html->engine);

	contents = create_clipboard_contents (html);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (html), GDK_SELECTION_CLIPBOARD);

	if (!gtk_clipboard_set_with_data (clipboard, selection_targets, n_selection_targets,
					  clipboard_get_contents_cb,
					  clipboard_clear_contents_cb,
					  contents)) {
		free_contents (contents);
	} else {
		gtk_clipboard_set_can_store (clipboard,
					     selection_targets + 1,
					     n_selection_targets - 1);
	}
}

void
gtk_html_copy (GtkHTML *html)
{
	GtkClipboard *clipboard;
	ClipboardContents *contents;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_copy (html->engine);

	contents = create_clipboard_contents (html);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (html), GDK_SELECTION_CLIPBOARD);

	if (!gtk_clipboard_set_with_data (clipboard, selection_targets, n_selection_targets,
					  clipboard_get_contents_cb,
					  clipboard_clear_contents_cb,
					  contents)) {
		free_contents (contents);
	}
	gtk_clipboard_set_can_store (clipboard,
			NULL,
			0);
}

void
gtk_html_paste (GtkHTML *html, gboolean as_cite)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	g_object_ref (html);
	html->priv->selection_as_cite = as_cite;
	html->priv->selection_type = 0;

	gtk_clipboard_request_contents (gtk_widget_get_clipboard (GTK_WIDGET (html), GDK_SELECTION_CLIPBOARD),
					gdk_atom_intern (selection_targets[0].target, FALSE),
					clipboard_paste_received_cb, html);
}

static void
update_primary_selection (GtkHTML *html)
{
	GtkClipboard *clipboard;
	gint text_len;
	gchar *text;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (!html->allow_selection)
		return;

	text = get_selection_string (html, &text_len, FALSE, TRUE, FALSE);
	if (!text)
		return;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (html), GDK_SELECTION_PRIMARY);

	gtk_clipboard_set_text (clipboard, text, text_len);

	g_free (text);
}


/* Undo/redo.  */

void
gtk_html_undo (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_undo (html->engine);
	gtk_html_update_styles (html);
}

void
gtk_html_redo (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_redo (html->engine);
	gtk_html_update_styles (html);
}

/* misc utils */
/* if engine_type == false - default behaviour*/
void
gtk_html_set_default_engine(GtkHTML *html, gboolean engine_type)
{
	html_engine_set_engine_type( html->engine, engine_type);
}

gboolean
gtk_html_get_default_engine(GtkHTML *html)
{
	return html_engine_get_engine_type( html->engine);
}

/**
 * gtk_html_set_default_content_type:
 * @html: the GtkHTML widget
 * @content_type: default content for main page.
 *
 * Set default @content_type for main page in @html widget.
 **/
void
gtk_html_set_default_content_type (GtkHTML *html, const gchar *content_type)
{
    html_engine_set_content_type( html->engine, content_type);
}

/**
 * gtk_html_get_default_content_type:
 * @html: the GtkHTML widget
 *
 * Returns: default content_type for main page in @html widget.
 **/
const gchar *
gtk_html_get_default_content_type (GtkHTML *html)
{
    return html_engine_get_content_type( html->engine);
}

gpointer
gtk_html_get_object_by_id (GtkHTML *html, const gchar *id)
{
	g_return_val_if_fail (html, NULL);
	g_return_val_if_fail (id, NULL);
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);
	g_return_val_if_fail (html->engine, NULL);

	return html_engine_get_object_by_id (html->engine, id);
}

/*******************************************

   keybindings

*/

static gint
get_line_height (GtkHTML *html)
{
	gint w, a, d;

	if (!html->engine || !html->engine->painter)
		return 0;

	html_painter_set_font_style (html->engine->painter, GTK_HTML_FONT_STYLE_SIZE_3);
	html_painter_set_font_face (html->engine->painter, NULL);
	html_painter_calc_text_size (html->engine->painter, "a", 1, &w, &a, &d);

	return a + d;
}

static void
scroll (GtkHTML *html,
	GtkOrientation orientation,
	GtkScrollType  scroll_type,
	gfloat         position)
{
	GtkAdjustment *adjustment;
	gint line_height;
	gfloat delta;
	gdouble value;
	gdouble lower;
	gdouble upper;
	gdouble page_size;
	gdouble page_increment;
	gdouble step_increment;

	/* we dont want scroll in editable (move cursor instead) */
	if (html_engine_get_editable (html->engine) || html->engine->caret_mode)
		return;

	adjustment = (orientation == GTK_ORIENTATION_VERTICAL) ?
		gtk_layout_get_vadjustment (GTK_LAYOUT (html)) :
		gtk_layout_get_hadjustment (GTK_LAYOUT (html));

	value = gtk_adjustment_get_value (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	page_increment = gtk_adjustment_get_page_increment (adjustment);
	step_increment = gtk_adjustment_get_step_increment (adjustment);

	line_height = (html->engine && page_increment > (3 * get_line_height (html)))
		? get_line_height (html)
		: 0;

	switch (scroll_type) {
	case GTK_SCROLL_STEP_FORWARD:
		delta = step_increment;
		break;
	case GTK_SCROLL_STEP_BACKWARD:
		delta = -step_increment;
		break;
	case GTK_SCROLL_PAGE_FORWARD:
		delta = page_increment - line_height;
		break;
	case GTK_SCROLL_PAGE_BACKWARD:
		delta = -page_increment + line_height;
		break;
	default:
		g_warning ("invalid scroll parameters: %d %d %f\n", orientation, scroll_type, position);
		return;
	}

	if (position == 1.0) {
		if (lower > (value + delta)) {
			if (lower >= value) {
				html->binding_handled = FALSE;
				return;
		}
		} else if (MAX (0.0, upper - page_size) < (value + delta)) {

			if (MAX (0.0, upper - page_size) <= value) {
				html->binding_handled = FALSE;
				return;
			}
		}
	}

	gtk_adjustment_set_value (adjustment, CLAMP (value + delta, lower, MAX (0.0, upper - page_size)));

	html->binding_handled = TRUE;
}

static gboolean
scroll_command (GtkHTML *html,
	GtkScrollType  scroll_type)
{
	GtkAdjustment *adjustment;
	gint line_height;
	gfloat delta = 0;
	gdouble value;
	gdouble lower;
	gdouble upper;
	gdouble page_increment;
	gdouble page_size;

	/* we dont want scroll in editable (move cursor instead) */
	if (html_engine_get_editable (html->engine) || html->engine->caret_mode)
		return FALSE;

	adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));
	value = gtk_adjustment_get_value (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_increment = gtk_adjustment_get_page_increment (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	line_height = (html->engine && page_increment > (3 * get_line_height (html)))
		? get_line_height (html)
		: 0;

	switch (scroll_type) {
	case GTK_SCROLL_PAGE_FORWARD:
		delta = page_increment - line_height;
		break;
	case GTK_SCROLL_PAGE_BACKWARD:
		delta = -page_increment + line_height;
		break;
	default:
		break;
		return FALSE;
	}

	d_s(printf("%f %f %f\n", value + delta, lower, MAX (0.0, upper - page_size));)

	if (lower > (value + delta)) {
		if (lower >= value)
			return FALSE;
	} else if (MAX (0.0, upper - page_size) < (value + delta)) {

		if (MAX (0.0, upper - page_size) <= value) {
			return FALSE;
		}
	}

	gtk_adjustment_set_value (adjustment, CLAMP (value + delta, lower, MAX (0.0, upper - page_size)));

	return TRUE;
}

static void
scroll_by_amount (GtkHTML *html, gint amount)
{
	GtkAdjustment *adjustment;
	gdouble value;
	gdouble lower;
	gdouble upper;
	gdouble page_size;

	adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));
	value = gtk_adjustment_get_value (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	gtk_adjustment_set_value (
		adjustment, CLAMP (value + (gfloat) amount,
		lower, MAX (0.0, upper - page_size)));
}

static void
cursor_move (GtkHTML *html, GtkDirectionType dir_type, GtkHTMLCursorSkipType skip)
{
	gint amount;

	if (!html->engine->caret_mode && !html_engine_get_editable (html->engine))
		return;

	html->priv->cursor_moved = TRUE;

	if (skip == GTK_HTML_CURSOR_SKIP_NONE) {
		update_primary_selection (html);
		g_signal_emit (GTK_HTML (html), signals [CURSOR_CHANGED], 0);
		return;
	}

	if (html->engine->selection_mode) {
		if (!html->engine->mark)
			html_engine_set_mark (html->engine);
	} else if (html->engine->shift_selection || html->engine->mark) {
		html_engine_disable_selection (html->engine);
		html_engine_edit_selection_updater_schedule (html->engine->selection_updater);
		html->engine->shift_selection = FALSE;
	}
	switch (skip) {
	case GTK_HTML_CURSOR_SKIP_ONE:
		switch (dir_type) {
		case GTK_DIR_LEFT:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_LEFT, 1);
			break;
		case GTK_DIR_RIGHT:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_RIGHT, 1);
			break;
		case GTK_DIR_UP:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_UP, 1);
			break;
		case GTK_DIR_DOWN:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_DOWN, 1);
			break;
		default:
			g_warning ("invalid cursor_move parameters\n");
		}
		break;
	case GTK_HTML_CURSOR_SKIP_WORD:
		switch (dir_type) {
		case GTK_DIR_UP:
		case GTK_DIR_LEFT:
			html_engine_backward_word (html->engine);
			break;
		case GTK_DIR_DOWN:
		case GTK_DIR_RIGHT:
			html_engine_forward_word (html->engine);
			break;
		default:
			g_warning ("invalid cursor_move parameters\n");
		}
		break;
	case GTK_HTML_CURSOR_SKIP_PAGE: {
		gint line_height;

		line_height =  GTK_WIDGET (html)->allocation.height > (3 * get_line_height (html))
			? get_line_height (html) : 0;

		switch (dir_type) {
		case GTK_DIR_UP:
		case GTK_DIR_LEFT:
			if ((amount = html_engine_scroll_up (html->engine,
							     GTK_WIDGET (html)->allocation.height - line_height)) > 0)
				scroll_by_amount (html, - amount);
			break;
		case GTK_DIR_DOWN:
		case GTK_DIR_RIGHT:
			if ((amount = html_engine_scroll_down (html->engine,
							       GTK_WIDGET (html)->allocation.height - line_height)) > 0)
				scroll_by_amount (html, amount);
			break;
		default:
			g_warning ("invalid cursor_move parameters\n");
		}
		break;
	}
	case GTK_HTML_CURSOR_SKIP_ALL:
		switch (dir_type) {
		case GTK_DIR_LEFT:
			html_engine_beginning_of_line (html->engine);
			break;
		case GTK_DIR_RIGHT:
			html_engine_end_of_line (html->engine);
			break;
		case GTK_DIR_UP:
			html_engine_beginning_of_document (html->engine);
			break;
		case GTK_DIR_DOWN:
			html_engine_end_of_document (html->engine);
			break;
		default:
			g_warning ("invalid cursor_move parameters\n");
		}
		break;
	default:
		g_warning ("invalid cursor_move parameters\n");
	}

	html->binding_handled = TRUE;
	html->priv->update_styles = TRUE;
	gtk_html_edit_make_cursor_visible (html);
	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
	g_signal_emit (GTK_HTML (html), signals [CURSOR_CHANGED], 0);
}

static gboolean
move_selection (GtkHTML *html, GtkHTMLCommandType com_type)
{
	gboolean rv;
	gint amount;

	if (!html_engine_get_editable (html->engine) && !html->engine->caret_mode)
		return FALSE;

	html->priv->cursor_moved = TRUE;
	html->engine->shift_selection = TRUE;
	if (!html->engine->mark)
		html_engine_set_mark (html->engine);
	switch (com_type) {
	case GTK_HTML_COMMAND_MODIFY_SELECTION_UP:
		rv = html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_UP, 1) > 0 ? TRUE : FALSE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_DOWN:
		rv = html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_DOWN, 1) > 0 ? TRUE : FALSE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_LEFT:
		rv = html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_LEFT, 1) > 0 ? TRUE : FALSE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_RIGHT:
		rv = html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_RIGHT, 1) > 0 ? TRUE : FALSE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOL:
		rv = html_engine_beginning_of_line (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOL:
		rv = html_engine_end_of_line (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOD:
		html_engine_beginning_of_document (html->engine);
		rv = TRUE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOD:
		html_engine_end_of_document (html->engine);
		rv = TRUE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PREV_WORD:
		rv = html_engine_backward_word (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_NEXT_WORD:
		rv = html_engine_forward_word (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEUP:
		if ((amount = html_engine_scroll_up (html->engine, GTK_WIDGET (html)->allocation.height)) > 0) {
			scroll_by_amount (html, - amount);
			rv = TRUE;
		} else
			rv = FALSE;
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEDOWN:
		if ((amount = html_engine_scroll_down (html->engine, GTK_WIDGET (html)->allocation.height)) > 0) {
			scroll_by_amount (html, amount);
			rv = TRUE;
		} else
			rv = FALSE;
		break;
	default:
		g_warning ("invalid move_selection parameters\n");
		rv = FALSE;
	}

	html->binding_handled = TRUE;
	html->priv->update_styles = TRUE;

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);

	update_primary_selection (html);

	return rv;
}

inline static void
delete_one (HTMLEngine *e, gboolean forward)
{
	if (e->cursor->object && html_object_is_container (e->cursor->object)
	    && ((forward && !e->cursor->offset) || (!forward && e->cursor->offset)))
		html_engine_delete_container (e);
	else
		html_engine_delete_n (e, 1, forward);
}

inline static gboolean
insert_tab_or_next_cell (GtkHTML *html)
{
	HTMLEngine *e = html->engine;
	if (!html_engine_next_cell (e, TRUE)) {
		if (html_clueflow_tabs (HTML_CLUEFLOW (e->cursor->object->parent), e->painter))
			html_engine_paste_text (e, "\t", 1);
		else
			html_engine_paste_text (e, "\xc2\xa0\xc2\xa0\xc2\xa0\xc2\xa0", 4);
		return TRUE;
	}

	return TRUE;
}

inline static void
indent_more_or_next_cell (GtkHTML *html)
{
	if (!html_engine_next_cell (html->engine, TRUE))
		gtk_html_indent_push_level (html, HTML_LIST_TYPE_BLOCKQUOTE);
}

static gboolean
command (GtkHTML *html, GtkHTMLCommandType com_type)
{
	HTMLEngine *e = html->engine;
	gboolean rv = TRUE;

	/* printf ("command %d %s\n", com_type, get_value_nick (com_type)); */
	html->binding_handled = TRUE;

	#define ensure_key_binding_not_editable() \
		if (html->priv->in_key_binding && html_engine_get_editable (e)) { \
			html->binding_handled = FALSE; \
			rv = FALSE; \
			break; \
		}

	/* non-editable + editable commands */
	switch (com_type) {
	case GTK_HTML_COMMAND_ZOOM_IN:
		ensure_key_binding_not_editable ();
		gtk_html_zoom_in (html);
		break;
	case GTK_HTML_COMMAND_ZOOM_OUT:
		ensure_key_binding_not_editable ();
		gtk_html_zoom_out (html);
		break;
	case GTK_HTML_COMMAND_ZOOM_RESET:
		ensure_key_binding_not_editable ();
		gtk_html_zoom_reset (html);
		break;
	case GTK_HTML_COMMAND_SEARCH_INCREMENTAL_FORWARD:
		gtk_html_isearch (html, TRUE);
		break;
	case GTK_HTML_COMMAND_SEARCH_INCREMENTAL_BACKWARD:
		gtk_html_isearch (html, FALSE);
		break;
	case GTK_HTML_COMMAND_FOCUS_FORWARD:
		if (!html_engine_get_editable (e))
			html->binding_handled = gtk_widget_child_focus (GTK_WIDGET (html), GTK_DIR_TAB_FORWARD);
		break;
	case GTK_HTML_COMMAND_FOCUS_BACKWARD:
		if (!html_engine_get_editable (e))
			html->binding_handled = gtk_widget_child_focus (GTK_WIDGET (html), GTK_DIR_TAB_BACKWARD);
		break;
	case GTK_HTML_COMMAND_SCROLL_FORWARD:
		rv = scroll_command (html, GTK_SCROLL_PAGE_FORWARD);
		break;
	case GTK_HTML_COMMAND_SCROLL_BACKWARD:
		rv = scroll_command (html, GTK_SCROLL_PAGE_BACKWARD);
		break;
	case GTK_HTML_COMMAND_SCROLL_BOD:
		if (!html_engine_get_editable (e) && !e->caret_mode)
			gtk_adjustment_set_value (gtk_layout_get_vadjustment (GTK_LAYOUT (html)), 0);
		break;
	case GTK_HTML_COMMAND_SCROLL_EOD:
		if (!html_engine_get_editable (e) && !e->caret_mode) {
			GtkAdjustment *adjustment;
			gdouble upper, page_size;

			adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));
			upper = gtk_adjustment_get_upper (adjustment);
			page_size = gtk_adjustment_get_page_size (adjustment);
			gtk_adjustment_set_value (adjustment, upper - page_size);
		}
		break;
	case GTK_HTML_COMMAND_COPY:
		gtk_html_copy (html);
		break;

	case GTK_HTML_COMMAND_MODIFY_SELECTION_UP:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_DOWN:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_LEFT:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_RIGHT:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOL:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOL:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOD:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOD:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEUP:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEDOWN:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PREV_WORD:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_NEXT_WORD:
		if (html->engine->caret_mode || html_engine_get_editable(e)) {
			gtk_im_context_reset (html->priv->im_context);
			rv = move_selection (html, com_type);
		}
		break;
	case GTK_HTML_COMMAND_SELECT_ALL:
		gtk_html_select_all (html);
		break;
	case GTK_HTML_COMMAND_EDITABLE_ON:
		gtk_html_set_editable (html, TRUE);
		break;
	case GTK_HTML_COMMAND_EDITABLE_OFF:
		gtk_html_set_editable (html, FALSE);
		break;
	case GTK_HTML_COMMAND_BLOCK_SELECTION:
		html_engine_block_selection (html->engine);
		break;
	case GTK_HTML_COMMAND_UNBLOCK_SELECTION:
		html_engine_unblock_selection (html->engine);
		break;
	case GTK_HTML_COMMAND_IS_SELECTION_ACTIVE:
		rv = html_engine_is_selection_active (html->engine);
		break;
	case GTK_HTML_COMMAND_UNSELECT_ALL:
		gtk_html_unselect_all (html);
		break;

	default:
		rv = FALSE;
		html->binding_handled = FALSE;
	}

	#undef ensure_key_binding_not_editable

	if (!html_engine_get_editable (e) || html->binding_handled)
		return rv;

	html->binding_handled = TRUE;
	html->priv->update_styles = FALSE;

	/* editable commands only */
	switch (com_type) {
	case GTK_HTML_COMMAND_UNDO:
		gtk_html_undo (html);
		break;
	case GTK_HTML_COMMAND_REDO:
		gtk_html_redo (html);
		break;
	case GTK_HTML_COMMAND_COPY_AND_DISABLE_SELECTION:
		gtk_html_copy (html);
		html_engine_disable_selection (e);
		html_engine_edit_selection_updater_schedule (e->selection_updater);
		e->selection_mode = FALSE;
		break;
	case GTK_HTML_COMMAND_CUT:
		gtk_html_cut (html);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_CUT_LINE:
		html_engine_cut_line (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_PASTE:
		gtk_html_paste (html, FALSE);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_INSERT_RULE:
		html_engine_insert_rule (e, 0, 100, 2, TRUE, HTML_HALIGN_LEFT);
		break;
	case GTK_HTML_COMMAND_INSERT_PARAGRAPH:
		html_engine_delete (e);

		/* stop inserting links after newlines */
		html_engine_set_insertion_link (e, NULL, NULL);

		html_engine_insert_empty_paragraph (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE:
		if (e->mark != NULL
		    && e->mark->position != e->cursor->position)
			html_engine_delete (e);
		else
			delete_one (e, TRUE);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_BACK:
		if (html_engine_is_selection_active (e))
			html_engine_delete (e);
		else
			delete_one (e, FALSE);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_BACK_OR_INDENT_DEC:
		if (html_engine_is_selection_active (e))
			html_engine_delete (e);
		else if (html_engine_cursor_on_bop (e) && html_engine_get_indent (e) > 0
			 && e->cursor->object->parent && HTML_IS_CLUEFLOW (e->cursor->object->parent)
			 && HTML_CLUEFLOW (e->cursor->object->parent)->style != HTML_CLUEFLOW_STYLE_LIST_ITEM)
			gtk_html_indent_pop_level (html);
		else
			delete_one (e, FALSE);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_TABLE:
		html_engine_delete_table (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_TABLE_ROW:
		html_engine_delete_table_row (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_TABLE_COLUMN:
		html_engine_delete_table_column (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_DELETE_TABLE_CELL_CONTENTS:
		html_engine_delete_table_cell_contents (e);
		html->priv->update_styles = TRUE;
		break;
	case GTK_HTML_COMMAND_SELECTION_MODE:
		e->selection_mode = TRUE;
		break;
	case GTK_HTML_COMMAND_DISABLE_SELECTION:
		html_engine_disable_selection (e);
		html_engine_edit_selection_updater_schedule (e->selection_updater);
		e->selection_mode = FALSE;
		break;
	case GTK_HTML_COMMAND_BOLD_ON:
		gtk_html_set_font_style (html, GTK_HTML_FONT_STYLE_MAX, GTK_HTML_FONT_STYLE_BOLD);
		break;
	case GTK_HTML_COMMAND_BOLD_OFF:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_BOLD, 0);
		break;
	case GTK_HTML_COMMAND_BOLD_TOGGLE:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_BOLD);
		break;
	case GTK_HTML_COMMAND_ITALIC_ON:
		gtk_html_set_font_style (html, GTK_HTML_FONT_STYLE_MAX, GTK_HTML_FONT_STYLE_ITALIC);
		break;
	case GTK_HTML_COMMAND_ITALIC_OFF:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_ITALIC, 0);
		break;
	case GTK_HTML_COMMAND_ITALIC_TOGGLE:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_ITALIC);
		break;
	case GTK_HTML_COMMAND_STRIKEOUT_ON:
		gtk_html_set_font_style (html, GTK_HTML_FONT_STYLE_MAX, GTK_HTML_FONT_STYLE_STRIKEOUT);
		break;
	case GTK_HTML_COMMAND_STRIKEOUT_OFF:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_STRIKEOUT, 0);
		break;
	case GTK_HTML_COMMAND_STRIKEOUT_TOGGLE:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_STRIKEOUT);
		break;
	case GTK_HTML_COMMAND_UNDERLINE_ON:
		gtk_html_set_font_style (html, GTK_HTML_FONT_STYLE_MAX, GTK_HTML_FONT_STYLE_UNDERLINE);
		break;
	case GTK_HTML_COMMAND_UNDERLINE_OFF:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_UNDERLINE, 0);
		break;
	case GTK_HTML_COMMAND_UNDERLINE_TOGGLE:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_UNDERLINE);
		break;
	case GTK_HTML_COMMAND_SIZE_MINUS_2:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_1);
		break;
	case GTK_HTML_COMMAND_SIZE_MINUS_1:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_2);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_0:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_3);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_1:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_4);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_2:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_5);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_3:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_6);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_4:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_7);
		break;
	case GTK_HTML_COMMAND_SIZE_INCREASE:
		html_engine_font_size_inc_dec (e, TRUE);
		break;
	case GTK_HTML_COMMAND_SIZE_DECREASE:
		html_engine_font_size_inc_dec (e, FALSE);
		break;
	case GTK_HTML_COMMAND_ALIGN_LEFT:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT);
		break;
	case GTK_HTML_COMMAND_ALIGN_CENTER:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER);
		break;
	case GTK_HTML_COMMAND_ALIGN_RIGHT:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT);
		break;
	case GTK_HTML_COMMAND_INDENT_ZERO:
		gtk_html_set_indent (html, NULL);
		break;
	case GTK_HTML_COMMAND_INDENT_INC:
		gtk_html_indent_push_level (html, HTML_LIST_TYPE_BLOCKQUOTE);
		break;
	case GTK_HTML_COMMAND_INDENT_INC_OR_NEXT_CELL:
		indent_more_or_next_cell (html);
		break;
	case GTK_HTML_COMMAND_INSERT_TAB:
		if (!html_engine_is_selection_active (e)
		    && html_clueflow_tabs (HTML_CLUEFLOW (e->cursor->object->parent), e->painter))
			html_engine_insert_text (e, "\t", 1);
		break;
	case GTK_HTML_COMMAND_INSERT_TAB_OR_INDENT_MORE:
		if (!html_engine_is_selection_active (e)
		    && html_clueflow_tabs (HTML_CLUEFLOW (e->cursor->object->parent), e->painter))
			html_engine_insert_text (e, "\t", 1);
		else
			gtk_html_indent_push_level (html, HTML_LIST_TYPE_BLOCKQUOTE);
		break;
	case GTK_HTML_COMMAND_INSERT_TAB_OR_NEXT_CELL:
		html->binding_handled = insert_tab_or_next_cell (html);
		break;
	case GTK_HTML_COMMAND_INDENT_DEC:
		gtk_html_indent_pop_level (html);
		break;
	case GTK_HTML_COMMAND_PREV_CELL:
		html->binding_handled = html_engine_prev_cell (html->engine);
		break;
	case GTK_HTML_COMMAND_INDENT_PARAGRAPH:
		html_engine_indent_paragraph (e);
		break;
	case GTK_HTML_COMMAND_BREAK_AND_FILL_LINE:
		html_engine_break_and_fill_line (e);
		break;
	case GTK_HTML_COMMAND_SPACE_AND_FILL_LINE:
		html_engine_space_and_fill_line (e);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_NORMAL:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_NORMAL);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H1:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H1);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H2:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H2);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H3:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H3);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H4:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H4);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H5:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H5);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H6:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H6);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_PRE:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_PRE);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ADDRESS:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ADDRESS);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMDOTTED:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMROMAN:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMDIGIT:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMALPHA:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMALPHA);
		break;
	case GTK_HTML_COMMAND_SELECT_WORD:
		gtk_html_select_word (html);
		break;
	case GTK_HTML_COMMAND_SELECT_LINE:
		gtk_html_select_line (html);
		break;
	case GTK_HTML_COMMAND_SELECT_PARAGRAPH:
		gtk_html_select_paragraph (html);
		break;
	case GTK_HTML_COMMAND_SELECT_PARAGRAPH_EXTENDED:
		gtk_html_select_paragraph_extended (html);
		break;
	case GTK_HTML_COMMAND_CURSOR_POSITION_SAVE:
		html_engine_edit_cursor_position_save (html->engine);
		break;
	case GTK_HTML_COMMAND_CURSOR_POSITION_RESTORE:
		html_engine_edit_cursor_position_restore (html->engine);
		break;
	case GTK_HTML_COMMAND_CAPITALIZE_WORD:
		html_engine_capitalize_word (e);
		break;
	case GTK_HTML_COMMAND_UPCASE_WORD:
		html_engine_upcase_downcase_word (e, TRUE);
		break;
	case GTK_HTML_COMMAND_DOWNCASE_WORD:
		html_engine_upcase_downcase_word (e, FALSE);
		break;
	case GTK_HTML_COMMAND_SPELL_SUGGEST:
		if (html->editor_api && !html_engine_spell_word_is_valid (e))
			(*html->editor_api->suggestion_request) (html, html->editor_data);
		break;
	case GTK_HTML_COMMAND_SPELL_PERSONAL_DICTIONARY_ADD:
	case GTK_HTML_COMMAND_SPELL_SESSION_DICTIONARY_ADD: {
		gchar *word;
		word = html_engine_get_spell_word (e);

		if (word && html->editor_api) {
			if (com_type == GTK_HTML_COMMAND_SPELL_PERSONAL_DICTIONARY_ADD)
				/* FIXME fire popup menu with more than 1 language enabled */
				(*html->editor_api->add_to_personal) (html, word, html_engine_get_language (html->engine), html->editor_data);
			else
				(*html->editor_api->add_to_session) (html, word, html->editor_data);
			g_free (word);
			html_engine_spell_check (e);
			gtk_widget_queue_draw (GTK_WIDGET (html));
		}
		break;
	}
	case GTK_HTML_COMMAND_CURSOR_FORWARD:
		rv = html_cursor_forward (html->engine->cursor, html->engine);
		break;
	case GTK_HTML_COMMAND_CURSOR_BACKWARD:
		rv = html_cursor_backward (html->engine->cursor, html->engine);
		break;
	case GTK_HTML_COMMAND_INSERT_TABLE_1_1:
		html_engine_insert_table_1_1 (e);
		break;
	case GTK_HTML_COMMAND_TABLE_INSERT_COL_BEFORE:
		html_engine_insert_table_column (e, FALSE);
		break;
	case GTK_HTML_COMMAND_TABLE_INSERT_COL_AFTER:
		html_engine_insert_table_column (e, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_DELETE_COL:
		html_engine_delete_table_column (e);
		break;
	case GTK_HTML_COMMAND_TABLE_INSERT_ROW_BEFORE:
		html_engine_insert_table_row (e, FALSE);
		break;
	case GTK_HTML_COMMAND_TABLE_INSERT_ROW_AFTER:
		html_engine_insert_table_row (e, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_DELETE_ROW:
		html_engine_delete_table_row (e);
		break;
	case GTK_HTML_COMMAND_TABLE_BORDER_WIDTH_INC:
		html_engine_table_set_border_width (e, html_engine_get_table (e), 1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_BORDER_WIDTH_DEC:
		html_engine_table_set_border_width (e, html_engine_get_table (e), -1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_BORDER_WIDTH_ZERO:
		html_engine_table_set_border_width (e, html_engine_get_table (e), 0, FALSE);
		break;
	case GTK_HTML_COMMAND_TABLE_SPACING_INC:
		html_engine_table_set_spacing (e, html_engine_get_table (e), 1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_SPACING_DEC:
		html_engine_table_set_spacing (e, html_engine_get_table (e), -1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_SPACING_ZERO:
		html_engine_table_set_spacing (e, html_engine_get_table (e), 0, FALSE);
		break;
	case GTK_HTML_COMMAND_TABLE_PADDING_INC:
		html_engine_table_set_padding (e, html_engine_get_table (e), 1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_PADDING_DEC:
		html_engine_table_set_padding (e, html_engine_get_table (e), -1, TRUE);
		break;
	case GTK_HTML_COMMAND_TABLE_PADDING_ZERO:
		html_engine_table_set_padding (e, html_engine_get_table (e), 0, FALSE);
		break;
	case GTK_HTML_COMMAND_TEXT_SET_DEFAULT_COLOR:
		html_engine_set_color (e, NULL);
		break;
	case GTK_HTML_COMMAND_CURSOR_BOD:
		html_engine_beginning_of_document (e);
		break;
	case GTK_HTML_COMMAND_CURSOR_EOD:
		html_engine_end_of_document (e);
		break;
	case GTK_HTML_COMMAND_BLOCK_REDRAW:
		html_engine_block_redraw (e);
		break;
	case GTK_HTML_COMMAND_UNBLOCK_REDRAW:
		html_engine_unblock_redraw (e);
		break;
	case GTK_HTML_COMMAND_GRAB_FOCUS:
		gtk_widget_grab_focus (GTK_WIDGET (html));
		break;
	case GTK_HTML_COMMAND_KILL_WORD:
	case GTK_HTML_COMMAND_KILL_WORD_BACKWARD:
		html_engine_block_selection (e);
		html_engine_set_mark (e);
		html_engine_update_selection_if_necessary (e);
		html_engine_freeze (e);
		rv = com_type == GTK_HTML_COMMAND_KILL_WORD
			? html_engine_forward_word (e)
			: html_engine_backward_word (e);
		if (rv)
			html_engine_delete (e);
		html_engine_unblock_selection (e);
		html_engine_thaw (e);
		break;
	case GTK_HTML_COMMAND_SAVE_DATA_ON:
		html->engine->save_data = TRUE;
		break;
	case GTK_HTML_COMMAND_SAVE_DATA_OFF:
		html->engine->save_data = FALSE;
		break;
	case GTK_HTML_COMMAND_SAVED:
		html_engine_saved (html->engine);
		break;
	case GTK_HTML_COMMAND_IS_SAVED:
		rv = html_engine_is_saved (html->engine);
		break;
	case GTK_HTML_COMMAND_CELL_CSPAN_INC:
		rv = html_engine_cspan_delta (html->engine, 1);
		break;
	case GTK_HTML_COMMAND_CELL_RSPAN_INC:
		rv = html_engine_rspan_delta (html->engine, 1);
		break;
	case GTK_HTML_COMMAND_CELL_CSPAN_DEC:
		rv = html_engine_cspan_delta (html->engine, -1);
		break;
	case GTK_HTML_COMMAND_CELL_RSPAN_DEC:
		rv = html_engine_rspan_delta (html->engine, -1);
		break;
	default:
		html->binding_handled = FALSE;
	}

	if (!html->binding_handled && html->editor_api)
		html->binding_handled = (* html->editor_api->command) (html, com_type, html->editor_data);

	return rv;
}

static void
add_bindings (GtkHTMLClass *klass)
{
	GtkBindingSet *binding_set;

	/* ensure enums are defined */
	gtk_html_cursor_skip_get_type ();
	gtk_html_command_get_type ();

	binding_set = gtk_binding_set_by_class (klass);

	/* layout scrolling */
#define BSCROLL(m,key,orient,sc) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "scroll", 3, \
				      GTK_TYPE_ORIENTATION, GTK_ORIENTATION_ ## orient, \
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_ ## sc, \
				      G_TYPE_FLOAT, 0.0); \

#define BSPACESCROLL(m,key,orient,sc) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "scroll", 3, \
				      GTK_TYPE_ORIENTATION, GTK_ORIENTATION_ ## orient, \
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_ ## sc, \
				      G_TYPE_FLOAT, 1.0); \

	BSCROLL (0, Up, VERTICAL, STEP_BACKWARD);
	BSCROLL (0, KP_Up, VERTICAL, STEP_BACKWARD);
	BSCROLL (0, Down, VERTICAL, STEP_FORWARD);
	BSCROLL (0, KP_Down, VERTICAL, STEP_FORWARD);

	BSCROLL (0, Left, HORIZONTAL, STEP_BACKWARD);
	BSCROLL (0, KP_Left, HORIZONTAL, STEP_BACKWARD);
	BSCROLL (0, Right, HORIZONTAL, STEP_FORWARD);
	BSCROLL (0, KP_Right, HORIZONTAL, STEP_FORWARD);

	BSCROLL (0, Page_Up, VERTICAL, PAGE_BACKWARD);
	BSCROLL (0, KP_Page_Up, VERTICAL, PAGE_BACKWARD);
	BSCROLL (0, Page_Down, VERTICAL, PAGE_FORWARD);
	BSCROLL (0, KP_Page_Down, VERTICAL, PAGE_FORWARD);
	BSPACESCROLL (0, BackSpace, VERTICAL, PAGE_BACKWARD);
	BSPACESCROLL (0, space, VERTICAL, PAGE_FORWARD);
	BSPACESCROLL (GDK_SHIFT_MASK, space, VERTICAL, PAGE_BACKWARD);

	BSCROLL (GDK_SHIFT_MASK, Left, HORIZONTAL, PAGE_BACKWARD);
	BSCROLL (GDK_SHIFT_MASK, KP_Left, HORIZONTAL, PAGE_BACKWARD);
	BSCROLL (GDK_SHIFT_MASK, Right, HORIZONTAL, PAGE_FORWARD);
	BSCROLL (GDK_SHIFT_MASK, KP_Right, HORIZONTAL, PAGE_FORWARD);

	/* editing */

#define BMOVE(m,key,dir,sk) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "cursor_move", 2, \
				      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_ ## dir, \
				      GTK_TYPE_HTML_CURSOR_SKIP, GTK_HTML_CURSOR_SKIP_ ## sk);

	BMOVE (0, Left,     LEFT,  ONE);
	BMOVE (0, KP_Left,  LEFT,  ONE);
	BMOVE (0, Right,    RIGHT, ONE);
	BMOVE (0, KP_Right, RIGHT, ONE);
	BMOVE (0, Up,       UP  ,  ONE);
	BMOVE (0, KP_Up,    UP  ,  ONE);
	BMOVE (0, Down,     DOWN,  ONE);
	BMOVE (0, KP_Down,  DOWN,  ONE);

	BMOVE (GDK_CONTROL_MASK, KP_Left,  LEFT,  WORD);
	BMOVE (GDK_CONTROL_MASK, Left,     LEFT,  WORD);
	BMOVE (GDK_MOD1_MASK,    Left,     LEFT,  WORD);
	BMOVE (GDK_SHIFT_MASK,   Left,     LEFT,  NONE);
	BMOVE (GDK_SHIFT_MASK | GDK_CONTROL_MASK, Left,  LEFT,  NONE);

	BMOVE (GDK_CONTROL_MASK, KP_Right, RIGHT, WORD);
	BMOVE (GDK_CONTROL_MASK, Right,    RIGHT, WORD);
	BMOVE (GDK_MOD1_MASK,    Right,    RIGHT, WORD);
	BMOVE (GDK_SHIFT_MASK,   Right,    RIGHT, NONE);
	BMOVE (GDK_SHIFT_MASK | GDK_CONTROL_MASK, Right, RIGHT, NONE);

	BMOVE (0, Page_Up,       UP,   PAGE);
	BMOVE (0, KP_Page_Up,    UP,   PAGE);
	BMOVE (0, Page_Down,     DOWN, PAGE);
	BMOVE (0, KP_Page_Down,  DOWN, PAGE);

	BMOVE (0, Home, LEFT, ALL);
	BMOVE (0, KP_Home, LEFT, ALL);
	BMOVE (0, End, RIGHT, ALL);
	BMOVE (0, KP_End, RIGHT, ALL);
	BMOVE (GDK_CONTROL_MASK, Home, UP, ALL);
	BMOVE (GDK_CONTROL_MASK, KP_Home, UP, ALL);
	BMOVE (GDK_CONTROL_MASK, End, DOWN, ALL);
	BMOVE (GDK_CONTROL_MASK, KP_End, DOWN, ALL);

#define BCOM(m,key,com) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "command", 1, \
				      GTK_TYPE_HTML_COMMAND, GTK_HTML_COMMAND_ ## com);
	BCOM (0, Home, SCROLL_BOD);
	BCOM (0, KP_Home, SCROLL_BOD);
	BCOM (0, End, SCROLL_EOD);
	BCOM (0, KP_End, SCROLL_EOD);

	BCOM (GDK_CONTROL_MASK, c, COPY);

	BCOM (0, Return, INSERT_PARAGRAPH);
	BCOM (GDK_SHIFT_MASK, Return, INSERT_PARAGRAPH);
	BCOM (0, KP_Enter, INSERT_PARAGRAPH);
	BCOM (GDK_SHIFT_MASK, KP_Enter, INSERT_PARAGRAPH);
	BCOM (0, BackSpace, DELETE_BACK_OR_INDENT_DEC);
	BCOM (GDK_SHIFT_MASK, BackSpace, DELETE_BACK_OR_INDENT_DEC);
	BCOM (0, Delete, DELETE);
	BCOM (0, KP_Delete, DELETE);

	BCOM (GDK_CONTROL_MASK | GDK_SHIFT_MASK, plus, ZOOM_IN);
	BCOM (GDK_CONTROL_MASK, plus, ZOOM_IN);
	BCOM (GDK_CONTROL_MASK, equal, ZOOM_IN);
	BCOM (GDK_CONTROL_MASK, KP_Add, ZOOM_IN);
	BCOM (GDK_CONTROL_MASK, minus, ZOOM_OUT);
	BCOM (GDK_CONTROL_MASK, KP_Subtract, ZOOM_OUT);
	BCOM (GDK_CONTROL_MASK, 8, ZOOM_IN);
	BCOM (GDK_CONTROL_MASK, 9, ZOOM_RESET);
	BCOM (GDK_CONTROL_MASK, 0, ZOOM_OUT);
	BCOM (GDK_CONTROL_MASK, KP_Multiply, ZOOM_RESET);

	/* selection */
	BCOM (GDK_SHIFT_MASK, Up, MODIFY_SELECTION_UP);
	BCOM (GDK_SHIFT_MASK, Down, MODIFY_SELECTION_DOWN);
	BCOM (GDK_SHIFT_MASK, Left, MODIFY_SELECTION_LEFT);
	BCOM (GDK_SHIFT_MASK, Right, MODIFY_SELECTION_RIGHT);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, Left, MODIFY_SELECTION_PREV_WORD);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, Right, MODIFY_SELECTION_NEXT_WORD);
	BCOM (GDK_SHIFT_MASK, Home, MODIFY_SELECTION_BOL);
	BCOM (GDK_SHIFT_MASK, KP_Home, MODIFY_SELECTION_BOL);
	BCOM (GDK_SHIFT_MASK, End, MODIFY_SELECTION_EOL);
	BCOM (GDK_SHIFT_MASK, KP_End, MODIFY_SELECTION_EOL);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, Home, MODIFY_SELECTION_BOD);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, KP_Home, MODIFY_SELECTION_BOD);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, End, MODIFY_SELECTION_EOD);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, KP_End, MODIFY_SELECTION_EOD);
	BCOM (GDK_SHIFT_MASK, Page_Up, MODIFY_SELECTION_PAGEUP);
	BCOM (GDK_SHIFT_MASK, Page_Down, MODIFY_SELECTION_PAGEDOWN);
	BCOM (GDK_SHIFT_MASK, KP_Page_Up, MODIFY_SELECTION_PAGEUP);
	BCOM (GDK_SHIFT_MASK, KP_Page_Down, MODIFY_SELECTION_PAGEDOWN);
	BCOM (GDK_CONTROL_MASK, a, SELECT_ALL);
	BCOM (GDK_CONTROL_MASK, p, SELECT_PARAGRAPH);

	/* copy, cut, paste, delete */
	BCOM (GDK_CONTROL_MASK, c, COPY);
	BCOM (GDK_CONTROL_MASK, Insert, COPY);
	BCOM (GDK_CONTROL_MASK, KP_Insert, COPY);
	BCOM (0, F16, COPY);
	BCOM (GDK_CONTROL_MASK, x, CUT);
	BCOM (GDK_SHIFT_MASK, Delete, CUT);
	BCOM (GDK_SHIFT_MASK, KP_Delete, CUT);
	BCOM (0, F20, CUT);
	BCOM (GDK_CONTROL_MASK, v, PASTE);
	BCOM (GDK_SHIFT_MASK, Insert, PASTE);
	BCOM (GDK_SHIFT_MASK, KP_Insert, PASTE);
	BCOM (0, F18, PASTE);
	BCOM (GDK_CONTROL_MASK, Delete, KILL_WORD);
	BCOM (GDK_CONTROL_MASK, BackSpace, KILL_WORD_BACKWARD);

	/* font style */
	BCOM (GDK_CONTROL_MASK, b, BOLD_TOGGLE);
	BCOM (GDK_CONTROL_MASK, i, ITALIC_TOGGLE);
	BCOM (GDK_CONTROL_MASK, u, UNDERLINE_TOGGLE);
	BCOM (GDK_CONTROL_MASK, o, TEXT_COLOR_APPLY);
	BCOM (GDK_MOD1_MASK, 1, SIZE_MINUS_2);
	BCOM (GDK_MOD1_MASK, 2, SIZE_MINUS_1);
	BCOM (GDK_MOD1_MASK, 3, SIZE_PLUS_0);
	BCOM (GDK_MOD1_MASK, 4, SIZE_PLUS_1);
	BCOM (GDK_MOD1_MASK, 5, SIZE_PLUS_2);
	BCOM (GDK_MOD1_MASK, 6, SIZE_PLUS_3);
	BCOM (GDK_MOD1_MASK, 7, SIZE_PLUS_4);
	BCOM (GDK_MOD1_MASK, 8, SIZE_INCREASE);
	BCOM (GDK_MOD1_MASK, 9, SIZE_DECREASE);

	/* undo/redo */
	BCOM (GDK_CONTROL_MASK, z, UNDO);
	BCOM (0, F14, UNDO);
	BCOM (GDK_CONTROL_MASK, r, REDO);

	/* paragraph style */
	BCOM (GDK_CONTROL_MASK | GDK_MOD1_MASK, l, ALIGN_LEFT);
	BCOM (GDK_CONTROL_MASK | GDK_MOD1_MASK, r, ALIGN_RIGHT);
	BCOM (GDK_CONTROL_MASK | GDK_MOD1_MASK, c, ALIGN_CENTER);

	/* tabs */
	BCOM (0, Tab, INSERT_TAB_OR_NEXT_CELL);
	BCOM (GDK_SHIFT_MASK, Tab, PREV_CELL);

	/* spell checking */
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK, s, SPELL_SUGGEST);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK, p, SPELL_PERSONAL_DICTIONARY_ADD);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK, n, SPELL_SESSION_DICTIONARY_ADD);

	/* popup menu, properties dialog */
	BCOM (GDK_MOD1_MASK, space, POPUP_MENU);
	BCOM (GDK_MOD1_MASK, Return, PROPERTIES_DIALOG);
	BCOM (GDK_MOD1_MASK, KP_Enter, PROPERTIES_DIALOG);

	/* tables */
	BCOM (GDK_CONTROL_MASK | GDK_SHIFT_MASK, t, INSERT_TABLE_1_1);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, l, TABLE_INSERT_COL_AFTER);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, r, TABLE_INSERT_ROW_AFTER);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, s, TABLE_SPACING_INC);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, p, TABLE_PADDING_INC);
	BCOM (GDK_SHIFT_MASK | GDK_CONTROL_MASK, o, TABLE_BORDER_WIDTH_INC);
	BCOM (GDK_MOD1_MASK | GDK_CONTROL_MASK, l, TABLE_INSERT_COL_BEFORE);
	BCOM (GDK_MOD1_MASK | GDK_CONTROL_MASK, r, TABLE_INSERT_ROW_BEFORE);
	BCOM (GDK_MOD1_MASK | GDK_CONTROL_MASK, s, TABLE_SPACING_DEC);
	BCOM (GDK_MOD1_MASK | GDK_CONTROL_MASK, p, TABLE_PADDING_DEC);
	BCOM (GDK_MOD1_MASK | GDK_CONTROL_MASK, o, TABLE_BORDER_WIDTH_DEC);
	BCOM (GDK_SHIFT_MASK | GDK_MOD1_MASK, l, TABLE_DELETE_COL);
	BCOM (GDK_SHIFT_MASK | GDK_MOD1_MASK, r, TABLE_DELETE_ROW);
	BCOM (GDK_SHIFT_MASK | GDK_MOD1_MASK, s, TABLE_SPACING_ZERO);
	BCOM (GDK_SHIFT_MASK | GDK_MOD1_MASK, p, TABLE_PADDING_ZERO);
	BCOM (GDK_SHIFT_MASK | GDK_MOD1_MASK, o, TABLE_BORDER_WIDTH_ZERO);
}

gint
gtk_html_set_iframe_parent (GtkHTML *html, GtkWidget *parent, HTMLObject *frame)
{
	GtkWidget *top_level;
	gint depth = 0;
	g_assert (GTK_IS_HTML (parent));

	gtk_html_set_animate (html, gtk_html_get_animate (GTK_HTML (parent)));

	html->iframe_parent = parent;
	html->frame = frame;

	top_level = GTK_WIDGET (gtk_html_get_top_html (html));
	if (html->engine && html->engine->painter) {
		html_painter_set_widget (html->engine->painter, top_level);
		gtk_html_set_fonts (html, html->engine->painter);
	}
	g_signal_emit (top_level, signals [IFRAME_CREATED], 0, html);

	while (html->iframe_parent) {
		depth++;
		html = GTK_HTML (html->iframe_parent);
	}

	return depth;
}

void
gtk_html_select_word (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_word_editable (e);
	else
		html_engine_select_word (e);

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_select_line (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_line_editable (e);
	else
		html_engine_select_line (e);

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_select_paragraph (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_paragraph_editable (e);
	/* FIXME: does anybody need this? if so bother me. rodo
	   else
	   html_engine_select_paragraph (e); */

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_select_paragraph_extended (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_paragraph_extended (e);
	/* FIXME: does anybody need this? if so bother me. rodo
	   else
	   html_engine_select_paragraph (e); */

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_select_all (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;

	if (html_engine_get_editable (e))
		html_engine_select_all_editable (e);
	else {
		html_engine_select_all (e);
	}

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_unselect_all (GtkHTML *html)
{
	HTMLEngine *e;

	e = html->engine;

	html_engine_unselect_all (e);

	html_engine_update_selection_active_state (html->engine, html->priv->event_time);
	update_primary_selection (html);
}

void
gtk_html_api_set_language (GtkHTML *html)
{
	g_return_if_fail (GTK_IS_HTML (html));

	if (html->editor_api) {
		html->editor_api->set_language (html, html_engine_get_language (html->engine), html->editor_data);
		html_engine_spell_check (html->engine);
	}
}

void
gtk_html_set_editor_api (GtkHTML *html, GtkHTMLEditorAPI *api, gpointer data)
{
	html->editor_api  = api;
	html->editor_data = data;

	gtk_html_api_set_language (html);
}

static const gchar *
get_value_nick (GtkHTMLCommandType com_type)
{
	GEnumValue *val;
	GEnumClass *enum_class;

	enum_class = g_type_class_ref (GTK_TYPE_HTML_COMMAND);
	val = g_enum_get_value (enum_class, com_type);
	g_type_class_unref (enum_class);
	if (val)
		return val->value_nick;

	g_warning ("Invalid GTK_TYPE_HTML_COMMAND enum value %d\n", com_type);

	return NULL;
}

void
gtk_html_editor_event_command (GtkHTML *html, GtkHTMLCommandType com_type, gboolean before)
{
	GValue arg;

	memset (&arg, 0, sizeof (GValue));
	g_value_init (&arg, G_TYPE_STRING);
	g_value_set_string (&arg, get_value_nick (com_type));

	gtk_html_editor_event (html, before ? GTK_HTML_EDITOR_EVENT_COMMAND_BEFORE : GTK_HTML_EDITOR_EVENT_COMMAND_AFTER,
			       &arg);

	g_value_unset (&arg);
}

void
gtk_html_editor_event (GtkHTML *html, GtkHTMLEditorEventType event, GValue *args)
{
	GValue *retval = NULL;

	if (html->editor_api && !html->engine->block_events)
		retval = (*html->editor_api->event) (html, event, args, html->editor_data);

	if (retval) {
		g_value_unset (retval);
		g_free (retval);
	}
}

gboolean
gtk_html_command (GtkHTML *html, const gchar *command_name)
{
	GEnumClass *class;
	GEnumValue *val;

	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (command_name != NULL, FALSE);

	class = G_ENUM_CLASS (g_type_class_ref (GTK_TYPE_HTML_COMMAND));
	val = g_enum_get_value_by_nick (class, command_name);
	g_type_class_unref (class);
	if (val) {
		if (command (html, val->value)) {
			if (html->priv->update_styles)
				gtk_html_update_styles (html);
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
gtk_html_edit_make_cursor_visible (GtkHTML *html)
{
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gboolean rv = FALSE;

	g_return_val_if_fail (GTK_IS_HTML (html), rv);

	hadjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (html));
	vadjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (html));

	html_engine_hide_cursor (html->engine);
	if (html_engine_make_cursor_visible (html->engine)) {
		gtk_adjustment_set_value (hadjustment, (gfloat) html->engine->x_offset);
		gtk_adjustment_set_value (vadjustment, (gfloat) html->engine->y_offset);
		rv = TRUE;
	}
	html_engine_show_cursor (html->engine);

	return rv;
}

gboolean
gtk_html_build_with_gconf (void)
{
	return TRUE;
}

static void
reparent_embedded (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (html_object_is_embedded (o)) {
		HTMLEmbedded *eo = HTML_EMBEDDED (o);
		GtkWidget *parent = NULL;

		if (eo->widget != NULL)
			parent = gtk_widget_get_parent (eo->widget);

		if (parent && GTK_IS_HTML (parent) &&
		    GTK_HTML (parent)->iframe_parent == NULL) {
			g_object_ref (eo->widget);
			gtk_container_remove (GTK_CONTAINER (parent), eo->widget);
			g_object_force_floating (G_OBJECT (eo->widget));
		}
		eo->parent = data;
	}

	if (HTML_IS_IFRAME (o) && GTK_HTML (HTML_IFRAME (o)->html)->iframe_parent &&
	    GTK_HTML (GTK_HTML (HTML_IFRAME (o)->html)->iframe_parent)->iframe_parent == NULL)
		gtk_html_set_iframe_parent (GTK_HTML (HTML_IFRAME (o)->html), data, o);

	if (HTML_IS_FRAME (o) && GTK_HTML (HTML_FRAME (o)->html)->iframe_parent &&
	    GTK_HTML (GTK_HTML (HTML_FRAME (o)->html)->iframe_parent)->iframe_parent == NULL)
		gtk_html_set_iframe_parent (GTK_HTML (HTML_FRAME (o)->html), data, o);

	if (HTML_IS_FRAMESET (o) && HTML_FRAMESET (o)->parent &&
	    HTML_FRAMESET (o)->parent->iframe_parent == NULL) {
		HTML_FRAMESET (o)->parent = data;
	}
}

static void
gtk_html_insert_html_generic (GtkHTML *html, GtkHTML *tmp, const gchar *html_src, gboolean obj_only)
{
	GtkWidget *window, *sw;
	HTMLObject *o;

	html_engine_freeze (html->engine);
	html_engine_delete (html->engine);
	if (!tmp)
		tmp    = GTK_HTML (gtk_html_new_from_string (html_src, -1));
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	sw     = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (sw));
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (tmp));
	gtk_widget_realize (GTK_WIDGET (tmp));
	html_image_factory_move_images (html->engine->image_factory, tmp->engine->image_factory);

	/* copy the forms */
	g_list_foreach (tmp->engine->formList, (GFunc)html_form_set_engine, html->engine);

	/* move top level iframes and embedded widgets from tmp to html */
	html_object_forall (tmp->engine->clue, html->engine, reparent_embedded, html);

	if (tmp->engine->formList && html->engine->formList) {
		GList *form_last;

		form_last = g_list_last (html->engine->formList);
		tmp->engine->formList->prev = form_last;
		form_last->next = tmp->engine->formList;
	} else if (tmp->engine->formList) {
		html->engine->formList = tmp->engine->formList;
	}
	tmp->engine->formList = NULL;

	if (obj_only) {
		HTMLObject *next;
		g_return_if_fail (tmp->engine->clue && HTML_CLUE (tmp->engine->clue)->head
				  && HTML_CLUE (HTML_CLUE (tmp->engine->clue)->head)->head);

		html_undo_level_begin (html->engine->undo, "Append HTML", "Remove appended HTML");
		o = HTML_CLUE (tmp->engine->clue)->head;
		for (; o; o = next) {
			next = o->next;
			html_object_remove_child (o->parent, o);
			html_engine_append_flow (html->engine, o, html_object_get_recursive_length (o));
		}
		html_undo_level_end (html->engine->undo);
	} else {
		g_return_if_fail (tmp->engine->clue);

		o = tmp->engine->clue;
		tmp->engine->clue = NULL;
		html_engine_insert_object (html->engine, o,
					   html_object_get_recursive_length (o),
					   html_engine_get_insert_level_for_object (html->engine, o));
	}
	gtk_widget_destroy (window);
	html_engine_thaw (html->engine);
}

void
gtk_html_insert_html (GtkHTML *html, const gchar *html_src)
{
	g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_insert_html_generic (html, NULL, html_src, FALSE);
}

void
gtk_html_insert_gtk_html (GtkHTML *html, GtkHTML *to_be_destroyed)
{
	g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_insert_html_generic (html, to_be_destroyed, NULL, FALSE);
}

void
gtk_html_append_html (GtkHTML *html, const gchar *html_src)
{
	g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_insert_html_generic (html, NULL, html_src, TRUE);
}

static void
set_magnification (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (HTML_IS_FRAME (o)) {
		html_font_manager_set_magnification (&GTK_HTML (HTML_FRAME (o)->html)->engine->painter->font_manager,
						     *(gdouble *) data);
	} else if (HTML_IS_IFRAME (o)) {
		html_font_manager_set_magnification (&GTK_HTML (HTML_IFRAME (o)->html)->engine->painter->font_manager,
						     *(gdouble *) data);
	} else if (HTML_IS_TEXT (o))
		html_text_calc_font_size (HTML_TEXT (o), e);
}

void
gtk_html_set_magnification (GtkHTML *html, gdouble magnification)
{
	g_return_if_fail (GTK_IS_HTML (html));

	if (magnification > 0.05 && magnification < 20.0
	    && magnification * html->engine->painter->font_manager.var_size >= 4*PANGO_SCALE
	    && magnification * html->engine->painter->font_manager.fix_size >= 4*PANGO_SCALE) {
		html_font_manager_set_magnification (&html->engine->painter->font_manager, magnification);
		if (html->engine->clue) {
			html_object_forall (html->engine->clue, html->engine,
					    set_magnification, &magnification);
			html_object_change_set_down (html->engine->clue, HTML_CHANGE_ALL);
		}

		html_engine_schedule_update (html->engine);
	}
}

#define MAG_STEP 1.1

void
gtk_html_zoom_in (GtkHTML *html)
{
	g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_set_magnification (html, html->engine->painter->font_manager.magnification * MAG_STEP);
}

void
gtk_html_zoom_out (GtkHTML *html)
{
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (HTML_IS_ENGINE (html->engine));

	gtk_html_set_magnification (html, html->engine->painter->font_manager.magnification * (1.0/MAG_STEP));
}

void
gtk_html_zoom_reset (GtkHTML *html)
{
	g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_set_magnification (html, 1.0);
}

void
gtk_html_set_allow_frameset (GtkHTML *html, gboolean allow)
{
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (HTML_IS_ENGINE (html->engine));

	html->engine->allow_frameset = allow;
}

gboolean
gtk_html_get_allow_frameset (GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (HTML_IS_ENGINE (html->engine), FALSE);

	return html->engine->allow_frameset;
}

void
gtk_html_images_ref (GtkHTML *html)
{
	html_image_factory_ref_all_images (HTML_IMAGE_FACTORY (html->engine->image_factory));
}

void
gtk_html_images_unref (GtkHTML *html)
{
	html_image_factory_unref_all_images (HTML_IMAGE_FACTORY (html->engine->image_factory));
}

void
gtk_html_image_ref (GtkHTML *html, const gchar *url)
{
	html_image_factory_ref_image_ptr (HTML_IMAGE_FACTORY (html->engine->image_factory), url);
}

void
gtk_html_image_unref (GtkHTML *html, const gchar *url)
{
	html_image_factory_unref_image_ptr (HTML_IMAGE_FACTORY (html->engine->image_factory), url);
}

void
gtk_html_image_preload (GtkHTML *html, const gchar *url)
{
	html_image_factory_register (HTML_IMAGE_FACTORY (html->engine->image_factory), NULL, url, FALSE);
}

void
gtk_html_set_blocking (GtkHTML *html, gboolean block)
{
	html->engine->block = block;
}

void
gtk_html_set_images_blocking (GtkHTML *html, gboolean block)
{
	html->engine->block_images = block;
}

gint
gtk_html_print_page_get_pages_num (GtkHTML *html,
				   GtkPrintContext *context,
				   gdouble header_height,
				   gdouble footer_height)
{
	return html_engine_print_get_pages_num (
		html->engine, context, header_height, footer_height);
}

GtkPrintOperationResult
gtk_html_print_operation_run (GtkHTML *html,
                              GtkPrintOperation *operation,
                              GtkPrintOperationAction action,
                              GtkWindow *parent,
			      GtkHTMLPrintCalcHeight calc_header_height,
			      GtkHTMLPrintCalcHeight calc_footer_height,
                              GtkHTMLPrintDrawFunc draw_header,
                              GtkHTMLPrintDrawFunc draw_footer,
                              gpointer user_data,
                              GError **error)
{
	return html_engine_print_operation_run (
		html->engine, operation, action, parent,
		calc_header_height, calc_footer_height,
		draw_header, draw_footer, user_data, error);
}

gboolean
gtk_html_has_undo (GtkHTML *html)
{
	return html_undo_has_undo_steps (html->engine->undo);
}

void
gtk_html_drop_undo (GtkHTML *html)
{
	html_undo_reset (html->engine->undo);
}

void
gtk_html_flush (GtkHTML *html)
{
	html_engine_flush (html->engine);
}

const gchar *
gtk_html_get_object_id_at (GtkHTML *html, gint x, gint y)
{
	HTMLObject *o = html_engine_get_object_at (html->engine, x, y, NULL, FALSE);
	const gchar *id = NULL;

	while (o) {
		id = html_object_get_id (o);
		if (id)
			break;
		o = o->parent;
	}

	return id;
}

gchar *
gtk_html_get_url_at (GtkHTML *html, gint x, gint y)
{
	HTMLObject *obj;
	gint offset;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	obj = html_engine_get_object_at (html->engine, x, y, (guint *) &offset, FALSE);

	if (obj)
		return gtk_html_get_url_object_relative (html, obj, html_object_get_url (obj, offset));

	return NULL;
}

gchar *
gtk_html_get_cursor_url (GtkHTML *html)
{
	HTMLObject *obj;
	gint offset;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	if (html->engine->caret_mode) {
		obj = html->engine->cursor->object;
		offset = html->engine->cursor->offset;
	} else
		obj = html_engine_get_focus_object (html->engine, &offset);

	if (obj)
		return gtk_html_get_url_object_relative (html, obj, html_object_get_url (obj, offset));

	return NULL;
}

gchar *
gtk_html_get_image_src_at (GtkHTML *html, gint x, gint y)
{
	HTMLObject *obj;
	gint offset;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	obj = html_engine_get_object_at (html->engine, x, y, (guint *) &offset, FALSE);

	if (obj && HTML_IS_IMAGE (obj)) {
		HTMLImage *image = (HTMLImage*)obj;

		if (!image->image_ptr)
			return NULL;

		return g_strdup (image->image_ptr->url);
	}

	return NULL;
}

gchar *
gtk_html_get_cursor_image_src (GtkHTML *html)
{
	HTMLObject *obj;
	gint offset;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	if (html->engine->caret_mode) {
		obj = html->engine->cursor->object;
		offset = html->engine->cursor->offset;
	} else
		obj = html_engine_get_focus_object (html->engine, &offset);

	if (obj && HTML_IS_IMAGE (obj)) {
		HTMLImage *image = (HTMLImage*)obj;

		if (!image->image_ptr)
			return NULL;

		return g_strdup (image->image_ptr->url);
	}

	return NULL;
}

gboolean
gtk_html_get_cursor_pos (GtkHTML *html, gint *position, gint *offset)
{
	gboolean read = FALSE;

	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	if (html->engine && html->engine->cursor) {
		read = TRUE;

		if (position)
			*position = html->engine->cursor->position;
		if (offset)
			*offset = html->engine->cursor->offset;
	}

	return read;
}

gchar *
gtk_html_filename_from_uri (const gchar *uri)
{
	const gchar *relative_fpath;
	gchar *temp_uri, *temp_filename;
	gchar *retval;

	if (!uri || !*uri)
		return NULL;

	if (g_ascii_strncasecmp (uri, "file://", 7) == 0)
		return g_filename_from_uri (uri, NULL, NULL);

	if (g_ascii_strncasecmp (uri, "file:", 5) == 0) {
		/* Relative (file or other) URIs shouldn't contain the
		 * scheme prefix at all. But accept such broken URIs
		 * anyway. Whether they are URI-encoded or not is
		 * anybody's guess, assume they are.
		 */
		relative_fpath = uri + 5;
	} else {
		/* A proper relative file URI. Just do the URI-decoding. */
		relative_fpath = uri;
	}

	if (g_path_is_absolute (relative_fpath)) {
		/* The totally broken case of "file:" followed
		 * directly by an absolute pathname.
		 */
		/* file:/foo/bar.zap or file:c:/foo/bar.zap */
#ifdef G_OS_WIN32
		if (g_ascii_isalpha (relative_fpath[0]) && relative_fpath[1] == ':')
			temp_uri = g_strconcat ("file:///", relative_fpath, NULL);
		else
			temp_uri = g_strconcat ("file://", relative_fpath, NULL);
#else
		temp_uri = g_strconcat ("file://", relative_fpath, NULL);
#endif
		retval = g_filename_from_uri (temp_uri, NULL, NULL);
		g_free (temp_uri);

		return retval;
	}

	/* Create a dummy absolute file: URI and call
	 * g_filename_from_uri(), then strip off the dummy
	 * prefix.
	 */
#ifdef G_OS_WIN32
	if (g_ascii_isalpha (relative_fpath[0]) && relative_fpath[1] == ':') {
		/* file:c:relative/path/foo.bar */
		gchar drive_letter = relative_fpath[0];

		temp_uri = g_strconcat ("file:///dummy/", relative_fpath + 2, NULL);
		temp_filename = g_filename_from_uri (temp_uri, NULL, NULL);
		g_free (temp_uri);

		if (temp_filename == NULL)
			return NULL;

		g_assert (strncmp (temp_filename, G_DIR_SEPARATOR_S "dummy" G_DIR_SEPARATOR_S, 7) == 0);

		retval = g_strdup_printf ("%c:%s", drive_letter, temp_filename + 7);
		g_free (temp_filename);

		return retval;
	}
#endif
	temp_uri = g_strconcat ("file:///dummy/", relative_fpath, NULL);
	temp_filename = g_filename_from_uri (temp_uri, NULL, NULL);
	g_free (temp_uri);

	if (temp_filename == NULL)
		return NULL;

	g_assert (strncmp (temp_filename, G_DIR_SEPARATOR_S "dummy" G_DIR_SEPARATOR_S, 7) == 0);

	retval = g_strdup (temp_filename + 7);
	g_free (temp_filename);

	return retval;
}

gchar *
gtk_html_filename_to_uri (const gchar *filename)
{
	gchar *fake_filename, *fake_uri, *retval;
	const gchar dummy_prefix[] = "file:///dummy/";
	const gint dummy_prefix_len = sizeof (dummy_prefix) - 1;
#ifdef G_OS_WIN32
	gchar drive_letter = 0;
#else
	gchar *first_end, *colon;
#endif

	if (!filename || !*filename)
		return NULL;

	if (g_path_is_absolute (filename))
		return g_filename_to_uri (filename, NULL, NULL);

	/* filename is a relative path, and the corresponding URI is
	 * filename as such but URI-escaped. Instead of yet again
	 * copy-pasteing the URI-escape code from gconvert.c (or
	 * somewhere else), prefix a fake top-level directory to make
	 * it into an absolute path, call g_filename_to_uri() to turn it
	 * into a full file: URI, and then strip away the file:/// and
	 * the fake top-level directory.
	 */

#ifdef G_OS_WIN32
	if (g_ascii_isalpha (*filename) && filename[1] == ':') {
		/* A non-absolute path, but with a drive letter. Ugh. */
		drive_letter = *filename;
		filename += 2;
	}
#endif
	fake_filename = g_build_filename ("/dummy", filename, NULL);
	fake_uri = g_filename_to_uri (fake_filename, NULL, NULL);
	g_free (fake_filename);

	if (fake_uri == NULL)
		return NULL;

	g_assert (strncmp (fake_uri, dummy_prefix, dummy_prefix_len) == 0);

#ifdef G_OS_WIN32
	/* Re-insert the drive letter if we had one. Double ugh.
	 * URI-encode the colon so the drive letter isn't taken for a
	 * URI scheme!
	 */
	if (drive_letter)
		retval = g_strdup_printf ("%c%%3a%s",
					  drive_letter,
					  fake_uri + dummy_prefix_len);
	else
		retval = g_strdup (fake_uri + dummy_prefix_len);
#else
	retval = g_strdup (fake_uri + dummy_prefix_len);
#endif
	g_free (fake_uri);

#ifdef G_OS_UNIX
	/* Check if there are colons in the first component of the
	 * pathname, and URI-encode them so that the part up to the
	 * colon isn't taken for a URI scheme name! This isn't
	 * necessary on Win32 as there can't be colons in a file name
	 * in the first place.
	 */
	first_end = strchr (retval, '/');
	if (first_end == NULL)
		first_end = retval + strlen (retval);

	while ((colon = strchr (retval, ':')) != NULL && colon < first_end) {
		gchar *new_retval = g_malloc (strlen (retval) + 3);

		strncpy (new_retval, retval, colon - retval);
		strcpy (new_retval + (colon - retval), "%3a");
		strcpy (new_retval + (colon - retval) + 3, colon + 1);

		g_free (retval);
		retval = new_retval;
	}
#endif

	return retval;
}

#ifdef UNIT_TEST_URI_CONVERSIONS

/* To test the uri<->filename code, cut&paste the above two functions
 * and this part into a separate file, insert #include <glib.h>, and
 * build with -DUNIT_TEST_URI_CONVERSIONS.
 */

static const gchar *const tests[][3] = {
	/* Each test case has three strings:
	 *
	 * 0) a URI, the source for the uri->filename conversion test,
	 * or NULL if this test is only for the filename->uri
	 * direction.
	 *
	 * 1) a filename or NULL, the expected result from
	 * uri->filename conversion. If non-NULL also the source for
	 * the filename->uri conversion test,
	 *
	 * 2) a URI if the expected result from filename->uri is
	 * different than string 0, or NULL if the result should be
	 * equal to string 0.
	 */
	{ "file:///top/s%20pace%20d/sub/file", "/top/s pace d/sub/file", NULL },
	{ "file:///top/sub/sub/", "/top/sub/sub/", NULL },
	{ "file:///top/sub/file#segment", NULL, NULL },
	{ "file://tem", NULL, NULL },
	{ "file:/tem", "/tem", "file:///tem" },
	{ "file:sub/tem", "sub/tem", "sub/tem" },
	{ "sub/tem", "sub/tem", NULL },
	{ "s%20pace%20d/tem", "s pace d/tem", NULL },
	{ "tem", "tem", NULL },
	{ "tem#segment", NULL, NULL },
#ifdef G_OS_WIN32
	/* More or less same tests, but including a drive letter */
	{ "file:///x:/top/s%20pace%20d/sub/file", "x:/top/s pace d/sub/file", NULL },
	{ "file:///x:top/sub/sub/", "x:top/sub/sub/", "x%3atop/sub/sub/" },
	{ "file:///x:top/sub/file#segment", NULL, NULL },
	{ "file://x:tem", NULL, NULL },
	{ "file:x:/tem", "x:/tem", "file:///x:/tem" },
	{ "file:x:tem", "x:tem", "x%3atem" },
	{ "file:x:sub/tem", "x:sub/tem", "x%3asub/tem" },
	{ "x%3as%20pace%20d/tem", "x:s pace d/tem", NULL },
	{ "x%3atem", "x:tem", NULL },
	{ "x%3atem#segment", NULL, NULL },
#endif
#ifdef G_OS_UNIX
	/* Test filenames with a colon in them. That's not possible on Win32 */
	{ "file:///top/silly:name/bar", "/top/silly:name/bar", NULL },
	{ "silly%3aname/bar", "silly:name/bar", NULL },
	{ "silly%3aname", "silly:name", NULL },
#endif
  { NULL, NULL }
};

gint
main (gint argc, gchar **argv)
{
	gint failures = 0;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *filename;
#ifdef G_OS_WIN32
		gchar *expected_result;
		gchar *slash;
#else
		const gchar *expected_result;
#endif
		if (tests[i][0] == NULL)
			continue;

		filename = gtk_html_filename_from_uri (tests[i][0]);
#ifdef G_OS_WIN32
		expected_result = g_strdup (tests[i][1]);
		if (expected_result)
			while ((slash = strchr (expected_result, '/')) != NULL)
				*slash = '\\';
#else
		expected_result = tests[i][1];
#endif

		if (((filename == NULL) != (expected_result == NULL)) ||
		    (filename != NULL && strcmp (filename, expected_result) != 0)) {
			g_print ("FAIL: %s -> %s, GOT: %s\n",
				 tests[i][0],
				 expected_result ? expected_result : "NULL",
				 filename ? filename : "NULL");
			failures++;
		} else {
			g_print ("OK: %s -> %s\n",
				 tests[i][0],
				 filename ? filename : "NULL");
		}
	}

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *uri;
		const gchar *expected_result;

		if (tests[i][1] == NULL)
			continue;

		uri = gtk_html_filename_to_uri (tests[i][1]);
		expected_result = tests[i][2] ? tests[i][2] : tests[i][0];
		if (((uri == NULL) != (expected_result == NULL)) ||
		    (uri != NULL && strcmp (uri, expected_result) != 0)) {
			g_printf ("FAIL: %s -> %s, GOT: %s\n",
				  tests[i][1],
				  expected_result ? expected_result : "NULL",
				  uri ? uri : "NULL");
			failures++;
		} else {
			g_print ("OK: %s -> %s\n",
				 tests[i][1],
				 uri ? uri : "NULL");
		}
	}

#ifdef G_OS_WIN32
	/* Test filename->uri also with backslashes */
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *uri;
		gchar *filename, *slash;
		const gchar *expected_result;

		if (tests[i][1] == NULL || strchr (tests[i][1], '/') == NULL)
			continue;

		filename = g_strdup (tests[i][1]);
		while ((slash = strchr (filename, '/')) != NULL)
			*slash = '\\';
		uri = gtk_html_filename_to_uri (tests[i][1]);
		expected_result = tests[i][2] ? tests[i][2] : tests[i][0];
		if (((uri == NULL) != (expected_result == NULL)) ||
		    (uri != NULL && strcmp (uri, expected_result) != 0)) {
			g_printf ("FAIL: %s -> %s, GOT: %s\n",
				  filename,
				  expected_result ? expected_result : "NULL",
				  uri ? uri : "NULL");
			failures++;
		} else {
			g_print ("OK: %s -> %s\n",
				 filename,
				 uri ? uri : "NULL");
		}
	}
#endif

	return failures != 0;
}

#endif
