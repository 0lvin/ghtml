/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/


#include "htmlfontmanager.h"


/* FIXME this should be dynamically done, and based on the base font name.  */

static guint real_font_sizes[HTML_FONT_STYLE_SIZE_MAX] = {
	8, 8, 10, 12, 14, 18, 19
};


static void
load_font (HTMLFontManager *manager,
	   HTMLFontStyle style)
{
	const gchar *weight_string;
	const gchar *slant_string;
	const gchar *family_string;
	gchar *font_name;
	gint size;
	gint real_size;
	GdkFont *font;

	if (manager->fonts[style] != NULL)
		return;

	size = style & HTML_FONT_STYLE_SIZE_MASK;
	if (size == 0)
		size = 3;

	real_size = real_font_sizes[size];

	if (style & HTML_FONT_STYLE_BOLD)
		weight_string = "bold";
	else
		weight_string = "medium";

	if (style & HTML_FONT_STYLE_ITALIC)
		slant_string = "i";
	else
		slant_string = "r";

	if (style & HTML_FONT_STYLE_FIXED)
		family_string = "courier";
	else
		family_string = "lucida";

	font_name = g_strdup_printf ("-*-%s-%s-%s-normal-*-%d-*-*-*-*-*-*-*",
				    family_string, weight_string, slant_string, real_size);
	font = gdk_font_load (font_name);

	if (font == NULL){
		g_warning ("font `%s' not found", font_name);
		g_free (font_name);
		font_name = g_strdup_printf ("-*-helvetica-medium-r-normal-*-%d-*-*-*-*-*-*-*",
					    real_size);
		font = gdk_font_load (font_name);
	}

	g_free (font_name);

	manager->fonts[style] = font;
}


HTMLFontManager *
html_font_manager_new  (void)
{
	HTMLFontManager *new;

	new = g_new (HTMLFontManager, 1);
	memset (new->fonts, 0, sizeof (new->fonts));

	return new;
}

void
html_font_manager_destroy (HTMLFontManager *manager)
{
	guint i;

	g_return_if_fail (manager != NULL);

	for (i = 0; i < HTML_FONT_STYLE_MAX; i++) {
		if (manager->fonts[i] != NULL)
			gdk_font_unref (manager->fonts[i]);
	}

	g_free (manager);
}

GdkFont *
html_font_manager_get_gdk_font (HTMLFontManager *manager,
				HTMLFontStyle style)
{
	g_return_val_if_fail (manager != NULL, NULL);
	g_return_val_if_fail (style < HTML_FONT_STYLE_MAX, NULL);

	load_font (manager, style);

	return gdk_font_ref (manager->fonts[style]);
}


HTMLFontStyle
html_font_style_merge (HTMLFontStyle a,
		       HTMLFontStyle b)
{
	HTMLFontStyle retval;

	if ((b & HTML_FONT_STYLE_SIZE_MASK) != 0)
		retval = ((b & HTML_FONT_STYLE_SIZE_MASK)
			  | (a & ~HTML_FONT_STYLE_SIZE_MASK));
	else
		retval = a;

	retval |= b & ~HTML_FONT_STYLE_SIZE_MASK;

	return retval;
}
