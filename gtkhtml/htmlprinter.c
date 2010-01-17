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
#include <ctype.h>

#include "htmlembedded.h"
#include "gtkhtml-embedded.h"
#include "htmlfontmanager.h"
#include "htmlprinter.h"
#include "htmltext.h"

/* #define PRINTER_DEBUG */


static gpointer parent_class = NULL;


/* The size of a pixel in the printed output, in points.  */
#define PIXEL_SIZE .5

static gdouble
printer_get_page_height (HTMLPrinter *printer)
{
	GtkPageSetup *page_setup;

	page_setup = gtk_print_context_get_page_setup (printer->context);
	return gtk_page_setup_get_page_height (page_setup, GTK_UNIT_POINTS);
}

static gdouble
printer_get_page_width (HTMLPrinter *printer)
{
	GtkPageSetup *page_setup;

	page_setup = gtk_print_context_get_page_setup (printer->context);
	return gtk_page_setup_get_page_width (page_setup, GTK_UNIT_POINTS);
}

gdouble
html_printer_scale_to_gnome_print (HTMLPrinter *printer, gint x)
{
	return SCALE_ENGINE_TO_GNOME_PRINT (x);
}


/* GtkObject methods.  */

static void
finalize (GObject *object)
{
	HTMLPrinter *printer;

	printer = HTML_PRINTER (object);

	if (printer->context != NULL) {
		/* FIXME g_object_unref (printer->context);*/
		printer->context = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
begin (HTMLPainter *painter,
       gint x1, gint y1,
       gint x2, gint y2)
{
	HTMLPrinter *printer;
	GtkPrintContext *pc;
	gdouble printer_x1, printer_y1;
	gdouble printer_x2, printer_y2;
	cairo_t *cr;
#ifdef PRINTER_DEBUG
	gdouble dash [2];
#endif
	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer);
	pc      = printer->context;
	g_return_if_fail (pc);

	cr = gtk_print_context_get_cairo_context (pc);
	cairo_save (cr);

	printer_x1 = SCALE_ENGINE_TO_GNOME_PRINT (x1);
	printer_y1 = SCALE_ENGINE_TO_GNOME_PRINT (y1);

	printer_x2 = printer_x1 + SCALE_ENGINE_TO_GNOME_PRINT (x2);
	printer_y2 = SCALE_ENGINE_TO_GNOME_PRINT (y2);

	cairo_rectangle (cr, printer_x1, printer_y1, printer_x2, printer_y2);
#ifdef PRINTER_DEBUG
	cairo_save (cr);
	dash [0] = 10.0;
	dash [1] = 10.0;
	cairo_set_source_rgb (cr, .5, .5, .5);
	cairo_set_line_width (cr, .3);
	cairo_set_dash (cr, 2, dash, .0);
	cairo_stroke (cr);
	cairo_restore (cr);
#endif
	cairo_clip (cr);
	cairo_restore (cr);
}

static void
end (HTMLPainter *painter)
{
}

static void
clear (HTMLPainter *painter)
{
}


static void
alloc_color (HTMLPainter *painter, GdkColor *color)
{
}

static void
free_color (HTMLPainter *painter, GdkColor *color)
{
}


static void
set_pen (HTMLPainter *painter,
	 const GdkColor *color)
{
	HTMLPrinter *printer;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer->context != NULL);

	cr = gtk_print_context_get_cairo_context (printer->context);
	cairo_set_source_rgb (cr, color->red / 65535.0, color->green / 65535.0, color->blue / 65535.0);
}

static const GdkColor *
get_black (const HTMLPainter *painter)
{
	static GdkColor black = { 0, 0, 0, 0 };

	return &black;
}

static void
prepare_rectangle (HTMLPainter *painter, gint _x, gint _y, gint w, gint h)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	GtkPrintContext *context = printer->context;
	gdouble x;
	gdouble y;
	gdouble width;
	gdouble height;
	cairo_t *cr;

	width = SCALE_ENGINE_TO_GNOME_PRINT (w);
	height = SCALE_ENGINE_TO_GNOME_PRINT (h);
	x = SCALE_ENGINE_TO_GNOME_PRINT (_x);
	y = SCALE_ENGINE_TO_GNOME_PRINT (_y);
	cr = gtk_print_context_get_cairo_context (context);
	cairo_new_path (cr);
	cairo_rectangle (cr, x, y, x + width, y + height );
	cairo_close_path (cr);
}

