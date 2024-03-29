/** \file midnight.h
 *  \brief Header: main dialog (file panel) for Midnight Commander
 */

#ifndef MC__MIDNIGHT_H
#define MC__MIDNIGHT_H

#include "lib/widget.h"

#include "panel.h"
#include "layout.h"

/* TODO: merge content of layout.h here */

/*** typedefs(not structures) and defined constants **********************************************/

#define MENU_PANEL (is_right ? right_panel : left_panel)
#define MENU_PANEL_IDX  (is_right ? 1 : 0)
#define SELECTED_IS_PANEL (get_display_type (is_right ? 1 : 0) == view_listing)

#define other_panel get_other_panel()

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WPanel;

/*** global variables defined in .c file *********************************************************/

extern Dlg_head *midnight_dlg;
extern WMenuBar *the_menubar;
extern WLabel *the_prompt;
extern WLabel *the_hint;
extern WButtonBar *the_bar;

/* Ugly hack in order to distinguish between left and right panel in menubar */
extern gboolean is_right;       /* If the selected menu was the right */

extern WPanel *left_panel;
extern WPanel *right_panel;
extern WPanel *current_panel;

/*** declarations of public functions ************************************************************/

void update_menu (void);
void midnight_set_buttonbar (WButtonBar * b);
void load_hint (gboolean force);
void change_panel (void);
void save_cwds_stat (void);
gboolean quiet_quit_cmd (void);
void do_nc (void);

/*** inline functions ****************************************************************************/

#endif /* MC__MIDNIGHT_H */
