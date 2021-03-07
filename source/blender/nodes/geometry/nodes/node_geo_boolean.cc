/*
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
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_matrix.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_mesh.h"

#include "bmesh.h"
#include "tools/bmesh_boolean.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_boolean_in[] = {
    {SOCK_GEOMETRY, N_("Geometry 1")},
    {SOCK_GEOMETRY, N_("Geometry 2")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_boolean_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_boolean_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
  return BM_elem_flag_test(f, BM_ELEM_DRAW) ? 1 : 0;
}

static Mesh *mesh_boolean_calc(const Mesh *mesh_a, const Mesh *mesh_b, int boolean_mode)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh_a, mesh_b);

  BMesh *bm;
  {
    struct BMeshCreateParams bmesh_create_params = {0};
    bmesh_create_params.use_toolflags = false;
    bm = BM_mesh_create(&allocsize, &bmesh_create_params);
  }

  {
    struct BMeshFromMeshParams bmesh_from_mesh_params = {0};
    bmesh_from_mesh_params.calc_face_normal = true;
    BM_mesh_bm_from_me(bm, mesh_b, &bmesh_from_mesh_params);
    BM_mesh_bm_from_me(bm, mesh_a, &bmesh_from_mesh_params);
  }

  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  int tottri;
  BMLoop *(*looptris)[3] = (BMLoop *
                            (*)[3])(MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__));
  BM_mesh_calc_tessellation_beauty(bm, looptris, &tottri);

  const int i_faces_end = mesh_b->totpoly;

  /* We need face normals because of 'BM_face_split_edgenet'
   * we could calculate on the fly too (before calling split). */

  int i = 0;
  BMIter iter;
  BMFace *bm_face;
  BM_ITER_MESH (bm_face, &iter, bm, BM_FACES_OF_MESH) {
    normalize_v3(bm_face->no);

    /* Temp tag to test which side split faces are from. */
    BM_elem_flag_enable(bm_face, BM_ELEM_DRAW);

    i++;
    if (i == i_faces_end) {
      break;
    }
  }

  BM_mesh_boolean(
      bm, looptris, tottri, bm_face_isect_pair, nullptr, 2, false, false, false, boolean_mode);

  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh_a);
  BM_mesh_free(bm);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  MEM_freeN(looptris);

  return result;
}

namespace blender::nodes {
static void geo_node_boolean_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set_in_a = params.extract_input<GeometrySet>("Geometry 1");
  GeometrySet geometry_set_in_b = params.extract_input<GeometrySet>("Geometry 2");
  GeometrySet geometry_set_out;

  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)params.node().custom1;
  if (operation < 0 || operation > 2) {
    BLI_assert(false);
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  /* TODO: Boolean does support an input of multiple meshes. Currently they must all be
   * converted to BMesh before running the operation though. D9957 will make it possible
   * to use the mesh structure directly. */
  geometry_set_in_a = geometry_set_realize_instances(geometry_set_in_a);
  geometry_set_in_b = geometry_set_realize_instances(geometry_set_in_b);

  const Mesh *mesh_in_a = geometry_set_in_a.get_mesh_for_read();
  const Mesh *mesh_in_b = geometry_set_in_b.get_mesh_for_read();

  if (mesh_in_a == nullptr || mesh_in_b == nullptr) {
    if (operation == GEO_NODE_BOOLEAN_UNION) {
      if (mesh_in_a != nullptr) {
        params.set_output("Geometry", geometry_set_in_a);
      }
      else {
        params.set_output("Geometry", geometry_set_in_b);
      }
    }
    else {
      params.set_output("Geometry", geometry_set_in_a);
    }
    return;
  }

  Mesh *mesh_out = mesh_boolean_calc(mesh_in_a, mesh_in_b, operation);
  geometry_set_out = GeometrySet::create_with_mesh(mesh_out);

  params.set_output("Geometry", std::move(geometry_set_out));
}
}  // namespace blender::nodes

void register_node_type_geo_boolean()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_BOOLEAN, "Boolean", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_boolean_in, geo_node_boolean_out);
  ntype.draw_buttons = geo_node_boolean_layout;
  ntype.geometry_node_execute = blender::nodes::geo_node_boolean_exec;
  nodeRegisterType(&ntype);
}
