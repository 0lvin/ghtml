/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-editor-private.c
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

#include "gtkhtml-editor-private.h"

#include <stdlib.h>

/* Regular expressions */
#define CTL	"\\x00-\\x1f\\x7f"
#define SEP	"\\x09\\x20\\(\\)<>@,;:\\\\\"/\\[\\]\\?=\\{\\}"
#define TOKEN	"[^" CTL SEP "]+"

/* Fatal error message given when a data file is not found. */
#define DATA_FILE_NOT_FOUND_MESSAGE \
"\n*** FATAL ERROR ***\n\n" \
PACKAGE " could not find the required file \"%s\" in any of\n" \
"the system-wide data directories given by the XDG_DATA_DIRS environment\n" \
"variable.  " PACKAGE " looked for:\n\n"

#define MORE_INFORMATION_MESSAGE \
"\nSee http://www.freedesktop.org/Standards/basedir-spec for more\n" \
"information about standard base directories.\n\n"

/* This controls how spelling suggestions are divided between the primary
 * context menu and a secondary menu.  The idea is to prevent the primary
 * menu from growing too long.
 *
 * The constants below are used as follows:
 *
 * if TOTAL_SUGGESTIONS <= MAX_LEVEL1_SUGGETIONS:
 *     LEVEL1_SUGGESTIONS = TOTAL_SUGGESTIONS
 * elif TOTAL_SUGGESTIONS - MAX_LEVEL1_SUGGESTIONS < MIN_LEVEL2_SUGGESTIONS:
 *     LEVEL1_SUGGESTIONS = TOTAL_SUGGESTIONS
 * else
 *     LEVEL1_SUGGESTIONS = MAX_LEVEL1_SUGGETIONS
 *
 * LEVEL2_SUGGESTIONS = TOTAL_SUGGESTIONS - LEVEL1_SUGGESTIONS
 *
 * Note that MAX_LEVEL1_SUGGETIONS is not a hard maximum.
 */
#define MAX_LEVEL1_SUGGESTIONS	4
#define MIN_LEVEL2_SUGGESTIONS	3

/************************ Begin Spell Dialog Callbacks ***********************/

static void
editor_set_word (GtkhtmlEditor *editor,
                 GtkhtmlSpellDialog *dialog)
{
	GtkHTML *html;
	gchar *word;

	html = gtkhtml_editor_get_html (editor);

	html_engine_select_spell_word_editable (html->engine);

	word = html_engine_get_spell_word (html->engine);
	gtkhtml_spell_dialog_set_word (dialog, word);
	g_free (word);
}

static void
editor_next_word_cb (GtkhtmlEditor *editor,
                     GtkhtmlSpellDialog *dialog)
{
	if (gtkhtml_editor_next_spell_error (editor))
		editor_set_word (editor, dialog);
	else
		gtkhtml_spell_dialog_close (dialog);
}

static void
editor_prev_word_cb (GtkhtmlEditor *editor,
                     GtkhtmlSpellDialog *dialog)
{
	if (gtkhtml_editor_prev_spell_error (editor))
		editor_set_word (editor, dialog);
	else
		gtkhtml_spell_dialog_close (dialog);
}

static void
editor_recheck_cb (GtkhtmlEditor *editor,
                   GtkhtmlSpellDialog *dialog)
{
	GtkHTML *html;

	html = gtkhtml_editor_get_html (editor);
	html_engine_spell_check (html->engine);
}

static void
editor_replace_cb (GtkhtmlEditor *editor,
                   const gchar *correction,
		   GtkhtmlSpellDialog *dialog)
{
	GtkHTML *html;

	html = gtkhtml_editor_get_html (editor);

	html_engine_replace_spell_word_with (html->engine, correction);
	gtkhtml_spell_dialog_next_word (dialog);
}

