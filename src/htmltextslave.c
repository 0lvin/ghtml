/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
   Copyright (C) 1997 Torben Weis (weis@kde.org)
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

#include <string.h>

#include "htmltextslave.h"
#include "htmlclue.h"
#include "htmlcursor.h"


/* #define HTML_TEXT_SLAVE_DEBUG */

HTMLTextSlaveClass html_text_slave_class;


/* Split this TextSlave at the specified offset.  */
static void
split (HTMLTextSlave *slave, gshort offset)
{
	HTMLObject *obj;
	HTMLObject *new;

	g_return_if_fail (offset >= 0);
	g_return_if_fail (offset < slave->posLen);

	obj = HTML_OBJECT (slave);

	new = html_text_slave_new (slave->owner,
				   slave->posStart + offset,
				   slave->posLen - offset);

	html_clue_append_after (HTML_CLUE (obj->parent), new, obj);
}

/* Split this TextSlave at the first newline character.  */
static void
split_at_newline (HTMLTextSlave *slave)
{
	const gchar *text;
	const gchar *p;

	text = HTML_TEXT (slave->owner)->text + slave->posStart;

	p = memchr (text, '\n', slave->posLen);
	if (p == NULL)
		return;

	split (slave, p - text + 1);
}

static gboolean
check_newline (HTMLTextSlave *slave)
{
	HTMLObject *obj;

	obj = HTML_OBJECT (slave);

	if (obj->next == NULL)
		return TRUE;
	if (obj->flags & HTML_OBJECT_FLAG_NEWLINE)
		return TRUE;

	return FALSE;
}


/* HTMLObject methods.  */

static void
calc_size (HTMLObject *self,
	   HTMLPainter *painter)
{
	HTMLText *owner;
	HTMLTextSlave *slave;
	HTMLFontStyle font_style;

	slave = HTML_TEXT_SLAVE (self);
	owner = HTML_TEXT (slave->owner);
	font_style = html_text_get_font_style (owner);

	self->ascent = html_painter_calc_ascent (painter, font_style);
	self->descent = html_painter_calc_descent (painter, font_style);

	self->width = html_painter_calc_text_width (painter,
						    owner->text + slave->posStart,
						    slave->posLen,
						    font_style);

	self->width += html_painter_calc_text_width (painter, " ", 1, font_style);
}

static HTMLFitType
fit_line (HTMLObject *o,
	  HTMLPainter *painter,
	  gboolean startOfLine,
	  gboolean firstRun,
	  gint widthLeft)
{
	HTMLTextSlave *textslave;
	HTMLTextMaster *textmaster;
	HTMLText *ownertext;
	HTMLObject *next_obj;
	HTMLFitType return_value;
	HTMLFontStyle font_style;
	gint newLen;
	gint newWidth;
	gchar *splitPtr;
	gchar *text;

	textslave = HTML_TEXT_SLAVE (o);
	textmaster = HTML_TEXT_MASTER (textslave->owner);
	ownertext = HTML_TEXT (textmaster);

	font_style = html_text_get_font_style (ownertext);

	next_obj = o->next;
	text = ownertext->text;

	/* Remove following existing slaves.  */

	if (next_obj != NULL
	    && (HTML_OBJECT_TYPE (next_obj) == HTML_TYPE_TEXTSLAVE)) {
		do {
			o->next = next_obj->next;
			html_clue_remove (HTML_CLUE (next_obj->parent), next_obj);
			html_object_destroy (next_obj);
			next_obj = o->next;
			if (next_obj != NULL)
				next_obj->prev = o;
		} while (next_obj && (HTML_OBJECT_TYPE (next_obj)
				      == HTML_TYPE_TEXTSLAVE));
		textslave->posLen = HTML_TEXT (textslave->owner)->text_len - textslave->posStart;
	}

	split_at_newline (HTML_TEXT_SLAVE (o));

	text += textslave->posStart;

	o->width = html_painter_calc_text_width (painter, text, textslave->posLen, font_style);
	if (o->width <= widthLeft || textslave->posLen <= 1 || widthLeft < 0) {
		/* Text fits completely */
		return_value = HTML_FIT_COMPLETE;
		goto done;
	} else {
		splitPtr = index (text + 1, ' ');
	}
	
	if (splitPtr) {
		newLen = splitPtr - text + 1;
		newWidth = html_painter_calc_text_width (painter, text, newLen, font_style);
		if (newWidth > widthLeft) {
			/* Splitting doesn't make it fit */
			splitPtr = 0;
		} else {
			gint extraLen;
			gint extraWidth;

			for (;;) {
				gchar *splitPtr2 = index (splitPtr + 1, ' ');
				if (!splitPtr2)
					break;
				extraLen = splitPtr2 - splitPtr;
				extraWidth = html_painter_calc_text_width (painter, splitPtr, extraLen,
									   font_style);
				if (extraWidth + newWidth <= widthLeft) {
					/* We can break on the next separator cause it still fits */
					newLen += extraLen;
					newWidth += extraWidth;
					splitPtr = splitPtr2;
				} else {
					/* Using this separator would over-do it */
					break;
				}
			}
		}
	} else {
		newLen = textslave->posLen;
		newWidth = o->width;
	}
	
	if (!splitPtr) {
		/* No separator available */
		if (firstRun == FALSE) {
			/* Text does not fit, wait for next line */
			return_value = HTML_FIT_NONE;
			goto done;
		}

		/* Text doesn't fit, too bad. 
		   newLen & newWidth are valid */
	}

	if (textslave->posLen - newLen > 0)
		split (textslave, newLen);

	textslave->posLen = newLen;

	o->width = newWidth;
	o->ascent = html_painter_calc_ascent (painter, font_style);
	o->descent = html_painter_calc_descent (painter, font_style);

	return_value = HTML_FIT_PARTIAL;

 done:
#ifdef HTML_TEXT_SLAVE_DEBUG
	/* FIXME */
	{
		gint i;

		printf ("Split text");
		switch (return_value) {
		case HTML_FIT_PARTIAL:
			printf (" (Partial): `");
			break;
		case HTML_FIT_NONE:
			printf (" (NoFit): `");
			break;
		case HTML_FIT_COMPLETE:
			printf (" (Complete): `");
			break;
		}

		for (i = 0; i < textslave->posLen; i++)
			putchar (ownertext->text[textslave->posStart + i]);

		printf ("'\n");
	}
#endif

	return return_value;
}


