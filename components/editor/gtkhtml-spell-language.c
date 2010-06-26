/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtkhtml-editor-spell-language.c
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

#include "gtkhtml-spell-language.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <enchant.h>

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

struct _GtkhtmlSpellLanguage {
	gchar *code;
	gchar *name;
	gchar *ckey;
};

static GHashTable *iso_639_table = NULL;
static GHashTable *iso_3166_table = NULL;

#ifdef HAVE_ISO_CODES

#define ISOCODESLOCALEDIR ISO_CODES_PREFIX "/share/locale"

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <shlobj.h>
static HMODULE hmodule;

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
	switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
		hmodule = hinstDLL;
		break;
    }

	return TRUE;
}

static gchar *
_get_iso_codes_prefix (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;
	gchar *temp_dir = 0;

	if (beenhere)
		return retval;

	if (!(temp_dir = g_win32_get_package_installation_directory_of_module ((gpointer)hmodule))) {
		strcpy(retval, ISO_CODES_PREFIX);
		return retval;
	}

	strcpy (retval, temp_dir);
	g_free (temp_dir);
	beenhere = 1;
	return retval;
}

static gchar *
_get_isocodeslocaledir (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;

	if (beenhere)
		return retval;

	strcpy (retval, _get_iso_codes_prefix ());
	strcat (retval, "\\share\\locale" );
	beenhere = 1;
	return retval;
}

#undef ISO_CODES_PREFIX
#define ISO_CODES_PREFIX _get_iso_codes_prefix ()

#undef ISOCODESLOCALEDIR
#define ISOCODESLOCALEDIR _get_isocodeslocaledir ()

#endif

static void
iso_639_start_element (GMarkupParseContext *context,
                       const gchar *element_name,
                       const gchar **attribute_names,
                       const gchar **attribute_values,
                       gpointer data,
                       GError **error)
{
	GHashTable *hash_table = data;
	const gchar *iso_639_1_code = NULL;
	const gchar *iso_639_2_code = NULL;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

	if (strcmp (element_name, "iso_639_entry") != 0)
		return;

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_1_code") == 0)
			iso_639_1_code = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_2T_code") == 0)
			iso_639_2_code = attribute_values[ii];
	}

	code = (iso_639_1_code != NULL) ? iso_639_1_code : iso_639_2_code;

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_strdup (code),
			g_strdup (dgettext (ISO_639_DOMAIN, name)));
}

static void
iso_3166_start_element (GMarkupParseContext *context,
                        const gchar *element_name,
                        const gchar **attribute_names,
                        const gchar **attribute_values,
                        gpointer data,
                        GError **error)
{
	GHashTable *hash_table = data;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

	if (strcmp (element_name, "iso_3166_entry") != 0)
		return;

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "alpha_2_code") == 0)
			code = attribute_values[ii];
	}

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_ascii_strdown (code, -1),
			g_strdup (dgettext (ISO_3166_DOMAIN, name)));
}

static GMarkupParser iso_639_parser = {
	iso_639_start_element,
	NULL, NULL, NULL, NULL
};

static GMarkupParser iso_3166_parser = {
	iso_3166_start_element,
	NULL, NULL, NULL, NULL
};

static void
iso_codes_parse (const GMarkupParser *parser,
                 const gchar *basename,
		 GHashTable *hash_table)
{
	GMappedFile *mapped_file;
	gchar *filename;
	GError *error = NULL;

	filename = g_build_filename (
		ISO_CODES_PREFIX, "share", "xml",
		"iso-codes", basename, NULL);
	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	g_free (filename);

	if (mapped_file != NULL) {
		GMarkupParseContext *context;
		const gchar *contents;
		gsize length;

		context = g_markup_parse_context_new (
			parser, 0, hash_table, NULL);
		contents = g_mapped_file_get_contents (mapped_file);
		length = g_mapped_file_get_length (mapped_file);
		g_markup_parse_context_parse (
			context, contents, length, &error);
		g_markup_parse_context_free (context);
#if GLIB_CHECK_VERSION(2,21,3)
		g_mapped_file_unref (mapped_file);
#else
		g_mapped_file_free (mapped_file);
#endif
	}

	if (error != NULL) {
		g_warning ("%s: %s", basename, error->message);
		g_error_free (error);
	}
}

#endif /* HAVE_ISO_CODES */

static void
spell_language_dict_describe_cb (const gchar * const language_code,
                                 const gchar * const provider_name,
                                 const gchar * const provider_desc,
                                 const gchar * const provider_file,
                                 GTree *tree)
{
	const gchar *iso_639_name;
	const gchar *iso_3166_name;
	gchar *language_name;
	gchar *lowercase;
	gchar **tokens;

	/* Split language code into lowercase tokens. */
	lowercase = g_ascii_strdown (language_code, -1);
	tokens = g_strsplit (lowercase, "_", -1);
	g_free (lowercase);

	g_return_if_fail (tokens != NULL);

	iso_639_name = g_hash_table_lookup (iso_639_table, tokens[0]);

	if (iso_639_name == NULL) {
		language_name = g_strdup_printf (
		/* Translators: %s is the language ISO code. */
			C_("language", "Unknown (%s)"), language_code);
		goto exit;
	}

	if (g_strv_length (tokens) < 2) {
		language_name = g_strdup (iso_639_name);
		goto exit;
	}

	iso_3166_name = g_hash_table_lookup (iso_3166_table, tokens[1]);

	if (iso_3166_name != NULL)
		language_name = g_strdup_printf (
		 /* Translators: The first %s is the language name, and the
		 * second is the country name. Example: "French (France)" */
			C_("language", "%s (%s)"), iso_639_name, iso_3166_name);
	else
		language_name = g_strdup_printf (
		 /* Translators: The first %s is the language name, and the
		 * second is the country name. Example: "French (France)" */
			C_("language", "%s (%s)"), iso_639_name, tokens[1]);

exit:
	g_strfreev (tokens);

	g_tree_replace (tree, g_strdup (language_code), language_name);
}

