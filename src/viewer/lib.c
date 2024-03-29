/*
   Internal file viewer for the Midnight Commander
   Common finctions (used from some other mcviewer functions)

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995, 1998 Miguel de Icaza
   1994, 1995 Janne Kukonlehto
   1995 Jakub Jelinek
   1996 Joseph M. Hinkle
   1997 Norbert Warmuth
   1998 Pavel Machek
   2004 Roland Illig <roland.illig@gmx.de>
   2005 Roland Illig <roland.illig@gmx.de>
   2009 Slava Zanko <slavazanko@google.com>
   2009 Andrew Borodin <aborodin@vmail.ru>
   2009 Ilia Maslakov <il.smind@gmail.com>

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>

#include <limits.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* save_file_position() */
#include "lib/lock.h"           /* unlock_file() */
#include "lib/widget.h"
#include "lib/charsets.h"

#include "src/main.h"
#include "src/selcodepage.h"

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

#define OFF_T_BITWIDTH (unsigned int) (sizeof (off_t) * CHAR_BIT - 1)
const off_t INVALID_OFFSET = (off_t) - 1;
const off_t OFFSETTYPE_MAX = ((off_t) 1 << (OFF_T_BITWIDTH - 1)) - 1;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_magic_mode (mcview_t * view)
{
    char *filename, *command;

    mcview_altered_magic_flag = 1;
    view->magic_mode = !view->magic_mode;
    filename = g_strdup (view->filename);
    command = g_strdup (view->command);

    mcview_done (view);
    mcview_init (view);
    mcview_load (view, command, filename, 0);
    g_free (filename);
    g_free (command);
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_wrap_mode (mcview_t * view)
{
    if (view->text_wrap_mode)
        view->dpy_start = mcview_bol (view, view->dpy_start, 0);
    view->text_wrap_mode = !view->text_wrap_mode;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_nroff_mode (mcview_t * view)
{
    view->text_nroff_mode = !view->text_nroff_mode;
    mcview_altered_nroff_flag = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_hex_mode (mcview_t * view)
{
    view->hex_mode = !view->hex_mode;

    if (view->hex_mode)
    {
        view->hex_cursor = view->dpy_start;
        view->dpy_start = mcview_offset_rounddown (view->dpy_start, view->bytes_per_line);
        widget_want_cursor (view->widget, 1);
    }
    else
    {
        view->dpy_start = view->hex_cursor;
        mcview_moveto_bol (view);
        widget_want_cursor (view->widget, 0);
    }
    mcview_altered_hex_mode = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_ok_to_quit (mcview_t * view)
{
    int r;

    if (view->change_list == NULL)
        return TRUE;

    if (!midnight_shutdown)
    {
        query_set_sel (2);
        r = query_dialog (_("Quit"),
                          _("File was modified. Save with exit?"), D_NORMAL, 3,
                          _("&Yes"), _("&No"), _("&Cancel quit"));
    }
    else
    {
        r = query_dialog (_("Quit"),
                          _("Midnight Commander is being shut down.\nSave modified file?"),
                          D_NORMAL, 2, _("&Yes"), _("&No"));
        /* Esc is No */
        if (r == -1)
            r = 1;
    }

    switch (r)
    {
    case 0:                    /* Yes */
        return mcview_hexedit_save_changes (view) || midnight_shutdown;
    case 1:                    /* No */
        mcview_hexedit_free_change_list (view);
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_init (mcview_t * view)
{
    size_t i;

    view->filename = NULL;
    view->workdir = NULL;
    view->command = NULL;
    view->search_nroff_seq = NULL;

    mcview_set_datasource_none (view);

    view->growbuf_in_use = FALSE;
    /* leave the other growbuf fields uninitialized */

    view->hexedit_lownibble = FALSE;
    view->locked = FALSE;
    view->coord_cache = NULL;

    view->dpy_start = 0;
    view->dpy_text_column = 0;
    view->dpy_end = 0;
    view->hex_cursor = 0;
    view->cursor_col = 0;
    view->cursor_row = 0;
    view->change_list = NULL;

    /* {status,ruler,data}_area are left uninitialized */

    view->dirty = 0;
    view->dpy_bbar_dirty = TRUE;
    view->bytes_per_line = 1;

    view->search_start = 0;
    view->search_end = 0;

    view->marker = 0;
    for (i = 0; i < sizeof (view->marks) / sizeof (view->marks[0]); i++)
        view->marks[i] = 0;

    view->move_dir = 0;
    view->update_steps = 0;
    view->update_activate = 0;

    view->saved_bookmarks = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_done (mcview_t * view)
{
    /* Save current file position */
    if (mcview_remember_file_position && view->filename != NULL)
    {
        char *canon_fname;
        canon_fname = vfs_canon (view->filename);
        save_file_position (canon_fname, -1, 0, view->dpy_start, view->saved_bookmarks);
        view->saved_bookmarks = NULL;
        g_free (canon_fname);
    }

    /* Write back the global viewer mode */
    mcview_default_hex_mode = view->hex_mode;
    mcview_default_nroff_flag = view->text_nroff_mode;
    mcview_default_magic_flag = view->magic_mode;
    mcview_global_wrap_mode = view->text_wrap_mode;

    /* Free memory used by the viewer */

    /* view->widget needs no destructor */

    g_free (view->filename);
    view->filename = NULL;
    g_free (view->workdir);
    view->workdir = NULL;
    g_free (view->command);
    view->command = NULL;

    mcview_close_datasource (view);
    /* the growing buffer is freed with the datasource */

    coord_cache_free (view->coord_cache), view->coord_cache = NULL;

    if (view->converter == INVALID_CONV)
        view->converter = str_cnv_from_term;

    if (view->converter != str_cnv_from_term)
    {
        str_close_conv (view->converter);
        view->converter = str_cnv_from_term;
    }

    mc_search_free (view->search);
    view->search = NULL;
    g_free (view->last_search_string);
    view->last_search_string = NULL;
    mcview_nroff_seq_free (&view->search_nroff_seq);
    mcview_hexedit_free_change_list (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_codeset (mcview_t * view)
{
#ifdef HAVE_CHARSET
    const char *cp_id = NULL;

    view->utf8 = TRUE;
    cp_id = get_codepage_id (source_codepage >= 0 ? source_codepage : display_codepage);
    if (cp_id != NULL)
    {
        GIConv conv;
        conv = str_crt_conv_from (cp_id);
        if (conv != INVALID_CONV)
        {
            if (view->converter != str_cnv_from_term)
                str_close_conv (view->converter);
            view->converter = conv;
        }
        view->utf8 = (gboolean) str_isutf8 (cp_id);
    }
#else
    (void) view;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_select_encoding (mcview_t * view)
{
#ifdef HAVE_CHARSET
    if (do_select_codepage ())
    {
        mcview_set_codeset (view);
    }
#else
    (void) view;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_show_error (mcview_t * view, const char *msg)
{
    mcview_close_datasource (view);
    if (mcview_is_in_panel (view))
    {
        mcview_set_datasource_string (view, msg);
    }
    else
    {
        message (D_ERROR, MSG_ERROR, "%s", msg);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** returns index of the first char in the line
 * it is constant for all line characters
 */

off_t
mcview_bol (mcview_t * view, off_t current, off_t limit)
{
    int c;
    off_t filesize;
    filesize = mcview_get_filesize (view);
    if (current <= 0)
        return 0;
    if (current > filesize)
        return filesize;
    if (!mcview_get_byte (view, current, &c))
        return current;
    if (c == '\n')
    {
        if (!mcview_get_byte (view, current - 1, &c))
            return current;
        if (c == '\r')
            current--;
    }
    while (current > 0 && current >= limit)
    {
        if (!mcview_get_byte (view, current - 1, &c))
            break;
        if (c == '\r' || c == '\n')
            break;
        current--;
    }
    return current;
}

/* --------------------------------------------------------------------------------------------- */
/** returns index of last char on line + width EOL
 * mcview_eol of the current line == mcview_bol next line
 */

off_t
mcview_eol (mcview_t * view, off_t current, off_t limit)
{
    int c, prev_ch = 0;
    off_t filesize;
    filesize = mcview_get_filesize (view);
    if (current < 0)
        return 0;
    if (current >= filesize)
        return filesize;
    while (current < filesize && current < limit)
    {
        if (!mcview_get_byte (view, current, &c))
            break;
        if (c == '\n')
        {
            current++;
            break;
        }
        else if (prev_ch == '\r')
        {
            break;
        }
        current++;
        prev_ch = c;
    }
    return current;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_title (const Dlg_head * h, size_t len)
{
    const mcview_t *view = (const mcview_t *) find_widget_type (h, mcview_callback);
    const char *modified = view->hexedit_mode && (view->change_list != NULL) ? "(*) " : "    ";
    const char *file_label;

    len -= 4;

    file_label = view->filename != NULL ? view->filename :
        view->command != NULL ? view->command : "";
    file_label = str_term_trim (file_label, len - str_term_width1 (_("View: ")));

    return g_strconcat (_("View: "), modified, file_label, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_lock_file (mcview_t * view)
{
    char *fullpath;
    gboolean ret;

    fullpath = g_build_filename (view->workdir, view->filename, (char *) NULL);
    ret = lock_file (fullpath);
    g_free (fullpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_unlock_file (mcview_t * view)
{
    char *fullpath;
    gboolean ret;

    fullpath = g_build_filename (view->workdir, view->filename, (char *) NULL);
    ret = unlock_file (fullpath);
    g_free (fullpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
