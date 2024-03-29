/* Setup loading/saving.
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2009, 2010 Free Software Foundation, Inc.

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

/** \file setup.c
 *  \brief Source: setup loading/saving
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/mcconfig.h"
#include "lib/fileloc.h"
#include "lib/timefmt.h"

#include "lib/vfs/mc-vfs/vfs.h"

#ifdef ENABLE_VFS_FTP
#include "lib/vfs/mc-vfs/ftpfs.h"
#endif
#ifdef ENABLE_VFS_FISH
#include "lib/vfs/mc-vfs/fish.h"
#endif

#include "lib/util.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "filemanager/dir.h"
#include "filemanager/midnight.h"
#include "filemanager/tree.h"   /* xtree_mode */
#include "filemanager/hotlist.h"        /* load/save/done hotlist */
#include "filemanager/panelize.h"       /* load/save/done panelize */
#include "filemanager/layout.h"
#include "filemanager/cmd.h"

#include "args.h"
#include "execute.h"            /* pause_after_run */
#include "clipboard.h"
#include "keybind-defaults.h"   /* keybind_lookup_action */

#ifdef HAVE_CHARSET
#include "selcodepage.h"
#endif

#ifdef USE_INTERNAL_EDIT
#include "src/editor/edit.h"
#endif

#include "src/viewer/mcviewer.h"        /* For the externs */

#include "setup.h"

/*** global variables ****************************************************************************/

char *profile_name;             /* ${XDG_CONFIG_HOME}/mc/ini */
char *global_profile_name;      /* mc.lib */

/* Only used at program boot */
gboolean boot_current_is_left = TRUE;

char *setup_color_string;
char *term_color_string;
char *color_terminal_string;

/* If on, default for "No" in delete operations */
int safe_delete = 0;

/* Controls screen clearing before an exec */
int clear_before_exec = 1;

/* Asks for confirmation before deleting a file */
int confirm_delete = 1;
/* Asks for confirmation before deleting a hotlist entry */
int confirm_directory_hotlist_delete = 1;
/* Asks for confirmation before overwriting a file */
int confirm_overwrite = 1;
/* Asks for confirmation before executing a program by pressing enter */
int confirm_execute = 0;
/* Asks for confirmation before leaving the program */
int confirm_exit = 1;
/* Asks for confirmation before clean up of history */
int confirm_history_cleanup = 1;

/* If true, at startup the user-menu is invoked */
int auto_menu = 0;
/* This flag indicates if the pull down menus by default drop down */
int drop_menus = 0;

/* Asks for confirmation when using F3 to view a directory and there
   are tagged files */
int confirm_view_dir = 0;

panel_view_mode_t startup_left_mode;
panel_view_mode_t startup_right_mode;

int setup_copymove_persistent_attr = 1;

/* Tab size */
int option_tab_spacing = DEFAULT_TAB_SPACING;

/* Ugly hack to allow panel_save_setup to work as a place holder for */
/* default panel values */
int saving_setup;

panels_options_t panels_options = {
    .show_mini_info = TRUE,
    .kilobyte_si = FALSE,
    .mix_all_files = FALSE,
    .show_backups = TRUE,
    .show_dot_files = TRUE,
    .fast_reload = FALSE,
    .fast_reload_msg_shown = FALSE,
    .mark_moves_down = TRUE,
    .reverse_files_only = TRUE,
    .auto_save_setup = FALSE,
    .navigate_with_arrows = FALSE,
    .scroll_pages = TRUE,
    .mouse_move_pages = TRUE,
    .filetype_mode = TRUE,
    .permission_mode = FALSE,
    .qsearch_mode = QSEARCH_PANEL_CASE,
    .torben_fj_mode = FALSE
};

int easy_patterns = 1;

/* It true saves the setup when quitting */
int auto_save_setup = 1;

/* If true, then the +, - and \ keys have their special meaning only if the
 * command line is emtpy, otherwise they behave like regular letters
 */
int only_leading_plus_minus = 1;

/* Set when cd symlink following is desirable (bash mode) */
int cd_symlinks = 1;

/* Set if you want the possible completions dialog for the first time */
int show_all_if_ambiguous = 0;

/* Automatically fills name with current selected item name on mkdir */
int auto_fill_mkdir_name = 1;

/* If set and you don't have subshell support,then C-o will give you a shell */
int output_starts_shell = 0;

/* If set, we execute the file command to check the file type */
int use_file_to_check_type = 1;

int verbose = 1;

/*
 * Whether the Midnight Commander tries to provide more
 * information about copy/move sizes and bytes transfered
 * at the expense of some speed
 */
int file_op_compute_totals = 1;

/*** file scope macro definitions ****************************************************************/

/* In order to use everywhere the same setup for the locale we use defines */
#define FMTYEAR _("%b %e  %Y")
#define FMTTIME _("%b %e %H:%M")

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static char *panels_profile_name = NULL;        /* ${XDG_CACHE_HOME}/mc/panels.ini */

/* *INDENT-OFF* */
static const struct
{
    const char *key;
    int  list_type;
} list_types [] = {
    { "full",  list_full  },
    { "brief", list_brief },
    { "long",  list_long  },
    { "user",  list_user  },
    { NULL, 0 }
};

static const struct
{
    const char *opt_name;
    panel_view_mode_t opt_type;
} panel_types [] = {
    { "listing",   view_listing },
    { "quickview", view_quick },
    { "info",      view_info },
    { "tree",      view_tree },
    { NULL,        view_listing }
};

