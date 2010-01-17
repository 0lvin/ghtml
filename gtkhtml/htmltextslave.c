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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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
#include "htmlobject.h"
#include "htmlprinter.h"
#include "htmlplainpainter.h"
#include "htmlgdkpainter.h"
#include "htmlsettings.h"
#include "gtkhtml.h"


/* #define HTML_TEXT_SLAVE_DEBUG */

HTMLTextSlaveClass html_text_slave_class;
static HTMLObjectClass *parent_class = NULL;

static void    clear_glyph_items (HTMLTextSlave *slave);

gchar *
html_text_slave_get_text (HTMLTextSlave *slave)
{
	if (!slave->charStart)
		slave->charStart = html_text_get_text (slave->owner, slave->posStart);

	return slave->charStart;
}

/* Split this TextSlave at the specified offset.  */
static void
split (HTMLTextSlave *slave, guint offset, gint skip, gchar *start_pointer)
{
	HTMLObject *obj;
	HTMLObject *new;

	g_return_if_fail (offset < slave->posLen);

	obj = HTML_OBJECT (slave);

	new = html_text_slave_new (slave->owner,
				   slave->posStart + offset + skip,
				   slave->posLen - (offset + skip));

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
	HTML_TEXT_SLAVE (dest)->glyph_items = NULL;
}

static inline gint
html_text_slave_get_start_byte_offset (HTMLTextSlave *slave)
{
	return html_text_slave_get_text (slave) - slave->owner->text;
}

static void
hts_update_asc_dsc (HTMLPainter *painter, PangoItem *item, gint *asc, gint *dsc)
{
	PangoFontMetrics *pfm;

	pfm = pango_font_get_metrics (item->analysis.font, item->analysis.language);
	if (asc)
		*asc = MAX (*asc, pango_font_metrics_get_ascent (pfm));
	if (dsc)
		*dsc = MAX (*dsc, pango_font_metrics_get_descent (pfm));
	pango_font_metrics_unref (pfm);
}

static gint
hts_calc_width (HTMLTextSlave *slave, HTMLPainter *painter, gint *asc, gint *dsc)
{
	/*HTMLText *text = slave->owner;
	  gint line_offset, tabs = 0, width = 0;

	line_offset = html_text_slave_get_line_offset (slave, 0, painter);
	if (line_offset != -1)
		width += (html_text_text_line_length (html_text_slave_get_text (slave), &line_offset, slave->posLen, &tabs) - slave->posLen)*
			html_painter_get_space_width (painter, html_text_get_font_style (text), text->face);

			width += html_text_calc_part_width (text, painter, html_text_slave_get_text (slave), slave->posStart, slave->posLen, asc, dsc); */

	GSList *cur, *gilist = html_text_slave_get_glyph_items (slave, painter);
	PangoLanguage *language = NULL;
	PangoFont *font = NULL;
	gint width = 0;

	if (asc)
		*asc = html_painter_engine_to_pango (painter,
						     html_painter_get_space_asc (painter, html_text_get_font_style (slave->owner), slave->owner->face));
	if (dsc)
		*dsc = html_painter_engine_to_pango (painter,
						     html_painter_get_space_dsc (painter, html_text_get_font_style (slave->owner), slave->owner->face));

	for (cur = gilist; cur; cur = cur->next) {
		HTMLTextSlaveGlyphItem *sgi = (HTMLTextSlaveGlyphItem *) cur->data;
		PangoRectangle log_rect;
		PangoItem *item = sgi->glyph_item.item;

		pango_glyph_string_extents (sgi->glyph_item.glyphs, sgi->glyph_item.item->analysis.font, NULL, &log_rect);
		width += log_rect.width;

		if (item->analysis.font != font || item->analysis.language != language)
			hts_update_asc_dsc (painter, item, asc, dsc);
	}

	if (asc)
		*asc = html_painter_pango_to_engine (painter, *asc);
	if (dsc)
		*dsc = html_painter_pango_to_engine (painter, *dsc);

	width = html_painter_pango_to_engine (painter, width);

	/* printf ("hts width: %d\n", width); */

	return width;
}

inline static void
glyphs_destroy (GList *glyphs)
{
	GList *l;

	for (l = glyphs; l; l = l->next->next)
		pango_glyph_string_free ((PangoGlyphString *) l->data);
	g_list_free (glyphs);
}

inline static void
glyph_items_destroy (GSList *glyph_items)
{
	GSList *l;

	for (l = glyph_items; l; l = l->next) {
		HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) l->data;

		if (gi->type == HTML_TEXT_SLAVE_GLYPH_ITEM_CREATED) {
			if (gi->glyph_item.item)
				pango_item_free (gi->glyph_item.item);
			if (gi->glyph_item.glyphs)
				pango_glyph_string_free (gi->glyph_item.glyphs);
			g_free (gi->widths);
		}

		g_free (gi);
	}
	g_slist_free (glyph_items);
}

