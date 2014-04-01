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

/** \file blender/editors/armature/editarmature_generate.c
 *  \ingroup edarmature
 */

#include "DNA_scene_types.h"
#include "DNA_armature_types.h"

#include "BLI_math.h"
#include "BLI_graph.h"

#include "ED_armature.h"
#include "BIF_generate.h"

void setBoneRollFromNormal(EditBone *bone, const float no[3], float UNUSED(invmat[4][4]), float tmat[3][3])
{
	if (no != NULL && !is_zero_v3(no)) {
		float normal[3];

		copy_v3_v3(normal, no);
		mul_m3_v3(tmat, normal);
		
		bone->roll = ED_rollBoneToVector(bone, normal, false);
	}
}

float calcArcCorrelation(BArcIterator *iter, int start, int end, float v0[3], float n[3])
{
	int len = 2 + abs(end - start);
	
	if (len > 2) {
		float avg_t = 0.0f;
		float s_t = 0.0f;
		float s_xyz = 0.0f;
		int i;
		
		/* First pass, calculate average */
		for (i = start; i <= end; i++) {
			float v[3];
			
			IT_peek(iter, i);
			sub_v3_v3v3(v, iter->p, v0);
			avg_t += dot_v3v3(v, n);
		}
		
		avg_t /= dot_v3v3(n, n);
		avg_t += 1.0f; /* adding start (0) and end (1) values */
		avg_t /= len;
		
		/* Second pass, calculate s_xyz and s_t */
		for (i = start; i <= end; i++) {
			float v[3], d[3];
			float dt;
			
			IT_peek(iter, i);
			sub_v3_v3v3(v, iter->p, v0);
			project_v3_v3v3(d, v, n);
			sub_v3_v3(v, d);
			
			dt = len_v3(d) - avg_t;
			
			s_t += dt * dt;
			s_xyz += dot_v3v3(v, v);
		}
		
		/* adding start(0) and end(1) values to s_t */
		s_t += (avg_t * avg_t) + (1 - avg_t) * (1 - avg_t);
		
		return 1.0f - s_xyz / s_t; 
	}
	else {
		return 1.0f;
	}
}

int nextFixedSubdivision(ToolSettings *toolsettings, BArcIterator *iter, int start, int end, float UNUSED(head[3]), float p[3])
{
	static float stroke_length = 0;
	static float current_length;
	static char n;
	float *v1, *v2;
	float length_threshold;
	int i;
	
	if (stroke_length == 0) {
		current_length = 0;

		IT_peek(iter, start);
		v1 = iter->p;
		
		for (i = start + 1; i <= end; i++) {
			IT_peek(iter, i);
			v2 = iter->p;

			stroke_length += len_v3v3(v1, v2);
			
			v1 = v2;
		}
		
		n = 0;
		current_length = 0;
	}
	
	n++;
	
	length_threshold = n * stroke_length / toolsettings->skgen_subdivision_number;
	
	IT_peek(iter, start);
	v1 = iter->p;

	/* < and not <= because we don't care about end, it is P_EXACT anyway */
	for (i = start + 1; i < end; i++) {
		IT_peek(iter, i);
		v2 = iter->p;

		current_length += len_v3v3(v1, v2);

		if (current_length >= length_threshold) {
			copy_v3_v3(p, v2);
			return i;
		}
		
		v1 = v2;
	}
	
	stroke_length = 0;
	
	return -1;
}
int nextAdaptativeSubdivision(ToolSettings *toolsettings, BArcIterator *iter, int start, int end, float head[3], float p[3])
{
	float correlation_threshold = toolsettings->skgen_correlation_limit;
	float *start_p;
	float n[3];
	int i;
	
	IT_peek(iter, start);
	start_p = iter->p;

	for (i = start + 2; i <= end; i++) {
		/* Calculate normal */
		IT_peek(iter, i);
		sub_v3_v3v3(n, iter->p, head);

		if (calcArcCorrelation(iter, start, i, start_p, n) < correlation_threshold) {
			IT_peek(iter, i - 1);
			copy_v3_v3(p, iter->p);
			return i - 1;
		}
	}
	
	return -1;
}

