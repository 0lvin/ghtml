/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef G_OS_WIN32
/* Clashes with objidl.h, which gets included through a chain of includes from libsoup/soup.h */
#undef DATADIR
#endif

#include <libsoup/soup.h>

#include "config.h"
#include "gtkhtml.h"
#include "htmlurl.h"
#include "htmlengine.h"
#include "gtkhtml-embedded.h"
#include "gtkhtml-properties.h"
#include "gtkhtml-private.h"

#include "gtkhtmldebug.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct _Example Example;

struct _Example {
	const gchar *filename;
	const gchar *title;
};
static GPtrArray *examples;

typedef struct {
  FILE *fil;
  GtkHTMLStream *handle;
} FileInProgress;

typedef struct {
	gchar *url;
	gchar *title;
	GtkWidget *widget;
} go_item;

#define MAX_GO_ENTRIES 20

static void exit_cb (GtkWidget *widget, gpointer data);
static void print_preview_cb (GtkWidget *widget, gpointer data);
static void bug_cb (GtkWidget *widget, gpointer data);
static void animate_cb (GtkToggleButton *togglebutton, gpointer data);
static void stop_cb (GtkWidget *widget, gpointer data);
static void dump_cb (GtkWidget *widget, gpointer data);
static void dump_simple_cb (GtkWidget *widget, gpointer data);
static void forward_cb (GtkWidget *widget, gpointer data);
static void back_cb (GtkWidget *widget, gpointer data);
static void home_cb (GtkWidget *widget, gpointer data);
static void reload_cb (GtkWidget *widget, gpointer data);
static void redraw_cb (GtkWidget *widget, gpointer data);
static void resize_cb (GtkWidget *widget, gpointer data);
static void select_all_cb (GtkWidget *widget, gpointer data);
static void title_changed_cb (GtkHTML *html, const gchar *title, gpointer data);
static void url_requested (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data);
static void entry_goto_url(GtkWidget *widget, gpointer data);
static void goto_url(const gchar *url, gint back_or_forward);
static void on_set_base (GtkHTML *html, const gchar *url, gpointer data);
static void change_status_bar(GtkStatusbar * statusbar, const gchar * text);
gboolean enabled_type(GtkHTML *html, GtkHTMLStream *stream, const gchar *mime_type);
static void http_requested(GtkHTML *html, const gchar *action, const gchar * method,const gchar * encoding, GtkHTMLStream *handle);
static SoupSession *session;

static gchar * decode (const gchar * token);
static gchar * get_data_url (const gchar * action, gsize * length, gchar ** contentType);
static void get_data_url_content(GtkHTML * html, GtkHTMLStream * stream, const gchar* url);

static GtkHTML *html;
static GtkWidget *entry;
static GtkWidget *popup_menu, *popup_menu_back, *popup_menu_forward, *popup_menu_home;
static GtkWidget *toolbar_back, *toolbar_forward;
static GtkWidget *statusbar;

static GList *go_list;
static gint go_position;

static gint redirect_timerId = 0;
static gchar *redirect_url = NULL;

