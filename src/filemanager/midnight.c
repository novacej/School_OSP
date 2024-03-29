/* Main dialog (file panels) of the Midnight Commander
   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2009, 2010  Free Software Foundation, Inc.

   Written by: 1994, 1995, 1996, 1997 Miguel de Icaza
   1994, 1995 Janne Kukonlehto
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file main.c
 *  \brief Source: main dialog (file panels) of the Midnight Commander
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>                /* for username in xterm title */

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* For init_key() */
#include "lib/tty/mouse.h"
#include "lib/tty/win.h"        /* xterm_flag */
#include "lib/skin.h"
#include "lib/util.h"

#include "lib/vfs/mc-vfs/vfs.h" /* vfs_translate_url() */

#include "src/args.h"
#include "src/subshell.h"
#include "src/setup.h"          /* variables */
#include "src/learn.h"          /* learn_keys() */
#include "src/execute.h"        /* suspend_cmd() */
#include "src/keybind-defaults.h"
#include "src/main.h"           /* quit */

#include "option.h"             /* configure_box() */
#include "tree.h"
#include "boxes.h"              /* sort_box(), tree_box() */
#include "layout.h"
#include "cmd.h"                /* commands */
#include "hotlist.h"
#include "panelize.h"
#include "command.h"            /* cmdline */
#include "lib/keybind.h"

#include "chmod.h"
#include "chown.h"
#include "achown.h"

#ifdef USE_INTERNAL_EDIT
#include "src/editor/edit.h"
#endif

#ifdef USE_DIFF_VIEW
#include "src/diffviewer/ydiff.h"
#endif

#include "src/consaver/cons.saver.h"    /* console_flag */

#include "midnight.h"

/* TODO: merge content of layout.c here */

/*** global variables ****************************************************************************/

/* When the modes are active, left_panel, right_panel and tree_panel */
/* point to a proper data structure.  You should check with the functions */
/* get_current_type and get_other_type the types of the panels before using */
/* this pointer variables */

/* The structures for the panels */
WPanel *left_panel = NULL;
WPanel *right_panel = NULL;
/* Pointer to the selected and unselected panel */
WPanel *current_panel = NULL;

/* Set if the command is being run from the "Right" menu */
int is_right = 0;

/* The Menubar */
WMenuBar *the_menubar = NULL;
/* The widget where we draw the prompt */
WLabel *the_prompt;
/* The hint bar */
WLabel *the_hint;
/* The button bar */
WButtonBar *the_bar;

/* The dialog handle for the main program */
Dlg_head *midnight_dlg = NULL;

/*** file scope macro definitions ****************************************************************/

#ifdef HAVE_CHARSET
/*
 * Don't restrict the output on the screen manager level,
 * the translation tables take care of it.
 */
#define full_eight_bits (1)
#define eight_bit_clean (1)
#endif /* !HAVE_CHARSET */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static Menu *left_menu, *right_menu;

static gboolean ctl_x_map_enabled = FALSE;

/*** file scope functions ************************************************************************/

/** Stop MC main dialog and the current dialog if it exists.
  * Needed to provide fast exit from MC viewer or editor on shell exit */
static void
stop_dialogs (void)
{
    midnight_dlg->state = DLG_CLOSED;

    if ((top_dlg != NULL) && (top_dlg->data != NULL))
        ((Dlg_head *) top_dlg->data)->state = DLG_CLOSED;
}

/* --------------------------------------------------------------------------------------------- */