static void
do_rectangle (HTMLPainter *painter, gint x, gint y, gint w, gint h, gint lw)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	GtkPrintContext *context = printer->context;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);
	cairo_set_line_width (cr, SCALE_ENGINE_TO_GNOME_PRINT (lw) * PIXEL_SIZE);
	prepare_rectangle (painter,x, y, w, h);
	cairo_stroke (cr);
}

static void
set_clip_rectangle (HTMLPainter *painter,
		    gint x, gint y,
		    gint width, gint height)
{
	cairo_t *cr;
	prepare_rectangle (painter, x, y, width, height);
	cr = gtk_print_context_get_cairo_context (HTML_PRINTER (painter)->context);
	cairo_clip (cr);
}

/* HTMLPainter drawing functions.  */

static void
draw_line (HTMLPainter *painter,
	   gint x1, gint y1,
	   gint x2, gint y2)
{
	HTMLPrinter *printer;
	double printer_x1, printer_y1;
	double printer_x2, printer_y2;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer->context != NULL);

	printer_x1 = SCALE_ENGINE_TO_GNOME_PRINT (x1);
	printer_y1 = SCALE_ENGINE_TO_GNOME_PRINT (y1);
	printer_x2 = SCALE_ENGINE_TO_GNOME_PRINT (x2);
	printer_y2 = SCALE_ENGINE_TO_GNOME_PRINT (y2);

	cr = gtk_print_context_get_cairo_context (printer->context);
	cairo_set_line_width (cr, PIXEL_SIZE);

	cairo_new_path (cr);
	cairo_move_to (cr, printer_x1, printer_y1);
	cairo_line_to (cr, printer_x2, printer_y2);
	cairo_stroke (cr);
}

static void
draw_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
	do_rectangle (painter, x, y, width, height, 1);
}

static void
draw_border (HTMLPainter *painter,
	     GdkColor *bg,
	     gint _x, gint _y,
	     gint w, gint h,
	     HTMLBorderStyle style,
	     gint bordersize)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	GtkPrintContext *pc = printer->context;
	GdkColor *col1 = NULL, *col2 = NULL;
	GdkColor dark, light;
	gdouble x;
	gdouble y;
	gdouble width, bs;
	gdouble height;
	cairo_t *cr;

	#define INC 0x8000
	#define DARK(c)  dark.c = MAX (((gint) bg->c) - INC, 0)
	#define LIGHT(c) light.c = MIN (((gint) bg->c) + INC, 0xffff)

	DARK(red);
	DARK(green);
	DARK(blue);
	LIGHT(red);
	LIGHT(green);
	LIGHT(blue);

	switch (style) {
	case HTML_BORDER_SOLID:
		/* do not draw peripheral border *
		col1 = bg;
		col2 = bg;
		break; */
		return;
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

	width  = SCALE_ENGINE_TO_GNOME_PRINT (w);
	height = SCALE_ENGINE_TO_GNOME_PRINT (h);
	bs     = SCALE_ENGINE_TO_GNOME_PRINT (bordersize);

	x = SCALE_ENGINE_TO_GNOME_PRINT (_x);
	y = SCALE_ENGINE_TO_GNOME_PRINT (_y);

	cr = gtk_print_context_get_cairo_context (pc);
	if (col2)
		cairo_set_source_rgb (cr, col1->red / 65535.0, col1->green / 65535.0, col1->blue / 65535.0);
	cairo_new_path (cr);
	cairo_move_to  (cr, x, y);
	cairo_line_to  (cr, x + width, y);
	cairo_line_to  (cr, x + width - bs, y + bs);
	cairo_line_to  (cr, x + bs, y + bs );
	cairo_line_to  (cr, x + bs, y + height - bs);
	cairo_line_to  (cr, x, y + height);
	cairo_close_path (cr);
	cairo_fill (cr);

	if (col1)
		cairo_set_source_rgb (cr, col2->red / 65535.0, col2->green / 65535.0, col2->blue / 65535.0);

	cairo_new_path (cr);
	cairo_move_to  (cr, x, y + height);
	cairo_line_to  (cr, x + width, y + height);
	cairo_line_to  (cr, x + width, y);
	cairo_line_to  (cr, x + width - bs, y + bs);
	cairo_line_to  (cr, x + width - bs, y + height - bs);
	cairo_line_to  (cr, x + bs, y + height - bs);
	cairo_close_path (cr);
	cairo_fill (cr);
}