static gboolean
html_text_slave_real_calc_size (HTMLObject *self, HTMLPainter *painter, GList **changed_objs)
{
	HTMLText *owner;
	HTMLTextSlave *slave;
	GtkHTMLFontStyle font_style;
	gint new_ascent, new_descent, new_width;
	gboolean changed;

	slave = HTML_TEXT_SLAVE (self);
	owner = HTML_TEXT (slave->owner);
	font_style = html_text_get_font_style (owner);

	new_width = MAX (1, hts_calc_width (slave, painter, &new_ascent, &new_descent));

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
html_text_slave_get_line_offset (HTMLTextSlave *slave, gint offset, HTMLPainter *p)
{
	HTMLObject *head = HTML_OBJECT (slave->owner)->next;

	g_assert (HTML_IS_TEXT_SLAVE (head));

	if (!html_clueflow_tabs (HTML_CLUEFLOW (HTML_OBJECT (slave)->parent), p))
		return -1;

	if (head->y + head->descent - 1 < HTML_OBJECT (slave)->y - HTML_OBJECT (slave)->ascent) {
		HTMLObject *prev;
		HTMLTextSlave *bol;
		gint line_offset = 0;

		prev = html_object_prev (HTML_OBJECT (slave)->parent, HTML_OBJECT (slave));
		while (prev->y + prev->descent - 1 >= HTML_OBJECT (slave)->y - HTML_OBJECT (slave)->ascent)
			prev = html_object_prev (HTML_OBJECT (slave)->parent, HTML_OBJECT (prev));

		bol = HTML_TEXT_SLAVE (prev->next);
		return html_text_text_line_length (html_text_slave_get_text (bol),
						   &line_offset, slave->posStart + offset - bol->posStart, NULL);
	} else
		return html_text_get_line_offset (slave->owner, p, slave->posStart + offset);
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

gchar *
html_text_slave_remove_leading_space (HTMLTextSlave *slave, HTMLPainter *painter, gboolean lineBegin)
{
	gchar *begin;

	begin = html_text_slave_get_text (slave);
	if (*begin == ' ' && could_remove_leading_space (slave, lineBegin)) {
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

	return html_object_calc_min_width (HTML_OBJECT (slave), painter);
}

/*
 * offset: current position - gchar offset
 * s: current position - string pointer
 * ii: current position - index of item
 * io: current position - offset within item
 * line_offset:
 * w: width for current position
 * lwl: last-whitespacing-length (in characters)
 * lbw: last-break-width
 * lbo: last-break-offset (into text)
 * lbsp: last-break-string-pointer
 *
 * Checks to see if breaking the text at the given position would result in it fitting
 * into the remaining width. If so, updates the last-break information (lwl,lbw,lbo,lbsp).
 * We'll actually break the item at the last break position that still fits.
 */
static gboolean
update_lb (HTMLTextSlave *slave, HTMLPainter *painter, gint widthLeft, gint offset, gchar *s, gint ii, gint io, gint line_offset,
	   gint w, gint *lwl, gint *lbw, gint *lbo, gchar **lbsp, gboolean *force_fit)
{
	gint new_ltw, new_lwl, aw;

	new_ltw = html_text_tail_white_space (slave->owner, painter, offset, ii, io, &new_lwl, line_offset, s);
	aw = w - new_ltw;

	if (aw <= widthLeft || *force_fit) {
		*lwl = new_lwl;
		*lbw = aw;
		*lbo = offset;
		*lbsp = s;
		if (*force_fit && *lbw >= widthLeft)
			return TRUE;
		*force_fit = FALSE;
	} else
		return TRUE;

	return FALSE;
}

static HTMLFitType
hts_fit_line (HTMLObject *o, HTMLPainter *painter,
	      gboolean lineBegin, gboolean firstRun, gboolean next_to_floating, gint widthLeft)
{
	HTMLTextSlave *slave = HTML_TEXT_SLAVE (o);
	gint lbw, w, lbo, lwl, offset;
	gint ii, io, line_offset;
	gchar *s, *lbsp;
	HTMLFitType rv = HTML_FIT_NONE;
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gboolean force_fit = lineBegin;

	if (slave->posLen == 0)
		return HTML_FIT_COMPLETE;

	widthLeft = html_painter_engine_to_pango (painter, widthLeft);

	lbw = lwl = w = 0;
	offset = lbo = slave->posStart;
	ii = html_text_get_item_index (slave->owner, painter, offset, &io);

	line_offset = html_text_get_line_offset (slave->owner, painter, offset);
	lbsp = s = html_text_slave_get_text (slave);

	while ((force_fit || widthLeft > lbw) && offset < slave->posStart + slave->posLen) {
		if (offset > slave->posStart && offset > lbo && html_text_is_line_break (pi->attrs [offset]))
			if (update_lb (slave, painter, widthLeft, offset, s, ii, io, line_offset, w, &lwl, &lbw, &lbo, &lbsp, &force_fit))
				break;

		if (io == 0 && slave->owner->text [pi->entries [ii].glyph_item.item->offset]  == '\t') {
			GtkHTMLFontStyle font_style;
			gchar *face;
			gint skip = 8 - (line_offset % 8);

			if (HTML_IS_PLAIN_PAINTER (painter)) {
				font_style = GTK_HTML_FONT_STYLE_FIXED;
				face = NULL;
			} else {
				font_style = html_text_get_font_style (slave->owner);
				face = slave->owner->face;
			}

			pi->entries [ii].glyph_item.glyphs->glyphs[0].geometry.width = pi->entries [ii].widths [io]
				= skip*html_painter_get_space_width (painter, font_style, face) * PANGO_SCALE;
			line_offset += skip;
		} else {
			line_offset ++;
		}
		w += pi->entries [ii].widths [io];

		html_text_pi_forward (pi, &ii, &io);
		s = g_utf8_next_char (s);
		offset ++;
	}

	if (offset == slave->posStart + slave->posLen && (widthLeft >= w || force_fit)) {
		rv = HTML_FIT_COMPLETE;
		if (slave->posLen)
			o->width = html_painter_pango_to_engine (painter, w);
	} else if (lbo > slave->posStart) {
		split (slave, lbo - slave->posStart - lwl, lwl, lbsp);
		rv = HTML_FIT_PARTIAL;
		o->width = html_painter_pango_to_engine (painter, lbw);
		o->change |= HTML_CHANGE_RECALC_PI;
	}

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

static inline GList *
get_glyphs_base_text (GList *glyphs, PangoItem *item, gint ii, const gchar *text, gint bytes)
{
	PangoGlyphString *str;

	str = pango_glyph_string_new ();
	pango_shape (text, bytes, &item->analysis, str);
	glyphs = g_list_prepend (glyphs, str);
	glyphs = g_list_prepend (glyphs, GINT_TO_POINTER (ii));

	return glyphs;
}

GList *
html_get_glyphs_non_tab (GList *glyphs, PangoItem *item, gint ii, const gchar *text, gint bytes, gint len)
{
	gchar *tab;

	while ((tab = memchr (text, (guchar) '\t', bytes))) {
		gint c_bytes = tab - text;
		if (c_bytes > 0)
			glyphs = get_glyphs_base_text (glyphs, item, ii, text, c_bytes);
		text += c_bytes + 1;
		bytes -= c_bytes + 1;
	}

	if (bytes > 0)
		glyphs = get_glyphs_base_text (glyphs, item, ii, text, bytes);

	return glyphs;
}

/*
 * NB: This implement the exact same algorithm as
 *     reorder-items.c:pango_reorder_items().
 */

static GSList *
reorder_glyph_items (GSList *glyph_items, gint n_items)
{
	GSList *tmp_list, *level_start_node;
	gint i, level_start_i;
	gint min_level = G_MAXINT;
	GSList *result = NULL;

	if (n_items == 0)
		return NULL;

	tmp_list = glyph_items;
	for (i = 0; i < n_items; i ++) {
		HTMLTextSlaveGlyphItem *gi = tmp_list->data;

		min_level = MIN (min_level, gi->glyph_item.item->analysis.level);

		tmp_list = tmp_list->next;
	}

	level_start_i = 0;
	level_start_node = glyph_items;
	tmp_list = glyph_items;
	for (i = 0; i < n_items; i ++) {
		HTMLTextSlaveGlyphItem *gi= tmp_list->data;

		if (gi->glyph_item.item->analysis.level == min_level) {
			if (min_level % 2) {
				if (i > level_start_i)
					result = g_slist_concat (reorder_glyph_items (level_start_node, i - level_start_i), result);
				result = g_slist_prepend (result, gi);
			} else {
				if (i > level_start_i)
					result = g_slist_concat (result, reorder_glyph_items (level_start_node, i - level_start_i));
				result = g_slist_append (result, gi);
			}

			level_start_i = i + 1;
			level_start_node = tmp_list->next;
		}

		tmp_list = tmp_list->next;
	}

	if (min_level % 2) {
		if (i > level_start_i)
			result = g_slist_concat (reorder_glyph_items (level_start_node, i - level_start_i), result);
	} else {
		if (i > level_start_i)
			result = g_slist_concat (result, reorder_glyph_items (level_start_node, i - level_start_i));
	}

	return result;
}

static GSList *
get_glyph_items_in_range (HTMLTextSlave *slave, HTMLPainter *painter, gint start_offset, gint len)
{
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gint i, offset, end_offset, n_items = 0;
	GSList *glyph_items = NULL;

	start_offset += slave->posStart;
	end_offset = start_offset + len;

	for (offset = 0, i = 0; i < pi->n; i ++) {
		PangoItem *item = pi->entries [i].glyph_item.item;

		/* do item and slave overlap? */
		if (MAX (offset, start_offset) < MIN (offset + item->num_chars, end_offset)) {
			HTMLTextSlaveGlyphItem *glyph_item = g_new (HTMLTextSlaveGlyphItem, 1);

			/* use text glyph item */
			glyph_item->type = HTML_TEXT_SLAVE_GLYPH_ITEM_PARENTAL;
			glyph_item->glyph_item = pi->entries [i].glyph_item;
			glyph_item->widths = pi->entries [i].widths;

			if (start_offset > offset) {
				/* need to cut the beginning of current glyph item */
				PangoGlyphItem *tmp_gi;
				gint split_index;

				glyph_item->type = HTML_TEXT_SLAVE_GLYPH_ITEM_CREATED;
				glyph_item->widths = NULL;
				glyph_item->glyph_item.item = pango_item_copy (glyph_item->glyph_item.item);
				glyph_item->glyph_item.glyphs = pango_glyph_string_copy (glyph_item->glyph_item.glyphs);

				split_index = g_utf8_offset_to_pointer (slave->owner->text + item->offset, start_offset - offset)
					- (slave->owner->text + item->offset);
				tmp_gi = pango_glyph_item_split (&glyph_item->glyph_item, slave->owner->text, split_index);

				/* free the beginning we don't need */
				pango_glyph_item_free (tmp_gi);

			}

			if (offset + item->num_chars > end_offset) {
				/* need to cut the ending of current glyph item */
				PangoGlyphItem tmp_gi1, *tmp_gi2;
				gint split_index;

				if (glyph_item->type == HTML_TEXT_SLAVE_GLYPH_ITEM_PARENTAL) {
					tmp_gi1.item = pango_item_copy (glyph_item->glyph_item.item);
					tmp_gi1.glyphs = pango_glyph_string_copy (glyph_item->glyph_item.glyphs);
				} else
					tmp_gi1 = glyph_item->glyph_item;

				split_index = g_utf8_offset_to_pointer (slave->owner->text + tmp_gi1.item->offset, end_offset - MAX (start_offset, offset))
					- (slave->owner->text + tmp_gi1.item->offset);
				tmp_gi2 = pango_glyph_item_split (&tmp_gi1, slave->owner->text, split_index);

				glyph_item->glyph_item = *tmp_gi2;
				tmp_gi2->item = NULL;
				tmp_gi2->glyphs = NULL;

				/* free the tmp1 content and tmp2 container, but not the content */
				pango_item_free (tmp_gi1.item);
				pango_glyph_string_free (tmp_gi1.glyphs);
				pango_glyph_item_free (tmp_gi2);

				glyph_item->type = HTML_TEXT_SLAVE_GLYPH_ITEM_CREATED;
				glyph_item->widths = NULL;
			}

			glyph_items = g_slist_prepend (glyph_items, glyph_item);
			n_items ++;
		}

		if (offset + item->num_chars >= end_offset)
			break;
		offset += item->num_chars;
	}

	if (glyph_items) {
		GSList *reversed;

		reversed = g_slist_reverse (glyph_items);
		glyph_items = reorder_glyph_items (reversed, n_items);
		g_slist_free (reversed);
	}

	return glyph_items;
}

GSList *
html_text_slave_get_glyph_items (HTMLTextSlave *slave, HTMLPainter *painter)
{
	if (painter && (!slave->glyph_items || (HTML_OBJECT (slave)->change & HTML_CHANGE_RECALC_PI))) {
		clear_glyph_items (slave);

		HTML_OBJECT (slave)->change &= ~HTML_CHANGE_RECALC_PI;
		slave->glyph_items = get_glyph_items_in_range (slave, painter, 0, slave->posLen);
	}

	return slave->glyph_items;
}

static gboolean
calc_glyph_range_size (HTMLText *text, PangoGlyphItem *glyph_item, gint start_index, gint end_index, gint *x_offset, gint *width, gint *asc, gint *height)
{
	gint isect_start, isect_end;

	isect_start = MAX (glyph_item->item->offset, start_index);
	isect_end = MIN (glyph_item->item->offset + glyph_item->item->length, end_index);

	isect_start -= glyph_item->item->offset;
	isect_end -= glyph_item->item->offset;

	/* printf ("calc_glyph_range_size isect start %d end %d (end_index %d)\n", isect_start, isect_end, end_index); */

	if (isect_start <= isect_end) {
		PangoRectangle log_rect;
		gint start_x, end_x;

		pango_glyph_string_index_to_x (glyph_item->glyphs,
					       text->text + glyph_item->item->offset,
					       glyph_item->item->length,
					       &glyph_item->item->analysis,
					       isect_start,
					       FALSE, &start_x);

		if (isect_start < isect_end)
			pango_glyph_string_index_to_x (glyph_item->glyphs,
						       text->text + glyph_item->item->offset,
						       glyph_item->item->length,
						       &glyph_item->item->analysis,
						       isect_end,
						       FALSE, &end_x);
		else
			end_x = start_x;

		if (asc || height)
			/* this call is used only to get ascent and height */
			pango_glyph_string_extents (glyph_item->glyphs, glyph_item->item->analysis.font, NULL, &log_rect);

		/* printf ("selection_start_index %d selection_end_index %d isect_start %d isect_end %d start_x %d end_x %d cwidth %d width %d\n",
		   selection_start_index, selection_end_index, isect_start, isect_end,
		   html_painter_pango_to_engine (p, start_x), html_painter_pango_to_engine (p, end_x),
		   html_painter_pango_to_engine (p, start_x < end_x ? (end_x - start_x) : (start_x - end_x)),
		   html_painter_pango_to_engine (p, log_rect.width)); */

		if (x_offset)
			*x_offset = MIN (start_x, end_x);
		if (width)
			*width = start_x < end_x ? (end_x - start_x) : (start_x - end_x);
		if (asc)
			*asc = PANGO_ASCENT (log_rect);
		if (height)
			*height = log_rect.height;

		return TRUE;
	}

	return FALSE;
}

static void
draw_text (HTMLTextSlave *self,
	   HTMLPainter *p,
	   GtkHTMLFontStyle font_style,
	   gint x, gint y,
	   gint width, gint height,
	   gint tx, gint ty)
{
	HTMLObject *obj;
	HTMLText *text = self->owner;
	GSList *cur;
	gint run_width;
	gint selection_start_index = 0;
	gint selection_end_index = 0;
	gint isect_start, isect_end;
	gboolean selection;
	GdkColor selection_fg, selection_bg;
	HTMLEngine *e = NULL;

	obj = HTML_OBJECT (self);

	isect_start = MAX (text->select_start, self->posStart);
	isect_end = MIN (text->select_start + text->select_length, self->posStart + self->posLen);
	selection = isect_start < isect_end;

	if (p->widget && GTK_IS_HTML (p->widget))
		e = html_object_engine (HTML_OBJECT (self->owner), GTK_HTML (p->widget)->engine);

	if (selection) {
		gchar *end;
		gchar *start;

		start = html_text_get_text (text, isect_start);
		end = g_utf8_offset_to_pointer (start, isect_end - isect_start);

		selection_start_index = start - text->text;
		selection_end_index = end - text->text;

		if (e) {
			selection_fg = html_colorset_get_color_allocated
				(e->settings->color_set, p,
				 p->focus ? HTMLHighlightTextColor : HTMLHighlightTextNFColor)->color;
			selection_bg = html_colorset_get_color_allocated
				(e->settings->color_set, p,
				 p->focus ? HTMLHighlightColor : HTMLHighlightNFColor)->color;
		}
	}

	/* printf ("draw_text %d %d %d\n", selection_bg.red, selection_bg.green, selection_bg.blue); */

	run_width = 0;
	for (cur = html_text_slave_get_glyph_items (self, p); cur; cur = cur->next) {
		HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) cur->data;
		GList *cur_se;
		gint cur_width;

		if (e)
			html_painter_set_pen (p, &html_colorset_get_color_allocated (e->settings->color_set,
										     e->painter, HTMLTextColor)->color);
		cur_width = html_painter_draw_glyphs (p, obj->x + tx + html_painter_pango_to_engine (p, run_width),
						      obj->y + ty + get_ys (text, p), gi->glyph_item.item, gi->glyph_item.glyphs, NULL, NULL);

		if (selection) {
			gint start_x, width, asc, height;
			gint cx, cy, cw, ch;

			if (calc_glyph_range_size (text, &gi->glyph_item, selection_start_index, selection_end_index, &start_x, &width, &asc, &height) && width > 0) {
				html_painter_get_clip_rectangle (p, &cx, &cy, &cw, &ch);
/*				printf ("run_width; %d start_x %d index %d\n", run_width, start_x, selection_start_index); */
				html_painter_set_clip_rectangle (p,
							obj->x + tx + html_painter_pango_to_engine (p, run_width + start_x),
							obj->y + ty + get_ys (text, p) - html_painter_pango_to_engine (p, asc),
							html_painter_pango_to_engine (p, width),
							html_painter_pango_to_engine (p, height));

				/* printf ("draw selection %d %d %d at %d, %d\n", selection_bg.red, selection_bg.green, selection_bg.blue,
				   obj->x + tx + run_width, obj->y + ty + get_ys (text, p)); */
				html_painter_draw_glyphs (p, obj->x + tx + html_painter_pango_to_engine (p, run_width),
							  obj->y + ty + get_ys (text, p), gi->glyph_item.item, gi->glyph_item.glyphs,
							  &selection_fg, &selection_bg);
				html_painter_set_clip_rectangle (p, cx, cy, cw, ch);
			}
		}

		for (cur_se = text->spell_errors; e && cur_se; cur_se = cur_se->next) {
			SpellError *se;
			guint ma, mi;

			se = (SpellError *) cur_se->data;
			ma = MAX (se->off, self->posStart);
			mi = MIN (se->off + se->len, self->posStart + self->posLen);

			if (ma < mi) {
				gint width, height, asc, start_x;

				gchar *end;
				gchar *start;
				gint se_start_index, se_end_index;

				start = html_text_get_text (text, ma);
				end = g_utf8_offset_to_pointer (start, mi - ma);

				se_start_index = start - text->text;
				se_end_index = end - text->text;

				if (calc_glyph_range_size (text, &gi->glyph_item, se_start_index, se_end_index, &start_x, &width, &asc, &height)) {
					html_painter_set_pen (p, &html_colorset_get_color_allocated (e->settings->color_set,
												     p, HTMLSpellErrorColor)->color);
					/* printf ("spell error: %s\n", html_text_get_text (slave->owner, off)); */

					html_painter_draw_spell_error (p, obj->x + tx + html_painter_pango_to_engine (p, run_width + start_x),
								       obj->y + ty + get_ys (self->owner, p), html_painter_pango_to_engine (p, width));
				}
			}
			if (se->off > self->posStart + self->posLen)
				break;
		}

		run_width += cur_width;
	}
}

static void
draw_focus_rectangle  (HTMLTextSlave *slave, HTMLPainter *painter, GdkRectangle *box)
{
	HTMLGdkPainter *p;
	GdkGCValues values;
	gint8 dash_list[] = { 1, 1 };
	HTMLEngine *e;

	if (painter->widget && GTK_IS_HTML (painter->widget))
		e = html_object_engine (HTML_OBJECT (slave->owner), GTK_HTML (painter->widget)->engine);
	else
		return;

	if (HTML_IS_PRINTER (painter))
		return;

	p = HTML_GDK_PAINTER (painter);
	/* printf ("draw_text_focus\n"); */

	gdk_gc_set_foreground (p->gc, &html_colorset_get_color_allocated (e->settings->color_set,
									  painter, HTMLTextColor)->color);
	gdk_gc_get_values (p->gc, &values);

	gdk_gc_set_line_attributes (p->gc, 1, GDK_LINE_ON_OFF_DASH, values.cap_style, values.join_style);
	gdk_gc_set_dashes (p->gc, 2, dash_list, 2);
	gdk_draw_rectangle (p->pixmap, p->gc, 0, box->x - p->x1, box->y - p->y1, box->width - 1, box->height - 1);
	gdk_gc_set_line_attributes (p->gc, 1, values.line_style, values.cap_style, values.join_style);
}

static void
draw_focus (HTMLTextSlave *slave, HTMLPainter *p, gint tx, gint ty)
{
	GdkRectangle rect;
	Link *link = html_text_get_link_at_offset (slave->owner, slave->owner->focused_link_offset);

	if (link && MAX (link->start_offset, slave->posStart) < MIN (link->end_offset, slave->posStart + slave->posLen)) {
		gint bw = 0;
		html_object_get_bounds (HTML_OBJECT (slave), &rect);
		if (slave->posStart < link->start_offset)
			bw = html_text_calc_part_width (slave->owner, p, html_text_slave_get_text (slave),
							slave->posStart, link->start_offset - slave->posStart, NULL, NULL);
		rect.x += tx + bw;
		rect.width -= bw;
		if (slave->posStart + slave->posLen > link->end_offset)
			rect.width -= html_text_calc_part_width (slave->owner, p, slave->owner->text + link->end_index, link->end_offset,
								 slave->posStart + slave->posLen - link->end_offset, NULL, NULL);
		rect.y += ty;
		draw_focus_rectangle (slave, p, &rect);
	}
}

static void
draw (HTMLObject *o,
      HTMLPainter *p,
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	HTMLTextSlave *slave;
	HTMLText *owner;
	GtkHTMLFontStyle font_style;
	GdkRectangle paint;

	/* printf ("slave draw %p\n", o); */

	slave = HTML_TEXT_SLAVE (o);
	if (!html_object_intersect (o, &paint, x, y, width, height) || slave->posLen == 0)
		return;

	owner = slave->owner;
	font_style = html_text_get_font_style (owner);

	draw_text (slave, p, font_style, x, y, width, height, tx, ty);

	if (HTML_OBJECT (owner)->draw_focused)
		draw_focus (slave, p, tx, ty);
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
get_url (HTMLObject *o, gint offset)
{
	HTMLTextSlave *slave;

	slave = HTML_TEXT_SLAVE (o);
	return html_object_get_url (HTML_OBJECT (slave->owner), offset);
}

static gint
calc_offset (HTMLTextSlave *slave, HTMLPainter *painter, gint x)
{
	GSList *cur, *glyphs = html_text_slave_get_glyph_items (slave, painter);
	gint width = 0, offset = html_object_get_direction (HTML_OBJECT (slave->owner)) == HTML_DIRECTION_RTL ? slave->posLen : 0;
	PangoItem *item = NULL;

	if (glyphs) {
		gint i = 0;

		for (cur = glyphs; cur; cur = cur->next) {
			HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) cur->data;

			item = gi->glyph_item.item;

			if (gi->widths == NULL) {

				gi->widths = g_new (PangoGlyphUnit, item->num_chars);
				html_tmp_fix_pango_glyph_string_get_logical_widths (gi->glyph_item.glyphs, slave->owner->text + item->offset, item->length,
										    item->analysis.level, gi->widths);
			}

			if (item->analysis.level % 2 == 0) {
				/* LTR */
				for (i = 0; i < item->num_chars; i ++) {
					if (x < html_painter_pango_to_engine (painter, width + gi->widths [i] / 2))
						goto done;
					width += gi->widths [i];
				}
			} else {
				/* RTL */
				for (i = item->num_chars - 1; i >= 0; i --) {
					if (x < html_painter_pango_to_engine (painter, width + gi->widths [i] / 2)) {
						i ++;
						goto done;
					}
					width += gi->widths [i];
				}
			}
		}

	done:

		if (cur)
			offset = g_utf8_pointer_to_offset (html_text_slave_get_text (slave), slave->owner->text + item->offset) + i;
		else {
			offset = html_object_get_direction (HTML_OBJECT (slave->owner)) == HTML_DIRECTION_RTL ? 0 : slave->posLen;
		}
	}

/*	printf ("offset %d\n", offset); */

	return offset;
}

static guint
get_offset_for_pointer (HTMLTextSlave *slave, HTMLPainter *painter, gint x, gint y)
{
	g_return_val_if_fail (slave != NULL, 0);

	x -= HTML_OBJECT (slave)->x;

	if (x <= 0)
		return 0;

	if (x >= HTML_OBJECT (slave)->width - 1)
		return slave->posLen;

	if (slave->posLen > 1)
		return calc_offset (slave, painter, x);
	else
		return x > HTML_OBJECT (slave)->width / 2 ? 1 : 0;
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
clear_glyph_items (HTMLTextSlave *slave)
{
	if (slave->glyph_items) {
		glyph_items_destroy (slave->glyph_items);
		slave->glyph_items = NULL;
	}
}

static void
destroy (HTMLObject *obj)
{
	HTMLTextSlave *slave = HTML_TEXT_SLAVE (obj);

	clear_glyph_items (slave);

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
	object_class->calc_size = html_text_slave_real_calc_size;
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
		      guint posLen)
{
	HTMLObject *object;

	object = HTML_OBJECT (slave);

	html_object_init (object, HTML_OBJECT_CLASS (klass));

	object->ascent = HTML_OBJECT (owner)->ascent;
	object->descent = HTML_OBJECT (owner)->descent;

	slave->posStart   = posStart;
	slave->posLen     = posLen;
	slave->owner      = owner;
	slave->charStart  = NULL;
	slave->pi         = NULL;
	slave->glyph_items = NULL;

	/* text slaves have always min_width 0 */
	object->min_width = 0;
	object->change   &= ~HTML_CHANGE_MIN_WIDTH;
}

HTMLObject *
html_text_slave_new (HTMLText *owner, guint posStart, guint posLen)
{
	HTMLTextSlave *slave;

	slave = g_new (HTMLTextSlave, 1);
	html_text_slave_init (slave, &html_text_slave_class, owner, posStart, posLen);

	return HTML_OBJECT (slave);
}

static gboolean
html_text_slave_is_index_in_glyph (HTMLTextSlave *slave, HTMLTextSlave *next_slave, GSList *cur, gint index, PangoItem *item)
{
	if (item->analysis.level % 2 == 0) {
		/* LTR */
		return item->offset <= index
			&& (index < item->offset + item->length
			    || (index == item->offset + item->length &&
				(!cur->next
				 || (!next_slave && slave->owner->text_bytes == item->offset + item->length)
				 || (next_slave && html_text_slave_get_text (next_slave) - next_slave->owner->text == item->offset + item->length))));
	} else {
		/* RTL */
		return index <= item->offset + item->length
			&& (item->offset < index
			    || (index == item->offset &&
				(!cur->next
				 || (!next_slave && slave->owner->text_bytes == item->offset + item->length)
				 || (next_slave && html_text_slave_get_text (next_slave) - next_slave->owner->text == item->offset))));
	}
}

static HTMLTextSlaveGlyphItem *
html_text_slave_get_glyph_item_at_offset (HTMLTextSlave *slave, HTMLPainter *painter, gint offset, HTMLTextSlaveGlyphItem **prev, HTMLTextSlaveGlyphItem **next, gint *start_width, gint *index_out)
{
	HTMLTextSlaveGlyphItem *rv = NULL;
	HTMLTextSlaveGlyphItem *prev_gi, *next_gi;
	HTMLTextSlave *next_slave = HTML_OBJECT (slave)->next && HTML_IS_TEXT_SLAVE (HTML_OBJECT (slave)->next) ? HTML_TEXT_SLAVE (HTML_OBJECT (slave)->next) : NULL;
	GSList *cur;
	gint index;

	next_gi = NULL;

	index = g_utf8_offset_to_pointer (html_text_slave_get_text (slave), offset) - slave->owner->text;
	if (index_out)
		*index_out = index;

	if (start_width)
		*start_width = 0;

	cur = html_text_slave_get_glyph_items (slave, painter);
	if (cur) {
		for (prev_gi = NULL; cur; cur = cur->next) {
			HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) cur->data;

			if (html_text_slave_is_index_in_glyph (slave, next_slave, cur, index, gi->glyph_item.item)) {
				next_gi = cur->next ? (HTMLTextSlaveGlyphItem *) cur->next->data : NULL;
				rv = gi;
				break;
			}

			prev_gi = gi;
			if (start_width) {
				PangoRectangle log_rect;

				pango_glyph_string_extents (gi->glyph_item.glyphs, gi->glyph_item.item->analysis.font, NULL, &log_rect);
				(*start_width) += log_rect.width;
			}
		}
	} else {
		prev_gi = next_gi = NULL;
	}

	if (prev)
		*prev = prev_gi;

	if (next)
		*next = next_gi;

	return rv;
}

static gboolean
html_text_slave_gi_left_edge (HTMLTextSlave *slave, HTMLCursor *cursor, HTMLTextSlaveGlyphItem *gi)
{
	gint old_offset = cursor->offset;

	if (gi->glyph_item.item->analysis.level % 2 == 0) {
		/* LTR */
		cursor->offset = slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
									     slave->owner->text + gi->glyph_item.item->offset);
		cursor->position += cursor->offset - old_offset;
	} else {
		/* RTL */
		cursor->offset = slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
									     slave->owner->text + gi->glyph_item.item->offset + gi->glyph_item.item->length);
		cursor->position += cursor->offset - old_offset;
	}

	return TRUE;
}

static gboolean
html_text_slave_gi_right_edge (HTMLTextSlave *slave, HTMLCursor *cursor, HTMLTextSlaveGlyphItem *gi)
{
	gint old_offset = cursor->offset;

	if (gi->glyph_item.item->analysis.level % 2 == 0) {
		/* LTR */
		cursor->offset = slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
									     slave->owner->text + gi->glyph_item.item->offset + gi->glyph_item.item->length);
		cursor->position += cursor->offset - old_offset;
	} else {
		/* RTL */
		cursor->offset = slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
									     slave->owner->text + gi->glyph_item.item->offset);
		cursor->position += cursor->offset - old_offset;
	}

	return TRUE;
}