static void
treebox_cmd (void)
{
    char *sel_dir;

    sel_dir = tree_box (selection (current_panel)->fname);
    if (sel_dir)
    {
        do_cd (sel_dir, cd_exact);
        g_free (sel_dir);
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef LISTMODE_EDITOR
static void
listmode_cmd (void)
{
    char *newmode;

    if (get_current_type () != view_listing)
        return;

    newmode = listmode_edit (current_panel->user_format);
    if (!newmode)
        return;

    g_free (current_panel->user_format);
    current_panel->list_type = list_user;
    current_panel->user_format = newmode;
    set_panel_formats (current_panel);

    do_refresh ();
}
#endif /* LISTMODE_EDITOR */

/* --------------------------------------------------------------------------------------------- */

static GList *
create_panel_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("File listin&g"), CK_ListingCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Quick view"), CK_QuickViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Info"), CK_InfoCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Tree"), CK_TreeCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries =
        g_list_append (entries, menu_entry_create (_("&Listing mode..."), CK_ChangeListingCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Sort order..."), CK_Sort));
    entries = g_list_append (entries, menu_entry_create (_("&Filter..."), CK_FilterCmd));
#ifdef HAVE_CHARSET
    entries =
        g_list_append (entries, menu_entry_create (_("&Encoding..."), CK_PanelSetPanelEncoding));
#endif
#ifdef ENABLE_VFS_NET
    entries = g_list_append (entries, menu_separator_create ());
#ifdef ENABLE_VFS_FTP
    entries = g_list_append (entries, menu_entry_create (_("FT&P link..."), CK_FtplinkCmd));
#endif
#ifdef ENABLE_VFS_FISH
    entries = g_list_append (entries, menu_entry_create (_("S&hell link..."), CK_FishlinkCmd));
#endif
#ifdef ENABLE_VFS_SMB
    entries = g_list_append (entries, menu_entry_create (_("SM&B link..."), CK_SmblinkCmd));
#endif
#endif /* ENABLE_VFS_NET */
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Rescan"), CK_RereadCmd));

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_file_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&View"), CK_ViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("Vie&w file..."), CK_ViewFileCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Filtered view"), CK_FilteredViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Edit"), CK_EditCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Copy"), CK_CopyCmd));
    entries = g_list_append (entries, menu_entry_create (_("C&hmod"), CK_ChmodCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Link"), CK_LinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Symlink"), CK_SymlinkCmd));
    entries =
        g_list_append (entries, menu_entry_create (_("Relative symlin&k"), CK_RelativeSymlinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("Edit s&ymlink"), CK_EditSymlinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("Ch&own"), CK_ChownCmd));
    entries =
        g_list_append (entries, menu_entry_create (_("&Advanced chown"), CK_ChownAdvancedCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Rename/Move"), CK_RenameCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Mkdir"), CK_MkdirCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Delete"), CK_DeleteCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Quick cd"), CK_QuickCdCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Select &group"), CK_SelectCmd));
    entries = g_list_append (entries, menu_entry_create (_("U&nselect group"), CK_UnselectCmd));
    entries =
        g_list_append (entries,
                       menu_entry_create (_("Reverse selec&tion"), CK_ReverseSelectionCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("E&xit"), CK_QuitCmd));

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_command_menu (void)
{
    /* I know, I'm lazy, but the tree widget when it's not running
     * as a panel still has some problems, I have not yet finished
     * the WTree widget port, sorry.
     */
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&User menu"), CK_UserMenuCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Directory tree"), CK_TreeBoxCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Find file"), CK_FindCmd));
    entries = g_list_append (entries, menu_entry_create (_("S&wap panels"), CK_SwapCmd));
    entries =
        g_list_append (entries, menu_entry_create (_("Switch &panels on/off"), CK_ShowCommandLine));
    entries =
        g_list_append (entries, menu_entry_create (_("&Compare directories"), CK_CompareDirsCmd));
#ifdef USE_DIFF_VIEW
    entries = g_list_append (entries, menu_entry_create (_("&View diff files"), CK_DiffViewCmd));
