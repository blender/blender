/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSE_EDITACTION_H
#define BSE_EDITACTION_H

struct bAction;
struct bActionChannel;
struct bPoseChannel;
struct Object;
struct Ipo;

struct bActionChannel* get_hilighted_action_channel(struct bAction* action);
void set_exprap_action(int mode);
void free_posebuf(void);
void copy_posebuf (void);
void paste_posebuf (int flip);
void set_action_key (struct bAction *act, struct bPoseChannel *chan, int adrcode, short makecurve);
struct bAction *add_empty_action(void);
void deselect_actionchannel_keys (struct bAction *act, int test);
void deselect_actionchannels (struct bAction *act, int test);
void winqreadactionspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);
void remake_action_ipos(struct bAction *act);
void select_actionchannel_by_name (struct bAction *act, char *name, int select);
struct bAction *bake_action_with_client (struct bAction *act, struct Object *arm, float tolerance);

#endif /* BSE_EDITACTION_H */