static gboolean
html_text_slave_cursor_right_one (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextSlaveGlyphItem *prev, *next;
	gint index;
	HTMLTextSlaveGlyphItem *gi = html_text_slave_get_glyph_item_at_offset (slave, painter, cursor->offset - slave->posStart, &prev, &next, NULL, &index);

	if (!gi)
		return FALSE;

	if (gi->glyph_item.item->analysis.level % 2 == 0) {
		/* LTR */
		if (index < gi->glyph_item.item->offset + gi->glyph_item.item->length) {
			cursor->offset ++;
			cursor->position ++;

			return TRUE;
		}
	} else {
		/* RTL */
		if (index > gi->glyph_item.item->offset && index <= gi->glyph_item.item->offset + gi->glyph_item.item->length) {
			cursor->offset --;
			cursor->position --;

			return TRUE;
		}
	}

	if (next) {
		if (html_text_slave_gi_left_edge (slave, cursor, next)) {
			if (next->glyph_item.item->analysis.level % 2 == 0) {
				/* LTR */
				cursor->offset ++;
				cursor->position ++;
			} else {
				/* RTL */
				cursor->offset --;
				cursor->position --;
			}

			return TRUE;
		}
	}

	return FALSE;
}

gboolean
html_text_slave_cursor_right (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gboolean step_success;

	do
		step_success = html_text_slave_cursor_right_one (slave, painter, cursor);
	while (step_success && !pi->attrs [cursor->offset].is_cursor_position);

	return step_success;
}