#endif
    entries =
        g_list_append (entries, menu_entry_create (_("E&xternal panelize"), CK_ExternalPanelize));
    entries =
        g_list_append (entries,
                       menu_entry_create (_("Show directory s&izes"), CK_SingleDirsizeCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Command &history"), CK_HistoryCmd));
    entries =
        g_list_append (entries, menu_entry_create (_("Di&rectory hotlist"), CK_QuickChdirCmd));
#ifdef ENABLE_VFS
    entries = g_list_append (entries, menu_entry_create (_("&Active VFS list"), CK_ReselectVfs));
#endif
#ifdef WITH_BACKGROUND
    entries = g_list_append (entries, menu_entry_create (_("&Background jobs"), CK_JobsCmd));
#endif
    entries = g_list_append (entries, menu_entry_create (_("Screen lis&t"), CK_DialogListCmd));
    entries = g_list_append (entries, menu_separator_create ());
#ifdef ENABLE_VFS_UNDELFS
    entries =
        g_list_append (entries,
                       menu_entry_create (_("&Undelete files (ext2fs only)"), CK_UndeleteCmd));
#endif
#ifdef LISTMODE_EDITOR
    entries =
        g_list_append (entries, menu_entry_create (_("&Listing format edit"), CK_ListmodeCmd));
#endif
#if defined (ENABLE_VFS_UNDELFS) || defined (LISTMODE_EDITOR)
    entries = g_list_append (entries, menu_separator_create ());
#endif
    entries =
        g_list_append (entries, menu_entry_create (_("Edit &extension file"), CK_EditExtFileCmd));
    entries = g_list_append (entries, menu_entry_create (_("Edit &menu file"), CK_EditMcMenuCmd));
    entries =
        g_list_append (entries,
                       menu_entry_create (_("Edit hi&ghlighting group file"), CK_EditFhlFileCmd));

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_options_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Configuration..."), CK_ConfigureBox));
    entries = g_list_append (entries, menu_entry_create (_("&Layout..."), CK_LayoutBox));
    entries =
        g_list_append (entries, menu_entry_create (_("&Panel options..."), CK_PanelOptionsBox));
    entries = g_list_append (entries, menu_entry_create (_("C&onfirmation..."), CK_ConfirmBox));
    entries = g_list_append (entries, menu_entry_create (_("&Display bits..."), CK_DisplayBitsBox));
    entries = g_list_append (entries, menu_entry_create (_("Learn &keys..."), CK_LearnKeys));
#ifdef ENABLE_VFS
    entries = g_list_append (entries, menu_entry_create (_("&Virtual FS..."), CK_ConfigureVfs));
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save setup"), CK_SaveSetupCmd));

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_menu (void)
{
    left_menu = create_menu ("", create_panel_menu (), "[Left and Right Menus]");
    menubar_add_menu (the_menubar, left_menu);
    menubar_add_menu (the_menubar, create_menu (_("&File"), create_file_menu (), "[File Menu]"));
    menubar_add_menu (the_menubar,
                      create_menu (_("&Command"), create_command_menu (), "[Command Menu]"));
    menubar_add_menu (the_menubar,
                      create_menu (_("&Options"), create_options_menu (), "[Options Menu]"));
    right_menu = create_menu ("", create_panel_menu (), "[Left and Right Menus]");
    menubar_add_menu (the_menubar, right_menu);
    update_menu ();
}

/* --------------------------------------------------------------------------------------------- */

static void
menu_last_selected_cmd (void)
{
    the_menubar->is_active = TRUE;
    the_menubar->is_dropped = (drop_menus != 0);
    the_menubar->previous_widget = dlg_get_current_widget_id (midnight_dlg);
    dlg_select_widget (the_menubar);
}

/* --------------------------------------------------------------------------------------------- */

static void
menu_cmd (void)
{
    if (the_menubar->is_active)
        return;

    if ((get_current_index () == 0) == (current_panel->active != 0))
        the_menubar->selected = 0;
    else
        the_menubar->selected = g_list_length (the_menubar->menu) - 1;
    menu_last_selected_cmd ();
}

/* --------------------------------------------------------------------------------------------- */

static void
sort_cmd (void)
{
    WPanel *p;
    const panel_field_t *sort_order;

    if (!SELECTED_IS_PANEL)
        return;

    p = MENU_PANEL;
    sort_order = sort_box (&p->sort_info);
    panel_set_sort_order (p, sort_order);
}

/* --------------------------------------------------------------------------------------------- */

static char *
midnight_get_shortcut (unsigned long command)
{
    const char *ext_map;
    const char *shortcut = NULL;

    shortcut = keybind_lookup_keymap_shortcut (main_map, command);
    if (shortcut != NULL)
        return g_strdup (shortcut);

    shortcut = keybind_lookup_keymap_shortcut (panel_map, command);
    if (shortcut != NULL)
        return g_strdup (shortcut);

    ext_map = keybind_lookup_keymap_shortcut (main_map, CK_StartExtMap1);
    if (ext_map != NULL)
        shortcut = keybind_lookup_keymap_shortcut (main_x_map, command);
    if (shortcut != NULL)
        return g_strdup_printf ("%s %s", ext_map, shortcut);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
midnight_get_title (const Dlg_head * h, size_t len)
{
    /* TODO: share code with update_xterm_title_path() */

    const char *path;
    char host[BUF_TINY];
    char *p;
    struct passwd *pw = NULL;
    char *login = NULL;
    int res = 0;

    (void) h;

    path = strip_home_and_password (current_panel->cwd);
    res = gethostname (host, sizeof (host));
    if (res != 0)
        host[0] = '\0';
    else
        host[sizeof (host) - 1] = '\0';

    pw = getpwuid (getuid ());
    if (pw != NULL)
        login = g_strdup_printf ("%s@%s", pw->pw_name, host);
    else
        login = g_strdup (host);

    p = g_strdup_printf ("%s [%s]:%s", _("Panels:"), login, path);
    path = str_trunc (p, len - 4);
    g_free (login);
    g_free (p);

    return g_strdup (path);
}

/* --------------------------------------------------------------------------------------------- */

static void
toggle_panels_split (void)
{
    horizontal_split = !horizontal_split;
    layout_change ();
    do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Just a hack for allowing url-like pathnames to be accepted from the
 * command line.
 */
static void
translated_mc_chdir (char *dir)
{
    char *newdir;
    int ret;

    newdir = vfs_translate_url (dir);
    ret = mc_chdir (newdir);
    g_free (newdir);
}

/* --------------------------------------------------------------------------------------------- */

static void
create_panels (void)
{
    int current_index;
    int other_index;
    panel_view_mode_t current_mode, other_mode;
    char original_dir[BUF_1K] = "\0";

    if (boot_current_is_left)
    {
        current_index = 0;
        other_index = 1;
        current_mode = startup_left_mode;
        other_mode = startup_right_mode;
    }
    else
    {
        current_index = 1;
        other_index = 0;
        current_mode = startup_right_mode;
        other_mode = startup_left_mode;
    }
    /* Creates the left panel */
    if (mc_run_param0 != NULL)
    {
        if (mc_run_param1 != NULL)
        {
            /* Ok, user has specified two dirs, save the original one,
             * since we may not be able to chdir to the proper
             * second directory later
             */
            mc_get_current_wd (original_dir, sizeof (original_dir) - 2);
        }
        translated_mc_chdir (mc_run_param0);
    }
    set_display_type (current_index, current_mode);

    /* The other panel */
    if (mc_run_param1 != NULL)
    {
        if (original_dir[0] != '\0')
            translated_mc_chdir (original_dir);
        translated_mc_chdir (mc_run_param1);
    }
    set_display_type (other_index, other_mode);

    if (startup_left_mode == view_listing)
    {
        current_panel = left_panel;
    }
    else
    {
        if (right_panel)
            current_panel = right_panel;
        else
            current_panel = left_panel;
    }

    /* Create the nice widgets */
    cmdline = command_new (0, 0, 0);
    the_prompt = label_new (0, 0, mc_prompt);
    the_prompt->transparent = 1;
    the_bar = buttonbar_new (keybar_visible);

    the_hint = label_new (0, 0, 0);
    the_hint->transparent = 1;
    the_hint->auto_adjust_cols = 0;
    the_hint->widget.cols = COLS;

    the_menubar = menubar_new (0, 0, COLS, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_current_pathname (void)
{
    char *cwd_path;
    if (!command_prompt)
        return;

    cwd_path = remove_encoding_from_path (current_panel->cwd);
    command_insert (cmdline, cwd_path, FALSE);

    if (cwd_path[strlen (cwd_path) - 1] != PATH_SEP)
        command_insert (cmdline, PATH_SEP_STR, FALSE);
    g_free (cwd_path);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_other_pathname (void)
{
    char *cwd_path;

    if (get_other_type () != view_listing)
        return;

    if (!command_prompt)
        return;

    cwd_path = remove_encoding_from_path (other_panel->cwd);
    command_insert (cmdline, cwd_path, FALSE);

    if (cwd_path[strlen (cwd_path) - 1] != PATH_SEP)
        command_insert (cmdline, PATH_SEP_STR, FALSE);
    g_free (cwd_path);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_readlink (WPanel * panel)
{
    if (!command_prompt)
        return;
    if (S_ISLNK (selection (panel)->st.st_mode))
    {
        char buffer[MC_MAXPATHLEN];
        char *p = concat_dir_and_file (panel->cwd, selection (panel)->fname);
        int i;

        i = mc_readlink (p, buffer, MC_MAXPATHLEN - 1);
        g_free (p);
        if (i > 0)
        {
            buffer[i] = 0;
            command_insert (cmdline, buffer, TRUE);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_current_readlink (void)
{
    copy_readlink (current_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_other_readlink (void)
{
    if (get_other_type () == view_listing)
        copy_readlink (other_panel);
}

/* --------------------------------------------------------------------------------------------- */

/** Insert the selected file name into the input line */
static void
copy_prog_name (void)
{
    char *tmp;
    if (!command_prompt)
        return;

    if (get_current_type () == view_tree)
    {
        WTree *tree = (WTree *) get_panel_widget (get_current_index ());
        tmp = tree_selected_name (tree);
    }
    else
        tmp = selection (current_panel)->fname;

    command_insert (cmdline, tmp, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_tagged (WPanel * panel)
{
    int i;

    if (!command_prompt)
        return;
    input_disable_update (cmdline);
    if (panel->marked)
    {
        for (i = 0; i < panel->count; i++)
        {
            if (panel->dir.list[i].f.marked)
                command_insert (cmdline, panel->dir.list[i].fname, TRUE);
        }
    }
    else
    {
        command_insert (cmdline, panel->dir.list[panel->selected].fname, TRUE);
    }
    input_enable_update (cmdline);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_current_tagged (void)
{
    copy_tagged (current_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_other_tagged (void)
{
    if (get_other_type () == view_listing)
        copy_tagged (other_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
ctl_x_cmd (void)
{
    ctl_x_map_enabled = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_xterm_support (void)
{
    const char *termvalue;

    termvalue = getenv ("TERM");
    if (!termvalue || !(*termvalue))
    {
        fputs (_("The TERM environment variable is unset!\n"), stderr);
        exit (EXIT_FAILURE);
    }

    /* Check mouse capabilities */
    xmouse_seq = tty_tgetstr ("Km");

    if (strcmp (termvalue, "cygwin") == 0)
    {
        mc_args__force_xterm = 1;
        use_mouse_p = MOUSE_DISABLED;
    }

    if (mc_args__force_xterm || strncmp (termvalue, "xterm", 5) == 0
        || strncmp (termvalue, "konsole", 7) == 0
        || strncmp (termvalue, "rxvt", 4) == 0
        || strcmp (termvalue, "Eterm") == 0 || strcmp (termvalue, "dtterm") == 0)
    {
        xterm_flag = 1;

        /* Default to the standard xterm sequence */
        if (!xmouse_seq)
        {
            xmouse_seq = ESC_STR "[M";
        }

        /* Enable mouse unless explicitly disabled by --nomouse */
        if (use_mouse_p != MOUSE_DISABLED)
        {
            const char *color_term = getenv ("COLORTERM");
            if (strncmp (termvalue, "rxvt", 4) == 0 ||
                (color_term != NULL && strncmp (color_term, "rxvt", 4) == 0) ||
                strcmp (termvalue, "Eterm") == 0)
            {
                use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
            }
            else
            {
                use_mouse_p = MOUSE_XTERM_BUTTON_EVENT_TRACKING;
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
setup_mc (void)
{
#ifdef HAVE_SLANG
    tty_display_8bit (full_eight_bits != 0);
#else
    tty_display_8bit (eight_bit_clean != 0);
#endif

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell)
        add_select_channel (subshell_pty, load_prompt, 0);
#endif /* !HAVE_SUBSHELL_SUPPORT */

    tty_setup_sigwinch (sigwinch_handler);

    if ((tty_baudrate () < 9600) || tty_is_slow ())
        verbose = 0;

    init_xterm_support ();
    init_mouse ();
}

/* --------------------------------------------------------------------------------------------- */

static void
setup_dummy_mc (void)
{
    char d[MC_MAXPATHLEN];
    int ret;

    mc_get_current_wd (d, MC_MAXPATHLEN);
    setup_mc ();
    ret = mc_chdir (d);
}

/* --------------------------------------------------------------------------------------------- */

static void
done_screen (void)
{
    if ((quit & SUBSHELL_EXIT) == 0)
        clr_scr ();
    tty_reset_shell_mode ();
    tty_noraw_mode ();
    tty_keypad (FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
done_mc (void)
{
    disable_mouse ();

    /* Setup shutdown
     *
     * We sync the profiles since the hotlist may have changed, while
     * we only change the setup data if we have the auto save feature set
     */

    save_setup (auto_save_setup, panels_options.auto_save_setup);
    done_screen ();

    vfs_stamp_path (vfs_get_current_dir ());

    if ((current_panel != NULL) && (get_current_type () == view_listing))
        vfs_stamp_path (current_panel->cwd);

    if ((other_panel != NULL) && (get_other_type () == view_listing))
        vfs_stamp_path (other_panel->cwd);
}

/* --------------------------------------------------------------------------------------------- */

static void
create_panels_and_run_mc (void)
{
    midnight_dlg->get_shortcut = midnight_get_shortcut;
    midnight_dlg->get_title = midnight_get_title;

    create_panels ();

    add_widget (midnight_dlg, the_menubar);
    init_menu ();

    add_widget (midnight_dlg, get_panel_widget (0));
    add_widget (midnight_dlg, get_panel_widget (1));

    add_widget (midnight_dlg, the_hint);
    add_widget (midnight_dlg, cmdline);
    add_widget (midnight_dlg, the_prompt);

    add_widget (midnight_dlg, the_bar);
    midnight_set_buttonbar (the_bar);

    /* Run the Midnight Commander if no file was specified in the command line */
    run_dlg (midnight_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/** result must be free'd (I think this should go in util.c) */
static char *
prepend_cwd_on_local (const char *filename)
{
    char *d;
    size_t l;

    if (!vfs_file_is_local (filename) || g_path_is_absolute (filename))
        return g_strdup (filename);

    d = g_malloc (MC_MAXPATHLEN + strlen (filename) + 2);
    mc_get_current_wd (d, MC_MAXPATHLEN);
    l = strlen (d);
    d[l++] = PATH_SEP;
    strcpy (d + l, filename);
    canonicalize_pathname (d);
    return d;
}

/* --------------------------------------------------------------------------------------------- */

/** Invoke the internal view/edit routine with:
 * the default processing and forcing the internal viewer/editor
 */
static void
mc_maybe_editor_or_viewer (void)
{
    switch (mc_run_mode)
    {
#ifdef USE_INTERNAL_EDIT
    case MC_RUN_EDITOR:
        edit_file (mc_run_param0, mc_args__edit_start_line);
        break;
#endif /* USE_INTERNAL_EDIT */
    case MC_RUN_VIEWER:
        {
            char *path;
            path = prepend_cwd_on_local (mc_run_param0);
            view_file (path, 0, 1);
            g_free (path);
            break;
        }
#ifdef USE_DIFF_VIEW
    case MC_RUN_DIFFVIEWER:
        diff_view (mc_run_param0, mc_run_param1, mc_run_param0, mc_run_param1);
        break;
#endif /* USE_DIFF_VIEW */
    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
quit_cmd_internal (int quiet)
{
    int q = quit;
    size_t n;

    n = dialog_switch_num () - 1;
    if (n != 0)
    {
        char msg[BUF_MEDIUM];

        g_snprintf (msg, sizeof (msg),
                    ngettext ("You have %zd opened screen. Quit anyway?",
                              "You have %zd opened screens. Quit anyway?", n), n);

        if (query_dialog (_("The Midnight Commander"), msg, D_NORMAL, 2, _("&Yes"), _("&No")) != 0)
            return FALSE;
        q = 1;
    }
    else if (quiet || !confirm_exit)
        q = 1;
    else if (query_dialog (_("The Midnight Commander"),
                           _("Do you really want to quit the Midnight Commander?"),
                           D_NORMAL, 2, _("&Yes"), _("&No")) == 0)
        q = 1;

    if (q != 0)
    {
#ifdef HAVE_SUBSHELL_SUPPORT
        if (!use_subshell)
            stop_dialogs ();
        else if ((q = exit_subshell ()))
#endif
            stop_dialogs ();
    }

    if (q != 0)
        quit |= 1;
    return (quit != 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
quit_cmd (void)
{
    return quit_cmd_internal (0);
}

/* --------------------------------------------------------------------------------------------- */

static void
toggle_show_hidden (void)
{
    panels_options.show_dot_files = !panels_options.show_dot_files;
    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Repaint the contents of the panels without frames.  To schedule panel
 * for repainting, set panel->dirty to 1.  There are many reasons why
 * the panels need to be repainted, and this is a costly operation, so
 * it's done once per event.
 */

static void
update_dirty_panels (void)
{
    if (get_current_type () == view_listing && current_panel->dirty)
        send_message ((Widget *) current_panel, WIDGET_DRAW, 0);

    if (get_other_type () == view_listing && other_panel->dirty)
        send_message ((Widget *) other_panel, WIDGET_DRAW, 0);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
midnight_execute_cmd (Widget * sender, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    (void) sender;

    /* stop quick search before executing any command */
    send_message ((Widget *) current_panel, WIDGET_COMMAND, CK_PanelStopSearch);

    switch (command)
    {
    case CK_AddHotlist:
        add2hotlist_cmd ();
        break;
    case CK_ChangeListingCmd:
        change_listing_cmd ();
        break;
    case CK_ChmodCmd:
        chmod_cmd ();
        break;
    case CK_ChownCmd:
        chown_cmd ();
        break;
    case CK_ChownAdvancedCmd:
        chown_advanced_cmd ();
        break;
    case CK_CompareDirsCmd:
        compare_dirs_cmd ();
        break;
    case CK_ConfigureBox:
        configure_box ();
        break;
#ifdef ENABLE_VFS
    case CK_ConfigureVfs:
        configure_vfs ();
        break;
#endif
    case CK_ConfirmBox:
        confirm_box ();
        break;
    case CK_CopyCmd:
        copy_cmd ();
        break;
    case CK_CopyCurrentPathname:
        copy_current_pathname ();
        break;
    case CK_CopyCurrentReadlink:
        copy_current_readlink ();
        break;
    case CK_CopyCurrentTagged:
        copy_current_tagged ();
        break;
    case CK_CopyOtherPathname:
        copy_other_pathname ();
        break;
    case CK_CopyOtherReadlink:
        copy_other_readlink ();
        break;
    case CK_CopyOtherTagged:
        copy_other_tagged ();
        break;
    case CK_DeleteCmd:
        delete_cmd ();
        break;
    case CK_DialogListCmd:
        dialog_switch_list ();
        break;
#ifdef USE_DIFF_VIEW
    case CK_DiffViewCmd:
        diff_view_cmd ();
        break;
#endif
    case CK_DisplayBitsBox:
        display_bits_box ();
        break;
    case CK_EditCmd:
        edit_cmd ();
        break;
#ifdef USE_INTERNAL_EDIT
    case CK_EditForceInternalCmd:
        edit_cmd_force_internal ();
        break;
#endif
    case CK_EditExtFileCmd:
        ext_cmd ();
        break;
    case CK_EditFhlFileCmd:
        edit_fhl_cmd ();
        break;
    case CK_EditMcMenuCmd:
        edit_mc_menu_cmd ();
        break;
    case CK_EditSymlinkCmd:
        edit_symlink_cmd ();
        break;
    case CK_ExternalPanelize:
        external_panelize ();
        break;
    case CK_FilterCmd:
        filter_cmd ();
        break;
    case CK_FilteredViewCmd:
        filtered_view_cmd ();
        break;
    case CK_FindCmd:
        find_cmd ();
        break;
#ifdef ENABLE_VFS_FISH
    case CK_FishlinkCmd:
        fishlink_cmd ();
        break;
#endif
#ifdef ENABLE_VFS_FTP
    case CK_FtplinkCmd:
        ftplink_cmd ();
        break;
#endif
    case CK_HelpCmd:
        help_cmd ();
        break;
    case CK_HistoryCmd:
        history_cmd ();
        break;
    case CK_InfoCmd:
        if (sender == (Widget *) the_menubar)
            info_cmd ();        /* mwnu */
        else
            info_cmd_no_menu ();        /* shortcut or buttonbar */
        break;
#ifdef WITH_BACKGROUND
    case CK_JobsCmd:
        jobs_cmd ();
        break;
#endif
    case CK_LayoutBox:
        layout_box ();
        break;
    case CK_LearnKeys:
        learn_keys ();
        break;
    case CK_LinkCmd:
        link_cmd (LINK_HARDLINK);
        break;
    case CK_ListingCmd:
        listing_cmd ();
        break;
#ifdef LISTMODE_EDITOR
    case CK_ListmodeCmd:
        listmode_cmd ();
        break;
#endif
    case CK_MenuCmd:
        menu_cmd ();
        break;
    case CK_MenuLastSelectedCmd:
        menu_last_selected_cmd ();
        break;
    case CK_MkdirCmd:
        mkdir_cmd ();
        break;
    case CK_PanelOptionsBox:
        panel_options_box ();
        break;
#ifdef HAVE_CHARSET
    case CK_PanelSetPanelEncoding:
        encoding_cmd ();
        break;
#endif
    case CK_QuickCdCmd:
        quick_cd_cmd ();
        break;
    case CK_QuickChdirCmd:
        quick_chdir_cmd ();
        break;
    case CK_QuickViewCmd:
        if (sender == (Widget *) the_menubar)
            quick_view_cmd ();  /* menu */
        else
            quick_cmd_no_menu ();       /* shortcut or buttonabr */
        break;
    case CK_QuietQuitCmd:
        quiet_quit_cmd ();
        break;
    case CK_QuitCmd:
        quit_cmd ();
        break;
    case CK_RelativeSymlinkCmd:
        link_cmd (LINK_SYMLINK_RELATIVE);
        break;
    case CK_RenameCmd:
        rename_cmd ();
        break;
    case CK_RereadCmd:
        reread_cmd ();
        break;
#ifdef ENABLE_VFS
    case CK_ReselectVfs:
        reselect_vfs ();
        break;
#endif
    case CK_ReverseSelectionCmd:
        reverse_selection_cmd ();
        break;
    case CK_SaveSetupCmd:
        save_setup_cmd ();
        break;
    case CK_SelectCmd:
        select_cmd ();
        break;
    case CK_ShowCommandLine:
        view_other_cmd ();
        break;
    case CK_SingleDirsizeCmd:
        smart_dirsize_cmd ();
        break;
#ifdef ENABLE_VFS_SMB
    case CK_SmblinkCmd:
        smblink_cmd ();
        break;
#endif /* ENABLE_VFS_SMB */
    case CK_Sort:
        sort_cmd ();
        break;
    case CK_StartExtMap1:
        ctl_x_cmd ();
        break;
    case CK_SuspendCmd:
        suspend_cmd ();
        break;
    case CK_SwapCmd:
        swap_cmd ();
        break;
    case CK_SymlinkCmd:
        link_cmd (LINK_SYMLINK_ABSOLUTE);
        break;
    case CK_ToggleListingCmd:
        toggle_listing_cmd ();
        break;
    case CK_ToggleShowHidden:
        toggle_show_hidden ();
        break;
    case CK_TogglePanelsSplit:
        toggle_panels_split ();
        break;
    case CK_TreeCmd:
        tree_cmd ();
        break;
    case CK_TreeBoxCmd:
        treebox_cmd ();
        break;
#ifdef ENABLE_VFS_UNDELFS
    case CK_UndeleteCmd:
        undelete_cmd ();
        break;
#endif
    case CK_UnselectCmd:
        unselect_cmd ();
        break;
    case CK_UserMenuCmd:
        user_file_menu_cmd ();
        break;
    case CK_ViewCmd:
        view_cmd ();
        break;
    case CK_ViewFileCmd:
        view_file_cmd ();
        break;
    default:
        res = MSG_NOT_HANDLED;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
midnight_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    unsigned long command;

    switch (msg)
    {
    case DLG_INIT:
        panel_init ();
        setup_panels ();
        return MSG_HANDLED;

    case DLG_DRAW:
        load_hint (1);
        /* We handle the special case of the output lines */
        if (console_flag && output_lines)
            show_console_contents (output_start_y,
                                   LINES - output_lines - keybar_visible -
                                   1, LINES - keybar_visible - 1);
        return MSG_HANDLED;

    case DLG_RESIZE:
        setup_panels ();
        menubar_arrange (the_menubar);
        return MSG_HANDLED;

    case DLG_IDLE:
        /* We only need the first idle event to show user menu after start */
        set_idle_proc (h, 0);

        if (boot_current_is_left)
            dlg_select_widget (get_panel_widget (0));
        else
            dlg_select_widget (get_panel_widget (1));

        if (auto_menu)
            midnight_execute_cmd (NULL, CK_UserMenuCmd);
        return MSG_HANDLED;

    case DLG_KEY:
        if (ctl_x_map_enabled)
        {
            ctl_x_map_enabled = FALSE;
            command = keybind_lookup_keymap_command (main_x_map, parm);
            if (command != CK_Ignore_Key)
                return midnight_execute_cmd (NULL, command);
        }

        /* FIXME: should handle all menu shortcuts before this point */
        if (the_menubar->is_active)
            return MSG_NOT_HANDLED;

        if (parm == '\t')
            input_free_completions (cmdline);

        if (parm == '\n')
        {
            size_t i;

            for (i = 0; cmdline->buffer[i] != '\0' &&
                 (cmdline->buffer[i] == ' ' || cmdline->buffer[i] == '\t'); i++)
                ;

            if (cmdline->buffer[i] != '\0')
            {
                send_message ((Widget *) cmdline, WIDGET_KEY, parm);
                return MSG_HANDLED;
            }

            input_insert (cmdline, "", FALSE);
            cmdline->point = 0;
        }

        /* Ctrl-Enter and Alt-Enter */
        if (((parm & ~(KEY_M_CTRL | KEY_M_ALT)) == '\n') && (parm & (KEY_M_CTRL | KEY_M_ALT)))
        {
            copy_prog_name ();
            return MSG_HANDLED;
        }

        /* Ctrl-Shift-Enter */
        if (parm == (KEY_M_CTRL | KEY_M_SHIFT | '\n'))
        {
            copy_current_pathname ();
            copy_prog_name ();
            return MSG_HANDLED;
        }

        if ((!alternate_plus_minus || !(console_flag || xterm_flag))
            && !quote && !current_panel->searching)
        {
            if (!only_leading_plus_minus)
            {
                /* Special treatement, since the input line will eat them */
                if (parm == '+')
                {
                    select_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '\\' || parm == '-')
                {
                    unselect_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '*')
                {
                    reverse_selection_cmd ();
                    return MSG_HANDLED;
                }
            }
            else if (!command_prompt || !cmdline->buffer[0])
            {
                /* Special treatement '+', '-', '\', '*' only when this is
                 * first char on input line
                 */

                if (parm == '+')
                {
                    select_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '\\' || parm == '-')
                {
                    unselect_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '*')
                {
                    reverse_selection_cmd ();
                    return MSG_HANDLED;
                }
            }
        }
        return MSG_NOT_HANDLED;

    case DLG_HOTKEY_HANDLED:
        if ((get_current_type () == view_listing) && current_panel->searching)
        {
            current_panel->dirty = 1;   /* FIXME: unneeded? */
            send_message ((Widget *) current_panel, WIDGET_COMMAND, CK_PanelStopSearch);
        }
        return MSG_HANDLED;

    case DLG_UNHANDLED_KEY:
        if (command_prompt)
        {
            cb_ret_t v;

            v = send_message ((Widget *) cmdline, WIDGET_KEY, parm);
            if (v == MSG_HANDLED)
                return MSG_HANDLED;
        }

        if (ctl_x_map_enabled)
        {
            ctl_x_map_enabled = FALSE;
            command = keybind_lookup_keymap_command (main_x_map, parm);
        }
        else
            command = keybind_lookup_keymap_command (main_map, parm);

        return (command == CK_Ignore_Key) ? MSG_NOT_HANDLED : midnight_execute_cmd (NULL, command);

    case DLG_POST_KEY:
        if (!the_menubar->is_active)
            update_dirty_panels ();
        return MSG_HANDLED;

    case DLG_ACTION:
        /* shortcut */
        if (sender == NULL)
            midnight_execute_cmd (NULL, parm);
        /* message from menu */
        else if (sender == (Widget *) the_menubar)
            midnight_execute_cmd (sender, parm);
        /* message from buttonbar */
        else if (sender == (Widget *) the_bar)
        {
            if (data == NULL)
                midnight_execute_cmd (sender, parm);
            else
                send_message ((Widget *) data, WIDGET_COMMAND, parm);
        }
        return MSG_HANDLED;

    case DLG_END:
        panel_deinit ();
        return MSG_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
update_menu (void)
{
    menu_set_name (left_menu, horizontal_split ? _("&Above") : _("&Left"));
    menu_set_name (right_menu, horizontal_split ? _("&Below") : _("&Right"));
    menubar_arrange (the_menubar);
    menubar_set_visible (the_menubar, menubar_visible);
}

void
midnight_set_buttonbar (WButtonBar * b)
{
    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), main_map, NULL);
    buttonbar_set_label (b, 2, Q_ ("ButtonBar|Menu"), main_map, NULL);
    buttonbar_set_label (b, 3, Q_ ("ButtonBar|View"), main_map, NULL);
    buttonbar_set_label (b, 4, Q_ ("ButtonBar|Edit"), main_map, NULL);
    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Copy"), main_map, NULL);
    buttonbar_set_label (b, 6, Q_ ("ButtonBar|RenMov"), main_map, NULL);
    buttonbar_set_label (b, 7, Q_ ("ButtonBar|Mkdir"), main_map, NULL);
    buttonbar_set_label (b, 8, Q_ ("ButtonBar|Delete"), main_map, NULL);
    buttonbar_set_label (b, 9, Q_ ("ButtonBar|PullDn"), main_map, NULL);
    buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), main_map, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load new hint and display it.
 * IF force is not 0, ignore the timeout.
 */

void
load_hint (gboolean force)
{
    char *hint;

    if (the_hint->widget.owner == NULL)
        return;

    if (!message_visible)
    {
        label_set_text (the_hint, NULL);
        return;
    }

    hint = get_random_hint (force);

    if (hint != NULL)
    {
        if (*hint != '\0')
            set_hintbar (hint);
        g_free (hint);
    }
    else
    {
        char text[BUF_SMALL];

        g_snprintf (text, sizeof (text), _("GNU Midnight Commander %s\n"), VERSION);
        set_hintbar (text);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
change_panel (void)
{
    input_free_completions (cmdline);
    dlg_one_down (midnight_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/** Save current stat of directories to avoid reloading the panels
 * when no modifications have taken place
 */
void
save_cwds_stat (void)
{
    if (panels_options.fast_reload)
    {
        mc_stat (current_panel->cwd, &(current_panel->dir_stat));
        if (get_other_type () == view_listing)
            mc_stat (other_panel->cwd, &(other_panel->dir_stat));
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
quiet_quit_cmd (void)
{
    print_last_revert = TRUE;
    return quit_cmd_internal (1);
}

/* --------------------------------------------------------------------------------------------- */

/** Run the main dialog that occupies the whole screen */
void
do_nc (void)
{
    dlg_colors_t midnight_colors;

    midnight_colors[DLG_COLOR_NORMAL] = mc_skin_color_get ("dialog", "_default_");
    midnight_colors[DLG_COLOR_FOCUS] = mc_skin_color_get ("dialog", "focus");
    midnight_colors[DLG_COLOR_HOT_NORMAL] = mc_skin_color_get ("dialog", "hotnormal");
    midnight_colors[DLG_COLOR_HOT_FOCUS] = mc_skin_color_get ("dialog", "hotfocus");
    midnight_colors[DLG_COLOR_TITLE] = mc_skin_color_get ("dialog", "title");

#ifdef USE_INTERNAL_EDIT
    edit_stack_init ();
#endif

    midnight_dlg = create_dlg (FALSE, 0, 0, LINES, COLS, midnight_colors, midnight_callback,
                               "[main]", NULL, DLG_WANT_IDLE);

    if (mc_run_mode == MC_RUN_FULL)
        setup_mc ();
    else
        setup_dummy_mc ();

    /* Check if we were invoked as an editor or file viewer */
    if (mc_run_mode != MC_RUN_FULL)
        mc_maybe_editor_or_viewer ();
    else
    {
        create_panels_and_run_mc ();

        /* destroy_dlg destroys even current_panel->cwd, so we have to save a copy :) */
        if (mc_args__last_wd_file != NULL && vfs_current_is_local ())
            last_wd_string = g_strdup (current_panel->cwd);
    }

    /* Program end */
    midnight_shutdown = 1;
    dialog_switch_shutdown ();
    done_mc ();
    destroy_dlg (midnight_dlg);
    current_panel = NULL;

#ifdef USE_INTERNAL_EDIT
    edit_stack_free ();
#endif
}

/* --------------------------------------------------------------------------------------------- */
