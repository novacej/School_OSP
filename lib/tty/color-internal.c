/*  Internal stuff of color setup
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2008, 2009, 2010 Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2009
   Egmont Koblinger <egmont@gmail.com>, 2010

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

/** \file color-internal.c
 *  \brief Source: Internal stuff of color setup
 */

#include <config.h>

#include <string.h>             /* strcmp */

#include "color.h"              /* colors and attributes */
#include "color-internal.h"

/*** global variables ****************************************************************************/

gboolean mc_tty_color_disable;

/*** file scope macro definitions ****************************************************************/

#define COLOR_INTENSITY 8

/*** file scope type declarations ****************************************************************/

typedef struct mc_tty_color_table_struct
{
    const char *name;
    int value;
} mc_tty_color_table_t;

/*** file scope variables ************************************************************************/

mc_tty_color_table_t const color_table[] = {
    {"black", COLOR_BLACK},
    {"gray", COLOR_BLACK + COLOR_INTENSITY},
    {"red", COLOR_RED},
    {"brightred", COLOR_RED + COLOR_INTENSITY},
    {"green", COLOR_GREEN},
    {"brightgreen", COLOR_GREEN + COLOR_INTENSITY},
    {"brown", COLOR_YELLOW},
    {"yellow", COLOR_YELLOW + COLOR_INTENSITY},
    {"blue", COLOR_BLUE},
    {"brightblue", COLOR_BLUE + COLOR_INTENSITY},
    {"magenta", COLOR_MAGENTA},
    {"brightmagenta", COLOR_MAGENTA + COLOR_INTENSITY},
    {"cyan", COLOR_CYAN},
    {"brightcyan", COLOR_CYAN + COLOR_INTENSITY},
    {"lightgray", COLOR_WHITE},
    {"white", COLOR_WHITE + COLOR_INTENSITY},
    {"default", -1},            /* default color of the terminal */
    /* special colors */
    {"A_REVERSE", SPEC_A_REVERSE},
    {"A_BOLD", SPEC_A_BOLD},
    {"A_BOLD_REVERSE", SPEC_A_BOLD_REVERSE},
    {"A_UNDERLINE", SPEC_A_UNDERLINE},
    /* End of list */
    {NULL, 0}
};

mc_tty_color_table_t const attributes_table[] = {
    {"bold", A_BOLD},
    {"underline", A_UNDERLINE},
    {"reverse", A_REVERSE},
    {"blink", A_BLINK},
    /* End of list */
    {NULL, 0}
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
parse_256_color_name (const char *color_name)
{
    int i;
    char dummy;
    if (sscanf (color_name, "color%d%c", &i, &dummy) == 1 && i >= 0 && i < 256)
    {
        return i;
    }
    if (sscanf (color_name, "gray%d%c", &i, &dummy) == 1 && i >= 0 && i < 24)
    {
        return 232 + i;
    }
    if (strncmp (color_name, "rgb", 3) == 0 &&
        color_name[3] >= '0' && color_name[3] < '6' &&
        color_name[4] >= '0' && color_name[4] < '6' &&
        color_name[5] >= '0' && color_name[5] < '6' && color_name[6] == '\0')
    {
        return 16 + 36 * (color_name[3] - '0') + 6 * (color_name[4] - '0') + (color_name[5] - '0');
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const char *
tty_color_get_name_by_index (int idx)
{
    static char **color_N_names = NULL;
    int i;

    /* Find the real English name of the first 16 colors, */
    /* as well as the A_* special values. */
    for (i = 0; color_table[i].name != NULL; i++)
        if (idx == color_table[i].value)
            return color_table[i].name;
    /* Create and return the strings "color16" to "color255". */
    if (idx >= 16 && idx < 256)
    {
        if (color_N_names == NULL)
        {
            color_N_names = g_try_malloc0 (240 * sizeof (char *));
        }
        if (color_N_names[idx - 16] == NULL)
        {
            color_N_names[idx - 16] = g_try_malloc (9);
            sprintf (color_N_names[idx - 16], "color%d", idx);
        }
        return color_N_names[idx - 16];
    }
    return "default";
}

/* --------------------------------------------------------------------------------------------- */

int
tty_color_get_index_by_name (const char *color_name)
{

    if (color_name != NULL)
    {
        size_t i;
        for (i = 0; color_table[i].name != NULL; i++)
            if (strcmp (color_name, color_table[i].name) == 0)
                return color_table[i].value;
        return parse_256_color_name (color_name);
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

int
tty_attr_get_bits (const char *attrs)
{
    int attr_bits = 0;
    gchar **attr_list;
    int i, j;

    if (attrs != NULL)
    {
        attr_list = g_strsplit (attrs, "+", -1);
        for (i = 0; attr_list[i] != NULL; i++)
        {
            for (j = 0; attributes_table[j].name != NULL; j++)
            {
                if (strcmp (attr_list[i], attributes_table[j].name) == 0)
                {
                    attr_bits |= attributes_table[j].value;
                    break;
                }
            }
        }
        g_strfreev (attr_list);
    }
    return attr_bits;
}

/* --------------------------------------------------------------------------------------------- */
