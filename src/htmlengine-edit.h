/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.
    Copyright (C) 1999 Helix Code, Inc.

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

#ifndef _HTMLENGINE_EDIT_H
#define _HTMLENGINE_EDIT_H

#include <glib.h>

#include "htmlengine.h"

#include "htmlengine-edit-cut.h"
#include "htmlengine-edit-copy.h"


enum _HTMLEngineCursorMovement {
	HTML_ENGINE_CURSOR_UP,
	HTML_ENGINE_CURSOR_DOWN,
	HTML_ENGINE_CURSOR_RIGHT,
	HTML_ENGINE_CURSOR_LEFT,
	HTML_ENGINE_CURSOR_HOME,
	HTML_ENGINE_CURSOR_END,
	HTML_ENGINE_CURSOR_PGUP,
	HTML_ENGINE_CURSOR_PGDOWN
};
typedef enum _HTMLEngineCursorMovement HTMLEngineCursorMovement;


/* Cursor movement.  */
guint  html_engine_move_cursor     (HTMLEngine               *e,
				    HTMLEngineCursorMovement  movement,
				    guint                     count);
void   html_engine_jump_to_object  (HTMLEngine               *e,
				    HTMLObject               *object,
				    guint                     offset);
void   html_engine_jump_at         (HTMLEngine               *e,
				    gint                      x,
				    gint                      y);

/* Undo/redo.  */
void  html_engine_undo  (HTMLEngine *e);
void  html_engine_redo  (HTMLEngine *e);

#endif /* _HTMLENGINE_EDIT_H */