static gboolean
html_text_slave_cursor_left_one (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextSlaveGlyphItem *prev, *next;
	gint index;
	HTMLObject *prev_obj = HTML_OBJECT (slave->owner)->prev;
	HTMLTextSlaveGlyphItem *gi = html_text_slave_get_glyph_item_at_offset (slave, painter, cursor->offset - slave->posStart, &prev, &next, NULL, &index);

/*	printf ("gi: %p item num chars: %d\n", gi, gi ? gi->glyph_item.item->num_chars : -1); */

	if (!gi)
		return FALSE;

	if (gi->glyph_item.item->analysis.level % 2 == 0) {
		/* LTR */
		if (index - gi->glyph_item.item->offset > 1 || (!prev && !prev_obj && index - gi->glyph_item.item->offset > 0)) {
			cursor->offset --;
			cursor->position --;

			return TRUE;
		}
	} else {
		/* RTL */
		if (index < gi->glyph_item.item->offset + gi->glyph_item.item->length) {
			cursor->offset ++;
			cursor->position ++;

			return TRUE;
		}
	}

	if (prev) {
		if (html_text_slave_gi_right_edge (slave, cursor, prev)) {
			if (prev->glyph_item.item->analysis.level % 2 == 0) {
				/* LTR */
				if (index - gi->glyph_item.item->offset == 0) {
					cursor->offset --;
					cursor->position --;
				}
			} else {
				/* RTL */
				cursor->offset ++;
				cursor->position ++;
			}

			return TRUE;
		}
	}

	return FALSE;
}

