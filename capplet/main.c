/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library

   Copyright (C) 2000 Helix Code, Inc.
   Authors:           Radek Doulik (rodo@helixcode.com)

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHcANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>
#include <gnome.h>
#include <capplet-widget.h>
#include <gtkhtml-properties.h>
#include <glade/glade.h>
#include "gnome-bindings-prop.h"
#include "../src/gtkhtml.h"

#define CUSTOM_KEYMAP_NAME "Custom"
#define EMACS_KEYMAP_NAME "Emacs like"
#define MS_KEYMAP_NAME "MS like"

static GtkWidget *capplet, *variable, *variable_print, *fixed, *fixed_print, *anim_check;
static GtkWidget *bi, *live_spell_check, *language, *live_spell_color, *live_spell_frame, *magic_check;

static gboolean active = FALSE;
#ifdef GTKHTML_HAVE_GCONF
static GError      *error  = NULL;
static GConfClient *client = NULL;
#endif
static GtkHTMLClassProperties *saved_prop;
static GtkHTMLClassProperties *orig_prop;
static GtkHTMLClassProperties *actual_prop;

static GList *saved_bindings;
static GList *orig_bindings;

static gchar *home_rcfile;

static void
set_ui ()
{
	gchar *font_name, *keymap_name;

	active = FALSE;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (anim_check), actual_prop->animations);

#define SET_FONT(f,s,w) \
	font_name = g_strdup_printf ("-*-%s-*-*-normal-*-%d-*-*-*-*-*-*-*", \
						 actual_prop-> ## f, actual_prop-> ## s); \
	gnome_font_picker_set_font_name (GNOME_FONT_PICKER (w), font_name); \
	g_free (font_name)

	SET_FONT (font_var_family,       font_var_size,       variable);
	SET_FONT (font_fix_family,       font_fix_size,       fixed);
	SET_FONT (font_var_family_print, font_var_size_print, variable_print);
	SET_FONT (font_fix_family_print, font_fix_size_print, fixed_print);

	/* set to current state */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (magic_check), actual_prop->magic_links);

	if (!strcmp (actual_prop->keybindings_theme, "emacs")) {
		keymap_name = EMACS_KEYMAP_NAME;
	} else if (!strcmp (actual_prop->keybindings_theme, "ms")) {
		keymap_name = MS_KEYMAP_NAME;
	} else
		keymap_name = CUSTOM_KEYMAP_NAME;
	gnome_bindings_properties_select_keymap (GNOME_BINDINGS_PROPERTIES (bi), keymap_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (live_spell_check), actual_prop->live_spell_check);
	gtk_widget_set_sensitive (live_spell_color, actual_prop->live_spell_check);
	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (live_spell_color),
							actual_prop->spell_error_color.red,
							actual_prop->spell_error_color.green,
							actual_prop->spell_error_color.blue, 0);
	gtk_entry_set_text (GTK_ENTRY (language), actual_prop->language);	

	active = TRUE;
}

static gchar *
get_attr (gchar *font_name, gint n)
{
    gchar *s, *end;

    /* Search paramether */
    for (s=font_name; n; n--,s++)
	    s = strchr (s,'-');

    if (s && *s != 0) {
	    end = strchr (s, '-');
	    if (end)
		    return g_strndup (s, end - s);
	    else
		    return g_strdup (s);
    } else
	    return g_strdup ("Unknown");
}

static void
apply_fonts ()
{
	gchar *size_str;

	actual_prop->animations = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (anim_check));

#define APPLY(f,s,w) \
	g_free (actual_prop-> ## f); \
	actual_prop-> ## f = get_attr (gnome_font_picker_get_font_name (GNOME_FONT_PICKER (w)), 2); \
	size_str = get_attr (gnome_font_picker_get_font_name (GNOME_FONT_PICKER (w)), 7); \
	actual_prop-> ## s = atoi (size_str); \
	g_free (size_str)

	APPLY (font_var_family,       font_var_size,       variable);
	APPLY (font_fix_family,       font_fix_size,       fixed);
	APPLY (font_var_family_print, font_var_size_print, variable_print);
	APPLY (font_fix_family_print, font_fix_size_print, fixed_print);
}

