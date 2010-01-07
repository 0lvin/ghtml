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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include "gtkhtml-compat.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n-lib.h>

#include "htmlentity.h"
#include "htmlgdkpainter.h"
#include "htmlcolor.h"
#include "htmlcolorset.h"
#include "htmlembedded.h"
#include "htmlengine.h"
#include "htmltextslave.h"
#include "htmlsettings.h"
#include "gtkhtml-embedded.h"
#include "gtkhtml.h"

static HTMLPainterClass *parent_class = NULL;

static void set_clip_rectangle (HTMLPainter *painter, gint x, gint y, gint width, gint height);

/* GObject methods.  */

static void
finalize (GObject *object)
{
	HTMLGdkPainter *painter;

	painter = HTML_GDK_PAINTER (object);

	if (painter->gc != NULL) {
		g_object_unref (painter->gc);
		painter->gc = NULL;
	}

	if (painter->pixmap != NULL) {
		g_object_unref (painter->pixmap);
		painter->pixmap = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
alloc_color (HTMLPainter *painter,
	     GdkColor *color)
{
	HTMLGdkPainter *gdk_painter;
	GdkColormap *colormap;

	gdk_painter = HTML_GDK_PAINTER (painter);
	g_return_if_fail (gdk_painter->window != NULL);

	colormap = gdk_drawable_get_colormap (gdk_painter->window);
	gdk_rgb_find_color (colormap, color);
}

static void
free_color (HTMLPainter *painter,
	    GdkColor *color)
{
}


static void
begin (HTMLPainter *painter, gint x1, gint y1, gint x2, gint y2)
{
	HTMLGdkPainter *gdk_painter;

	/* printf ("painter begin %d,%d %d,%d\n", x1, y1, x2, y2); */

	gdk_painter = HTML_GDK_PAINTER (painter);
	g_return_if_fail (gdk_painter->window != NULL);

	set_clip_rectangle (painter, 0, 0, 0, 0);

	if (gdk_painter->double_buffer) {
		const gint width = x2 - x1 + 1;
		const gint height = y2 - y1 + 1;

		/* FIXME: Ideally it should be NULL before coming here. */
		if (gdk_painter->pixmap && gdk_painter->pixmap != gdk_painter->window)
			g_object_unref (gdk_painter->pixmap);

		gdk_painter->pixmap = gdk_pixmap_new (gdk_painter->window, width, height, -1);
		gdk_painter->x1 = x1;
		gdk_painter->y1 = y1;
		gdk_painter->x2 = x2;
		gdk_painter->y2 = y2;

		if (gdk_painter->set_background) {
			gdk_gc_set_background (gdk_painter->gc, &gdk_painter->background);
			gdk_painter->set_background = FALSE;
		}

		gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->background);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
				    TRUE, 0, 0, width, height);
	} else {
		gdk_painter->pixmap = gdk_painter->window;
		gdk_painter->x1 = 0;
		gdk_painter->y1 = 0;
		gdk_painter->x2 = 0;
		gdk_painter->y2 = 0;
	}

	g_return_if_fail (gdk_drawable_get_colormap (gdk_painter->pixmap) != NULL);
}

static void
end (HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	/* printf ("painter end\n"); */

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (! gdk_painter->double_buffer)
		return;

	gdk_draw_drawable (gdk_painter->window, gdk_painter->gc,
			   gdk_painter->pixmap,
			   0, 0,
			   gdk_painter->x1, gdk_painter->y1,
			   gdk_painter->x2 - gdk_painter->x1,
			   gdk_painter->y2 - gdk_painter->y1);

	g_object_unref (gdk_painter->pixmap);
	gdk_painter->pixmap = NULL;
}

static void
clear (HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (! gdk_painter->double_buffer) {
		gdk_window_clear (gdk_painter->window);
	} else {
		if (gdk_painter->pixmap != NULL)
			gdk_window_clear (gdk_painter->pixmap);
		else
			gdk_painter->do_clear = TRUE;
	}

	g_return_if_fail (gdk_drawable_get_colormap (gdk_painter->pixmap) != NULL);
}