static const GtkhtmlSpellLanguage *
spell_language_copy (const GtkhtmlSpellLanguage *language)
{
	return language;
}

static void
spell_language_free (const GtkhtmlSpellLanguage *language)
{
	/* do nothing */
}

static const GtkhtmlSpellLanguage *
spell_language_lookup (const gchar *language_code)
{
	const GtkhtmlSpellLanguage *closest_match = NULL;
	const GList *available_languages;

	available_languages = gtkhtml_spell_language_get_available ();

	while (available_languages != NULL && language_code != NULL) {
		GtkhtmlSpellLanguage *language = available_languages->data;
		const gchar *code = language->code;
		gsize length = strlen (code);

		if (g_ascii_strcasecmp (language_code, code) == 0)
			return language;

		if (g_ascii_strncasecmp (language_code, code, length) == 0)
			closest_match = language;

		available_languages = g_list_next (available_languages);
	}

	return closest_match;
}

static gboolean
spell_language_traverse_cb (const gchar *code,
                            const gchar *name,
                            GList **available_languages)
{
	GtkhtmlSpellLanguage *language;

	language = g_slice_new (GtkhtmlSpellLanguage);
	language->code = g_strdup (code);
	language->name = g_strdup (name);
	language->ckey = g_utf8_collate_key (name, -1);

	*available_languages = g_list_insert_sorted (
		*available_languages, language,
		(GCompareFunc) gtkhtml_spell_language_compare);

	return FALSE;
}

GType
gtkhtml_spell_language_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_boxed_type_register_static (
			"GtkhtmlSpellLanguage",
			(GBoxedCopyFunc) spell_language_copy,
			(GBoxedFreeFunc) spell_language_free);

	return type;
}

const GList *
gtkhtml_spell_language_get_available (void)
{
	static gboolean initialized = FALSE;
	static GList *available_languages = NULL;
	EnchantBroker *broker;
	GTree *tree;

	if (initialized)
		return available_languages;

	initialized = TRUE;

#if defined (ENABLE_NLS) && defined (HAVE_ISO_CODES)
	bindtextdomain (ISO_639_DOMAIN, ISOCODESLOCALEDIR);
	bind_textdomain_codeset (ISO_639_DOMAIN, "UTF-8");

	bindtextdomain (ISO_3166_DOMAIN, ISOCODESLOCALEDIR);
	bind_textdomain_codeset (ISO_3166_DOMAIN, "UTF-8");
#endif

	iso_639_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	iso_3166_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

#ifdef HAVE_ISO_CODES
	iso_codes_parse (&iso_639_parser, "iso_639.xml", iso_639_table);
	iso_codes_parse (&iso_3166_parser, "iso_3166.xml", iso_3166_table);
#endif

	tree = g_tree_new_full (
		(GCompareDataFunc) strcmp, NULL,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	broker = enchant_broker_init ();
	enchant_broker_list_dicts (
		broker, (EnchantDictDescribeFn)
		spell_language_dict_describe_cb, tree);
	enchant_broker_free (broker);

	g_tree_foreach (
		tree, (GTraverseFunc)
		spell_language_traverse_cb,
		&available_languages);

	g_tree_destroy (tree);

	return available_languages;
}

static const GtkhtmlSpellLanguage *
spell_language_pick_default (void)
{
	const GtkhtmlSpellLanguage *language = NULL;
	const gchar * const *language_names;
	const GList *available_languages;
	gint ii;

	language_names = g_get_language_names ();
	available_languages = gtkhtml_spell_language_get_available ();

	for (ii = 0; language == NULL && language_names[ii] != NULL; ii++)
		language = spell_language_lookup (language_names[ii]);

	if (language == NULL)
		language = spell_language_lookup ("en_US");

	if (language == NULL && available_languages != NULL)
		language = available_languages->data;

	return language;
}

const GtkhtmlSpellLanguage *
gtkhtml_spell_language_lookup (const gchar *language_code)
{
	const GtkhtmlSpellLanguage *language = NULL;

	language = spell_language_lookup (language_code);

	if (language == NULL)
		language = spell_language_pick_default ();

	return language;
}

const gchar *
gtkhtml_spell_language_get_code (const GtkhtmlSpellLanguage *language)
{
	g_return_val_if_fail (language != NULL, NULL);

	return language->code;
}

const gchar *
gtkhtml_spell_language_get_name (const GtkhtmlSpellLanguage *language)
{
	if (language == NULL)
		 /* Translators: This refers to the default language used
		 * by the spell checker. */
		return C_("language", "Default");

	return language->name;
}

gint
gtkhtml_spell_language_compare (const GtkhtmlSpellLanguage *language_a,
                                const GtkhtmlSpellLanguage *language_b)
{
	return strcmp (language_a->ckey, language_b->ckey);
}