static void
editor_replace_all_cb (GtkhtmlEditor *editor,
                       const gchar *correction,
                       GtkhtmlSpellDialog *dialog)
{
	GtkHTML *html;
	gchar *misspelled;

	html = gtkhtml_editor_get_html (editor);

	/* Replace this occurrence. */
	misspelled = html_engine_get_spell_word (html->engine);
	html_engine_replace_spell_word_with (html->engine, correction);

	/* Replace all subsequent occurrences. */
	while (gtkhtml_editor_next_spell_error (editor)) {
		gchar *word;

		word = html_engine_get_spell_word (html->engine);
		if (g_str_equal (word, misspelled))
			html_engine_replace_spell_word_with (
				html->engine, correction);
		g_free (word);
	}

	/* Jump to beginning of document (XXX is this right?) */
	html_engine_beginning_of_document (html->engine);
	gtkhtml_spell_dialog_next_word (dialog);

	g_free (misspelled);
}

/************************* End Spell Dialog Callbacks ************************/

void
gtkhtml_editor_private_init (GtkhtmlEditor *editor)
{
	GtkhtmlEditorPrivate *priv = editor->priv;

	gchar *filename;
	GError *error = NULL;

	priv->manager = gtk_ui_manager_new ();
	priv->core_actions = gtk_action_group_new ("core");
	priv->html_actions = gtk_action_group_new ("html");
	priv->context_actions = gtk_action_group_new ("core-context");
	priv->html_context_actions = gtk_action_group_new ("html-context");
	priv->language_actions = gtk_action_group_new ("language");
	priv->spell_check_actions = gtk_action_group_new ("spell-check");
	priv->suggestion_actions = gtk_action_group_new ("suggestion");

	/* GtkhtmlSpellLanguage -> GtkhtmlSpellChecker */
	priv->available_spell_checkers = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_object_unref);

	/* GtkhtmlSpellLanguage -> UI Merge ID */
	priv->spell_suggestion_menus =
		g_hash_table_new (g_direct_hash, g_direct_equal);

	filename = gtkhtml_editor_find_data_file ("gtkhtml-editor-manager.ui");

	if (!gtk_ui_manager_add_ui_from_file (priv->manager, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}

	g_free (filename);

	filename = gtkhtml_editor_find_data_file ("gtkhtml-editor-builder.ui");

	priv->builder = gtk_builder_new ();
	/* To keep translated strings in subclasses */
        gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
	if (!gtk_builder_add_from_file (priv->builder, filename, &error)) {
		g_critical ("Couldn't load builder file: %s\n", error->message);
		g_clear_error (&error);
	}

	g_free (filename);

	gtkhtml_editor_actions_init (editor);

	gtk_window_add_accel_group (
		GTK_WINDOW (editor),
		gtk_ui_manager_get_accel_group (priv->manager));

	gtk_builder_connect_signals (priv->builder, NULL);

	/* Wait to construct the main window widgets
	 * until the 'html' property is initialized. */
}

