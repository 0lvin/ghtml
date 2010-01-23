/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-editor.h
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

#ifndef GTKHTML_EDITOR_H
#define GTKHTML_EDITOR_H

#include <gtkhtml/gtkhtml.h>

#include <gtkhtml-editor-common.h>
#include <gtkhtml-editor-actions.h>
#include <gtkhtml-editor-widgets.h>

#include <gtkhtml-spell-checker.h>
#include <gtkhtml-spell-language.h>

#include <stdarg.h>

/* Standard GObject macros */
#define GTKHTML_TYPE_EDITOR \
	(gtkhtml_editor_get_type ())
#define GTKHTML_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GTKHTML_TYPE_EDITOR, GtkhtmlEditor))
#define GTKHTML_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GTKHTML_TYPE_EDITOR, GtkhtmlEditorClass))
#define GTKHTML_IS_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GTKHTML_TYPE_EDITOR))
#define GTKHTML_IS_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GTKHTML_TYPE_EDITOR))
#define GTKHTML_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GTKHTML_TYPE_EDITOR, GtkhtmlEditorClass))

G_BEGIN_DECLS

typedef struct _GtkhtmlEditor GtkhtmlEditor;
typedef struct _GtkhtmlEditorClass GtkhtmlEditorClass;
typedef struct _GtkhtmlEditorPrivate GtkhtmlEditorPrivate;

struct _GtkhtmlEditor {
	GtkWindow parent;
	GtkWidget *vbox;
	GtkhtmlEditorPrivate *priv;
};

struct _GtkhtmlEditorClass {
	GtkWindowClass parent_class;

	void		(*cut_clipboard)	(GtkhtmlEditor *editor);
	void		(*copy_clipboard)	(GtkhtmlEditor *editor);
	void		(*paste_clipboard)	(GtkhtmlEditor *editor);
	void		(*select_all)		(GtkhtmlEditor *editor);

	/* GtkHTML events */
	void		(*command_before)	(GtkhtmlEditor *editor,
						 const gchar *command);
	void		(*command_after)	(GtkhtmlEditor *editor,
						 const gchar *command);
	gchar *		(*image_uri)		(GtkhtmlEditor *editor,
						 const gchar *uri);
	void		(*link_clicked)		(GtkhtmlEditor *editor,
						 const gchar *uri);
	void		(*object_deleted)	(GtkhtmlEditor *editor);
	void		(*uri_requested)	(GtkhtmlEditor *editor,
						 const gchar *uri,
						 GtkHTMLStream *stream);
};

GType		gtkhtml_editor_get_type		(void);
GtkWidget *	gtkhtml_editor_new		(void);
GtkHTML *	gtkhtml_editor_get_html		(GtkhtmlEditor *editor);
GtkBuilder *	gtkhtml_editor_get_builder	(GtkhtmlEditor *editor);
GtkUIManager *	gtkhtml_editor_get_ui_manager	(GtkhtmlEditor *editor);
GtkAction *	gtkhtml_editor_get_action	(GtkhtmlEditor *editor,
						 const gchar *action_name);
GtkActionGroup *gtkhtml_editor_get_action_group	(GtkhtmlEditor *editor,
						 const gchar *group_name);
GtkWidget *	gtkhtml_editor_get_widget	(GtkhtmlEditor *editor,
						 const gchar *widget_name);
GtkWidget *	gtkhtml_editor_get_managed_widget
						(GtkhtmlEditor *editor,
						 const gchar *widget_path);
gboolean	gtkhtml_editor_get_changed	(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_changed	(GtkhtmlEditor *editor,
						 gboolean changed);
const gchar *	gtkhtml_editor_get_current_folder
						(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_current_folder
						(GtkhtmlEditor *editor,
						 const gchar *current_folder);
const gchar *	gtkhtml_editor_get_filename	(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_filename	(GtkhtmlEditor *editor,
						 const gchar *filename);
gboolean	gtkhtml_editor_get_html_mode	(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_html_mode	(GtkhtmlEditor *editor,
						 gboolean html_mode);
gboolean	gtkhtml_editor_get_inline_spelling
						(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_inline_spelling
						(GtkhtmlEditor *editor,
						 gboolean inline_spelling);
gboolean	gtkhtml_editor_get_magic_links	(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_magic_links	(GtkhtmlEditor *editor,
						 gboolean magic_links);
gboolean	gtkhtml_editor_get_magic_smileys(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_magic_smileys(GtkhtmlEditor *editor,
						 gboolean magic_smileys);
GList *		gtkhtml_editor_get_spell_languages
						(GtkhtmlEditor *editor);
void		gtkhtml_editor_set_spell_languages
						(GtkhtmlEditor *editor,
						 GList *spell_languages);
gint		gtkhtml_editor_file_chooser_dialog_run
						(GtkhtmlEditor *editor,
						 GtkWidget *dialog);

/*****************************************************************************
 * High-Level Editing Interface
 *****************************************************************************/

gchar *		gtkhtml_editor_get_text_html	(GtkhtmlEditor *editor,
						 gsize *length);
gchar *		gtkhtml_editor_get_text_plain	(GtkhtmlEditor *editor,
						 gsize *length);
void		gtkhtml_editor_set_text_html	(GtkhtmlEditor *editor,
						 const gchar *text,
						 gssize length);
gboolean	gtkhtml_editor_save		(GtkhtmlEditor *editor,
						 const gchar *filename,
						 gboolean as_html,
						 GError **error);
const gchar *	gtkhtml_editor_get_paragraph_data
						(GtkhtmlEditor *editor,
						 const gchar *key);
gboolean	gtkhtml_editor_set_paragraph_data
						(GtkhtmlEditor *editor,
						 const gchar *key,
						 const gchar *value);
gboolean	gtkhtml_editor_run_command	(GtkhtmlEditor *editor,
						 const gchar *command);
gboolean	gtkhtml_editor_is_paragraph_empty
						(GtkhtmlEditor *editor);
gboolean	gtkhtml_editor_is_previous_paragraph_empty
						(GtkhtmlEditor *editor);
void		gtkhtml_editor_insert_html	(GtkhtmlEditor *editor,
						 const gchar *html_text);
void		gtkhtml_editor_insert_image	(GtkhtmlEditor *editor,
						 const gchar *image_uri);
void		gtkhtml_editor_insert_text	(GtkhtmlEditor *editor,
						 const gchar *plain_text);
gboolean	gtkhtml_editor_search_by_data	(GtkhtmlEditor *editor,
						 glong level,
						 const gchar *klass,
						 const gchar *key,
						 const gchar *value);
void		gtkhtml_editor_freeze		(GtkhtmlEditor *editor);
void		gtkhtml_editor_thaw		(GtkhtmlEditor *editor);
void		gtkhtml_editor_undo_begin	(GtkhtmlEditor *editor,
						 const gchar *undo_name,
						 const gchar *redo_name);
void		gtkhtml_editor_undo_end		(GtkhtmlEditor *editor);
gboolean	gtkhtml_editor_has_undo		(GtkhtmlEditor *editor);
void		gtkhtml_editor_drop_undo	(GtkhtmlEditor *editor);

G_END_DECLS

#endif /* GTKHTML_EDITOR_H */