static void
set_clip_rectangle (HTMLPainter *painter,
		    gint x, gint y,
		    gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;
	GdkRectangle rect;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (width == 0 || height == 0) {
		gdk_gc_set_clip_rectangle (gdk_painter->gc, NULL);
		return;
	}

	rect.x = CLAMP (x - gdk_painter->x1, 0, gdk_painter->x2 - gdk_painter->x1);
	rect.y = CLAMP (y - gdk_painter->y1, 0, gdk_painter->y2 - gdk_painter->y1);
	rect.width = CLAMP (width, 0, gdk_painter->x2 - gdk_painter->x1 - rect.x);
	rect.height = CLAMP (height, 0, gdk_painter->y2 - gdk_painter->y1 - rect.y);

	gdk_gc_set_clip_rectangle (gdk_painter->gc, &rect);
}

static void
set_background_color (HTMLPainter *painter,
		      const GdkColor *color)
{
	g_warning ("HTMLGdkPainter::set_background_color() needs to be implemented.");
}

static void
set_pen (HTMLPainter *painter,
	 const GdkColor *color)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	/* GdkColor API not const-safe!  */
	gdk_gc_set_foreground (gdk_painter->gc, (GdkColor *) color);
}

static const GdkColor *
get_black (const HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);
	return &gdk_painter->black;
}


/* HTMLPainter drawing functions.  */

static void
draw_line (HTMLPainter *painter,
	   gint x1, gint y1,
	   gint x2, gint y2)
 {
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	x1 -= gdk_painter->x1;
	y1 -= gdk_painter->y1;
	x2 -= gdk_painter->x1;
	y2 -= gdk_painter->y1;

	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x1, y1, x2, y2);
}

static void
draw_ellipse (HTMLPainter *painter,
	      gint x, gint y,
	      gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_arc (gdk_painter->pixmap, gdk_painter->gc, TRUE,
		      x - gdk_painter->x1, y - gdk_painter->y1,
		      width, height,
		      0, 360 * 64);
}

static void
draw_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc, FALSE,
			    x - gdk_painter->x1, y - gdk_painter->y1,
			    width, height);
}

static void
draw_border (HTMLPainter *painter,
	     GdkColor *bg,
	     gint x, gint y,
	     gint width, gint height,
	     HTMLBorderStyle style,
	     gint bordersize)
{
	HTMLGdkPainter *gdk_painter;
	GdkColor *col1 = NULL, *col2 = NULL;
	GdkColor dark, light;

	#define INC 0x8000
	#define DARK(c)  dark.c = MAX (((gint) bg->c) - INC, 0)
	#define LIGHT(c) light.c = MIN (((gint) bg->c) + INC, 0xffff)

	DARK(red);
	DARK(green);
	DARK(blue);
	LIGHT(red);
	LIGHT(green);
	LIGHT(blue);

	alloc_color (painter, &dark);
	alloc_color (painter, &light);

	gdk_painter = HTML_GDK_PAINTER (painter);

	switch (style) {
	case HTML_BORDER_SOLID:
		col1 = bg;
		col2 = bg;
		break;
	case HTML_BORDER_OUTSET:
		col1 = &light;
		col2 = &dark;
		break;
	default:
	case HTML_BORDER_INSET:
		col1 = &dark;
		col2 = &light;
		break;
	}

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	while (bordersize > 0) {
		if (col2) {
			gdk_gc_set_foreground (gdk_painter->gc, col2);
		}

		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x + width - 1, y, x + width - 1, y + height - 1);
		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x + 1, y + height - 1, x + width - 1, y + height - 1);
		if (col1) {
			gdk_gc_set_foreground (gdk_painter->gc, col1);
		}

		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x, y, x + width - 2, y);
		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x, y, x, y + height - 1);
		bordersize--;
		x++;
		y++;
		width-=2;
		height-=2;
	}

	free_color (painter, &dark);
	free_color (painter, &light);
}

