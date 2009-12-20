/* "a -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright (C) 2002, Ximian Inc.

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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "htmlstyle.h"

HTMLLength * parse_length (const gchar *str);
HTMLLength * html_length_copy(HTMLLength * orig);
HTMLStyle  * parse_border_style (HTMLStyle *style,const gchar *value);
HTMLStyle  * parse_border_color (HTMLStyle *style,const gchar *value);
HTMLStyle  * parse_border (HTMLStyle *style,const gchar *origvalue);
HTMLStyle  * parse_border_width (HTMLStyle *style,const gchar *value);

/* Color handling.  */
gboolean
html_parse_color (const gchar *text,
		  GdkColor *color)
{
	gchar c [8];
	gint  len = strlen (text);

	if (gdk_color_parse (text, color))
		return TRUE;

	c [7] = 0;
	if (*text != '#') {
		c[0] = '#';
		strncpy (c + 1, text, 6);
		len++;
	} else {
		strncpy (c, text, MIN (7, len));
	}

	if (len < 7)
		memset (c + len, '\0', 7-len);

	return gdk_color_parse (c, color);
}

HTMLLength *
html_length_copy(HTMLLength * orig)
{
	HTMLLength *len;
	if(!orig)
		return NULL;

	len = g_new0 (HTMLLength, 1);
	len->type = orig->type;
	len->val = orig->val;
	return len;
}

HTMLLength *
parse_length (const gchar *str) {
	const gchar *cur = str;
	HTMLLength *len;

	len = g_new0 (HTMLLength, 1);

	if (!str)
		return len;

	/* g_warning ("begin \"%s\"", *str); */

	while (isspace (*cur)) cur++;

	len->val = atoi (cur);
	len->type = HTML_LENGTH_TYPE_PIXELS;

	while (isdigit (*cur) || *cur == '-') cur++;

	switch (*cur) {
	case '*':
		if (len->val == 0)
			len->val = 1;
		len->type = HTML_LENGTH_TYPE_FRACTION;
		cur++;
		break;
	case '%':
		len->type = HTML_LENGTH_TYPE_PERCENT;
                cur++;
		break;
	}

	if (cur <= str) {
		g_free (len);
		return NULL;
	}

	/* g_warning ("length len->val=%d, len->type=%d", len->val, len->type); */

	return len;
}

HTMLStyle *
html_style_new (void)
{
	HTMLStyle *style = g_new0 (HTMLStyle, 1);

	style->display = HTMLDISPLAY_NONE;

	style->color = NULL;
	style->mask = 0;
	style->settings = 0;

	/* BLOCK */
	style->text_align = HTML_HALIGN_NONE;
	style->clear = HTML_CLEAR_NONE;
	style->listtype = HTML_LIST_TYPE_BLOCKQUOTE;

	style->text_valign = HTML_VALIGN_NONE;
	style->dir = HTML_DIRECTION_DERIVED;
	style->listnumber = 0;
	return style;
}

HTMLStyle *
html_style_copy(HTMLStyle *orig)
{
	HTMLStyle *style;

	if (!orig)
		return NULL;
	style = g_new0 (HTMLStyle, 1);
	style->color = html_color_copy(orig->color);

	if (orig->face)
		style->face = g_strdup(orig->face);

	style->settings = orig->settings;
	style->mask = orig->mask;

	/* Block Level */
	style->text_align = orig->text_align;
	style->clear = orig->clear;
	style->dir = orig->dir;
	style->listtype = orig->listtype;

	/* Cell Level */
	style->text_valign = orig->text_valign;

	/* box settings */
	orig->width = html_length_copy(orig->width);
	orig->height = html_length_copy(orig->height);

	if(orig->bg_image)
		style->bg_image = g_strdup(orig->bg_image);

	style->bg_color = html_color_copy(orig->bg_color);
	style->display = orig->display;
	style->listnumber = 0;
	style->listtype = HTML_LIST_TYPE_BLOCKQUOTE;

	/* border */
	style->border_width = orig->border_width;
	style->border_style = orig->border_style;
	style->border_color = html_color_copy(orig->border_color);
	style->padding = orig->padding;
	/*url*/
	if (orig->url)
		style->url = g_strdup(orig->url);
	if (orig->target)
		style->url = g_strdup(orig->target);
	if (orig->href)
		style->href = g_strdup(orig->href);
	/*flow*/
	style->fstyle = orig->fstyle;
	return style;
}

