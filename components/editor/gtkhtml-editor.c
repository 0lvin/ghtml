/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-editor.c
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

#include "gtkhtml-editor.h"

#include <string.h>

#include "gtkhtml-editor-private.h"

enum {
	PROP_0,
	PROP_CURRENT_FOLDER,
	PROP_FILENAME,
	PROP_HTML,
	PROP_HTML_MODE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_CHANGED
};

enum {
	COMMAND_AFTER,
	COMMAND_BEFORE,
	IMAGE_URI,
	LINK_CLICKED,
	OBJECT_DELETED,
	URI_REQUESTED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
editor_alignment_changed_cb (GtkhtmlEditor *editor,
                             GtkHTMLParagraphAlignment alignment)
{
	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (ACTION (JUSTIFY_CENTER)), alignment);
}

static void
editor_show_popup_menu (GtkhtmlEditor *editor,
                        GdkEventButton *event,
                        HTMLObject *object,
			guint offset)
{
	GtkHTML *html;
	GtkWidget *menu;
	gboolean in_selection;

	html = gtkhtml_editor_get_html (editor);
	menu = gtkhtml_editor_get_managed_widget (editor, "/context-menu");

	/* Did we right-click in a selection? */
	in_selection =
		html_engine_is_selection_active (html->engine) &&
		html_engine_point_in_selection (html->engine, object, offset);

	if (!in_selection) {
		html_engine_disable_selection (html->engine);
		if (event != NULL)
			html_engine_jump_at (
				html->engine, event->x, event->y);
	}

	gtkhtml_editor_update_context (editor);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

static gboolean
editor_button_press_event_cb (GtkhtmlEditor *editor,
                              GdkEventButton *event)
{
	GtkHTML *html;
	HTMLObject *object;
	guint offset;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (event->button != 3)
		return FALSE;

	html = gtkhtml_editor_get_html (editor);

	object = html_engine_get_object_at (
		html->engine, event->x, event->y, &offset, FALSE);

	editor_show_popup_menu (editor, event, object, offset);

	return TRUE;
}

static void
editor_font_style_changed_cb (GtkhtmlEditor *editor,
                              GtkHTMLFontStyle style)
{
	GtkHTMLFontStyle size;

	editor->priv->ignore_style_change++;

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (ACTION (BOLD)),
		style & GTK_HTML_FONT_STYLE_BOLD);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (ACTION (MONOSPACED)),
		style & GTK_HTML_FONT_STYLE_FIXED);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (ACTION (ITALIC)),
		style & GTK_HTML_FONT_STYLE_ITALIC);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (ACTION (STRIKETHROUGH)),
		style & GTK_HTML_FONT_STYLE_STRIKEOUT);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (ACTION (UNDERLINE)),
		style & GTK_HTML_FONT_STYLE_UNDERLINE);

	size = style & GTK_HTML_FONT_STYLE_SIZE_MASK;
	if (size == GTK_HTML_FONT_STYLE_DEFAULT)
		size = GTK_HTML_FONT_STYLE_SIZE_3;

	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (ACTION (SIZE_PLUS_ZERO)), size);

	editor->priv->ignore_style_change--;
}

static void
editor_indentation_changed_cb (GtkhtmlEditor *editor,
                               guint indentation_level)
{
	gtk_action_set_sensitive (ACTION (UNINDENT), indentation_level > 0);
}

static void
editor_paragraph_style_changed_cb (GtkhtmlEditor *editor,
                                   GtkHTMLParagraphStyle style)
{
	editor->priv->ignore_style_change++;

	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (ACTION (STYLE_NORMAL)), style);

	editor->priv->ignore_style_change--;
}

static gboolean
editor_popup_menu_cb (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	HTMLObject *object;

	html = gtkhtml_editor_get_html (editor);
	object = html->engine->cursor->object;

	editor_show_popup_menu (editor, NULL, object, html->engine->cursor->offset);

	return TRUE;
}

static void
editor_text_color_changed_cb (GtkhtmlEditor *editor)
{
	GtkhtmlColorState *state;
	GdkColor gdk_color;
	GtkHTML *html;

	state = editor->priv->text_color;
	html = gtkhtml_editor_get_html (editor);

	if (gtkhtml_color_state_get_current_color (state, &gdk_color)) {
		HTMLColor *color;

		color = html_color_new_from_gdk_color (&gdk_color);
		gtk_html_set_color (html, color);
		html_color_unref (color);
	} else
		gtk_html_set_color (html, NULL);
}

static void
editor_url_requested_cb (GtkhtmlEditor *editor,
                         const gchar *url,
                         GtkHTMLStream *stream)
{
	g_signal_emit (editor, signals[URI_REQUESTED], 0, url, stream);
}

