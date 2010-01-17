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
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "gtkhtml.h"
#include "gtkhtmldebug.h"
#include "gtkhtml-private.h"
#include "gtkhtml-properties.h"
#include "htmlprinter.h"
#include "htmlengine-print.h"
#include "htmlobject.h"



/* #define CLIP_DEBUG */

static void
print_header_footer (HTMLPainter *painter,
		     HTMLEngine *engine,
		     gint width,
		     gint y,
		     gdouble height,
		     GtkHTMLPrintCallback callback,
		     gpointer user_data)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	GtkPrintContext *context = printer->context;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);
	html_painter_set_clip_rectangle (
		painter, 0, y, width, SCALE_GNOME_PRINT_TO_ENGINE (height));
#ifdef CLIP_DEBUG
	html_painter_draw_rect (
		painter, 0, y, width, SCALE_GNOME_PRINT_TO_ENGINE (height));
#endif
	(*callback) (
		GTK_HTML (engine->widget), context,
		SCALE_ENGINE_TO_GNOME_PRINT (0),
		SCALE_ENGINE_TO_GNOME_PRINT (y),
		SCALE_ENGINE_TO_GNOME_PRINT (width),
		height, user_data);
	cairo_restore (cr);
}

static void
print_page (HTMLPainter *painter,
	    HTMLEngine *engine,
	    gint start_y,
	    gint page_width,
	    gint page_height,
	    gint body_height,
	    gdouble header_height,
	    gdouble footer_height,
	    GtkHTMLPrintCallback header_print,
	    GtkHTMLPrintCallback footer_print,
	    gpointer user_data)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	GtkPrintContext *context = printer->context;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);

	/* Show the previous page before we start drawing a new one.
	 * GtkPrint will show the last page automatically for us. */
	if (start_y > 0)
		cairo_show_page (cr);

	html_painter_begin (painter, 0, 0, page_width, page_height);

	if (header_print != NULL)
		print_header_footer (
			painter, engine, page_width, 0, header_height,
			header_print, user_data);

	cairo_save (cr);
	html_painter_set_clip_rectangle (
		painter, 0, header_height, page_width, body_height);
#ifdef CLIP_DEBUG
	html_painter_draw_rect (
		painter, 0, header_height, page_width, body_height);
#endif
	html_object_draw (
		engine->clue, painter, 0, start_y, page_width,
		body_height, 0, -start_y + header_height);
	cairo_restore (cr);

	if (footer_print != NULL)
		print_header_footer (
			painter, engine, page_width, page_height -
			SCALE_GNOME_PRINT_TO_ENGINE (footer_height),
			footer_height, footer_print, user_data);

	html_painter_end (painter);
}

static gint
print_all_pages (HTMLPainter *painter,
		 HTMLEngine *engine,
		 gdouble header_height,
		 gdouble footer_height,
		 GtkHTMLPrintCallback header_print,
		 GtkHTMLPrintCallback footer_print,
		 gpointer user_data,
		 gboolean do_print)
{
	HTMLPrinter *printer = HTML_PRINTER (painter);
	gint new_split_offset, split_offset;
	gint document_height;
	gint pages = 0;
	gint page_width;
	gint page_height;
	gint body_height;

	page_height = html_printer_get_page_height (printer);
	page_width = html_printer_get_page_width (printer);

	if (header_height + footer_height >= page_height) {
		header_print = footer_print = NULL;
		g_warning ("Page header height + footer height >= "
			   "page height, disabling header/footer printing");
	}

	body_height = page_height -
		SCALE_GNOME_PRINT_TO_ENGINE (header_height + footer_height);
	split_offset = 0;

	document_height = html_engine_get_doc_height (engine);

	do {
		pages ++;
		new_split_offset = html_object_check_page_split (
			engine->clue, painter, split_offset + body_height);

		if (new_split_offset <= split_offset ||
		    new_split_offset - split_offset <
		    engine->min_split_index * body_height)
			new_split_offset = split_offset + body_height;

		if (do_print)
			print_page (
				painter, engine, split_offset, page_width,
				page_height, new_split_offset - split_offset,
				header_height, footer_height, header_print,
				footer_print, user_data);

		split_offset = new_split_offset;

	} while (split_offset < document_height);

	return pages;
}