gboolean
html_text_slave_cursor_left (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gboolean step_success;

	do
		step_success = html_text_slave_cursor_left_one (slave, painter, cursor);
	while (step_success && !pi->attrs [cursor->offset].is_cursor_position);

	return step_success;
}

static gboolean
html_text_slave_get_left_edge (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gint old_offset = cursor->offset;
	gint old_position = cursor->position;

	cursor->offset = html_text_slave_get_left_edge_offset (slave, painter);

	if (pi->attrs [cursor->offset].is_cursor_position && old_offset != cursor->offset)
		return TRUE;
	else {
		if (html_text_slave_cursor_right (slave, painter, cursor)) {
			/* we should preserve position here as caller function correct position themselves */
			cursor->position = old_position;
			return TRUE;
		} else
			return FALSE;
	}
}

static gboolean
html_text_slave_get_right_edge (HTMLTextSlave *slave, HTMLPainter *painter, HTMLCursor *cursor)
{
	HTMLTextPangoInfo *pi = html_text_get_pango_info (slave->owner, painter);
	gint old_offset = cursor->offset;
	gint old_position = cursor->position;

	cursor->offset = html_text_slave_get_right_edge_offset (slave, painter);

	if (pi->attrs [cursor->offset].is_cursor_position && old_offset != cursor->offset)
		return TRUE;
	else {
		if (html_text_slave_cursor_left (slave, painter, cursor)) {
			/* we should preserve position here as caller function correct position themselves */
			cursor->position = old_position;
			return TRUE;
		} else
			return FALSE;
	}
}

