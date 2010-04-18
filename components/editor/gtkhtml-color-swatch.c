/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-color-swatch.c
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkhtml-color-swatch.h"

#include <glib/gi18n-lib.h>

#define GTKHTML_COLOR_SWATCH_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GTKHTML_TYPE_COLOR_SWATCH, GtkhtmlColorSwatchPrivate))

enum {
	PROP_0,
	PROP_COLOR,
	PROP_SHADOW_TYPE
};

struct _GtkhtmlColorSwatchPrivate {
	GtkWidget *drawing_area;
	GtkWidget *frame;
};

static gpointer parent_class;

static gboolean
color_swatch_expose_cb (GtkWidget *drawing_area,
                        GdkEventExpose *event)
{
	GtkStyle *style;
	GdkWindow *window;
	GdkColor *color;
	cairo_t *cr;

	style = gtk_widget_get_style (drawing_area);
	window = gtk_widget_get_window (drawing_area);

	if (window == NULL)
		return FALSE;

	cr = gdk_cairo_create (window);

	color = &style->bg[GTK_STATE_NORMAL];
	gdk_cairo_set_source_color (cr, color);
	gdk_cairo_rectangle (cr, &event->area);
	cairo_fill (cr);

	cairo_destroy (cr);

	return FALSE;
}

static void
color_swatch_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COLOR:
			gtkhtml_color_swatch_set_color (
				GTKHTML_COLOR_SWATCH (object),
				g_value_get_boxed (value));
			return;

		case PROP_SHADOW_TYPE:
			gtkhtml_color_swatch_set_shadow_type (
				GTKHTML_COLOR_SWATCH (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_swatch_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	GdkColor color;

	switch (property_id) {
		case PROP_COLOR:
			gtkhtml_color_swatch_get_color (
				GTKHTML_COLOR_SWATCH (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_SHADOW_TYPE:
			g_value_set_enum (
				value, gtkhtml_color_swatch_get_shadow_type (
				GTKHTML_COLOR_SWATCH (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_swatch_dispose (GObject *object)
{
	GtkhtmlColorSwatch *swatch = GTKHTML_COLOR_SWATCH (object);

	if (swatch->priv->drawing_area != NULL) {
		g_object_unref (swatch->priv->drawing_area);
		swatch->priv->drawing_area = NULL;
	}

	if (swatch->priv->frame != NULL) {
		g_object_unref (swatch->priv->frame);
		swatch->priv->frame = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
color_swatch_size_request (GtkWidget *widget,
                           GtkRequisition *requisition)
{
	GtkhtmlColorSwatchPrivate *priv;

	priv = GTKHTML_COLOR_SWATCH_GET_PRIVATE (widget);

	gtk_widget_size_request (priv->frame, requisition);
}

static void
color_swatch_size_allocate (GtkWidget *widget,
                            GtkAllocation *allocation)
{
	GtkhtmlColorSwatchPrivate *priv;

	priv = GTKHTML_COLOR_SWATCH_GET_PRIVATE (widget);

	gtk_widget_set_allocation (widget, allocation);
	gtk_widget_size_allocate (priv->frame, allocation);
}

static void
color_swatch_class_init (GtkhtmlColorSwatchClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (GtkhtmlColorSwatchPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = color_swatch_set_property;
	object_class->get_property = color_swatch_get_property;
	object_class->dispose = color_swatch_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = color_swatch_size_request;
	widget_class->size_allocate = color_swatch_size_allocate;

	g_object_class_install_property (
		object_class,
		PROP_COLOR,
		g_param_spec_boxed (
			"color",
			"Color",
			"The current color",
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHADOW_TYPE,
		g_param_spec_enum (
			"shadow-type",
			"Frame Shadow",
			"Appearance of the frame border",
			GTK_TYPE_SHADOW_TYPE,
			GTK_SHADOW_ETCHED_IN,
			G_PARAM_READWRITE));
}

static void
color_swatch_init (GtkhtmlColorSwatch *swatch)
{
	GtkWidget *container;
	GtkWidget *widget;

	swatch->priv = GTKHTML_COLOR_SWATCH_GET_PRIVATE (swatch);

	widget = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (swatch), widget);
	swatch->priv->frame = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_drawing_area_new ();
	gtk_widget_set_size_request (widget, 16, 16);
	gtk_container_add (GTK_CONTAINER (container), widget);
	swatch->priv->drawing_area = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "expose-event",
		G_CALLBACK (color_swatch_expose_cb), swatch);
}

GType
gtkhtml_color_swatch_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GtkhtmlColorSwatchClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) color_swatch_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (GtkhtmlColorSwatch),
			0,     /* n_preallocs */
			(GInstanceInitFunc) color_swatch_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "GtkhtmlColorSwatch", &type_info, 0);
	}

	return type;
}

GtkWidget *
gtkhtml_color_swatch_new (void)
{
	return g_object_new (GTKHTML_TYPE_COLOR_SWATCH, NULL);
}

void
gtkhtml_color_swatch_get_color (GtkhtmlColorSwatch *swatch,
                                GdkColor *color)
{
	GtkStyle *style;
	GtkWidget *drawing_area;

	g_return_if_fail (GTKHTML_IS_COLOR_SWATCH (swatch));
	g_return_if_fail (color != NULL);

	drawing_area = swatch->priv->drawing_area;
	style = gtk_widget_get_style (drawing_area);
	color->red   = style->bg[GTK_STATE_NORMAL].red;
	color->green = style->bg[GTK_STATE_NORMAL].green;
	color->blue  = style->bg[GTK_STATE_NORMAL].blue;
}

void
gtkhtml_color_swatch_set_color (GtkhtmlColorSwatch *swatch,
                                const GdkColor *color)
{
	GtkWidget *drawing_area;

	g_return_if_fail (GTKHTML_IS_COLOR_SWATCH (swatch));

	drawing_area = swatch->priv->drawing_area;
	gtk_widget_modify_bg (drawing_area, GTK_STATE_NORMAL, color);

	g_object_notify (G_OBJECT (swatch), "color");
}

GtkShadowType
gtkhtml_color_swatch_get_shadow_type (GtkhtmlColorSwatch *swatch)
{
	GtkFrame *frame;

	g_return_val_if_fail (GTKHTML_IS_COLOR_SWATCH (swatch), 0);

	frame = GTK_FRAME (swatch->priv->frame);

	return gtk_frame_get_shadow_type (frame);
}

void
gtkhtml_color_swatch_set_shadow_type (GtkhtmlColorSwatch *swatch,
                                      GtkShadowType shadow_type)
{
	GtkFrame *frame;

	g_return_if_fail (GTKHTML_IS_COLOR_SWATCH (swatch));

	frame = GTK_FRAME (swatch->priv->frame);
	gtk_frame_set_shadow_type (frame, shadow_type);

	g_object_notify (G_OBJECT (swatch), "shadow-type");
}