static GtkActionEntry entries[] = {

	{ "FileMenu",
	  NULL,
	  "_File",
	  NULL,
	  NULL },

	{ "DebugMenu",
	  NULL,
	  "_Debug",
	  NULL,
	  NULL },

	{ "Preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (print_preview_cb) },

	{ "Quit",
	  GTK_STOCK_QUIT,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (exit_cb) },

	{ "DumpObjectTree",
	  NULL,
	  "Dump _Object tree",
	  "<control>o",
	  NULL,
	  G_CALLBACK (dump_cb) },

	{ "DumpObjectTreeSimple",
	  NULL,
	  "Dump Object tree (_simple)",
	  "<control>s",
	  NULL,
	  G_CALLBACK (dump_simple_cb) },

	{ "ForceRepaint",
	  NULL,                   /* name, stock id */
	  "Force _Repaint",
	  "<control>r",         /* label, accelerator */
	  "ForceRepaint",                         /* tooltip */
	  G_CALLBACK (redraw_cb) },

	{ "ForceResize",
	  NULL,
	  "Force R_esize",
	  "<control>e",
	  NULL,
	  G_CALLBACK (resize_cb) },

	{ "SelectAll",
	  NULL,
	  "Select _All",
	  "<control>a",
	  NULL,
	  G_CALLBACK (select_all_cb)},

	{ "ShowBugList",
	  NULL,
	  "Show _Bug List",
	  "<control>b",
	  "ShowBugList",
	  G_CALLBACK (bug_cb)},
};

static const gchar *ui_info =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Preview'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='DebugMenu'>"
"       <menuitem action='ShowBugList'/>"
"       <menuitem action='DumpObjectTree'/>"
"       <menuitem action='DumpObjectTreeSimple'/>"
"       <menuitem action='ForceResize'/>"
"       <menuitem action='ForceRepaint'/>"
"       <menuitem action='SelectAll'/>"
"    </menu>"
"  </menubar>"
"</ui>";

/* find examples*/
static void
example_changed_cb (GtkComboBox *combo_box, gpointer data)
{
	gint i = gtk_combo_box_get_active (combo_box);
	Example *example = examples->pdata[i];

	if (example->filename) {
		goto_url(example->filename, 0);
	} else
		goto_url("http://www.gnome.org", 0);
}

/* We want to sort "a2" < "b1" < "B1" < "b2" < "b12". Vastly
 * overengineered
 */
static gint
compare_examples (gconstpointer a,
		  gconstpointer b)
{
	const Example *example_a = *(const Example *const *)a;
	const Example *example_b = *(const Example *const *)b;
	gchar *a_fold, *b_fold;
	const guchar *p, *q;
	gint result = 0;

	/* Special case "Welcome" to sort first */
	if (!example_a->filename)
		return -1;
	if (!example_b->filename)
		return 1;

	a_fold = g_utf8_casefold (example_a->title, -1);
	b_fold = g_utf8_casefold (example_b->title, -1);
	p = (const guchar *)a_fold;
	q = (const guchar *)b_fold;

	while (*p && *q) {
		gboolean p_digit = g_ascii_isdigit (*p);
		gboolean q_digit = g_ascii_isdigit (*q);

		if (p_digit && !q_digit) {
			result = 1;
			goto out;
		}
		else if (!p_digit && q_digit) {
			result = -1;
			goto out;
		}

		if (p_digit) {
			gint num_a = atoi ((const gchar *) p);
			gint num_b = atoi ((const gchar *) q);

			if (num_a < num_b) {
				result = -1;
				goto out;
			}
			else if (num_a > num_b) {
				result = 1;
				goto out;
			}

			while (g_ascii_isdigit (*p))
				p++;
			while (g_ascii_isdigit (*q))
				q++;

		} else {
			gint p_len = 1, q_len = 1;
			gchar *p_str, *q_str;

			while (*(p + p_len) && !g_ascii_isdigit (*(p + p_len)))
				p_len++;
			while (*(q + q_len) && !g_ascii_isdigit (*(q + q_len)))
				q_len++;

			p_str = g_strndup ((gchar *) p, p_len);
			q_str = g_strndup ((gchar *) q, q_len);

			result = g_utf8_collate (p_str, q_str);
			g_free (p_str);
			g_free (q_str);

			if (result != 0)
				goto out;

			p += p_len;
			q += p_len;
		}

		p++;
		q++;
	}

	if (*p)
		result = 1;
	else if (*q)
		result = -1;
	else
		result = g_utf8_collate (example_a->title, example_b->title);

 out:
	g_free (a_fold);
	g_free (b_fold);

	return result;
}

static void
find_examples (void)
{
	GDir *dir;
	GError *error = NULL;
	gchar *cwd;
	Example *example;

	examples = g_ptr_array_new ();

	example = g_new (Example, 1);
	example->filename = NULL;
	example->title = "Home";
	g_ptr_array_add (examples, example);

	dir = g_dir_open ("tests", 0, &error);
	if (!dir) {
		g_printerr ("Cannot open tests directory: %s\n", error->message);
		return;
	}
	cwd = g_get_current_dir ();
	while (TRUE) {
		const gchar *name = g_dir_read_name (dir);

		if (!name)
			break;
		if (!g_str_has_suffix (name, ".html"))
			continue;

		example = g_new (Example, 1);
		example->filename = g_strdup_printf ("file://%s/tests/%s",
					cwd,
					name);
		example->title = g_strndup (name, strlen (name) - 5);

		g_ptr_array_add (examples, example);
		qsort (examples->pdata, examples->len, sizeof (gpointer), compare_examples);
	}

	g_dir_close (dir);
}
/* find exaples */

static GtkWidget *
create_toolbars ()
{
	gint i;
	GtkWidget  *animate_checkbox;
	GtkWidget  *test_combo_box;
	GtkWidget  *label;
	GtkToolItem *item;
	GtkWidget *action_table;

	action_table = gtk_table_new (9, 1, FALSE);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
	gtk_tool_item_set_tooltip_text (item, "Move back");
	g_signal_connect (item, "clicked", G_CALLBACK (back_cb), NULL);
	gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
	toolbar_back = GTK_WIDGET (item);
	gtk_table_attach (
		GTK_TABLE (action_table),
		GTK_WIDGET (item),
		/* X direction */       /* Y direction */
		0, 1,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	gtk_tool_item_set_tooltip_text (item, "Move forward");
	g_signal_connect (item, "clicked", G_CALLBACK (forward_cb), NULL);
	gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
	toolbar_forward = GTK_WIDGET (item);
	gtk_table_attach (
		GTK_TABLE (action_table),
		GTK_WIDGET (item),
		/* X direction */       /* Y direction */
		1, 2,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_STOP);
	gtk_tool_item_set_tooltip_text (item, "Stop loading");
	g_signal_connect (item, "clicked", G_CALLBACK (stop_cb), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		GTK_WIDGET (item),
		/* X direction */       /* Y direction */
		2, 3,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH);
	gtk_tool_item_set_tooltip_text (item, "Reload page");
	g_signal_connect (item, "clicked", G_CALLBACK (reload_cb), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		GTK_WIDGET (item),
		/* X direction */       /* Y direction */
		3, 4,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_HOME);
	gtk_tool_item_set_tooltip_text (item, "Home page");
	g_signal_connect (item, "clicked", G_CALLBACK (home_cb), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		GTK_WIDGET (item),
		/* X direction */       /* Y direction */
		4, 5,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	label = gtk_label_new ("Location:");
	gtk_table_attach (
		GTK_TABLE (action_table),
		label,
		/* X direction */       /* Y direction */
		5, 6,                   0, 1,
		GTK_SHRINK,			GTK_SHRINK,
		0,                      0);

	entry = gtk_entry_new ();
	g_signal_connect (entry, "activate", G_CALLBACK (entry_goto_url), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		entry,
		/* X direction */       /* Y direction */
		6, 7,                   0, 1,
		GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
		0,                      0);

	find_examples ();
	test_combo_box = gtk_combo_box_new_text ();
	for (i = 0; i < examples->len; i++) {
		Example *example = examples->pdata[i];
		gtk_combo_box_append_text (GTK_COMBO_BOX (test_combo_box), example->title);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (test_combo_box), 0);
	g_signal_connect (test_combo_box, "changed", G_CALLBACK (example_changed_cb), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		test_combo_box,
		/* X direction */       /* Y direction */
		7, 8,                   0, 1,
		GTK_SHRINK,  GTK_SHRINK,
		0,                      0);

	animate_checkbox = gtk_check_button_new_with_label ("Disable Animations");
	g_signal_connect (animate_checkbox, "toggled", G_CALLBACK (animate_cb), NULL);
	gtk_table_attach (
		GTK_TABLE (action_table),
		animate_checkbox,
		/* X direction */       /* Y direction */
		8, 9,                   0, 1,
		GTK_SHRINK,  GTK_SHRINK,
		0,                      0);

	return action_table;
}

static gint page_num, pages;
static PangoLayout *layout;

static void
print_footer (GtkHTML *html, GtkPrintContext *context, gdouble x, gdouble y,
              gdouble width, gdouble height, gpointer user_data)
{
	gchar *text;
	cairo_t *cr;

	text = g_strdup_printf ("- %d of %d -", page_num++, pages);

	pango_layout_set_width (layout, width * PANGO_SCALE);
	pango_layout_set_text (layout, text, -1);

	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	g_free (text);
}

static void
draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context,
              gint page_nr, gpointer user_data)
{
	/* XXX GtkHTML's printing API doesn't really fit well with GtkPrint.
	 *     Instead of calling a function for each page, GtkHTML prints
	 *     everything in one shot. */

	PangoFontDescription *desc;
	PangoFontMetrics *metrics;
	gdouble footer_height;

	desc = pango_font_description_from_string ("Helvetica 16px");

	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description (layout, desc);

	metrics = pango_context_get_metrics (
		pango_layout_get_context (layout),
		desc, gtk_get_default_language ());
	footer_height = (pango_font_metrics_get_ascent (metrics) +
		pango_font_metrics_get_descent (metrics)) / PANGO_SCALE;
	pango_font_metrics_unref (metrics);

	pango_font_description_free (desc);

	page_num = 1;
	pages = gtk_html_print_page_get_pages_num (
		html, context, .0, footer_height);

	gtk_html_print_page_with_header_footer (
		html, context, .0, footer_height,
		NULL, print_footer, NULL);

	g_object_unref (layout);
}

static void
print_preview_cb (GtkWidget *widget,
		  gpointer data)
{
	GtkPrintOperation *operation;

	operation = gtk_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (
		operation, "draw-page",
		G_CALLBACK (draw_page_cb), NULL);

	gtk_print_operation_run (
		operation, GTK_PRINT_OPERATION_ACTION_PREVIEW, NULL, NULL);

	g_object_unref (operation);
}

static void
dump_cb (GtkWidget *widget, gpointer data)
{
	g_print ("Object Tree\n");
	g_print ("-----------\n");

	gtk_html_debug_dump_tree (html->engine->clue, 0);
}

static void
dump_simple_cb (GtkWidget *widget, gpointer data)
{
	g_print ("Simple Object Tree\n");
	g_print ("-----------\n");

	gtk_html_debug_dump_tree_simple (html->engine->clue, 0);
}

static void
resize_cb (GtkWidget *widget, gpointer data)
{
	g_print ("forcing resize\n");
	html_engine_calc_size (html->engine, NULL);
}

static void
select_all_cb (GtkWidget *widget, gpointer data)
{
	g_print ("select all\n");
	gtk_html_select_all (html);
}

static void
redraw_cb (GtkWidget *widget, gpointer data)
{
	g_print ("forcing redraw\n");
	gtk_widget_queue_draw (GTK_WIDGET (html));
}

static void
animate_cb (GtkToggleButton *togglebutton, gpointer data)
{
	gtk_html_set_animate (html, !gtk_toggle_button_get_mode(togglebutton));
}

static void
title_changed_cb (GtkHTML *html, const gchar *title, gpointer data)
{
	gchar *s;

	s = g_strconcat ("GtkHTML: ", title, NULL);
	gtk_window_set_title (GTK_WINDOW (data), s);
	g_free (s);
}

static void
entry_goto_url(GtkWidget *widget, gpointer data)
{
	gchar *tmpurl;

	tmpurl = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));

	/* Add "http://" if no protocol is specified */
	if (strchr(tmpurl, ':')) {
		goto_url (tmpurl, 0);
	} else {
		gchar *url;
		url = g_strdup_printf("http://%s", tmpurl);
		goto_url (url, 0);
		g_free(url);
	}
	g_free (tmpurl);
}

