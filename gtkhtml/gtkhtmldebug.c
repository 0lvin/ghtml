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

/* Various debugging routines.  */

#include <config.h>
#include "gtkhtml-compat.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "gtkhtml.h"
#include "htmlobject.h"
#include "htmltext.h"
#include "htmltextslave.h"
#include "htmltable.h"
#include "htmltablecell.h"
#include "htmlclue.h"
#include "htmlclueflow.h"
#include "htmlframe.h"
#include "htmlframeset.h"
#include "htmliframe.h"
#include "htmlengine.h"
#include "htmltype.h"
#include "htmlenums.h"
#include "htmlenumutils.h"

#include "gtkhtmldebug.h"


/**
 * gtk_html_debug_log:
 * @html: A GtkHTML widget
 * @format: A format string, in printf() style
 *
 * If @html has debugging turned on, print out the message, just like libc
 * printf().  Otherwise, just do nothing.
 **/
void
gtk_html_debug_log (GtkHTML *html,
		    const gchar *format,
		    ...)
{
	va_list ap;

	if (!html->debug)
		return;

	va_start (ap, format);
	vprintf (format, ap);
}


static const gchar *
clueflow_style_to_string (HTMLClueFlowStyle style)
{
	switch (style) {
	case HTML_CLUEFLOW_STYLE_NORMAL:
		return "Normal";
	case HTML_CLUEFLOW_STYLE_H1:
		return "H1";
	case HTML_CLUEFLOW_STYLE_H2:
		return "H2";
	case HTML_CLUEFLOW_STYLE_H3:
		return "H3";
	case HTML_CLUEFLOW_STYLE_H4:
		return "H4";
	case HTML_CLUEFLOW_STYLE_H5:
		return "H5";
	case HTML_CLUEFLOW_STYLE_H6:
		return "H6";
	case HTML_CLUEFLOW_STYLE_ADDRESS:
		return "Address";
	case HTML_CLUEFLOW_STYLE_PRE:
		return "Pre";
	case HTML_CLUEFLOW_STYLE_LIST_ITEM:
		return "List Item";
	default:
		return "UNKNOWN";
	}
}

static const gchar *
direction_to_string (HTMLDirection dir)
{
	switch (dir) {
	case HTML_DIRECTION_DERIVED:
		return "derived";
	case HTML_DIRECTION_RTL:
		return "rtl";
	case HTML_DIRECTION_LTR:
		return "ltr";
	default:
		return "UNKNOWN";
	}
}


void
gtk_html_debug_dump_table (HTMLObject *o,
			   gint level)
{
	gint c, r;
	HTMLTable *table = HTML_TABLE (o);

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			gtk_html_debug_dump_tree (HTML_OBJECT (table->cells[r][c]), level);
		}
	}

}

static void
gtk_html_debug_dump_table_simple (HTMLObject *o, gint level)
{
	gint c, r;
	HTMLTable *table = HTML_TABLE (o);

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			gtk_html_debug_dump_tree_simple (HTML_OBJECT (table->cells[r][c]), level);
		}
	}

}