static gint
print_with_header_footer (HTMLEngine *engine,
			  GtkPrintContext *context,
			  gdouble header_height,
			  gdouble footer_height,
			  GtkHTMLPrintCallback header_print,
			  GtkHTMLPrintCallback footer_print,
			  gpointer user_data,
			  gboolean do_print)
{
	HTMLPainter *printer;
	HTMLFont *default_font;
	gint pages = 0;

	g_return_val_if_fail (engine->clue != NULL, 0);

	printer = html_printer_new (
		GTK_WIDGET (engine->widget), context);
	gtk_html_set_fonts (engine->widget, printer);

	default_font = html_painter_get_font (
		printer, NULL, GTK_HTML_FONT_STYLE_DEFAULT);

	if (default_font != NULL) {
		HTMLPainter *old_painter;
		gint min_width, page_width;

		old_painter = g_object_ref (engine->painter);

		html_engine_set_painter (engine, printer);

		min_width = html_engine_calc_min_width (engine);
		page_width = html_painter_get_page_width (
			engine->painter, engine);
		if (min_width > page_width) {
			html_printer_set_scale (
				HTML_PRINTER (printer),
				MAX (0.5, ((gdouble) page_width) / min_width));
			html_font_manager_clear_font_cache (
				&printer->font_manager);
			html_object_change_set_down (
				engine->clue, HTML_CHANGE_ALL);
			html_engine_calc_size (engine, NULL);
		}

		pages = print_all_pages (
			HTML_PAINTER (printer), engine, header_height,
			footer_height, header_print, footer_print,
			user_data, do_print);

		html_engine_set_painter (engine, old_painter);
		g_object_unref (old_painter);
	} else {
		/* TODO2 dialog instead of warning */
		g_warning (_("Cannot allocate default font for printing"));
	}

	g_object_unref (printer);

	return pages;
}

void
html_engine_print (HTMLEngine *engine,
		   GtkPrintContext *context,
		   gdouble header_height,
		   gdouble footer_height,
		   GtkHTMLPrintCallback header_print,
		   GtkHTMLPrintCallback footer_print,
		   gpointer user_data)
{
	print_with_header_footer (
		engine, context, header_height, footer_height,
		header_print, footer_print, user_data, TRUE);
}

gint
html_engine_print_get_pages_num (HTMLEngine *engine,
				 GtkPrintContext *context,
				 gdouble header_height,
				 gdouble footer_height)
{
	return print_with_header_footer (
		engine, context, header_height, footer_height,
		NULL, NULL, NULL, FALSE);
}

void
html_engine_print_set_min_split_index (HTMLEngine *engine, gdouble index)
{
	engine->min_split_index = index;
}

typedef struct {
	HTMLEngine *engine;
	HTMLPainter *painter;
	HTMLPainter *old_painter;
	GtkHTMLPrintCalcHeight calc_header_height;
	GtkHTMLPrintCalcHeight calc_footer_height;
	GtkHTMLPrintDrawFunc draw_header;
	GtkHTMLPrintDrawFunc draw_footer;
	gint header_height;
	gint footer_height;
	gpointer user_data;
	GArray *offsets;
} EnginePrintData;

static void
engine_print_begin_print (GtkPrintOperation *operation,
                          GtkPrintContext *context,
                          EnginePrintData *data)
{
	HTMLPrinter *printer;
	HTMLFont *default_font;
	gint min_width;
	gint page_width;
	gint page_height;
	gint body_height;
	gint document_height;
	gint new_split_offset;
	gint split_offset;

	data->painter = html_printer_new (
		GTK_WIDGET (data->engine->widget), context);
	gtk_html_set_fonts (data->engine->widget, data->painter);

	data->offsets = g_array_new (FALSE, TRUE, sizeof (gint));

	default_font = html_painter_get_font (
		data->painter, NULL, GTK_HTML_FONT_STYLE_DEFAULT);

	if (default_font == NULL)
		g_warning (_("Cannot allocate default font for printing"));

	data->old_painter = g_object_ref (data->engine->painter);
	html_engine_set_painter (data->engine, data->painter);

	printer = HTML_PRINTER (data->painter);

	min_width = html_engine_calc_min_width (data->engine);
	page_width = html_painter_get_page_width (
		data->engine->painter, data->engine);
	if (min_width > page_width) {
		html_printer_set_scale (
			printer, MAX (0.5,
			((gdouble) page_width) / min_width));
		html_font_manager_clear_font_cache (
			&data->painter->font_manager);
		html_object_change_set_down (
			data->engine->clue, HTML_CHANGE_ALL);
		html_engine_calc_size (data->engine, NULL);
	}

	page_height = html_printer_get_page_height (printer);

	if (data->calc_header_height != NULL)
		data->header_height = data->calc_header_height (
			GTK_HTML (data->engine->widget),
			operation, context, data->user_data);
	else
		data->header_height = 0;

	if (data->calc_footer_height != NULL)
		data->footer_height = data->calc_footer_height (
			GTK_HTML (data->engine->widget),
			operation, context, data->user_data);
	else
		data->footer_height = 0;

	if (data->header_height + data->footer_height >= page_height) {
		data->draw_header = data->draw_footer = NULL;
		g_warning ("Page header height + footer height >= "
			   "page height, disabling header/footer printing");
	}

	/* Calculate page offsets */

	body_height =
		page_height - (data->header_height + data->footer_height);
	document_height = html_engine_get_doc_height (data->engine);

	split_offset = 0;
	g_array_append_val (data->offsets, split_offset);

	do {
		new_split_offset = html_object_check_page_split (
			data->engine->clue, data->painter,
			split_offset + body_height);

		if (new_split_offset <= split_offset ||
		    new_split_offset - split_offset <
		    data->engine->min_split_index * body_height)
			new_split_offset = split_offset + body_height;

		split_offset = new_split_offset;
		g_array_append_val (data->offsets, split_offset);

	} while (split_offset < document_height);

	gtk_print_operation_set_n_pages (operation, data->offsets->len - 1);
}