static void
home_cb (GtkWidget *widget, gpointer data)
{
	goto_url("http://www.gnome.org", 0);
}

static void
back_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	go_position++;

	if ((item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);
		gtk_widget_set_sensitive(popup_menu_forward, TRUE);
		gtk_widget_set_sensitive(toolbar_forward, TRUE);

		if (go_position == (g_list_length(go_list) - 1)) {

			gtk_widget_set_sensitive(popup_menu_back, FALSE);
			gtk_widget_set_sensitive(toolbar_back, FALSE);
		}

	} else
		go_position--;
}

static void
forward_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	go_position--;

	if ((go_position >= 0) && (item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);

		gtk_widget_set_sensitive(popup_menu_back, TRUE);
		gtk_widget_set_sensitive(toolbar_back, TRUE);

		if (go_position == 0) {
			gtk_widget_set_sensitive(popup_menu_forward, FALSE);
			gtk_widget_set_sensitive(toolbar_forward, FALSE);
		}
	} else
		go_position++;
}

static void
reload_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	if ((item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);
	}
}

static void
stop_cb (GtkWidget *widget, gpointer data)
{
	/* Kill all requests */
	soup_session_abort (session);
}

static void
load_done (GtkHTML *html)
{
	/* TODO2 animator stop

	if (exit_when_done)
		gtk_main_quit();
	*/
}