void
gtkhtml_editor_private_constructed (GtkhtmlEditor *editor)
{
	GtkhtmlEditorPrivate *priv = editor->priv;

	GtkHTML *html;
	GtkWidget *widget;
	GtkToolbar *toolbar;
	GtkToolItem *tool_item;

	/* Construct main window widgets. */

	widget = gtkhtml_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtkhtml_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtkhtml_editor_get_managed_widget (editor, "/edit-toolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	priv->edit_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtkhtml_editor_get_managed_widget (editor, "/html-toolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	priv->html_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, TRUE, TRUE, 0);
	priv->scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (gtkhtml_editor_get_html (editor));
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window), widget);
	gtk_widget_show (widget);

	/* Add some combo boxes to the "edit" toolbar. */

	toolbar = GTK_TOOLBAR (priv->edit_toolbar);

	tool_item = gtk_tool_item_new ();
	widget = gtkhtml_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (STYLE_NORMAL)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Paragraph Style"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->style_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (toolbar, tool_item, 0);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_tool_item_new ();
	widget = gtkhtml_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (MODE_HTML)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Editing Mode"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->mode_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	/* Add some combo boxes to the "html" toolbar. */

	toolbar = GTK_TOOLBAR (priv->html_toolbar);

	tool_item = gtk_tool_item_new ();
	widget = gtkhtml_color_combo_new ();
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Color"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->color_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	tool_item = gtk_tool_item_new ();
	widget = gtkhtml_combo_box_new_with_action (
		GTK_RADIO_ACTION (ACTION (SIZE_PLUS_ZERO)));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (tool_item), widget);
	gtk_widget_set_tooltip_text (widget, _("Font Size"));
	gtk_toolbar_insert (toolbar, tool_item, 0);
	priv->size_combo_box = g_object_ref (widget);
	gtk_widget_show_all (GTK_WIDGET (tool_item));

	/* Initialize painters (requires "edit_area"). */

	html = gtkhtml_editor_get_html (editor);
	gtk_widget_ensure_style (GTK_WIDGET (html));
	priv->html_painter = g_object_ref (html->engine->painter);
	priv->plain_painter = html_plain_painter_new (priv->edit_area, TRUE);

	/* Add input methods to the context menu. */

	widget = gtkhtml_editor_get_managed_widget (
		editor, "/context-menu/context-input-methods-menu");
	widget = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
	gtk_im_multicontext_append_menuitems (
		GTK_IM_MULTICONTEXT (html->priv->im_context),
		GTK_MENU_SHELL (widget));

	/* Configure color stuff. */

	priv->palette = gtkhtml_color_palette_new ();
	priv->text_color = gtkhtml_color_state_new ();

	gtkhtml_color_state_set_default_label (
		priv->text_color, _("Automatic"));
	gtkhtml_color_state_set_palette (
		priv->text_color, priv->palette);

	/* Text color widgets share state. */

	widget = priv->color_combo_box;
	gtkhtml_color_combo_set_state (
		GTKHTML_COLOR_COMBO (widget), priv->text_color);

	widget = WIDGET (TEXT_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_state (
		GTKHTML_COLOR_COMBO (widget), priv->text_color);

	/* These color widgets share a custom color palette. */

	widget = WIDGET (CELL_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (PAGE_PROPERTIES_BACKGROUND_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (PAGE_PROPERTIES_LINK_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);

	widget = WIDGET (TABLE_PROPERTIES_COLOR_COMBO);
	gtkhtml_color_combo_set_palette (
		GTKHTML_COLOR_COMBO (widget), priv->palette);
}

void
gtkhtml_editor_private_dispose (GtkhtmlEditor *editor)
{
	GtkhtmlEditorPrivate *priv = editor->priv;

	/* Disconnect signal handlers from the color
	 * state object since it may live on. */
	if (priv->text_color != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->text_color, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, editor);
	}

	DISPOSE (priv->manager);
	DISPOSE (priv->core_actions);
	DISPOSE (priv->html_actions);
	DISPOSE (priv->context_actions);
	DISPOSE (priv->html_context_actions);
	DISPOSE (priv->language_actions);
	DISPOSE (priv->spell_check_actions);
	DISPOSE (priv->suggestion_actions);
	DISPOSE (priv->builder);

	DISPOSE (priv->html_painter);
	DISPOSE (priv->plain_painter);

	g_hash_table_remove_all (priv->available_spell_checkers);

	g_list_foreach (
		priv->active_spell_checkers,
		(GFunc) g_object_unref, NULL);
	g_list_free (priv->active_spell_checkers);
	priv->active_spell_checkers = NULL;

	DISPOSE (priv->main_menu);
	DISPOSE (priv->main_toolbar);
	DISPOSE (priv->edit_toolbar);
	DISPOSE (priv->html_toolbar);
	DISPOSE (priv->edit_area);

	DISPOSE (priv->color_combo_box);
	DISPOSE (priv->mode_combo_box);
	DISPOSE (priv->size_combo_box);
	DISPOSE (priv->style_combo_box);
	DISPOSE (priv->scrolled_window);

	DISPOSE (priv->palette);
	DISPOSE (priv->text_color);
}

void
gtkhtml_editor_private_finalize (GtkhtmlEditor *editor)
{
	GtkhtmlEditorPrivate *priv = editor->priv;

	/* All URI requests should be complete or cancelled by now. */
	if (priv->requests != NULL)
		g_warning ("Finalizing GtkhtmlEditor with active URI requests");

	g_hash_table_destroy (priv->available_spell_checkers);
	g_hash_table_destroy (priv->spell_suggestion_menus);

	g_free (priv->filename);
	g_free (priv->current_folder);
}

gchar *
gtkhtml_editor_find_data_file (const gchar *basename)
{
	const gchar * const *datadirs;
	gchar *filename;

	g_return_val_if_fail (basename != NULL, NULL);

	/* Support running directly from the source tree. */
	filename = g_build_filename (".", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	/* Check our own installation prefix. */
	filename = g_build_filename (
		DATADIR, GTKHTML_RELEASE_STRING, basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	/* Check the standard system data directories. */
	datadirs = g_get_system_data_dirs ();
	while (*datadirs != NULL) {
		filename = g_build_filename (
			*datadirs++, GTKHTML_RELEASE_STRING, basename, NULL);
		if (g_file_test (filename, G_FILE_TEST_EXISTS))
			return filename;
		g_free (filename);
	}

	/* Print a helpful message and die. */
	g_printerr (DATA_FILE_NOT_FOUND_MESSAGE, basename);
	filename = g_build_filename (
		DATADIR, GTKHTML_RELEASE_STRING, basename, NULL);
	g_printerr ("\t%s\n", filename);
	g_free (filename);
	datadirs = g_get_system_data_dirs ();
	while (*datadirs != NULL) {
		filename = g_build_filename (
			*datadirs++, GTKHTML_RELEASE_STRING, basename, NULL);
		g_printerr ("\t%s\n", filename);
		g_free (filename);
	}
	g_printerr (MORE_INFORMATION_MESSAGE);
	abort ();

	return NULL;  /* never gets here */
}

gint
gtkhtml_editor_insert_file (GtkhtmlEditor *editor,
                            const gchar *title,
                            GCallback response_cb)
{
	GtkWidget *dialog;
	gint response;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), GTK_RESPONSE_CANCEL);
	g_return_val_if_fail (response_cb != NULL, GTK_RESPONSE_CANCEL);

	dialog = gtk_file_chooser_dialog_new (
		title, GTK_WINDOW (editor),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK,
		NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	g_signal_connect (dialog, "response", response_cb, editor);

	response = gtkhtml_editor_file_chooser_dialog_run (editor, dialog);

	gtk_widget_destroy (dialog);

	return response;
}

GFile *
gtkhtml_editor_run_open_dialog (GtkhtmlEditor *editor,
                                const gchar *title,
                                GtkCallback customize_func,
                                gpointer customize_data)
{
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkWidget *dialog;
	gchar *uri;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	dialog = gtk_file_chooser_dialog_new (
		title, GTK_WINDOW (editor),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);

	/* Restore the current folder from the previous file chooser. */
	uri = (gchar *) gtkhtml_editor_get_current_folder (editor);
	if (uri != NULL)
		gtk_file_chooser_set_current_folder_uri (file_chooser, uri);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (dialog, customize_data);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
		goto exit;

	chosen_file = gtk_file_chooser_get_file (file_chooser);

	/* Save the current folder for subsequent file choosers. */
	uri = gtk_file_chooser_get_current_folder_uri (file_chooser);
	gtkhtml_editor_set_current_folder (editor, uri);
	g_free (uri);

exit:
	gtk_widget_destroy (dialog);

	return chosen_file;
}

void
gtkhtml_editor_show_uri (GtkWindow *parent,
                         const gchar *uri)
{
	GtkWidget *dialog;
	GdkScreen *screen = NULL;
	GError *error = NULL;
	guint32 timestamp;

	g_return_if_fail (uri != NULL);

	timestamp = gtk_get_current_event_time ();

	if (parent != NULL)
		screen = gtk_widget_get_screen (GTK_WIDGET (parent));

	if (gtk_show_uri (screen, uri, timestamp, &error))
		return;

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		_("Could not open the link."));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

void
gtkhtml_editor_spell_check (GtkhtmlEditor *editor,
                            gboolean whole_document)
{
	GtkHTML *html;
	gboolean spelling_errors;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	if (whole_document) {
		html_engine_disable_selection (html->engine);
		html_engine_beginning_of_document (html->engine);
		gtk_html_set_inline_spelling (html, TRUE);
	}

	spelling_errors =
		!html_engine_spell_word_is_valid (html->engine) ||
		gtkhtml_editor_next_spell_error (editor);

	if (spelling_errors) {
		GtkWidget *dialog;

		dialog = gtkhtml_spell_dialog_new (GTK_WINDOW (editor));

		gtkhtml_spell_dialog_set_spell_checkers (
			GTKHTML_SPELL_DIALOG (dialog),
			editor->priv->active_spell_checkers);

		editor_set_word (editor, GTKHTML_SPELL_DIALOG (dialog));

		g_signal_connect_swapped (
			dialog, "added",
			G_CALLBACK (editor_recheck_cb), editor);

		g_signal_connect_swapped (
			dialog, "ignored",
			G_CALLBACK (editor_recheck_cb), editor);

		g_signal_connect_swapped (
			dialog, "next-word",
			G_CALLBACK (editor_next_word_cb), editor);

		g_signal_connect_swapped (
			dialog, "prev-word",
			G_CALLBACK (editor_prev_word_cb), editor);

		g_signal_connect_swapped (
			dialog, "replace",
			G_CALLBACK (editor_replace_cb), editor);

		g_signal_connect_swapped (
			dialog, "replace-all",
			G_CALLBACK (editor_replace_all_cb), editor);

		gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);
	}

	/* XXX Restore original inline spelling value. */
}

gboolean
gtkhtml_editor_next_spell_error (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	gboolean found = FALSE;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	html_engine_disable_selection (html->engine);

	while (!found && html_engine_forward_word (html->engine))
		found = !html_engine_spell_word_is_valid (html->engine);

	return found;
}

gboolean
gtkhtml_editor_prev_spell_error (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	gboolean found = FALSE;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	html_engine_disable_selection (html->engine);
	html_engine_backward_word (html->engine);

	while (!found && html_engine_backward_word (html->engine))
		found = !html_engine_spell_word_is_valid (html->engine);

	return found;
}

/* Action callback for context menu spelling suggestions.
 * XXX This should really be in gtkhtml-editor-actions.c */
static void
action_context_spell_suggest_cb (GtkAction *action,
                                 GtkhtmlEditor *editor)
{
	GtkHTML *html;
	const gchar *word;

	html = gtkhtml_editor_get_html (editor);

	word = g_object_get_data (G_OBJECT (action), "word");
	g_return_if_fail (word != NULL);

	html_engine_replace_spell_word_with (html->engine, word);
}

static void
editor_inline_spelling_suggestions (GtkhtmlEditor *editor,
                                    GtkhtmlSpellChecker *checker)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GtkHTML *html;
	GList *list;
	const gchar *path;
	gchar *word;
	guint count = 0;
	guint length;
	guint merge_id;
	guint threshold;

	html = gtkhtml_editor_get_html (editor);
	word = html_engine_get_spell_word (html->engine);
	list = gtkhtml_spell_checker_get_suggestions (checker, word, -1);

	path = "/context-menu/context-spell-suggest/";
	manager = gtkhtml_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;
	merge_id = editor->priv->spell_suggestions_merge_id;

	/* Calculate how many suggestions to put directly in the
	 * context menu.  The rest will go in a secondary menu. */
	length = g_list_length (list);
	if (length <= MAX_LEVEL1_SUGGESTIONS)
		threshold = length;
	else if (length - MAX_LEVEL1_SUGGESTIONS < MIN_LEVEL2_SUGGESTIONS)
		threshold = length;
	else
		threshold = MAX_LEVEL1_SUGGESTIONS;

	while (list != NULL) {
		gchar *suggestion = list->data;
		gchar *action_name;
		gchar *action_label;
		GtkAction *action;
		GtkWidget *child;
		GSList *proxies;

		/* Once we reach the threshold, put all subsequent
		 * spelling suggestions in a secondary menu. */
		if (count == threshold)
			path = "/context-menu/context-more-suggestions-menu/";

		/* Action name just needs to be unique. */
		action_name = g_strdup_printf ("suggest-%d", count++);

		action_label = g_markup_printf_escaped (
			"<b>%s</b>", suggestion);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_object_set_data_full (
			G_OBJECT (action), "word",
			g_strdup (suggestion), g_free);

		g_signal_connect (
			action, "activate", G_CALLBACK (
			action_context_spell_suggest_cb), editor);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			manager, merge_id, path,
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		/* XXX GtkAction offers no support for Pango markup,
		 *     so we have to manually set "use-markup" on the
		 *     child of the proxy widget. */
		gtk_ui_manager_ensure_update (manager);
		proxies = gtk_action_get_proxies (action);
		child = gtk_bin_get_child (proxies->data);
		g_object_set (child, "use-markup", TRUE, NULL);

		g_free (suggestion);
		g_free (action_name);
		g_free (action_label);

		list = g_list_delete_link (list, list);
	}

	g_free (word);
}

