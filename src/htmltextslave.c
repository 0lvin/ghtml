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

#include <config.h>
#include <stdio.h>
#include <string.h>

#include "htmltextslave.h"
#include "htmlclue.h"
#include "htmlclueflow.h"
#include "htmlcursor.h"
#include "htmlcolor.h"
#include "htmlcolorset.h"
#include "htmlpainter.h"
#include "htmlplainpainter.h"
#include "htmlgdkpainter.h"


/* #define HTML_TEXT_SLAVE_DEBUG */

HTMLTextSlaveClass html_text_slave_class;
static HTMLObjectClass *parent_class = NULL;

static GList * get_items (HTMLTextSlave *slave, HTMLPainter *painter);
static GList * get_glyphs (HTMLTextSlave *slave, HTMLPainter *painter, gint line_offset);
static GList * get_glyphs_part (HTMLTextSlave *slave, HTMLPainter *painter, guint offset, guint len, gint line_offset, GList **items);

char *
html_text_slave_get_text (HTMLTextSlave *slave)
{
	if (!slave->charStart)
		slave->charStart = html_text_get_text (slave->owner, slave->posStart);

	return slave->charStart;
}

/* Split this TextSlave at the specified offset.  */
static void
split (HTMLTextSlave *slave, guint offset, guint start_word, char *start_pointer)
{
	HTMLObject *obj;
	HTMLObject *new;

	g_return_if_fail (offset >= 0);
	g_return_if_fail (offset < slave->posLen);

	obj = HTML_OBJECT (slave);

	new = html_text_slave_new (slave->owner,
				   slave->posStart + offset,
				   slave->posLen - offset, start_word);

	HTML_TEXT_SLAVE (new)->charStart = start_pointer;

	html_clue_append_after (HTML_CLUE (obj->parent), new, obj);

	slave->posLen = offset;
}


/* HTMLObject methods.  */

static void
copy (HTMLObject *self,
      HTMLObject *dest)
{
	(* HTML_OBJECT_CLASS (parent_class)->copy) (self, dest);

	/* FIXME it does not make much sense to me to share the owner.  */
	HTML_TEXT_SLAVE (dest)->owner = HTML_TEXT_SLAVE (self)->owner;
	HTML_TEXT_SLAVE (dest)->posStart = HTML_TEXT_SLAVE (self)->posStart;
	HTML_TEXT_SLAVE (dest)->posLen = HTML_TEXT_SLAVE (self)->posLen;
}

static guint
get_words_width (HTMLText *text, HTMLPainter *p, guint start_word, guint words)
{
	return words > 0
		? text->word_width [start_word + words - 1]
		- (start_word ? text->word_width [start_word - 1]
		   + html_painter_get_space_width (p, html_text_get_font_style (text), text->face) : 0)
		: 0;
}

static guint
calc_width (HTMLTextSlave *slave, HTMLPainter *painter, gint *asc, gint *dsc)
{
	HTMLText *text = slave->owner;
	HTMLObject *next, *prev;

	html_text_request_word_width (text, painter);
	if (slave->posStart == 0 && slave->posLen == text->text_len) {
		*asc = HTML_OBJECT (text)->ascent;
		*dsc = HTML_OBJECT (text)->descent;

		return text->word_width [text->words - 1];
	}

	next = HTML_OBJECT (slave)->next;
	prev = HTML_OBJECT (slave)->prev;
	if ((prev && HTML_OBJECT_TYPE (prev) == HTML_TYPE_TEXTSLAVE
	     && slave->start_word == HTML_TEXT_SLAVE (prev)->start_word)
	    || (next && HTML_OBJECT_TYPE (next) == HTML_TYPE_TEXTSLAVE
		&& slave->start_word == HTML_TEXT_SLAVE (next)->start_word)) {
		gint line_offset = -1;
		gint width;

		html_painter_calc_text_size (painter, html_text_slave_get_text (slave), slave->posLen, get_items (slave, painter), get_glyphs (slave, painter, line_offset),
					     &line_offset, html_text_get_font_style (text),
					     text->face, &width, asc, dsc);	
		return width;
	} else {
		gint width;

		width = get_words_width (text, painter, slave->start_word,
					 (next && HTML_OBJECT_TYPE (next) == HTML_TYPE_TEXTSLAVE
					  ? HTML_TEXT_SLAVE (next)->start_word : text->words) - slave->start_word);
		*asc = HTML_OBJECT (text)->ascent;
		*dsc = HTML_OBJECT (text)->descent;

		return width;
	}
}

