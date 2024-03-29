/* Widgets for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2010  Free Software Foundation, Inc.

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

/** \file hline.c
 *  \brief Source: WHLine widget (horizontal line)
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/skin.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
hline_callback (Widget * w, widget_msg_t msg, int parm)
{
    WHLine *l = (WHLine *) w;
    Dlg_head *h = l->widget.owner;

    switch (msg)
    {
    case WIDGET_INIT:
    case WIDGET_RESIZED:
        if (l->auto_adjust_cols)
        {
            if (((w->owner->flags & DLG_COMPACT) != 0))
            {
                w->x = w->owner->x;
                w->cols = w->owner->cols;
            }
            else
            {
                w->x = w->owner->x + 1;
                w->cols = w->owner->cols - 2;
            }
        }

    case WIDGET_FOCUS:
        /* We don't want to get the focus */
        return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
        if (l->transparent)
            tty_setcolor (DEFAULT_COLOR);
        else
            tty_setcolor (h->color[DLG_COLOR_NORMAL]);

        tty_draw_hline (w->y, w->x + 1, ACS_HLINE, w->cols - 2);

        if (l->auto_adjust_cols)
        {
            widget_move (w, 0, 0);
            tty_print_alt_char (ACS_LTEE, FALSE);
            widget_move (w, 0, w->cols - 1);
            tty_print_alt_char (ACS_RTEE, FALSE);
        }
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WHLine *
hline_new (int y, int x, int width)
{
    WHLine *l;
    int lines = 1;

    l = g_new (WHLine, 1);
    init_widget (&l->widget, y, x, lines, width, hline_callback, NULL);
    l->auto_adjust_cols = (width < 0);
    l->transparent = FALSE;
    widget_want_cursor (l->widget, FALSE);
    widget_want_hotkey (l->widget, FALSE);

    return l;
}

/* --------------------------------------------------------------------------------------------- */
