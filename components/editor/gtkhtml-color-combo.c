/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-color-combo.c
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

#include "gtkhtml-color-combo.h"

/* XXX The "combo" aspects of this widget are based heavily on the
 *     GtkComboBox tree-view implementation.  Consider splitting it
 *     into a reusable "combo-with-an-empty-window" widget. */

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include "gtkhtml-color-swatch.h"

#define NUM_CUSTOM_COLORS	8

#define GTKHTML_COLOR_COMBO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GTKHTML_TYPE_COLOR_COMBO, GtkhtmlColorComboPrivate))

enum {
	PROP_0,
	PROP_CURRENT_COLOR,
	PROP_DEFAULT_COLOR,
	PROP_DEFAULT_LABEL,
	PROP_DEFAULT_TRANSPARENT,
	PROP_PALETTE,
	PROP_POPUP_SHOWN,
	PROP_STATE
};

enum {
	CHANGED,
	POPUP,
	POPDOWN,
	LAST_SIGNAL
};

struct _GtkhtmlColorComboPrivate {
	GtkWidget *color_button;
	GtkWidget *default_button;
	GtkWidget *toggle_button;
	GtkWidget *swatch;
	GtkWidget *window;

	GtkhtmlColorState *state;

	GtkWidget *custom[NUM_CUSTOM_COLORS];

	guint popup_shown	: 1;
	guint popup_in_progress	: 1;
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];
static GdkColor black = { 0, 0, 0, 0 };

static struct {
	const gchar *color;
	const gchar *tooltip;
} default_colors[] = {

	{ "#000000", N_("black") },
	{ "#993300", N_("light brown") },
	{ "#333300", N_("brown gold") },
	{ "#003300", N_("dark green #2") },
	{ "#003366", N_("navy") },
	{ "#000080", N_("dark blue") },
	{ "#333399", N_("purple #2") },
	{ "#333333", N_("very dark gray") },

	{ "#800000", N_("dark red") },
	{ "#FF6600", N_("red-orange") },
	{ "#808000", N_("gold") },
	{ "#008000", N_("dark green") },
	{ "#008080", N_("dull blue") },
	{ "#0000FF", N_("blue") },
	{ "#666699", N_("dull purple") },
	{ "#808080", N_("dark grey") },

	{ "#FF0000", N_("red") },
	{ "#FF9900", N_("orange") },
	{ "#99CC00", N_("lime") },
	{ "#339966", N_("dull green") },
	{ "#33CCCC", N_("dull blue #2") },
	{ "#3366FF", N_("sky blue #2") },
	{ "#800080", N_("purple") },
	{ "#969696", N_("gray") },

	{ "#FF00FF", N_("magenta") },
	{ "#FFCC00", N_("bright orange") },
	{ "#FFFF00", N_("yellow") },
	{ "#00FF00", N_("green") },
	{ "#00FFFF", N_("cyan") },
	{ "#00CCFF", N_("bright blue") },
	{ "#993366", N_("red purple") },
	{ "#C0C0C0", N_("light grey") },

	{ "#FF99CC", N_("pink") },
	{ "#FFCC99", N_("light orange") },
	{ "#FFFF99", N_("light yellow") },
	{ "#CCFFCC", N_("light green") },
	{ "#CCFFFF", N_("light cyan") },
	{ "#99CCFF", N_("light blue") },
	{ "#CC99FF", N_("light purple") },
	{ "#FFFFFF", N_("white") }
};

static void
color_combo_notify_current_color_cb (GtkhtmlColorCombo *combo)
{
	GtkhtmlColorSwatch *swatch;
	GdkColor color;

	swatch = GTKHTML_COLOR_SWATCH (combo->priv->swatch);
	if (gtkhtml_color_combo_get_current_color (combo, &color))
		gtkhtml_color_swatch_set_color (swatch, &color);
	else if (gtkhtml_color_combo_get_default_transparent (combo))
		gtkhtml_color_swatch_set_color (swatch, NULL);
	else
		gtkhtml_color_swatch_set_color (swatch, &color);

	g_signal_emit (G_OBJECT (combo), signals[CHANGED], 0);
}