inline static void
glyphs_destroy (GList *glyphs)
{
	GList *l;

	for (l = glyphs; l; l = l->next)
		pango_glyph_string_free ((PangoGlyphString *) l->data);
	g_list_free (glyphs);
}

static gint
get_offset_for_bounded_width (HTMLTextSlave *slave, HTMLPainter *painter, gint *words, gint max_width)
{
	HTMLText *text = slave->owner;
	gint upper = slave->posLen;
	gint lower = 0;
	gint len, width, asc, dsc;
	gint line_offset = -1;
	gchar *sep, *str;
	char *buffer = html_text_slave_get_text (slave);

	len = (lower + upper) / 2;
	html_painter_calc_text_size (painter, buffer, len, get_items (slave, painter), get_glyphs (slave, painter, line_offset), &line_offset,
				     html_text_get_font_style (text), text->face, &width, &asc, &dsc);
	while (lower < upper) {
		if (width > max_width)
			upper = len - 1;
		else
			lower = len + 1;
		len = (lower + upper) / 2;
		line_offset = -1;
		html_painter_calc_text_size (painter, buffer, len, get_items (slave, painter), get_glyphs (slave, painter, line_offset), &line_offset, 
					     html_text_get_font_style (text), text->face, &width, &asc, &dsc);
	}

	if (width > max_width && len > 1)
		len --;

	*words = 0;
	str = sep = buffer;

	while ((sep = strchr (sep, ' '))) {
		if (g_utf8_pointer_to_offset (str, sep) < len)
			(*words) ++;
		else
			break;
		sep = sep + 1;
	}

	return len;
}

static void
slave_split_if_too_long (HTMLTextSlave *slave, HTMLPainter *painter, gint *width, gint *asc, gint *dsc)
{
	gint x, y;

	html_object_calc_abs_position (HTML_OBJECT (slave), &x, &y);

	if (*width + x > MAX_WIDGET_WIDTH && slave->posLen > 1) {
		gint words, pos;

		pos = get_offset_for_bounded_width (slave, painter, &words, MAX_WIDGET_WIDTH - x);
		if (pos > 0 && pos < slave->posLen) {
			split (slave, pos, slave->start_word + words, NULL);
			*width = MAX (1, calc_width (slave, painter, asc, dsc));
		}
	}
}

static gboolean
calc_size (HTMLObject *self, HTMLPainter *painter, GList **changed_objs)
{
	HTMLText *owner;
	HTMLTextSlave *slave;
	HTMLObject *next;
	GtkHTMLFontStyle font_style;
	gint new_ascent, new_descent, new_width;
	gboolean changed;

	slave = HTML_TEXT_SLAVE (self);
	owner = HTML_TEXT (slave->owner);
	font_style = html_text_get_font_style (owner);

	new_width = MAX (1, calc_width (slave, painter, &new_ascent, &new_descent));

	next = HTML_OBJECT (slave)->next;
	if ((slave->start_word == owner->words - 1
	     && (!next || (HTML_IS_TEXT_SLAVE (next) && HTML_TEXT_SLAVE (next)->start_word == slave->start_word + 1)))
	    && (HTML_IS_PLAIN_PAINTER (painter) || HTML_IS_GDK_PAINTER (painter))
	    && new_width > HTML_OBJECT (owner)->max_width)
		slave_split_if_too_long (slave, painter, &new_width, &new_ascent, &new_descent);

	/* handle sub & super script */
	if (font_style & GTK_HTML_FONT_STYLE_SUBSCRIPT || font_style & GTK_HTML_FONT_STYLE_SUPERSCRIPT) {
		gint shift = (new_ascent + new_descent) >> 1;

		if (font_style & GTK_HTML_FONT_STYLE_SUBSCRIPT) {
			new_descent += shift;
			new_ascent  -= shift;
		} else {
			new_descent -= shift;
			new_ascent  += shift;
		}
	}

	changed = FALSE;

	if (new_ascent != self->ascent) {
		self->ascent = new_ascent;
		changed = TRUE;
	}

	if (new_descent != self->descent) {
		self->descent = new_descent;
		changed = TRUE;
	}

	if (new_width != self->width) {
		self->width = new_width;
		changed = TRUE;
	}

	return changed;
}