static void
engine_undo_changed_cb (GtkhtmlEditor *editor, HTMLEngine *engine)
{
	g_object_notify (G_OBJECT (editor), "changed");
}

static void
editor_command_popup_menu (GtkhtmlEditor *editor)
{
	g_warning ("GtkHTML command \"popup-menu\" not implemented");
}

static void
editor_command_properties_dialog (GtkhtmlEditor *editor)
{
	g_warning ("GtkHTML command \"property-dialog\" not implemented");
}

static void
editor_command_text_color_apply (GtkhtmlEditor *editor)
{
	GObject *object;

	object = G_OBJECT (editor->priv->text_color);
	g_object_notify (object, "current-color");
}

static gboolean
editor_method_check_word (GtkHTML *html,
                          const gchar *word,
                          gpointer user_data)
{
	GtkhtmlEditor *editor = user_data;
	gboolean correct = FALSE;
	GList *list;

	list = editor->priv->active_spell_checkers;

	/* If no spell checkers are active, assume the word is correct. */
	if (list == NULL)
		return TRUE;

	/* The word is correct if ANY active spell checker can verify it. */
	while (list != NULL && !correct) {
		GtkhtmlSpellChecker *checker = list->data;

		correct = gtkhtml_spell_checker_check_word (checker, word, -1);
		list = g_list_next (list);
	}

	return correct;
}

static void
editor_method_suggestion_request (GtkHTML *html,
                                  gpointer user_data)
{
	g_warning ("GtkHTML suggestion_request() method not implemented");
}

static void
editor_method_add_to_session (GtkHTML *html,
                              const gchar *word,
                              gpointer user_data)
{
	g_warning ("GtkHTML add_to_session() method not implemented");
}

static void
editor_method_add_to_personal (GtkHTML *html,
                               const gchar *word,
                               const gchar *language_code,
                               gpointer user_data)
{
	GtkhtmlEditor *editor = user_data;
	const GtkhtmlSpellLanguage *language;
	GtkhtmlSpellChecker *checker;
	GHashTable *hash_table;

	language = gtkhtml_spell_language_lookup (language_code);
	g_return_if_fail (language != NULL);

	hash_table = editor->priv->available_spell_checkers;
	checker = g_hash_table_lookup (hash_table, language);
	g_return_if_fail (checker != NULL);

	gtkhtml_spell_checker_add_word (checker, word, -1);
}

static gboolean
editor_method_command (GtkHTML *html,
                       GtkHTMLCommandType command,
                       gpointer user_data)
{
	GtkhtmlEditor *editor = user_data;
	gboolean handled = TRUE;

	switch (command) {
		case GTK_HTML_COMMAND_POPUP_MENU:
			editor_command_popup_menu (editor);
			break;

		case GTK_HTML_COMMAND_PROPERTIES_DIALOG:
			editor_command_properties_dialog (editor);
			break;

		case GTK_HTML_COMMAND_TEXT_COLOR_APPLY:
			editor_command_text_color_apply (editor);
			break;

		default:
			handled = FALSE;
			break;
	}

	return handled;
}

static GValue *
editor_method_event (GtkHTML *html,
                     GtkHTMLEditorEventType event,
                     GValue *args,
                     gpointer user_data)
{
	GtkhtmlEditor *editor = user_data;
	GValue *return_value = NULL;
	gchar *return_string = NULL;
	const gchar *string = NULL;
	guint signal_id;

	/* GtkHTML event arguments are either NULL or a single string. */
	if (args != NULL && G_VALUE_HOLDS (args, G_TYPE_STRING))
		string = g_value_get_string (args);

	switch (event) {
		case GTK_HTML_EDITOR_EVENT_COMMAND_BEFORE:
			/* Signal argument is the command name. */
			signal_id = signals[COMMAND_BEFORE];
			g_return_val_if_fail (string != NULL, NULL);
			g_signal_emit (editor, signal_id, 0, string);
			break;

		case GTK_HTML_EDITOR_EVENT_COMMAND_AFTER:
			/* Signal argument is the command name. */
			signal_id = signals[COMMAND_AFTER];
			g_return_val_if_fail (string != NULL, NULL);
			g_signal_emit (editor, signal_id, 0, string);
			break;

		case GTK_HTML_EDITOR_EVENT_IMAGE_URL:
			/* Signal argument is the image URL. */
			/* XXX Returns a string. */
			signal_id = signals[IMAGE_URI];
			g_return_val_if_fail (string != NULL, NULL);
			g_signal_emit (
				editor, signal_id, 0, string, &return_string);
			return_value = g_new0 (GValue, 1);
			g_value_init (return_value, G_TYPE_STRING);
			g_value_take_string (return_value, return_string);
			break;

		case GTK_HTML_EDITOR_EVENT_DELETE:
			/* No signal arguments. */
			signal_id = signals[OBJECT_DELETED];
			g_return_val_if_fail (string == NULL, NULL);
			g_signal_emit (editor, signal_id, 0);
			break;

		case GTK_HTML_EDITOR_EVENT_LINK_CLICKED:
			/* Signal argument is the link URL. */
			signal_id = signals[LINK_CLICKED];
			g_return_val_if_fail (string != NULL, NULL);
			g_signal_emit (editor, signal_id, 0, string);
			break;
	}

	return return_value;
}

