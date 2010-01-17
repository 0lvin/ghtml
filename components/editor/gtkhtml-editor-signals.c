/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-editor-signals.c
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

/* This file is for widget signal handlers, primarily for property dialogs.
 * Most of these handlers will be auto-connected by libglade.  GtkAction
 * callbacks belong in gtkhtml-editor-actions.c. */

#include "gtkhtml-editor-private.h"

/* Think of this as a Python decorator.  It silences compiler
 * warnings about "no previous prototype for 'function_name'". */
#define AUTOCONNECTED_SIGNAL_HANDLER(x) x; x

enum {
	SIZE_UNIT_PX,
	SIZE_UNIT_PERCENT,
	SIZE_UNIT_FOLLOW
};

/* Dialogs are transient windows for the main editor window.  So
 * given a dialog window we can extract the main editor window. */
static GtkhtmlEditor *
extract_gtkhtml_editor (GtkWidget *window)
{
	GtkhtmlEditor *editor;

	g_object_get (window, "transient-for", &editor, NULL);
	g_assert (GTKHTML_IS_EDITOR (editor));

	return editor;
}

/*****************************************************************************
 * Cell Properties Window
 *****************************************************************************/

typedef void (*CellCallback) (GtkhtmlEditor *editor,
                              HTMLTableCell *cell,
                              GtkWidget *widget);

static void
cell_properties_set_column (GtkhtmlEditor *editor,
                            CellCallback callback,
                            GtkWidget *widget)
{
	GtkHTML *html;
	HTMLObject *parent;
	HTMLTableCell *cell;
	HTMLTableCell *iter;

	html = gtkhtml_editor_get_html (editor);
	cell = HTML_TABLE_CELL (editor->priv->cell_object);
	parent = editor->priv->cell_parent;

	iter = html_engine_get_table_cell (html->engine);

	while (iter != NULL) {
		if (iter->col == cell->col &&
			HTML_OBJECT (iter)->parent == parent)
			callback (editor, iter, widget);
		html_engine_next_cell (html->engine, FALSE);
		iter = html_engine_get_table_cell (html->engine);
	}
}

static void
cell_properties_set_row (GtkhtmlEditor *editor,
                         CellCallback callback,
                         GtkWidget *widget)
{
	GtkHTML *html;
	HTMLObject *parent;
	HTMLTableCell *cell;
	HTMLTableCell *iter;

	html = gtkhtml_editor_get_html (editor);
	cell = HTML_TABLE_CELL (editor->priv->cell_object);
	parent = editor->priv->cell_parent;

	iter = html_engine_get_table_cell (html->engine);

	while (iter != NULL && iter->row == cell->row) {
		if (HTML_OBJECT (iter)->parent == parent)
			callback (editor, iter, widget);
		html_engine_next_cell (html->engine, FALSE);
		iter = html_engine_get_table_cell (html->engine);
	}
}

static void
cell_properties_set_table (GtkhtmlEditor *editor,
                           CellCallback callback,
                           GtkWidget *widget)
{
	GtkHTML *html;
	HTMLObject *parent;
	HTMLTableCell *iter;

	html = gtkhtml_editor_get_html (editor);
	parent = editor->priv->cell_parent;

	iter = html_engine_get_table_cell (html->engine);

	while (iter != NULL) {
		if (HTML_OBJECT (iter)->parent == parent)
			callback (editor, iter, widget);
		html_engine_next_cell (html->engine, FALSE);
		iter = html_engine_get_table_cell (html->engine);
	}
}

static void
cell_properties_set (GtkhtmlEditor *editor,
                     CellCallback callback,
                     GtkWidget *widget)
{
	GtkHTML *html;
	HTMLTable *table;
	HTMLTableCell *cell;
	guint position;

	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->cell_parent);
	cell = HTML_TABLE_CELL (editor->priv->cell_object);

	position = html->engine->cursor->position;

	switch (editor->priv->cell_scope) {
		case TABLE_CELL_SCOPE_CELL:
			callback (editor, cell, widget);
			break;

		case TABLE_CELL_SCOPE_ROW:
			if (html_engine_table_goto_row (
				html->engine, table, cell->row))
				cell_properties_set_row (
					editor, callback, widget);
			break;

		case TABLE_CELL_SCOPE_COLUMN:
			if (html_engine_table_goto_col (
				html->engine, table, cell->col))
				cell_properties_set_column (
					editor, callback, widget);
			break;

		case TABLE_CELL_SCOPE_TABLE:
			if (html_engine_goto_table_0 (html->engine, table))
				cell_properties_set_table (
					editor, callback, widget);
			break;
	}

	html_cursor_jump_to_position (
		html->engine->cursor, html->engine, position);
}

static void
cell_properties_set_background_color_cb (GtkhtmlEditor *editor,
                                         HTMLTableCell *cell,
                                         GtkWidget *widget)
{
	GtkhtmlColorCombo *combo;
	GtkHTML *html;
	GdkColor color;
	gboolean got_color;

	combo = GTKHTML_COLOR_COMBO (widget);
	html = gtkhtml_editor_get_html (editor);

	/* The default table cell color is transparent. */
	got_color = gtkhtml_color_combo_get_current_color (combo, &color);

	html_engine_table_cell_set_bg_color (
		html->engine, cell, got_color ? &color : NULL);
}

static void
cell_properties_set_background_image_cb (GtkhtmlEditor *editor,
                                         HTMLTableCell *cell,
                                         GtkWidget *widget)
{
	GtkHTML *html;
	const gchar *filename;
	gchar *uri;

	html = gtkhtml_editor_get_html (editor);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	uri = gtk_html_filename_to_uri (filename);
	html_engine_table_cell_set_bg_pixmap (html->engine, cell, uri);
	g_free (uri);
}

