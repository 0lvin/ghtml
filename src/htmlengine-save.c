/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright (C) 1998 World Wide Web Consortium
    Copyright (C) 2000 Helix Code, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    Author: Ettore Perazzoli <ettore@helixcode.com>
    `encode_entities()' adapted from gnome-xml by Daniel Veillard
    <Daniel.Veillard@w3.org>.
*/

#include "htmlengine.h"


static void
save_forall (HTMLObject *object,
	     gpointer data)
{
	HTMLEngineSaveState *s;

	s = (HTMLEngineSaveState *) data;
	if (s->error)
		return;

	if (! html_object_save (object, s))
		s->error = TRUE;
}

gboolean
html_engine_save (const HTMLEngine *engine,
		  HTMLEngineSaveReceiverFn receiver,
		  gpointer user_data)
{
	HTMLEngineSaveState state;
	gboolean retval;

	if (engine->clue == NULL) {
		/* Empty document.  */
		return FALSE;
	}

	state.engine = engine;
	state.receiver = receiver;
	state.br_count = 0;
	state.error = FALSE;
	state.user_data = user_data;

	html_object_forall (engine->clue, save_forall, &state);

	retval = ! state.error;

	return retval;
}


/* This routine was originally written by Daniel Velliard, (C) 1998 World Wide
   Web Consortium.  */
static gchar *
encode_entities (const guchar *input,
		 guint len,
		 guint *encoded_len_return)
{
    const guchar *cur = input;
    guchar *buffer = NULL;
    guchar *out = NULL;
    gint buffer_size = 0;
    gint html = 0;
    guint count;

    /* Allocate an translation buffer.  */
    buffer_size = 1000;
    buffer = g_malloc (buffer_size);

    out = buffer;
    count = 0;

    while (count < len) {
        if (out - buffer > buffer_size - 100) {
	    gint index = out - buffer;

	    buffer_size *= 2;
	    buffer = g_realloc (buffer, buffer_size);
	    out = &buffer[index];
	}

	/* By default one have to encode at least '<', '>', '"' and '&'.  */

	if (*cur == '<') {
	    *out++ = '&';
	    *out++ = 'l';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '>') {
	    *out++ = '&';
	    *out++ = 'g';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '&') {
	    *out++ = '&';
	    *out++ = 'a';
	    *out++ = 'm';
	    *out++ = 'p';
	    *out++ = ';';
	} else if (*cur == '"') {
	    *out++ = '&';
	    *out++ = 'q';
	    *out++ = 'u';
	    *out++ = 'o';
	    *out++ = 't';
	    *out++ = ';';
	} else if ((*cur == '\'') && (!html)) {
	    *out++ = '&';
	    *out++ = 'a';
	    *out++ = 'p';
	    *out++ = 'o';
	    *out++ = 's';
	    *out++ = ';';
	} else if (((*cur >= 0x20) && (*cur < 0x80))
		   || (*cur == '\n') || (*cur == '\r') || (*cur == '\t')) {
	    /* Default case, just copy. */
	    *out++ = *cur;
	} else {
	    char buf[10], *ptr;

	    g_snprintf(buf, 9, "&#%d;", *cur);

            ptr = buf;
	    while (*ptr != 0)
		    *out++ = *ptr++;
	}

	cur++;
	count++;
    }

    *out = 0;
    *encoded_len_return = out - buffer;

    return buffer;
}

gboolean
html_engine_save_encode (HTMLEngineSaveState *state,
			 const gchar *buffer,
			 guint length)
{
	guchar *encoded_buffer;
	guint encoded_length;

	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (buffer != NULL, FALSE);

	if (length == 0)
		return TRUE;

	encoded_buffer = encode_entities ((const guchar *) buffer, length, &encoded_length);

	return state->receiver (state->engine,
				encoded_buffer,
				encoded_length,
				state->user_data);
}

gboolean
html_engine_save_encode_string (HTMLEngineSaveState *state,
				const gchar *s)
{
	guint len;

	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (s != NULL, FALSE);

	len = strlen (s);

	return html_engine_save_encode (state, s, len);
}

gboolean
html_engine_save_output_string (HTMLEngineSaveState *state,
				const gchar *s)
{
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (s != NULL, FALSE);

	return state->receiver (state->engine, s, strlen (s), state->user_data);
}