static gint
on_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GtkMenu *menu;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	menu = GTK_MENU (popup_menu);

	if (event->type == GDK_BUTTON_PRESS) {

		if (event->button == 3) {
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
					event->button, event->time);
			return TRUE;
		}
	}

	return FALSE;
}

static void
on_set_base (GtkHTML *html, const gchar *url, gpointer data)
{
	gtk_entry_set_text (GTK_ENTRY (entry), url);
}

static gboolean
redirect_timer_event (gpointer data) {
	g_print("Redirecting to '%s' NOW\n", redirect_url);
	goto_url(redirect_url, 0);

	/*	OBS: redirect_url is freed in goto_url */

	return FALSE;
}

static void
on_redirect (GtkHTML *html, const gchar *url, gint delay, gpointer data) {
	g_print("Redirecting to '%s' in %d seconds\n", url, delay);

	if (redirect_timerId == 0) {

		redirect_url = g_strdup(url);

		redirect_timerId = g_timeout_add (delay * 1000,(GtkFunction) redirect_timer_event, NULL);
	}
}

static void
on_submit (GtkHTML *html, const gchar *method, const gchar *action, const gchar *encoding, gpointer data) {
	if (!g_ascii_strcasecmp(method, "GET")) {
		gchar *tmpstr;
		gchar *tmpencoding;
		if (encoding)
			tmpencoding = g_strdup(encoding);
		else
			tmpencoding = g_strdup("");
		tmpstr = g_new0(gchar,strlen(action) + strlen(tmpencoding) + 2);
		strcpy (tmpstr, action);
		if (*tmpencoding != 0) {
			if (!strchr(tmpstr,'?'))
				strcat(tmpstr, "?");
			strcat (tmpstr, tmpencoding);
		}
		goto_url (tmpstr, 0);
		g_free (tmpstr);
		g_free (tmpencoding);
	} else if(! g_ascii_strcasecmp(method, "POST")) {
		GtkHTMLStream* handle = gtk_html_begin_content (html, (gchar *)gtk_html_get_default_content_type (html));
		http_requested(html, action, method, encoding, handle);
	} else {
		g_warning ("Unsupported submit method '%s'\n", method);
	}

}

static void
change_status_bar(GtkStatusbar * statusbar, const gchar * text)
{
	gchar *msg;

	if (!text)
		msg = g_strdup ("");
	else
		msg = g_strdup (text);

	gtk_statusbar_pop (statusbar, 0);
	 /* clear any previous message,
	  * underflow is allowed
	  */
	gtk_statusbar_push (statusbar, 0, msg);

	g_free (msg);
}

static void
on_url (GtkHTML *html, const gchar *url, gpointer data)
{
	change_status_bar (GTK_STATUSBAR(statusbar), url);
}

static void
on_link_clicked (GtkHTML *html, const gchar *url, gpointer data)
{
	goto_url (url, 0);
}

/* simulate an async object isntantiation */
static gint
object_timeout(GtkHTMLEmbedded *eb)
{
	GtkWidget *w;

	w = gtk_check_button_new();
	gtk_widget_show(w);

	printf("inserting custom widget after a delay ...\n");
	gtk_html_embedded_set_descent(eb, rand()%8);
	gtk_container_add (GTK_CONTAINER(eb), w);
	g_object_unref (eb);

	return FALSE;
}

static gboolean
object_requested_cmd (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data)
{
	/* printf("object requested, wiaint a bit before creating it ...\n"); */

	if (eb->classid && strcmp (eb->classid, "mine:NULL") == 0)
		return FALSE;

	g_object_ref (eb);
	g_timeout_add(rand() % 5000 + 1000, (GtkFunction) object_timeout, eb);
	/* object_timeout (eb); */

	return TRUE;
}