static void
draw_background (HTMLPainter *painter,
		 GdkColor *color,
		 GdkPixbuf *pixbuf,
		 gint x, gint y,
		 gint width, gint height,
		 gint tile_x, gint tile_y)
{
	HTMLGdkPainter *gdk_painter;
	gint pw;
	gint ph;
	gint tile_width, tile_height;
	gint w, h;
	GdkRectangle expose, paint, clip;

	gdk_painter = HTML_GDK_PAINTER (painter);

	expose.x = x;
	expose.y = y;
	expose.width  = width;
	expose.height = height;

	clip.x = gdk_painter->x1;
	clip.width = gdk_painter->x2 - gdk_painter->x1;
	clip.y = gdk_painter->y1;
	clip.height = gdk_painter->y2 - gdk_painter->y1;

	if (!gdk_rectangle_intersect (&clip, &expose, &paint))
		return;

	tile_x += paint.x - expose.x;
	tile_y += paint.y - expose.y;

	if (!color && !pixbuf)
		return;

	if (color && !pixbuf) {
		gdk_gc_set_foreground (gdk_painter->gc, color);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
				    TRUE, paint.x - clip.x, paint.y - clip.y,
				    paint.width, paint.height);

	}

	if (!pixbuf)
		return;

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	/* optimize out some special cases */
	if (pw == 1 && ph == 1) {
		GdkColor pixcol;
		guchar *p;

		p = gdk_pixbuf_get_pixels (pixbuf);

		if (!(gdk_pixbuf_get_has_alpha (pixbuf) && (p[3] < 0x80))) {
			pixcol.red = p[0] * 0xff;
			pixcol.green = p[1] * 0xff;
			pixcol.blue = p[2] * 0xff;

			html_painter_alloc_color (painter, &pixcol);
			color = &pixcol;
		}

		if (color) {
			gdk_gc_set_foreground (gdk_painter->gc, color);
			gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
					    TRUE, paint.x - clip.x, paint.y - clip.y,
					    paint.width, paint.height);
		}

		return;
	}

	tile_width = (tile_x % pw) + paint.width;
	tile_height = (tile_y % ph) + paint.height;

	/* do tiling */
	if (tile_width > pw || tile_height > ph) {
		GdkPixmap *pixmap = NULL;
		gint cw, ch, cx, cy;
		gint dw, dh;
		GdkGC *gc;

		dw = MIN (pw, tile_width);
		dh = MIN (ph, tile_height);

		gc = gdk_gc_new (gdk_painter->window);

		if (color || !gdk_pixbuf_get_has_alpha (pixbuf)) {
			pixmap = gdk_pixmap_new (gdk_painter->window, dw, dh, -1);

			if (color) {
				gdk_gc_set_foreground (gc, color);
				gdk_draw_rectangle (pixmap, gc,
						    TRUE, 0, 0,
						    dw, dh);
			}

			gdk_draw_pixbuf (pixmap, NULL, pixbuf,
					 0, 0,
					 0, 0,
					 dw, dh,
					 GDK_RGB_DITHER_NORMAL,
					 paint.x, paint.y);

			gdk_gc_set_tile (gc, pixmap);
			gdk_gc_set_fill (gc, GDK_TILED);
			gdk_gc_set_ts_origin (gc,
					      paint.x - (tile_x % pw) - clip.x,
					      paint.y - (tile_y % ph) - clip.y);

			gdk_draw_rectangle (gdk_painter->pixmap, gc, TRUE,
					    paint.x - clip.x, paint.y - clip.y,
					    paint.width, paint.height);

			g_object_unref (pixmap);
			g_object_unref (gc);
		} else {
			gint incr_x = 0;
			gint incr_y = 0;

			cy = paint.y;
			ch = paint.height;
			h = tile_y % ph;
			while (ch > 0) {
				incr_y = dh - h;

				cx = paint.x;
				cw = paint.width;
				w = tile_x % pw;
				while (cw > 0) {
					incr_x = dw - w;

					gdk_draw_pixbuf (gdk_painter->pixmap, NULL, pixbuf,
							 w, h,
							 cx - clip.x, cy - clip.y,
							 (cw >= incr_x) ? incr_x : cw,
							 (ch >= incr_y) ? incr_y : ch,
							 GDK_RGB_DITHER_NORMAL,
							 cx, cy);

					cw -= incr_x;
					cx += incr_x;
					w = 0;
				}
				ch -= incr_y;
				cy += incr_y;
				h = 0;
			}

			g_object_unref (gc);
		}
	} else {
		if (color && gdk_pixbuf_get_has_alpha (pixbuf)) {
			gdk_gc_set_foreground (gdk_painter->gc, color);
			gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc, TRUE,
					    paint.x - clip.x, paint.y - clip.y,
					    paint.width, paint.height);
		}

		gdk_draw_pixbuf (gdk_painter->pixmap, NULL, pixbuf,
				 tile_x % pw, tile_y % ph,
				 paint.x - clip.x, paint.y - clip.y,
				 paint.width, paint.height,
				 GDK_RGB_DITHER_NORMAL,
				 paint.x, paint.y);
	}
}

