/*
 * ConsoleTask.h
 *
 */

#ifndef DTC1200_TIVATM4C123AE6PMI_TERMINALTASK_H_
#define DTC1200_TIVATM4C123AE6PMI_TERMINALTASK_H_

#define PROMPT_ROW      23
#define PROMPT_COL      1

#define KEYBUF_SIZE     8   /* maximum keystroke input buffer size */

/* The following menu item types
 * determine the way each menu item
 * entry is handled.
 */

/* Menu item is a menu prompt string. Just display
 * the string at the specified location.
 */
#define MI_PROMPT   0

/* Display text at location, parm1 non-zero for underline
 */
#define MI_TEXT     1

/* Just execute the menu function without any input
 * parameters or validation.
 */
#define MI_EXEC     2

/* The menu item selects a new current menu and displays it.
 * The member 'parm1' specifies the new menu number to display.
 */
#define MI_NMENU    3

/* Menu item is a value between a min/max value. The value
 * entered must be within this range specified by the
 * members 'parm1' and 'parm2'.
 */
#define MI_NRANGE   4

/* The menu item contains a list of discrete values. The
 * value entered must match one of the values in the list
 * before the execute method is called. The member 'arglist'
 * points to a MENU_ARGLIST structure which contains a list
 * of options and 'parm1' specifies the count of items in the list.
 */
#define MI_VALLIST  5

/* The menu item contains an array of bitflag values. The member
 * 'arglist' points to a MENU_ARGLIST structure which points
 * to an array of bitflag masks and text descriptions.
 */
#define MI_BITLIST  6

/* The menu item contains bitflag boolean value. The member 'arg1'
 * specifies the bitmask to enable a feature and 'arg2'
 * specifies the bitmask to clear a feature.
 */
#define MI_BITBOOL  7

/* The menu item contains bitflag boolean value. The member 'arg1'
 * specifies the maximum length up to KEYBUF_SIZE characters.
 */
#define MI_STRING   8

/* Hotkey menu item. The menu handler function is called directly
 * if the corresponding option string key is pressed.
 */
#define MI_HOTKEY   9

/*****************************************************************************
 * MENU ITEM STRUCTURES
 *****************************************************************************/

/* Data Type Parameters for 'datatype' below */

#define DT_BYTE     1           /* unsigned char 8 bits     */
#define DT_INT      2           /* signed integer           */
#define DT_LONG     3           /* signed long              */
#define DT_FLOAT    4           /* float type               */

/*
 * Optional argument list structure for a menu item.
 */

typedef struct _MENU_ARGLIST {
    char*       text;           /* arg description text     */
    long        value;          /* arg discrete value       */
} MENU_ARGLIST;

/*
 * Menu Item Entry Structure
 */

typedef struct _MENUITEM {
    int         row;            /* menu text row number               */
    int         col;            /* menu text col number               */
    char*       optstr;         /* menu entry option text             */
    char*       text;           /* menu item text string              */
    int         type;           /* menu item type (MI_xxx)            */
    long        parm1;          /* parm1 - item count for MI_VALLIST  */
                                /*       - min value for MI_NRANGE    */
                                /*       - menu number for MI_MENU    */
                                /*       - on bitmask for MI_BITMASK  */
                                /*       - underline for MI_DISPLAY   */
    long        parm2;          /* parm2 - max value for MI_NRANGE    */
                                /*       - off bitmask for MI_BITMASK */
    void* 		arglist;
    int         (*exec)(struct _MENUITEM* mp);
    int         datatype;       /* config data size in bytes          */
    void*       data;           /* pointer to binary data item        */
} MENUITEM;

/* Array of menu items structure */

typedef struct _MENU {
    int         id;
    MENUITEM*   items;      	/* pointer to menu items */
    int         count;      	/* count of menu items   */
    char*       heading;    	/* menu heading string   */
} MENU;

/* Function Prototypes */

void Terminal_initialize(void);

Void TerminalTask(UArg a0, UArg a1);

#endif /* DTC1200_TIVATM4C123AE6PMI_TERMINALTASK_H_ */