static void
color_combo_propagate_notify_cb (GtkhtmlColorCombo *combo,
                                 GParamSpec *pspec)
{
	g_object_notify (G_OBJECT (combo), pspec->name);
}

static void
color_combo_reposition_window (GtkhtmlColorCombo *combo)
{
	GdkScreen *screen;
	GdkWindow *window;
	GdkRectangle monitor;
	GtkAllocation allocation;
	gint monitor_num;
	gint x, y, width, height;

	screen = gtk_widget_get_screen (GTK_WIDGET (combo));
	window = gtk_widget_get_window (GTK_WIDGET (combo));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gdk_window_get_origin (window, &x, &y);

	if (!gtk_widget_get_has_window (GTK_WIDGET (combo))) {
		gtk_widget_get_allocation (GTK_WIDGET (combo), &allocation);
		x += allocation.x;
		y += allocation.y;
	}

	gtk_widget_get_allocation (combo->priv->window, &allocation);
	width = allocation.width;
	height = allocation.height;

	x = CLAMP (x, monitor.x, monitor.x + monitor.width - width);
	y = CLAMP (y, monitor.y, monitor.y + monitor.height - height);

	gtk_window_move (GTK_WINDOW (combo->priv->window), x, y);
}

static gboolean
color_combo_button_press_event_cb (GtkhtmlColorCombo *combo,
                                   GdkEventButton *event)
{
	GtkToggleButton *toggle_button;
	GtkWidget *event_widget;

	toggle_button = GTK_TOGGLE_BUTTON (combo->priv->toggle_button);
	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	if (event_widget == combo->priv->window)
		return TRUE;

	if (event_widget != combo->priv->toggle_button)
		return FALSE;

	if (gtk_toggle_button_get_active (toggle_button))
		return FALSE;

	gtkhtml_color_combo_popup (combo);

	combo->priv->popup_in_progress = TRUE;

	return TRUE;
}

static gboolean
color_combo_button_release_event_cb (GtkhtmlColorCombo *combo,
                                     GdkEventButton *event)
{
	GtkToggleButton *toggle_button;
	GtkWidget *event_widget;
	gboolean popup_in_progress;

	toggle_button = GTK_TOGGLE_BUTTON (combo->priv->toggle_button);
	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	popup_in_progress = combo->priv->popup_in_progress;
	combo->priv->popup_in_progress = FALSE;

	if (event_widget != combo->priv->toggle_button)
		goto popdown;

	if (popup_in_progress)
		return FALSE;

	if (gtk_toggle_button_get_active (toggle_button))
		goto popdown;

	return FALSE;

popdown:
	gtkhtml_color_combo_popdown (combo);

	return TRUE;
}

static void
color_combo_child_show_cb (GtkhtmlColorCombo *combo)
{
	combo->priv->popup_shown = TRUE;
	g_object_notify (G_OBJECT (combo), "popup-shown");
}

static void
color_combo_child_hide_cb (GtkhtmlColorCombo *combo)
{
	combo->priv->popup_shown = FALSE;
	g_object_notify (G_OBJECT (combo), "popup-shown");
}

static gboolean
color_combo_child_key_press_event_cb (GtkhtmlColorCombo *combo,
                                      GdkEventKey *event)
{
	GtkWidget *window = combo->priv->window;

	if (!gtk_bindings_activate_event (GTK_OBJECT (window), event))
		gtk_bindings_activate_event (GTK_OBJECT (combo), event);

	return TRUE;
}

static void
color_combo_custom_clicked_cb (GtkhtmlColorCombo *combo)
{
	GtkhtmlColorPalette *palette;
	GtkWidget *dialog;
	GtkWidget *colorsel;
	GtkWidget *toplevel;
	GdkColor color;
	gint response;

	gtkhtml_color_combo_popdown (combo);

	dialog = gtk_color_selection_dialog_new (_("Choose Custom Color"));
	colorsel = gtk_color_selection_dialog_get_color_selection (
		GTK_COLOR_SELECTION_DIALOG (dialog));
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));

	if (gtk_widget_is_toplevel (toplevel))
		gtk_window_set_transient_for (
			GTK_WINDOW (dialog), GTK_WINDOW (toplevel));

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK)
		goto exit;

	palette = gtkhtml_color_state_get_palette (combo->priv->state);

	gtk_color_selection_get_current_color (
		GTK_COLOR_SELECTION (colorsel), &color);
	gtkhtml_color_palette_add_color (palette, &color);
	gtkhtml_color_combo_set_current_color (combo, &color);

