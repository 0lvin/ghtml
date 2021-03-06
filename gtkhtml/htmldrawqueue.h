/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library
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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _HTMLDRAWQUEUE_H
#define _HTMLDRAWQUEUE_H

#include <gtk/gtk.h>
#include "htmltypes.h"

struct _HTMLDrawQueueClearElement {
	gint x, y;
	guint width, height;
	GdkColor *background_color;
	GdkPixbuf *background_image;
	guint background_image_x_offset, background_image_y_offset;
};

struct _HTMLDrawQueue {
	/* The associated engine.  */
	HTMLEngine *engine;

	/* Elements to be drawn.  */
	GList *elems;
	/* Pointer to the last element in the list, for faster appending.  */
	GList *last;

	/* Elements to be cleared (HTMLDrawQueueClearElement).  */
	GList *clear_elems;
	/* Pointer to the last element.  */
	GList *clear_last;
};


/* Creation/destruction.  */
HTMLDrawQueue *html_draw_queue_new      (HTMLEngine    *engine);
void           html_draw_queue_destroy  (HTMLDrawQueue *queue);
void           html_draw_queue_flush    (HTMLDrawQueue *queue);
void           html_draw_queue_clear    (HTMLDrawQueue *queue);

/* Adding objects.  */
void           html_draw_queue_add      (HTMLDrawQueue *queue,
					 HTMLObject    *object);

/* Adding clear areas.  */
void  html_draw_queue_add_clear                  (HTMLDrawQueue  *queue,
						  gint            x,
						  gint            y,
						  guint           width,
						  guint           height,
						  const GdkColor *background_color);
void  html_draw_queue_add_clear_with_background  (HTMLDrawQueue  *queue,
						  gint            x,
						  gint            y,
						  guint           width,
						  guint           height,
						  GdkPixbuf      *background_image,
						  guint           image_x_offset,
						  guint           image_y_offset);

#endif /* _HTMLDRAWQUEUE_H */