static const struct
{
    const char *opt_name;
    int *opt_addr;
} layout [] = {
    { "equal_split", &equal_split },
    { "first_panel_size", &first_panel_size },
    { "message_visible", &message_visible },
    { "keybar_visible", &keybar_visible },
    { "xterm_title", &xterm_title },
    { "output_lines", &output_lines },
    { "command_prompt", &command_prompt },
    { "menubar_visible", &menubar_visible },
    { "free_space", &free_space },
    { NULL, NULL }
};

static const struct
{
    const char *opt_name;
    int *opt_addr;
} int_options [] = {
    { "verbose", &verbose },
    { "pause_after_run", &pause_after_run },
    { "shell_patterns", &easy_patterns },
    { "auto_save_setup", &auto_save_setup },
    { "auto_menu", &auto_menu },
    { "use_internal_view", &use_internal_view },
    { "use_internal_edit", &use_internal_edit },
    { "clear_before_exec", &clear_before_exec },
    { "confirm_delete", &confirm_delete },
    { "confirm_overwrite", &confirm_overwrite },
    { "confirm_execute", &confirm_execute },
    { "confirm_history_cleanup", &confirm_history_cleanup },
    { "confirm_exit", &confirm_exit },
    { "confirm_directory_hotlist_delete", &confirm_directory_hotlist_delete },
    { "safe_delete", &safe_delete },
    { "mouse_repeat_rate", &mou_auto_repeat },
    { "double_click_speed", &double_click_speed },
#ifndef HAVE_CHARSET
    { "eight_bit_clean", &eight_bit_clean },
    { "full_eight_bits", &full_eight_bits },
#endif /* !HAVE_CHARSET */
    { "use_8th_bit_as_meta", &use_8th_bit_as_meta },
    { "confirm_view_dir", &confirm_view_dir },
    { "mouse_move_pages_viewer", &mcview_mouse_move_pages },
    { "mouse_close_dialog", &mouse_close_dialog},
    { "fast_refresh", &fast_refresh },
    { "drop_menus", &drop_menus },
    { "wrap_mode",  &mcview_global_wrap_mode},
    { "old_esc_mode", &old_esc_mode },
    { "old_esc_mode_timeout", &old_esc_mode_timeout },
    { "cd_symlinks", &cd_symlinks },
    { "show_all_if_ambiguous", &show_all_if_ambiguous },
    { "max_dirt_limit", &mcview_max_dirt_limit },
    { "use_file_to_guess_type", &use_file_to_check_type },
    { "alternate_plus_minus", &alternate_plus_minus },
    { "only_leading_plus_minus", &only_leading_plus_minus },
    { "show_output_starts_shell", &output_starts_shell },
    { "xtree_mode", &xtree_mode },
    { "num_history_items_recorded", &num_history_items_recorded },
    { "file_op_compute_totals", &file_op_compute_totals },
    { "classic_progressbar", &classic_progressbar},
#ifdef ENABLE_VFS
    { "vfs_timeout", &vfs_timeout },
#ifdef ENABLE_VFS_FTP
    { "ftpfs_directory_timeout", &ftpfs_directory_timeout },
    { "use_netrc", &ftpfs_use_netrc },
    { "ftpfs_retry_seconds", &ftpfs_retry_seconds },
    { "ftpfs_always_use_proxy", &ftpfs_always_use_proxy },
    { "ftpfs_use_passive_connections", &ftpfs_use_passive_connections },
    { "ftpfs_use_passive_connections_over_proxy", &ftpfs_use_passive_connections_over_proxy },
    { "ftpfs_use_unix_list_options", &ftpfs_use_unix_list_options },
    { "ftpfs_first_cd_then_ls", &ftpfs_first_cd_then_ls },
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_FISH
    { "fish_directory_timeout", &fish_directory_timeout },
#endif /* ENABLE_VFS_FISH */
#endif /* ENABLE_VFS */
    /* option_tab_spacing is used in internal viewer */
    { "editor_tab_spacing", &option_tab_spacing },
#ifdef USE_INTERNAL_EDIT
    { "editor_word_wrap_line_length", &option_word_wrap_line_length },
    { "editor_fill_tabs_with_spaces", &option_fill_tabs_with_spaces },
    { "editor_return_does_auto_indent", &option_return_does_auto_indent },
    { "editor_backspace_through_tabs", &option_backspace_through_tabs },
    { "editor_fake_half_tabs", &option_fake_half_tabs },
    { "editor_option_save_mode", &option_save_mode },
    { "editor_option_save_position", &option_save_position },
    { "editor_option_auto_para_formatting", &option_auto_para_formatting },
    { "editor_option_typewriter_wrap", &option_typewriter_wrap },
    { "editor_edit_confirm_save", &edit_confirm_save },
    { "editor_syntax_highlighting", &option_syntax_highlighting },
    { "editor_persistent_selections", &option_persistent_selections },
    { "editor_cursor_beyond_eol", &option_cursor_beyond_eol },
    { "editor_visible_tabs", &visible_tabs },
    { "editor_visible_spaces", &visible_tws },
    { "editor_line_state", &option_line_state },
    { "editor_simple_statusbar", &simple_statusbar },
    { "editor_check_new_line", &option_check_nl_at_eof },
    { "editor_show_right_margin", &show_right_margin },
    { "editor_group_undo", &option_group_undo },
#endif /* USE_INTERNAL_EDIT */
    { "nice_rotating_dash", &nice_rotating_dash },
    { "horizontal_split",   &horizontal_split },
    { "mcview_remember_file_position", &mcview_remember_file_position },
    { "auto_fill_mkdir_name", &auto_fill_mkdir_name },
    { "copymove_persistent_attr", &setup_copymove_persistent_attr },
    { "select_flags", &select_flags },
    { NULL, NULL }
};

