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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_ACTION_INTERN_H
#define ED_ACTION_INTERN_H

struct bContext;
struct bAnimContext;
struct SpaceAction;
struct ARegion;
struct wmWindowManager;
struct wmOperatorType;

/* internal exports only */

/* action_draw.c */
void draw_channel_names(struct bAnimContext *ac, struct SpaceAction *saction, struct ARegion *ar); 
void draw_channel_strips(struct bAnimContext *ac, struct SpaceAction *saction, struct ARegion *ar);

/* action_header.c */
void action_header_buttons(const struct bContext *C, struct ARegion *ar);

/* action_select.c */
void ED_ACT_OT_keyframes_deselectall(struct wmOperatorType *ot);
void ED_ACT_OT_keyframes_borderselect(struct wmOperatorType *ot);
void ED_ACT_OT_keyframes_clickselect(struct wmOperatorType *ot);

/* action_ops.c */
void action_operatortypes(void);
void action_keymap(struct wmWindowManager *wm);

#endif /* ED_ACTION_INTERN_H */