gboolean enabled_type(GtkHTML *html, GtkHTMLStream *stream,
		       const gchar *mime_type)
{
	gboolean knowtype = FALSE;
	gchar ** types;
	gsize len_mimetype;
	gchar * cleantype;
	if (!mime_type)
		return FALSE;
	if (!stream)
		return FALSE;
	len_mimetype =  strcspn(mime_type," \t;");
	cleantype = g_new(gchar, len_mimetype + 1);
	memcpy(cleantype,mime_type,len_mimetype);
	cleantype[len_mimetype] = 0;
	types = gtk_html_get_types(html, stream);
	if (types != NULL)
		while (*types != NULL && !knowtype) {
			if (!strncmp(*types,cleantype, len_mimetype))
				knowtype = TRUE;
			types ++;
		}
	g_free(cleantype);
	return knowtype;
}

static void
got_data (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
	const gchar *ContentType;
	GtkHTMLStream *handle = user_data;
	gchar * href;

	href = soup_uri_to_string(soup_message_get_uri(msg), FALSE);

	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_warning ("%d - %s(%s)", msg->status_code, msg->reason_phrase, href);
		gtk_html_end (html, handle, GTK_HTML_STREAM_ERROR);
		return;
	}
	/* Enable change content type in engine */
	gtk_html_set_default_engine(html, TRUE);

	ContentType = soup_message_headers_get (msg->response_headers, "Content-type");

	if (ContentType != NULL)
		if (!enabled_type (html, handle, ContentType))
		{
			/*
			check received and understanding types
			gchar ** types;
			g_print("Recieved type:\n\t%s\n", ContentType);
			g_print("Supported type:\n");
			types = gtk_html_get_types(html, handle);
			if (types != NULL)
				while (*types != NULL) {
					g_print("\t%s\n", *types);
					types ++;
				}
			*/
			if (enabled_type (html, handle, "text/html")) {/*support html*/

				if (!g_ascii_strncasecmp (ContentType, "text/plain", strlen("text/plain"))) {
					/*generate html with include text*/
					gtk_html_write (html, handle, "<html><body><pre>",strlen("<html><body><pre>"));
					gtk_html_write (html, handle, msg->response_body->data, msg->response_body->length);
					gtk_html_write (html, handle, "</pre></body></html>",strlen("</pre></body></html>"));
					gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
					g_free (href);
					return;
				} else if (	href && /*i know from*/
						!g_ascii_strncasecmp(ContentType,"image/",6 /*strlen("image/")*/) /*recieve image*/
				) {
					/*generate html with include image*/
					gchar * new_content = g_strdup_printf("<html><body><img src='%s'>%s</img></body></html>",href,href);
					gtk_html_write (html, handle, new_content, strlen(new_content));
					gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
					g_free (href);
					g_free (new_content);
					return;
				}
			}
		}
	if (ContentType != NULL)
		gtk_html_set_mime_type (html, handle, ContentType);

	gtk_html_set_href (html, handle, href);
	g_free(href);

	gtk_html_write (html, handle, msg->response_body->data,
			msg->response_body->length);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}

/* data uri */

/*
 * write content for data url to stream
 */
static void
get_data_url_content(GtkHTML * html, GtkHTMLStream * stream, const gchar* url) {

	gsize length = 0;
	gchar *ContentType = NULL;
	gchar *buf = NULL;
	
	g_return_if_fail (url!=NULL);
	g_return_if_fail (stream!=NULL);
	g_return_if_fail (html!=NULL);
	
	buf = get_data_url (url, &length, &ContentType);
	if (buf!= NULL) {
		if (ContentType!=NULL) {
			gtk_html_set_mime_type (html, stream, ContentType);
			gtk_html_set_href (html, stream, url);
			g_free (ContentType);
		}
		gtk_html_write (html, stream, buf, length);
		gtk_html_end (html, stream, GTK_HTML_STREAM_OK);
		g_free (buf);
	} else
		gtk_html_end (html, stream, GTK_HTML_STREAM_ERROR);
}


/*
 * support data url by RFC 2397
 * dataurl := "data:" [ contentType ] [ ";base64" ] "," data
 *
 * return content and as params: content length and content type
 */
