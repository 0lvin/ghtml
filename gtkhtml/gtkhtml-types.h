/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library

   Copyright (C) 2000 Helix Code, Inc.
   Author:            Radek Doulik <rodo@helixcode.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHcANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _GTK_HTML_TYPES_H_
#define _GTK_HTML_TYPES_H_

#include "gtkhtml-enums.h"
#include <gtk/gtk.h>

typedef struct _GtkHTML GtkHTML;
typedef struct _GtkHTMLClass GtkHTMLClass;
typedef struct _GtkHTMLClassProperties GtkHTMLClassProperties;
typedef struct _GtkHTMLEditorAPI GtkHTMLEditorAPI;
typedef struct _GtkHTMLEmbedded GtkHTMLEmbedded;
typedef struct _GtkHTMLEmbeddedClass GtkHTMLEmbeddedClass;
typedef struct _GtkHTMLEmbeddedPrivate GtkHTMLEmbeddedPrivate;
typedef struct _GtkHTMLPrivate GtkHTMLPrivate;
typedef struct _GtkHTMLStream GtkHTMLStream;

typedef gchar **(* GtkHTMLStreamTypesFunc) (GtkHTMLStream *stream,
					   gpointer user_data);
typedef void   (* GtkHTMLStreamCloseFunc) (GtkHTMLStream *stream,
					   GtkHTMLStreamStatus status,
					   gpointer user_data);
typedef void   (* GtkHTMLStreamMIMEFunc) (GtkHTMLStream *stream,
					   const gchar *mime_type,
					   gpointer user_data);
typedef void   (* GtkHTMLStreamHREFFunc) (GtkHTMLStream *stream,
					   const gchar *href,
					   gpointer user_data);
typedef void   (* GtkHTMLStreamWriteFunc) (GtkHTMLStream *stream,
					   const gchar *buffer,
					   gsize size,
					   gpointer user_data);

/* FIXME 1st param should be Engine */
typedef gboolean (* GtkHTMLSaveReceiverFn)   (gpointer     engine,
					      const gchar *data,
					      gsize       len,
					      gpointer     user_data);

typedef gint    (*GtkHTMLPrintCalcHeight) (GtkHTML *html,
					   GtkPrintOperation *operation,
                                           GtkPrintContext *context,
                                           gpointer user_data);
typedef void    (*GtkHTMLPrintDrawFunc)   (GtkHTML *html,
					   GtkPrintOperation *operation,
					   GtkPrintContext *context,
					   gint page_nr,
					   PangoRectangle *rec,
					   gpointer user_data);

typedef void (*GtkHTMLPrintCallback) (GtkHTML *html, GtkPrintContext *print_context,
				      gdouble x, gdouble y, gdouble width, gdouble height, gpointer user_data);

#endif
