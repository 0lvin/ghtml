/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-color-state.c
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

#include "gtkhtml-color-state.h"

#include <glib/gi18n-lib.h>

#define GTKHTML_COLOR_STATE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GTKHTML_TYPE_COLOR_STATE, GtkhtmlColorStatePrivate))

enum {
	PROP_0,
	PROP_CURRENT_COLOR,
	PROP_DEFAULT_COLOR,
	PROP_DEFAULT_LABEL,
	PROP_DEFAULT_TRANSPARENT,
	PROP_PALETTE
};

enum {
	PALETTE_CHANGED,
	LAST_SIGNAL
};

struct _GtkhtmlColorStatePrivate {
	GdkColor *current_color;
	GdkColor *default_color;
	gchar *default_label;
	GtkhtmlColorPalette *palette;
	gulong palette_handler_id;
	gboolean default_transparent;
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];
static GdkColor black = { 0, 0, 0, 0 };

static void
color_state_palette_changed_cb (GtkhtmlColorState *state)
{
	/* Propagate the signal. */
	g_signal_emit (state, signals[PALETTE_CHANGED], 0);
}

static void
color_state_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_COLOR:
			gtkhtml_color_state_set_current_color (
				GTKHTML_COLOR_STATE (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_COLOR:
			gtkhtml_color_state_set_default_color (
				GTKHTML_COLOR_STATE (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_LABEL:
			gtkhtml_color_state_set_default_label (
				GTKHTML_COLOR_STATE (object),
				g_value_get_string (value));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			gtkhtml_color_state_set_default_transparent (
				GTKHTML_COLOR_STATE (object),
				g_value_get_boolean (value));
			return;

		case PROP_PALETTE:
			gtkhtml_color_state_set_palette (
				GTKHTML_COLOR_STATE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_state_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	GdkColor color;

	switch (property_id) {
		case PROP_CURRENT_COLOR:
			gtkhtml_color_state_get_current_color (
				GTKHTML_COLOR_STATE (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_COLOR:
			gtkhtml_color_state_get_default_color (
				GTKHTML_COLOR_STATE (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_LABEL:
			g_value_set_string (
				value, gtkhtml_color_state_get_default_label (
				GTKHTML_COLOR_STATE (object)));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			g_value_set_boolean (
				value,
				gtkhtml_color_state_get_default_transparent (
				GTKHTML_COLOR_STATE (object)));
			return;

		case PROP_PALETTE:
			g_value_set_object (
				value, gtkhtml_color_state_get_palette (
				GTKHTML_COLOR_STATE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_state_dispose (GObject *object)
{
	GtkhtmlColorStatePrivate *priv;

	priv = GTKHTML_COLOR_STATE_GET_PRIVATE (object);

	if (priv->palette != NULL) {
		g_signal_handler_disconnect (
			priv->palette,
			priv->palette_handler_id);
		g_object_unref (priv->palette);
		priv->palette = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
color_state_finalize (GObject *object)
{
	GtkhtmlColorStatePrivate *priv;

	priv = GTKHTML_COLOR_STATE_GET_PRIVATE (object);

	if (priv->current_color != NULL)
		gdk_color_free (priv->current_color);

	if (priv->default_color != NULL)
		gdk_color_free (priv->default_color);

	g_free (priv->default_label);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
color_state_class_init (GtkhtmlColorStateClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (GtkhtmlColorStatePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = color_state_set_property;
	object_class->get_property = color_state_get_property;
	object_class->dispose = color_state_dispose;
	object_class->finalize = color_state_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_COLOR,
		g_param_spec_boxed (
			"current-color",
			"Current color",
			"The current color",
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_COLOR,
		g_param_spec_boxed (
			"default-color",
			"Default color",
			"The default color",
			GDK_TYPE_COLOR,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_LABEL,
		g_param_spec_string (
			"default-label",
			"Default label",
			"Description of the default color",
			_("Default"),
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_TRANSPARENT,
		g_param_spec_boolean (
			"default-transparent",
			"Default is transparent",
			"Whether the default color is transparent",
			FALSE,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PALETTE,
		g_param_spec_object (
			"palette",
			"Color palette",
			"Custom color palette",
			GTKHTML_TYPE_COLOR_PALETTE,
			G_PARAM_READWRITE));

	signals[PALETTE_CHANGED] = g_signal_new (
		"palette-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
color_state_init (GtkhtmlColorState *state)
{
	GtkhtmlColorPalette *palette;

	state->priv = GTKHTML_COLOR_STATE_GET_PRIVATE (state);

	palette = gtkhtml_color_palette_new ();
	gtkhtml_color_state_set_palette (state, palette);
	g_object_unref (palette);
}

GType
gtkhtml_color_state_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GtkhtmlColorStateClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) color_state_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (GtkhtmlColorState),
			0,     /* n_preallocs */
			(GInstanceInitFunc) color_state_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "GtkhtmlColorState", &type_info, 0);
	}

	return type;
}

GtkhtmlColorState *
gtkhtml_color_state_new (void)
{
	return g_object_new (GTKHTML_TYPE_COLOR_STATE, NULL);
}

GtkhtmlColorState *
gtkhtml_color_state_new_default (GdkColor *default_color,
                                 const gchar *default_label)
{
	g_return_val_if_fail (default_color != NULL, NULL);
	g_return_val_if_fail (default_label != NULL, NULL);

	return g_object_new (
		GTKHTML_TYPE_COLOR_STATE,
		"default-color", default_color,
		"default-label", default_label);
}

gboolean
gtkhtml_color_state_get_current_color (GtkhtmlColorState *state,
                                       GdkColor *color)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_STATE (state), FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	if (state->priv->current_color != NULL) {
		color->red   = state->priv->current_color->red;
		color->green = state->priv->current_color->green;
		color->blue  = state->priv->current_color->blue;
		return TRUE;
	} else {
		color->red   = state->priv->default_color->red;
		color->green = state->priv->default_color->green;
		color->blue  = state->priv->default_color->blue;
		return FALSE;
	}
}

void
gtkhtml_color_state_set_current_color (GtkhtmlColorState *state,
                                       const GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));

	if (state->priv->current_color != NULL) {
		gdk_color_free (state->priv->current_color);
		state->priv->current_color = NULL;
	}

	if (color != NULL)
		state->priv->current_color = gdk_color_copy (color);

	g_object_notify (G_OBJECT (state), "current-color");
}

void
gtkhtml_color_state_get_default_color (GtkhtmlColorState *state,
                                       GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));
	g_return_if_fail (color != NULL);

	color->red   = state->priv->default_color->red;
	color->green = state->priv->default_color->green;
	color->blue  = state->priv->default_color->blue;
}

void
gtkhtml_color_state_set_default_color (GtkhtmlColorState *state,
                                       const GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));

	if (state->priv->default_color != NULL) {
		gdk_color_free (state->priv->default_color);
		state->priv->default_color = NULL;
	}

	/* Default to black. */
	if (color == NULL)
		color = &black;

	state->priv->default_color = gdk_color_copy (color);

	g_object_notify (G_OBJECT (state), "default-color");

	/* If the current color is deferring to the default color, then
	 * changing the default color also changes the current color. */
	if (state->priv->current_color == NULL)
		g_object_notify (G_OBJECT (state), "current-color");
}

const gchar *
gtkhtml_color_state_get_default_label (GtkhtmlColorState *state)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_STATE (state), NULL);

	return state->priv->default_label;
}

void
gtkhtml_color_state_set_default_label (GtkhtmlColorState *state,
                                       const gchar *text)
{
	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));
	g_return_if_fail (text != NULL);

	g_free (state->priv->default_label);
	state->priv->default_label = g_strdup (text);

	g_object_notify (G_OBJECT (state), "default-label");
}

gboolean
gtkhtml_color_state_get_default_transparent (GtkhtmlColorState *state)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_STATE (state), FALSE);

	return state->priv->default_transparent;
}

void
gtkhtml_color_state_set_default_transparent (GtkhtmlColorState *state,
                                             gboolean transparent)
{
	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));

	state->priv->default_transparent = transparent;

	g_object_notify (G_OBJECT (state), "default-transparent");
}

GtkhtmlColorPalette *
gtkhtml_color_state_get_palette (GtkhtmlColorState *state)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_STATE (state), NULL);

	return state->priv->palette;
}

void
gtkhtml_color_state_set_palette (GtkhtmlColorState *state,
                                 GtkhtmlColorPalette *palette)
{
	gulong handler_id;

	g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));

	if (palette == NULL)
		palette = gtkhtml_color_palette_new ();
	else
		g_return_if_fail (GTKHTML_IS_COLOR_PALETTE (palette));

	if (state->priv->palette != NULL) {
		g_signal_handler_disconnect (
			state->priv->palette,
			state->priv->palette_handler_id);
		g_object_unref (state->priv->palette);
	}

	handler_id = g_signal_connect_swapped (
		palette, "changed",
		G_CALLBACK (color_state_palette_changed_cb), state);

	state->priv->palette = g_object_ref (palette);
	state->priv->palette_handler_id = handler_id;

	g_object_notify (G_OBJECT (state), "palette");
}