static gchar*
get_data_url (const gchar * action, gsize * length, gchar ** contentType) {
	guchar *buf = NULL;
	g_return_val_if_fail (action != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);
	g_return_val_if_fail (contentType != NULL, NULL);
	if (!strncmp(action, "data:", 5 /*strlen("data:")*/)) {
		const gchar *real_action = action + 5 /*strlen("data:")*/;
		/*find first ','*/
		const gchar *start_data = strchr(real_action, ',');
		if (start_data != NULL) {
			gboolean isbase64 = FALSE;
			const char *endcontent = start_data;
			gchar *result_decode = NULL;
			/* is base 64?*/
			if ( (start_data - real_action) > 7 ) /*8 == strlen(";base64,")*/
				if (!strncmp(start_data - 7, ";base64",  7)) {
					isbase64 = TRUE;
					endcontent -= 7;
				}
			/* get content type */
			{
				gsize ContentType_length = endcontent - real_action;
				*contentType = g_new(gchar, ContentType_length + 1);
				memcpy(*contentType, real_action, ContentType_length);
				*(*contentType + ContentType_length) = 0;
			}
			/*unescape content*/
			result_decode = decode(start_data + 1); /* skip ','*/
			*length = strlen( result_decode );
			if (isbase64) {
				gint state = 0;
				guint save = 0;
				buf = g_new(guchar,( (*length) * 3) / 4 + 3);
				*length = g_base64_decode_step(result_decode,
					       strlen(result_decode),
					       buf, &state, &save);
			} else {
				buf = g_new(guchar, (*length) + 1);
				buf [*length] = 0;
				memcpy(buf, result_decode, *length);
			}
			g_free(result_decode);
		}
    }
    return (gchar *)buf;
}

/*unescape url*/
static gchar *
decode(const gchar * token)
{
    const gchar *full_pos;
    gchar *resulted;
    gchar *write_pos;
    const gchar *read_pos;

    if (token == NULL)
		return NULL;

    /*stop pointer */
    full_pos = token + strlen (token);
    resulted = g_new (gchar, strlen(token) + 1);
    write_pos = resulted;
    read_pos = token;
    while (read_pos < full_pos) {
	size_t count_chars = strcspn (read_pos, "%");

	memcpy (write_pos, read_pos, count_chars);
	write_pos += count_chars;
	read_pos += count_chars;
	/*may be end string? */
	if (read_pos < full_pos)
	    if (*read_pos == '%') {
		/*skip not needed % */
		read_pos++;
		if (*(read_pos) != 0)
		    if (*(read_pos + 1) != 0) {
				gchar save[3];
				save[0] = *read_pos;
				save[1] = *(read_pos + 1);
				save[2] = 0;
				(*write_pos) = strtol (save, NULL, 16);
				write_pos += 1;
				read_pos += 2;
		    }
	    }
    }
    *write_pos = 0;
    return resulted;
}

/*end datauri*/

static void
http_requested(GtkHTML *html, const gchar *action, const gchar * method,const gchar * encoding, GtkHTMLStream *handle)
{
	if (action && !strncmp (action, "http", 4)) {
		SoupMessage *msg = NULL;
		if (!g_ascii_strcasecmp(method, "POST")) {
			msg = soup_message_new("POST", action);
			soup_message_set_request(msg,
				"application/x-www-form-urlencoded",
				SOUP_MEMORY_TAKE, (gchar *) action,
				strlen(encoding));
		} else if (!g_ascii_strcasecmp(method, "GET")) {
			msg = soup_message_new("GET", action);
		}
		if (msg) {
			if (gtk_html_get_base (html) != NULL)
				soup_message_headers_append(msg->request_headers, "Referer",
					gtk_html_get_base(html));
			soup_session_queue_message (session, msg, got_data, handle);
		}
	}
}

static void
url_requested (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data)
{

	if (url && !strncmp (url, "http", 4)) {
		http_requested(html, url, "GET", NULL, handle);
	} else if (url && !strncmp (url, "data:", 5)) {
		/* RFC 2397 (data url) */
		get_data_url_content( html, handle, url);
	} else if (url && !strncmp (url, "file:", 5)) {
		gchar *filename = gtk_html_filename_from_uri (url);
		struct stat st;
		gchar *buf;
		gint fd, nread, total;

		gtk_html_set_href (html, handle, url);

		fd = g_open (filename, O_RDONLY|O_BINARY, 0);
		g_free (filename);
		if (fd != -1 && fstat (fd, &st) != -1) {
			buf = g_malloc (st.st_size);
			for (nread = total = 0; total < st.st_size; total += nread) {
				nread = read (fd, buf + total, st.st_size - total);
				if (nread == -1) {
					if (errno == EINTR)
						continue;
					g_warning ("read error: %s", g_strerror (errno));
					gtk_html_end (html, handle, GTK_HTML_STREAM_ERROR);
					break;
				}
				gtk_html_write (html, handle, buf + total, nread);
			}
			g_free (buf);
			if (nread != -1)
				gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
		} else
			gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
		if (fd != -1)
			close (fd);
	} else
		g_warning ("Unrecognized URL %s", url);
}

static void
go_list_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;
	gint num;
	/* Only if the item was selected, not deselected */
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget))) {

		go_position = GPOINTER_TO_INT(data);

		if ((item = g_list_nth_data(go_list, go_position))) {

			goto_url(item->url, 1);
			num = g_list_length(go_list);

			if (go_position == 0 || num < 2) {
				gtk_widget_set_sensitive(popup_menu_forward, FALSE);
				gtk_widget_set_sensitive(toolbar_forward, FALSE);
			} else {
				gtk_widget_set_sensitive(popup_menu_forward, TRUE);
				gtk_widget_set_sensitive(toolbar_forward, TRUE);
			}
			if (go_position == (num - 1) || num < 2) {
				gtk_widget_set_sensitive(popup_menu_back, FALSE);
				gtk_widget_set_sensitive(toolbar_back, FALSE);
			} else {
				gtk_widget_set_sensitive(popup_menu_back, TRUE);
				gtk_widget_set_sensitive(toolbar_back, TRUE);
			}
		}
	}
}

