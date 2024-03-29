/* Terminal management xterm and rxvt support
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009 Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file win.c
 *  \brief Source: Terminal management xterm and rxvt support
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"           /* is_printable() */
#include "tty.h"                /* tty_gotoyx, tty_print_char */
#include "win.h"
#include "src/consaver/cons.saver.h"    /* console_flag */

/*** global variables ****************************************************************************/

/* This flag is set by xterm detection routine in function main() */
/* It is used by function view_other_cmd() */
int xterm_flag = 0;

extern int keybar_visible;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean rxvt_extensions = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* my own wierd protocol base 16 - paul */
static int
rxvt_getc (void)
{
    int r;
    unsigned char c;

    while (read (0, &c, 1) != 1);
    if (c == '\n')
        return -1;
    r = (c - 'A') * 16;
    while (read (0, &c, 1) != 1);
    r += (c - 'A');
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
anything_ready (void)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO (&fds);
    FD_SET (0, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select (1, &fds, 0, 0, &tv);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
do_enter_ca_mode (void)
{
    if (xterm_flag)
    {
        fprintf (stdout, /* ESC_STR ")0" */ ESC_STR "7" ESC_STR "[?47h");
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
do_exit_ca_mode (void)
{
    if (xterm_flag)
    {
        fprintf (stdout, ESC_STR "[?47l" ESC_STR "8" ESC_STR "[m");
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
show_rxvt_contents (int starty, unsigned char y1, unsigned char y2)
{
    unsigned char *k;
    int bytes, i, j, cols = 0;

    y1 += (keybar_visible != 0);        /* i don't knwo why we need this - paul */
    y2 += (keybar_visible != 0);
    while (anything_ready ())
        tty_lowlevel_getch ();

    /* my own wierd protocol base 26 - paul */
    printf ("\033CL%c%c%c%c\n", (y1 / 26) + 'A', (y1 % 26) + 'A', (y2 / 26) + 'A', (y2 % 26) + 'A');

    bytes = (y2 - y1) * (COLS + 1) + 1; /* *should* be the number of bytes read */
    j = 0;
    k = g_malloc (bytes);
    for (;;)
    {
        int c;
        c = rxvt_getc ();
        if (c < 0)
            break;
        if (j < bytes)
            k[j++] = c;
        for (cols = 1;; cols++)
        {
            c = rxvt_getc ();
            if (c < 0)
                break;
            if (j < bytes)
                k[j++] = c;
        }
    }
    for (i = 0; i < j; i++)
    {
        if ((i % cols) == 0)
            tty_gotoyx (starty + (i / cols), 0);
        tty_print_char (is_printable (k[i]) ? k[i] : ' ');
    }
    g_free (k);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
look_for_rxvt_extensions (void)
{
    static gboolean been_called = FALSE;

    if (!been_called)
    {
        const char *e = getenv ("RXVT_EXT");
        rxvt_extensions = ((e != NULL) && (strcmp (e, "1.0") == 0));
        been_called = TRUE;
    }

    if (rxvt_extensions)
        console_flag = 4;

    return rxvt_extensions;
}

/* --------------------------------------------------------------------------------------------- */
