/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library

   Copyright (C) 2000 Helix Code, Inc.

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

#include "htmlengine.h"

gboolean  html_engine_search                    (HTMLEngine *e,
						 const gchar *text,
						 gboolean case_sensitive,
						 gboolean forward,
						 gboolean regular);
void      html_engine_search_set_forward        (HTMLEngine *e,
						 gboolean forward);
gboolean  html_engine_search_next               (HTMLEngine *e);
gboolean  html_engine_search_incremental        (HTMLEngine *e,
						 const gchar *text,
						 gboolean forward);
