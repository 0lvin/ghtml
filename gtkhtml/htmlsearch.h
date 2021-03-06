/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library

   Copyright (C) 2000 Helix Code, Inc.
   Authors:           Radek Doulik (rodo@helixcode.com)

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

#ifndef _HTML_SEARCH_H_
#define _HTML_SEARCH_H_

#include <sys/types.h>
#include <regex.h>
#include "htmltypes.h"

struct _HTMLSearch {
	HTMLEngine *engine;
	gchar *trans;
	gchar *text;
	guint  text_bytes;
	guint  found_bytes;

	gboolean case_sensitive;
	gboolean forward;
	gboolean regular;

	GSList      *stack;
	GList       *found;
	HTMLObject *last;

	gint start_pos;
	gint stop_pos;

	regex_t *reb;        /* regex buffer */
};

HTMLSearch      *html_search_new            (HTMLEngine *e,
					     const gchar *text,
					     gboolean case_sensitive,
					     gboolean forward,
					     gboolean regular);
void             html_search_destroy        (HTMLSearch *search);
void             html_search_push           (HTMLSearch *search, HTMLObject *obj);
HTMLObject      *html_search_pop            (HTMLSearch *search);
gboolean         html_search_child_on_stack (HTMLSearch *search, HTMLObject *obj);
gboolean         html_search_next_parent    (HTMLSearch *search);
void             html_search_set_text       (HTMLSearch *search,
					     const gchar *text);
void             html_search_set_forward    (HTMLSearch *search,
					     gboolean    forward);
#endif