static void
cell_properties_set_column_span_cb (GtkhtmlEditor *editor,
                                    HTMLTableCell *cell,
                                    GtkWidget *widget)
{
	GtkHTML *html;
	gint value;

	html = gtkhtml_editor_get_html (editor);
	widget = WIDGET (CELL_PROPERTIES_COLUMN_SPAN_SPIN_BUTTON);
	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	html_engine_set_cspan (html->engine, value);
}

static void
cell_properties_set_header_style_cb (GtkhtmlEditor *editor,
                                     HTMLTableCell *cell,
                                     GtkWidget *widget)
{
	GtkHTML *html;
	gboolean active;

	html = gtkhtml_editor_get_html (editor);
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	html_engine_table_cell_set_heading (html->engine, cell, active);
}

static void
cell_properties_set_horizontal_alignment_cb (GtkhtmlEditor *editor,
                                             HTMLTableCell *cell,
                                             GtkWidget *widget)
{
	GtkHTML *html;
	HTMLHAlignType align;
	gint active;

	html = gtkhtml_editor_get_html (editor);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	align = HTML_HALIGN_LEFT + (HTMLHAlignType) active;

	html_engine_table_cell_set_halign (html->engine, cell, align);
}

static void
cell_properties_set_row_span_cb (GtkhtmlEditor *editor,
                                 HTMLTableCell *cell,
                                 GtkWidget *widget)
{
	GtkHTML *html;
	gint value;

	html = gtkhtml_editor_get_html (editor);
	widget = WIDGET (CELL_PROPERTIES_ROW_SPAN_SPIN_BUTTON);
	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	html_engine_set_rspan (html->engine, value);
}

static void
cell_properties_set_vertical_alignment_cb (GtkhtmlEditor *editor,
                                           HTMLTableCell *cell,
                                           GtkWidget *widget)
{
	GtkHTML *html;
	HTMLVAlignType align;
	gint active;

	html = gtkhtml_editor_get_html (editor);

	widget = WIDGET (CELL_PROPERTIES_VERTICAL_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	align = HTML_VALIGN_TOP + (HTMLVAlignType) active;

	html_engine_table_cell_set_valign (html->engine, cell, align);
}

static void
cell_properties_set_width_cb (GtkhtmlEditor *editor,
                              HTMLTableCell *cell,
                              GtkWidget *widget)
{
	GtkHTML *html;
	gboolean sensitive;
	gint active;
	gint value;

	html = gtkhtml_editor_get_html (editor);

	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = WIDGET (CELL_PROPERTIES_WIDTH_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	widget = WIDGET (CELL_PROPERTIES_WIDTH_SPIN_BUTTON);
	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	html_engine_table_cell_set_width (
		html->engine, cell,
		sensitive ? value : 0,
		(active == SIZE_UNIT_PERCENT));
}

static void
cell_properties_set_wrap_text_cb (GtkhtmlEditor *editor,
                                  HTMLTableCell *cell,
                                  GtkWidget *widget)
{
	GtkHTML *html;
	gboolean active;

	html = gtkhtml_editor_get_html (editor);
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	html_engine_table_cell_set_no_wrap (html->engine, cell, !active);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_color_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_background_color_cb,
		WIDGET (CELL_PROPERTIES_COLOR_COMBO));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_column_span_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_column_span_cb,
		WIDGET (CELL_PROPERTIES_COLUMN_SPAN_SPIN_BUTTON));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_image_file_set_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_background_image_cb,
		WIDGET (CELL_PROPERTIES_IMAGE_FILE_CHOOSER));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_header_style_toggled_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_header_style_cb,
		WIDGET (CELL_PROPERTIES_HEADER_STYLE_CHECK_BUTTON));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_horizontal_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_horizontal_alignment_cb,
		WIDGET (CELL_PROPERTIES_HORIZONTAL_COMBO_BOX));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_row_span_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_row_span_cb,
		WIDGET (CELL_PROPERTIES_ROW_SPAN_SPIN_BUTTON));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_scope_toggled_cb (GtkWidget *window,
                                                 GtkWidget *button))
{
	GtkhtmlEditor *editor;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		return;

	editor = extract_gtkhtml_editor (window);

	if (button == WIDGET (CELL_PROPERTIES_CELL_RADIO_BUTTON))
		editor->priv->cell_scope = TABLE_CELL_SCOPE_CELL;

	else if (button == WIDGET (CELL_PROPERTIES_ROW_RADIO_BUTTON))
		editor->priv->cell_scope = TABLE_CELL_SCOPE_ROW;

	else if (button == WIDGET (CELL_PROPERTIES_COLUMN_RADIO_BUTTON))
		editor->priv->cell_scope = TABLE_CELL_SCOPE_COLUMN;

	else if (button == WIDGET (CELL_PROPERTIES_TABLE_RADIO_BUTTON))
		editor->priv->cell_scope = TABLE_CELL_SCOPE_TABLE;

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_show_window_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	HTMLTableCell *cell;
	GtkWidget *widget;
	GtkHTML *html;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	cell = html_engine_get_table_cell (html->engine);
	editor->priv->cell_object = HTML_OBJECT (cell);
	g_assert (HTML_IS_TABLE_CELL (cell));

	editor->priv->cell_parent = editor->priv->cell_object->parent;
	g_assert (HTML_IS_TABLE (editor->priv->cell_parent));

	/* Select 'cell' scope. */
	widget = WIDGET (CELL_PROPERTIES_CELL_RADIO_BUTTON);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = WIDGET (CELL_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_current_color (
		GTKHTML_COLOR_COMBO (widget),
		cell->have_bg ? &cell->bg : NULL);

	widget = WIDGET (CELL_PROPERTIES_IMAGE_FILE_CHOOSER);
	if (cell->have_bgPixmap) {
		gchar *filename;

		filename = gtk_html_filename_from_uri (cell->bgPixmap->url);
		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (widget), filename);
		g_free (filename);
	}

	if (HTML_CLUE (cell)->halign == HTML_HALIGN_NONE)
		active = HTML_HALIGN_LEFT;
	else
		active = HTML_CLUE (cell)->halign - HTML_HALIGN_LEFT;
	widget = WIDGET (CELL_PROPERTIES_HORIZONTAL_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	active = HTML_CLUE (cell)->valign - HTML_VALIGN_TOP;
	widget = WIDGET (CELL_PROPERTIES_VERTICAL_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	if (cell->percent_width) {
		widget = WIDGET (CELL_PROPERTIES_WIDTH_CHECK_BUTTON);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (widget), TRUE);

		value = (gdouble) cell->fixed_width;
		widget = WIDGET (CELL_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

		active = SIZE_UNIT_PERCENT;
		widget = WIDGET (CELL_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	} else if (cell->fixed_width) {
		widget = WIDGET (CELL_PROPERTIES_WIDTH_CHECK_BUTTON);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (widget), TRUE);

		value = (gdouble) cell->fixed_width;
		widget = WIDGET (CELL_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

		active = SIZE_UNIT_PX;
		widget = WIDGET (CELL_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	} else {
		widget = WIDGET (CELL_PROPERTIES_WIDTH_CHECK_BUTTON);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (widget), FALSE);

		widget = WIDGET (CELL_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 0.0);

		widget = WIDGET (CELL_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}

	active = !cell->no_wrap;
	widget = WIDGET (CELL_PROPERTIES_WRAP_TEXT_CHECK_BUTTON);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

	active = cell->heading;
	widget = WIDGET (CELL_PROPERTIES_HEADER_STYLE_CHECK_BUTTON);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

	value = (gdouble) cell->cspan;
	widget = WIDGET (CELL_PROPERTIES_COLUMN_SPAN_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) cell->rspan;
	widget = WIDGET (CELL_PROPERTIES_ROW_SPAN_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_vertical_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_vertical_alignment_cb,
		WIDGET (CELL_PROPERTIES_VERTICAL_COMBO_BOX));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_width_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkAdjustment *adjustment;
	GtkWidget *widget;
	gboolean sensitive;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);

	widget = WIDGET (CELL_PROPERTIES_WIDTH_CHECK_BUTTON);
	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = WIDGET (CELL_PROPERTIES_WIDTH_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	gtk_widget_set_sensitive (widget, sensitive);

	widget = WIDGET (CELL_PROPERTIES_WIDTH_SPIN_BUTTON);
	adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
	gtk_widget_set_sensitive (widget, sensitive);

	if (active == SIZE_UNIT_PERCENT) {
		gtk_adjustment_set_upper (adjustment, 100.0);
		gtk_adjustment_changed (adjustment);
	} else {
		gtk_adjustment_set_upper (adjustment, (gdouble) G_MAXINT);
		gtk_adjustment_changed (adjustment);
	}

	/* Clamp the value between the new bounds. */
	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (adjustment, value);

	cell_properties_set (
		editor, cell_properties_set_width_cb,
		WIDGET (CELL_PROPERTIES_WIDTH_CHECK_BUTTON));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_cell_properties_wrap_text_toggled_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	cell_properties_set (
		editor, cell_properties_set_wrap_text_cb,
		WIDGET (CELL_PROPERTIES_WRAP_TEXT_CHECK_BUTTON));

	g_object_unref (editor);
}

/*****************************************************************************
 * Find Window
 *****************************************************************************/

static void
sensitize_find_action (GtkWidget *window)
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);
	gtk_action_set_sensitive (ACTION (FIND), TRUE);
	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_find_backwards_toggled_cb (GtkWidget *window,
                                          GtkToggleButton *button))
{
	sensitize_find_action (window);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_find_case_sensitive_toggled_cb (GtkWidget *window,
                                               GtkToggleButton *button))
{
	if (!gtk_toggle_button_get_active (button))
		sensitize_find_action (window);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_find_entry_activate_cb (GtkWidget *window,
                                       GtkEntry *entry))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);
	gtk_action_activate (ACTION (FIND));
	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_find_entry_changed_cb (GtkWidget *window,
                                      GtkEntry *entry))
{
	sensitize_find_action (window);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_find_regular_expression_toggled_cb (GtkWidget *window,
                                                   GtkToggleButton *button))
{
	sensitize_find_action (window);
}

