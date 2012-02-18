/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/BIF_generate.h
 *  \ingroup edarmature
 */

 
#ifndef __BIF_GENERATE_H__
#define __BIF_GENERATE_H__

struct ToolSettings;
struct EditBone;
struct BArcIterator;
struct bArmature;
struct ListBase;

typedef int(NextSubdivisionFunc)(struct ToolSettings*, struct BArcIterator*, int, int, float[3], float[3]);
 
float calcArcCorrelation(struct BArcIterator *iter, int start, int end, float v0[3], float n[3]);

int nextFixedSubdivision(struct ToolSettings *toolsettings, struct BArcIterator *iter, int start, int end, float head[3], float p[3]);
int nextLengthSubdivision(struct ToolSettings *toolsettings, struct BArcIterator *iter, int start, int end, float head[3], float p[3]);
int nextAdaptativeSubdivision(struct ToolSettings *toolsettings, struct BArcIterator *iter, int start, int end, float head[3], float p[3]);

struct EditBone * subdivideArcBy(struct ToolSettings *toolsettings, struct bArmature *arm, ListBase *editbones, struct BArcIterator *iter, float invmat[][4], float tmat[][3], NextSubdivisionFunc next_subdividion);

void setBoneRollFromNormal(struct EditBone *bone, float *no, float invmat[][4], float tmat[][3]);
 

#endif /* __BIF_GENERATE_H__ */