static void
engine_print_draw_page (GtkPrintOperation *operation,
                        GtkPrintContext *context,
                        gint page_nr,
                        EnginePrintData *data)
{
	HTMLPainter *painter = data->painter;
	HTMLPrinter *printer = HTML_PRINTER (painter);
	PangoRectangle rec;
	gint page_width;
	gint page_height;
	gint body_height;
	gint offset[2];
	cairo_t *cr;

	g_return_if_fail (data->offsets->len > page_nr);
	offset[0] = g_array_index (data->offsets, gint, page_nr + 0);
	offset[1] = g_array_index (data->offsets, gint, page_nr + 1);

	page_width = html_printer_get_page_width (printer);
	page_height = html_printer_get_page_height (printer);
	body_height = offset[1] - offset[0];

	cr = gtk_print_context_get_cairo_context (context);

	html_painter_begin (painter, 0, 0, page_width, page_height);

	/* Draw Header */
	if (data->draw_header != NULL) {
		rec.x = 0;
		rec.y = 0;
		rec.width = page_width;
		rec.height = data->header_height;
		cairo_save (cr);
		html_painter_set_clip_rectangle (
			painter, rec.x, rec.y, rec.width, rec.height);
#ifdef CLIP_DEBUG
		html_painter_draw_rect (
			painter, rec.x, rec.y, rec.width, rec.height);
#endif
		data->draw_header (
			GTK_HTML (data->engine->widget), operation,
			context, page_nr, &rec, data->user_data);
		cairo_restore (cr);
	}

	/* Draw Body */
	rec.x = 0;
	rec.y = data->header_height;
	rec.width = page_width;
	rec.height = body_height;
	cairo_save (cr);
	html_painter_set_clip_rectangle (
		painter, rec.x, rec.y, rec.width, rec.height);
#ifdef CLIP_DEBUG
	html_painter_draw_rect (
		painter, rec.x, rec.y, rec.width, rec.height);
#endif
	html_object_draw (
		data->engine->clue, painter, 0, offset[0], page_width,
		body_height, 0, -offset[0] + data->header_height);
	cairo_restore (cr);

	/* Draw Footer */
	if (data->draw_footer != NULL) {
		rec.x = 0;
		rec.y = page_height - data->footer_height;
		rec.width = page_width;
		rec.height = data->footer_height;
		cairo_save (cr);
		html_painter_set_clip_rectangle (
			painter, rec.x, rec.y, rec.width, rec.height);
#ifdef CLIP_DEBUG
		html_painter_draw_rect (
			painter, rec.x, rec.y, rec.width, rec.height);
#endif
		data->draw_footer (
			GTK_HTML (data->engine->widget), operation,
			context, page_nr, &rec, data->user_data);
		cairo_restore (cr);
	}

	html_painter_end (painter);
}

static void
engine_print_end_print (GtkPrintOperation *operation,
                        GtkPrintContext *context,
                        EnginePrintData *data)
{
	html_engine_set_painter (data->engine, data->old_painter);

	g_object_unref (data->painter);
	g_object_unref (data->old_painter);
	g_array_free (data->offsets, TRUE);
}

GtkPrintOperationResult
html_engine_print_operation_run (HTMLEngine *engine,
                                 GtkPrintOperation *operation,
				 GtkPrintOperationAction action,
				 GtkWindow *parent,
				 GtkHTMLPrintCalcHeight calc_header_height,
				 GtkHTMLPrintCalcHeight calc_footer_height,
                                 GtkHTMLPrintDrawFunc draw_header,
                                 GtkHTMLPrintDrawFunc draw_footer,
                                 gpointer user_data,
				 GError **error)
{
	EnginePrintData data;

	g_return_val_if_fail (
		engine != NULL, GTK_PRINT_OPERATION_RESULT_ERROR);
	g_return_val_if_fail (
		operation != NULL, GTK_PRINT_OPERATION_RESULT_ERROR);

	data.engine = engine;
	data.calc_header_height = calc_header_height;
	data.calc_footer_height = calc_footer_height;
	data.draw_header = draw_header;
	data.draw_footer = draw_footer;
	data.user_data = user_data;

	g_signal_connect (
		operation, "begin-print",
		G_CALLBACK (engine_print_begin_print), &data);
	g_signal_connect (
		operation, "draw-page",
		G_CALLBACK (engine_print_draw_page), &data);
	g_signal_connect (
		operation, "end-print",
		G_CALLBACK (engine_print_end_print), &data);

	return gtk_print_operation_run (operation, action, parent, error);
}