/*****************************************************************************
 * Image Properties Window
 *****************************************************************************/

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_alignment_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	HTMLVAlignType valign;
	gint active;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_ALIGNMENT_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	valign = HTML_VALIGN_TOP + (HTMLVAlignType) active;

	html_image_set_valign (image, valign);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_border_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	gint border;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_BORDER_SPIN_BUTTON);
	border = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	html_image_set_border (image, border);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_description_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	const gchar *text;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_DESCRIPTION_ENTRY);
	text = gtk_entry_get_text (GTK_ENTRY (widget));

	/* Convert empty strings to NULL. */
	if (text != NULL && *text == '\0')
		text = NULL;

	html_image_set_alt (image, text);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_padding_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	gint hspace;
	gint vspace;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_X_PADDING_SPIN_BUTTON);
	hspace = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	widget = WIDGET (IMAGE_PROPERTIES_Y_PADDING_SPIN_BUTTON);
	vspace = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

	html_image_set_spacing (image, hspace, vspace);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_show_window_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLObject *parent;
	HTMLImage *image;
	GtkHTML *html;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	editor->priv->image_object = html->engine->cursor->object;
	image = HTML_IMAGE (editor->priv->image_object);
	g_assert (HTML_IS_IMAGE (image));

	widget = WIDGET (IMAGE_PROPERTIES_SOURCE_FILE_CHOOSER);
	parent = editor->priv->image_object->parent;
	if ((parent == NULL
		|| html_object_get_data (parent, "template_image") == NULL)
		&& image->image_ptr->url != NULL) {

		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (widget), image->image_ptr->url);
	} else
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (widget));

	widget = WIDGET (IMAGE_PROPERTIES_DESCRIPTION_ENTRY);
	if (image->alt != NULL)
		gtk_entry_set_text (GTK_ENTRY (widget), image->alt);
	else
		gtk_entry_set_text (GTK_ENTRY (widget), "");

	if (image->percent_width) {
		active = SIZE_UNIT_PERCENT;
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) image->specified_width;
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	} else if (image->specified_width > 0) {
		active = SIZE_UNIT_PX;
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) image->specified_width;
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	} else {
		active = SIZE_UNIT_FOLLOW;
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) html_image_get_actual_width (image, NULL);
		widget = WIDGET (IMAGE_PROPERTIES_WIDTH_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	}

	if (image->percent_height) {
		active = SIZE_UNIT_PERCENT;
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) image->specified_height;
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	} else if (image->specified_height > 0) {
		active = SIZE_UNIT_PX;
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) image->specified_height;
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	} else {
		active = SIZE_UNIT_FOLLOW;
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_COMBO_BOX);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

		value = (gdouble) html_image_get_actual_height (image, NULL);
		widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_SPIN_BUTTON);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	}

	active = image->valign - HTML_VALIGN_TOP;
	widget = WIDGET (IMAGE_PROPERTIES_ALIGNMENT_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	value = (gdouble) image->hspace;
	widget = WIDGET (IMAGE_PROPERTIES_X_PADDING_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) image->vspace;
	widget = WIDGET (IMAGE_PROPERTIES_Y_PADDING_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) image->border;
	widget = WIDGET (IMAGE_PROPERTIES_BORDER_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = WIDGET (IMAGE_PROPERTIES_URL_ENTRY);
	if (image->url != NULL) {
		gchar *text;

		if (image->target != NULL)
			text = g_strdup_printf (
				"%s#%s", image->url, image->target);
		else
			text = g_strdup (image->url);
		gtk_entry_set_text (GTK_ENTRY (widget), text);
		g_free (text);
	} else
		gtk_entry_set_text (GTK_ENTRY (widget), "");

	widget = WIDGET (IMAGE_PROPERTIES_URL_BUTTON);
	gtk_widget_set_sensitive (widget, image->url != NULL);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_size_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	gint height_units;
	gint width_units;
	gint height;
	gint width;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_WIDTH_COMBO_BOX);
	width_units = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	widget = WIDGET (IMAGE_PROPERTIES_WIDTH_SPIN_BUTTON);
	width = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	gtk_widget_set_sensitive (widget, (width_units != SIZE_UNIT_FOLLOW));

	widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_COMBO_BOX);
	height_units = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	widget = WIDGET (IMAGE_PROPERTIES_HEIGHT_SPIN_BUTTON);
	height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	gtk_widget_set_sensitive (widget, (height_units != SIZE_UNIT_FOLLOW));

	html_image_set_size (
		image,
		(width_units != SIZE_UNIT_FOLLOW) ? width : 0,
		(height_units != SIZE_UNIT_FOLLOW) ? height : 0,
		(width_units == SIZE_UNIT_PERCENT),
		(height_units == SIZE_UNIT_PERCENT));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_source_file_set_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLImage *image;
	gchar *filename;
	gchar *url = NULL;

	editor = extract_gtkhtml_editor (window);
	image = HTML_IMAGE (editor->priv->image_object);

	widget = WIDGET (IMAGE_PROPERTIES_SOURCE_FILE_CHOOSER);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	if (filename != NULL)
		url = gtk_html_filename_to_uri (filename);

	html_image_edit_set_url (image, url);

	g_free (filename);
	g_free (url);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_url_button_clicked_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	const gchar *text;

	editor = extract_gtkhtml_editor (window);

	widget = WIDGET (IMAGE_PROPERTIES_URL_ENTRY);
	text = gtk_entry_get_text (GTK_ENTRY (widget));

	g_return_if_fail (text != NULL);
	gtkhtml_editor_show_uri (GTK_WINDOW (window), text);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_image_properties_url_entry_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	HTMLColorSet *color_set;
	HTMLColor *color;
	GtkWidget *widget;
	GtkHTML *html;
	gchar **parts;
	guint length;
	const gchar *text;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	color_set = html->engine->settings->color_set;

	widget = WIDGET (IMAGE_PROPERTIES_URL_ENTRY);
	text = gtk_entry_get_text (GTK_ENTRY (widget));

	widget = WIDGET (IMAGE_PROPERTIES_URL_BUTTON);
	if (text != NULL && *text != '\0') {
		color = html_colorset_get_color (color_set, HTMLLinkColor);
		gtk_widget_set_sensitive (widget, TRUE);
	} else {
		color = html_colorset_get_color (color_set, HTMLTextColor);
		gtk_widget_set_sensitive (widget, FALSE);
	}

	parts = g_strsplit (text, "#", 2);
	length = g_strv_length (parts);

	html_object_set_link (
		editor->priv->image_object, color,
		(length > 0) ? parts[0] : NULL,
		(length > 1) ? parts[1] : NULL);

	g_strfreev (parts);

	g_object_unref (editor);
}

