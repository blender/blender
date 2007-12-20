/*
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
 * The Original Code is: none of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef STRAND_H
#define STRAND_H 

struct StrandVert;
struct StrandRen;
struct StrandBuffer;
struct ShadeSample;
struct StrandPart;
struct Render;
struct RenderPart;
struct RenderBuckets;
struct RenderPrimitiveIterator;
struct ZSpan;
struct ObjectInstanceRen;

typedef struct StrandPoint {
	/* position within segment */
	float t;

	/* camera space */
	float co[3];
	float nor[3];
	float tan[3];
	float strandco;
	float width;

	/* derivatives */
	float dtco[3], dsco[3];
	float dtstrandco;

	/* outer points */
	float co1[3], co2[3];
	float hoco1[4], hoco2[4];
	float zco1[3], zco2[3];

	/* screen space */
	float hoco[4];
	float x, y;

	/* simplification */
	float alpha;
} StrandPoint;

typedef struct StrandSegment {
	struct StrandVert *v[4];
	struct StrandRen *strand;
	struct StrandBuffer *buffer;
	struct ObjectInstanceRen *obi;
	float sqadaptcos;

	StrandPoint point1, point2;
	int shaded;
} StrandSegment;

void strand_eval_point(StrandSegment *sseg, StrandPoint *spoint);
void render_strand_segment(struct Render *re, float winmat[][4], struct StrandPart *spart, struct ZSpan *zspan, StrandSegment *sseg);
void project_strands(Render *re, void (*projectfunc)(float *, float mat[][4], float *),  int do_pano, int do_buckets);

struct RenderBuckets *init_buckets(struct Render *re);
void add_buckets_primitive(struct RenderBuckets *buckets, float *min, float *max, void *prim);
void free_buckets(struct RenderBuckets *buckets);
void project_hoco_to_bucket(struct RenderBuckets *buckets, float *hoco, float *bucketco);

struct RenderPrimitiveIterator *init_primitive_iterator(struct Render *re, struct RenderBuckets *buckets, struct RenderPart *pa);
void *next_primitive_iterator(struct RenderPrimitiveIterator *iter);
void free_primitive_iterator(struct RenderPrimitiveIterator *iter);

#endif

