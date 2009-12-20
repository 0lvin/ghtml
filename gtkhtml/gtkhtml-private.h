/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright 1999, 2000 Helix Code, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef _GTKHTML_PRIVATE_H
#define _GTKHTML_PRIVATE_H

#include <gtk/gtk.h>
#include "gtkhtml-types.h"
#include "htmltypes.h"

struct _GtkHTMLPrivate {
	guint idle_handler_id;
	guint scroll_timeout_id;

	GtkHTMLParagraphStyle paragraph_style;
	guint paragraph_indentation;
	GtkHTMLParagraphAlignment paragraph_alignment;
	GtkHTMLFontStyle insertion_font_style;

	gboolean update_styles;
	gboolean skip_update_cursor;
	gboolean cursor_moved;

	gint selection_type;

	GtkWidget *search_input_line;

#ifdef HAVE_GCONF
	guint notify_monospace_font_id;
#endif

	GtkIMContext *im_context;
	gboolean need_im_reset;
	gint im_pre_len;
	gint im_pre_pos;
	GtkHTMLFontStyle im_orig_style;
	gboolean im_block_reset;

	HTMLObject *dnd_object;
	gint        dnd_object_offset;
	HTMLObject *dnd_real_object;
	gint        dnd_real_object_offset;
	gboolean    dnd_in_progress;
	gchar      *dnd_url;

	guint32     event_time;
	gboolean    selection_as_cite;

	gboolean    magic_links;
	gboolean    magic_smileys;
	gboolean    inline_spelling;

	gulong      toplevel_unmap_handler;

	gboolean in_object_resize;
	GdkCursor *resize_cursor;
	HTMLObject *resize_object;

	gboolean in_url_test_mode;

	gboolean in_key_binding;

	gchar *caret_first_focus_anchor;
};

void  gtk_html_private_calc_scrollbars  (GtkHTML                *html,
					 gboolean               *changed_x,
					 gboolean               *changed_y);
void  gtk_html_editor_event_command     (GtkHTML                *html,
					 GtkHTMLCommandType      com_type,
					 gboolean                before);
void  gtk_html_editor_event             (GtkHTML                *html,
					 GtkHTMLEditorEventType  event,
					 GValue                 *args);
void  gtk_html_api_set_language         (GtkHTML                *html);
void  gtk_html_im_reset                 (GtkHTML                *html);
void  gtk_html_set_fonts                (GtkHTML                *html,
					 HTMLPainter            *painter);

gchar *gtk_html_filename_to_uri		(const gchar		*filename);
gchar *gtk_html_filename_from_uri	(const gchar		*uri);

#ifdef G_OS_WIN32

const gchar *_get_icondir (void) G_GNUC_CONST;
const gchar *_get_gtkhtml_datadir (void) G_GNUC_CONST;
const gchar *_get_localedir (void) G_GNUC_CONST;
const gchar *_get_prefix (void) G_GNUC_CONST;
const gchar *_get_sysconfdir (void) G_GNUC_CONST;
const gchar *_get_datadir (void) G_GNUC_CONST;
const gchar *_get_libdir (void) G_GNUC_CONST;

#undef ICONDIR
#define ICONDIR _get_icondir ()

#undef GTKHTML_DATADIR
#define GTKHTML_DATADIR _get_gtkhtml_datadir ()

#undef GNOMELOCALEDIR
#define GNOMELOCALEDIR _get_localedir ()

#undef PREFIX
#define PREFIX _get_prefix ()

#undef SYSCONFDIR
#define SYSCONFDIR _get_sysconfdir ()

#undef DATADIR
#define DATADIR _get_datadir ()

#undef LIBDIR
#define LIBDIR _get_libdir ()

#endif

#endif /* _GTKHTML_PRIVATE_H */
