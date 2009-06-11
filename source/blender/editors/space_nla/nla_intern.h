/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_NLA_INTERN_H
#define ED_NLA_INTERN_H

/* internal exports only */

/* **************************************** */
/* Macros, etc. only used by NLA */

/* -------------- NLA Channel Defines -------------- */

/* NLA channel heights */
#define NLACHANNEL_FIRST			-16
#define	NLACHANNEL_HEIGHT			24
#define NLACHANNEL_HEIGHT_HALF	12
#define	NLACHANNEL_SKIP			2
#define NLACHANNEL_STEP			(NLACHANNEL_HEIGHT + NLACHANNEL_SKIP)

/* channel widths */
#define NLACHANNEL_NAMEWIDTH		200

/* channel toggle-buttons */
#define NLACHANNEL_BUTTON_WIDTH	16

/* **************************************** */
/* space_nla.c / nla_buttons.c */

ARegion *nla_has_buttons_region(ScrArea *sa);

void nla_buttons_register(ARegionType *art);
void NLAEDIT_OT_properties(wmOperatorType *ot);

/* **************************************** */
/* nla_draw.c */

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *ar);
void draw_nla_channel_list(bAnimContext *ac, SpaceNla *snla, ARegion *ar);

/* **************************************** */
/* nla_header.c */

void nla_header_buttons(const bContext *C, ARegion *ar);

/* **************************************** */
/* nla_select.c */

/* defines for left-right select tool */
enum {
	NLAEDIT_LRSEL_TEST	= -1,
	NLAEDIT_LRSEL_NONE,
	NLAEDIT_LRSEL_LEFT,
	NLAEDIT_LRSEL_RIGHT,
} eNlaEdit_LeftRightSelect_Mode;

/* --- */

void NLAEDIT_OT_select_all_toggle(wmOperatorType *ot);
void NLAEDIT_OT_select_border(wmOperatorType *ot);
void NLAEDIT_OT_click_select(wmOperatorType *ot);

/* **************************************** */
/* nla_edit.c */

void NLAEDIT_OT_tweakmode_enter(wmOperatorType *ot);
void NLAEDIT_OT_tweakmode_exit(wmOperatorType *ot);

/* --- */

void NLAEDIT_OT_delete(wmOperatorType *ot);
void NLAEDIT_OT_split(wmOperatorType *ot);


/* **************************************** */
/* nla_channels.c */

void NLA_OT_channels_click(wmOperatorType *ot);

void NLA_OT_add_tracks(wmOperatorType *ot);

/* **************************************** */
/* nla_ops.c */

int nlaop_poll_tweakmode_off(bContext *C);
int nlaop_poll_tweakmode_on (bContext *C);

/* --- */

void nla_operatortypes(void);
void nla_keymap(wmWindowManager *wm);

#endif /* ED_NLA_INTERN_H */

