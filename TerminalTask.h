/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 *
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================ */

#ifndef _TERMINALTASK_H_
#define _TERMINALTASK_H_

#define PROMPT_ROW      23
#define PROMPT_COL      2

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

/* Menu item is a long value between a min/max value. The value
 * entered must be within the range specified by the
 * members 'parm1' and 'parm2'.
 */
#define MI_LONG     4

/* Menu item is a float value between a min/max value. The value
 * entered must be within the range specified by the
 * members 'parm1' and 'parm2'.
 */
#define MI_FLOAT    5

/* The menu item contains a list of discrete values. The
 * value entered must match one of the values in the list
 * before the execute method is called. The member 'arglist'
 * points to a MENU_ARGLIST structure which contains a list
 * of options and 'parm1' specifies the count of items in the list.
 */
#define MI_VALLIST  6

/* The menu item contains an array of bitflag values. The member
 * 'arglist' points to a MENU_ARGLIST structure which points
 * to an array of bitflag masks and text descriptions.
 */
#define MI_BITLIST  7

/* The menu item contains bitflag boolean value. The member 'parm1'
 * specifies the bitmask to enable a feature and 'parm2'
 * specifies the bitmask to clear a feature.
 */
#define MI_BITFLAG  8

/* The menu item contains bitflag boolean value. The member 'parm1'
 * specifies the maximum length up to KEYBUF_SIZE characters.
 */
#define MI_STRING   9

/* Hotkey menu item. The menu handler function is called directly
 * if the corresponding option string key is pressed.
 */
#define MI_HOTKEY   10

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
    uint32_t    value;          /* arg discrete value       */
} MENU_ARGLIST;

/*
 * Menu Item Entry Structure
 */

typedef struct _MENUITEM {
    int         row;            /* menu text row number               */
    int         col;            /* menu text col number               */
    char*       optstr;         /* menu entry option text             */
    char*       text;           /* menu item text string              */
    int         menutype;       /* menu item type (MI_xxx)            */
    union {                     /* parm1 - item count for MI_VALLIST  */
        uint32_t    U;          /*       - min value for MI_NRANGE    */
        float       F;          /*       - menu number for MI_MENU    */
    } parm1;                    /*       - on bitmask for MI_BITMASK  */
    union {                     /*       - underline for MI_DISPLAY   */
        uint32_t    U;          /* parm2 - max value for MI_NRANGE    */
        float       F;          /*       - off bitmask for MI_BITMASK */
    }  parm2;
    void*       arglist;        /* pointer to variable argument list  */
    int         (*exec)(struct _MENUITEM* mp);
    int         datatype;       /* data type size specifier           */
    void*       data;           /* pointer to binary data item        */
} MENUITEM;

/* Array of menu items structure */

typedef struct _MENU {
    int         id;
    MENUITEM*   items;          /* pointer to menu items */
    int         count;          /* count of menu items   */
    char*       heading;        /* menu heading string   */
} MENU;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

void Terminal_initialize(void);
Void TerminalTask(UArg a0, UArg a1);

#endif /* _TERMINALTASK_H_ */
