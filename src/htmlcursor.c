/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.
   Copyright 1999, Helix Code, Inc.

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <glib.h>
#include "htmlcursor.h"

HTMLCursor *
html_cursor_new (void)
{
	HTMLCursor *new;

	new = g_new (HTMLCursor, 1);
	new->object = NULL;
	new->offset = 0;

	return new;
}

void
html_cursor_set_position (HTMLCursor *cursor,
			  HTMLObject *object,
			  guint offset)
{
	g_return_if_fail (cursor != NULL);

	cursor->object = object;
	cursor->offset = offset;
}

void
html_cursor_destroy (HTMLCursor *cursor)
{
	g_free (cursor);
}