int nextLengthSubdivision(ToolSettings *toolsettings, BArcIterator *iter, int start, int end, float head[3], float p[3])
{
	float lengthLimit = toolsettings->skgen_length_limit;
	int same = 1;
	int i;
	
	i = start + 1;
	while (i <= end) {
		float *vec0;
		float *vec1;
		
		IT_peek(iter, i - 1);
		vec0 = iter->p;

		IT_peek(iter, i);
		vec1 = iter->p;
		
		/* If lengthLimit hits the current segment */
		if (len_v3v3(vec1, head) > lengthLimit) {
			if (same == 0) {
				float dv[3], off[3];
				float a, b, c, f;
				
				/* Solve quadratic distance equation */
				sub_v3_v3v3(dv, vec1, vec0);
				a = dot_v3v3(dv, dv);
				
				sub_v3_v3v3(off, vec0, head);
				b = 2 * dot_v3v3(dv, off);
				
				c = dot_v3v3(off, off) - (lengthLimit * lengthLimit);
				
				f = (-b + sqrtf(b * b - 4 * a * c)) / (2 * a);
				
				//printf("a %f, b %f, c %f, f %f\n", a, b, c, f);
				
				if (isnan(f) == 0 && f < 1.0f) {
					copy_v3_v3(p, dv);
					mul_v3_fl(p, f);
					add_v3_v3(p, vec0);
				}
				else {
					copy_v3_v3(p, vec1);
				}
			}
			else {
				float dv[3];
				
				sub_v3_v3v3(dv, vec1, vec0);
				normalize_v3(dv);
				 
				copy_v3_v3(p, dv);
				mul_v3_fl(p, lengthLimit);
				add_v3_v3(p, head);
			}
			
			return i - 1; /* restart at lower bound */
		}
		else {
			i++;
			same = 0; // Reset same
		}
	}
	
	return -1;
}

EditBone *subdivideArcBy(ToolSettings *toolsettings, bArmature *arm, ListBase *UNUSED(editbones), BArcIterator *iter,
                         float invmat[4][4], float tmat[3][3], NextSubdivisionFunc next_subdividion)
{
	EditBone *lastBone = NULL;
	EditBone *child = NULL;
	EditBone *parent = NULL;
	float *normal = NULL;
	float size_buffer = 1.2;
	int bone_start = 0;
	int end = iter->length;
	int index;
	
	IT_head(iter);
	
	parent = ED_armature_edit_bone_add(arm, "Bone");
	copy_v3_v3(parent->head, iter->p);
	
	if (iter->size > FLT_EPSILON) {
		parent->rad_head = iter->size * size_buffer;
	}
	
	normal = iter->no;
	
	index = next_subdividion(toolsettings, iter, bone_start, end, parent->head, parent->tail);
	while (index != -1) {
		IT_peek(iter, index);

		child = ED_armature_edit_bone_add(arm, "Bone");
		copy_v3_v3(child->head, parent->tail);
		child->parent = parent;
		child->flag |= BONE_CONNECTED;
		
		if (iter->size > FLT_EPSILON) {
			child->rad_head = iter->size * size_buffer;
			parent->rad_tail = iter->size * size_buffer;
		}

		/* going to next bone, fix parent */
		mul_m4_v3(invmat, parent->tail);
		mul_m4_v3(invmat, parent->head);
		setBoneRollFromNormal(parent, normal, invmat, tmat);

		parent = child; // new child is next parent
		bone_start = index; // start next bone from current index

		normal = iter->no; /* use normal at head, not tail */

		index = next_subdividion(toolsettings, iter, bone_start, end, parent->head, parent->tail);
	}
	
	iter->tail(iter);

	copy_v3_v3(parent->tail, iter->p);
	if (iter->size > FLT_EPSILON) {
		parent->rad_tail = iter->size * size_buffer;
	}
		
	/* fix last bone */
	mul_m4_v3(invmat, parent->tail);
	mul_m4_v3(invmat, parent->head);
	setBoneRollFromNormal(parent, iter->no, invmat, tmat);
	lastBone = parent;
	
	return lastBone;
}