gboolean
html_text_slave_cursor_head (HTMLTextSlave *slave, HTMLCursor *cursor, HTMLPainter *painter)
{
	if (html_text_slave_get_glyph_items (slave, painter)) {
		cursor->object = HTML_OBJECT (slave->owner);

		if (html_text_get_pango_direction (slave->owner) != PANGO_DIRECTION_RTL) {
			/* LTR */
			return html_text_slave_get_left_edge (slave, painter, cursor);
		} else {
			/* RTL */
			return html_text_slave_get_right_edge (slave, painter, cursor);
		}
	}

	return FALSE;
}

gboolean
html_text_slave_cursor_tail (HTMLTextSlave *slave, HTMLCursor *cursor, HTMLPainter *painter)
{
	if (html_text_slave_get_glyph_items (slave, painter)) {
		cursor->object = HTML_OBJECT (slave->owner);

		if (html_text_get_pango_direction (slave->owner) != PANGO_DIRECTION_RTL) {
			/* LTR */
			return html_text_slave_get_right_edge (slave, painter, cursor);
		} else {
			/* RTL */
			return html_text_slave_get_left_edge (slave, painter, cursor);
		}
	}

	return FALSE;
}

void
html_text_slave_get_cursor_base (HTMLTextSlave *slave, HTMLPainter *painter, guint offset, gint *x, gint *y)
{
	HTMLTextSlaveGlyphItem *gi;
	gint index, start_width;

	html_object_calc_abs_position (HTML_OBJECT (slave), x, y);

	gi = html_text_slave_get_glyph_item_at_offset (slave, painter, (gint) offset, NULL, NULL, &start_width, &index);

/*	printf ("gi: %p index: %d start_width: %d item indexes %d %d\n", */
/*		gi, index, start_width, gi ? gi->glyph_item.item->offset : -1, */
/*		gi ? gi->glyph_item.item->offset + gi->glyph_item.item->length : -1); */

	if (gi) {
		gint start_x;

		if (calc_glyph_range_size (slave->owner, &gi->glyph_item, index, index, &start_x, NULL, NULL, NULL) && x) {
/*			printf ("start_width: %d start_x: %d\n", start_width, start_x); */
			*x += html_painter_pango_to_engine (painter, start_width + start_x);
		}
	}
}

