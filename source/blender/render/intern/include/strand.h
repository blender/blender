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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/strand.h
 *  \ingroup render
 */


#ifndef __STRAND_H__
#define __STRAND_H__ 

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
struct StrandSurface;
struct DerivedMesh;
struct ObjectRen;

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
	int clip1, clip2;

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

struct StrandShadeCache;
typedef struct StrandShadeCache StrandShadeCache;

void strand_eval_point(StrandSegment *sseg, StrandPoint *spoint);
void render_strand_segment(struct Render *re, float winmat[4][4], struct StrandPart *spart, struct ZSpan *zspan, int totzspan, StrandSegment *sseg);
void strand_minmax(struct StrandRen *strand, float min[3], float max[3], const float width);

struct StrandSurface *cache_strand_surface(struct Render *re, struct ObjectRen *obr, struct DerivedMesh *dm, float mat[4][4], int timeoffset);
void free_strand_surface(struct Render *re);

struct StrandShadeCache *strand_shade_cache_create(void);
void strand_shade_cache_free(struct StrandShadeCache *cache);
void strand_shade_segment(struct Render *re, struct StrandShadeCache *cache, struct StrandSegment *sseg, struct ShadeSample *ssamp, float t, float s, int addpassflag);
void strand_shade_unref(struct StrandShadeCache *cache, struct ObjectInstanceRen *obi, struct StrandVert *svert);

#endif