/* Helper for gtkhtml_editor_update_context() */
static void
editor_spell_checkers_foreach (GtkhtmlSpellChecker *checker,
                               GtkhtmlEditor *editor)
{
	const GtkhtmlSpellLanguage *language;
	const gchar *language_code;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GtkHTML *html;
	GList *list;
	gchar *path;
	gchar *word;
	gint count = 0;
	guint merge_id;

	language = gtkhtml_spell_checker_get_language (checker);
	language_code = gtkhtml_spell_language_get_code (language);

	html = gtkhtml_editor_get_html (editor);
	word = html_engine_get_spell_word (html->engine);
	list = gtkhtml_spell_checker_get_suggestions (checker, word, -1);

	manager = gtkhtml_editor_get_ui_manager (editor);
	action_group = editor->priv->suggestion_actions;
	merge_id = editor->priv->spell_suggestions_merge_id;

	path = g_strdup_printf (
		"/context-menu/context-spell-suggest/"
		"context-spell-suggest-%s-menu", language_code);

	while (list != NULL) {
		gchar *suggestion = list->data;
		gchar *action_name;
		gchar *action_label;
		GtkAction *action;
		GtkWidget *child;
		GSList *proxies;

		/* Action name just needs to be unique. */
		action_name = g_strdup_printf (
			"suggest-%s-%d", language_code, count++);

		action_label = g_markup_printf_escaped (
			"<b>%s</b>", suggestion);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_object_set_data_full (
			G_OBJECT (action), "word",
			g_strdup (suggestion), g_free);

		g_signal_connect (
			action, "activate", G_CALLBACK (
			action_context_spell_suggest_cb), editor);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			manager, merge_id, path,
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		/* XXX GtkAction offers no supports for Pango markup,
		 *     so we have to manually set "use-markup" on the
		 *     child of the proxy widget. */
		gtk_ui_manager_ensure_update (manager);
		proxies = gtk_action_get_proxies (action);
		child = gtk_bin_get_child (proxies->data);
		g_object_set (child, "use-markup", TRUE, NULL);

		g_free (suggestion);
		g_free (action_name);
		g_free (action_label);

		list = g_list_delete_link (list, list);
	}

	g_free (path);
	g_free (word);
}

