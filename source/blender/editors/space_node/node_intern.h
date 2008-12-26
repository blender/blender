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
#ifndef ED_NODE_INTERN_H
#define ED_NODE_INTERN_H

/* internal exports only */

struct ARegion;
struct View2D;
struct bContext;


/* node_header.c */
void node_header_buttons(const bContext *C, ARegion *ar);

/* node_draw.c */
void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d);

/* drawnode.c */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link);
void node_draw_link_bezier(View2D *v2d, float vec[][3], int th_col1, int th_col2, int do_shaded);
void draw_nodespace_back_pix(ScrArea *sa, SpaceNode *snode);

/* node_edit.c */
void snode_set_context(SpaceNode *snode, Scene *scene);

#endif /* ED_NODE_INTERN_H */

