
/** \file listbox.h
 *  \brief Header: WListbox widget
 */

#ifndef MC__WIDGET_LISTBOX_H
#define MC__WIDGET_LISTBOX_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/* callback should return one of the following values */
typedef enum
{
    LISTBOX_CONT,               /* continue */
    LISTBOX_DONE                /* finish dialog */
} lcback_ret_t;

typedef enum
{
    LISTBOX_APPEND_AT_END = 0,  /* append at the end */
    LISTBOX_APPEND_BEFORE,      /* insert before current */
    LISTBOX_APPEND_AFTER,       /* insert after current */
    LISTBOX_APPEND_SORTED       /* insert alphabetically */
} listbox_append_t;

/*** structures declarations (and typedefs of structures)*****************************************/

struct WListbox;
typedef lcback_ret_t (*lcback_fn) (struct WListbox * l);

typedef struct WLEntry
{
    char *text;                 /* Text to display */
    int hotkey;
    void *data;                 /* Client information */
} WLEntry;

typedef struct WListbox
{
    Widget widget;
    GList *list;                /* Pointer to the double linked list */
    int pos;                    /* The current element displayed */
    int top;                    /* The first element displayed */
    int count;                  /* Number of items in the listbox */
    gboolean allow_duplicates;  /* Do we allow duplicates on the list? */
    gboolean scrollbar;         /* Draw a scrollbar? */
    gboolean deletable;         /* Can list entries be deleted? */
    lcback_fn callback;         /* The callback function */
    int cursor_x, cursor_y;     /* Cache the values */
} WListbox;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WListbox *listbox_new (int y, int x, int height, int width, gboolean deletable, lcback_fn callback);
int listbox_search_text (WListbox * l, const char *text);
void listbox_select_first (WListbox * l);
void listbox_select_last (WListbox * l);
void listbox_select_entry (WListbox * l, int dest);
void listbox_get_current (WListbox * l, char **string, void **extra);
void listbox_remove_current (WListbox * l);
void listbox_set_list (WListbox * l, GList * list);
void listbox_remove_list (WListbox * l);
char *listbox_add_item (WListbox * l, listbox_append_t pos,
                        int hotkey, const char *text, void *data);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_LISTBOX_H */