void
gtkhtml_editor_update_context (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	HTMLType type;
	HTMLObject *object;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *list;
	gboolean visible;
	guint merge_id;

	html = gtkhtml_editor_get_html (editor);
	manager = gtkhtml_editor_get_ui_manager (editor);
	gtk_html_update_styles (html);

	/* Update context menu item visibility. */

	object = html->engine->cursor->object;

	if (object != NULL)
		type = HTML_OBJECT_TYPE (object);
	else
		type = HTML_TYPE_NONE;

	visible = (type == HTML_TYPE_IMAGE);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_IMAGE), visible);

	visible = (type == HTML_TYPE_LINKTEXT);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_LINK), visible);

	visible = (type == HTML_TYPE_RULE);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_RULE), visible);

	visible = (type == HTML_TYPE_TEXT);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_TEXT), visible);

	visible =
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_IMAGE)) ||
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_LINK)) ||
		gtk_action_get_visible (ACTION (CONTEXT_PROPERTIES_TEXT));
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_PARAGRAPH), visible);

	/* Set to visible if any of these are true:
	 *   - Selection is active and contains a link.
	 *   - Cursor is on a link.
	 *   - Cursor is on an image that has a URL or target.
	 */
	visible =
		(html_engine_is_selection_active (html->engine) &&
		 html_engine_selection_contains_link (html->engine)) ||
		(type == HTML_TYPE_LINKTEXT) ||
		(type == HTML_TYPE_IMAGE &&
			(HTML_IMAGE (object)->url != NULL ||
			 HTML_IMAGE (object)->target != NULL));
	gtk_action_set_visible (ACTION (CONTEXT_REMOVE_LINK), visible);

	/* Get the parent object. */
	object = (object != NULL) ? object->parent : NULL;

	/* Get the grandparent object. */
	object = (object != NULL) ? object->parent : NULL;

	if (object != NULL)
		type = HTML_OBJECT_TYPE (object);
	else
		type = HTML_TYPE_NONE;

	visible = (type == HTML_TYPE_TABLECELL);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_CELL), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_COLUMN), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_ROW), visible);
	gtk_action_set_visible (ACTION (CONTEXT_DELETE_TABLE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_COLUMN_AFTER), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_COLUMN_BEFORE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_ROW_ABOVE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_ROW_BELOW), visible);
	gtk_action_set_visible (ACTION (CONTEXT_INSERT_TABLE), visible);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_CELL), visible);

	/* Get the great grandparent object. */
	object = (object != NULL) ? object->parent : NULL;

	if (object != NULL)
		type = HTML_OBJECT_TYPE (object);
	else
		type = HTML_TYPE_NONE;

	/* Note the |= (cursor must be in a table cell). */
	visible |= (type == HTML_TYPE_TABLE);
	gtk_action_set_visible (ACTION (CONTEXT_PROPERTIES_TABLE), visible);

	/********************** Spell Check Suggestions **********************/

	object = html->engine->cursor->object;
	action_group = editor->priv->suggestion_actions;

	/* Remove the old content from the context menu. */
	merge_id = editor->priv->spell_suggestions_merge_id;
	if (merge_id > 0) {
		gtk_ui_manager_remove_ui (manager, merge_id);
		editor->priv->spell_suggestions_merge_id = 0;
	}

	/* Clear the action group for spelling suggestions. */
	list = gtk_action_group_list_actions (action_group);
	while (list != NULL) {
		GtkAction *action = list->data;

		gtk_action_group_remove_action (action_group, action);
		list = g_list_delete_link (list, list);
	}

	/* Decide if we should show spell checking items. */
	visible =
		!html_engine_is_selection_active (html->engine) &&
		object != NULL && html_object_is_text (object) &&
		!html_engine_spell_word_is_valid (html->engine);
	action_group = editor->priv->spell_check_actions;
	gtk_action_group_set_visible (action_group, visible);

	/* Exit early if spell checking items are invisible. */
	if (!visible)
		return;

	list = editor->priv->active_spell_checkers;
	merge_id = gtk_ui_manager_new_merge_id (manager);
	editor->priv->spell_suggestions_merge_id = merge_id;

	/* Handle a single active language as a special case. */
	if (g_list_length (list) == 1) {
		editor_inline_spelling_suggestions (editor, list->data);
		return;
	}

	/* Add actions and context menu content for active languages. */
	g_list_foreach (list, (GFunc) editor_spell_checkers_foreach, editor);
}
