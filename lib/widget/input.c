/* Widgets for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2010 Free Software Foundation, Inc.

   Authors: 1994, 1995 Radek Doulik
   1994, 1995 Miguel de Icaza
   1995 Jakub Jelinek
   1996 Andrej Borsenkow
   1997 Norbert Warmuth
   2009, 2010 Andrew Borodin

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

 */

/** \file input.c
 *  \brief Source: WInput widget
 */

#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/widget.h"

#include "src/main.h"           /* home_dir */
#include "src/filemanager/midnight.h"   /* current_panel */
#include "src/clipboard.h"      /* copy_file_to_ext_clip, paste_to_file_from_ext_clip */
#include "src/keybind-defaults.h"       /* input_map */

/*** global variables ****************************************************************************/

int quote = 0;

/*** file scope macro definitions ****************************************************************/

#define LARGE_HISTORY_BUTTON 1

#ifdef LARGE_HISTORY_BUTTON
#define HISTORY_BUTTON_WIDTH 3
#else
#define HISTORY_BUTTON_WIDTH 1
#endif

#define should_show_history_button(in) \
    (in->history != NULL && in->field_width > HISTORY_BUTTON_WIDTH * 2 + 1 \
         && in->widget.owner != NULL)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* Input widgets have a global kill ring */
/* Pointer to killed data */
static char *kill_buffer = NULL;

/*** file scope functions ************************************************************************/