static void
draw_pixmap (HTMLPainter *painter,
	     GdkPixbuf *pixbuf,
	     gint x, gint y,
	     gint scale_width, gint scale_height,
	     const GdkColor *color)
{
	GdkRectangle clip, image, paint;
	HTMLGdkPainter *gdk_painter;
	GdkPixbuf *tmp_pixbuf;
	guint n_channels;
	gint orig_width;
	gint orig_height;
	gint bilinear;

	gdk_painter = HTML_GDK_PAINTER (painter);

	orig_width = gdk_pixbuf_get_width (pixbuf);
	orig_height = gdk_pixbuf_get_height (pixbuf);

	if (scale_width < 0)
		scale_width = orig_width;
	if (scale_height < 0)
		scale_height = orig_height;

	image.x = x;
	image.y = y;
	image.width  = scale_width;
	image.height = scale_height;

	clip.x = gdk_painter->x1;
	clip.width = gdk_painter->x2 - gdk_painter->x1;
	clip.y = gdk_painter->y1;
	clip.height = gdk_painter->y2 - gdk_painter->y1;

	if (!gdk_rectangle_intersect (&clip, &image, &paint))
	    return;

	if (scale_width == orig_width && scale_height == orig_height && color == NULL) {
		gdk_draw_pixbuf (gdk_painter->pixmap, NULL, pixbuf,
				 paint.x - image.x,
				 paint.y - image.y,
				 paint.x - clip.x,
				 paint.y - clip.y,
				 paint.width,
				 paint.height,
				 GDK_RGB_DITHER_NORMAL,
				 paint.x, paint.y);
		return;
	}

	tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				     gdk_pixbuf_get_has_alpha (pixbuf),
				     gdk_pixbuf_get_bits_per_sample (pixbuf),
				     paint.width, paint.height);

	gdk_pixbuf_fill (tmp_pixbuf, 0xff000000);

	if (tmp_pixbuf == NULL)
		return;

	/*
	 * FIXME this is a hack to work around a gdk-pixbuf bug
	 * it could be removed when
	 * http://bugzilla.ximian.com/show_bug.cgi?id=12968
	 * is fixed.
	 */
	bilinear = !((scale_width == 1) && (scale_height == 1));

	gdk_pixbuf_composite (pixbuf, tmp_pixbuf,
			      0,
			      0,
			      paint.width, paint.height,
			      (double)-(paint.x - image.x),
			      (double)-(paint.y - image.y),
			      (gdouble) scale_width/ (gdouble) orig_width,
			      (gdouble) scale_height/ (gdouble) orig_height,
			      bilinear ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST,
			      255);

	if (color != NULL) {
		guchar *p, *q;
		guint i, j;

		n_channels = gdk_pixbuf_get_n_channels (tmp_pixbuf);
		p = q = gdk_pixbuf_get_pixels (tmp_pixbuf);
		for (i = 0; i < paint.height; i++) {
			p = q;

			for (j = 0; j < paint.width; j++) {
				gint r, g, b, a;

				if (n_channels > 3)
					a = p[3];
				else
					a = 0xff;

				r = (a * p[0] + color->red) >> 9;
				g = (a * p[1] + color->green) >> 9;
				b = (a * p[2] + color->blue) >> 9;

				p[0] = r;
				p[1] = g;
				p[2] = b;

				if (n_channels > 3)
					p[3] = (a + 127) / 2;

				p += n_channels;
			}

			q += gdk_pixbuf_get_rowstride (tmp_pixbuf);
		}
	}

	gdk_draw_pixbuf (gdk_painter->pixmap, NULL, tmp_pixbuf,
			 0,
			 0,
			 paint.x - clip.x,
			 paint.y - clip.y,
			 paint.width,
			 paint.height,
			 GDK_RGB_DITHER_NORMAL,
			 paint.x, paint.y);
	g_object_unref (tmp_pixbuf);
}