/* HTMLObject::draw() implementation.  */

static void
draw_newline_highlight (HTMLTextSlave *self,
			HTMLPainter *p,
			HTMLFontStyle font_style,
			gint x, gint y,
			gint width, gint height,
			gint tx, gint ty)
{
	HTMLObject *obj;
	guint space_width, space_x;

	if (! check_newline (self))
		return;

	obj = HTML_OBJECT (self);

	space_width = html_painter_calc_text_width (p, " ", 1, font_style);
	space_x = obj->x + obj->width - space_width;

	html_painter_set_pen (p, html_painter_get_default_highlight_color (p));
	html_painter_fill_rect (p,
				space_x + tx, obj->y - obj->ascent + ty,
				space_width,
				obj->ascent + obj->descent);
}

static void
draw_normal (HTMLTextSlave *self,
	     HTMLPainter *p,
	     HTMLFontStyle font_style,
	     gint x, gint y,
	     gint width, gint height,
	     gint tx, gint ty)
{
	HTMLObject *obj;

	obj = HTML_OBJECT (self);

	html_painter_set_font_style (p, font_style);
	html_painter_set_pen (p, html_text_get_color (HTML_TEXT (self->owner), p));
	html_painter_draw_text (p,
				obj->x + tx, obj->y + ty, 
				HTML_TEXT (self->owner)->text + self->posStart,
				self->posLen);
}

static void
draw_highlighted (HTMLTextSlave *slave,
		  HTMLPainter *p,
		  HTMLFontStyle font_style,
		  gint x, gint y,
		  gint width, gint height,
		  gint tx, gint ty)
{
	HTMLTextMaster *owner;
	HTMLObject *obj;
	guint start, end, len;
	guint offset_width, text_width;
	const gchar *text;
	gboolean highlight_newline;

	obj = HTML_OBJECT (slave);
	owner = HTML_TEXT_MASTER (slave->owner);
	start = owner->select_start;
	end = start + owner->select_length;

	text = HTML_TEXT (owner)->text;

	if (owner->select_start + owner->select_length > slave->posStart + slave->posLen)
		highlight_newline = TRUE;
	else
		highlight_newline = FALSE;

	if (start < slave->posStart)
		start = slave->posStart;
	if (end > slave->posStart + slave->posLen)
		end = slave->posStart + slave->posLen;
	len = end - start;

	offset_width = html_painter_calc_text_width (p, text + slave->posStart, start - slave->posStart,
						     font_style);
	text_width = html_painter_calc_text_width (p, text + start, len, font_style);

	html_painter_set_font_style (p, font_style);

	/* Draw the highlighted part with a highlight background.  */

	html_painter_set_pen (p, html_painter_get_default_highlight_color (p));
	html_painter_fill_rect (p, obj->x + tx + offset_width, obj->y + ty - obj->ascent,
				text_width, obj->ascent + obj->descent);
	html_painter_set_pen (p, html_painter_get_default_highlight_foreground_color (p));
	html_painter_draw_text (p, obj->x + tx + offset_width, obj->y + ty, text + start, len);

	/* Draw the non-highlighted part.  */

	html_painter_set_pen (p, html_text_get_color (HTML_TEXT (owner), p));

	/* 1. Draw the leftmost non-highlighted part, if any.  */

	if (start > slave->posStart)
		html_painter_draw_text (p,
					obj->x + tx, obj->y + ty,
					text + slave->posStart,
					start - slave->posStart);

	/* 2. Draw the rightmost non-highlighted part, if any.  */

	if (end < slave->posStart + slave->posLen)
		html_painter_draw_text (p,
					obj->x + tx + offset_width + text_width, obj->y + ty,
					text + end,
					slave->posStart + slave->posLen - end);

	if (highlight_newline)
		draw_newline_highlight (slave, p, font_style, x, y, width, height, tx, ty);
}