/*****************************************************************************
 * Link Properties Window
 *****************************************************************************/

static gchar *
sanitize_description_text (const gchar *ptext)
{
	gchar *p, *text = g_strdup (ptext);

	if (!text)
		return text;

	/* replace line breakers with spaces */
	for (p = text; *p; p++) {
		if (*p == '\r' || *p == '\n')
			*p = ' ';
	}

	return g_strstrip (text);
}

static void
update_link_offsets (GtkhtmlEditor *editor)
{
	Link *link;
	GtkHTML *html;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (GTKHTML_IS_EDITOR (editor));
	g_return_if_fail (editor->priv != NULL);

	html = gtkhtml_editor_get_html (editor);

	link = html_text_get_link_at_offset (HTML_TEXT (html->engine->cursor->object), html->engine->cursor->offset);
	g_return_if_fail (link != NULL);

	editor->priv->link_start_offset = link->start_offset;
	editor->priv->link_end_offset = link->end_offset;
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_link_properties_description_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *dsc_entry;
	GtkWidget *url_entry;
	GtkHTML *html;
	gchar *text;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	dsc_entry = WIDGET (LINK_PROPERTIES_DESCRIPTION_ENTRY);
	url_entry = WIDGET (LINK_PROPERTIES_URL_ENTRY);

	text = sanitize_description_text (gtk_entry_get_text (GTK_ENTRY (dsc_entry)));

	editor->priv->link_custom_description = (*text != '\0');

	if (editor->priv->link_custom_description) {
		gint start_offset;
		gint end_offset;
		glong length;

		start_offset = editor->priv->link_start_offset;
		end_offset = editor->priv->link_end_offset;
		length = g_utf8_strlen (text, -1);

		if (start_offset != end_offset) {
			html_cursor_jump_to (
				html->engine->cursor, html->engine,
				editor->priv->link_object, start_offset);
			html_engine_set_mark (html->engine);
			html_cursor_jump_to (
				html->engine->cursor, html->engine,
				editor->priv->link_object, end_offset);
			html_engine_delete (html->engine);
		}

		html_engine_paste_link (
			html->engine, text, length,
			gtk_entry_get_text (GTK_ENTRY (url_entry)));

		editor->priv->link_object = html->engine->cursor->object;
		update_link_offsets (editor);
	}

	g_free (text);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_link_properties_url_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *dsc_entry;
	GtkWidget *url_entry;
	GtkHTML *html;
	gchar *text;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	dsc_entry = WIDGET (LINK_PROPERTIES_DESCRIPTION_ENTRY);
	url_entry = WIDGET (LINK_PROPERTIES_URL_ENTRY);

	text = g_strstrip (g_strdup (
		gtk_entry_get_text (GTK_ENTRY (url_entry))));

	gtk_action_set_sensitive (ACTION (TEST_URL), *text != '\0');

	if (html_engine_is_selection_active (html->engine)) {
		html_engine_set_link (html->engine, text);
	} else if (!editor->priv->link_custom_description) {
		gchar *descr = sanitize_description_text (text);
		gtk_entry_set_text (GTK_ENTRY (dsc_entry), descr);
		g_free (descr);
		editor->priv->link_custom_description = FALSE;
	} else {
		gint start_offset;
		gint end_offset;
		glong length;
		const gchar *descr = gtk_entry_get_text (GTK_ENTRY (dsc_entry));

		start_offset = editor->priv->link_start_offset;
		end_offset = editor->priv->link_end_offset;
		length = g_utf8_strlen (descr, -1);

		if (start_offset != end_offset) {
			html_cursor_jump_to (
				html->engine->cursor, html->engine,
				editor->priv->link_object, start_offset);
			html_engine_set_mark (html->engine);
			html_cursor_jump_to (
				html->engine->cursor, html->engine,
				editor->priv->link_object, end_offset);
			html_engine_delete (html->engine);
		}

		html_engine_paste_link (
			html->engine, descr, length, text);

		editor->priv->link_object = html->engine->cursor->object;
		update_link_offsets (editor);
	}

	g_free (text);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_link_properties_show_window_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *dsc_entry;
	GtkWidget *url_entry;
	GtkHTML *html;
	gchar *url = NULL, *dsc = NULL;
	gboolean sensitive;
	HTMLCursor *cursor;
	gint start_offset = 0;
	gint end_offset = 0;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	dsc_entry = WIDGET (LINK_PROPERTIES_DESCRIPTION_ENTRY);
	url_entry = WIDGET (LINK_PROPERTIES_URL_ENTRY);

	editor->priv->link_custom_description = FALSE;

	cursor = html->engine->cursor;

	if (HTML_IS_TEXT (cursor->object))
		url = html_object_get_complete_url (
			cursor->object, cursor->offset);

	if (url != NULL) {
		if (HTML_IS_IMAGE (cursor->object)) {
			start_offset = 0;
			end_offset = 1;
		} else {
			Link *link;

			link = html_text_get_link_at_offset (
				HTML_TEXT (cursor->object),
				cursor->offset);
			if (link != NULL) {
				start_offset = link->start_offset;
				end_offset = link->end_offset;
				dsc = html_text_get_link_text (HTML_TEXT (cursor->object), cursor->offset);
				editor->priv->link_custom_description = dsc && !g_str_equal (dsc, url);
			}
		}
	} else if (HTML_IS_TEXT (cursor->object)) {
		start_offset = cursor->offset;
		end_offset = start_offset;
	}

	sensitive = (url == NULL);

	if (html_engine_is_selection_active (html->engine)) {
		if (sensitive && !dsc)
			dsc = html_engine_get_selection_string (html->engine);
		sensitive = FALSE;
	}

	editor->priv->link_object = cursor->object;
	editor->priv->link_start_offset = start_offset;
	editor->priv->link_end_offset = end_offset;

	gtk_widget_set_sensitive (dsc_entry, sensitive);
	gtk_entry_set_text (
		GTK_ENTRY (url_entry),
		(url != NULL) ? url : "http://");

	if (dsc) {
		gchar *tmp = dsc;

		dsc = sanitize_description_text (dsc);
		g_free (tmp);
	}

	gtk_entry_set_text (GTK_ENTRY (dsc_entry), dsc ? dsc : "");
	gtk_widget_grab_focus (url_entry);

	g_free (url);
	g_free (dsc);

	g_object_unref (editor);
}