static void remove_go_list(gpointer data, gpointer user_data) {
	go_item *item = (go_item *)data;

	if (item->widget)
		gtk_widget_destroy(item->widget);

	item->widget = NULL;
}

static void
goto_url(const gchar *url, gint back_or_forward)
{
	gint tmp, i;
	go_item *item;
	GSList *group = NULL;
	GtkHTMLStream * handle;
	/* Kill all requests */
	soup_session_abort (session);

	/* Remove any pending redirection */
	if (redirect_timerId) {
		g_source_remove(redirect_timerId);

		redirect_timerId = 0;
	}

	/* TODO2 animator start */
	handle = gtk_html_begin_content (html, (gchar *)gtk_html_get_default_content_type (html));

	/* Yuck yuck yuck.  Well this code is butt-ugly already
	anyway.  */

	on_set_base (NULL, url, NULL);
	url_requested (html, url, handle, NULL);

	if (!back_or_forward) {
		if (go_position) {
			/* Removes "Forward entries"*/
			tmp = go_position;
			while (tmp) {
				item = g_list_nth_data(go_list, --tmp);
				go_list = g_list_remove(go_list, item);
				if (item->url)
					g_free(item->url);
				if (item->title)
					g_free(item->title);
				if (item->url)
					gtk_widget_destroy(item->widget);
				g_free(item);
			}
			go_position = 0;
		}

		/* Removes old entries if the list is to big */
		tmp = g_list_length(go_list);
		while (tmp > MAX_GO_ENTRIES) {
			item = g_list_nth_data(go_list, MAX_GO_ENTRIES);

			if (item->url)
				g_free(item->url);
			if (item->title)
				g_free(item->title);
			if (item->url)
				gtk_widget_destroy(item->widget);
			g_free(item);

			go_list = g_list_remove(go_list, item);
			tmp--;
		}
		gtk_widget_set_sensitive(popup_menu_forward, FALSE);
		gtk_widget_set_sensitive(toolbar_forward, FALSE);

		item = g_malloc0(sizeof(go_item));
		item->url = g_strdup(url);

		/* Remove old go list */
		g_list_foreach(go_list, remove_go_list, NULL);

		/* Add new url to go list */
		go_list = g_list_prepend(go_list, item);

		/* Create a new go list menu */
		tmp = g_list_length(go_list);
		group = NULL;

		for (i=0;i<tmp;i++) {

			item = g_list_nth_data(go_list, i);
			item->widget = gtk_radio_menu_item_new_with_label(group, item->url);

			g_signal_connect (item->widget, "activate",
					  G_CALLBACK (go_list_cb), GINT_TO_POINTER (i));

			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item->widget));

			if (i == 0)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->widget), TRUE);

			gtk_widget_show(item->widget);

		}
		/* Enable the "Back" button if there are more then one url in the list */
		if (g_list_length(go_list) > 1) {

			gtk_widget_set_sensitive(popup_menu_back, TRUE);
			gtk_widget_set_sensitive(toolbar_back, TRUE);
		}
	} else {
		/* Update current link in the go list */
		item = g_list_nth_data(go_list, go_position);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->widget), TRUE);
	}

	if (redirect_url) {

		g_free(redirect_url);
		redirect_url = NULL;
	}
}

static void
bug_cb (GtkWidget *widget, gpointer data)
{
	gchar *cwd, *filename, *url;

	cwd = g_get_current_dir ();
	filename = g_strdup_printf("%s/bugs.html", cwd);
	url = g_filename_to_uri(filename, NULL, NULL);
	goto_url(url, 0);
	g_free(url);
	g_free(filename);
	g_free(cwd);
}