static void
draw (HTMLObject *o,
      HTMLPainter *p,
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	HTMLTextSlave *textslave;
	HTMLTextMaster *owner;
	HTMLText *ownertext;
	HTMLFontStyle font_style;

	if (y + height < o->y - o->ascent || y > o->y + o->descent)
		return;

	textslave = HTML_TEXT_SLAVE (o);
	owner = textslave->owner;
	ownertext = HTML_TEXT (owner);
	font_style = html_text_get_font_style (ownertext);

	if (owner->select_start + owner->select_length <= textslave->posStart
	    || owner->select_start >= textslave->posStart + textslave->posLen) {
		draw_normal (textslave, p, font_style, x, y, width, height, tx, ty);
	} else {
		draw_highlighted (textslave, p, font_style, x, y, width, height, tx, ty);
	}
}

static gint
calc_min_width (HTMLObject *o,
		HTMLPainter *painter)
{
	return 0;
}

static gint
calc_preferred_width (HTMLObject *o,
		      HTMLPainter *painter)
{
	return 0;
}

static const gchar *
get_url (HTMLObject *o)
{
	HTMLTextSlave *slave;

	slave = HTML_TEXT_SLAVE (o);
	return html_object_get_url (HTML_OBJECT (slave->owner));
}

static HTMLObject *
check_point (HTMLObject *self,
	     HTMLPainter *painter,
	     gint x, gint y,
	     guint *offset_return,
	     gboolean for_cursor)
{
	return NULL;
}


void
html_text_slave_type_init (void)
{
	html_text_slave_class_init (&html_text_slave_class,
				    HTML_TYPE_TEXTSLAVE);
}

void
html_text_slave_class_init (HTMLTextSlaveClass *klass,
			    HTMLType type)
{
	HTMLObjectClass *object_class;

	object_class = HTML_OBJECT_CLASS (klass);

	html_object_class_init (object_class, type);

	object_class->draw = draw;
	object_class->calc_size = calc_size;
	object_class->fit_line = fit_line;
	object_class->calc_min_width = calc_min_width;
	object_class->calc_preferred_width = calc_preferred_width;
	object_class->get_url = get_url;
	object_class->check_point = check_point;
}

void
html_text_slave_init (HTMLTextSlave *slave,
		      HTMLTextSlaveClass *klass,
		      HTMLTextMaster *owner,
		      gint posStart,
		      gint posLen)
{
	HTMLText *owner_text;
	HTMLObject *object;

	object = HTML_OBJECT (slave);
	owner_text = HTML_TEXT (owner);

	html_object_init (object, HTML_OBJECT_CLASS (klass));

	object->ascent = HTML_OBJECT (owner)->ascent;
	object->descent = HTML_OBJECT (owner)->descent;

	slave->posStart = posStart;
	slave->posLen = posLen;
	slave->owner = owner;
}

HTMLObject *
html_text_slave_new (HTMLTextMaster *owner, gint posStart, gint posLen)
{
	HTMLTextSlave *slave;

	slave = g_new (HTMLTextSlave, 1);
	html_text_slave_init (slave, &html_text_slave_class, owner, posStart, posLen);

	return HTML_OBJECT (slave);
}


guint
html_text_slave_get_offset_for_pointer (HTMLTextSlave *slave,
					HTMLPainter *painter,
					gint x, gint y)
{
	HTMLText *owner;
	HTMLFontStyle font_style;
	guint width, prev_width;
	guint i;

	g_return_val_if_fail (slave != NULL, 0);

	owner = HTML_TEXT (slave->owner);
	font_style = html_text_get_font_style (owner);

	x -= HTML_OBJECT (slave)->x;

	prev_width = 0;
	for (i = 1; i <= slave->posLen; i++) {
		width = html_painter_calc_text_width (painter,
						      owner->text + slave->posStart,
						      i,
						      font_style);

		if ((width + prev_width) / 2 >= x) {
			g_print ("%p offset %d\n", slave, i - 1);
			return i - 1;
		}

		prev_width = width;
	}

	return slave->posLen;
}