HTMLStyle *
html_style_copy_onlyinherit(HTMLStyle *orig)
{
	HTMLStyle *style;
	if (!orig)
		return NULL;

	style = html_style_copy(orig);

	style->border_width = 0;
	style->border_style = HTML_BORDER_NONE;
	if (style->border_color)
		html_color_unref (style->border_color);
	style->border_color = NULL;
	style->padding = 0;
	return style;
}

void
html_style_free (HTMLStyle *style)
{
	if (!style)
		return;

	g_free (style->face);
	g_free (style->bg_image);
	g_free (style->width);
	g_free (style->height);

	if (style->color)
		html_color_unref (style->color);

	if (style->bg_color)
		html_color_unref (style->bg_color);

	if (style->border_color)
		html_color_unref (style->border_color);
	g_free(style->url);
	g_free(style->target);
	g_free (style);
}

HTMLStyle *
html_style_set_flow_style (HTMLStyle *style, HTMLClueFlowStyle value)
{
	if (!style)
		style = html_style_new ();
	style->fstyle = value;
	return style;
}

HTMLStyle *
html_style_set_list_type (HTMLStyle *style, HTMLListType value)
{
	if (!style)
		style = html_style_new ();
	style->listtype = value;
	return style;
}

HTMLStyle *
parse_list_type (HTMLStyle *style, const gchar* value)
{
	if (!value)
		return style;
	else if (*value =='i')
		style = html_style_set_list_type (style, HTML_LIST_TYPE_ORDERED_LOWER_ROMAN);
	else if (*value =='I')
		style = html_style_set_list_type (style, HTML_LIST_TYPE_ORDERED_UPPER_ROMAN);
	else if (*value =='a')
		style = html_style_set_list_type (style, HTML_LIST_TYPE_ORDERED_LOWER_ALPHA);
	else if (*value =='A')
		style = html_style_set_list_type (style, HTML_LIST_TYPE_ORDERED_UPPER_ALPHA);
	else if (*value =='1')
		style = html_style_set_list_type (style, HTML_LIST_TYPE_ORDERED_ARABIC);
	else if (!g_ascii_strcasecmp (value, "circle"))
		style = html_style_set_list_type (style, HTML_LIST_TYPE_CIRCLE);
	else if (!g_ascii_strcasecmp (value, "disc"))
		style = html_style_set_list_type (style, HTML_LIST_TYPE_DISC);
	else if (!g_ascii_strcasecmp (value, "square"))
		style = html_style_set_list_type (style, HTML_LIST_TYPE_SQUARE);
	return style;
}

HTMLStyle *
html_style_add_color (HTMLStyle *style, HTMLColor *color)
{
	HTMLColor *old;

	if (!style)
		style = html_style_new ();

	old = style->color;

	style->color = color;

	if (color)
		html_color_ref (color);

	if (old)
		html_color_unref (old);

	return style;
}

HTMLStyle *
html_style_add_dir (HTMLStyle *style, const gchar* dir_text)
{
	if (dir_text) {

		if (!style)
			style = html_style_new ();

		if (!g_ascii_strncasecmp (dir_text, "ltr", 3))
			style->dir = HTML_DIRECTION_LTR;
		else if (!g_ascii_strncasecmp (dir_text, "rtl", 3))
			style->dir = HTML_DIRECTION_RTL;
	}

	return style;
}

HTMLHAlignType
parse_halign (const gchar *token, HTMLHAlignType default_val)
{
	if (g_ascii_strcasecmp (token, "right") == 0)
		return HTML_HALIGN_RIGHT;
	else if (g_ascii_strcasecmp (token, "left") == 0)
		return HTML_HALIGN_LEFT;
	else if (g_ascii_strcasecmp (token, "center") == 0 || g_ascii_strcasecmp (token, "middle") == 0)
		return HTML_HALIGN_CENTER;
	else
		return default_val;
}

