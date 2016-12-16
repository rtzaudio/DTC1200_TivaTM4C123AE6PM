/*
 * diag.h : created 7/30/99 10:50:41 AM
 *  
 * Copyright (C) 1999-2000, RTZ Audio. ALL RIGHTS RESERVED.
 *  
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION OF RTZ AUDIO. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF RTZ AUDIO.
 */

void LampBlinkChase();
void LampBlinkError();

int diag_lamp(MENUITEM* mp);
int diag_tach(MENUITEM* mp);
int diag_transport(MENUITEM* mp);
int diag_dacramp(MENUITEM* mp);
int diag_dacadjust(MENUITEM* mp);
int diag_dump_capture(MENUITEM* mp);

/* end-of-file */