static void
apply_editable (void)
{
	gchar *keymap_id, *keymap_name;

	/* bindings */
	gnome_bindings_properties_save_keymap (GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME, home_rcfile);
	gnome_binding_entry_list_destroy (saved_bindings);
	saved_bindings = gnome_binding_entry_list_copy (gnome_bindings_properties_get_keymap (GNOME_BINDINGS_PROPERTIES (bi),
											      CUSTOM_KEYMAP_NAME));

	/* properties */
	actual_prop->magic_links = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (magic_check));
	keymap_name = gnome_bindings_properties_get_keymap_name (GNOME_BINDINGS_PROPERTIES (bi));
	if (!strcmp (keymap_name, EMACS_KEYMAP_NAME)) {
		keymap_id = "emacs";
	} else if (!strcmp (keymap_name, MS_KEYMAP_NAME)) {
		keymap_id = "ms";
	} else
		keymap_id = "custom";

	g_free (actual_prop->keybindings_theme);
	actual_prop->keybindings_theme = g_strdup (keymap_id);

	actual_prop->live_spell_check = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (live_spell_check));
	gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (live_spell_color),
				    &actual_prop->spell_error_color.red,
				    &actual_prop->spell_error_color.green,
				    &actual_prop->spell_error_color.blue, NULL);
	g_free (actual_prop->language);
	actual_prop->language = g_strdup (gtk_entry_get_text (GTK_ENTRY (language)));
}

static void
apply (void)
{
	apply_fonts ();
	apply_editable ();
#ifdef GTKHTML_HAVE_GCONF
	gtk_html_class_properties_update (actual_prop, client, saved_prop);
#else
	gtk_html_class_properties_save (actual_prop);
#endif
	gtk_html_class_properties_copy   (saved_prop, actual_prop);

}

static void
revert (void)
{
	gnome_bindings_properties_set_keymap (GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME, orig_bindings);
	gnome_binding_entry_list_destroy (saved_bindings);
	saved_bindings = gnome_binding_entry_list_copy (orig_bindings);
	gnome_bindings_properties_save_keymap (GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME, home_rcfile);

#ifdef GTKHTML_HAVE_GCONF
	gtk_html_class_properties_update (orig_prop, client, saved_prop);
#else
	gtk_html_class_properties_save (orig_prop);
#endif
	gtk_html_class_properties_copy   (saved_prop, orig_prop);
	gtk_html_class_properties_copy   (actual_prop, orig_prop);

	set_ui ();
}

static void
changed (GtkWidget *widget, gpointer null)
{
	if (active)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet), TRUE);
}