static void
draw_background (HTMLPainter *painter,
		 GdkColor *color,
		 GdkPixbuf *pixbuf,
		 gint ix, gint iy,
		 gint pix_width, gint pix_height,
		 gint tile_x, gint tile_y)
{
	GtkPrintContext *pc;
	HTMLPrinter *printer;
	gdouble x, y, width, height;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer);
	pc = printer->context;
	g_return_if_fail (printer->context);

	width = SCALE_ENGINE_TO_GNOME_PRINT  (pix_width);
	height = SCALE_ENGINE_TO_GNOME_PRINT (pix_height);
	x = SCALE_ENGINE_TO_GNOME_PRINT (ix);
	y = SCALE_ENGINE_TO_GNOME_PRINT (iy);

	if (color) {
		cr = gtk_print_context_get_cairo_context (pc);
		cairo_save (cr);
		cairo_set_source_rgb (cr, color->red / 65535.0, color->green / 65535.0, color->blue / 65535.0);
		cairo_new_path (cr);
		cairo_rectangle (cr, x, y, width, height);
		cairo_close_path (cr);
		cairo_fill (cr);
		cairo_restore (cr);
	}
}

static void
print_pixbuf (GtkPrintContext *pc, GdkPixbuf *pixbuf)
{
	cairo_t *cr;
	if (!pixbuf || (gdk_pixbuf_get_colorspace (pixbuf) != GDK_COLORSPACE_RGB))
		return;

	cr = gtk_print_context_get_cairo_context (pc);
	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
		cairo_rectangle (cr, 0, 0, (double)gdk_pixbuf_get_width (pixbuf), (double)gdk_pixbuf_get_height (pixbuf));                 cairo_clip (cr);
		cairo_paint (cr);

	} else {
		gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
		cairo_rectangle (cr, 0, 0, (double)gdk_pixbuf_get_width (pixbuf), (double)gdk_pixbuf_get_height (pixbuf));
		cairo_clip (cr);
		cairo_paint (cr);
	}
}

static void
draw_pixmap (HTMLPainter *painter, GdkPixbuf *pixbuf, gint x, gint y, gint scale_width, gint scale_height, const GdkColor *color)
{
	HTMLPrinter *printer;
	double print_x, print_y;
	double print_scale_width, print_scale_height;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer->context != NULL);
	cr = gtk_print_context_get_cairo_context (printer->context);
	print_x = SCALE_ENGINE_TO_GNOME_PRINT (x);
	print_y = SCALE_ENGINE_TO_GNOME_PRINT (y);
	print_scale_width  = SCALE_ENGINE_TO_GNOME_PRINT (scale_width);
	print_scale_height = SCALE_ENGINE_TO_GNOME_PRINT (scale_height);
	cairo_save (cr);
	cairo_translate (cr, print_x, print_y);
	cairo_scale (cr, print_scale_width / (double)gdk_pixbuf_get_width (pixbuf), print_scale_height / (double)gdk_pixbuf_get_height (pixbuf));
	print_pixbuf (printer->context, pixbuf);
	cairo_restore (cr);
}

static void
fill_rect (HTMLPainter *painter, gint x, gint y, gint width, gint height)
{
	HTMLPrinter *printer;
	double printer_x, printer_y;
	double printer_width, printer_height;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer->context != NULL);

	printer_width = SCALE_ENGINE_TO_GNOME_PRINT (width);
	printer_height = SCALE_ENGINE_TO_GNOME_PRINT (height);

	printer_x = SCALE_ENGINE_TO_GNOME_PRINT (x);
	printer_y = SCALE_ENGINE_TO_GNOME_PRINT (y);

	cr = gtk_print_context_get_cairo_context (printer->context);
	cairo_new_path (cr);
	cairo_rectangle (cr, printer_x, printer_y, printer_width, printer_height);
	cairo_close_path (cr);
	cairo_fill (cr);
}