static void
exit_cb (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

static gboolean
motion_notify_event (GtkHTML *html, GdkEventMotion *event, gpointer data)
{
	const gchar *id;

	id = gtk_html_get_object_id_at (html, event->x, event->y);
	if (id)
		change_status_bar(GTK_STATUSBAR (statusbar), id);

	return FALSE;
}

gint
main (gint argc, gchar *argv[])
{
	GtkWidget *app, *bar, *main_table;
	GtkWidget *html_widget;
	GtkWidget *scrolled_window;
	GtkActionGroup *action_group;
	GtkUIManager *merge;
	GError *error = NULL;
#ifdef HAVE_NEWSOUP
	SoupCookieJar *cookie_jar;
#endif

#ifdef MEMDEBUG
	gpointer p = malloc (1024);	/* to make linker happy with ccmalloc */
#endif

	gtk_init(&argc, &argv);

	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	g_signal_connect (app, "delete_event", G_CALLBACK (exit_cb), NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	statusbar = gtk_statusbar_new ();
	/* Menus */
	action_group = gtk_action_group_new ("AppWindowActions");
	gtk_action_group_add_actions (action_group,
	    entries, G_N_ELEMENTS (entries),
	    scrolled_window);

	merge = gtk_ui_manager_new ();
	g_object_set_data_full (G_OBJECT (scrolled_window), "ui-manager", merge,
			      g_object_unref);

	gtk_ui_manager_insert_action_group (merge, action_group, 0);
	gtk_window_add_accel_group (GTK_WINDOW (app),
				  gtk_ui_manager_get_accel_group (merge));

	if (!gtk_ui_manager_add_ui_from_string (merge, ui_info, -1, &error)) {
	  g_message ("building menus failed: %s", error->message);
	  g_error_free (error);
	}

	bar = gtk_ui_manager_get_widget (merge, "/MenuBar");
	gtk_widget_show (bar);
	/* main table*/
	main_table = gtk_table_new (1, 4, FALSE);

	gtk_table_attach (GTK_TABLE (main_table),
                        bar,
                        /* X direction */       /* Y direction */
                        0, 1,                   0, 1,
                        GTK_EXPAND | GTK_FILL,  GTK_SHRINK,
                        0,                      0);
	gtk_table_attach (GTK_TABLE (main_table),
                        create_toolbars (),
                        /* X direction */       /* Y direction */
                        0, 1,                   1, 2,
                        GTK_EXPAND | GTK_FILL,  GTK_SHRINK,
                        0,                      0);
	gtk_table_attach (GTK_TABLE (main_table),
                        scrolled_window,
                        /* X direction */       /* Y direction */
                        0, 1,                   2, 3,
                        GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                        0,                      0);
	gtk_table_attach (GTK_TABLE (main_table),
                        statusbar,
                        /* X direction */       /* Y direction */
                        0, 1,                   3, 4,
                        GTK_EXPAND | GTK_FILL,  GTK_SHRINK,
                        0,                      0);
	/*app*/
	gtk_container_add (GTK_CONTAINER (app), main_table);

	html_widget = gtk_html_new ();
	html = GTK_HTML (html_widget);
	gtk_html_set_allow_frameset (html, TRUE);
	gtk_html_load_empty (html);
	/* gtk_html_set_default_background_color (GTK_HTML (html_widget), &bgColor); */
	/* gtk_html_set_editable (GTK_HTML (html_widget), TRUE); */

	gtk_container_add (GTK_CONTAINER (scrolled_window), html_widget);

	/* Create a popup menu with disabled back and forward items */
	popup_menu = gtk_menu_new();

	popup_menu_back = gtk_menu_item_new_with_label ("Back");
	gtk_widget_set_sensitive (popup_menu_back, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), popup_menu_back);
	gtk_widget_show (popup_menu_back);
	g_signal_connect (popup_menu_back, "activate", G_CALLBACK (back_cb), NULL);

	popup_menu_forward = gtk_menu_item_new_with_label ("Forward");
	gtk_widget_set_sensitive (popup_menu_forward, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), popup_menu_forward);
	gtk_widget_show (popup_menu_forward);
	g_signal_connect (popup_menu_forward, "activate", G_CALLBACK (forward_cb), NULL);

	popup_menu_home = gtk_menu_item_new_with_label ("Home");
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), popup_menu_home);
	gtk_widget_show (popup_menu_home);
	g_signal_connect (popup_menu_home, "activate", G_CALLBACK (home_cb), NULL);

	/* End of menu creation */

	gtk_widget_set_events (html_widget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (html, "title_changed", G_CALLBACK (title_changed_cb), (gpointer)app);
	g_signal_connect (html, "url_requested", G_CALLBACK (url_requested), (gpointer)app);
	g_signal_connect (html, "load_done", G_CALLBACK (load_done), (gpointer)app);
	g_signal_connect (html, "on_url", G_CALLBACK (on_url), (gpointer)app);
	g_signal_connect (html, "set_base", G_CALLBACK (on_set_base), (gpointer)app);
	g_signal_connect (html, "button_press_event", G_CALLBACK (on_button_press_event), popup_menu);
	g_signal_connect (html, "link_clicked", G_CALLBACK (on_link_clicked), NULL);
	g_signal_connect (html, "redirect", G_CALLBACK (on_redirect), NULL);
	g_signal_connect (html, "submit", G_CALLBACK (on_submit), NULL);
	g_signal_connect (html, "object_requested", G_CALLBACK (object_requested_cmd), NULL);
	g_signal_connect (html, "motion_notify_event", G_CALLBACK (motion_notify_event), app);

#if 0
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (html), TRUE, TRUE, 0);
	vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (html)->vadjustment);
	gtk_box_pack_start (GTK_BOX (hbox), vscrollbar, FALSE, TRUE, 0);

#endif
	gtk_widget_realize (GTK_WIDGET (html));

	gtk_window_set_default_size (GTK_WINDOW (app), 540, 400);
	gtk_window_set_focus (GTK_WINDOW (app), GTK_WIDGET (html));

	gtk_widget_show_all (app);

	session = soup_session_async_new ();

#ifdef HAVE_NEWSOUP
	cookie_jar = soup_cookie_jar_text_new ("./cookies.txt", FALSE);
	soup_session_add_feature (session, SOUP_SESSION_FEATURE (cookie_jar));
#endif

	if (argc > 1 && *argv [argc - 1] != '-')
		goto_url (argv [argc - 1], 0);

	gtk_main ();

#ifdef MEMDEBUG

	/* g_object_unref (html_widget); */
	free (p);
#endif

	return 0;
}