static void
live_changed (GtkWidget *widget, gpointer null)
{
	gtk_widget_set_sensitive (live_spell_frame,
				  GTK_TOGGLE_BUTTON (live_spell_check)->active);

	changed (widget, null);
}
static void
setup (void)
{
	GtkWidget *vbox, *ebox;
	GladeXML *xml;
	char *base, *rcfile;

	glade_gnome_init ();
	xml = glade_xml_new (GLADE_DATADIR "/gtkhtml-capplet.glade", "prefs_widget");

	if (!xml)
		g_error (_("Could not load glade file."));

	glade_xml_signal_connect (xml, "changed", GTK_SIGNAL_FUNC (changed));
	glade_xml_signal_connect (xml, "live_changed", GTK_SIGNAL_FUNC (live_changed));

        capplet = capplet_widget_new();
	vbox    = glade_xml_get_widget (xml, "prefs_widget");

	variable         = glade_xml_get_widget (xml, "screen_variable");
	variable_print   = glade_xml_get_widget (xml, "print_variable");
	fixed            = glade_xml_get_widget (xml, "screen_fixed");
	fixed_print      = glade_xml_get_widget (xml, "print_fixed");

	anim_check       = glade_xml_get_widget (xml, "anim_check");
	magic_check      = glade_xml_get_widget (xml, "magic_check");
	live_spell_check = glade_xml_get_widget (xml, "live_spell_check");
	live_spell_color = glade_xml_get_widget (xml, "live_spell_color");
	language         = glade_xml_get_widget (xml, "live_spell_entry");
	live_spell_frame = glade_xml_get_widget (xml, "live_spell_frame");

#define LOAD(x) \
	base = g_strconcat ("gtkhtml/keybindingsrc.", x, NULL); \
	rcfile = gnome_unconditional_datadir_file (base); \
        gtk_rc_parse (rcfile); \
        g_free (base); \
	g_free (rcfile)
	
	home_rcfile = g_strconcat (gnome_util_user_home (), "/.gnome/gtkhtml-bindings-custom", NULL);
	gtk_rc_parse (home_rcfile);
	LOAD ("emacs");
	LOAD ("ms");

	bi = gnome_bindings_properties_new ();
	gnome_bindings_properties_add_keymap (GNOME_BINDINGS_PROPERTIES (bi),
					      EMACS_KEYMAP_NAME, "gtkhtml-bindings-emacs", "command",
					      GTK_TYPE_HTML_COMMAND, FALSE);
	gnome_bindings_properties_add_keymap (GNOME_BINDINGS_PROPERTIES (bi),
					      MS_KEYMAP_NAME, "gtkhtml-bindings-ms", "command",
					      GTK_TYPE_HTML_COMMAND, FALSE);
	gnome_bindings_properties_add_keymap (GNOME_BINDINGS_PROPERTIES (bi),
					      CUSTOM_KEYMAP_NAME, "gtkhtml-bindings-custom", "command",
					      GTK_TYPE_HTML_COMMAND, TRUE);
	gnome_bindings_properties_select_keymap (GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME);
	orig_bindings = gnome_binding_entry_list_copy (gnome_bindings_properties_get_keymap
						       (GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME));
	saved_bindings = gnome_binding_entry_list_copy (gnome_bindings_properties_get_keymap
							(GNOME_BINDINGS_PROPERTIES (bi), CUSTOM_KEYMAP_NAME));
	gtk_signal_connect (GTK_OBJECT (bi), "changed", changed, NULL);

	ebox = glade_xml_get_widget (xml, "bindings_ebox");
	gtk_container_add (GTK_CONTAINER (ebox), bi);

        gtk_container_add (GTK_CONTAINER (capplet), vbox);
        gtk_widget_show_all (capplet);

	set_ui ();
}

int
main (int argc, char **argv)
{
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        if (gnome_capplet_init ("gtkhtml-properties", VERSION, argc, argv, NULL, 0, NULL) < 0)
		return 1;

#ifdef GTKHTML_HAVE_GCONF
	if (!gconf_init(argc, argv, &error)) {
		g_assert(error != NULL);
		g_warning("GConf init failed:\n  %s", error->message);
		return 1;
	}

	client = gconf_client_get_default ();
	gconf_client_add_dir(client, GTK_HTML_GCONF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
#endif
	orig_prop = gtk_html_class_properties_new ();
	saved_prop = gtk_html_class_properties_new ();
	actual_prop = gtk_html_class_properties_new ();
#ifdef GTKHTML_HAVE_GCONF
	gtk_html_class_properties_load (actual_prop, client);
#else
	gtk_html_class_properties_load (actual_prop);
#endif
	gtk_html_class_properties_copy (saved_prop, actual_prop);
	gtk_html_class_properties_copy (orig_prop, actual_prop);

        setup ();

	/* connect signals */
        gtk_signal_connect (GTK_OBJECT (capplet), "try",
                            GTK_SIGNAL_FUNC (apply), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "revert",
                            GTK_SIGNAL_FUNC (revert), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "ok",
                            GTK_SIGNAL_FUNC (apply), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "cancel",
                            GTK_SIGNAL_FUNC (revert), NULL);

        capplet_gtk_main ();

	g_free (home_rcfile);

        return 0;
}