exit:
	gtk_widget_destroy (dialog);
}

static gboolean
color_combo_custom_release_event_cb (GtkhtmlColorCombo *combo,
                                     GdkEventButton *event,
                                     GtkButton *button)
{
	gtk_button_clicked (button);

	return FALSE;
}

static void
color_combo_default_clicked_cb (GtkhtmlColorCombo *combo)
{
	gtkhtml_color_combo_set_current_color (combo, NULL);
	gtkhtml_color_combo_popdown (combo);
}

static gboolean
color_combo_default_release_event_cb (GtkhtmlColorCombo *combo,
                                      GdkEventButton *event,
                                      GtkButton *button)
{
	GtkStateType state;

	state = gtk_widget_get_state (GTK_WIDGET (button));

	if (state != GTK_STATE_NORMAL)
		gtk_button_clicked (button);

	return FALSE;
}

static void
color_combo_palette_changed_cb (GtkhtmlColorCombo *combo)
{
	GtkhtmlColorPalette *palette;
	GtkhtmlColorSwatch *swatch;
	GSList *list;
	guint ii;

	palette = gtkhtml_color_state_get_palette (combo->priv->state);
	list = gtkhtml_color_palette_list_colors (palette);

	/* Make sure we have at least NUM_CUSTOM_COLORS. */
	for (ii = g_slist_length (list); ii < NUM_CUSTOM_COLORS; ii++)
		list = g_slist_append (list, gdk_color_copy (&black));

	/* Pop items off the list and update the custom swatches. */
	for (ii = 0; ii < NUM_CUSTOM_COLORS; ii++) {
		swatch = GTKHTML_COLOR_SWATCH (combo->priv->custom[ii]);
		gtkhtml_color_swatch_set_color (swatch, list->data);
		gdk_color_free (list->data);
		list = g_slist_delete_link (list, list);
	}

	/* Delete any remaining list items. */
	g_slist_foreach (list, (GFunc) gdk_color_free, NULL);
	g_slist_free (list);
}

static void
color_combo_swatch_clicked_cb (GtkhtmlColorCombo *combo,
                               GtkButton *button)
{
	GtkWidget *swatch;
	GdkColor color;

	swatch = gtk_bin_get_child (GTK_BIN (button));
	gtkhtml_color_swatch_get_color (
		GTKHTML_COLOR_SWATCH (swatch), &color);
	gtkhtml_color_state_set_current_color (combo->priv->state, &color);
	gtkhtml_color_combo_popdown (combo);
}

static gboolean
color_combo_swatch_release_event_cb (GtkhtmlColorCombo *combo,
                                     GdkEventButton *event,
                                     GtkButton *button)
{
	GtkStateType state;

	state = gtk_widget_get_state (GTK_WIDGET (button));

	if (state != GTK_STATE_NORMAL)
		gtk_button_clicked (button);

	return FALSE;
}

static void
color_combo_toggled_cb (GtkhtmlColorCombo *combo)
{
	GtkToggleButton *toggle_button;
	gboolean active;

	toggle_button = GTK_TOGGLE_BUTTON (combo->priv->toggle_button);
	active = gtk_toggle_button_get_active (toggle_button);

	if (active)
		gtkhtml_color_combo_popup (combo);
	else
		gtkhtml_color_combo_popdown (combo);
}