static gboolean
save_text_to_clip_file (const char *text)
{
    int file;
    char *fname = NULL;
    ssize_t ret;
    size_t str_len;

    fname = g_build_filename (mc_config_get_cache_path (), EDIT_CLIP_FILE, NULL);
    file = mc_open (fname, O_CREAT | O_WRONLY | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_BINARY);
    g_free (fname);

    if (file == -1)
        return FALSE;

    str_len = strlen (text);
    ret = mc_write (file, (char *) text, str_len);
    mc_close (file);
    return ret == (ssize_t) str_len;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
load_text_from_clip_file (char **text)
{
    char buf[BUF_LARGE];
    FILE *f;
    char *fname = NULL;
    gboolean first = TRUE;

    fname = g_build_filename (mc_config_get_cache_path (), EDIT_CLIP_FILE, NULL);
    f = fopen (fname, "r");
    g_free (fname);

    if (f == NULL)
        return FALSE;

    *text = NULL;

    while (fgets (buf, sizeof (buf), f))
    {
        size_t len;

        len = strlen (buf);
        if (len > 0)
        {
            if (buf[len - 1] == '\n')
                buf[len - 1] = '\0';

            if (first)
            {
                first = FALSE;
                *text = g_strdup (buf);
            }
            else
            {
                /* remove \n on EOL */
                char *tmp;

                tmp = g_strconcat (*text, " ", buf, (char *) NULL);
                g_free (*text);
                *text = tmp;
            }
        }
    }

    fclose (f);

    return (*text != NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
panel_save_curent_file_to_clip_file (void)
{
    gboolean res = FALSE;

    if (current_panel->marked == 0)
        res = save_text_to_clip_file (selection (current_panel)->fname);
    else
    {
        int i;
        gboolean first = TRUE;
        char *flist = NULL;

        for (i = 0; i < current_panel->count; i++)
            if (current_panel->dir.list[i].f.marked != 0)
            {                   /* Skip the unmarked ones */
                if (first)
                {
                    flist = g_strdup (current_panel->dir.list[i].fname);
                    first = FALSE;
                }
                else
                {
                    /* Add empty lines after the file */
                    char *tmp;

                    tmp =
                        g_strconcat (flist, "\n", current_panel->dir.list[i].fname, (char *) NULL);
                    g_free (flist);
                    flist = tmp;
                }
            }

        if (flist != NULL)
        {
            res = save_text_to_clip_file (flist);
            g_free (flist);
        }
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_history_button (WInput * in)
{
    char c;
    gboolean disabled = (((Widget *) in)->options & W_DISABLED) != 0;

    c = in->history->next ? (in->history->prev ? '|' : 'v') : '^';
    widget_move (&in->widget, 0, in->field_width - HISTORY_BUTTON_WIDTH);
    tty_setcolor (disabled ? DISABLED_COLOR : in->color[WINPUTC_HISTORY]);
#ifdef LARGE_HISTORY_BUTTON
    {
        Dlg_head *h;
        h = in->widget.owner;
        tty_print_string ("[ ]");
        widget_move (&in->widget, 0, in->field_width - HISTORY_BUTTON_WIDTH + 1);
    }
#endif
    tty_print_char (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
input_set_markers (WInput * in, long m1)
{
    in->mark = m1;
}

/* --------------------------------------------------------------------------------------------- */

static void
input_mark_cmd (WInput * in, gboolean mark)
{
    if (mark == 0)
    {
        in->highlight = FALSE;
        input_set_markers (in, 0);
    }
    else
    {
        in->highlight = TRUE;
        input_set_markers (in, in->point);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
input_eval_marks (WInput * in, long *start_mark, long *end_mark)
{
    if (in->highlight)
    {
        *start_mark = min (in->mark, in->point);
        *end_mark = max (in->mark, in->point);
        return TRUE;
    }
    else
    {
        *start_mark = *end_mark = 0;
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
delete_region (WInput * in, int x_first, int x_last)
{
    int first = min (x_first, x_last);
    int last = max (x_first, x_last);
    size_t len;

    input_mark_cmd (in, FALSE);
    in->point = first;
    last = str_offset_to_pos (in->buffer, last);
    first = str_offset_to_pos (in->buffer, first);
    len = strlen (&in->buffer[last]) + 1;
    memmove (&in->buffer[first], &in->buffer[last], len);
    in->charpoint = 0;
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
do_show_hist (WInput * in)
{
    char *r;

    r = history_show (&in->history, &in->widget);
    if (r != NULL)
    {
        input_assign_text (in, r);
        g_free (r);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
push_history (WInput * in, const char *text)
{
    /* input widget where urls with passwords are entered without any
       vfs prefix */
    const char *password_input_fields[] = {
        " Link to a remote machine ",
        " FTP to machine ",
        " SMB link to machine "
    };
    const size_t ELEMENTS = (sizeof (password_input_fields) / sizeof (password_input_fields[0]));

    char *t;
    size_t i;
    gboolean empty;

    if (text == NULL)
        return;

#ifdef ENABLE_NLS
    for (i = 0; i < ELEMENTS; i++)
        password_input_fields[i] = _(password_input_fields[i]);
#endif

    t = g_strstrip (g_strdup (text));
    empty = *t == '\0';
    g_free (t);
    t = g_strdup (empty ? "" : text);

    if (in->history_name != NULL)
    {
        /* FIXME: It is the strange code. Rewrite is needed. */

        const char *p = in->history_name + 3;

        for (i = 0; i < ELEMENTS; i++)
            if (strcmp (p, password_input_fields[i]) == 0)
                break;

        strip_password (t, i >= ELEMENTS);
    }

    in->history = list_append_unique (in->history, t);
    in->need_push = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_buffer_backward (WInput * in, int start, int end)
{
    int i, pos, len;
    int str_len;

    str_len = str_length (in->buffer);
    if (start >= str_len || end > str_len + 1)
        return;

    pos = str_offset_to_pos (in->buffer, start);
    len = str_offset_to_pos (in->buffer, end) - pos;

    for (i = pos; in->buffer[i + len - 1]; i++)
        in->buffer[i] = in->buffer[i + len];
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
insert_char (WInput * in, int c_code)
{
    size_t i;
    int res;

    if (in->highlight)
    {
        long m1, m2;
        if (input_eval_marks (in, &m1, &m2))
            delete_region (in, m1, m2);
    }
    if (c_code == -1)
        return MSG_NOT_HANDLED;

    if (in->charpoint >= MB_LEN_MAX)
        return MSG_HANDLED;

    in->charbuf[in->charpoint] = c_code;
    in->charpoint++;

    res = str_is_valid_char (in->charbuf, in->charpoint);
    if (res < 0)
    {
        if (res != -2)
            in->charpoint = 0;  /* broken multibyte char, skip */
        return MSG_HANDLED;
    }

    in->need_push = TRUE;
    if (strlen (in->buffer) + 1 + in->charpoint >= in->current_max_size)
    {
        /* Expand the buffer */
        size_t new_length = in->current_max_size + in->field_width + in->charpoint;
        char *narea = g_try_renew (char, in->buffer, new_length);
        if (narea)
        {
            in->buffer = narea;
            in->current_max_size = new_length;
        }
    }

    if (strlen (in->buffer) + in->charpoint < in->current_max_size)
    {
        /* bytes from begin */
        size_t ins_point = str_offset_to_pos (in->buffer, in->point);
        /* move chars */
        size_t rest_bytes = strlen (in->buffer + ins_point);

        for (i = rest_bytes + 1; i > 0; i--)
            in->buffer[ins_point + i + in->charpoint - 1] = in->buffer[ins_point + i - 1];

        memcpy (in->buffer + ins_point, in->charbuf, in->charpoint);
        in->point++;
    }

    in->charpoint = 0;
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
beginning_of_line (WInput * in)
{
    in->point = 0;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
end_of_line (WInput * in)
{
    in->point = str_length (in->buffer);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_char (WInput * in)
{
    const char *act;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    if (in->point > 0)
        in->point -= str_cprev_noncomb_char (&act, in->buffer);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
forward_char (WInput * in)
{
    const char *act;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    if (act[0] != '\0')
        in->point += str_cnext_noncomb_char (&act);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
forward_word (WInput * in)
{
    const char *p;

    p = in->buffer + str_offset_to_pos (in->buffer, in->point);
    while (p[0] != '\0' && (str_isspace (p) || str_ispunct (p)))
    {
        str_cnext_char (&p);
        in->point++;
    }
    while (p[0] != '\0' && !str_isspace (p) && !str_ispunct (p))
    {
        str_cnext_char (&p);
        in->point++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_word (WInput * in)
{
    const char *p, *p_tmp;

    for (p = in->buffer + str_offset_to_pos (in->buffer, in->point);
         (p != in->buffer) && (p[0] == '\0'); str_cprev_char (&p), in->point--);

    while (p != in->buffer)
    {
        p_tmp = p;
        str_cprev_char (&p);
        if (!str_isspace (p) && !str_ispunct (p))
        {
            p = p_tmp;
            break;
        }
        in->point--;
    }
    while (p != in->buffer)
    {
        str_cprev_char (&p);
        if (str_isspace (p) || str_ispunct (p))
            break;

        in->point--;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_delete (WInput * in)
{
    const char *act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    int start;

    if (in->point == 0)
        return;

    start = in->point - str_cprev_noncomb_char (&act, in->buffer);
    move_buffer_backward (in, start, in->point);
    in->charpoint = 0;
    in->need_push = TRUE;
    in->point = start;
}

/* --------------------------------------------------------------------------------------------- */

static void
delete_char (WInput * in)
{
    const char *act;
    int end = in->point;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    end += str_cnext_noncomb_char (&act);

    move_buffer_backward (in, in->point, end);
    in->charpoint = 0;
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_region (WInput * in, int x_first, int x_last)
{
    int first = min (x_first, x_last);
    int last = max (x_first, x_last);

    if (last == first)
    {
        /* Copy selected files to clipboard */
        panel_save_curent_file_to_clip_file ();
        /* try use external clipboard utility */
        copy_file_to_ext_clip ();
        return;
    }

    g_free (kill_buffer);

    first = str_offset_to_pos (in->buffer, first);
    last = str_offset_to_pos (in->buffer, last);

    kill_buffer = g_strndup (in->buffer + first, last - first);

    save_text_to_clip_file (kill_buffer);
    /* try use external clipboard utility */
    copy_file_to_ext_clip ();
}

/* --------------------------------------------------------------------------------------------- */

static void
kill_word (WInput * in)
{
    int old_point = in->point;
    int new_point;

    forward_word (in);
    new_point = in->point;
    in->point = old_point;

    delete_region (in, old_point, new_point);
    in->need_push = TRUE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
back_kill_word (WInput * in)
{
    int old_point = in->point;
    int new_point;

    backward_word (in);
    new_point = in->point;
    in->point = old_point;

    delete_region (in, old_point, new_point);
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
yank (WInput * in)
{
    if (kill_buffer != NULL)
    {
        char *p;

        in->charpoint = 0;
        for (p = kill_buffer; *p != '\0'; p++)
            insert_char (in, *p);
        in->charpoint = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
kill_line (WInput * in)
{
    int chp;

    chp = str_offset_to_pos (in->buffer, in->point);
    g_free (kill_buffer);
    kill_buffer = g_strdup (&in->buffer[chp]);
    in->buffer[chp] = '\0';
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
clear_line (WInput * in)
{
    in->need_push = 1;
    in->buffer[0] = '\0';
    in->point = 0;
    in->mark = 0;
    in->highlight = FALSE;
    in->charpoint = 0;
}

static void
ins_from_clip (WInput * in)
{
    char *p = NULL;

    /* try use external clipboard utility */
    paste_to_file_from_ext_clip ();

    if (load_text_from_clip_file (&p))
    {
        char *pp;

        for (pp = p; *pp != '\0'; pp++)
            insert_char (in, *pp);

        g_free (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
hist_prev (WInput * in)
{
    GList *prev;

    if (in->history == NULL)
        return;

    if (in->need_push)
        push_history (in, in->buffer);

    prev = g_list_previous (in->history);
    if (prev != NULL)
    {
        in->history = prev;
        input_assign_text (in, (char *) prev->data);
        in->need_push = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
hist_next (WInput * in)
{
    if (in->need_push)
    {
        push_history (in, in->buffer);
        input_assign_text (in, "");
        return;
    }

    if (in->history == NULL)
        return;

    if (in->history->next == NULL)
        input_assign_text (in, "");
    else
    {
        in->history = g_list_next (in->history);
        input_assign_text (in, (char *) in->history->data);
        in->need_push = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
port_region_marked_for_delete (WInput * in)
{
    in->buffer[0] = '\0';
    in->point = 0;
    in->first = FALSE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
input_execute_cmd (WInput * in, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    /* a highlight command like shift-arrow */
    if (command == CK_InputLeftHighlight ||
        command == CK_InputRightHighlight ||
        command == CK_InputWordLeftHighlight ||
        command == CK_InputWordRightHighlight ||
        command == CK_InputBolHighlight || command == CK_InputEolHighlight)
    {
        if (!in->highlight)
        {
            input_mark_cmd (in, FALSE); /* clear */
            input_mark_cmd (in, TRUE);  /* marking on */
        }
    }

    switch (command)
    {
    case CK_InputForwardWord:
    case CK_InputBackwardWord:
    case CK_InputForwardChar:
    case CK_InputBackwardChar:
        if (in->highlight)
            input_mark_cmd (in, FALSE);
    }

    switch (command)
    {
    case CK_InputBol:
    case CK_InputBolHighlight:
        beginning_of_line (in);
        break;
    case CK_InputEol:
    case CK_InputEolHighlight:
        end_of_line (in);
        break;
    case CK_InputMoveLeft:
    case CK_InputLeftHighlight:
        backward_char (in);
        break;
    case CK_InputWordLeft:
    case CK_InputWordLeftHighlight:
        backward_word (in);
        break;
    case CK_InputMoveRight:
    case CK_InputRightHighlight:
        forward_char (in);
        break;
    case CK_InputWordRight:
    case CK_InputWordRightHighlight:
        forward_word (in);
        break;
    case CK_InputBackwardChar:
        backward_char (in);
        break;
    case CK_InputBackwardWord:
        backward_word (in);
        break;
    case CK_InputForwardChar:
        forward_char (in);
        break;
    case CK_InputForwardWord:
        forward_word (in);
        break;
    case CK_InputBackwardDelete:
        if (in->highlight)
        {
            long m1, m2;
            if (input_eval_marks (in, &m1, &m2))
                delete_region (in, m1, m2);
        }
        else
            backward_delete (in);
        break;
    case CK_InputDeleteChar:
        if (in->first)
            port_region_marked_for_delete (in);
        else if (in->highlight)
        {
            long m1, m2;
            if (input_eval_marks (in, &m1, &m2))
                delete_region (in, m1, m2);
        }
        else
            delete_char (in);
        break;
    case CK_InputKillWord:
        kill_word (in);
        break;
    case CK_InputBackwardKillWord:
        back_kill_word (in);
        break;
    case CK_InputSetMark:
        input_mark_cmd (in, TRUE);
        break;
    case CK_InputKillRegion:
        delete_region (in, in->point, in->mark);
        break;
    case CK_InputKillLine:
        /* clear command line from cursor to the EOL */
        kill_line (in);
        break;
    case CK_InputClearLine:
        /* clear command line */
        clear_line (in);
        break;
    case CK_InputCopyRegion:
        copy_region (in, in->mark, in->point);
        break;
    case CK_InputKillSave:
        copy_region (in, in->mark, in->point);
        delete_region (in, in->point, in->mark);
        break;
    case CK_InputYank:
        yank (in);
        break;
    case CK_InputPaste:
        ins_from_clip (in);
        break;
    case CK_InputHistoryPrev:
        hist_prev (in);
        break;
    case CK_InputHistoryNext:
        hist_next (in);
        break;
    case CK_InputHistoryShow:
        do_show_hist (in);
        break;
    case CK_InputComplete:
        complete (in);
        break;
    default:
        res = MSG_NOT_HANDLED;
    }

    if (command != CK_InputLeftHighlight &&
        command != CK_InputRightHighlight &&
        command != CK_InputWordLeftHighlight &&
        command != CK_InputWordRightHighlight &&
        command != CK_InputBolHighlight && command != CK_InputEolHighlight)
        in->highlight = FALSE;

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
input_destroy (WInput * in)
{
    if (in == NULL)
    {
        fprintf (stderr, "Internal error: null Input *\n");
        exit (EXIT_FAILURE);
    }

    input_clean (in);

    if (in->history != NULL)
    {
        if (!in->is_password && (((Widget *) in)->owner->ret_value != B_CANCEL))
            history_put (in->history_name, in->history);

        in->history = g_list_first (in->history);
        g_list_foreach (in->history, (GFunc) g_free, NULL);
        g_list_free (in->history);
    }

    g_free (in->buffer);
    input_free_completions (in);
    g_free (in->history_name);

    g_free (kill_buffer);
    kill_buffer = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
input_event (Gpm_Event * event, void *data)
{
    WInput *in = (WInput *) data;

    if ((event->type & GPM_DOWN) != 0)
    {
        in->first = FALSE;
        input_mark_cmd (in, FALSE);
    }

    if ((event->type & (GPM_DOWN | GPM_DRAG)) != 0)
    {
        dlg_select_widget (in);

        if (event->x >= in->field_width - HISTORY_BUTTON_WIDTH + 1
            && should_show_history_button (in))
            do_show_hist (in);
        else
        {
            in->point = str_length (in->buffer);
            if (event->x + in->term_first_shown - 1 < str_term_width1 (in->buffer))
                in->point = str_column_to_pos (in->buffer, event->x + in->term_first_shown - 1);
        }
        input_update (in, TRUE);
    }
    /* A lone up mustn't do anything */
    if (in->highlight && (event->type & (GPM_UP | GPM_DRAG)) != 0)
        return MOU_NORMAL;

    if ((event->type & GPM_DRAG) == 0)
        input_mark_cmd (in, TRUE);

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Create new instance of WInput object.
  * @param y                    Y coordinate
  * @param x                    X coordinate
  * @param input_colors         Array of used colors
  * @param width                Widget width
  * @param def_text             Default text filled in widget
  * @param histname             Name of history
  * @param completion_flags     Flags for specify type of completions
  * @returns                    WInput object
  */
WInput *
input_new (int y, int x, const int *input_colors, int width, const char *def_text,
           const char *histname, input_complete_t completion_flags)
{
    WInput *in = g_new (WInput, 1);
    size_t initial_buffer_len;

    init_widget (&in->widget, y, x, 1, width, input_callback, input_event);

    /* history setup */
    in->history_name = NULL;
    in->history = NULL;
    if ((histname != NULL) && (*histname != '\0'))
    {
        in->history_name = g_strdup (histname);
        in->history = history_get (histname);
    }

    if (def_text == NULL)
        def_text = "";
    else if (def_text == INPUT_LAST_TEXT)
    {
        if ((in->history != NULL) && (in->history->data != NULL))
            def_text = (char *) in->history->data;
        else
            def_text = "";
    }

    initial_buffer_len = strlen (def_text);
    initial_buffer_len = 1 + max ((size_t) width, initial_buffer_len);
    in->widget.options |= W_IS_INPUT;
    in->completions = NULL;
    in->completion_flags = completion_flags;
    in->current_max_size = initial_buffer_len;
    in->buffer = g_new (char, initial_buffer_len);

    memmove (in->color, input_colors, sizeof (input_colors_t));

    in->field_width = width;
    in->first = TRUE;
    in->highlight = FALSE;
    in->term_first_shown = 0;
    in->disable_update = 0;
    in->mark = 0;
    in->need_push = TRUE;
    in->is_password = FALSE;

    strcpy (in->buffer, def_text);
    in->point = str_length (in->buffer);
    in->charpoint = 0;

    return in;
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
input_callback (Widget * w, widget_msg_t msg, int parm)
{
    WInput *in = (WInput *) w;
    cb_ret_t v;

    switch (msg)
    {
    case WIDGET_KEY:
        if (parm == XCTRL ('q'))
        {
            quote = 1;
            v = input_handle_char (in, ascii_alpha_to_cntrl (tty_getch ()));
            quote = 0;
            return v;
        }

        /* Keys we want others to handle */
        if (parm == KEY_UP || parm == KEY_DOWN || parm == ESC_CHAR
            || parm == KEY_F (10) || parm == '\n')
            return MSG_NOT_HANDLED;

        /* When pasting multiline text, insert literal Enter */
        if ((parm & ~KEY_M_MASK) == '\n')
        {
            quote = 1;
            v = input_handle_char (in, '\n');
            quote = 0;
            return v;
        }

        return input_handle_char (in, parm);

    case WIDGET_COMMAND:
        return input_execute_cmd (in, parm);

    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
        input_update (in, FALSE);
        return MSG_HANDLED;

    case WIDGET_CURSOR:
        widget_move (&in->widget, 0, str_term_width2 (in->buffer, in->point)
                     - in->term_first_shown);
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        input_destroy (in);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

/** Get default colors for WInput widget.
  * @returns default colors
  */
const int *
input_get_default_colors (void)
{
    static input_colors_t standart_colors;

    standart_colors[WINPUTC_MAIN] = INPUT_COLOR;
    standart_colors[WINPUTC_MARK] = INPUT_MARK_COLOR;
    standart_colors[WINPUTC_UNCHANGED] = INPUT_UNCHANGED_COLOR;
    standart_colors[WINPUTC_HISTORY] = INPUT_HISTORY_COLOR;

    return standart_colors;
}

/* --------------------------------------------------------------------------------------------- */

void
input_set_origin (WInput * in, int x, int field_width)
{
    in->widget.x = x;
    in->field_width = in->widget.cols = field_width;
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
input_handle_char (WInput * in, int key)
{
    cb_ret_t v;
    unsigned long command;

    v = MSG_NOT_HANDLED;

    if (quote != 0)
    {
        input_free_completions (in);
        v = insert_char (in, key);
        input_update (in, TRUE);
        quote = 0;
        return v;
    }

    command = keybind_lookup_keymap_command (input_map, key);

    if (command == CK_Ignore_Key)
    {
        if (key > 255)
            return MSG_NOT_HANDLED;
        if (in->first)
            port_region_marked_for_delete (in);
        input_free_completions (in);
        v = insert_char (in, key);
    }
    else
    {
        if (command != CK_InputComplete)
            input_free_completions (in);
        input_execute_cmd (in, command);
        v = MSG_HANDLED;
        if (in->first)
            input_update (in, TRUE);    /* needed to clear in->first */
    }

    input_update (in, TRUE);
    return v;
}

/* --------------------------------------------------------------------------------------------- */

/* This function is a test for a special input key used in complete.c */
/* Returns 0 if it is not a special key, 1 if it is a non-complete key
   and 2 if it is a complete key */
int
input_key_is_in_map (WInput * in, int key)
{
    unsigned long command;

    (void) in;

    command = keybind_lookup_keymap_command (input_map, key);
    if (command == CK_Ignore_Key)
        return 0;

    return (command == CK_InputComplete) ? 2 : 1;
}

/* --------------------------------------------------------------------------------------------- */

void
input_assign_text (WInput * in, const char *text)
{
    input_free_completions (in);
    g_free (in->buffer);
    in->buffer = g_strdup (text);       /* was in->buffer->text */
    in->current_max_size = strlen (in->buffer) + 1;
    in->point = str_length (in->buffer);
    in->mark = 0;
    in->need_push = TRUE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Inserts text in input line */
void
input_insert (WInput * in, const char *text, gboolean insert_extra_space)
{
    input_disable_update (in);
    while (*text != '\0')
        input_handle_char (in, (unsigned char) *text++);        /* unsigned extension char->int */
    if (insert_extra_space)
        input_handle_char (in, ' ');
    input_enable_update (in);
    input_update (in, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_set_point (WInput * in, int pos)
{
    int max_pos;

    max_pos = str_length (in->buffer);
    pos = min (pos, max_pos);
    if (pos != in->point)
        input_free_completions (in);
    in->point = pos;
    in->charpoint = 0;
    input_update (in, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_update (WInput * in, gboolean clear_first)
{
    int has_history = 0;
    int i;
    int buf_len;
    const char *cp;
    int pw;

    if (should_show_history_button (in))
        has_history = HISTORY_BUTTON_WIDTH;

    if (in->disable_update != 0)
        return;

    buf_len = str_length (in->buffer);
    pw = str_term_width2 (in->buffer, in->point);

    /* Make the point visible */
    if ((pw < in->term_first_shown) || (pw >= in->term_first_shown + in->field_width - has_history))
    {
        in->term_first_shown = pw - (in->field_width / 3);
        if (in->term_first_shown < 0)
            in->term_first_shown = 0;
    }

    /* Adjust the mark */
    in->mark = min (in->mark, buf_len);

    if (has_history != 0)
        draw_history_button (in);

    if ((((Widget *) in)->options & W_DISABLED) != 0)
        tty_setcolor (DISABLED_COLOR);
    else if (in->first)
        tty_setcolor (in->color[WINPUTC_UNCHANGED]);
    else
        tty_setcolor (in->color[WINPUTC_MAIN]);

    widget_move (&in->widget, 0, 0);

    if (!in->is_password)
    {
        if (!in->highlight)
            tty_print_string (str_term_substring (in->buffer, in->term_first_shown,
                                                  in->field_width - has_history));
        else
        {
            long m1, m2;

            if (input_eval_marks (in, &m1, &m2))
            {
                tty_setcolor (in->color[WINPUTC_MAIN]);
                cp = str_term_substring (in->buffer, in->term_first_shown,
                                         in->field_width - has_history);
                tty_print_string (cp);
                tty_setcolor (in->color[WINPUTC_MARK]);
                if (m1 < in->term_first_shown)
                {
                    widget_move (&in->widget, 0, 0);
                    tty_print_string (str_term_substring
                                      (in->buffer, in->term_first_shown,
                                       m2 - in->term_first_shown));
                }
                else
                {
                    int sel_width;

                    widget_move (&in->widget, 0, m1 - in->term_first_shown);
                    sel_width =
                        min (m2 - m1,
                             (in->field_width - has_history) - (str_term_width2 (in->buffer, m1) -
                                                                in->term_first_shown));
                    tty_print_string (str_term_substring (in->buffer, m1, sel_width));
                }
            }
        }
    }
    else
    {
        cp = str_term_substring (in->buffer, in->term_first_shown, in->field_width - has_history);
        for (i = 0; i < in->field_width - has_history; i++)
        {
            if (i >= 0)
            {
                tty_setcolor (in->color[WINPUTC_MAIN]);
                tty_print_char ((cp[0] != '\0') ? '*' : ' ');
            }
            if (cp[0] != '\0')
                str_cnext_char (&cp);
        }
    }

    if (clear_first)
        in->first = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
input_enable_update (WInput * in)
{
    in->disable_update--;
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_disable_update (WInput * in)
{
    in->disable_update++;
}

/* --------------------------------------------------------------------------------------------- */

/* Cleans the input line and adds the current text to the history */
void
input_clean (WInput * in)
{
    push_history (in, in->buffer);
    in->need_push = TRUE;
    in->buffer[0] = '\0';
    in->point = 0;
    in->charpoint = 0;
    in->mark = 0;
    in->highlight = FALSE;
    input_free_completions (in);
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_free_completions (WInput * in)
{
    g_strfreev (in->completions);
    in->completions = NULL;
}

/* --------------------------------------------------------------------------------------------- */