static GtkWidget *
editor_method_create_input_line (GtkHTML *html,
                                 gpointer user_data)
{
	g_warning ("GtkHTML create_input_line() method not implemented");

	return NULL;
}

static void
editor_method_set_language (GtkHTML *html,
                            const gchar *language,
                            gpointer user_data)
{
	GtkhtmlEditor *editor = user_data;
	GtkActionGroup *action_group;
	GtkAction *action;
	gchar *action_name;

	action_group = editor->priv->language_actions;
	action_name = g_strdelimit (g_strdup (language), "-", '_');
	action = gtk_action_group_get_action (action_group, action_name);
	g_free (action_name);

	if (action != NULL)
		gtk_action_activate (action);
	else
		g_warning ("%s: No such language", language);
}

static GtkHTMLEditorAPI editor_api = {
	editor_method_check_word,
	editor_method_suggestion_request,
	editor_method_add_to_session,
	editor_method_add_to_personal,
	editor_method_command,
	editor_method_event,
	editor_method_create_input_line,
	editor_method_set_language
};

static void
editor_set_html (GtkhtmlEditor *editor,
                 GtkHTML *html)
{
	g_return_if_fail (editor->priv->edit_area == NULL);

	if (html == NULL)
		html = (GtkHTML *) gtk_html_new ();
	else
		g_return_if_fail (GTK_IS_HTML (html));

	gtk_html_load_empty (html);
	gtk_html_set_editable (html, TRUE);

	editor->priv->edit_area = g_object_ref_sink (html);
}