static void
fill_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
			    TRUE, x - gdk_painter->x1, y - gdk_painter->y1,
			    width, height);
}

static gint
draw_spell_error (HTMLPainter *painter, gint x, gint y, gint width)
{
	HTMLGdkPainter *gdk_painter;
	GdkGCValues values;
	gint8 dash_list[] = { 2, 2 };

	gdk_painter = HTML_GDK_PAINTER (painter);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	gdk_gc_get_values (gdk_painter->gc, &values);
	gdk_gc_set_fill (gdk_painter->gc, GDK_OPAQUE_STIPPLED);
	gdk_gc_set_line_attributes (gdk_painter->gc, 1, GDK_LINE_ON_OFF_DASH, values.cap_style, values.join_style);
	gdk_gc_set_dashes (gdk_painter->gc, 2, dash_list, 2);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y, x + width, y);
	gdk_gc_set_dashes (gdk_painter->gc, 0, dash_list, 2);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y + 1, x + width, y + 1);
	gdk_gc_set_fill (gdk_painter->gc, values.fill);
	gdk_gc_set_line_attributes (gdk_painter->gc, values.line_width,
				    values.line_style, values.cap_style, values.join_style);

	return width;
}

static void
draw_embedded (HTMLPainter * p, HTMLEmbedded *o, gint x, gint y)
{
	HTMLGdkPainter *gdk_painter = HTML_GDK_PAINTER(p);
	GtkWidget *embedded_widget;

	embedded_widget = html_embedded_get_widget (o);
	if (embedded_widget && GTK_IS_HTML_EMBEDDED (embedded_widget)) {
		g_signal_emit_by_name (embedded_widget,
				       "draw_gdk", 0,
				       gdk_painter->pixmap,
				       gdk_painter->gc,
				       x, y);
	}
}

static void
set_gdk_color_from_pango_color (GdkColor   *gdkc,
				PangoColor *pc)
{
	gdkc->red = pc->red;
	gdkc->green = pc->green;
	gdkc->blue = pc->blue;
}

static void
set_item_gc (HTMLPainter *p, HTMLPangoProperties *properties, GdkColor **fg_color, GdkColor **bg_color)
{
	if (properties->fg_color) {
		*fg_color = g_new0 (GdkColor, 1);
		set_gdk_color_from_pango_color (*fg_color, properties->fg_color);
	} else
		*fg_color = NULL;

	if (properties->bg_color) {
		*bg_color = g_new0 (GdkColor, 1);
		set_gdk_color_from_pango_color (*bg_color, properties->bg_color);
	} else {
		*bg_color = NULL;
	}
}

static gint
draw_lines (PangoGlyphString *str, gint x, gint y, GdkDrawable *drawable, GdkGC *gc, PangoItem *item, HTMLPangoProperties *properties)
{
	PangoRectangle log_rect;
	gint width, dsc, asc;

	pango_glyph_string_extents (str, item->analysis.font, NULL, &log_rect);

	width = log_rect.width;
	dsc = PANGO_PIXELS (PANGO_DESCENT (log_rect));
	asc = PANGO_PIXELS (PANGO_ASCENT (log_rect));

	if (properties->underline)
		gdk_draw_line (drawable, gc, x, y + dsc - 2, x + PANGO_PIXELS (width), y + dsc - 2);

	if (properties->strikethrough)
		gdk_draw_line (drawable, gc, x, y - asc + (asc + dsc)/2, x + PANGO_PIXELS (width), y - asc + (asc + dsc)/2);

	return width;
}

