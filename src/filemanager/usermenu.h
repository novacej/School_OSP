/** \file usermenu.h
 *  \brief Header: user menu implementation
 */

#ifndef MC__USERMENU_H
#define MC__USERMENU_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WEdit;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean user_menu_cmd (struct WEdit *edit_widget);
char *expand_format (struct WEdit *edit_widget, char c, gboolean do_quote);
int check_format_view (const char *);
int check_format_var (const char *, char **);
int check_format_cd (const char *);

/*** inline functions ****************************************************************************/

#endif /* MC__USERMENU_H */