/*****************************************************************************
 * Page Properties Window
 *****************************************************************************/

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_background_color_changed_cb (GtkWidget *window,
                                                            GtkhtmlColorCombo *combo))
{
	const HTMLColorId color_id = HTMLBgColor;
	GtkhtmlEditor *editor;
	GdkColor color;
	GtkHTML *html;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	if (!gtkhtml_color_combo_get_current_color (combo, &color))
		color = html_colorset_get_color (
			html->engine->defaultSettings->color_set,
			color_id)->color;

	html_colorset_set_color (
		html->engine->settings->color_set, &color, color_id);
	html_object_change_set_down (
		html->engine->clue, HTML_CHANGE_RECALC_PI);

	gtk_widget_queue_draw (GTK_WIDGET (html));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_custom_file_changed_cb (GtkWidget *window,
                                                       GtkFileChooser *file_chooser))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	gchar *filename;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	if (html->engine->bgPixmapPtr != NULL) {
		html_image_factory_unregister (
			html->engine->image_factory,
			html->engine->bgPixmapPtr, NULL);
		html->engine->bgPixmapPtr = NULL;
	}

	filename = gtk_file_chooser_get_filename (file_chooser);
	if (filename != NULL && *filename != '\0') {
		gchar *uri;

		uri = gtk_html_filename_to_uri (filename);
		html->engine->bgPixmapPtr = html_image_factory_register (
			html->engine->image_factory, NULL, uri, TRUE);
		g_free (uri);
	}
	g_free (filename);

	gtk_widget_queue_draw (GTK_WIDGET (html));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_link_color_changed_cb (GtkWidget *window,
                                                      GtkhtmlColorCombo *combo))
{
	const HTMLColorId color_id = HTMLLinkColor;
	GtkhtmlEditor *editor;
	GdkColor color;
	GtkHTML *html;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	if (!gtkhtml_color_combo_get_current_color (combo, &color))
		color = html_colorset_get_color (
			html->engine->defaultSettings->color_set,
			color_id)->color;

	html_colorset_set_color (
		html->engine->settings->color_set, &color, color_id);
	html_object_change_set_down (
		html->engine->clue, HTML_CHANGE_RECALC_PI);

	gtk_widget_queue_draw (GTK_WIDGET (html));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_template_changed_cb (GtkWidget *window,
                                                    GtkComboBox *combo_box))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	GtkWidget *widget;
	gchar *filename;

	/* Template Parameters */
	const gchar *basename;
	GdkColor text_color;
	GdkColor link_color;
	GdkColor bgnd_color;
	gint left_margin = 10;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	switch (gtk_combo_box_get_active (combo_box)) {
		case 0:
			basename = NULL;
			text_color = html_colorset_get_color_allocated (
				html->engine->settings->color_set,
				html->engine->painter,
				HTMLTextColor)->color;
			link_color = html_colorset_get_color_allocated (
				html->engine->settings->color_set,
				html->engine->painter,
				HTMLLinkColor)->color;
			bgnd_color = html_colorset_get_color_allocated (
				html->engine->settings->color_set,
				html->engine->painter,
				HTMLBgColor)->color;
			break;

		case 1:
			basename = "paper.png";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x0;
			link_color.red   = 0x0;
			link_color.green = 0x3380;
			link_color.blue  = 0x6680;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			left_margin = 30;
			break;

		case 2:
			basename = "texture.png";
			text_color.red   = 0x1FFF;
			text_color.green = 0x1FFF;
			text_color.blue  = 0x8FFF;
			link_color.red   = 0x0;
			link_color.green = 0x0;
			link_color.blue  = 0xFFFF;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			break;

		case 3:
			basename = "rect.png";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x0;
			link_color.red   = 0x0;
			link_color.green = 0x0;
			link_color.blue  = 0xFFFF;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			break;

		case 4:
			basename = "ribbon.jpg";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x0;
			link_color.red   = 0x9900;
			link_color.green = 0x3300;
			link_color.blue  = 0x6600;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			left_margin = 70;
			break;

		case 5:
			basename = "midnight-stars.jpg";
			text_color.red   = 0xFFFF;
			text_color.green = 0xFFFF;
			text_color.blue  = 0xFFFF;
			link_color.red   = 0xFFFF;
			link_color.green = 0x9900;
			link_color.blue  = 0x0;
			bgnd_color.red   = 0x0;
			bgnd_color.green = 0x0;
			bgnd_color.blue  = 0x0;
			break;

		case 6:
			basename = "confidential-stamp.jpg";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x0;
			link_color.red   = 0x0;
			link_color.green = 0x0;
			link_color.blue  = 0xFFFF;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			break;

		case 7:
			basename = "draft-stamp.jpg";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x0;
			link_color.red   = 0x0;
			link_color.green = 0x0;
			link_color.blue  = 0xFFFF;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			break;

		case 8:
			basename = "draft-paper.png";
			text_color.red   = 0x0;
			text_color.green = 0x0;
			text_color.blue  = 0x8000;
			link_color.red   = 0xE300;
			link_color.green = 0x2100;
			link_color.blue  = 0x2300;
			bgnd_color.red   = 0xFFFF;
			bgnd_color.green = 0xFFFF;
			bgnd_color.blue  = 0xFFFF;
			break;

		default:
			g_return_if_reached ();
	}

	widget = WIDGET (PAGE_PROPERTIES_CUSTOM_FILE_CHOOSER);
	if (basename != NULL)
		filename = g_build_filename (ICONDIR, basename, NULL);
	else
		filename = g_strdup ("");
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
	g_free (filename);

	widget = WIDGET (PAGE_PROPERTIES_TEXT_COLOR_COMBO);
	gtkhtml_color_combo_set_current_color (
		GTKHTML_COLOR_COMBO (widget), &text_color);

	widget = WIDGET (PAGE_PROPERTIES_LINK_COLOR_COMBO);
	gtkhtml_color_combo_set_current_color (
		GTKHTML_COLOR_COMBO (widget), &link_color);

	widget = WIDGET (PAGE_PROPERTIES_BACKGROUND_COLOR_COMBO);
	gtkhtml_color_combo_set_current_color (
		GTKHTML_COLOR_COMBO (widget), &bgnd_color);

	html->engine->leftBorder = left_margin;
	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_text_color_changed_cb (GtkWidget *window,
                                                      GtkhtmlColorCombo *combo))
{
	const HTMLColorId color_id = HTMLTextColor;
	GtkhtmlEditor *editor;
	GdkColor color;
	GtkHTML *html;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	if (!gtkhtml_color_combo_get_current_color (combo, &color))
		color = html_colorset_get_color (
			html->engine->defaultSettings->color_set,
			color_id)->color;

	gtkhtml_color_state_set_default_color (
		editor->priv->text_color, &color);

	html_colorset_set_color (
		html->engine->settings->color_set, &color, color_id);
	html_object_change_set_down (
		html->engine->clue, HTML_CHANGE_RECALC_PI);

	gtk_widget_queue_draw (GTK_WIDGET (html));

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_page_properties_window_realized_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);

	/* Active "None" Template */
	gtk_combo_box_set_active (
		GTK_COMBO_BOX (WIDGET (
		PAGE_PROPERTIES_TEMPLATE_COMBO_BOX)), 0);

	g_object_unref (editor);
}