HTMLVAlignType
parse_valign (const gchar *token, HTMLVAlignType default_val)
{
	if (g_ascii_strcasecmp (token, "top") == 0)
		return HTML_VALIGN_TOP;
	else if (g_ascii_strcasecmp (token, "bottom") == 0)
		return HTML_VALIGN_BOTTOM;
	else if (g_ascii_strcasecmp (token, "middle") == 0)
		return HTML_VALIGN_MIDDLE;
	else
		return default_val;
}

HTMLStyle *
html_style_unset_decoration (HTMLStyle *style, GtkHTMLFontStyle font_style)
{
	if (!style)
		style = html_style_new ();

	font_style &= ~GTK_HTML_FONT_STYLE_SIZE_MASK;
	style->mask |= font_style;
	style->settings &= ~font_style;

	return style;
}

HTMLStyle *
html_style_set_decoration (HTMLStyle *style, GtkHTMLFontStyle font_style)
{
	if (!style)
		style = html_style_new ();

	font_style &= ~GTK_HTML_FONT_STYLE_SIZE_MASK;
	style->mask |= font_style;
	style->settings |= font_style;

	return style;
}

HTMLStyle *
html_style_set_font_size (HTMLStyle *style, GtkHTMLFontStyle font_style)
{
	if (!style)
		style = html_style_new ();

	font_style &= GTK_HTML_FONT_STYLE_SIZE_MASK;
	style->mask |= GTK_HTML_FONT_STYLE_SIZE_MASK;
	style->settings |= font_style;

	return style;
}

HTMLStyle *
html_style_add_font_face (HTMLStyle *style, const HTMLFontFace *face)
{
	if (!style)
		style = html_style_new ();

	g_free (style->face);
	style->face = g_strdup (face);

	return style;
}

HTMLStyle *
html_style_add_text_align (HTMLStyle *style, HTMLHAlignType type)
{
	if (!style)
		style = html_style_new ();

	style->text_align = type;

	return style;
}

HTMLStyle *
html_style_add_text_valign (HTMLStyle *style, HTMLVAlignType type)
{
	if (!style)
		style = html_style_new ();

	style->text_valign = type;

	return style;
}

HTMLStyle *
html_style_add_background_color (HTMLStyle *style, HTMLColor *color)
{
	HTMLColor *old;

	if (!style)
		style = html_style_new ();

	old = style->bg_color;

	style->bg_color = color;

	if (color)
		html_color_ref (color);

	if (old)
		html_color_unref (old);

	return style;
}

HTMLStyle *
html_style_set_display (HTMLStyle *style, HTMLDisplayType display)
{
	if (!style)
		style = html_style_new ();

	style->display = display;

	return style;
}

HTMLStyle *
html_style_set_clear (HTMLStyle *style, HTMLClearType clear)
{
	if (!style)
		style = html_style_new ();

	style->clear = clear;

	return style;
}

HTMLStyle *
html_style_add_width (HTMLStyle *style,const gchar *len)
{
	if (!style)
		style = html_style_new ();

	g_free (style->width);

	style->width = parse_length (len);

	return style;
}

HTMLStyle *
html_style_add_height (HTMLStyle *style,const gchar *len)
{
	if (!style)
		style = html_style_new ();

	g_free (style->height);

	style->height = parse_length (len);

	return style;
}

HTMLStyle *
html_style_add_background_image (HTMLStyle *style, const gchar *url)
{
	if (!style)
		style = html_style_new ();

	g_free (style->bg_image);
	style->bg_image = g_strdup (url);

	return style;
}

HTMLStyle *
html_style_set_border_style (HTMLStyle *style, HTMLBorderStyle bstyle)
{
	if (!style)
		style = html_style_new ();

	style->border_style = bstyle;

	return style;
}

HTMLStyle *
html_style_set_border_width (HTMLStyle *style, gint width)
{
	if (!style)
		style = html_style_new ();

	style->border_width = width;

	return style;
}

HTMLStyle *
html_style_set_padding (HTMLStyle *style, gint padding)
{
	if (!style)
		style = html_style_new ();

	style->padding = padding;

	return style;
}

HTMLStyle *
html_style_set_border_color (HTMLStyle *style, HTMLColor *color)
{
	HTMLColor *old;

	if (!style)
		style = html_style_new ();

	old = style->border_color;

	style->border_color = color;

	if (color)
		html_color_ref (color);

	if (old)
		html_color_unref (old);

	return style;
}