void
gtk_html_debug_dump_object (HTMLObject *obj,
			    gint level)
{
	gint i;
	for (i = 0; i < level; i++)
		g_print (" ");

	g_print ("ObjectType: %s Pos: %d, %d, MinWidth: %d, Width: %d PrefWidth: %d MaxWidth: %d Ascent %d Descent %d Direction %s",
		 html_type_name (HTML_OBJECT_TYPE (obj)),
		 obj->x, obj->y, obj->min_width, obj->width, obj->pref_width, obj->max_width, obj->ascent, obj->descent,
		 direction_to_string (html_object_get_direction (obj)));

	if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_CLUEFLOW) {
		g_print (" [%s, %d]",
			 clueflow_style_to_string (HTML_CLUEFLOW (obj)->style), HTML_CLUEFLOW (obj)->levels->len);
		g_print (" levels: [");
		for (i = 0; i < HTML_CLUEFLOW (obj)->levels->len; i ++) {
			g_print ("%d", HTML_CLUEFLOW (obj)->levels->data [i]);
			if (i < HTML_CLUEFLOW (obj)->levels->len - 1)
				g_print (" ");
		}
		g_print ("]");
	}
	else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TEXTSLAVE) {
		GSList *cur;
		gint width;
		gint ii, io, tw, offset;
		gchar *sl_text = g_strndup (html_text_get_text (HTML_TEXT (HTML_TEXT_SLAVE (obj)->owner),
								HTML_TEXT_SLAVE (obj)->posStart),
					    html_text_get_index (HTML_TEXT (HTML_TEXT_SLAVE (obj)->owner),
								 HTML_TEXT_SLAVE (obj)->posStart
								 + HTML_TEXT_SLAVE (obj)->posLen)
					    - html_text_get_index (HTML_TEXT (HTML_TEXT_SLAVE (obj)->owner),
								   HTML_TEXT_SLAVE (obj)->posStart));
		g_print ("[start %d end %d len %d] \"%s\"\n",
			 HTML_TEXT_SLAVE (obj)->posStart,
			 HTML_TEXT_SLAVE (obj)->posStart + HTML_TEXT_SLAVE (obj)->posLen - 1,
			 HTML_TEXT_SLAVE (obj)->posLen,
			 sl_text);
		g_free (sl_text);

		width = 0;
		if (HTML_TEXT_SLAVE (obj)->glyph_items)
			for (cur = HTML_TEXT_SLAVE (obj)->glyph_items; cur; cur = cur->next) {
				HTMLTextSlaveGlyphItem *sgi = (HTMLTextSlaveGlyphItem *) cur->data;
				PangoItem *item = sgi->glyph_item.item;
				PangoRectangle log_rect;

				pango_glyph_string_extents (sgi->glyph_item.glyphs, sgi->glyph_item.item->analysis.font, NULL, &log_rect);
				g_print ("type: %d item level: %d offset: %d length: %d width: %d chars %d glyphs %d\n", sgi->type, item->analysis.level, item->offset, item->length, log_rect.width, item->num_chars, sgi->glyph_item.glyphs->num_glyphs);
				width += log_rect.width;

			}

		g_print ("total width by glyph string extents (in engine units): %d\n", (gint)(0.5 + width / PANGO_SCALE));

		ii = html_text_get_item_index (HTML_TEXT_SLAVE (obj)->owner, NULL, HTML_TEXT_SLAVE (obj)->posStart, &io);
		tw = 0;
		offset = 0;
		while (offset < HTML_TEXT_SLAVE (obj)->posLen) {
			tw += HTML_TEXT_SLAVE (obj)->owner->pi->entries [ii].widths [io];
			g_print ("%d ", HTML_TEXT_SLAVE (obj)->owner->pi->entries [ii].widths [io]);
			html_text_pi_forward (HTML_TEXT_SLAVE (obj)->owner->pi, &ii, &io);
			offset ++;
		}

		g_print ("\ntotal width by widths in entries (in engine units): %d", (gint)(0.5 + tw / PANGO_SCALE));
	}

	g_print ("\n");

	switch (HTML_OBJECT_TYPE (obj)) {
	case HTML_TYPE_TABLE:
		gtk_html_debug_dump_table (obj, level + 1);
		break;
	case HTML_TYPE_TEXT:
	case HTML_TYPE_LINKTEXT: {
		HTMLText *text = HTML_TEXT (obj);
		for (i = 0; i < level; i++)
			g_print (" ");
		g_print ("Text (len %d bytes %d): \"%s\"\n",
			 text->text_len, text->text_bytes, text->text);
/*		debug_spell_errors (text->spell_errors); */
		if (text->pi) {
			for (i =0; i < text->pi->n; i ++)
				g_print ("item %d offset: %d length: %d\n", i, text->pi->entries [i].glyph_item.item->offset, text->pi->entries [i].glyph_item.item->length);

			for (i = 0; i < text->text_len; i ++) {
				union {
					PangoLogAttr attr;
					guint as_int;
				} u;
				u.attr = text->pi->attrs [i];
				g_print ("log attrs[%d]: %d\n\t", i, u.as_int & 0x7ff);
				if (u.attr.is_line_break)
					g_print ("line break, ");
				if (u.attr.is_mandatory_break)
					g_print ("mandatory break, ");
				if (u.attr.is_char_break)
					g_print ("char break, ");
				if (u.attr.is_white)
					g_print ("white, ");
				if (u.attr.is_cursor_position)
					g_print ("cursor position, ");
				if (u.attr.is_word_start)
					g_print ("word start, ");
				if (u.attr.is_word_end)
					g_print ("word end, ");
				if (u.attr.is_sentence_boundary)
					g_print ("sentence boundary, ");
				if (u.attr.is_sentence_start)
					g_print ("sentence start, ");
				if (u.attr.is_sentence_end)
					g_print ("sentence end, ");
				g_print ("\n");
			}
		}
		gtk_html_debug_list_links (text);
		gtk_html_debug_list_text_attrs (text);
		break;
	}
	case HTML_TYPE_CLUEV:
		g_print ("Direction: %s\n", direction_to_string (HTML_CLUEV (obj)->dir));
	case HTML_TYPE_CLUEH:
	case HTML_TYPE_CLUEFLOW:
		/* g_print ("Head: %p Tail: %p\n", HTML_CLUE (obj)->head, HTML_CLUE (obj)->tail); */
	case HTML_TYPE_CLUEALIGNED:
	case HTML_TYPE_TABLECELL:
		for (i = 0; i < level; i++) g_print (" ");
		g_print ("HAlign: %s VAlign: %s\n",
			 html_halign_name (HTML_CLUE (obj)->halign),
			 html_valign_name (HTML_CLUE (obj)->valign));
		gtk_html_debug_dump_tree (HTML_CLUE (obj)->head, level + 1);
		break;
	case HTML_TYPE_IFRAME:
		gtk_html_debug_dump_tree (GTK_HTML (HTML_IFRAME (obj)->html)->engine->clue, level + 1);
		break;
	case HTML_TYPE_FRAME:
		gtk_html_debug_dump_tree (GTK_HTML (HTML_FRAME (obj)->html)->engine->clue, level + 1);
		break;
	case HTML_TYPE_IMAGE:
		for (i = 0; i < level; i++) g_print (" ");
		g_print ("Location: %s\n", HTML_IMAGE (obj)->image_ptr->url);
		break;
	case HTML_TYPE_FRAMESET: {
		for (i = 0; i < HTML_FRAMESET (obj)->frames->len; i++)
			gtk_html_debug_dump_tree (g_ptr_array_index (HTML_FRAMESET (obj)->frames, i), level + 1);
		}
		break;
	default:
		break;
	}
}