static GObject *
editor_constructor (GType type,
                    guint n_construct_properties,
                    GObjectConstructParam *construct_properties)
{
	GtkHTML *html;
	GtkhtmlEditor *editor;
	GObject *object;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	editor = GTKHTML_EDITOR (object);

	gtkhtml_editor_private_constructed (editor);

	html = gtkhtml_editor_get_html (editor);
	gtk_html_set_editor_api (html, &editor_api, editor);

	gtk_container_add (GTK_CONTAINER (object), editor->vbox);
	gtk_window_set_default_size (GTK_WINDOW (object), 600, 440);

	/* Listen for events from core and update the UI accordingly. */

	g_signal_connect_swapped (
		html, "button_press_event",
		G_CALLBACK (editor_button_press_event_cb), editor);

	g_signal_connect_swapped (
		html, "current_paragraph_alignment_changed",
		G_CALLBACK (editor_alignment_changed_cb), editor);

	g_signal_connect_swapped (
		html, "current_paragraph_indentation_changed",
		G_CALLBACK (editor_indentation_changed_cb), editor);

	g_signal_connect_swapped (
		html, "current_paragraph_style_changed",
		G_CALLBACK (editor_paragraph_style_changed_cb), editor);

	g_signal_connect_swapped (
		html, "insertion_font_style_changed",
		G_CALLBACK (editor_font_style_changed_cb), editor);

	g_signal_connect_swapped (
		html, "popup-menu",
		G_CALLBACK (editor_popup_menu_cb), editor);

	g_signal_connect_swapped (
		html, "url_requested",
		G_CALLBACK (editor_url_requested_cb), editor);

	g_signal_connect_swapped (
		html->engine, "undo-changed",
		G_CALLBACK (engine_undo_changed_cb), editor);

	/* Connect property dialog widgets to actions. */

	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (TEXT_PROPERTIES_BOLD_BUTTON)),
		ACTION (BOLD));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (REPLACE_CONFIRMATION_REPLACE_BUTTON)),
		ACTION (CONFIRM_REPLACE));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (REPLACE_CONFIRMATION_REPLACE_ALL_BUTTON)),
		ACTION (CONFIRM_REPLACE_ALL));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (REPLACE_CONFIRMATION_CLOSE_BUTTON)),
		ACTION (CONFIRM_REPLACE_CANCEL));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (REPLACE_CONFIRMATION_NEXT_BUTTON)),
		ACTION (CONFIRM_REPLACE_NEXT));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (FIND_BUTTON)),
		ACTION (FIND));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (REPLACE_BUTTON)),
		ACTION (FIND_AND_REPLACE));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (TEXT_PROPERTIES_ITALIC_BUTTON)),
		ACTION (ITALIC));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (PARAGRAPH_PROPERTIES_CENTER_BUTTON)),
		ACTION (JUSTIFY_CENTER));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (PARAGRAPH_PROPERTIES_LEFT_BUTTON)),
		ACTION (JUSTIFY_LEFT));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (PARAGRAPH_PROPERTIES_RIGHT_BUTTON)),
		ACTION (JUSTIFY_RIGHT));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (TEXT_PROPERTIES_STRIKETHROUGH_BUTTON)),
		ACTION (STRIKETHROUGH));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (LINK_PROPERTIES_TEST_BUTTON)),
		ACTION (TEST_URL));
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (WIDGET (TEXT_PROPERTIES_UNDERLINE_BUTTON)),
		ACTION (UNDERLINE));

	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (CELL_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (FIND_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (IMAGE_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (LINK_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (PAGE_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (PARAGRAPH_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (REPLACE_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (REPLACE_CONFIRMATION_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (RULE_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (TABLE_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));
	gtk_window_set_transient_for (
		GTK_WINDOW (WIDGET (TEXT_PROPERTIES_WINDOW)),
		GTK_WINDOW (object));

	/* Initialize various GtkhtmlComboBox instances. */
	g_object_set (
		WIDGET (PARAGRAPH_PROPERTIES_STYLE_COMBO_BOX),
		"action", ACTION (STYLE_NORMAL), NULL);
	g_object_set (
		WIDGET (TEXT_PROPERTIES_SIZE_COMBO_BOX),
		"action", ACTION (SIZE_PLUS_ZERO), NULL);

	g_signal_connect_swapped (
		editor->priv->text_color, "notify::current-color",
		G_CALLBACK (editor_text_color_changed_cb), editor);

	gtkhtml_editor_set_html_mode (editor, TRUE);

	return object;
}

static void
editor_set_property (GObject *object,
                     guint property_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FOLDER:
			gtkhtml_editor_set_current_folder (
				GTKHTML_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_FILENAME:
			gtkhtml_editor_set_filename (
				GTKHTML_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_HTML:
			editor_set_html (
				GTKHTML_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_HTML_MODE:
			gtkhtml_editor_set_html_mode (
				GTKHTML_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			gtkhtml_editor_set_inline_spelling (
				GTKHTML_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			gtkhtml_editor_set_magic_links (
				GTKHTML_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			gtkhtml_editor_set_magic_smileys (
				GTKHTML_EDITOR (object),
				g_value_get_boolean (value));
			return;
		case PROP_CHANGED:
			gtkhtml_editor_set_changed (
				GTKHTML_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_get_property (GObject *object,
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FOLDER:
			g_value_set_string (
				value, gtkhtml_editor_get_current_folder (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_FILENAME:
			g_value_set_string (
				value, gtkhtml_editor_get_filename (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_HTML:
			g_value_set_object (
				value, gtkhtml_editor_get_html (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, gtkhtml_editor_get_html_mode (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, gtkhtml_editor_get_inline_spelling (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, gtkhtml_editor_get_magic_links (
				GTKHTML_EDITOR (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, gtkhtml_editor_get_magic_smileys (
				GTKHTML_EDITOR (object)));
			return;
		case PROP_CHANGED:
			g_value_set_boolean (
				value, gtkhtml_editor_get_changed (
				GTKHTML_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_dispose (GObject *object)
{
	GtkhtmlEditor *editor = GTKHTML_EDITOR (object);

	DISPOSE (editor->vbox);

	gtkhtml_editor_private_dispose (editor);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
editor_finalize (GObject *object)
{
	GtkhtmlEditor *editor = GTKHTML_EDITOR (object);

	gtkhtml_editor_private_finalize (editor);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
editor_key_press_event (GtkWidget *widget,
                        GdkEventKey *event)
{
#ifdef HAVE_XFREE
	GtkhtmlEditor *editor = GTKHTML_EDITOR (widget);

	if (event->keyval == XF86XK_Spell) {
		gtk_action_activate (ACTION (SPELL_CHECK));
		return TRUE;
	}
#endif

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static void
editor_cut_clipboard (GtkhtmlEditor *editor)
{
	gtk_html_command (gtkhtml_editor_get_html (editor), "cut");
}

static void
editor_copy_clipboard (GtkhtmlEditor *editor)
{
	gtk_html_command (gtkhtml_editor_get_html (editor), "copy");
}

static void
editor_paste_clipboard (GtkhtmlEditor *editor)
{
	gtk_html_command (gtkhtml_editor_get_html (editor), "paste");
}

static void
editor_select_all (GtkhtmlEditor *editor)
{
	gtk_html_command (gtkhtml_editor_get_html (editor), "select-all");
}

static void
editor_class_init (GtkhtmlEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (GtkhtmlEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = editor_constructor;
	object_class->set_property = editor_set_property;
	object_class->get_property = editor_get_property;
	object_class->dispose = editor_dispose;
	object_class->finalize = editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->key_press_event = editor_key_press_event;

	class->cut_clipboard = editor_cut_clipboard;
	class->copy_clipboard = editor_copy_clipboard;
	class->paste_clipboard = editor_paste_clipboard;
	class->select_all = editor_select_all;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_FOLDER,
		g_param_spec_string (
			"current-folder",
			"Current Folder",
			"The initial folder for file chooser dialogs",
			g_get_home_dir (),
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			"Filename",
			"The filename to use when saving",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_HTML,
		g_param_spec_object (
			"html",
			"HTML Editing Widget",
			"The main HTML editing widget",
			GTK_TYPE_HTML,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_HTML_MODE,
		g_param_spec_boolean (
			"html-mode",
			"HTML Mode",
			"Edit HTML or plain text",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			"Check your spelling as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			"Make URIs clickable as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			_("Changed property"),
			_("Whether editor changed"),
			FALSE,
			G_PARAM_READWRITE));

	signals[COMMAND_AFTER] = g_signal_new (
		"command-after",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, command_after),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[COMMAND_BEFORE] = g_signal_new (
		"command-before",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, command_before),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[IMAGE_URI] = g_signal_new (
		"image-uri",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, image_uri),
		NULL, NULL,
		gtkhtml_editor_marshal_STRING__STRING,
		G_TYPE_STRING, 1,
		G_TYPE_STRING);

	signals[LINK_CLICKED] = g_signal_new (
		"link-clicked",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, link_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[OBJECT_DELETED] = g_signal_new (
		"object-deleted",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, object_deleted),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[URI_REQUESTED] = g_signal_new (
		"uri-requested",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkhtmlEditorClass, uri_requested),
		NULL, NULL,
		gtkhtml_editor_marshal_VOID__STRING_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_POINTER);
}

static void
editor_init (GtkhtmlEditor *editor)
{
	editor->priv = GTKHTML_EDITOR_GET_PRIVATE (editor);
	editor->vbox = g_object_ref_sink (gtk_vbox_new (FALSE, 0));
	gtk_widget_show (editor->vbox);

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	gtkhtml_editor_private_init (editor);
}

GType
gtkhtml_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GtkhtmlEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (GtkhtmlEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) editor_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "GtkhtmlEditor", &type_info, 0);
	}

	return type;
}

GtkWidget *
gtkhtml_editor_new (void)
{
	return g_object_new (GTKHTML_TYPE_EDITOR, NULL);
}

GtkHTML *
gtkhtml_editor_get_html (GtkhtmlEditor *editor)
{
	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	return GTK_HTML (editor->priv->edit_area);
}

GtkBuilder *
gtkhtml_editor_get_builder (GtkhtmlEditor *editor)
{
	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	return editor->priv->builder;
}

GtkUIManager *
gtkhtml_editor_get_ui_manager (GtkhtmlEditor *editor)
{
	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	return editor->priv->manager;
}

GtkAction *
gtkhtml_editor_get_action (GtkhtmlEditor *editor,
                           const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = gtkhtml_editor_get_ui_manager (editor);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

GtkActionGroup *
gtkhtml_editor_get_action_group (GtkhtmlEditor *editor,
                                 const gchar *group_name)
{
	GtkUIManager *manager;
	GList *iter;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = gtkhtml_editor_get_ui_manager (editor);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		iter = g_list_next (iter);
	}

	return NULL;
}

GtkWidget *
gtkhtml_editor_get_widget (GtkhtmlEditor *editor,
                           const gchar *widget_name)
{
	GtkBuilder *builder;
	GObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_name != NULL, NULL);

	builder = gtkhtml_editor_get_builder (editor);
	object = gtk_builder_get_object (builder, widget_name);
	g_return_val_if_fail (GTK_IS_WIDGET (object), NULL);

	return GTK_WIDGET (object);
}

GtkWidget *
gtkhtml_editor_get_managed_widget (GtkhtmlEditor *editor,
                                   const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = gtkhtml_editor_get_ui_manager (editor);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

gboolean
gtkhtml_editor_get_changed (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	/* XXX GtkHTML does not notify us when its internal "saved" state
	 *     changes, so we can't have a "changed" property because its
	 *     notifications would be unreliable. */

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	return editor->priv->changed || !html_engine_is_saved (html->engine);
}

void
gtkhtml_editor_set_changed (GtkhtmlEditor *editor,
                            gboolean changed)
{
	GtkHTML *html;

	/* XXX GtkHTML does not notify us when its internal "saved" state
	 *     changes, so we can't have a "changed" property because its
	 *     notifications would be unreliable. */

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	if (!changed) {
		html = gtkhtml_editor_get_html (editor);

		/* XXX The NULL check was added to deal with a race in
		 *     Evolution when saving a message to a remote Drafts
		 *     folder (which is asynchronous) and then closing the
		 *     editor (which is immediate).  The GtkHTML object
		 *     may already be disposed by the time the Save Draft
		 *     callback gets here.  See bug #553995 for details. */
		if (html)
			html_engine_saved (html->engine);
	}

	editor->priv->changed = changed;

	g_object_notify (G_OBJECT (editor), "changed");
}

const gchar *
gtkhtml_editor_get_current_folder (GtkhtmlEditor *editor)
{
	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	return editor->priv->current_folder;
}

void
gtkhtml_editor_set_current_folder (GtkhtmlEditor *editor,
                                   const gchar *current_folder)
{
	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	if (current_folder == NULL)
		current_folder = g_get_home_dir ();

	g_free (editor->priv->current_folder);
	editor->priv->current_folder = g_strdup (current_folder);

	g_object_notify (G_OBJECT (editor), "current-folder");
}

const gchar *
gtkhtml_editor_get_filename (GtkhtmlEditor *editor)
{
	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	return editor->priv->filename;
}

void
gtkhtml_editor_set_filename (GtkhtmlEditor *editor,
                             const gchar *filename)
{
	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	g_free (editor->priv->filename);
	editor->priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (editor), "filename");
}

gboolean
gtkhtml_editor_get_html_mode (GtkhtmlEditor *editor)
{
	GtkRadioAction *action;
	EditorMode mode;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	action = GTK_RADIO_ACTION (ACTION (MODE_HTML));
	mode = gtk_radio_action_get_current_value (action);

	return (mode == EDITOR_MODE_HTML);
}

void
gtkhtml_editor_set_html_mode (GtkhtmlEditor *editor,
                              gboolean html_mode)
{
	GtkRadioAction *action;
	EditorMode mode;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	action = GTK_RADIO_ACTION (ACTION (MODE_HTML));
	mode = html_mode ? EDITOR_MODE_HTML : EDITOR_MODE_TEXT;
	gtk_radio_action_set_current_value (action, mode);

	/* We emit "notify::html-mode" in the action handler. */
}

gboolean
gtkhtml_editor_get_inline_spelling (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	return gtk_html_get_inline_spelling (html);
}

void
gtkhtml_editor_set_inline_spelling (GtkhtmlEditor *editor,
                                    gboolean inline_spelling)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);
	gtk_html_set_inline_spelling (html, inline_spelling);

	g_object_notify (G_OBJECT (editor), "inline-spelling");
}

gboolean
gtkhtml_editor_get_magic_links (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	return gtk_html_get_magic_links (html);
}

void
gtkhtml_editor_set_magic_links (GtkhtmlEditor *editor,
                                gboolean magic_links)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);
	gtk_html_set_magic_links (html, magic_links);

	g_object_notify (G_OBJECT (editor), "magic-links");
}

gboolean
gtkhtml_editor_get_magic_smileys (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	return gtk_html_get_magic_smileys (html);
}

void
gtkhtml_editor_set_magic_smileys (GtkhtmlEditor *editor,
                                  gboolean magic_smileys)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);
	gtk_html_set_magic_smileys (html, magic_smileys);

	g_object_notify (G_OBJECT (editor), "magic-smileys");
}

GList *
gtkhtml_editor_get_spell_languages (GtkhtmlEditor *editor)
{
	GList *spell_languages = NULL;
	GtkActionGroup *action_group;
	GList *list;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	action_group = editor->priv->language_actions;
	list = gtk_action_group_list_actions (action_group);

	while (list != NULL) {
		GtkToggleAction *action = list->data;
		const GtkhtmlSpellLanguage *language;
		const gchar *language_code;

		list = g_list_delete_link (list, list);
		if (!gtk_toggle_action_get_active (action))
			continue;

		language_code = gtk_action_get_name (GTK_ACTION (action));
		language = gtkhtml_spell_language_lookup (language_code);
		if (language == NULL)
			continue;

		spell_languages = g_list_prepend (
			spell_languages, (gpointer) language);
	}

	return g_list_reverse (spell_languages);
}

void
gtkhtml_editor_set_spell_languages (GtkhtmlEditor *editor,
                                    GList *spell_languages)
{
	GtkActionGroup *action_group;
	GList *list;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	action_group = editor->priv->language_actions;
	list = gtk_action_group_list_actions (action_group);

	while (list != NULL) {
		GtkToggleAction *action = list->data;
		const GtkhtmlSpellLanguage *language;
		const gchar *language_code;
		gboolean active;

		list = g_list_delete_link (list, list);
		language_code = gtk_action_get_name (GTK_ACTION (action));
		language = gtkhtml_spell_language_lookup (language_code);
		active = (g_list_find (spell_languages, language) != NULL);

		gtk_toggle_action_set_active (action, active);
	}
}

gint
gtkhtml_editor_file_chooser_dialog_run (GtkhtmlEditor *editor,
                                        GtkWidget *dialog)
{
	gint response = GTK_RESPONSE_NONE;
	gboolean save_folder;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), response);
	g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), response);

	gtk_file_chooser_set_current_folder_uri (
		GTK_FILE_CHOOSER (dialog),
		gtkhtml_editor_get_current_folder (editor));

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	save_folder =
		(response == GTK_RESPONSE_ACCEPT) ||
		(response == GTK_RESPONSE_OK) ||
		(response == GTK_RESPONSE_YES) ||
		(response == GTK_RESPONSE_APPLY);

	if (save_folder) {
		gchar *folder;

		folder = gtk_file_chooser_get_current_folder_uri (
			GTK_FILE_CHOOSER (dialog));
		gtkhtml_editor_set_current_folder (editor, folder);
		g_free (folder);
	}

	return response;
}

/* Helper for gtkhtml_editor_get_text_[html/plain]() */
static gboolean
editor_save_receiver (HTMLEngine *engine,
                      const gchar *data,
                      guint length,
                      GString *contents)
{
	g_string_append_len (contents, data, length);

	return TRUE;
}

gchar *
gtkhtml_editor_get_text_html (GtkhtmlEditor *editor,
                              gsize *length)
{
	GString *string;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	string = g_string_sized_new (4096);

	gtk_html_export (
		gtkhtml_editor_get_html (editor), "text/html",
		(GtkHTMLSaveReceiverFn) editor_save_receiver, string);

	if (length != NULL)
		*length = string->len;

	return g_string_free (string, FALSE);
}

gchar *
gtkhtml_editor_get_text_plain (GtkhtmlEditor *editor,
                               gsize *length)
{
	GString *string;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);

	string = g_string_sized_new (4096);

	gtk_html_export (
		gtkhtml_editor_get_html (editor), "text/plain",
		(GtkHTMLSaveReceiverFn) editor_save_receiver, string);

	if (length != NULL)
		*length = string->len;

	return g_string_free (string, FALSE);
}

void
gtkhtml_editor_set_text_html (GtkhtmlEditor *editor,
                              const gchar *text,
                              gssize length)
{
	GtkHTML *html;
	GtkHTMLStream *stream;
	gboolean editable;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));
	g_return_if_fail (text != NULL);

	/* XXX gtk_html_write() should really do this. */
	if (length < 0)
		length = strlen (text);

	html = gtkhtml_editor_get_html (editor);
	editable = gtk_html_get_editable (html);
	gtk_html_set_editable (html, FALSE);

	stream = gtk_html_begin_content (html, "text/html; charset=utf-8");

	/* XXX gtk_html_write() requires length > 0, which is
	 *     unnecessary.  Zero length should be a no-op. */
	if (length > 0)
		gtk_html_write (html, stream, text, length);

	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);

	gtk_html_set_editable (html, editable);
}

gboolean
gtkhtml_editor_save (GtkhtmlEditor *editor,
                     const gchar *filename,
                     gboolean as_html,
                     GError **error)
{
	gchar *contents;
	gsize length;
	gboolean success;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (as_html)
		contents = gtkhtml_editor_get_text_html (editor, &length);
	else
		contents = gtkhtml_editor_get_text_plain (editor, &length);

	success = g_file_set_contents (filename, contents, length, error);

	g_free (contents);

	if (success) {
		GtkHTML *html;

		html = gtkhtml_editor_get_html (editor);
		html->engine->saved_step_count =
			html_undo_get_step_count (html->engine->undo);
		gtkhtml_editor_run_command (editor, "saved");
	}

	return success;
}

const gchar *
gtkhtml_editor_get_paragraph_data (GtkhtmlEditor *editor,
                                   const gchar *key)
{
	GtkHTML *html;
	HTMLObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	html = gtkhtml_editor_get_html (editor);

	object = html->engine->cursor->object;
	if (object == NULL)
		return NULL;

	object = object->parent;
	if (object == NULL)
		return NULL;

	if (!HTML_IS_CLUEFLOW (object))
		return NULL;

	return html_object_get_data (object, key);
}

gboolean
gtkhtml_editor_set_paragraph_data (GtkhtmlEditor *editor,
                                   const gchar *key,
                                   const gchar *value)
{
	GtkHTML *html;
	HTMLObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	html = gtkhtml_editor_get_html (editor);

	object = html->engine->cursor->object;
	if (object == NULL)
		return FALSE;

	object = object->parent;
	if (object == NULL)
		return FALSE;

	if (HTML_OBJECT_TYPE (object) != HTML_TYPE_CLUEFLOW)
		return FALSE;

	html_object_set_data (object, key, value);

	return TRUE;
}

gboolean
gtkhtml_editor_run_command (GtkhtmlEditor *editor,
                            const gchar *command)
{
	GtkHTML *html;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (command != NULL, FALSE);

	html = gtkhtml_editor_get_html (editor);

	return gtk_html_command (html, command);
}

gboolean
gtkhtml_editor_is_paragraph_empty (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	HTMLObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	object = html->engine->cursor->object;
	if (object == NULL)
		return FALSE;

	object = object->parent;
	if (object == NULL)
		return FALSE;

	if (HTML_OBJECT_TYPE (object) != HTML_TYPE_CLUEFLOW)
		return FALSE;

	return html_clueflow_is_empty (HTML_CLUEFLOW (object));
}

gboolean
gtkhtml_editor_is_previous_paragraph_empty (GtkhtmlEditor *editor)
{
	GtkHTML *html;
	HTMLObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	object = html->engine->cursor->object;
	if (object == NULL)
		return FALSE;

	object = object->parent;
	if (object == NULL)
		return FALSE;

	object = object->prev;
	if (object == NULL)
		return FALSE;

	if (HTML_OBJECT_TYPE (object) != HTML_TYPE_CLUEFLOW)
		return FALSE;

	return html_clueflow_is_empty (HTML_CLUEFLOW (object));
}

void
gtkhtml_editor_insert_html (GtkhtmlEditor *editor,
                            const gchar *html_text)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	gtk_html_insert_html (html, html_text);
}

void
gtkhtml_editor_insert_image (GtkhtmlEditor *editor,
                             const gchar *image_uri)
{
	GtkHTML *html;
	HTMLObject *image;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));
	g_return_if_fail (image_uri != NULL);

	html = gtkhtml_editor_get_html (editor);

	image = html_image_new (
		html_engine_get_image_factory (html->engine), image_uri,
		NULL, NULL, 0, 0, 0, 0, 0, NULL, HTML_VALIGN_NONE, FALSE);

	html_engine_paste_object (html->engine, image, 1);
}

void
gtkhtml_editor_insert_text (GtkhtmlEditor *editor,
                            const gchar *plain_text)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	html_engine_paste_text (html->engine, plain_text, -1);
}

gboolean
gtkhtml_editor_search_by_data (GtkhtmlEditor *editor,
                               glong level,
                               const gchar *klass,
                               const gchar *key,
                               const gchar *value)
{
	GtkHTML *html;
	HTMLObject *last_object = NULL;
	HTMLObject *object;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	html = gtkhtml_editor_get_html (editor);

	do {
		if (html->engine->cursor->object != last_object) {
			object = html_object_nth_parent (
				html->engine->cursor->object, level);
			if (object != NULL) {
				const gchar *object_value;

				object_value =
					html_object_get_data (object, key);
				if (object_value != NULL &&
					strcmp (object_value, value) == 0)
					return TRUE;
			}
		}
		last_object = html->engine->cursor->object;
	} while (html_cursor_forward (html->engine->cursor, html->engine));

	return FALSE;
}

void
gtkhtml_editor_freeze (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	html_engine_freeze (html->engine);
}

void
gtkhtml_editor_thaw (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	html_engine_thaw (html->engine);
}

void
gtkhtml_editor_undo_begin (GtkhtmlEditor *editor,
                           const gchar *undo_name,
                           const gchar *redo_name)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));
	g_return_if_fail (undo_name != NULL);
	g_return_if_fail (redo_name != NULL);

	html = gtkhtml_editor_get_html (editor);

	html_undo_level_begin (html->engine->undo, undo_name, redo_name);
}

void
gtkhtml_editor_undo_end (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	html_undo_level_end (html->engine->undo, html->engine);
}

gboolean
gtkhtml_editor_has_undo (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_val_if_fail (GTKHTML_IS_EDITOR (editor), FALSE);

	html = gtkhtml_editor_get_html (editor);

	return gtk_html_has_undo (html);
}

void
gtkhtml_editor_drop_undo (GtkhtmlEditor *editor)
{
	GtkHTML *html;

	g_return_if_fail (GTKHTML_IS_EDITOR (editor));

	html = gtkhtml_editor_get_html (editor);

	gtk_html_drop_undo (html);
}