HTMLStyle *
parse_border_style (HTMLStyle *style,const gchar *value)
{

	while (isspace (*value))
		value ++;

	if (!g_ascii_strcasecmp (value, "solid"))
		style = html_style_set_border_style (style, HTML_BORDER_SOLID);
	else if (!g_ascii_strcasecmp (value, "inset"))
		style = html_style_set_border_style (style, HTML_BORDER_INSET);

	return style;
}

HTMLStyle *
parse_border_color (HTMLStyle *style,const gchar *value)
{
	GdkColor color;

	if (html_parse_color (value, &color)) {
		HTMLColor *hc = html_color_new_from_gdk_color (&color);
		style = html_style_set_border_color (style, hc);
		html_color_unref (hc);
	}

	return style;
}

HTMLStyle *
parse_border_width (HTMLStyle *style,const gchar *value)
{
	while (isspace (*value))
		value ++;

	if (!g_ascii_strcasecmp (value, "thin"))
		style = html_style_set_border_width (style, 1);
	else if (!g_ascii_strcasecmp (value, "medium"))
		style = html_style_set_border_width (style, 2);
	else if (!g_ascii_strcasecmp (value, "thick"))
		style = html_style_set_border_width (style, 5);
	else if (isdigit (*value))
		style = html_style_set_border_width (style, atoi (value));

	return style;
}

HTMLStyle *
parse_border (HTMLStyle *style,const gchar *origvalue)
{
	if (origvalue && *origvalue) {
		gchar * value = g_strdup(origvalue);
		gchar * tmpvalue = value;
		while (value && *value) {
			gchar *next;
			gint modified;
			gchar orig = 0;

			while (isspace (*value))
				value ++;

			next = value;
			while (*next && !isspace (*next))
				next ++;
			if (*next) {
				orig = *next;
				*next = 0;
				modified = 1;
			} else
				modified = 0;

			style = parse_border_style (style, value);
			style = parse_border_color (style, value);
			style = parse_border_width (style, value);

			if (modified) {
				*next = orig;
				next ++;
			}

			value = next;
		}
		g_free(tmpvalue);
	}
	return style;
}

