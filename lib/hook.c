/* Hooks
   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2007, 2009, 2010 Free Software Foundation, Inc.
   Written 1994, 1995, 1996 by:
   Miguel de Icaza, Janne Kukonlehto, Dugan Porter,
   Jakub Jelinek, Mauricio Plaza.

   The file_date routine is mostly from GNU's fileutils package,
   written by Richard Stallman and David MacKenzie.

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

/** \file
 *  \brief Source: hooks
 */

#include <config.h>

#include "lib/global.h"
#include "lib/hook.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
add_hook (hook_t ** hook_list, void (*hook_fn) (void *), void *data)
{
    hook_t *new_hook = g_new (hook_t, 1);

    new_hook->hook_fn = hook_fn;
    new_hook->next = *hook_list;
    new_hook->hook_data = data;

    *hook_list = new_hook;
}

/* --------------------------------------------------------------------------------------------- */

void
execute_hooks (hook_t * hook_list)
{
    hook_t *new_hook = NULL;
    hook_t *p;

    /* We copy the hook list first so tahat we let the hook
     * function call delete_hook
     */

    while (hook_list != NULL)
    {
        add_hook (&new_hook, hook_list->hook_fn, hook_list->hook_data);
        hook_list = hook_list->next;
    }
    p = new_hook;

    while (new_hook != NULL)
    {
        new_hook->hook_fn (new_hook->hook_data);
        new_hook = new_hook->next;
    }

    for (hook_list = p; hook_list != NULL;)
    {
        p = hook_list;
        hook_list = hook_list->next;
        g_free (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
delete_hook (hook_t ** hook_list, void (*hook_fn) (void *))
{
    hook_t *new_list = NULL;
    hook_t *current, *next;

    for (current = *hook_list; current != NULL; current = next)
    {
        next = current->next;
        if (current->hook_fn == hook_fn)
            g_free (current);
        else
            add_hook (&new_list, current->hook_fn, current->hook_data);
    }
    *hook_list = new_list;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
hook_present (hook_t * hook_list, void (*hook_fn) (void *))
{
    hook_t *p;

    for (p = hook_list; p != NULL; p = p->next)
        if (p->hook_fn == hook_fn)
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
