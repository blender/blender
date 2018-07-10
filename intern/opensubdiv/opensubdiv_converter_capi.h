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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_CONVERTER_CAPI_H__
#define __OPENSUBDIV_CONVERTER_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

struct OpenSubdiv_TopologyRefinerDescr;
typedef struct OpenSubdiv_TopologyRefinerDescr OpenSubdiv_TopologyRefinerDescr;

typedef struct OpenSubdiv_Converter OpenSubdiv_Converter;

typedef enum OpenSubdiv_SchemeType {
	OSD_SCHEME_BILINEAR,
	OSD_SCHEME_CATMARK,
	OSD_SCHEME_LOOP,
} OpenSubdiv_SchemeType;

typedef enum OpenSubdiv_FVarLinearInterpolation {
	OSD_FVAR_LINEAR_INTERPOLATION_NONE,
	OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY,
	OSD_FVAR_LINEAR_INTERPOLATION_BOUNDARIES,
	OSD_FVAR_LINEAR_INTERPOLATION_ALL,
} OpenSubdiv_FVarLinearInterpolation;

typedef struct OpenSubdiv_Converter {
	/* TODO(sergey): Needs to be implemented. */
	/* OpenSubdiv::Sdc::Options get_options() const; */

	OpenSubdiv_SchemeType (*get_scheme_type)(
	        const OpenSubdiv_Converter *converter);

	OpenSubdiv_FVarLinearInterpolation (*get_fvar_linear_interpolation)(
	        const OpenSubdiv_Converter *converter);

	int (*get_num_faces)(const OpenSubdiv_Converter *converter);
	int (*get_num_edges)(const OpenSubdiv_Converter *converter);
	int (*get_num_verts)(const OpenSubdiv_Converter *converter);

	/* Face relationships. */
	int (*get_num_face_verts)(const OpenSubdiv_Converter *converter,
	                          int face);
	void (*get_face_verts)(const OpenSubdiv_Converter *converter,
	                       int face,
	                       int *face_verts);
	void (*get_face_edges)(const OpenSubdiv_Converter *converter,
	                       int face,
	                       int *face_edges);

	/* Edge relationships. */
	void (*get_edge_verts)(const OpenSubdiv_Converter *converter,
	                       int edge,
	                       int *edge_verts);
	int (*get_num_edge_faces)(const OpenSubdiv_Converter *converter,
	                          int edge);
	void (*get_edge_faces)(const OpenSubdiv_Converter *converter,
	                       int edge,
	                       int *edge_faces);
	float (*get_edge_sharpness)(const OpenSubdiv_Converter *converter,
	                            int edge);

	/* Vertex relationships. */
	int (*get_num_vert_edges)(const OpenSubdiv_Converter *converter, int vert);
	void (*get_vert_edges)(const OpenSubdiv_Converter *converter,
	                       int vert,
	                       int *vert_edges);
	int (*get_num_vert_faces)(const OpenSubdiv_Converter *converter, int vert);
	void (*get_vert_faces)(const OpenSubdiv_Converter *converter,
	                       int vert,
	                       int *vert_faces);

	/* Face-varying data. */
	int (*get_num_uv_layers)(const OpenSubdiv_Converter *converter);

	void (*precalc_uv_layer)(const OpenSubdiv_Converter *converter, int layer);
	void (*finish_uv_layer)(const OpenSubdiv_Converter *converter);

	int (*get_num_uvs)(const OpenSubdiv_Converter *converter);
	void (*get_uvs)(const OpenSubdiv_Converter *converter, float *uvs);

	int (*get_face_corner_uv_index)(const OpenSubdiv_Converter *converter,
	                                int face,
	                                int corner);

	/* User data associated with this converter. */
	void (*free_user_data)(const OpenSubdiv_Converter *converter);
	void *user_data;
} OpenSubdiv_Converter;

OpenSubdiv_TopologyRefinerDescr *openSubdiv_createTopologyRefinerDescr(
        OpenSubdiv_Converter *converter);

void openSubdiv_deleteTopologyRefinerDescr(
        OpenSubdiv_TopologyRefinerDescr *topology_refiner);

/* TODO(sergey): Those calls are not strictly related to conversion.
 * needs some dedicated file perhaps.
 */

int openSubdiv_topologyRefinerGetSubdivLevel(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner);

int openSubdiv_topologyRefinerGetNumVerts(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner);

int openSubdiv_topologyRefinerGetNumEdges(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner);

int openSubdiv_topologyRefinerGetNumFaces(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner);

int openSubdiv_topologyRefinerGetNumFaceVerts(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner,
        int face);

int openSubdiv_topologyRefnerCompareConverter(
        const OpenSubdiv_TopologyRefinerDescr *topology_refiner,
        OpenSubdiv_Converter *converter);

#ifdef __cplusplus
}
#endif

#endif  /* __OPENSUBDIV_CONVERTER_CAPI_H__ */
