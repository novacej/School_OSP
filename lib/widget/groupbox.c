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

/** \file groupbox.c
 *  \brief Source: WGroupbox widget
 */

#include <config.h>

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
groupbox_callback (Widget * w, widget_msg_t msg, int parm)
{
    WGroupbox *g = (WGroupbox *) w;

    switch (msg)
    {
    case WIDGET_INIT:
        return MSG_HANDLED;

    case WIDGET_FOCUS:
        return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
        {
            gboolean disabled = (w->options & W_DISABLED) != 0;
            tty_setcolor (disabled ? DISABLED_COLOR : COLOR_NORMAL);
            draw_box (g->widget.owner, g->widget.y - g->widget.owner->y,
                      g->widget.x - g->widget.owner->x, g->widget.lines, g->widget.cols, TRUE);

            if (g->title != NULL)
            {
                tty_setcolor (disabled ? DISABLED_COLOR : COLOR_TITLE);
                dlg_move (g->widget.owner, g->widget.y - g->widget.owner->y,
                          g->widget.x - g->widget.owner->x + 1);
                tty_print_string (g->title);
            }
            return MSG_HANDLED;
        }

    case WIDGET_DESTROY:
        g_free (g->title);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WGroupbox *
groupbox_new (int y, int x, int height, int width, const char *title)
{
    WGroupbox *g;

    g = g_new (WGroupbox, 1);
    init_widget (&g->widget, y, x, height, width, groupbox_callback, NULL);

    widget_want_cursor (g->widget, FALSE);
    widget_want_hotkey (g->widget, FALSE);

    /* Strip existing spaces, add one space before and after the title */
    if (title != NULL)
    {
        char *t;

        t = g_strstrip (g_strdup (title));
        g->title = g_strconcat (" ", t, " ", (char *) NULL);
        g_free (t);
    }

    return g;
}

/* --------------------------------------------------------------------------------------------- */