static void
draw_lines (HTMLPrinter *printer, double x, double y, double width, PangoAnalysis *analysis, HTMLPangoProperties *properties)
{
	PangoFontMetrics *metrics;
	cairo_t *cr;

	if (!properties->underline && !properties->strikethrough)
		return;

	metrics = pango_font_get_metrics (analysis->font, analysis->language);
	cr = gtk_print_context_get_cairo_context (printer->context);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

	if (properties->underline) {
		double thickness = pango_units_to_double (pango_font_metrics_get_underline_thickness (metrics));
		double position = pango_units_to_double (pango_font_metrics_get_underline_position (metrics));
		double ly = y + position - thickness / 2;

		cairo_new_path (cr);
		cairo_move_to (cr, x, ly + 4);
		cairo_line_to (cr, x + width, ly + 4);
		cairo_set_line_width (cr, thickness);
		cairo_stroke (cr);
	}

	if (properties->strikethrough) {
		double thickness = pango_units_to_double (pango_font_metrics_get_strikethrough_thickness (metrics));
		double position = pango_units_to_double (pango_font_metrics_get_strikethrough_position (metrics));
		double ly = y + position - thickness / 2;

		cairo_new_path (cr);
		cairo_move_to (cr, x, ly - 8);
		cairo_line_to (cr, x + width, ly - 8);
		cairo_set_line_width (cr, thickness);
		cairo_stroke (cr);
	}
}

static gint
draw_glyphs (HTMLPainter *painter, gint x, gint y, PangoItem *item, PangoGlyphString *glyphs, GdkColor *fg, GdkColor *bg)
{
	HTMLPrinter *printer;
	gdouble print_x, print_y;
	PangoRectangle log_rect;
	HTMLPangoProperties properties;
	cairo_t *cr;

	printer = HTML_PRINTER (painter);

	print_x = SCALE_ENGINE_TO_GNOME_PRINT (x);
	print_y = SCALE_ENGINE_TO_GNOME_PRINT (y);

	cr = gtk_print_context_get_cairo_context (printer->context);

	cairo_save (cr);

	html_pango_get_item_properties (item, &properties);
	pango_glyph_string_extents (glyphs, item->analysis.font, NULL, &log_rect);

	if (properties.bg_color) {
		   cairo_set_source_rgb (cr,
					 properties.bg_color->red / 65535.0,
					 properties.bg_color->green / 65535.0,
					 properties.bg_color->blue / 65535.0);
			cairo_rectangle (cr,
					 print_x,
					 print_y + pango_units_to_double (log_rect.y + log_rect.height),
					 pango_units_to_double (log_rect.width),
					 pango_units_to_double (log_rect.height));
			cairo_fill (cr);
	}

	if (properties.fg_color) {
		cairo_set_source_rgb     (cr,
					 properties.fg_color->red / 65535.0,
					 properties.fg_color->green / 65535.0,
					 properties.fg_color->blue / 65535.0);
	} else {
			cairo_set_source_rgb (cr, 0., 0., 0.);
	}
	cairo_move_to (cr, print_x, print_y);
	pango_cairo_show_glyph_string (cr,
				       item->analysis.font,
				       glyphs);
	draw_lines (printer, print_x, print_y,
		    pango_units_to_double (log_rect.width),
		    &item->analysis, &properties);

	cairo_restore (cr);
	return log_rect.width;
}

static void
draw_embedded (HTMLPainter *p, HTMLEmbedded *o, gint x, gint y)
{
	gdouble print_x, print_y;
	HTMLPrinter *printer = HTML_PRINTER(p);
	GtkWidget *embedded_widget;
	cairo_t *cr;

	print_x = SCALE_ENGINE_TO_GNOME_PRINT (x);
	print_y = SCALE_ENGINE_TO_GNOME_PRINT (y);

	cr = gtk_print_context_get_cairo_context (printer->context);
	cairo_save (cr);
	cairo_translate (cr,
			print_x, print_y + o->height * PIXEL_SIZE);
	embedded_widget = html_embedded_get_widget(o);
	if (embedded_widget && GTK_IS_HTML_EMBEDDED (embedded_widget)) {
		g_signal_emit_by_name(G_OBJECT (embedded_widget),
				      "draw_print",
				       cr);
	}
	cairo_restore (cr);
}

