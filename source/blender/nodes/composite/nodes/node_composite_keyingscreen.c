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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_keyingscreen.c
 *  \ingroup cmpnodes
 */

#include "BLF_translation.h"

#include "DNA_movieclip_types.h"

#include "BKE_movieclip.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_voronoi.h"

#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_keyingscreen_out[] = {
	{	SOCK_RGBA,  0, "Screen"},
	{	-1, 0, ""	}
};


static void compute_gradient_screen(RenderData *rd, NodeKeyingScreenData *keyingscreen_data, MovieClip *clip, CompBuf *screenbuf)
{
	MovieClipUser user = {0};
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	VoronoiTriangulationPoint *triangulated_points;
	VoronoiSite *sites;
	ImBuf *ibuf;
	ListBase *tracksbase;
	ListBase edges = {NULL, NULL};
	int sites_total, triangulated_points_total, triangles_total;
	int (*triangles)[3];
	int i, x, y;
	float *rect = screenbuf->rect;

	if (keyingscreen_data->tracking_object[0]) {
		MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, keyingscreen_data->tracking_object);

		if (!object)
			return;

		tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	}
	else
		tracksbase = BKE_tracking_get_active_tracks(tracking);

	sites_total = BLI_countlist(tracksbase);

	if (!sites_total)
		return;

	BKE_movieclip_user_set_frame(&user, rd->cfra);
	ibuf = BKE_movieclip_get_ibuf(clip, &user);

	sites = MEM_callocN(sizeof(VoronoiSite) * sites_total, "keyingscreen voronoi sites");
	track = tracksbase->first;
	i = 0;
	while (track) {
		VoronoiSite *site = &sites[i];
		MovieTrackingMarker *marker = BKE_tracking_marker_get(track, rd->cfra);
		ImBuf *pattern_ibuf = BKE_tracking_get_pattern_imbuf(ibuf, track, marker, TRUE, FALSE);
		int j;

		zero_v3(site->color);
		for (j = 0; j < pattern_ibuf->x * pattern_ibuf->y; j++) {
			if (pattern_ibuf->rect_float) {
				add_v3_v3(site->color, &pattern_ibuf->rect_float[4 * j]);
			}
			else {
				unsigned char *rrgb = (unsigned char *)pattern_ibuf->rect;

				site->color[0] += srgb_to_linearrgb((float)rrgb[4 * j + 0] / 255.0f);
				site->color[1] += srgb_to_linearrgb((float)rrgb[4 * j + 1] / 255.0f);
				site->color[2] += srgb_to_linearrgb((float)rrgb[4 * j + 2] / 255.0f);
			}
		}

		mul_v3_fl(site->color, 1.0f / (pattern_ibuf->x * pattern_ibuf->y));
		IMB_freeImBuf(pattern_ibuf);

		site->co[0] = marker->pos[0] * screenbuf->x;
		site->co[1] = marker->pos[1] * screenbuf->y;

		track = track->next;
		i++;
	}

	IMB_freeImBuf(ibuf);

	BLI_voronoi_compute(sites, sites_total, screenbuf->x, screenbuf->y, &edges);

	BLI_voronoi_triangulate(sites, sites_total, &edges, screenbuf->x, screenbuf->y,
	                        &triangulated_points, &triangulated_points_total,
                            &triangles, &triangles_total);

	for (y = 0; y < screenbuf->y; y++) {
		for (x = 0; x < screenbuf->x; x++) {
			int index = 4 * (y * screenbuf->x + x);

			rect[index + 0] = rect[index + 1] = rect[index + 2] = 0.0f;
			rect[index + 3] = 1.0f;

			for (i = 0; i < triangles_total; i++) {
				int *triangle = triangles[i];
				VoronoiTriangulationPoint *a = &triangulated_points[triangle[0]],
				                          *b = &triangulated_points[triangle[1]],
				                          *c = &triangulated_points[triangle[2]];
				float co[2] = {x, y}, w[3];

				if (barycentric_coords_v2(a->co, b->co, c->co, co, w)) {
					if (barycentric_inside_triangle_v2(w)) {
						rect[index + 0] += a->color[0] * w[0] + b->color[0] * w[1] + c->color[0] * w[2];
						rect[index + 1] += a->color[1] * w[0] + b->color[1] * w[1] + c->color[1] * w[2];
						rect[index + 2] += a->color[2] * w[0] + b->color[2] * w[1] + c->color[2] * w[2];
					}
				}
			}
		}
	}

	MEM_freeN(triangulated_points);
	MEM_freeN(triangles);
	MEM_freeN(sites);
	BLI_freelistN(&edges);
}

static void exec(void *data, bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	NodeKeyingScreenData *keyingscreen_data = node->storage;
	RenderData *rd = data;
	CompBuf *screenbuf = NULL;

	if (node->id) {
		MovieClip *clip = (MovieClip *) node->id;
		MovieClipUser user = {0};
		int width, height;

		BKE_movieclip_user_set_frame(&user, rd->cfra);
		BKE_movieclip_get_size(clip, &user, &width, &height);

		screenbuf = alloc_compbuf(width, height, CB_RGBA, TRUE);
		compute_gradient_screen(rd, keyingscreen_data, clip, screenbuf);
	}

	out[0]->data = screenbuf;
}

static void node_composit_init_keyingscreen(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeKeyingScreenData *data;

	data = MEM_callocN(sizeof(NodeKeyingScreenData), "node keyingscreen data");

	node->storage = data;
}

void register_node_type_cmp_keyingscreen(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_KEYINGSCREEN, "Keying Screen", NODE_CLASS_MATTE, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_keyingscreen_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_keyingscreen);
	node_type_storage(&ntype, "NodeKeyingScreenData", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, exec);

	nodeRegisterType(ttype, &ntype);
}