static GtkWidget *
color_combo_new_swatch_button (GtkhtmlColorCombo *combo,
                               const gchar *tooltip,
                               GdkColor *color)
{
	GtkWidget *button;
	GtkWidget *swatch;

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (button, tooltip);
	gtk_widget_show (button);

	swatch = gtkhtml_color_swatch_new ();
	gtkhtml_color_swatch_set_color (
		GTKHTML_COLOR_SWATCH (swatch), color);
	gtkhtml_color_swatch_set_shadow_type (
		GTKHTML_COLOR_SWATCH (swatch), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (button), swatch);
	gtk_widget_show (swatch);

	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (color_combo_swatch_clicked_cb), combo);
	g_signal_connect_swapped (
		button, "button-release-event",
		G_CALLBACK (color_combo_swatch_release_event_cb), combo);

	return button;
}

static void
color_combo_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_COLOR:
			gtkhtml_color_combo_set_current_color (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_COLOR:
			gtkhtml_color_combo_set_default_color (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_LABEL:
			gtkhtml_color_combo_set_default_label (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_string (value));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			gtkhtml_color_combo_set_default_transparent (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_boolean (value));
			return;

		case PROP_PALETTE:
			gtkhtml_color_combo_set_palette (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_object (value));
			return;

		case PROP_POPUP_SHOWN:
			if (g_value_get_boolean (value))
				gtkhtml_color_combo_popup (
					GTKHTML_COLOR_COMBO (object));
			else
				gtkhtml_color_combo_popdown (
					GTKHTML_COLOR_COMBO (object));
			return;

		case PROP_STATE:
			gtkhtml_color_combo_set_state (
				GTKHTML_COLOR_COMBO (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_combo_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	GtkhtmlColorComboPrivate *priv;
	GdkColor color;

	priv = GTKHTML_COLOR_COMBO_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CURRENT_COLOR:
			gtkhtml_color_combo_get_current_color (
				GTKHTML_COLOR_COMBO (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_COLOR:
			gtkhtml_color_combo_get_default_color (
				GTKHTML_COLOR_COMBO (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_LABEL:
			g_value_set_string (
				value, gtkhtml_color_combo_get_default_label (
				GTKHTML_COLOR_COMBO (object)));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			g_value_set_boolean (
				value,
				gtkhtml_color_combo_get_default_transparent (
				GTKHTML_COLOR_COMBO (object)));
			return;

		case PROP_PALETTE:
			g_value_set_object (
				value, gtkhtml_color_combo_get_palette (
				GTKHTML_COLOR_COMBO (object)));
			return;

		case PROP_POPUP_SHOWN:
			g_value_set_boolean (value, priv->popup_shown);
			return;

		case PROP_STATE:
			g_value_set_object (
				value, gtkhtml_color_combo_get_state (
				GTKHTML_COLOR_COMBO (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_combo_dispose (GObject *object)
{
	GtkhtmlColorComboPrivate *priv;
	gint ii;

	priv = GTKHTML_COLOR_COMBO_GET_PRIVATE (object);

	if (priv->color_button != NULL) {
		g_object_unref (priv->color_button);
		priv->color_button = NULL;
	}

	if (priv->default_button != NULL) {
		g_object_unref (priv->default_button);
		priv->default_button = NULL;
	}

	if (priv->toggle_button != NULL) {
		g_object_unref (priv->toggle_button);
		priv->toggle_button = NULL;
	}

	if (priv->swatch != NULL) {
		g_object_unref (priv->swatch);
		priv->swatch = NULL;
	}

	if (priv->window != NULL) {
		g_object_unref (priv->window);
		priv->window = NULL;
	}

	if (priv->state != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->state, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->state);
		priv->state = NULL;
	}

	for (ii = 0; ii < NUM_CUSTOM_COLORS; ii++) {
		if (priv->custom[ii] != NULL) {
			g_object_unref (priv->custom[ii]);
			priv->custom[ii] = NULL;
		}
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
color_combo_size_request (GtkWidget *widget,
                          GtkRequisition *requisition)
{
	GtkhtmlColorComboPrivate *priv;

	priv = GTKHTML_COLOR_COMBO_GET_PRIVATE (widget);

	gtk_widget_size_request (priv->toggle_button, requisition);
}

static void
color_combo_size_allocate (GtkWidget *widget,
                           GtkAllocation *allocation)
{
	GtkhtmlColorComboPrivate *priv;

	priv = GTKHTML_COLOR_COMBO_GET_PRIVATE (widget);

	gtk_widget_set_allocation (widget, allocation);
	gtk_widget_size_allocate (priv->toggle_button, allocation);
}

static void
color_combo_popup (GtkhtmlColorCombo *combo)
{
	GtkToggleButton *toggle_button;
	GtkButton *button;
	GdkWindow *window;
	GdkGrabStatus status;
	const gchar *label;

	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (combo->priv->popup_shown)
		return;

	/* Update the default button label. */
	button = GTK_BUTTON (combo->priv->default_button);
	label = gtkhtml_color_state_get_default_label (combo->priv->state);
	gtk_button_set_label (button, label);

	/* Position the window over the button. */
	color_combo_reposition_window (combo);

	/* Show the pop-up. */
	gtk_widget_show (combo->priv->window);
	gtk_widget_grab_focus (combo->priv->window);

	/* Activate the toggle button. */
	toggle_button = GTK_TOGGLE_BUTTON (combo->priv->toggle_button);
	gtk_toggle_button_set_active (toggle_button, TRUE);

	/* Try to grab the pointer and keyboard. */
	window = gtk_widget_get_window (combo->priv->window);
	status = gdk_pointer_grab (
		window, TRUE,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK, NULL, NULL, GDK_CURRENT_TIME);
	if (status == GDK_GRAB_SUCCESS) {
		status = gdk_keyboard_grab (window, TRUE, GDK_CURRENT_TIME);
		if (status != GDK_GRAB_SUCCESS)
			gdk_display_pointer_ungrab (
				gdk_drawable_get_display (window),
				GDK_CURRENT_TIME);
	}

	if (status == GDK_GRAB_SUCCESS)
		gtk_grab_add (combo->priv->window);
	else
		gtk_widget_hide (combo->priv->window);
}

static void
color_combo_popdown (GtkhtmlColorCombo *combo)
{
	GtkToggleButton *toggle_button;

	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (!combo->priv->popup_shown)
		return;

	/* Hide the pop-up. */
	gtk_grab_remove (combo->priv->window);
	gtk_widget_hide (combo->priv->window);

	/* Deactivate the toggle button. */
	toggle_button = GTK_TOGGLE_BUTTON (combo->priv->toggle_button);
	gtk_toggle_button_set_active (toggle_button, FALSE);
}

static void
color_combo_class_init (GtkhtmlColorComboClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (GtkhtmlColorComboPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = color_combo_set_property;
	object_class->get_property = color_combo_get_property;
	object_class->dispose = color_combo_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = color_combo_size_request;
	widget_class->size_allocate = color_combo_size_allocate;

	class->popup = color_combo_popup;
	class->popdown = color_combo_popdown;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_COLOR,
		g_param_spec_boxed (
			"current-color",
			"Current color",
			"The currently selected color",
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_COLOR,
		g_param_spec_boxed (
			"default-color",
			"Default color",
			"The color associated with the default button",
			GDK_TYPE_COLOR,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_LABEL,
		g_param_spec_string (
			"default-label",
			"Default label",
			"The label for the default button",
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

	g_object_class_install_property (
		object_class,
		PROP_POPUP_SHOWN,
		g_param_spec_boolean (
			"popup-shown",
			"Popup shown",
			"Whether the combo's dropdown is shown",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_object (
			"state",
			"Color state",
			"The state of a color combo box",
			GTKHTML_TYPE_COLOR_STATE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPUP] = g_signal_new (
		"popup",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GtkhtmlColorComboClass, popup),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPDOWN] = g_signal_new (
		"popdown",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GtkhtmlColorComboClass, popdown),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_Down, GDK_MOD1_MASK, "popup", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KP_Down, GDK_MOD1_MASK, "popup", 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KP_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_Escape, 0, "popdown", 0);
}

static void
color_combo_init (GtkhtmlColorCombo *combo)
{
	GtkhtmlColorState *state;
	GtkWidget *toplevel;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *window;
	guint ii;

	combo->priv = GTKHTML_COLOR_COMBO_GET_PRIVATE (combo);

	state = gtkhtml_color_state_new ();
	gtkhtml_color_combo_set_state (combo, state);
	g_object_unref (state);

	/* Build the combo button. */

	widget = gtk_toggle_button_new ();
	gtk_container_add (GTK_CONTAINER (combo), widget);
	combo->priv->toggle_button = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtkhtml_color_swatch_new ();
	gtkhtml_color_swatch_set_shadow_type (
		GTKHTML_COLOR_SWATCH (widget), GTK_SHADOW_ETCHED_OUT);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	combo->priv->swatch = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_vseparator_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 3);
	gtk_widget_show (widget);

	widget = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		combo->priv->toggle_button, "button-press-event",
		G_CALLBACK (color_combo_button_press_event_cb), combo);

	g_signal_connect_swapped (
		combo->priv->toggle_button, "toggled",
		G_CALLBACK (color_combo_toggled_cb), combo);

	/* Build the pop-up window. */

	window = gtk_window_new (GTK_WINDOW_POPUP);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_type_hint (
		GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_COMBO);
	if (gtk_widget_is_toplevel (toplevel)) {
		gtk_window_group_add_window (
			gtk_window_get_group (GTK_WINDOW (toplevel)),
			GTK_WINDOW (window));
		gtk_window_set_transient_for (
			GTK_WINDOW (window), GTK_WINDOW (toplevel));
	}
	combo->priv->window = g_object_ref (window);

	g_signal_connect_swapped (
		window, "show",
		G_CALLBACK (color_combo_child_show_cb), combo);
	g_signal_connect_swapped (
		window, "hide",
		G_CALLBACK (color_combo_child_hide_cb), combo);
	g_signal_connect_swapped (
		window, "button-press-event",
		G_CALLBACK (color_combo_button_press_event_cb), combo);
	g_signal_connect_swapped (
		window, "button-release-event",
		G_CALLBACK (color_combo_button_release_event_cb), combo);
	g_signal_connect_swapped (
		window, "key-press-event",
		G_CALLBACK (color_combo_child_key_press_event_cb), combo);

	/* Build the pop-up window contents. */

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_table_new (5, 8, TRUE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 0);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new ();
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 8, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	combo->priv->default_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (color_combo_default_clicked_cb), combo);
	g_signal_connect_swapped (
		widget, "button-release-event",
		G_CALLBACK (color_combo_default_release_event_cb), combo);

	for (ii = 0; ii < G_N_ELEMENTS (default_colors); ii++) {
		guint left = ii % 8;
		guint top = ii / 8 + 1;
		GdkColor color;
		const gchar *tooltip;

		tooltip = gettext (default_colors[ii].tooltip);
		gdk_color_parse (default_colors[ii].color, &color);
		widget = color_combo_new_swatch_button (
			combo, tooltip, &color);
		gtk_table_attach (
			GTK_TABLE (container), widget,
			left, left + 1, top, top + 1, 0, 0, 0, 0);
	}

	for (ii = 0; ii < NUM_CUSTOM_COLORS; ii++) {
		widget = color_combo_new_swatch_button (
			combo, _("custom"), &black);
		gtk_table_attach (
			GTK_TABLE (container), widget,
			ii, ii + 1, 6, 7, 0, 0, 0, 0);
		combo->priv->custom[ii] = g_object_ref (
			gtk_bin_get_child (GTK_BIN (widget)));
	}

	widget = gtk_button_new_with_label (_("Custom Color..."));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_SELECT_COLOR, GTK_ICON_SIZE_BUTTON));
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 8, 7, 8, GTK_FILL, 0, 0, 0);
	combo->priv->color_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (color_combo_custom_clicked_cb), combo);
	g_signal_connect_swapped (
		widget, "button-release-event",
		G_CALLBACK (color_combo_custom_release_event_cb), combo);
}

GType
gtkhtml_color_combo_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GtkhtmlColorComboClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) color_combo_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (GtkhtmlColorCombo),
			0,     /* n_preallocs */
			(GInstanceInitFunc) color_combo_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "GtkhtmlColorCombo", &type_info, 0);
	}

	return type;
}

GtkWidget *
gtkhtml_color_combo_new (void)
{
	return g_object_new (GTKHTML_TYPE_COLOR_COMBO, NULL);
}

GtkWidget *
gtkhtml_color_combo_new_defaults (GdkColor *default_color,
                                  const gchar *default_label)
{
	g_return_val_if_fail (default_color != NULL, NULL);
	g_return_val_if_fail (default_label != NULL, NULL);

	return g_object_new (
		GTKHTML_TYPE_COLOR_COMBO,
		"default-color", default_color,
		"default-label", default_label,
		NULL);
}

void
gtkhtml_color_combo_popup (GtkhtmlColorCombo *combo)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	g_signal_emit (combo, signals[POPUP], 0);
}

void
gtkhtml_color_combo_popdown (GtkhtmlColorCombo *combo)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	g_signal_emit (combo, signals[POPDOWN], 0);
}

gboolean
gtkhtml_color_combo_get_current_color (GtkhtmlColorCombo *combo,
                                       GdkColor *color)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_COMBO (combo), FALSE);

	return gtkhtml_color_state_get_current_color (
		combo->priv->state, color);
}

void
gtkhtml_color_combo_set_current_color (GtkhtmlColorCombo *combo,
                                       GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_set_current_color (combo->priv->state, color);
}

void
gtkhtml_color_combo_get_default_color (GtkhtmlColorCombo *combo,
                                       GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_get_default_color (combo->priv->state, color);
}

void
gtkhtml_color_combo_set_default_color (GtkhtmlColorCombo *combo,
                                       GdkColor *color)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_set_default_color (combo->priv->state, color);
}