HTMLStyle *
html_style_add_attribute (HTMLStyle *style, const gchar *attr, const gchar *value)
{
	if (!attr)
		return style;
	if (!value)
		return style;

	if (!g_ascii_strcasecmp ("align", attr)) {
		style = html_style_add_text_align (style, parse_halign (value, HTML_VALIGN_NONE));
	} else if (!g_ascii_strcasecmp ("valign", attr)) {
		style = html_style_add_text_valign (style, parse_valign (value, HTML_VALIGN_MIDDLE));
	} else if (!g_ascii_strcasecmp ("width", attr) ||
		   !g_ascii_strcasecmp ("length", attr) ) { //hr
		style = html_style_add_width (style, value);
	} else if (!g_ascii_strcasecmp ("height", attr) ||
		   !g_ascii_strcasecmp ("size", attr)) { //hr,font,select
		style = html_style_add_height (style, value);
	} else if (!g_ascii_strcasecmp ("color", attr)) {
		GdkColor color;

		if (html_parse_color (value, &color)) {
			HTMLColor *hc = html_color_new_from_gdk_color (&color);
			style = html_style_add_color (style, hc);
			html_color_unref (hc);

		}
	} else if (!g_ascii_strcasecmp ("face", attr)) {
		style = html_style_add_font_face (style, value);
	} else if (!g_ascii_strcasecmp ("background-color", attr) ||
		   !g_ascii_strcasecmp ("background", attr) ||
		   !g_ascii_strcasecmp ("bgcolor", attr) ) {
		GdkColor color;

		if (html_parse_color (value, &color)) {
			HTMLColor *hc = html_color_new_from_gdk_color (&color);
			style = html_style_add_background_color (style, hc);
			html_color_unref (hc);
		}
	} else if (!g_ascii_strcasecmp ("background-image", attr)) {
		style = html_style_add_background_image (style, value);
	} else if (!g_ascii_strcasecmp ("border", attr)) {
		style = parse_border (style, value);
	} else if (!g_ascii_strcasecmp ("border-style", attr)) {
		style = parse_border_style (style, value);
	} else if (!g_ascii_strcasecmp ("dir", attr)) {
		style = html_style_add_dir (style, value);
	} else if (!g_ascii_strcasecmp ("border-color", attr)) {
		style = parse_border_color (style, value);
	} else if (!g_ascii_strcasecmp ("border-width", attr)) {
		style = parse_border_width (style, value);
	} else if (!g_ascii_strcasecmp ("padding", attr)) {
		style = html_style_set_padding (style, atoi (value));
	} else if (!g_ascii_strcasecmp ("white-space", attr)) {
		/* normal, pre, nowrap, pre-wrap, pre-line, inherit  */
		/*
		if (!g_ascii_strcasecmp ("normal", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_NORMAL);
		} else if (!g_ascii_strcasecmp ("pre", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_PRE);
		} else if (!g_ascii_strcasecmp ("nowrap", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_NOWRAP);
		} else if (!g_ascii_strcasecmp ("pre-wrap", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_PRE_WRAP);
		} else if (!g_ascii_strcasecmp ("pre-line", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_PRE_LINE);
		} else if (!g_ascii_strcasecmp ("inherit", value)) {
			style = html_style_set_white_space (style, HTML_WHITE_SPACE_INHERIT);
		}*/
	} else if (!g_ascii_strcasecmp ("attr-decoration", attr)) {
		if (!g_ascii_strcasecmp ("none", value)) {
			style = html_style_unset_decoration (style, ~GTK_HTML_FONT_STYLE_SIZE_MASK);
		}
	} else if (!g_ascii_strcasecmp ("display", attr)) {
		if (!g_ascii_strcasecmp ("block", value)) {
			style = html_style_set_display (style, HTMLDISPLAY_BLOCK);
		} else if (!g_ascii_strcasecmp ("inline", value)) {
			style = html_style_set_display (style, HTMLDISPLAY_INLINE);
		} else if (!g_ascii_strcasecmp ("none", value)) {
			style = html_style_set_display (style, HTMLDISPLAY_NONE);
		} else if (!g_ascii_strcasecmp ("inline-table", value)) {
			style = html_style_set_display (style, HTMLDISPLAY_INLINE_TABLE);
		}
	} else if (!g_ascii_strcasecmp ("attr-align", attr)) {
		if (!g_ascii_strcasecmp ("center", value)) {
			style = html_style_add_text_align (style, HTML_HALIGN_CENTER);
		}
	} else if (!g_ascii_strcasecmp ("width", attr)) {
		style = html_style_add_width (style, value);
	} else if (!g_ascii_strcasecmp ("height", attr)) {
		style = html_style_add_height (style, value);
	} else if (!g_ascii_strcasecmp ("clear", attr)) {
		if (!g_ascii_strcasecmp ("left", value)) {
			style = html_style_set_clear (style, HTML_CLEAR_LEFT);
		} else if (!g_ascii_strcasecmp ("right", value)) {
			style = html_style_set_clear (style, HTML_CLEAR_RIGHT);
		} else if (!g_ascii_strcasecmp ("both", value)) {
			style = html_style_set_clear (style, HTML_CLEAR_ALL);
		} else if (!g_ascii_strcasecmp ("inherit", value)) {
			style = html_style_set_clear (style, HTML_CLEAR_INHERIT);
		} else if (!g_ascii_strcasecmp ("none", value)) {
			style = html_style_set_clear (style, HTML_CLEAR_NONE);
		}
	} else if (!g_ascii_strcasecmp ("href", attr)) { /*a*/
		if (!style)
			style = html_style_new ();
		if (style->href)
			g_free (style->href);
		style->href = g_strdup(value);
	} else if (!g_ascii_strcasecmp ("src", attr)) { /*iframe, frame, img*/
		if (!style)
			style = html_style_new ();
		if (style->url)
			g_free (style->url);
		style->url = g_strdup(value);
	} else if (!g_ascii_strcasecmp ("target", attr)) {
		if (!style)
			style = html_style_new ();
		if (style->target)
			g_free (style->target);
		style->target = g_strdup(value);
	} else if ( !g_ascii_strcasecmp ("type", attr) ||
				!g_ascii_strcasecmp ("list-style-type", attr)) { /*ol li*/
		style = parse_list_type (style, value);
	}
	return style;
}
