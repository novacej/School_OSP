/*
   Provides a log file to ease tracing the program.

   Copyright (C) 2006, 2009 Free Software Foundation, Inc.

   Written: 2006 Roland Illig <roland.illig@gmx.de>.

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

/** \file logging.c
 *  \brief Source: provides a log file to ease tracing the program
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/fileloc.h"

#include "logging.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define CONFIG_GROUP_NAME "Development"
#define CONFIG_KEY_NAME "logging"
#define CONFIG_KEY_NAME_FILE "logfile"

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean logging_initialized = FALSE;
static gboolean logging_enabled = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
is_logging_enabled_from_env (void)
{
    const char *env_is_enabled;

    env_is_enabled = g_getenv ("MC_LOG_ENABLE");
    if (env_is_enabled == NULL)
        return FALSE;

    logging_enabled = (*env_is_enabled == '1' || g_ascii_strcasecmp (env_is_enabled, "true") == 0);
    logging_initialized = TRUE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_logging_enabled (void)
{

    if (logging_initialized)
        return logging_enabled;

    if (is_logging_enabled_from_env ())
        return logging_enabled;

    logging_enabled =
        mc_config_get_bool (mc_main_config, CONFIG_GROUP_NAME, CONFIG_KEY_NAME, FALSE);
    logging_initialized = TRUE;

    return logging_enabled;
}

/* --------------------------------------------------------------------------------------------- */

static char *
get_log_filename (void)
{
    const char *env_filename;

    env_filename = g_getenv ("MC_LOG_FILE");
    if (env_filename != NULL)
        return g_strdup (env_filename);

    if (mc_config_has_param (mc_main_config, CONFIG_GROUP_NAME, CONFIG_KEY_NAME_FILE))
        return mc_config_get_string (mc_main_config, CONFIG_GROUP_NAME, CONFIG_KEY_NAME_FILE, NULL);

    return g_build_filename (mc_config_get_cache_path (), "mc.log", NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_va_log (const char *fmt, va_list args)
{
    FILE *f;
    char *logfilename;

    logfilename = get_log_filename ();

    if (logfilename != NULL)
    {
        f = fopen (logfilename, "a");
        if (f != NULL)
        {
            (void) vfprintf (f, fmt, args);
            (void) fclose (f);
        }
        g_free (logfilename);
    }

}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_log (const char *fmt, ...)
{
    va_list args;

    if (!is_logging_enabled ())
        return;

    va_start (args, fmt);
    mc_va_log (fmt, args);
    va_end (args);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_always_log (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    mc_va_log (fmt, args);
    va_end (args);
}

/* --------------------------------------------------------------------------------------------- */