static void
draw_shade_line (HTMLPainter *painter,
		 gint x, gint y,
		 gint width)
{
	HTMLPrinter *printer;

	printer = HTML_PRINTER (painter);
	g_return_if_fail (printer->context != NULL);

	/* FIXME */
}

static guint
get_pixel_size (HTMLPainter *painter)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);

	return SCALE_GNOME_PRINT_TO_ENGINE (PIXEL_SIZE);
}

static guint
get_page_width (HTMLPainter *painter, HTMLEngine *e)
{
	return html_printer_get_page_width (HTML_PRINTER (painter));
}

static guint
get_page_height (HTMLPainter *painter, HTMLEngine *e)
{
	return html_printer_get_page_height (HTML_PRINTER (painter));
}

static void
html_printer_init (HTMLPrinter *printer)
{
	html_printer_set_scale (printer, 1.0);
}

static void
html_printer_class_init (HTMLPrinterClass *class)
{
	GObjectClass *object_class;
	HTMLPainterClass *painter_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = finalize;

	painter_class = HTML_PAINTER_CLASS (class);
	painter_class->begin = begin;
	painter_class->end = end;
	painter_class->alloc_color = alloc_color;
	painter_class->free_color = free_color;
	painter_class->set_pen = set_pen;
	painter_class->get_black = get_black;
	painter_class->draw_line = draw_line;
	painter_class->draw_rect = draw_rect;
	painter_class->draw_border = draw_border;
	painter_class->draw_glyphs = draw_glyphs;
	painter_class->fill_rect = fill_rect;
	painter_class->draw_pixmap = draw_pixmap;
	painter_class->clear = clear;
	painter_class->draw_shade_line = draw_shade_line;
	painter_class->draw_background = draw_background;
	painter_class->get_pixel_size = get_pixel_size;
	painter_class->set_clip_rectangle = set_clip_rectangle;
	painter_class->draw_embedded = draw_embedded;
	painter_class->get_page_width = get_page_width;
	painter_class->get_page_height = get_page_height;
}

GType
html_printer_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (HTMLPrinterClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) html_printer_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (HTMLPrinter),
			0,     /* n_preallocs */
			(GInstanceInitFunc) html_printer_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			HTML_TYPE_PAINTER, "HTMLPrinter", &type_info, 0);
	}

	return type;
}

HTMLPainter *
html_printer_new (GtkWidget *widget, GtkPrintContext *context)
{
	GtkStyle *style;
	HTMLPrinter *printer;
	HTMLPainter *painter;

	printer = g_object_new (HTML_TYPE_PRINTER, NULL);
	printer->context = g_object_ref (context);

	painter = HTML_PAINTER (printer);
	html_painter_set_widget (painter, widget);
	style = gtk_widget_get_style (widget);
	painter->pango_context =
		gtk_print_context_create_pango_context (context);
	pango_context_set_font_description (
		painter->pango_context, style->font_desc);

	return painter;

}


guint
html_printer_get_page_width (HTMLPrinter *printer)
{
	double printer_width;
	guint engine_width;

	g_return_val_if_fail (printer != NULL, 0);
	g_return_val_if_fail (HTML_IS_PRINTER (printer), 0);

	printer_width = printer_get_page_width (printer);
	engine_width = SCALE_GNOME_PRINT_TO_ENGINE (printer_width);

	return engine_width;
}

guint
html_printer_get_page_height (HTMLPrinter *printer)
{
	double printer_height;
	guint engine_height;

	g_return_val_if_fail (printer != NULL, 0);
	g_return_val_if_fail (HTML_IS_PRINTER (printer), 0);

	printer_height = printer_get_page_height (printer);
	engine_height = SCALE_GNOME_PRINT_TO_ENGINE (printer_height);

	return engine_height;
}

/**
 * html_printer_set_scale:
 * @printer: a #HTMLPrinter
 * @scale: new scale factor
 *
 * Sets the scale factor for printer output. Output coordinates
 * will be multiplied by @scale.
 **/
void
html_printer_set_scale (HTMLPrinter *printer, gdouble scale)
{
	HTMLPainter *painter;

	g_return_if_fail (HTML_IS_PRINTER (printer));
	g_return_if_fail (scale >= 0);

	painter = HTML_PAINTER (printer);

	printer->scale = scale;
	painter->engine_to_pango = scale;
}