#ifdef HTML_TEXT_SLAVE_DEBUG
static void
debug_print (HTMLFitType rv, gchar *text, gint len)
{
	gint i;

	printf ("Split text");
	switch (rv) {
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

	for (i = 0; i < len; i++)
		putchar (text [i]);

	printf ("'\n");
}
#endif

gint
html_text_slave_get_line_offset (HTMLTextSlave *slave, gint line_offset, gint offset, HTMLPainter *p)
{
	HTMLObject *head = HTML_OBJECT (slave->owner)->next;

	g_assert (HTML_IS_TEXT_SLAVE (head));

	if (!html_clueflow_tabs (HTML_CLUEFLOW (HTML_OBJECT (slave)->parent), p) || line_offset == -1)
		return -1;

	if (head->y + head->descent - 1 < HTML_OBJECT (slave)->y - HTML_OBJECT (slave)->ascent) {
		HTMLObject *prev;
		HTMLTextSlave *bol;

		prev = html_object_prev (HTML_OBJECT (slave)->parent, HTML_OBJECT (slave));
		while (prev->y + prev->descent - 1 >= HTML_OBJECT (slave)->y - HTML_OBJECT (slave)->ascent)
			prev = html_object_prev (HTML_OBJECT (slave)->parent, HTML_OBJECT (prev));

		bol = HTML_TEXT_SLAVE (prev->next);
		return html_text_text_line_length (html_text_slave_get_text (bol),
						   0, offset - bol->posStart);
	} else {
		html_text_text_line_length (slave->owner->text, &line_offset, offset);
		return line_offset;
	}
}

static gint
get_next_nb_width (HTMLTextSlave *slave, HTMLPainter *painter)
{
	
	gint width = 0;

	if (HTML_TEXT (slave->owner)->text_len == 0
	    || html_text_get_char (HTML_TEXT (slave->owner), slave->posStart + slave->posLen - 1) != ' ') {
		HTMLObject *obj;
		obj = html_object_next_not_slave (HTML_OBJECT (slave));
		if (obj && html_object_is_text (obj)
		    && html_text_get_char (HTML_TEXT (obj), 0) != ' ')
			width = html_text_get_nb_width (HTML_TEXT (obj), painter, TRUE);
	}

	return width;
}

static gboolean
could_remove_leading_space (HTMLTextSlave *slave, gboolean lineBegin)
{
	HTMLObject *o = HTML_OBJECT (slave->owner);

	if (lineBegin && (HTML_OBJECT (slave)->prev != o || o->prev))
		return TRUE;

	if (!o->prev)
		return FALSE;

	while (o->prev && HTML_OBJECT_TYPE (o->prev) == HTML_TYPE_CLUEALIGNED)
		o = o->prev;

	return o->prev ? FALSE : TRUE;
}

inline gint
html_text_slave_nb_width (HTMLTextSlave *slave, HTMLPainter *painter, gint words)
{
	return get_words_width (slave->owner, painter, slave->start_word, words)
		+ (slave->start_word + words == slave->owner->words ? get_next_nb_width (slave, painter) : 0);
}

inline gchar *
html_text_slave_remove_leading_space (HTMLTextSlave *slave, HTMLPainter *painter, gboolean lineBegin)
{
	gchar *begin;

	html_text_request_word_width (slave->owner, painter);

	begin = html_text_slave_get_text (slave);
	if (*begin == ' ' && could_remove_leading_space (slave, lineBegin)) {
		if (slave->posStart == 0)
			slave->start_word ++;
		begin = g_utf8_next_char (begin);
		slave->charStart = begin;
		slave->posStart ++;
		slave->posLen --;
	}

	return begin;
}

gint
html_text_slave_get_nb_width (HTMLTextSlave *slave, HTMLPainter *painter, gboolean lineBegin)
{
	html_text_slave_remove_leading_space (slave, painter, lineBegin);
	if (slave->owner->words - slave->start_word > 1)
		return html_text_slave_nb_width (slave, painter, 1);

	return html_object_calc_min_width (HTML_OBJECT (slave), painter);
}

static HTMLFitType
hts_fit_line (HTMLObject *o, HTMLPainter *painter,
	      gboolean lineBegin, gboolean firstRun, gboolean next_to_floating, gint widthLeft)
{
	HTMLFitType rv = HTML_FIT_PARTIAL;
	HTMLTextSlave *slave;
	HTMLText *text;
	guint  pos = 0;
	gchar *sep = NULL, *lsep;
	gchar *begin;
	guint words = 0;

	slave = HTML_TEXT_SLAVE (o);
	text  = HTML_TEXT (slave->owner);

	begin = html_text_slave_remove_leading_space (slave, painter, lineBegin);

	/* printf ("fit_line %d left: %d lspacetext: \"%s\"\n", firstRun, widthLeft, begin); */

	sep = begin;
	while (sep && *sep
	       && widthLeft >= html_text_slave_nb_width (slave, painter, words + 1)) {
		words ++;
		lsep   = sep;
		sep    = strchr (lsep + (words > 1 ? 1 : 0), ' ');
		pos    = sep ? g_utf8_pointer_to_offset (begin, sep) : g_utf8_strlen (begin, -1);
		if (words + slave->start_word >= text->words)
			break;
	}

	if (words + slave->start_word == text->words)
		rv = HTML_FIT_COMPLETE;
	else if (words == 0 || get_words_width (text, painter, slave->start_word, words) == 0) {
		if (!firstRun) {
			if (slave->posStart == 0 && text->text [0] != ' ' && HTML_OBJECT (text)->prev) {
				HTMLObject *prev = HTML_OBJECT (text)->prev;
				if (HTML_IS_TEXT_SLAVE (prev) && HTML_TEXT_SLAVE (prev)->posLen
				    && HTML_TEXT_SLAVE (prev)->owner->text [strlen (HTML_TEXT_SLAVE (prev)->owner->text) - 1]
				    != ' ')
					rv = slave->start_word + 1 == text->words ? HTML_FIT_COMPLETE : HTML_FIT_PARTIAL;
				else
					rv = HTML_FIT_NONE;
			} else
				rv = HTML_FIT_NONE;
		} else if (slave->start_word + 1 == text->words)
			rv = next_to_floating ? HTML_FIT_NONE : HTML_FIT_COMPLETE;
		else {
			words ++;

			if (sep)
				sep = strchr (sep + (words > 1 ? 0 : 1), ' ');

			pos = sep ? g_utf8_pointer_to_offset (begin, sep) : g_utf8_strlen (begin, -1);
		}
	}

	if (rv == HTML_FIT_PARTIAL)
		if (pos < slave->posLen) {
			split (slave, pos, slave->start_word + words, sep);
		o->width = get_words_width (text, painter, slave->start_word, words);
	}

#ifdef HTML_TEXT_SLAVE_DEBUG
	debug_print (rv, html_text_slave_get_text (slave), slave->posLen);
#endif

	return rv;	
}

static gboolean
select_range (HTMLObject *self,
	      HTMLEngine *engine,
	      guint start, gint length,
	      gboolean queue_draw)
{
	return FALSE;
}

static guint
get_length (HTMLObject *self)
{
	return 0;
}


/* HTMLObject::draw() implementation.  */

static gint
get_ys (HTMLText *text, HTMLPainter *p)
{
	if (text->font_style & GTK_HTML_FONT_STYLE_SUBSCRIPT || text->font_style & GTK_HTML_FONT_STYLE_SUPERSCRIPT) {
		gint height2;

		height2 = (HTML_OBJECT (text)->ascent + HTML_OBJECT (text)->descent) / 2;
		/* FIX2? (html_painter_calc_ascent (p, text->font_style, text->face)
		   + html_painter_calc_descent (p, text->font_style, text->face)) >> 1; */
		return (text->font_style & GTK_HTML_FONT_STYLE_SUBSCRIPT) ? height2 : -height2;
			
	} else 
		return 0;
}

static void
draw_spell_errors (HTMLTextSlave *slave, HTMLPainter *p, gint tx, gint ty, gint line_offset)
{
	GList *cur = HTML_TEXT (slave->owner)->spell_errors;
	HTMLObject *obj = HTML_OBJECT (slave);
	SpellError *se;
	guint ma, mi;
	gint x_off = 0;
	gint last_off = 0;
	gchar *text = html_text_slave_get_text (slave);

	while (cur) {

		se = (SpellError *) cur->data;
		ma = MAX (se->off, slave->posStart);
		mi = MIN (se->off + se->len, slave->posStart + slave->posLen);
		if (ma < mi) {
			GList *items, *glyphs;
			guint off = ma - slave->posStart;
			guint len = mi - ma;
			gint lo, width, asc, dsc;

			html_painter_set_pen (p, &html_colorset_get_color_allocated (p, HTMLSpellErrorColor)->color);
			/* printf ("spell error: %s\n", html_text_get_text (slave->owner, off)); */
			lo = line_offset;
			
			glyphs = get_glyphs_part (slave, p, last_off, off - last_off, line_offset, &items);
			html_painter_calc_text_size (p, text,
						     off - last_off, items, glyphs, &line_offset,
						     p->font_style,
						     p->font_face, &width, &asc, &dsc);
			glyphs_destroy (glyphs);
			x_off += width;
			text = g_utf8_offset_to_pointer (text, off - last_off);
			glyphs = get_glyphs_part (slave, p, off, len, line_offset, &items);
			x_off += html_painter_draw_spell_error (p, obj->x + tx + x_off,
								obj->y + ty + get_ys (HTML_TEXT (slave->owner), p),
								text, len, items, glyphs);
			glyphs_destroy (glyphs);
			last_off = off + len;
			line_offset += len;
			text = g_utf8_offset_to_pointer (text, len);
		}
		if (se->off > slave->posStart + slave->posLen)
			break;
		cur = cur->next;
	}
}

static GList *
get_items (HTMLTextSlave *slave, HTMLPainter *painter)
{
	if (!slave->items) {
		PangoItem *item;
		gint start_offset = html_text_slave_get_text (slave) - slave->owner->text;

		slave->items = html_text_get_items (slave->owner, painter);
		if (slave->items) {
			item = (PangoItem *) slave->items->data;

			while (slave->items && start_offset >= item->offset + item->length) {
				slave->items = slave->items->next;
				item = (PangoItem *) slave->items->data;
			}
		}
	}

	return slave->items;
}

static GList *
get_glyphs_part (HTMLTextSlave *slave, HTMLPainter *painter, guint offset, guint len, gint line_offset, GList **items)
{
	GList *glyphs = NULL;
	const gchar *text;
	gchar *translated_text;

	*items = get_items (slave, painter);
	if (items) {
		PangoGlyphString *str;
		PangoItem *item;
		GList *il;
		gint bytes, index, current_len, index_in_item;
		const gchar *slave_text, *item_text;

		/* printf ("get_glyphs_part %d,%d\n", offset, len); */

		slave_text = html_text_slave_get_text (slave);
		text = g_utf8_offset_to_pointer (slave_text, offset);
		index = offset;
		while (text - slave->owner->text >= ((PangoItem *) (*items)->data)->offset + ((PangoItem *) (*items)->data)->length) {
			*items = (*items)->next;
			/* printf ("skip item, new num_chars: %d\n", ((PangoItem *) (*items)->data)->num_chars); */
		}
		il = *items;
		while (il && index - offset < len) {
			item = (PangoItem *) il->data;
			str = pango_glyph_string_new ();
			item_text = slave->owner->text + item->offset;
			index_in_item = g_utf8_pointer_to_offset (item_text, text);
			current_len = MIN (item->num_chars - index_in_item, len - (index - offset));
			/* printf ("iii: %d oii: %d\n", index_in_item, text - item_text);
			   printf ("cl: %d offset: %d idx: %d\n", current_len, offset, index); */
			translated_text = html_painter_translate_text (text, current_len, &line_offset, &bytes);
			pango_shape (translated_text, bytes, &item->analysis, str);
			g_free (translated_text);
			glyphs = g_list_prepend (glyphs, str);
			text = g_utf8_offset_to_pointer (text, current_len);
			index += current_len;
			il = il->next;
		}
		glyphs = g_list_reverse (glyphs);
	}

	return glyphs;
}

static GList *
get_glyphs (HTMLTextSlave *slave, HTMLPainter *painter, gint line_offset)
{
	if (!slave->glyphs) {
		GList *items = get_items (slave, painter);
		const gchar *text;
		gchar *translated_text;

		if (items) {
			PangoGlyphString *str;
			PangoItem *item;
			GList *il = items;
			gint bytes, index, len;

			text = html_text_slave_get_text (slave);
			index = 0;
			while (il && index < slave->posLen) {
				item = (PangoItem *) il->data;
				str = pango_glyph_string_new ();
				len = MIN (item->num_chars, slave->posLen - index);
				translated_text = html_painter_translate_text (text, len, &line_offset, &bytes);
				pango_shape (translated_text, bytes, &item->analysis, str);
				g_free (translated_text);
				slave->glyphs = g_list_prepend (slave->glyphs, str);
				text = g_utf8_offset_to_pointer (text, len);
				index += len;
				il = il->next;
			}
			slave->glyphs = g_list_reverse (slave->glyphs);
		}
	}
	return slave->glyphs;
}

static void
draw_normal (HTMLTextSlave *self,
	     HTMLPainter *p,
	     GtkHTMLFontStyle font_style,
	     gint x, gint y,
	     gint width, gint height,
	     gint tx, gint ty, gint line_offset)
{
	HTMLObject *obj;
	HTMLText *text = self->owner;
	gchar *str;
	gint lo;

	obj = HTML_OBJECT (self);

	str = html_text_slave_get_text (self);
	if (*str) {
		html_painter_set_font_style (p, font_style);
		html_painter_set_font_face  (p, HTML_TEXT (self->owner)->face);
		html_color_alloc (HTML_TEXT (self->owner)->color, p);
		html_painter_set_pen (p, &HTML_TEXT (self->owner)->color->color);
		lo = html_text_slave_get_line_offset (self, line_offset, self->posStart, p);
		html_painter_draw_text (p,
					obj->x + tx, obj->y + ty + get_ys (text, p),
					str,
					self->posLen,
					get_items (self, p), get_glyphs (self, p, lo),
					lo);
	}
}

static void
draw_highlighted (HTMLTextSlave *slave,
		  HTMLPainter *p,
		  GtkHTMLFontStyle font_style,
		  gint x, gint y,
		  gint width, gint height,
		  gint tx, gint ty, gint line_offset)
{
	HTMLText *owner;
	HTMLObject *obj;
	GList *glyphs1, *glyphs2, *glyphs3, *items1, *items2, *items3;
	guint start, end, len;
	gint offset_width, text_width, lo, lo_start, lo_sel, asc, dsc;
	const gchar *text;
	
	char *slave_begin;
	char *highlight_begin;

	obj = HTML_OBJECT (slave);
	owner = HTML_TEXT (slave->owner);
	start = owner->select_start;
	end = start + owner->select_length;

	text = HTML_TEXT (owner)->text;

	if (start < slave->posStart)
		start = slave->posStart;
	if (end > slave->posStart + slave->posLen)
		end = slave->posStart + slave->posLen;
	len = end - start;

	slave_begin = html_text_slave_get_text (slave);
	highlight_begin = g_utf8_offset_to_pointer (slave_begin, start - slave->posStart);

	lo_start = lo = html_text_slave_get_line_offset (slave, line_offset, slave->posStart, p);

	if (start > slave->posStart) {
		glyphs1 = get_glyphs_part (slave, p, 0, start - slave->posStart, lo_start, &items1);
		html_painter_calc_text_size (p, slave_begin, start - slave->posStart, items1, glyphs1, &lo,
					     font_style, HTML_TEXT (owner)->face, &offset_width, &asc, &dsc);
	} else
		offset_width = 0;
	lo_sel = lo;

	if (len) {
		glyphs2 = get_glyphs_part (slave, p, start - slave->posStart, len, lo_sel, &items2);

		html_painter_calc_text_size (p, highlight_begin, len, items2, glyphs2, &lo,
					     font_style, HTML_TEXT (owner)->face, &text_width, &asc, &dsc);
		/* printf ("s: %d l: %d - %d %d\n", start, len, offset_width, text_width); */
	} else
		text_width = 0;

	html_painter_set_font_style (p, font_style);
	html_painter_set_font_face  (p, HTML_TEXT (owner)->face);

	/* Draw the highlighted part with a highlight background.  */

	html_color_alloc (HTML_TEXT (owner)->color, p);
	html_painter_set_pen (p, &html_colorset_get_color_allocated
			      (p, p->focus ? HTMLHighlightColor : HTMLHighlightNFColor)->color);
	html_painter_fill_rect (p, obj->x + tx + offset_width, obj->y + ty - obj->ascent,
				text_width, obj->ascent + obj->descent);
	html_painter_set_pen (p, &html_colorset_get_color_allocated
			      (p, p->focus ? HTMLHighlightTextColor : HTMLHighlightTextNFColor)->color);

	if (len) {
		html_painter_draw_text (p, obj->x + tx + offset_width, 
					obj->y + ty + get_ys (HTML_TEXT (slave->owner), p),
					highlight_begin, len, items2, glyphs2,
					lo_sel);
		if (glyphs2)
			glyphs_destroy (glyphs2);
	}
	/* Draw the non-highlighted part.  */
	html_painter_set_pen (p, &HTML_TEXT (owner)->color->color);

	/* 1. Draw the leftmost non-highlighted part, if any.  */
	if (start > slave->posStart) {
		html_painter_draw_text (p,
					obj->x + tx, obj->y + ty + get_ys (HTML_TEXT (slave->owner), p),
					slave_begin,
					start - slave->posStart, items1, glyphs1,
					lo_start);
		if (glyphs1)
			glyphs_destroy (glyphs1);
	}

	/* 2. Draw the rightmost non-highlighted part, if any.  */
	if (end < slave->posStart + slave->posLen) {
		glyphs3 = get_glyphs_part (slave, p, start + len - slave->posStart, slave->posLen - start - len + slave->posStart, lo, &items3);
		html_painter_draw_text (p,
					obj->x + tx + offset_width + text_width,
					obj->y + ty + get_ys (HTML_TEXT (slave->owner), p),
					g_utf8_offset_to_pointer (highlight_begin, end - start),
					slave->posStart + slave->posLen - end, items3, glyphs3, lo);
		if (glyphs3)
			glyphs_destroy (glyphs3);
	}
}

static void
draw (HTMLObject *o,
      HTMLPainter *p,
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	HTMLTextSlave *textslave;
	HTMLText *owner;
	HTMLText *ownertext;
	GtkHTMLFontStyle font_style;
	guint end;
	GdkRectangle paint;
	gint line_offset;

	/* printf ("slave draw %p\n", o); */

	textslave = HTML_TEXT_SLAVE (o);
	if (!html_object_intersect (o, &paint, x, y, width, height) || textslave->posLen == 0)
		return;
	
	owner = textslave->owner;
	ownertext = HTML_TEXT (owner);
	font_style = html_text_get_font_style (ownertext);
	line_offset = html_text_get_line_offset (owner, p);

	end = textslave->posStart + textslave->posLen;
	if (owner->select_start + owner->select_length <= textslave->posStart
	    || owner->select_start >= end) {
		draw_normal (textslave, p, font_style, x, y, width, height, tx, ty, line_offset);
	} else {
		draw_highlighted (textslave, p, font_style, x, y, width, height, tx, ty, line_offset);
	}

	if (HTML_TEXT (textslave->owner)->spell_errors)
		draw_spell_errors (textslave, p, tx ,ty, line_offset);
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

static guint
get_offset_for_pointer (HTMLTextSlave *slave, HTMLPainter *painter, gint x, gint y)
{
	HTMLText *owner;
	GtkHTMLFontStyle font_style;
	guint width, prev_width;
	guint i;
	gint line_offset, lo, word, ww;
	gchar *space;
	gchar *text;

	g_return_val_if_fail (slave != NULL, 0);

	owner = HTML_TEXT (slave->owner);
	font_style = html_text_get_font_style (owner);

	x -= HTML_OBJECT (slave)->x;

	prev_width  = 0;
	line_offset = html_text_slave_get_line_offset (slave, html_text_get_line_offset (slave->owner, painter), 0, painter);

	i = 1;
	word = 1;
	text = html_text_slave_get_text (slave);
	space = strchr (text, ' ');
	if (space && g_utf8_pointer_to_offset (text, space) <= slave->posLen) {
		html_text_request_word_width (owner, painter);

		ww = get_words_width (owner, painter, slave->start_word, word);
		while (ww < x && space) {
			i = g_utf8_pointer_to_offset (text, space);
			space = strchr (space + 1, ' ');
			if (space && g_utf8_pointer_to_offset (text, space) >= slave->posLen) {
				break;
			}
			word ++;
			ww = get_words_width (owner, painter, slave->start_word, word);
		}
	}

	for ( ; i <= slave->posLen; i++) {
		GList *items;
		GList *glyphs;
		gint asc, dsc;

		lo = line_offset;
		glyphs = get_glyphs_part (slave, painter, 0, i, lo, &items);
		html_painter_calc_text_size (painter, text, i, items, glyphs, &lo, font_style, owner->face, &width, &asc, &dsc);

		if ((width + prev_width) / 2 >= x)
			return i - 1;

		prev_width = width;
	}

	return slave->posLen;
}

static HTMLObject *
check_point (HTMLObject *self,
	     HTMLPainter *painter,
	     gint x, gint y,
	     guint *offset_return,
	     gboolean for_cursor)
{
	if (x >= self->x
	    && x < self->x + MAX (1, self->width)
	    && y >= self->y - self->ascent
	    && y < self->y + self->descent) {
		HTMLTextSlave *slave = HTML_TEXT_SLAVE (self);

		if (offset_return != NULL)
			*offset_return = slave->posStart
				+ get_offset_for_pointer (slave, painter, x, y);

		return HTML_OBJECT (slave->owner);
	}

	return NULL;
}

static void
destroy (HTMLObject *obj)
{
	HTMLTextSlave *slave = HTML_TEXT_SLAVE (obj);

	if (slave->glyphs) {
		glyphs_destroy (slave->glyphs);
		slave->glyphs = NULL;
	}

	HTML_OBJECT_CLASS (parent_class)->destroy (obj);
}

void
html_text_slave_type_init (void)
{
	html_text_slave_class_init (&html_text_slave_class, HTML_TYPE_TEXTSLAVE, sizeof (HTMLTextSlave));
}

void
html_text_slave_class_init (HTMLTextSlaveClass *klass,
			    HTMLType type,
			    guint object_size)
{
	HTMLObjectClass *object_class;

	object_class = HTML_OBJECT_CLASS (klass);

	html_object_class_init (object_class, type, object_size);

	object_class->select_range = select_range;
	object_class->copy = copy;
	object_class->destroy = destroy;
	object_class->draw = draw;
	object_class->calc_size = calc_size;
	object_class->fit_line = hts_fit_line;
	object_class->calc_min_width = calc_min_width;
	object_class->calc_preferred_width = calc_preferred_width;
	object_class->get_url = get_url;
	object_class->get_length = get_length;
	object_class->check_point = check_point;

	parent_class = &html_object_class;
}

void
html_text_slave_init (HTMLTextSlave *slave,
		      HTMLTextSlaveClass *klass,
		      HTMLText *owner,
		      guint posStart,
		      guint posLen,
		      guint start_word)
{
	HTMLText *owner_text;
	HTMLObject *object;

	object = HTML_OBJECT (slave);
	owner_text = HTML_TEXT (owner);

	html_object_init (object, HTML_OBJECT_CLASS (klass));

	object->ascent = HTML_OBJECT (owner)->ascent;
	object->descent = HTML_OBJECT (owner)->descent;

	slave->posStart   = posStart;
	slave->posLen     = posLen;
	slave->start_word = start_word;
	slave->owner      = owner;
	slave->charStart  = NULL;
	slave->items      = NULL;
	slave->glyphs     = NULL;

	/* text slaves have always min_width 0 */
	object->min_width = 0;
	object->change   &= ~HTML_CHANGE_MIN_WIDTH;
}

HTMLObject *
html_text_slave_new (HTMLText *owner, guint posStart, guint posLen, guint start_word)
{
	HTMLTextSlave *slave;

	slave = g_new (HTMLTextSlave, 1);
	html_text_slave_init (slave, &html_text_slave_class, owner, posStart, posLen, start_word);

	return HTML_OBJECT (slave);
}