const gchar *
gtkhtml_color_combo_get_default_label (GtkhtmlColorCombo *combo)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_COMBO (combo), NULL);

	return gtkhtml_color_state_get_default_label (combo->priv->state);
}

void
gtkhtml_color_combo_set_default_label (GtkhtmlColorCombo *combo,
                                       const gchar *text)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_set_default_label (combo->priv->state, text);
}

gboolean
gtkhtml_color_combo_get_default_transparent (GtkhtmlColorCombo *combo)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_COMBO (combo), FALSE);

	return gtkhtml_color_state_get_default_transparent (combo->priv->state);
}

void
gtkhtml_color_combo_set_default_transparent (GtkhtmlColorCombo *combo,
                                             gboolean transparent)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_set_default_transparent (
		combo->priv->state, transparent);
}

GtkhtmlColorPalette *
gtkhtml_color_combo_get_palette (GtkhtmlColorCombo *combo)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_COMBO (combo), NULL);

	return gtkhtml_color_state_get_palette (combo->priv->state);
}

void
gtkhtml_color_combo_set_palette (GtkhtmlColorCombo *combo,
                                 GtkhtmlColorPalette *palette)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	gtkhtml_color_state_set_palette (combo->priv->state, palette);
}

GtkhtmlColorState *
gtkhtml_color_combo_get_state (GtkhtmlColorCombo *combo)
{
	g_return_val_if_fail (GTKHTML_IS_COLOR_COMBO (combo), NULL);

	return combo->priv->state;
}

void
gtkhtml_color_combo_set_state (GtkhtmlColorCombo *combo,
                               GtkhtmlColorState *state)
{
	g_return_if_fail (GTKHTML_IS_COLOR_COMBO (combo));

	if (state == NULL)
		state = gtkhtml_color_state_new ();
	else
		g_return_if_fail (GTKHTML_IS_COLOR_STATE (state));

	if (combo->priv->state != NULL) {
		g_signal_handlers_disconnect_matched (
			combo->priv->state, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, combo);
		g_object_unref (combo->priv->state);
	}

	combo->priv->state = g_object_ref (state);

	/* Listen and respond to changes in the state object. */

	g_signal_connect_swapped (
		combo->priv->state, "notify::current-color",
		G_CALLBACK (color_combo_notify_current_color_cb), combo);

	g_signal_connect_swapped (
		combo->priv->state, "palette-changed",
		G_CALLBACK (color_combo_palette_changed_cb), combo);

	/* Propagate all "notify" signals through our own properties. */

	g_signal_connect_swapped (
		combo->priv->state, "notify",
		G_CALLBACK (color_combo_propagate_notify_cb), combo);
}