void
gtk_html_debug_dump_tree (HTMLObject *o,
			  gint level)
{
	HTMLObject *obj;

	obj = o;
	while (obj) {
		gtk_html_debug_dump_object (obj, level);
		obj = obj->next;
	}
}

static void
dump_data (GQuark key_id, gpointer data, gpointer user_data)
{
	gint i, level = GPOINTER_TO_INT (user_data);

	for (i = 0; i < level; i++)
		g_print ("\t");

	printf ("%s: '%s'\n", g_quark_to_string (key_id), (gchar *) data);
}

static void
dump_object_simple (HTMLObject *obj,
		    gint level)
{
	gint i;

	for (i = 0; i < level; i++)
		g_print ("\t");

	if (html_object_is_text (obj)) {
		HTMLText *text = HTML_TEXT (obj);
		g_print ("%s `%s'\n",
			 html_type_name (HTML_OBJECT_TYPE (obj)),
			 text->text);
/*		g_print ("len %d bytes %d\n", text->text_len, text->text_bytes); */
	} else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TEXTSLAVE) {
		HTMLTextSlave *slave = HTML_TEXT_SLAVE (obj);
		gchar *text;

		text = alloca (slave->posLen+1);
		text [slave->posLen] = 0;
		strncpy (text, slave->owner->text + slave->posStart, slave->posLen);
		g_print ("%s `%s'\n",
			 html_type_name (HTML_OBJECT_TYPE (obj)),
			 text);
	} else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TABLECELL) {
		g_print ("%s %d,%d\n", html_type_name (HTML_OBJECT_TYPE (obj)),
			 HTML_TABLE_CELL (obj)->row, HTML_TABLE_CELL (obj)->col);
	} else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TABLE) {
		g_print ("%s %d,%d\n", html_type_name (HTML_OBJECT_TYPE (obj)),
			 HTML_TABLE (obj)->totalRows, HTML_TABLE (obj)->totalCols);
	} else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_IFRAME) {
		g_print ("%s\n", html_type_name (HTML_OBJECT_TYPE (obj)));
		gtk_html_debug_dump_tree_simple (GTK_HTML (HTML_IFRAME (obj)->html)->engine->clue, level + 1);
	} else if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_FRAME) {
		g_print ("%s\n", html_type_name (HTML_OBJECT_TYPE (obj)));
		gtk_html_debug_dump_tree_simple (GTK_HTML (HTML_FRAME (obj)->html)->engine->clue, level + 1);
	} else
		g_print ("%s\n", html_type_name (HTML_OBJECT_TYPE (obj)));

	if (obj->object_data)
		g_datalist_foreach (&obj->object_data, dump_data, GINT_TO_POINTER (level));
}

