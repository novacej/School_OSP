/** \file win.h
 *  \brief Header: X terminal management: xterm and rxvt
 */

#ifndef MC__WIN_H
#define MC__WIN_H

#include "lib/global.h"         /* <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern int xterm_flag;

/*** declarations of public functions ************************************************************/

void do_enter_ca_mode (void);
void do_exit_ca_mode (void);

void show_rxvt_contents (int starty, unsigned char y1, unsigned char y2);
gboolean look_for_rxvt_extensions (void);

/*** inline functions ****************************************************************************/
#endif /* MC_WIN_H */