static const struct
{
    const char *opt_name;
    char **opt_addr;
    const char *opt_defval;
} str_options[] = {
#ifdef USE_INTERNAL_EDIT
    { "editor_backup_extension", &option_backup_ext, "~" },
#endif
    { "mcview_eof", &mcview_show_eof, "" },
    {  NULL, NULL, NULL }
};

static const struct
{
    const char *opt_name;
    gboolean *opt_addr;
} panels_ini_options[] = {
    { "show_mini_info", &panels_options.show_mini_info },
    { "kilobyte_si", &panels_options.kilobyte_si },
    { "mix_all_files", &panels_options.mix_all_files },
    { "show_backups", &panels_options.show_backups },
    { "show_dot_files", &panels_options.show_dot_files },
    { "fast_reload", &panels_options.fast_reload },
    { "fast_reload_msg_shown", &panels_options.fast_reload_msg_shown },
    { "mark_moves_down", &panels_options.mark_moves_down },
    { "reverse_files_only", &panels_options.reverse_files_only },
    { "auto_save_setup_panels", &panels_options.auto_save_setup },
    { "navigate_with_arrows", &panels_options.navigate_with_arrows },
    { "panel_scroll_pages", &panels_options.scroll_pages },
    { "mouse_move_pages",  &panels_options.mouse_move_pages },
    { "filetype_mode", &panels_options.filetype_mode },
    { "permission_mode", &panels_options.permission_mode },
    { "torben_fj_mode", &panels_options.torben_fj_mode },
    { NULL, NULL }
};
/* *INDENT-ON* */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
  Get name of config file.

 \param subdir
 if not NULL, then config also search into specified subdir.

 \param config_file_name
 If specified filename is relative, then will search in standart patches.

 \return
 Newly allocated path to config name or NULL if file not found.

 If config_file_name is a relative path, then search config in stantart paths.
*/
static char *
load_setup_get_full_config_name (const char *subdir, const char *config_file_name)
{
    /*
       TODO: IMHO, in future this function must be placed into mc_config module.
     */
    char *lc_basename, *ret;

    if (config_file_name == NULL)
        return NULL;

    if (g_path_is_absolute (config_file_name))
        return g_strdup (config_file_name);


    lc_basename = g_path_get_basename (config_file_name);
    if (lc_basename == NULL)
        return NULL;

    if (subdir != NULL)
        ret = g_build_filename (mc_config_get_path (), subdir, lc_basename, NULL);
    else
        ret = g_build_filename (mc_config_get_path (), lc_basename, NULL);

    if (exist_file (ret))
    {
        g_free (lc_basename);
        return ret;
    }
    g_free (ret);

    if (subdir != NULL)
        ret = g_build_filename (mc_sysconfig_dir, subdir, lc_basename, NULL);
    else
        ret = g_build_filename (mc_sysconfig_dir, lc_basename, NULL);

    if (exist_file (ret))
    {
        g_free (lc_basename);
        return ret;
    }
    g_free (ret);

    if (subdir != NULL)
        ret = g_build_filename (mc_share_data_dir, subdir, lc_basename, NULL);
    else
        ret = g_build_filename (mc_share_data_dir, lc_basename, NULL);

    g_free (lc_basename);

    if (exist_file (ret))
        return ret;

    g_free (ret);
    return NULL;

}

/* --------------------------------------------------------------------------------------------- */

