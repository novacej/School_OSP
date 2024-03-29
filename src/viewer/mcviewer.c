/*
   Internal file viewer for the Midnight Commander
   Interface functions

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
#include <errno.h>
#include <fcntl.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* load_file_position() */
#include "lib/widget.h"
#include "lib/charsets.h"

#include "src/main.h"

#include "src/filemanager/layout.h"     /* menubar_visible */
#include "src/filemanager/midnight.h"   /* the_menubar */

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

int mcview_default_hex_mode = 0;
int mcview_default_nroff_flag = 0;
int mcview_global_wrap_mode = 1;
int mcview_default_magic_flag = 1;

int mcview_altered_hex_mode = 0;
int mcview_altered_magic_flag = 0;
int mcview_altered_nroff_flag = 0;

int mcview_remember_file_position = FALSE;

/* Maxlimit for skipping updates */
int mcview_max_dirt_limit = 10;

/* Scrolling is done in pages or line increments */
int mcview_mouse_move_pages = 1;

/* end of file will be showen from mcview_show_eof */
char *mcview_show_eof = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Both views */

static int
mcview_event (mcview_t * view, Gpm_Event * event, int *result)
{
    screen_dimen y, x;

    *result = MOU_NORMAL;

    /* rest of the upper frame, the menu is invisible - call menu */
    if (mcview_is_in_panel (view) && (event->type & GPM_DOWN) && event->y == 1 && !menubar_visible)
    {
        event->x += view->widget.x;
        *result = the_menubar->widget.mouse (event, the_menubar);
        return 0;               /* don't draw viewer over menu */
    }

    /* We are not interested in the release events */
    if (!(event->type & (GPM_DOWN | GPM_DRAG)))
        return 0;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN))
    {
        mcview_move_up (view, 2);
        return 1;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN))
    {
        mcview_move_down (view, 2);
        return 1;
    }

    x = event->x;
    y = event->y;

    /* Scrolling left and right */
    if (!view->text_wrap_mode)
    {
        if (x < view->data_area.width * 1 / 4)
        {
            mcview_move_left (view, 1);
            goto processed;
        }
        else if (x < view->data_area.width * 3 / 4)
        {
            /* ignore the click */
        }
        else
        {
            mcview_move_right (view, 1);
            goto processed;
        }
    }

    /* Scrolling up and down */
    if (y < view->data_area.top + view->data_area.height * 1 / 3)
    {
        if (mcview_mouse_move_pages)
            mcview_move_up (view, view->data_area.height / 2);
        else
            mcview_move_up (view, 1);
        goto processed;
    }
    else if (y < view->data_area.top + view->data_area.height * 2 / 3)
    {
        /* ignore the click */
    }
    else
    {
        if (mcview_mouse_move_pages)
            mcview_move_down (view, view->data_area.height / 2);
        else
            mcview_move_down (view, 1);
        goto processed;
    }

    return 0;

  processed:
    *result = MOU_REPEAT;
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/** Real view only */

