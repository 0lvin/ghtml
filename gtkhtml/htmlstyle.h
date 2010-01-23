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

#ifndef __HTML_STYLE_H__
#define __HTML_STYLE_H__
#include "gtkhtml-enums.h"
#include "htmlenums.h"
#include "htmltypes.h"
#include "htmlcolor.h"

struct _HTMLLength {
	gint           val;
	HTMLLengthType type;
};

struct _HTMLStyle {
	HTMLColor          *color;
	HTMLFontFace       *face;
	GtkHTMLFontStyle    settings;
	GtkHTMLFontStyle    mask;

	/* Block Level */
	HTMLHAlignType      text_align;
	HTMLClearType       clear;
	HTMLDirection       dir;
	HTMLListType        listtype;
	gint                listnumber;

	/* Cell Level */
	HTMLVAlignType      text_valign;

	/* box settings */
	HTMLLength     *width;
	HTMLLength     *height;

	gchar           *bg_image;
	HTMLColor       *bg_color;
	HTMLDisplayType display;

	/* border */
	gint border_width;
	HTMLBorderStyle border_style;
	HTMLColor *border_color;
	gint padding;
	/*url*/
	gchar *url;     /* for download url */
	gchar *target;  /* for target */
	gchar *href;    /* for click url */
	/*flow*/
	HTMLClueFlowStyle fstyle;
};

HTMLStyle *html_style_new                  (void);
/*copy all fields*/
HTMLStyle *html_style_copy                 (HTMLStyle *style);
/*copy only inherit */
HTMLStyle *html_style_copy_onlyinherit     (HTMLStyle *orig);
HTMLStyle *html_style_unset_decoration     (HTMLStyle *style, GtkHTMLFontStyle decoration);
HTMLStyle *html_style_set_decoration       (HTMLStyle *style, GtkHTMLFontStyle decoration);
HTMLStyle *html_style_set_font_size        (HTMLStyle *style, GtkHTMLFontStyle decoration);
HTMLStyle *html_style_set_size             (HTMLStyle *style, GtkHTMLFontStyle size);
HTMLStyle *html_style_set_display          (HTMLStyle *style, HTMLDisplayType display);
HTMLStyle *html_style_set_clear            (HTMLStyle *style, HTMLClearType clear);
HTMLStyle *html_style_set_border_style     (HTMLStyle *style, HTMLBorderStyle bstyle);
HTMLStyle *html_style_set_border_width     (HTMLStyle *style, gint width);
HTMLStyle *html_style_set_border_color     (HTMLStyle *style, HTMLColor *color);
HTMLStyle *html_style_set_padding          (HTMLStyle *style, gint padding);
HTMLStyle *html_style_add_text_align       (HTMLStyle *style, HTMLHAlignType type);
HTMLStyle *html_style_add_text_valign      (HTMLStyle *style, HTMLVAlignType type);
HTMLStyle *html_style_set_flow_style       (HTMLStyle *style, HTMLClueFlowStyle value);
HTMLStyle *html_style_add_font_face        (HTMLStyle *style, const HTMLFontFace *face);
HTMLStyle *html_style_add_color            (HTMLStyle *style, HTMLColor *face);
HTMLStyle *html_style_add_attribute        (HTMLStyle *style, const gchar *attr, const gchar *value);
HTMLStyle *html_style_add_styleattribute (HTMLStyle *style, const gchar *attr, const gchar *value);
HTMLStyle *html_style_add_background_image (HTMLStyle *style, const gchar *url);
HTMLStyle *html_style_add_dir              (HTMLStyle *style, const gchar *dir_text);
HTMLStyle *html_style_add_background_color (HTMLStyle *style, HTMLColor *color);
HTMLStyle *html_style_add_width            (HTMLStyle *style, const gchar *width);
HTMLStyle *html_style_add_height           (HTMLStyle *style, const gchar *height);
HTMLStyle *html_style_set_list_type        (HTMLStyle *style, HTMLListType value);
HTMLStyle *parse_list_type                 (HTMLStyle *style, const gchar* value);
void       html_style_free                 (HTMLStyle *style);

gboolean   html_parse_color                (const gchar *text, GdkColor *color);
HTMLHAlignType parse_halign                (const gchar *token, HTMLHAlignType default_val);
HTMLVAlignType parse_valign                (const gchar *token, HTMLVAlignType default_val);
#endif /* __HTML_COLOR_H__ */