static const char *
setup__is_cfg_group_must_panel_config (const char *grp)
{
    return (!strcasecmp ("Dirs", grp) ||
            !strcasecmp ("Temporal:New Right Panel", grp) ||
            !strcasecmp ("Temporal:New Left Panel", grp) ||
            !strcasecmp ("New Left Panel", grp) || !strcasecmp ("New Right Panel", grp))
        ? grp : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup__move_panels_config_into_separate_file (const char *profile)
{
    mc_config_t *tmp_cfg;
    char **groups, **curr_grp;
    const char *need_grp;

    if (!exist_file (profile))
        return;

    tmp_cfg = mc_config_init (profile);
    if (!tmp_cfg)
        return;

    curr_grp = groups = mc_config_get_groups (tmp_cfg, NULL);
    if (!groups)
    {
        mc_config_deinit (tmp_cfg);
        return;
    }

    while (*curr_grp)
    {
        if (setup__is_cfg_group_must_panel_config (*curr_grp) == NULL)
            mc_config_del_group (tmp_cfg, *curr_grp);
        curr_grp++;
    }

    mc_config_save_to_file (tmp_cfg, panels_profile_name, NULL);
    mc_config_deinit (tmp_cfg);

    tmp_cfg = mc_config_init (profile);
    if (!tmp_cfg)
    {
        g_strfreev (groups);
        return;
    }

    curr_grp = groups;

    while (*curr_grp)
    {
        need_grp = setup__is_cfg_group_must_panel_config (*curr_grp);
        if (need_grp != NULL)
        {
            mc_config_del_group (tmp_cfg, need_grp);
        }
        curr_grp++;
    }
    g_strfreev (groups);

    mc_config_save_file (tmp_cfg, NULL);
    mc_config_deinit (tmp_cfg);
}

/* --------------------------------------------------------------------------------------------- */
/**
  Create new mc_config object from specified ini-file or
  append data to existing mc_config object from ini-file

*/

static void
load_setup_init_config_from_file (mc_config_t ** config, const char *fname)
{
    /*
       TODO: IMHO, in future this function must be placed into mc_config module.
     */
    if (exist_file (fname))
    {
        if (*config != NULL)
            mc_config_read_file (*config, fname);
        else
            *config = mc_config_init (fname);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
load_layout (void)
{
    size_t i;

    for (i = 0; layout[i].opt_name != NULL; i++)
        *layout[i].opt_addr = mc_config_get_int (mc_main_config, "Layout",
                                                 layout[i].opt_name, *layout[i].opt_addr);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_keys_from_section (const char *terminal, mc_config_t * cfg)
{
    char *section_name;
    gchar **profile_keys, **keys;
    gchar **values, **curr_values;
    char *valcopy, *value;
    long key_code;
    gsize len, values_len;

    if (terminal == NULL)
        return;

    section_name = g_strconcat ("terminal:", terminal, (char *) NULL);
    profile_keys = keys = mc_config_get_keys (cfg, section_name, &len);

    while (*profile_keys != NULL)
    {
        /* copy=other causes all keys from [terminal:other] to be loaded. */
        if (g_ascii_strcasecmp (*profile_keys, "copy") == 0)
        {
            valcopy = mc_config_get_string (cfg, section_name, *profile_keys, "");
            load_keys_from_section (valcopy, cfg);
            g_free (valcopy);
            profile_keys++;
            continue;
        }

        curr_values = values =
            mc_config_get_string_list (cfg, section_name, *profile_keys, &values_len);

        key_code = lookup_key (*profile_keys, NULL);

        if (key_code != 0)
        {
            if (curr_values != NULL)
            {
                while (*curr_values != NULL)
                {
                    valcopy = convert_controls (*curr_values);
                    define_sequence (key_code, valcopy, MCKEY_NOACTION);
                    g_free (valcopy);
                    curr_values++;
                }
            }
            else
            {
                value = mc_config_get_string (cfg, section_name, *profile_keys, "");
                valcopy = convert_controls (value);
                define_sequence (key_code, valcopy, MCKEY_NOACTION);
                g_free (valcopy);
                g_free (value);
            }
        }

        profile_keys++;
        g_strfreev (values);
    }
    g_strfreev (keys);
    g_free (section_name);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_keymap_from_section (const char *section_name, GArray * keymap, mc_config_t * cfg)
{
    gchar **profile_keys, **keys;
    gchar **values, **curr_values;
    char *valcopy, *value;
    int action;
    gsize len, values_len;

    if (section_name == NULL)
        return;

    profile_keys = keys = mc_config_get_keys (cfg, section_name, &len);

    while (*profile_keys != NULL)
    {
        curr_values = values =
            mc_config_get_string_list (cfg, section_name, *profile_keys, &values_len);

        action = keybind_lookup_action (*profile_keys);

        if (action > 0)
        {
            if (curr_values != NULL)
            {
                while (*curr_values != NULL)
                {
                    valcopy = convert_controls (*curr_values);
                    keybind_cmd_bind (keymap, valcopy, action);
                    g_free (valcopy);
                    curr_values++;
                }
            }
            else
            {
                value = mc_config_get_string (cfg, section_name, *profile_keys, "");
                valcopy = convert_controls (value);
                /* define_sequence (key_code, valcopy, MCKEY_NOACTION); */
                g_free (valcopy);
                g_free (value);
            }
        }

        profile_keys++;
        g_strfreev (values);
    }
    g_strfreev (keys);
}

/* --------------------------------------------------------------------------------------------- */

static mc_config_t *
load_setup_get_keymap_profile_config (void)
{
    /*
       TODO: IMHO, in future this function must be placed into mc_config module.
     */
    mc_config_t *keymap_config = NULL;
    char *fname, *fname2;

    /* 1) /usr/share/mc (mc_share_data_dir) */
    fname = g_build_filename (mc_share_data_dir, GLOBAL_KEYMAP_FILE, NULL);
    load_setup_init_config_from_file (&keymap_config, fname);
    g_free (fname);

    /* 2) /etc/mc (mc_sysconfig_dir) */
    fname = g_build_filename (mc_sysconfig_dir, GLOBAL_KEYMAP_FILE, NULL);
    load_setup_init_config_from_file (&keymap_config, fname);
    g_free (fname);

    /* 3) ${XDG_CONFIG_HOME}/mc */
    fname = g_build_filename (mc_config_get_path (), GLOBAL_KEYMAP_FILE, NULL);
    load_setup_init_config_from_file (&keymap_config, fname);
    g_free (fname);

    /* 4) main config; [Midnight Commander] -> keymap */

    fname2 =
        mc_config_get_string (mc_main_config, CONFIG_APP_SECTION, "keymap", GLOBAL_KEYMAP_FILE);
    fname = load_setup_get_full_config_name (NULL, fname2);
    if (fname != NULL)
    {
        load_setup_init_config_from_file (&keymap_config, fname);
        g_free (fname);
    }
    g_free (fname2);

    /* 5) getenv("MC_KEYMAP") */
    fname = load_setup_get_full_config_name (NULL, g_getenv ("MC_KEYMAP"));
    if (fname != NULL)
    {
        load_setup_init_config_from_file (&keymap_config, fname);
        g_free (fname);
    }

    /* 6) --keymap=<keymap> */
    fname = load_setup_get_full_config_name (NULL, mc_args__keymap_file);
    if (fname != NULL)
    {
        load_setup_init_config_from_file (&keymap_config, fname);
        g_free (fname);
    }

    return keymap_config;
}

/* --------------------------------------------------------------------------------------------- */

static panel_view_mode_t
setup__load_panel_state (const char *section)
{
    char *buffer;
    size_t i;
    panel_view_mode_t mode = view_listing;

    /* Load the display mode */
    buffer = mc_config_get_string (mc_panels_config, section, "display", "listing");

    for (i = 0; panel_types[i].opt_name != NULL; i++)
        if (g_ascii_strcasecmp (panel_types[i].opt_name, buffer) == 0)
        {
            mode = panel_types[i].opt_type;
            break;
        }

    g_free (buffer);

    return mode;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_save_type (const char *section, panel_view_mode_t type)
{
    size_t i;

    for (i = 0; panel_types[i].opt_name != NULL; i++)
        if (panel_types[i].opt_type == type)
        {
            mc_config_set_string (mc_panels_config, section, "display", panel_types[i].opt_name);
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

/* save panels.ini */
static void
save_panel_types (void)
{
    panel_view_mode_t type;

    if (mc_run_mode != MC_RUN_FULL)
        return;

    type = get_display_type (0);
    panel_save_type ("New Left Panel", type);
    if (type == view_listing)
        panel_save_setup (left_panel, left_panel->panel_name);
    type = get_display_type (1);
    panel_save_type ("New Right Panel", type);
    if (type == view_listing)
        panel_save_setup (right_panel, right_panel->panel_name);

    mc_config_set_string (mc_panels_config, "Dirs", "other_dir", get_panel_dir_for (other_panel));

    if (current_panel != NULL)
        mc_config_set_string (mc_panels_config, "Dirs", "current_is_left",
                              get_current_index () == 0 ? "1" : "0");

    if (mc_panels_config->ini_path == NULL)
        mc_panels_config->ini_path = g_strdup (panels_profile_name);

    mc_config_del_group (mc_panels_config, "Temporal:New Left Panel");
    mc_config_del_group (mc_panels_config, "Temporal:New Right Panel");

    mc_config_save_file (mc_panels_config, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
setup_init (void)
{
    char *profile;
    char *inifile;

    if (profile_name != NULL)
        return profile_name;

    profile = g_build_filename (mc_config_get_path (), MC_CONFIG_FILE, NULL);
    if (!exist_file (profile))
    {
        inifile = concat_dir_and_file (mc_sysconfig_dir, "mc.ini");
        if (exist_file (inifile))
        {
            g_free (profile);
            profile = inifile;
        }
        else
        {
            g_free (inifile);
            inifile = concat_dir_and_file (mc_share_data_dir, "mc.ini");
            if (exist_file (inifile))
            {
                g_free (profile);
                profile = inifile;
            }
            else
                g_free (inifile);
        }
    }

    profile_name = profile;

    return profile;
}

/* --------------------------------------------------------------------------------------------- */

void
load_setup (void)
{
    char *profile;
    size_t i;
    char *buffer;
    const char *kt;

#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif /* HAVE_CHARSET */

    profile = setup_init ();

    /* mc.lib is common for all users, but has priority lower than
       ${XDG_CONFIG_HOME}/mc/ini.  FIXME: it's only used for keys and treestore now */
    global_profile_name = g_build_filename (mc_sysconfig_dir, MC_GLOBAL_CONFIG_FILE, (char *) NULL);
    if (!exist_file (global_profile_name))
    {
        g_free (global_profile_name);
        global_profile_name = g_build_filename (mc_share_data_dir, MC_GLOBAL_CONFIG_FILE, (char *) NULL);
    }

    panels_profile_name = g_build_filename (mc_config_get_cache_path (), MC_PANELS_FILE, NULL);

    mc_main_config = mc_config_init (profile);

    if (!exist_file (panels_profile_name))
        setup__move_panels_config_into_separate_file (profile);

    mc_panels_config = mc_config_init (panels_profile_name);

    /* Load integer boolean options */
    for (i = 0; int_options[i].opt_name != NULL; i++)
        *int_options[i].opt_addr =
            mc_config_get_int (mc_main_config, CONFIG_APP_SECTION, int_options[i].opt_name,
                               *int_options[i].opt_addr);

    if (option_tab_spacing <= 0)
        option_tab_spacing = DEFAULT_TAB_SPACING;

#ifdef USE_INTERNAL_EDIT
    if (option_word_wrap_line_length <= 0)
        option_word_wrap_line_length = DEFAULT_WRAP_LINE_LENGTH;
#endif /* USE_INTERNAL_EDIT */

    /* overwrite old_esc_mode_timeout */
    kt = getenv ("KEYBOARD_KEY_TIMEOUT_US");
    if ((kt != NULL) && (kt[0] != '\0'))
        old_esc_mode_timeout = atoi (kt);

    /* Load string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
        *str_options[i].opt_addr =
            mc_config_get_string (mc_main_config, CONFIG_APP_SECTION, str_options[i].opt_name,
                                  str_options[i].opt_defval);

    load_layout ();
    panels_load_options ();
    load_panelize ();

    startup_left_mode = setup__load_panel_state ("New Left Panel");
    startup_right_mode = setup__load_panel_state ("New Right Panel");

    /* At least one of the panels is a listing panel */
    if (startup_left_mode != view_listing && startup_right_mode != view_listing)
        startup_left_mode = view_listing;

    if (mc_run_param1 == NULL)
    {
        buffer = mc_config_get_string (mc_panels_config, "Dirs", "other_dir", ".");
        if (vfs_file_is_local (buffer))
            mc_run_param1 = buffer;
        else
            g_free (buffer);
    }

    boot_current_is_left = mc_config_get_bool (mc_panels_config, "Dirs", "current_is_left", TRUE);

    /* Load time formats */
    user_recent_timeformat =
        mc_config_get_string (mc_main_config, "Misc", "timeformat_recent", FMTTIME);
    user_old_timeformat = mc_config_get_string (mc_main_config, "Misc", "timeformat_old", FMTYEAR);

#ifdef ENABLE_VFS_FTP
    ftpfs_proxy_host = mc_config_get_string (mc_main_config, "Misc", "ftp_proxy_host", "gate");
    ftpfs_ignore_chattr_errors = mc_config_get_bool (mc_main_config, CONFIG_APP_SECTION,
                                                     "ignore_ftp_chattr_errors", TRUE);
    ftpfs_init_passwd ();
#endif /* ENABLE_VFS_FTP */

    /* The default color and the terminal dependent color */
    setup_color_string = mc_config_get_string (mc_main_config, "Colors", "base_color", "");
    term_color_string = mc_config_get_string (mc_main_config, "Colors", getenv ("TERM"), "");
    color_terminal_string = mc_config_get_string (mc_main_config, "Colors", "color_terminals", "");

    /* Load the directory history */
    /*    directory_history_load (); */
    /* Remove the temporal entries */

#ifdef HAVE_CHARSET
    if (codepages->len > 1)
    {
        buffer = mc_config_get_string (mc_main_config, "Misc", "display_codepage", "");
        if (buffer[0] != '\0')
        {
            display_codepage = get_codepage_index (buffer);
            cp_display = get_codepage_id (display_codepage);
        }
        g_free (buffer);
        buffer = mc_config_get_string (mc_main_config, "Misc", "source_codepage", "");
        if (buffer[0] != '\0')
        {
            default_source_codepage = get_codepage_index (buffer);
            source_codepage = default_source_codepage;  /* May be source_codepage doesn't need this */
            cp_source = get_codepage_id (source_codepage);
        }
        g_free (buffer);
    }

    autodetect_codeset = mc_config_get_string (mc_main_config, "Misc", "autodetect_codeset", "");
    if ((autodetect_codeset[0] != '\0') && (strcmp (autodetect_codeset, "off") != 0))
        is_autodetect_codeset_enabled = TRUE;

    g_free (init_translation_table (source_codepage, display_codepage));
    buffer = (char *) get_codepage_id (display_codepage);
    if (buffer != NULL)
        utf8_display = str_isutf8 (buffer);
#endif /* HAVE_CHARSET */

    clipboard_store_path = mc_config_get_string (mc_main_config, "Misc", "clipboard_store", "");
    clipboard_paste_path = mc_config_get_string (mc_main_config, "Misc", "clipboard_paste", "");
}

/* --------------------------------------------------------------------------------------------- */

gboolean
save_setup (gboolean save_options, gboolean save_panel_options)
{
    gboolean ret = TRUE;

    saving_setup = 1;

    save_hotlist ();

    if (save_panel_options)
        save_panel_types ();

    if (save_options)
    {
        char *tmp_profile;

        save_config ();
        save_layout ();
        panels_save_options ();
        save_panelize ();
        /* directory_history_save (); */

#ifdef ENABLE_VFS_FTP
        mc_config_set_string (mc_main_config, "Misc", "ftpfs_password", ftpfs_anonymous_passwd);
        if (ftpfs_proxy_host)
            mc_config_set_string (mc_main_config, "Misc", "ftp_proxy_host", ftpfs_proxy_host);
#endif /* ENABLE_VFS_FTP */

#ifdef HAVE_CHARSET
        mc_config_set_string (mc_main_config, "Misc", "display_codepage",
                              get_codepage_id (display_codepage));
        mc_config_set_string (mc_main_config, "Misc", "source_codepage",
                              get_codepage_id (default_source_codepage));
        mc_config_set_string (mc_main_config, "Misc", "autodetect_codeset", autodetect_codeset);
#endif /* HAVE_CHARSET */
        mc_config_set_string (mc_main_config, "Misc", "clipboard_store", clipboard_store_path);
        mc_config_set_string (mc_main_config, "Misc", "clipboard_paste", clipboard_paste_path);

        tmp_profile = g_build_filename (mc_config_get_path (), MC_CONFIG_FILE, NULL);
        ret = mc_config_save_to_file (mc_main_config, tmp_profile, NULL);

        g_free (tmp_profile);
    }

    saving_setup = 0;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
done_setup (void)
{
    size_t i;

    g_free (clipboard_store_path);
    g_free (clipboard_paste_path);
    g_free (profile_name);
    g_free (global_profile_name);
    g_free (color_terminal_string);
    g_free (term_color_string);
    g_free (setup_color_string);
    g_free (panels_profile_name);
    mc_config_deinit (mc_main_config);
    mc_config_deinit (mc_panels_config);

    g_free (user_recent_timeformat);
    g_free (user_old_timeformat);

    for (i = 0; str_options[i].opt_name != NULL; i++)
        g_free (*str_options[i].opt_addr);

    done_hotlist ();
    done_panelize ();
    /*    directory_history_free (); */

#ifdef HAVE_CHARSET
    g_free (autodetect_codeset);
    free_codepages_list ();
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
save_config (void)
{
    char *profile;
    GError *error = NULL;
    size_t i;

    profile = g_build_filename (mc_config_get_path (), MC_CONFIG_FILE, NULL);

    /* Save integer options */
    for (i = 0; int_options[i].opt_name != NULL; i++)
        mc_config_set_int (mc_main_config, CONFIG_APP_SECTION, int_options[i].opt_name,
                           *int_options[i].opt_addr);

    /* Save string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
        mc_config_set_string (mc_main_config, CONFIG_APP_SECTION, str_options[i].opt_name,
                              *str_options[i].opt_addr);

    if (!mc_config_save_to_file (mc_main_config, profile, &error))
        setup_save_config_show_error (profile, &error);

    g_free (profile);
}

/* --------------------------------------------------------------------------------------------- */

void
setup_save_config_show_error (const char *filename, GError ** error)
{
    if (error != NULL && *error != NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot save file %s:\n%s"), filename, (*error)->message);
        g_error_free (*error);
        *error = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
save_layout (void)
{
    char *profile;
    size_t i;

    profile = g_build_filename (mc_config_get_path (), MC_CONFIG_FILE, NULL);
    /* Save integer options */
    for (i = 0; layout[i].opt_name != NULL; i++)
        mc_config_set_int (mc_main_config, "Layout", layout[i].opt_name, *layout[i].opt_addr);
    mc_config_save_to_file (mc_main_config, profile, NULL);
    g_free (profile);
}

/* --------------------------------------------------------------------------------------------- */

void
load_key_defs (void)
{
    /*
     * Load keys from mc.lib before ${XDG_CONFIG_HOME}/mc/ini, so that the user
     * definitions override global settings.
     */
    mc_config_t *mc_global_config;

    mc_global_config = mc_config_init (global_profile_name);
    if (mc_global_config != NULL)
    {
        load_keys_from_section ("general", mc_global_config);
        load_keys_from_section (getenv ("TERM"), mc_global_config);
        mc_config_deinit (mc_global_config);
    }

    load_keys_from_section ("general", mc_main_config);
    load_keys_from_section (getenv ("TERM"), mc_main_config);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_FTP
char *
load_anon_passwd (void)
{
    char *buffer;

    buffer = mc_config_get_string (mc_main_config, "Misc", "ftpfs_password", "");

    if ((buffer != NULL) && (buffer[0] != '\0'))
        return buffer;

    g_free (buffer);
    return NULL;
}
#endif /* ENABLE_VFS_FTP */

/* --------------------------------------------------------------------------------------------- */

void
load_keymap_defs (void)
{
    /*
     * Load keymap from GLOBAL_KEYMAP_FILE before ${XDG_DATA_HOME}/mc/keymap, so that the user
     * definitions override global settings.
     */
    mc_config_t *mc_global_keymap;

    mc_global_keymap = load_setup_get_keymap_profile_config ();

    if (mc_global_keymap != NULL)
    {
#ifdef USE_INTERNAL_EDIT
        editor_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("editor", editor_keymap, mc_global_keymap);
        editor_x_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("editor:xmap", editor_x_keymap, mc_global_keymap);
#endif

        viewer_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("viewer", viewer_keymap, mc_global_keymap);
        viewer_hex_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("viewer:hex", viewer_hex_keymap, mc_global_keymap);

        main_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("main", main_keymap, mc_global_keymap);
        main_x_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("main:xmap", main_x_keymap, mc_global_keymap);

        panel_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("panel", panel_keymap, mc_global_keymap);

        input_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("input", input_keymap, mc_global_keymap);

        listbox_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("listbox", listbox_keymap, mc_global_keymap);

        tree_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("tree", tree_keymap, mc_global_keymap);

        help_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("help", help_keymap, mc_global_keymap);

        dialog_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("dialog", dialog_keymap, mc_global_keymap);

#ifdef USE_DIFF_VIEW
        diff_keymap = g_array_new (TRUE, FALSE, sizeof (global_keymap_t));
        load_keymap_from_section ("diffviewer", diff_keymap, mc_global_keymap);
#endif
        mc_config_deinit (mc_global_keymap);
    }

    main_map = default_main_map;
    if (main_keymap && main_keymap->len > 0)
        main_map = (global_keymap_t *) main_keymap->data;

    main_x_map = default_main_x_map;
    if (main_x_keymap && main_x_keymap->len > 0)
        main_x_map = (global_keymap_t *) main_x_keymap->data;

    panel_map = default_panel_keymap;
    if (panel_keymap && panel_keymap->len > 0)
        panel_map = (global_keymap_t *) panel_keymap->data;

    input_map = default_input_keymap;
    if (input_keymap && input_keymap->len > 0)
        input_map = (global_keymap_t *) input_keymap->data;

    listbox_map = default_listbox_keymap;
    if (listbox_keymap && listbox_keymap->len > 0)
        listbox_map = (global_keymap_t *) listbox_keymap->data;

    tree_map = default_tree_keymap;
    if (tree_keymap && tree_keymap->len > 0)
        tree_map = (global_keymap_t *) tree_keymap->data;

    help_map = default_help_keymap;
    if (help_keymap && help_keymap->len > 0)
        help_map = (global_keymap_t *) help_keymap->data;

    dialog_map = default_dialog_keymap;
    if (dialog_keymap && dialog_keymap->len > 0)
        dialog_map = (global_keymap_t *) dialog_keymap->data;

#ifdef USE_DIFF_VIEW
    diff_map = default_diff_keymap;
    if (diff_keymap && diff_keymap->len > 0)
        diff_map = (global_keymap_t *) diff_keymap->data;
#endif

}

/* --------------------------------------------------------------------------------------------- */

void
free_keymap_defs (void)
{
#ifdef USE_INTERNAL_EDIT
    if (editor_keymap != NULL)
        g_array_free (editor_keymap, TRUE);
    if (editor_x_keymap != NULL)
        g_array_free (editor_x_keymap, TRUE);
#endif
    if (viewer_keymap != NULL)
        g_array_free (viewer_keymap, TRUE);
    if (viewer_hex_keymap != NULL)
        g_array_free (viewer_hex_keymap, TRUE);
    if (main_keymap != NULL)
        g_array_free (main_keymap, TRUE);
    if (main_x_keymap != NULL)
        g_array_free (main_x_keymap, TRUE);
    if (panel_keymap != NULL)
        g_array_free (panel_keymap, TRUE);
    if (input_keymap != NULL)
        g_array_free (input_keymap, TRUE);
    if (listbox_keymap != NULL)
        g_array_free (listbox_keymap, TRUE);
    if (tree_keymap != NULL)
        g_array_free (tree_keymap, TRUE);
    if (help_keymap != NULL)
        g_array_free (help_keymap, TRUE);
    if (dialog_keymap != NULL)
        g_array_free (dialog_keymap, TRUE);
#ifdef USE_DIFF_VIEW
    if (diff_keymap != NULL)
        g_array_free (diff_keymap, TRUE);
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
panel_load_setup (WPanel * panel, const char *section)
{
    size_t i;
    char *buffer, buffer2[BUF_TINY];

    panel->sort_info.reverse = mc_config_get_int (mc_panels_config, section, "reverse", 0);
    panel->sort_info.case_sensitive =
        mc_config_get_int (mc_panels_config, section, "case_sensitive",
                           OS_SORT_CASE_SENSITIVE_DEFAULT);
    panel->sort_info.exec_first = mc_config_get_int (mc_panels_config, section, "exec_first", 0);

    /* Load sort order */
    buffer = mc_config_get_string (mc_panels_config, section, "sort_order", "name");
    panel->sort_info.sort_field = panel_get_field_by_id (buffer);
    if (panel->sort_info.sort_field == NULL)
        panel->sort_info.sort_field = panel_get_field_by_id ("name");

    g_free (buffer);

    /* Load the listing mode */
    buffer = mc_config_get_string (mc_panels_config, section, "list_mode", "full");
    panel->list_type = list_full;
    for (i = 0; list_types[i].key != NULL; i++)
        if (g_ascii_strcasecmp (list_types[i].key, buffer) == 0)
        {
            panel->list_type = list_types[i].list_type;
            break;
        }
    g_free (buffer);

    /* User formats */
    g_free (panel->user_format);
    panel->user_format =
        mc_config_get_string (mc_panels_config, section, "user_format", DEFAULT_USER_FORMAT);

    for (i = 0; i < LIST_TYPES; i++)
    {
        g_free (panel->user_status_format[i]);
        g_snprintf (buffer2, BUF_TINY, "user_status%lld", (long long) i);
        panel->user_status_format[i] =
            mc_config_get_string (mc_panels_config, section, buffer2, DEFAULT_USER_FORMAT);
    }

    panel->user_mini_status = mc_config_get_int (mc_panels_config, section, "user_mini_status", 0);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_save_setup (struct WPanel *panel, const char *section)
{
    char buffer[BUF_TINY];
    size_t i;

    mc_config_set_int (mc_panels_config, section, "reverse", panel->sort_info.reverse);
    mc_config_set_int (mc_panels_config, section, "case_sensitive", panel->sort_info.case_sensitive);
    mc_config_set_int (mc_panels_config, section, "exec_first", panel->sort_info.exec_first);

    mc_config_set_string (mc_panels_config, section, "sort_order", panel->sort_info.sort_field->id);

    for (i = 0; list_types[i].key != NULL; i++)
        if (list_types[i].list_type == panel->list_type)
        {
            mc_config_set_string (mc_panels_config, section, "list_mode", list_types[i].key);
            break;
        }

    mc_config_set_string (mc_panels_config, section, "user_format", panel->user_format);

    for (i = 0; i < LIST_TYPES; i++)
    {
        g_snprintf (buffer, BUF_TINY, "user_status%lld", (long long) i);
        mc_config_set_string (mc_panels_config, section, buffer, panel->user_status_format[i]);
    }

    mc_config_set_int (mc_panels_config, section, "user_mini_status", panel->user_mini_status);
}

/* --------------------------------------------------------------------------------------------- */

/**
  Load panels options from [Panels] section.
*/
void
panels_load_options (void)
{
    if (mc_config_has_group (mc_main_config, CONFIG_PANELS_SECTION))
    {
        size_t i;
        int qmode;

        for (i = 0; panels_ini_options[i].opt_name != NULL; i++)
            *panels_ini_options[i].opt_addr =
                mc_config_get_bool (mc_main_config, CONFIG_PANELS_SECTION,
                                    panels_ini_options[i].opt_name,
                                    *panels_ini_options[i].opt_addr);

        qmode = mc_config_get_int (mc_main_config, CONFIG_PANELS_SECTION,
                                   "quick_search_mode", (int) panels_options.qsearch_mode);
        if (qmode < 0)
            panels_options.qsearch_mode = QSEARCH_CASE_INSENSITIVE;
        else if (qmode >= QSEARCH_NUM)
            panels_options.qsearch_mode = QSEARCH_PANEL_CASE;
        else
            panels_options.qsearch_mode = (qsearch_mode_t) qmode;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
  Save panels options in [Panels] section.
*/
void
panels_save_options (void)
{
    size_t i;

    for (i = 0; panels_ini_options[i].opt_name != NULL; i++)
        mc_config_set_bool (mc_main_config, CONFIG_PANELS_SECTION,
                            panels_ini_options[i].opt_name, *panels_ini_options[i].opt_addr);

    mc_config_set_int (mc_main_config, CONFIG_PANELS_SECTION,
                       "quick_search_mode", (int) panels_options.qsearch_mode);
}

/* --------------------------------------------------------------------------------------------- */
