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

/** \file radio.c
 *  \brief Source: WRadui widget (radiobuttons)
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
radio_callback (Widget * w, widget_msg_t msg, int parm)
{
    WRadio *r = (WRadio *) w;
    int i;
    Dlg_head *h = r->widget.owner;

    switch (msg)
    {
    case WIDGET_HOTKEY:
        {
            for (i = 0; i < r->count; i++)
            {
                if (r->texts[i].hotkey != NULL)
                {
                    int c = g_ascii_tolower ((gchar) r->texts[i].hotkey[0]);

                    if (c != parm)
                        continue;
                    r->pos = i;

                    /* Take action */
                    radio_callback (w, WIDGET_KEY, ' ');
                    return MSG_HANDLED;
                }
            }
        }
        return MSG_NOT_HANDLED;

    case WIDGET_KEY:
        switch (parm)
        {
        case ' ':
            r->sel = r->pos;
            h->callback (h, w, DLG_ACTION, 0, NULL);
            radio_callback (w, WIDGET_FOCUS, ' ');
            return MSG_HANDLED;

        case KEY_UP:
        case KEY_LEFT:
            if (r->pos > 0)
            {
                r->pos--;
                return MSG_HANDLED;
            }
            return MSG_NOT_HANDLED;

        case KEY_DOWN:
        case KEY_RIGHT:
            if (r->count - 1 > r->pos)
            {
                r->pos++;
                return MSG_HANDLED;
            }
        }
        return MSG_NOT_HANDLED;

    case WIDGET_CURSOR:
        h->callback (h, w, DLG_ACTION, 0, NULL);
        radio_callback (w, WIDGET_FOCUS, ' ');
        widget_move (&r->widget, r->pos, 1);
        return MSG_HANDLED;

    case WIDGET_UNFOCUS:
    case WIDGET_FOCUS:
    case WIDGET_DRAW:
        for (i = 0; i < r->count; i++)
        {
            const gboolean focused = (i == r->pos && msg == WIDGET_FOCUS);

            widget_selectcolor (w, focused, FALSE);
            widget_move (&r->widget, i, 0);
            tty_draw_hline (r->widget.y + i, r->widget.x, ' ', r->widget.cols);
            tty_print_string ((r->sel == i) ? "(*) " : "( ) ");
            hotkey_draw (w, r->texts[i], focused);
        }
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        for (i = 0; i < r->count; i++)
            release_hotkey (r->texts[i]);
        g_free (r->texts);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
radio_event (Gpm_Event * event, void *data)
{
    WRadio *r = data;
    Widget *w = data;

    if ((event->type & (GPM_DOWN | GPM_UP)) != 0)
    {
        Dlg_head *h = r->widget.owner;

        r->pos = event->y - 1;
        dlg_select_widget (r);
        if ((event->type & GPM_UP) != 0)
        {
            radio_callback (w, WIDGET_KEY, ' ');
            h->callback (h, w, DLG_POST_KEY, ' ', NULL);
            return MOU_NORMAL;
        }
    }
    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WRadio *
radio_new (int y, int x, int count, const char **texts)
{
    WRadio *r;
    int i, wmax = 0;

    r = g_new (WRadio, 1);
    /* Compute the longest string */
    r->texts = g_new (hotkey_t, count);

    for (i = 0; i < count; i++)
    {
        int w;

        r->texts[i] = parse_hotkey (texts[i]);
        w = hotkey_width (r->texts[i]);
        wmax = max (w, wmax);
    }

    init_widget (&r->widget, y, x, count, 4 + wmax, radio_callback, radio_event);
    /* 4 is width of "(*) " */
    r->state = 1;
    r->pos = 0;
    r->sel = 0;
    r->count = count;
    widget_want_hotkey (r->widget, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */
