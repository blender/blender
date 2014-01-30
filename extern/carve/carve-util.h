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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __CARVE_UTIL_H__
#define __CARVE_UTIL_H__

#include <carve/csg.hpp>
#include <carve/geom3d.hpp>
#include <carve/interpolator.hpp>
#include <carve/mesh.hpp>

#include "carve-capi.h"

void carve_getRescaleMinMax(const carve::mesh::MeshSet<3> *left,
                            const carve::mesh::MeshSet<3> *right,
                            carve::geom3d::Vector *min,
                            carve::geom3d::Vector *max);

void carve_unionIntersections(carve::csg::CSG *csg,
                              carve::mesh::MeshSet<3> **left_r,
                              carve::mesh::MeshSet<3> **right_r);

bool carve_checkPolyPlanarAndGetNormal(const std::vector<carve::geom3d::Vector> &vertices,
                                       const int verts_per_poly,
                                       const int *verts_of_poly,
                                       carve::math::Matrix3 *axis_matrix_r);

int carve_triangulatePoly(struct ImportMeshData *import_data,
                          CarveMeshImporter *mesh_importer,
                          int poly_index,
                          int start_loop_index,
                          const std::vector<carve::geom3d::Vector> &vertices,
                          const int verts_per_poly,
                          const int *verts_of_poly,
                          const carve::math::Matrix3 &axis_matrix,
                          std::vector<int> *face_indices,
                          std::vector<int> *orig_loop_index_map,
                          std::vector<int> *orig_poly_index_map);

namespace carve {
	namespace interpolate {

		template<typename attr_t>
		class VertexAttr : public Interpolator {
		public:
			typedef const meshset_t::vertex_t *key_t;

		protected:
			typedef std::unordered_map<key_t, attr_t> attrmap_t;

			attrmap_t attrs;

			virtual void resultFace(const carve::csg::CSG &csg,
			                        const meshset_t::face_t *new_face,
			                        const meshset_t::face_t *orig_face,
			                        bool flipped)
			{
				typedef meshset_t::face_t::const_edge_iter_t const_edge_iter_t;
				for (const_edge_iter_t new_edge_iter = new_face->begin();
					 new_edge_iter != new_face->end();
					 ++new_edge_iter)
				{
					typename attrmap_t::const_iterator found =
						attrs.find(new_edge_iter->vert);
					if (found == attrs.end()) {
						for (const_edge_iter_t orig_edge_iter = orig_face->begin();
							 orig_edge_iter != orig_face->end();
							 ++orig_edge_iter)
						{
							if ((orig_edge_iter->vert->v - new_edge_iter->vert->v).length2() < 1e-5) {
								attrs[new_edge_iter->vert] = attrs[orig_edge_iter->vert];
							}
						}
					}
				}
			}

		public:
			bool hasAttribute(const meshset_t::vertex_t *v) {
				return attrs.find(v) != attrs.end();
			}

			const attr_t &getAttribute(const meshset_t::vertex_t *v, const attr_t &def = attr_t()) {
				typename attrmap_t::const_iterator found = attrs.find(v);
				if (found != attrs.end()) {
					return found->second;
				}
				return def;
			}

			void setAttribute(const meshset_t::vertex_t *v, const attr_t &attr) {
				attrs[v] = attr;
			}
		};

	}  // namespace interpolate
}  // namespace carve

#endif  // __CARVE_UTIL_H__