static gint
draw_glyphs (HTMLPainter *painter, gint x, gint y, PangoItem *item, PangoGlyphString *glyphs, GdkColor *fg, GdkColor *bg)
{
	HTMLGdkPainter *gdk_painter;
	guint i;
	HTMLPangoProperties properties;
	GdkColor *fg_text_color;
	GdkColor *bg_text_color;
	GdkGCValues orig_fg;
	gint cw = 0;

	gdk_painter = HTML_GDK_PAINTER (painter);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	html_pango_get_item_properties (item, &properties);

	set_item_gc (painter, &properties, &fg_text_color, &bg_text_color);

	if (bg_text_color || bg) {
		PangoRectangle log_rect;

		gdk_gc_get_values (gdk_painter->gc, &orig_fg);
		if (bg)
			gdk_gc_set_rgb_fg_color (gdk_painter->gc, bg);
		else
			gdk_gc_set_rgb_fg_color (gdk_painter->gc, bg_text_color);
		pango_glyph_string_extents (glyphs, item->analysis.font, NULL, &log_rect);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc, TRUE, x, y - PANGO_PIXELS (PANGO_ASCENT (log_rect)),
				    PANGO_PIXELS (log_rect.width), PANGO_PIXELS (log_rect.height));
		gdk_gc_set_foreground (gdk_painter->gc, &orig_fg.foreground);
	}

	if (fg_text_color || fg) {
		gdk_gc_get_values (gdk_painter->gc, &orig_fg);
		if (fg)
			gdk_gc_set_rgb_fg_color (gdk_painter->gc, fg);
		else
			gdk_gc_set_rgb_fg_color (gdk_painter->gc, fg_text_color);
	}

	gdk_draw_glyphs (gdk_painter->pixmap, gdk_painter->gc,
			 item->analysis.font, x, y, glyphs);
	if (properties.strikethrough || properties.underline)
		cw = draw_lines (glyphs, x, y, gdk_painter->pixmap, gdk_painter->gc, item, &properties);
	else
		for (i=0; i < glyphs->num_glyphs; i ++)
			cw += glyphs->glyphs [i].geometry.width;

	if (fg_text_color || fg)
		gdk_gc_set_foreground (gdk_painter->gc, &orig_fg.foreground);

	if (fg_text_color)
		g_free (fg_text_color);

	if (bg_text_color)
		g_free (bg_text_color);

	return cw;
}

static void
draw_shade_line (HTMLPainter *painter,
		 gint x, gint y,
		 gint width)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->dark);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y, x+width, y);
	gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->light);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y + 1, x + width, y + 1);
}

static guint
get_pixel_size (HTMLPainter *painter)
{
	return 1;
}

static guint
get_page_width (HTMLPainter *painter, HTMLEngine *e)
{
	return html_engine_get_view_width (e) + html_engine_get_left_border (e) + html_engine_get_right_border (e);
}

static guint
get_page_height (HTMLPainter *painter, HTMLEngine *e)
{
	return html_engine_get_view_height (e) + html_engine_get_top_border (e) + html_engine_get_bottom_border (e);
}

static void
init_color (GdkColor *color, gushort red, gushort green, gushort blue)
{
	color->pixel = 0;
	color->red = red;
	color->green = green;
	color->blue = blue;
}

static void
html_gdk_painter_init (GObject *object)
{
	HTMLPainter *painter;
	HTMLGdkPainter *gdk_painter;

	painter = HTML_PAINTER (object);
	gdk_painter = HTML_GDK_PAINTER (object);

	painter->engine_to_pango = PANGO_SCALE;

	gdk_painter->window = NULL;

	gdk_painter->gc = NULL;

	gdk_painter->double_buffer = TRUE;
	gdk_painter->pixmap = NULL;
	gdk_painter->x1 = gdk_painter->y1 = 0;
	gdk_painter->x2 = gdk_painter->y2 = 0;
	gdk_painter->set_background = FALSE;
	gdk_painter->do_clear = FALSE;

	init_color (& gdk_painter->background, 0xffff, 0xffff, 0xffff);
	init_color (& gdk_painter->dark, 0, 0, 0);
	init_color (& gdk_painter->light, 0, 0, 0);
}

static void
html_gdk_painter_real_set_widget (HTMLPainter *painter, GtkWidget *widget)
{
	parent_class->set_widget (painter, widget);

	if (painter->pango_context)
		g_object_unref (painter->pango_context);
	painter->pango_context = gtk_widget_get_pango_context (widget);
	g_object_ref (painter->pango_context);
}

