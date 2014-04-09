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
#include <carve/triangulator.hpp>

#include "carve-capi.h"

struct TriIdxCompare {
	bool operator() (const carve::triangulate::tri_idx &left,
	                 const carve::triangulate::tri_idx &right) const {
		if (left.a < right.a) {
			return true;
		}
		else if (left.a > right.a) {
			return false;
		}

		if (left.b < right.b) {
			return true;
		}
		else if (left.b > right.b) {
			return false;
		}

		if (left.c < right.c) {
			return true;
		}
		else if (left.c > right.c) {
			return false;
		}

		return false;
	}
};

typedef std::set<carve::triangulate::tri_idx, TriIdxCompare> TrianglesStorage;

void carve_getRescaleMinMax(const carve::mesh::MeshSet<3> *left,
                            const carve::mesh::MeshSet<3> *right,
                            carve::geom3d::Vector *min,
                            carve::geom3d::Vector *max);

bool carve_unionIntersections(carve::csg::CSG *csg,
                              carve::mesh::MeshSet<3> **left_r,
                              carve::mesh::MeshSet<3> **right_r);

bool carve_checkPolyPlanarAndGetNormal(const std::vector<carve::geom3d::Vector> &vertices,
                                       const int verts_per_poly,
                                       const int *verts_of_poly,
                                       carve::math::Matrix3 *axis_matrix_r);

int carve_triangulatePoly(struct ImportMeshData *import_data,
                          CarveMeshImporter *mesh_importer,
                          const std::vector<carve::geom3d::Vector> &vertices,
                          const int verts_per_poly,
                          const int *verts_of_poly,
                          const carve::math::Matrix3 &axis_matrix,
                          std::vector<int> *face_indices,
                          TrianglesStorage *triangles_storage);

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

		template<typename attr_t>
		class SimpleFaceEdgeAttr : public Interpolator {
		public:
			typedef std::pair<const meshset_t::face_t *, unsigned> key_t;

		protected:
			typedef std::pair<const meshset_t::vertex_t *, const meshset_t::vertex_t *> vpair_t;

			struct key_hash {
				size_t operator()(const key_t &v) const {
					return size_t(v.first) ^ size_t(v.second);
				}
			};
			struct vpair_hash {
				size_t operator()(const vpair_t &v) const {
					return size_t(v.first) ^ size_t(v.second);
				}
			};

			typedef std::unordered_map<key_t, attr_t, key_hash> attrmap_t;
			typedef std::unordered_map<vpair_t, key_t, vpair_hash> edgedivmap_t;

			attrmap_t attrs;

			struct Hook : public Interpolator::Hook {
			public:
				virtual unsigned hookBits() const {
					return carve::csg::CSG::Hooks::PROCESS_OUTPUT_FACE_BIT;
				}
				Hook(Interpolator *_interpolator, const carve::csg::CSG &_csg) : Interpolator::Hook(_interpolator, _csg) {
				}
				virtual ~Hook() {
				}
			};

			virtual Interpolator::Hook *makeHook(carve::csg::CSG &csg) {
				return new Hook(this, csg);
			}

			virtual void processOutputFace(const carve::csg::CSG &csg,
			                               std::vector<carve::mesh::MeshSet<3>::face_t *> &new_faces,
			                               const meshset_t::face_t *orig_face,
			                               bool flipped) {
				edgedivmap_t undiv;

				for (meshset_t::face_t::const_edge_iter_t e = orig_face->begin(); e != orig_face->end(); ++e) {
					key_t k(orig_face, e.idx());
					typename attrmap_t::const_iterator attr_i = attrs.find(k);
					if (attr_i == attrs.end()) {
						continue;
					} else {
						undiv[vpair_t(e->v1(), e->v2())] = k;
					}
				}

				for (size_t fnum = 0; fnum < new_faces.size(); ++fnum) {
					const carve::mesh::MeshSet<3>::face_t *new_face = new_faces[fnum];
					for (meshset_t::face_t::const_edge_iter_t e = new_face->begin(); e != new_face->end(); ++e) {
						key_t k(new_face, e.idx());

						vpair_t vp;
						if (!flipped) {
							vp = vpair_t(e->v1(), e->v2());
						} else {
							vp = vpair_t(e->v2(), e->v1());
						}
						typename edgedivmap_t::const_iterator vp_i;
						if ((vp_i = undiv.find(vp)) != undiv.end()) {
							attrs[k] = attrs[vp_i->second];
						}
					}
				}
			}

		public:

			bool hasAttribute(const meshset_t::face_t *f, unsigned e) {
				return attrs.find(std::make_pair(f, e)) != attrs.end();
			}

			attr_t getAttribute(const meshset_t::face_t *f, unsigned e, const attr_t &def = attr_t()) {
				typename attrmap_t::const_iterator fv = attrs.find(std::make_pair(f, e));
				if (fv != attrs.end()) {
					return (*fv).second;
				}
				return def;
			}

			void setAttribute(const meshset_t::face_t *f, unsigned e, const attr_t &attr) {
				attrs[std::make_pair(f, e)] = attr;
			}

			void copyAttribute(const meshset_t::face_t *face,
			                   unsigned edge,
			                   SimpleFaceEdgeAttr<attr_t> *interpolator) {
				key_t key(face, edge);
				typename attrmap_t::const_iterator fv = interpolator->attrs.find(key);
				if (fv != interpolator->attrs.end()) {
					attrs[key] = (*fv).second;
				}
			}

			void swapAttributes(SimpleFaceEdgeAttr<attr_t> *interpolator) {
				attrs.swap(interpolator->attrs);
			}

			SimpleFaceEdgeAttr() : Interpolator() {
			}

			virtual ~SimpleFaceEdgeAttr() {
			}
		};

		template<typename attr_t>
		class SwapableFaceEdgeAttr : public FaceEdgeAttr<attr_t> {
		public:
			typedef carve::mesh::MeshSet<3> meshset_t;

			void copyAttribute(const meshset_t::face_t *face,
			                   unsigned edge,
			                   SwapableFaceEdgeAttr<attr_t> *interpolator) {
				typename FaceEdgeAttr<attr_t>::key_t key(face, edge);
				typename FaceEdgeAttr<attr_t>::attrmap_t::const_iterator fv = interpolator->attrs.find(key);
				if (fv != interpolator->attrs.end()) {
					this->attrs[key] = (*fv).second;
				}
			}

			void swapAttributes(SwapableFaceEdgeAttr<attr_t> *interpolator) {
				this->attrs.swap(interpolator->attrs);
			}
		};
	}  // namespace interpolate
}  // namespace carve

#endif  // __CARVE_UTIL_H__