static int
mcview_real_event (Gpm_Event * event, void *x)
{
    mcview_t *view = (mcview_t *) x;
    int result;

    if (mcview_event (view, event, &result))
        mcview_update (view);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_set_keymap (mcview_t * view)
{
    view->plain_map = default_viewer_keymap;
    if (viewer_keymap && viewer_keymap->len > 0)
        view->plain_map = (global_keymap_t *) viewer_keymap->data;

    view->hex_map = default_viewer_hex_keymap;
    if (viewer_hex_keymap && viewer_hex_keymap->len > 0)
        view->hex_map = (global_keymap_t *) viewer_hex_keymap->data;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mcview_t *
mcview_new (int y, int x, int lines, int cols, gboolean is_panel)
{
    mcview_t *view = g_new0 (mcview_t, 1);

    init_widget (&view->widget, y, x, lines, cols, mcview_callback, mcview_real_event);

    mcview_set_keymap (view);

    view->hex_mode = FALSE;
    view->hexedit_mode = FALSE;
    view->locked = FALSE;
    view->hexview_in_text = FALSE;
    view->text_nroff_mode = FALSE;
    view->text_wrap_mode = FALSE;
    view->magic_mode = FALSE;

    view->dpy_frame_size = is_panel ? 1 : 0;
    view->converter = str_cnv_from_term;

    mcview_init (view);

    if (mcview_default_hex_mode)
        mcview_toggle_hex_mode (view);
    if (mcview_default_nroff_flag)
        mcview_toggle_nroff_mode (view);
    if (mcview_global_wrap_mode)
        mcview_toggle_wrap_mode (view);
    if (mcview_default_magic_flag)
        mcview_toggle_magic_mode (view);

    return view;
}

/* --------------------------------------------------------------------------------------------- */
/** Real view only */

mcview_ret_t
mcview_viewer (const char *command, const char *file, int start_line)
{
    gboolean succeeded;
    mcview_t *lc_mcview;
    Dlg_head *view_dlg;
    mcview_ret_t ret;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg = create_dlg (FALSE, 0, 0, LINES, COLS, NULL, mcview_dialog_callback,
                           "[Internal File Viewer]", NULL, DLG_WANT_TAB);

    lc_mcview = mcview_new (0, 0, LINES - 1, COLS, FALSE);
    add_widget (view_dlg, lc_mcview);

    add_widget (view_dlg, buttonbar_new (TRUE));

    view_dlg->get_title = mcview_get_title;

    succeeded = mcview_load (lc_mcview, command, file, start_line);

    if (succeeded)
    {
        run_dlg (view_dlg);

        ret = lc_mcview->move_dir == 0 ? MCVIEW_EXIT_OK :
            lc_mcview->move_dir > 0 ? MCVIEW_WANT_NEXT : MCVIEW_WANT_PREV;
    }
    else
    {
        view_dlg->state = DLG_CLOSED;
        ret = MCVIEW_EXIT_FAILURE;
    }

    if (view_dlg->state == DLG_CLOSED)
        destroy_dlg (view_dlg);

    return ret;
}

/* {{{ Miscellaneous functions }}} */

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_load (mcview_t * view, const char *command, const char *file, int start_line)
{
    gboolean retval = FALSE;

    assert (view->bytes_per_line != 0);

    view->filename = g_strdup (file);

    if ((view->workdir == NULL) && (file != NULL))
    {
        if (!g_path_is_absolute (file))
            view->workdir = g_strdup (vfs_get_current_dir ());
        else
        {
            /* try extract path form filename */
            char *dirname;

            dirname = g_path_get_dirname (file);
            if (strcmp (dirname, ".") != 0)
                view->workdir = dirname;
            else
            {
                g_free (dirname);
                view->workdir = g_strdup (vfs_get_current_dir ());
            }
        }
    }

    if (!mcview_is_in_panel (view))
        view->dpy_text_column = 0;

    mcview_set_codeset (view);

    if (command != NULL && (view->magic_mode || file == NULL || file[0] == '\0'))
        retval = mcview_load_command_output (view, command);
    else if (file != NULL && file[0] != '\0')
    {
        int fd = -1;
        char tmp[BUF_MEDIUM];
        struct stat st;

        /* Open the file */
        fd = mc_open (file, O_RDONLY | O_NONBLOCK);
        if (fd == -1)
        {
            g_snprintf (tmp, sizeof (tmp), _("Cannot open \"%s\"\n%s"),
                        file, unix_error_string (errno));
            mcview_show_error (view, tmp);
            g_free (view->filename);
            view->filename = NULL;
            g_free (view->workdir);
            view->workdir = NULL;
            goto finish;
        }

        /* Make sure we are working with a regular file */
        if (mc_fstat (fd, &st) == -1)
        {
            mc_close (fd);
            g_snprintf (tmp, sizeof (tmp), _("Cannot stat \"%s\"\n%s"),
                        file, unix_error_string (errno));
            mcview_show_error (view, tmp);
            g_free (view->filename);
            view->filename = NULL;
            g_free (view->workdir);
            view->workdir = NULL;
            goto finish;
        }

        if (!S_ISREG (st.st_mode))
        {
            mc_close (fd);
            mcview_show_error (view, _("Cannot view: not a regular file"));
            g_free (view->filename);
            view->filename = NULL;
            g_free (view->workdir);
            view->workdir = NULL;
            goto finish;
        }

        if (st.st_size == 0 || mc_lseek (fd, 0, SEEK_SET) == -1)
        {
            /* Must be one of those nice files that grow (/proc) */
            mcview_set_datasource_vfs_pipe (view, fd);
        }
        else
        {
            int type;

            type = get_compression_type (fd, file);

            if (view->magic_mode && (type != COMPRESSION_NONE))
            {
                g_free (view->filename);
                view->filename = g_strconcat (file, decompress_extension (type), (char *) NULL);
            }
            mcview_set_datasource_file (view, fd, &st);
        }
        retval = TRUE;
    }

  finish:
    view->command = g_strdup (command);
    view->dpy_start = 0;
    view->search_start = 0;
    view->search_end = 0;
    view->dpy_text_column = 0;

    mcview_compute_areas (view);
    mcview_update_bytes_per_line (view);

    if (mcview_remember_file_position && view->filename != NULL && start_line == 0)
    {
        char *canon_fname;
        long line, col;
        off_t new_offset;

        canon_fname = vfs_canon (view->filename);
        load_file_position (canon_fname, &line, &col, &new_offset, &view->saved_bookmarks);
        new_offset = min (new_offset, mcview_get_filesize (view));
        view->dpy_start = mcview_bol (view, new_offset, 0);
        g_free (canon_fname);
    }
    else if (start_line > 0)
        mcview_moveto (view, start_line - 1, 0);

    view->hexedit_lownibble = FALSE;
    view->hexview_in_text = FALSE;
    view->change_list = NULL;
    return retval;
}

/* --------------------------------------------------------------------------------------------- */
