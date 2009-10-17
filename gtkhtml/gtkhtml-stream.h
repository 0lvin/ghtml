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

#ifndef _GTKHTML_STREAM_H
#define _GTKHTML_STREAM_H

#include <sys/types.h>
#include "gtkhtml-types.h"

struct _GtkHTMLStream {
	GtkHTMLStreamWriteFunc write_func;
	GtkHTMLStreamCloseFunc close_func;
	GtkHTMLStreamTypesFunc types_func;
	GtkHTMLStreamMIMEFunc  mime_func;
	GtkHTMLStreamHREFFunc  href_func;
	gpointer user_data;
};


GtkHTMLStream *gtk_html_stream_new       (GtkHTML                *html,
					  GtkHTMLStreamTypesFunc  type_func,
					  GtkHTMLStreamWriteFunc  write_func,
					  GtkHTMLStreamCloseFunc  close_func,
					  GtkHTMLStreamMIMEFunc   mime_func,
					  GtkHTMLStreamHREFFunc   href_func,
					  gpointer                user_data);
void           gtk_html_stream_write     (GtkHTMLStream          *stream,
					  const gchar            *buffer,
					  gsize                  size);
void           gtk_html_stream_mime      (GtkHTMLStream *stream,
					  const gchar *mime_type);
void           gtk_html_stream_href      (GtkHTMLStream *stream,
					  const gchar *href);
void           gtk_html_stream_destroy   (GtkHTMLStream          *stream);
void           gtk_html_stream_close     (GtkHTMLStream          *stream,
					  GtkHTMLStreamStatus     status);
gchar **        gtk_html_stream_get_types (GtkHTMLStream *stream);

GtkHTMLStream *gtk_html_stream_log_new   (GtkHTML *html, GtkHTMLStream *stream);

gint            gtk_html_stream_vprintf   (GtkHTMLStream *stream,
					  const gchar *format,
					  va_list ap);
gint            gtk_html_stream_printf    (GtkHTMLStream *stream,
					  const gchar *format,
					  ...) G_GNUC_PRINTF (2, 3);

#endif /* _GTKHTML_STREAM_H */