gint
html_text_slave_get_left_edge_offset (HTMLTextSlave *slave, HTMLPainter *painter)
{
	GSList *gis = html_text_slave_get_glyph_items (slave, painter);

	if (gis) {
		HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) gis->data;

		if (gi->glyph_item.item->analysis.level % 2 == 0) {
			/* LTR */
			return slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave), slave->owner->text + gi->glyph_item.item->offset);
		} else {
			/* RTL */
			return slave->posStart + MIN (slave->posLen, g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
											       slave->owner->text
											       + gi->glyph_item.item->offset + gi->glyph_item.item->length));
		}
	} else {
		if (slave->owner->text_len > 0)
			g_warning ("html_text_slave_get_left_edge_offset failed");

		return 0;
	}
}

gint
html_text_slave_get_right_edge_offset (HTMLTextSlave *slave, HTMLPainter *painter)
{
	GSList *gis = html_text_slave_get_glyph_items (slave, painter);

	if (gis) {
		HTMLTextSlaveGlyphItem *gi = (HTMLTextSlaveGlyphItem *) g_slist_last (gis)->data;

		if (gi->glyph_item.item->analysis.level % 2 == 0) {
			/* LTR */
			return slave->posStart + MIN (slave->posLen, g_utf8_pointer_to_offset (html_text_slave_get_text (slave),
											       slave->owner->text
											       + gi->glyph_item.item->offset + gi->glyph_item.item->length));
		} else {
			/* RTL */
			return slave->posStart + g_utf8_pointer_to_offset (html_text_slave_get_text (slave), slave->owner->text + gi->glyph_item.item->offset);
		}
	} else {
		if (slave->owner->text_len > 0)
			g_warning ("html_text_slave_get_left_edge_offset failed");

		return 0;
	}
}