/*****************************************************************************
 * Rule Properties Window
 *****************************************************************************/

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_rule_properties_alignment_changed_cb (GtkWidget *window,
                                                     GtkComboBox *combo_box))
{
	GtkhtmlEditor *editor;
	HTMLHAlignType align;
	GtkHTML *html;
	HTMLRule *rule;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	rule = HTML_RULE (editor->priv->rule_object);

	align = HTML_HALIGN_LEFT + gtk_combo_box_get_active (combo_box);
	html_rule_set_align (rule, html->engine, align);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_rule_properties_shaded_toggled_cb (GtkWidget *window,
                                                  GtkToggleButton *button))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	HTMLRule *rule;
	gboolean active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	rule = HTML_RULE (editor->priv->rule_object);

	active = gtk_toggle_button_get_active (button);
	html_rule_set_shade (rule, html->engine, active);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_rule_properties_size_changed_cb (GtkWidget *window,
                                                GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	HTMLRule *rule;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	rule = HTML_RULE (editor->priv->rule_object);

	value = gtk_spin_button_get_value_as_int (button);
	html_rule_set_size (rule, html->engine, value);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_rule_properties_show_window_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLRule *rule;
	GtkHTML *html;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	editor->priv->rule_object = html->engine->cursor->object;
	rule = HTML_RULE (editor->priv->rule_object);
	g_assert (HTML_IS_RULE (rule));

	if (editor->priv->rule_object->percent > 0) {
		value = (gdouble) editor->priv->rule_object->percent;
		active = SIZE_UNIT_PERCENT;
	} else {
		value = (gdouble) rule->length;
		active = SIZE_UNIT_PX;
	}

	widget = WIDGET (RULE_PROPERTIES_WIDTH_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = WIDGET (RULE_PROPERTIES_WIDTH_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	value = (gdouble) rule->size;
	widget = WIDGET (RULE_PROPERTIES_SIZE_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	active = rule->halign - HTML_HALIGN_LEFT;
	widget = WIDGET (RULE_PROPERTIES_ALIGNMENT_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	active = rule->shade;
	widget = WIDGET (RULE_PROPERTIES_SHADED_CHECK_BUTTON);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_rule_properties_width_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkAdjustment *adjustment;
	GtkWidget *widget;
	GtkHTML *html;
	HTMLRule *rule;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	rule = HTML_RULE (editor->priv->rule_object);

	widget = WIDGET (RULE_PROPERTIES_WIDTH_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	widget = WIDGET (RULE_PROPERTIES_WIDTH_SPIN_BUTTON);
	adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));

	if (active == SIZE_UNIT_PERCENT) {
		gtk_adjustment_set_upper (adjustment, 100.0);
		gtk_adjustment_changed (adjustment);
	} else {
		gtk_adjustment_set_upper (adjustment, (gdouble) G_MAXINT);
		gtk_adjustment_changed (adjustment);
	}

	/* Clamp the value between the new bounds. */
	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (adjustment, value);
	value = gtk_adjustment_get_value (adjustment);

	html_rule_set_length (
		rule, html->engine,
		(active == SIZE_UNIT_PX) ? (gint) value : 0,
		(active == SIZE_UNIT_PX) ? 0 : (gint) value);

	g_object_unref (editor);
}

/*****************************************************************************
 * Replace Confirmation Window
 *****************************************************************************/

AUTOCONNECTED_SIGNAL_HANDLER (gboolean
gtkhtml_editor_replace_confirmation_delete_event_cb (GtkWidget *window,
                                                     GdkEvent *event))
{
	GtkhtmlEditor *editor;

	editor = extract_gtkhtml_editor (window);
	gtk_action_activate (ACTION (CONFIRM_REPLACE_CANCEL));
	g_object_unref (editor);

	return gtk_widget_hide_on_delete (window);
}

/*****************************************************************************
 * Table Properties Window
 *****************************************************************************/

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_alignment_changed_cb (GtkWidget *window,
                                                      GtkComboBox *combo_box))
{
	GtkhtmlEditor *editor;
	HTMLHAlignType align;
	GtkHTML *html;
	HTMLTable *table;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	html_cursor_forward (html->engine->cursor, html->engine);

	align = HTML_HALIGN_LEFT + gtk_combo_box_get_active (combo_box);
	html_engine_table_set_align (html->engine, table, align);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_border_changed_cb (GtkWidget *window,
                                                   GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	HTMLTable *table;
	GtkHTML *html;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	html_cursor_forward (html->engine->cursor, html->engine);

	value = gtk_spin_button_get_value_as_int (button);
	html_engine_table_set_border_width (html->engine, table, value, FALSE);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_color_changed_cb (GtkWidget *window,
                                                  GtkhtmlColorCombo *combo))
{
	GtkhtmlEditor *editor;
	HTMLTable *table;
	GtkHTML *html;
	GdkColor color;
	gboolean got_color;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	/* The default table color is transparent. */
	got_color = gtkhtml_color_combo_get_current_color (combo, &color);

	html_engine_table_set_bg_color (
		html->engine, table, got_color ? &color : NULL);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_cols_changed_cb (GtkWidget *window,
                                                 GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	html_cursor_jump_to (
		html->engine->cursor, html->engine,
		editor->priv->table_object, 1);
	html_cursor_backward (html->engine->cursor, html->engine);

	value = gtk_spin_button_get_value_as_int (button);
	html_engine_table_set_cols (html->engine, value);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_image_changed_cb (GtkWidget *window,
                                                  GtkFileChooser *file_chooser))
{
	GtkhtmlEditor *editor;
	HTMLTable *table;
	GtkHTML *html;
	gchar *uri;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	html_cursor_forward (html->engine->cursor, html->engine);

	uri = gtk_file_chooser_get_uri (file_chooser);
	html_engine_table_set_bg_pixmap (html->engine, table, uri);
	g_free (uri);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_padding_changed_cb (GtkWidget *window,
                                                    GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	HTMLTable *table;
	GtkHTML *html;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	html_cursor_forward (html->engine->cursor, html->engine);

	value = gtk_spin_button_get_value_as_int (button);
	html_engine_table_set_padding (html->engine, table, value, FALSE);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_spacing_changed_cb (GtkWidget *window,
                                                    GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	HTMLTable *table;
	GtkHTML *html;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	html_cursor_forward (html->engine->cursor, html->engine);

	value = gtk_spin_button_get_value_as_int (button);
	html_engine_table_set_spacing (html->engine, table, value, FALSE);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_rows_changed_cb (GtkWidget *window,
                                                 GtkSpinButton *button))
{
	GtkhtmlEditor *editor;
	GtkHTML *html;
	gint value;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	html_cursor_jump_to (
		html->engine->cursor, html->engine,
		editor->priv->table_object, 1);
	html_cursor_backward (html->engine->cursor, html->engine);

	value = gtk_spin_button_get_value_as_int (button);
	html_engine_table_set_rows (html->engine, value);

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_show_window_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	HTMLTable *table;
	HTMLClue *clue;
	GtkHTML *html;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);

	table = html_engine_get_table (html->engine);
	editor->priv->table_object = HTML_OBJECT (table);
	g_assert (HTML_IS_TABLE (table));

	value = (gdouble) table->totalRows;
	widget = WIDGET (TABLE_PROPERTIES_ROWS_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) table->totalCols;
	widget = WIDGET (TABLE_PROPERTIES_COLS_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	if (editor->priv->table_object->percent > 0) {
		value = (gdouble) editor->priv->table_object->percent;
		active = SIZE_UNIT_PERCENT;
	} else if (table->specified_width > 0) {
		value = (gdouble) table->specified_width;
		active = SIZE_UNIT_PX;
	} else {
		value = 0.0;
		active = SIZE_UNIT_PERCENT;
	}

	widget = WIDGET (TABLE_PROPERTIES_WIDTH_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = WIDGET (TABLE_PROPERTIES_WIDTH_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	active = (value > 0.0);
	widget = WIDGET (TABLE_PROPERTIES_WIDTH_CHECK_BUTTON);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

	clue = HTML_CLUE (editor->priv->table_object->parent);
	active = clue->halign - HTML_HALIGN_LEFT;
	if (active == HTML_HALIGN_NONE)
		active = HTML_HALIGN_LEFT;
	widget = WIDGET (TABLE_PROPERTIES_ALIGNMENT_COMBO_BOX);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);

	value = (gdouble) table->spacing;
	widget = WIDGET (TABLE_PROPERTIES_SPACING_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) table->padding;
	widget = WIDGET (TABLE_PROPERTIES_PADDING_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	value = (gdouble) table->border;
	widget = WIDGET (TABLE_PROPERTIES_BORDER_SPIN_BUTTON);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = WIDGET (TABLE_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_current_color (
		GTKHTML_COLOR_COMBO (widget), table->bgColor);

	if (table->bgPixmap != NULL) {
		gchar *filename;
		GError *error = NULL;

		filename = g_filename_from_uri (
			table->bgPixmap->url, NULL, &error);
		if (filename != NULL) {
			widget = WIDGET (TABLE_PROPERTIES_IMAGE_BUTTON);
			gtk_file_chooser_set_filename (
				GTK_FILE_CHOOSER (widget), filename);
			g_free (filename);
		} else {
			g_assert (error != NULL);
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}

	g_object_unref (editor);
}

AUTOCONNECTED_SIGNAL_HANDLER (void
gtkhtml_editor_table_properties_width_changed_cb (GtkWidget *window))
{
	GtkhtmlEditor *editor;
	GtkAdjustment *adjustment;
	GtkWidget *widget;
	GtkHTML *html;
	HTMLTable *table;
	gboolean sensitive;
	gdouble value;
	gint active;

	editor = extract_gtkhtml_editor (window);
	html = gtkhtml_editor_get_html (editor);
	table = HTML_TABLE (editor->priv->table_object);

	widget = WIDGET (TABLE_PROPERTIES_WIDTH_CHECK_BUTTON);
	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = WIDGET (TABLE_PROPERTIES_WIDTH_COMBO_BOX);
	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	gtk_widget_set_sensitive (widget, sensitive);

	widget = WIDGET (TABLE_PROPERTIES_WIDTH_SPIN_BUTTON);
	adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
	gtk_widget_set_sensitive (widget, sensitive);

	if (active == SIZE_UNIT_PERCENT) {
		gtk_adjustment_set_upper (adjustment, 100.0);
		gtk_adjustment_changed (adjustment);
	} else {
		gtk_adjustment_set_upper (adjustment, (gdouble) G_MAXINT);
		gtk_adjustment_changed (adjustment);
	}

	/* Clamp the value between the new bounds. */
	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (adjustment, value);
	value = gtk_adjustment_get_value (adjustment);

	html_engine_table_set_width (
		html->engine, table,
		sensitive ? (gint) value : 0,
		(active == SIZE_UNIT_PERCENT));

	g_object_unref (editor);
}