void
gtk_html_debug_dump_object_type (HTMLObject *o)
{
	dump_object_simple (o, 0);
}

void
gtk_html_debug_dump_tree_simple (HTMLObject *o,
				 gint level)
{
	HTMLObject *obj;

	for (obj = o; obj != NULL; obj = obj->next) {
		if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TEXTSLAVE)
			continue;

		dump_object_simple (obj, level);

		switch (HTML_OBJECT_TYPE (obj)) {
		case HTML_TYPE_CLUEH:
		case HTML_TYPE_CLUEV:
		case HTML_TYPE_CLUEFLOW:
		case HTML_TYPE_CLUEALIGNED:
		case HTML_TYPE_TABLECELL:
			gtk_html_debug_dump_tree_simple (HTML_CLUE (obj)->head, level + 1);
			break;
		case HTML_TYPE_TABLE:
			gtk_html_debug_dump_table_simple (obj, level + 1);
			break;
		default:
			break;
		}
	}
}

void
gtk_html_debug_dump_list_simple (GList *list,
				 gint level)
{
	HTMLObject *obj;
	GList *p;

	for (p = list; p != NULL; p = p->next) {
		obj = HTML_OBJECT (p->data);

		if (HTML_OBJECT_TYPE (obj) == HTML_TYPE_TEXTSLAVE)
			continue;

		dump_object_simple (obj, level);
	}
}

#define D_ATTR_TYPE(x, s) if ((attr = pango_attr_iterator_get (iter, PANGO_ATTR_ ## x))) g_print ("%3d-%3d: %s\n", attr->start_index, attr->end_index, s);
#define D_ATTR_TYPE_INT(x, s) if ((attr = pango_attr_iterator_get (iter, PANGO_ATTR_ ## x))) { g_print ("%3d-%3d: %s %d\n", attr->start_index, attr->end_index, s, ((PangoAttrInt *)attr)->value); }

static void
gtk_html_debug_list_attrs (PangoAttrList *attrs)
{
	PangoAttrIterator *iter = pango_attr_list_get_iterator (attrs);
	PangoAttribute *attr;

	do {
		D_ATTR_TYPE (INVALID, "Invalid");
		D_ATTR_TYPE (LANGUAGE, "Language");
		D_ATTR_TYPE (FAMILY, "Family");
		D_ATTR_TYPE (STYLE, "Style");
		D_ATTR_TYPE (WEIGHT, "Weight");
		D_ATTR_TYPE (VARIANT, "Variant");
		D_ATTR_TYPE (STRETCH, "Stretch");
		D_ATTR_TYPE_INT (SIZE, "Size");
		D_ATTR_TYPE (FONT_DESC, "Font Desc");
		D_ATTR_TYPE (FOREGROUND, "Foreground");
		D_ATTR_TYPE (BACKGROUND, "Background");
		D_ATTR_TYPE (UNDERLINE, "Underline");
		D_ATTR_TYPE (STRIKETHROUGH, "Strikethrough");
		D_ATTR_TYPE (RISE, "Rise");
		D_ATTR_TYPE (SHAPE, "Shape");
		D_ATTR_TYPE (SCALE, "Scale");
		g_print ("------------\n");
	} while (pango_attr_iterator_next (iter));

	pango_attr_iterator_destroy (iter);
}

void
gtk_html_debug_list_text_attrs (HTMLText *text)
{
	gtk_html_debug_list_attrs (text->attr_list);
}

void
gtk_html_debug_list_links (HTMLText *text)
{
	GSList *l;

	for (l = text->links; l; l = l->next)
		if (l->data) {
			Link *link = (Link *) l->data;

			g_print ("%d-%d(%d-%d): %s#%s\n", link->start_offset, link->end_offset, link->start_index, link->end_index, link->url, link->target);
		}
}