static void
html_gdk_painter_class_init (GObjectClass *object_class)
{
	HTMLPainterClass *painter_class;

	painter_class = HTML_PAINTER_CLASS (object_class);

	object_class->finalize = finalize;
	parent_class = g_type_class_ref (HTML_TYPE_PAINTER);

	painter_class->set_widget = html_gdk_painter_real_set_widget;
	painter_class->begin = begin;
	painter_class->end = end;
	painter_class->alloc_color = alloc_color;
	painter_class->free_color = free_color;
	painter_class->set_pen = set_pen;
	painter_class->get_black = get_black;
	painter_class->draw_line = draw_line;
	painter_class->draw_rect = draw_rect;
	painter_class->draw_glyphs = draw_glyphs;
	painter_class->draw_spell_error = draw_spell_error;
	painter_class->fill_rect = fill_rect;
	painter_class->draw_pixmap = draw_pixmap;
	painter_class->draw_ellipse = draw_ellipse;
	painter_class->clear = clear;
	painter_class->set_background_color = set_background_color;
	painter_class->draw_shade_line = draw_shade_line;
	painter_class->draw_border = draw_border;
	painter_class->set_clip_rectangle = set_clip_rectangle;
	painter_class->draw_background = draw_background;
	painter_class->get_pixel_size = get_pixel_size;
	painter_class->draw_embedded = draw_embedded;
	painter_class->get_page_width = get_page_width;
	painter_class->get_page_height = get_page_height;
}

GType
html_gdk_painter_get_type (void)
{
	static GType html_gdk_painter_type = 0;

	if (html_gdk_painter_type == 0) {
		static const GTypeInfo html_gdk_painter_info = {
			sizeof (HTMLGdkPainterClass),
			NULL,
			NULL,
			(GClassInitFunc) html_gdk_painter_class_init,
			NULL,
			NULL,
			sizeof (HTMLGdkPainter),
			1,
			(GInstanceInitFunc) html_gdk_painter_init,
		};
		html_gdk_painter_type = g_type_register_static (HTML_TYPE_PAINTER, "HTMLGdkPainter",
								&html_gdk_painter_info, 0);
	}

	return html_gdk_painter_type;
}

HTMLPainter *
html_gdk_painter_new (GtkWidget *widget, gboolean double_buffer)
{
	HTMLGdkPainter *new;

	new = g_object_new (HTML_TYPE_GDK_PAINTER, NULL);

	new->double_buffer = double_buffer;
	html_painter_set_widget (HTML_PAINTER (new), widget);

	return HTML_PAINTER (new);
}

void
html_gdk_painter_realize (HTMLGdkPainter *gdk_painter,
			  GdkWindow *window)
{
	g_return_if_fail (gdk_painter != NULL);
	g_return_if_fail (window != NULL);

	gdk_painter->gc = gdk_gc_new (window);
	gdk_painter->window = window;

	gdk_painter->light.red = 0xffff;
	gdk_painter->light.green = 0xffff;
	gdk_painter->light.blue = 0xffff;
	html_painter_alloc_color (HTML_PAINTER (gdk_painter), &gdk_painter->light);

	gdk_painter->dark.red = 0x7fff;
	gdk_painter->dark.green = 0x7fff;
	gdk_painter->dark.blue = 0x7fff;
	html_painter_alloc_color (HTML_PAINTER (gdk_painter), &gdk_painter->dark);

	gdk_painter->black.red = 0x0000;
	gdk_painter->black.green = 0x0000;
	gdk_painter->black.blue = 0x0000;
	html_painter_alloc_color (HTML_PAINTER (gdk_painter), &gdk_painter->black);
}

void
html_gdk_painter_unrealize (HTMLGdkPainter *painter)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_GDK_PAINTER (painter));

	if (html_gdk_painter_realized (painter)) {
		g_object_unref (painter->gc);
		painter->gc = NULL;

		painter->window = NULL;
	}
}

gboolean
html_gdk_painter_realized (HTMLGdkPainter *painter)
{
	g_return_val_if_fail (painter != NULL, FALSE);
	g_return_val_if_fail (HTML_IS_GDK_PAINTER (painter), FALSE);

	if (painter->window == NULL)
		return FALSE;
	else
		return TRUE;
}
