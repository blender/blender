/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#include <algorithm>
#include <iostream>

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWPolygons.h"

#include "MEM_guardedalloc.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "ArmatureImporter.h"
#include "MeshImporter.h"
#include "collada_utils.h"

using blender::float3;
using blender::MutableSpan;

/* get node name, or fall back to original id if not present (name is optional) */
template<class T> static std::string bc_get_dae_name(T *node)
{
  return node->getName().empty() ? node->getOriginalId() : node->getName();
}

static const char *bc_primTypeToStr(COLLADAFW::MeshPrimitive::PrimitiveType type)
{
  switch (type) {
    case COLLADAFW::MeshPrimitive::LINES:
      return "LINES";
    case COLLADAFW::MeshPrimitive::LINE_STRIPS:
      return "LINESTRIPS";
    case COLLADAFW::MeshPrimitive::POLYGONS:
      return "POLYGONS";
    case COLLADAFW::MeshPrimitive::POLYLIST:
      return "POLYLIST";
    case COLLADAFW::MeshPrimitive::TRIANGLES:
      return "TRIANGLES";
    case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
      return "TRIANGLE_FANS";
    case COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS:
      return "TRIANGLE_STRIPS";
    case COLLADAFW::MeshPrimitive::POINTS:
      return "POINTS";
    case COLLADAFW::MeshPrimitive::UNDEFINED_PRIMITIVE_TYPE:
      return "UNDEFINED_PRIMITIVE_TYPE";
  }
  return "UNKNOWN";
}

static const char *bc_geomTypeToStr(COLLADAFW::Geometry::GeometryType type)
{
  switch (type) {
    case COLLADAFW::Geometry::GEO_TYPE_MESH:
      return "MESH";
    case COLLADAFW::Geometry::GEO_TYPE_SPLINE:
      return "SPLINE";
    case COLLADAFW::Geometry::GEO_TYPE_CONVEX_MESH:
      return "CONVEX_MESH";
    case COLLADAFW::Geometry::GEO_TYPE_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

UVDataWrapper::UVDataWrapper(COLLADAFW::MeshVertexData &vdata) : mVData(&vdata) {}

#ifdef COLLADA_DEBUG
void WVDataWrapper::print()
{
  fprintf(stderr, "UVs:\n");
  switch (mVData->getType()) {
    case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT: {
      COLLADAFW::ArrayPrimitiveType<float> *values = mVData->getFloatValues();
      if (values->getCount()) {
        for (int i = 0; i < values->getCount(); i += 2) {
          fprintf(stderr, "%.1f, %.1f\n", (*values)[i], (*values)[i + 1]);
        }
      }
    } break;
    case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE: {
      COLLADAFW::ArrayPrimitiveType<double> *values = mVData->getDoubleValues();
      if (values->getCount()) {
        for (int i = 0; i < values->getCount(); i += 2) {
          fprintf(stderr, "%.1f, %.1f\n", float((*values)[i]), float((*values)[i + 1]));
        }
      }
    } break;
  }
  fprintf(stderr, "\n");
}
#endif

void UVDataWrapper::getUV(int uv_index, float *uv)
{
  int stride = mVData->getStride(0);
  if (stride == 0) {
    stride = 2;
  }

  switch (mVData->getType()) {
    case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT: {
      COLLADAFW::ArrayPrimitiveType<float> *values = mVData->getFloatValues();
      if (values->empty()) {
        return;
      }
      uv[0] = (*values)[uv_index * stride];
      uv[1] = (*values)[uv_index * stride + 1];

    } break;
    case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE: {
      COLLADAFW::ArrayPrimitiveType<double> *values = mVData->getDoubleValues();
      if (values->empty()) {
        return;
      }
      uv[0] = float((*values)[uv_index * stride]);
      uv[1] = float((*values)[uv_index * stride + 1]);

    } break;
    case COLLADAFW::MeshVertexData::DATA_TYPE_UNKNOWN:
    default:
      fprintf(stderr, "MeshImporter.getUV(): unknown data type\n");
  }
}

VCOLDataWrapper::VCOLDataWrapper(COLLADAFW::MeshVertexData &vdata) : mVData(&vdata) {}

template<typename T>
static void colladaAddColor(T values, MLoopCol *mloopcol, int v_index, int stride)
{
  if (values->empty() || values->getCount() < (v_index + 1) * stride) {
    fprintf(stderr,
            "VCOLDataWrapper.getvcol(): Out of Bounds error: index %d points outside value "
            "list of length %zd (with stride=%d) \n",
            v_index,
            values->getCount(),
            stride);
    return;
  }

  mloopcol->r = unit_float_to_uchar_clamp((*values)[v_index * stride]);
  mloopcol->g = unit_float_to_uchar_clamp((*values)[v_index * stride + 1]);
  mloopcol->b = unit_float_to_uchar_clamp((*values)[v_index * stride + 2]);
  if (stride == 4) {
    mloopcol->a = unit_float_to_uchar_clamp((*values)[v_index * stride + 3]);
  }
}

void VCOLDataWrapper::get_vcol(int v_index, MLoopCol *mloopcol)
{
  int stride = mVData->getStride(0);
  if (stride == 0) {
    stride = 3;
  }

  switch (mVData->getType()) {
    case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT: {
      COLLADAFW::ArrayPrimitiveType<float> *values = mVData->getFloatValues();
      colladaAddColor<COLLADAFW::ArrayPrimitiveType<float> *>(values, mloopcol, v_index, stride);
    } break;

    case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE: {
      COLLADAFW::ArrayPrimitiveType<double> *values = mVData->getDoubleValues();
      colladaAddColor<COLLADAFW::ArrayPrimitiveType<double> *>(values, mloopcol, v_index, stride);
    } break;

    default:
      fprintf(stderr, "VCOLDataWrapper.getvcol(): unknown data type\n");
  }
}

MeshImporter::MeshImporter(UnitConverter *unitconv,
                           bool use_custom_normals,
                           ArmatureImporter *arm,
                           Main *bmain,
                           Scene *sce,
                           ViewLayer *view_layer)
    : unitconverter(unitconv),
      use_custom_normals(use_custom_normals),
      m_bmain(bmain),
      scene(sce),
      view_layer(view_layer),
      armature_importer(arm)
{
  /* pass */
}

bool MeshImporter::set_poly_indices(int *face_verts,
                                    int loop_index,
                                    const uint *indices,
                                    int loop_count)
{
  bool broken_loop = false;
  for (int index = 0; index < loop_count; index++) {

    /* Test if loop defines a hole */
    if (!broken_loop) {
      for (int i = 0; i < index; i++) {
        if (indices[i] == indices[index]) {
          /* duplicate index -> not good */
          broken_loop = true;
        }
      }
    }

    *face_verts = indices[index];
    face_verts++;
  }
  return broken_loop;
}

void MeshImporter::set_vcol(MLoopCol *mloopcol,
                            VCOLDataWrapper &vob,
                            int loop_index,
                            COLLADAFW::IndexList &index_list,
                            int count)
{
  int index;
  for (index = 0; index < count; index++, mloopcol++) {
    int v_index = index_list.getIndex(index + loop_index);
    vob.get_vcol(v_index, mloopcol);
  }
}

void MeshImporter::set_face_uv(blender::float2 *mloopuv,
                               UVDataWrapper &uvs,
                               int start_index,
                               COLLADAFW::IndexList &index_list,
                               int count)
{
  /* per face vertex indices, this means for quad we have 4 indices, not 8 */
  COLLADAFW::UIntValuesArray &indices = index_list.getIndices();

  for (int index = 0; index < count; index++) {
    int uv_index = indices[index + start_index];
    uvs.getUV(uv_index, mloopuv[index]);
  }
}

#ifdef COLLADA_DEBUG
void MeshImporter::print_index_list(COLLADAFW::IndexList &index_list)
{
  fprintf(stderr, "Index list for \"%s\":\n", index_list.getName().c_str());
  for (int i = 0; i < index_list.getIndicesCount(); i += 2) {
    fprintf(stderr, "%u, %u\n", index_list.getIndex(i), index_list.getIndex(i + 1));
  }
  fprintf(stderr, "\n");
}
#endif

bool MeshImporter::is_nice_mesh(COLLADAFW::Mesh *mesh)
{
  COLLADAFW::MeshPrimitiveArray &prim_arr = mesh->getMeshPrimitives();

  const std::string &name = bc_get_dae_name(mesh);

  for (uint i = 0; i < prim_arr.getCount(); i++) {

    COLLADAFW::MeshPrimitive *mp = prim_arr[i];
    COLLADAFW::MeshPrimitive::PrimitiveType type = mp->getPrimitiveType();

    const char *type_str = bc_primTypeToStr(type);

    /* OpenCollada passes POLYGONS type for `<polylist>`. */
    if (ELEM(type, COLLADAFW::MeshPrimitive::POLYLIST, COLLADAFW::MeshPrimitive::POLYGONS)) {

      COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
      COLLADAFW::Polygons::VertexCountArray &vca = mpvc->getGroupedVerticesVertexCountArray();

      int hole_count = 0;
      int nonface_count = 0;

      for (uint j = 0; j < vca.getCount(); j++) {
        int count = vca[j];
        if (abs(count) < 3) {
          nonface_count++;
        }

        if (count < 0) {
          hole_count++;
        }
      }

      if (hole_count > 0) {
        fprintf(stderr,
                "WARNING: Primitive %s in %s: %d holes not imported (unsupported)\n",
                type_str,
                name.c_str(),
                hole_count);
      }

      if (nonface_count > 0) {
        fprintf(stderr,
                "WARNING: Primitive %s in %s: %d faces with vertex count < 3 (rejected)\n",
                type_str,
                name.c_str(),
                nonface_count);
      }
    }

    else if (type == COLLADAFW::MeshPrimitive::LINES) {
      /* TODO: Add Checker for line syntax here */
    }

    else if (!ELEM(type,
                   COLLADAFW::MeshPrimitive::TRIANGLES,
                   COLLADAFW::MeshPrimitive::TRIANGLE_FANS)) {
      fprintf(stderr, "ERROR: Primitive type %s is not supported.\n", type_str);
      return false;
    }
  }

  return true;
}

void MeshImporter::read_vertices(COLLADAFW::Mesh *mesh, Mesh *me)
{
  /* vertices */
  COLLADAFW::MeshVertexData &pos = mesh->getPositions();
  if (pos.empty()) {
    return;
  }

  int stride = pos.getStride(0);
  if (stride == 0) {
    stride = 3;
  }

  me->totvert = pos.getFloatValues()->getCount() / stride;
  CustomData_add_layer_named(
      &me->vert_data, CD_PROP_FLOAT3, CD_CONSTRUCT, me->totvert, "position");
  MutableSpan<float3> positions = me->vert_positions_for_write();
  for (const int i : positions.index_range()) {
    get_vector(positions[i], pos, i, stride);
  }
}

bool MeshImporter::primitive_has_useable_normals(COLLADAFW::MeshPrimitive *mp)
{

  bool has_useable_normals = false;

  int normals_count = mp->getNormalIndices().getCount();
  if (normals_count > 0) {
    int index_count = mp->getPositionIndices().getCount();
    if (index_count == normals_count) {
      has_useable_normals = true;
    }
    else {
      fprintf(stderr,
              "Warning: Number of normals %d is different from the number of vertices %d, "
              "skipping normals\n",
              normals_count,
              index_count);
    }
  }

  return has_useable_normals;
}

bool MeshImporter::primitive_has_faces(COLLADAFW::MeshPrimitive *mp)
{

  bool has_faces = false;
  int type = mp->getPrimitiveType();
  switch (type) {
    case COLLADAFW::MeshPrimitive::TRIANGLES:
    case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
    case COLLADAFW::MeshPrimitive::POLYLIST:
    case COLLADAFW::MeshPrimitive::POLYGONS: {
      has_faces = true;
      break;
    }
    default: {
      has_faces = false;
      break;
    }
  }
  return has_faces;
}

static std::string extract_vcolname(const COLLADAFW::String &collada_id)
{
  std::string colname = collada_id;
  int spos = colname.find("-mesh-colors-");
  if (spos != std::string::npos) {
    colname = colname.substr(spos + 13);
  }
  return colname;
}

void MeshImporter::allocate_poly_data(COLLADAFW::Mesh *collada_mesh, Mesh *me)
{
  COLLADAFW::MeshPrimitiveArray &prim_arr = collada_mesh->getMeshPrimitives();
  int total_poly_count = 0;
  int total_loop_count = 0;

  /* collect edge_count and face_count from all parts */
  for (int i = 0; i < prim_arr.getCount(); i++) {
    COLLADAFW::MeshPrimitive *mp = prim_arr[i];
    int type = mp->getPrimitiveType();
    switch (type) {
      case COLLADAFW::MeshPrimitive::TRIANGLES:
      case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
      case COLLADAFW::MeshPrimitive::POLYLIST:
      case COLLADAFW::MeshPrimitive::POLYGONS: {
        COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
        size_t prim_poly_count = mpvc->getFaceCount();

        size_t prim_loop_count = 0;
        for (int index = 0; index < prim_poly_count; index++) {
          int vcount = get_vertex_count(mpvc, index);
          if (vcount > 0) {
            prim_loop_count += vcount;
            total_poly_count++;
          }
          else {
            /* TODO: this is a hole and not another polygon! */
          }
        }

        total_loop_count += prim_loop_count;

        break;
      }
      default:
        break;
    }
  }

  /* Add the data containers */
  if (total_poly_count > 0) {
    me->faces_num = total_poly_count;
    me->totloop = total_loop_count;
    BKE_mesh_face_offsets_ensure_alloc(me);
    CustomData_add_layer_named(
        &me->loop_data, CD_PROP_INT32, CD_SET_DEFAULT, me->totloop, ".corner_vert");

    uint totuvset = collada_mesh->getUVCoords().getInputInfosArray().getCount();
    for (int i = 0; i < totuvset; i++) {
      if (collada_mesh->getUVCoords().getLength(i) == 0) {
        totuvset = 0;
        break;
      }
    }

    if (totuvset > 0) {
      for (int i = 0; i < totuvset; i++) {
        COLLADAFW::MeshVertexData::InputInfos *info =
            collada_mesh->getUVCoords().getInputInfosArray()[i];
        COLLADAFW::String &uvname = info->mName;
        /* Allocate space for UV_data */
        CustomData_add_layer_named(
            &me->loop_data, CD_PROP_FLOAT2, CD_SET_DEFAULT, me->totloop, uvname.c_str());
      }
      /* activate the first uv map */
      CustomData_set_layer_active(&me->loop_data, CD_PROP_FLOAT2, 0);
    }

    int totcolset = collada_mesh->getColors().getInputInfosArray().getCount();
    if (totcolset > 0) {
      for (int i = 0; i < totcolset; i++) {
        COLLADAFW::MeshVertexData::InputInfos *info =
            collada_mesh->getColors().getInputInfosArray()[i];
        COLLADAFW::String colname = extract_vcolname(info->mName);
        CustomData_add_layer_named(
            &me->loop_data, CD_PROP_BYTE_COLOR, CD_SET_DEFAULT, me->totloop, colname.c_str());
      }
      BKE_id_attributes_active_color_set(
          &me->id, CustomData_get_layer_name(&me->loop_data, CD_PROP_BYTE_COLOR, 0));
      BKE_id_attributes_default_color_set(
          &me->id, CustomData_get_layer_name(&me->loop_data, CD_PROP_BYTE_COLOR, 0));
    }
  }
}

uint MeshImporter::get_vertex_count(COLLADAFW::Polygons *mp, int index)
{
  int type = mp->getPrimitiveType();
  int result;
  switch (type) {
    case COLLADAFW::MeshPrimitive::TRIANGLES:
    case COLLADAFW::MeshPrimitive::TRIANGLE_FANS: {
      result = 3;
      break;
    }
    case COLLADAFW::MeshPrimitive::POLYLIST:
    case COLLADAFW::MeshPrimitive::POLYGONS: {
      result = mp->getGroupedVerticesVertexCountArray()[index];
      break;
    }
    default: {
      result = -1;
      break;
    }
  }
  return result;
}

uint MeshImporter::get_loose_edge_count(COLLADAFW::Mesh *mesh)
{
  COLLADAFW::MeshPrimitiveArray &prim_arr = mesh->getMeshPrimitives();
  int loose_edge_count = 0;

  /* collect edge_count and face_count from all parts */
  for (int i = 0; i < prim_arr.getCount(); i++) {
    COLLADAFW::MeshPrimitive *mp = prim_arr[i];
    int type = mp->getPrimitiveType();
    switch (type) {
      case COLLADAFW::MeshPrimitive::LINES: {
        size_t prim_totface = mp->getFaceCount();
        loose_edge_count += prim_totface;
        break;
      }
      default:
        break;
    }
  }
  return loose_edge_count;
}

void MeshImporter::mesh_add_edges(Mesh *mesh, int len)
{
  CustomData edge_data;
  int totedge;

  if (len == 0) {
    return;
  }

  totedge = mesh->totedge + len;

  /* Update custom-data. */
  CustomData_copy_layout(
      &mesh->edge_data, &edge_data, CD_MASK_MESH.emask, CD_SET_DEFAULT, totedge);
  CustomData_copy_data(&mesh->edge_data, &edge_data, 0, 0, mesh->totedge);

  if (!CustomData_has_layer_named(&edge_data, CD_PROP_INT32_2D, ".edge_verts")) {
    CustomData_add_layer_named(&edge_data, CD_PROP_INT32_2D, CD_CONSTRUCT, totedge, ".edge_verts");
  }

  CustomData_free(&mesh->edge_data, mesh->totedge);
  mesh->edge_data = edge_data;
  mesh->totedge = totedge;
}

void MeshImporter::read_lines(COLLADAFW::Mesh *mesh, Mesh *me)
{
  uint loose_edge_count = get_loose_edge_count(mesh);
  if (loose_edge_count > 0) {

    uint face_edge_count = me->totedge;
    /* uint total_edge_count = loose_edge_count + face_edge_count; */ /* UNUSED */

    mesh_add_edges(me, loose_edge_count);
    MutableSpan<blender::int2> edges = me->edges_for_write();
    blender::int2 *edge = edges.data() + face_edge_count;

    COLLADAFW::MeshPrimitiveArray &prim_arr = mesh->getMeshPrimitives();

    for (int index = 0; index < prim_arr.getCount(); index++) {
      COLLADAFW::MeshPrimitive *mp = prim_arr[index];

      int type = mp->getPrimitiveType();
      if (type == COLLADAFW::MeshPrimitive::LINES) {
        uint edge_count = mp->getFaceCount();
        uint *indices = mp->getPositionIndices().getData();

        for (int j = 0; j < edge_count; j++, edge++) {
          (*edge)[0] = indices[2 * j];
          (*edge)[1] = indices[2 * j + 1];
        }
      }
    }
  }
}

void MeshImporter::read_polys(COLLADAFW::Mesh *collada_mesh,
                              Mesh *me,
                              blender::Vector<blender::float3> &loop_normals)
{
  uint i;

  allocate_poly_data(collada_mesh, me);

  UVDataWrapper uvs(collada_mesh->getUVCoords());
  VCOLDataWrapper vcol(collada_mesh->getColors());

  MutableSpan<int> face_offsets = me->face_offsets_for_write();
  MutableSpan<int> corner_verts = me->corner_verts_for_write();
  int face_index = 0;
  int loop_index = 0;

  MaterialIdPrimitiveArrayMap mat_prim_map;

  int *material_indices = BKE_mesh_material_indices_for_write(me);
  bool *sharp_faces = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &me->face_data, CD_PROP_BOOL, "sharp_face", me->faces_num));
  if (!sharp_faces) {
    sharp_faces = static_cast<bool *>(CustomData_add_layer_named(
        &me->face_data, CD_PROP_BOOL, CD_SET_DEFAULT, me->faces_num, "sharp_face"));
  }

  COLLADAFW::MeshPrimitiveArray &prim_arr = collada_mesh->getMeshPrimitives();
  COLLADAFW::MeshVertexData &nor = collada_mesh->getNormals();

  for (i = 0; i < prim_arr.getCount(); i++) {

    COLLADAFW::MeshPrimitive *mp = prim_arr[i];

    /* faces */
    size_t prim_faces_num = mp->getFaceCount();
    uint *position_indices = mp->getPositionIndices().getData();
    uint *normal_indices = mp->getNormalIndices().getData();

    bool mp_has_normals = primitive_has_useable_normals(mp);
    bool mp_has_faces = primitive_has_faces(mp);

    int collada_meshtype = mp->getPrimitiveType();

    /* Since we cannot set `poly->mat_nr` here, we store a portion of `me->mpoly` in Primitive. */
    Primitive prim = {face_index, &material_indices[face_index], 0};

    /* If MeshPrimitive is TRIANGLE_FANS we split it into triangles
     * The first triangle-fan vertex will be the first vertex in every triangle
     * XXX The proper function of TRIANGLE_FANS is not tested!!!
     * XXX In particular the handling of the normal_indices is very wrong */
    /* TODO: UV, vertex color and custom normal support */
    if (collada_meshtype == COLLADAFW::MeshPrimitive::TRIANGLE_FANS) {
      uint grouped_vertex_count = mp->getGroupedVertexElementsCount();
      for (uint group_index = 0; group_index < grouped_vertex_count; group_index++) {
        uint first_vertex = position_indices[0]; /* Store first triangle-fan vertex. */
        uint first_normal = normal_indices[0];   /* Store first triangle-fan vertex normal. */
        uint vertex_count = mp->getGroupedVerticesVertexCount(group_index);

        for (uint vertex_index = 0; vertex_index < vertex_count - 2; vertex_index++) {
          /* For each triangle store indices of its 3 vertices */
          uint triangle_vertex_indices[3] = {
              first_vertex, position_indices[1], position_indices[2]};
          face_offsets[face_index] = loop_index;
          set_poly_indices(&corner_verts[loop_index], loop_index, triangle_vertex_indices, 3);

          if (mp_has_normals) { /* vertex normals, same implementation as for the triangles */
            /* The same for vertices normals. */
            uint vertex_normal_indices[3] = {first_normal, normal_indices[1], normal_indices[2]};
            sharp_faces[face_index] = is_flat_face(vertex_normal_indices, nor, 3);
            normal_indices++;
          }

          face_index++;
          loop_index += 3;
          prim.faces_num++;
        }

        /* Moving cursor to the next triangle fan. */
        if (mp_has_normals) {
          normal_indices += 2;
        }

        position_indices += 2;
      }
    }

    if (ELEM(collada_meshtype,
             COLLADAFW::MeshPrimitive::POLYLIST,
             COLLADAFW::MeshPrimitive::POLYGONS,
             COLLADAFW::MeshPrimitive::TRIANGLES))
    {
      COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
      uint start_index = 0;

      COLLADAFW::IndexListArray &index_list_array_uvcoord = mp->getUVCoordIndicesArray();
      COLLADAFW::IndexListArray &index_list_array_vcolor = mp->getColorIndicesArray();

      int invalid_loop_holes = 0;
      for (uint j = 0; j < prim_faces_num; j++) {

        /* Vertices in polygon: */
        int vcount = get_vertex_count(mpvc, j);
        if (vcount < 0) {
          continue; /* TODO: add support for holes */
        }

        face_offsets[face_index] = loop_index;
        bool broken_loop = set_poly_indices(
            &corner_verts[loop_index], loop_index, position_indices, vcount);
        if (broken_loop) {
          invalid_loop_holes += 1;
        }

        for (uint uvset_index = 0; uvset_index < index_list_array_uvcoord.getCount();
             uvset_index++) {
          COLLADAFW::IndexList &index_list = *index_list_array_uvcoord[uvset_index];
          blender::float2 *mloopuv = static_cast<blender::float2 *>(
              CustomData_get_layer_named_for_write(
                  &me->loop_data, CD_PROP_FLOAT2, index_list.getName().c_str(), me->totloop));
          if (mloopuv == nullptr) {
            fprintf(stderr,
                    "Collada import: Mesh [%s] : Unknown reference to TEXCOORD [#%s].\n",
                    me->id.name,
                    index_list.getName().c_str());
          }
          else {
            set_face_uv(mloopuv + loop_index,
                        uvs,
                        start_index,
                        *index_list_array_uvcoord[uvset_index],
                        vcount);
          }
        }

        if (mp_has_normals) {
          /* If it turns out that we have complete custom normals for each poly
           * and we want to use custom normals, this will be overridden. */
          sharp_faces[face_index] = is_flat_face(normal_indices, nor, vcount);

          if (use_custom_normals) {
            /* Store the custom normals for later application. */
            float vert_normal[3];
            uint *cur_normal = normal_indices;
            for (int k = 0; k < vcount; k++, cur_normal++) {
              get_vector(vert_normal, nor, *cur_normal, 3);
              normalize_v3(vert_normal);
              loop_normals.append(vert_normal);
            }
          }
        }

        if (mp->hasColorIndices()) {
          int vcolor_count = index_list_array_vcolor.getCount();

          for (uint vcolor_index = 0; vcolor_index < vcolor_count; vcolor_index++) {

            COLLADAFW::IndexList &color_index_list = *mp->getColorIndices(vcolor_index);
            COLLADAFW::String colname = extract_vcolname(color_index_list.getName());
            MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer_named_for_write(
                &me->loop_data, CD_PROP_BYTE_COLOR, colname.c_str(), me->totloop);
            if (mloopcol == nullptr) {
              fprintf(stderr,
                      "Collada import: Mesh [%s] : Unknown reference to VCOLOR [#%s].\n",
                      me->id.name,
                      color_index_list.getName().c_str());
            }
            else {
              set_vcol(mloopcol + loop_index, vcol, start_index, color_index_list, vcount);
            }
          }
        }

        face_index++;
        loop_index += vcount;
        start_index += vcount;
        prim.faces_num++;

        if (mp_has_normals) {
          normal_indices += vcount;
        }

        position_indices += vcount;
      }

      if (invalid_loop_holes > 0) {
        fprintf(stderr,
                "Collada import: Mesh [%s] : contains %d unsupported loops (holes).\n",
                me->id.name,
                invalid_loop_holes);
      }
    }

    else if (collada_meshtype == COLLADAFW::MeshPrimitive::LINES) {
      continue; /* read the lines later after all the rest is done */
    }

    if (mp_has_faces) {
      mat_prim_map[mp->getMaterialId()].push_back(prim);
    }
  }

  geom_uid_mat_mapping_map[collada_mesh->getUniqueId()] = mat_prim_map;
}

void MeshImporter::get_vector(float v[3], COLLADAFW::MeshVertexData &arr, int i, int stride)
{
  i *= stride;

  switch (arr.getType()) {
    case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT: {
      COLLADAFW::ArrayPrimitiveType<float> *values = arr.getFloatValues();
      if (values->empty()) {
        return;
      }

      v[0] = (*values)[i++];
      v[1] = (*values)[i++];
      if (stride >= 3) {
        v[2] = (*values)[i];
      }
      else {
        v[2] = 0.0f;
      }

    } break;
    case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE: {
      COLLADAFW::ArrayPrimitiveType<double> *values = arr.getDoubleValues();
      if (values->empty()) {
        return;
      }

      v[0] = float((*values)[i++]);
      v[1] = float((*values)[i++]);
      if (stride >= 3) {
        v[2] = float((*values)[i]);
      }
      else {
        v[2] = 0.0f;
      }
    } break;
    default:
      break;
  }
}

bool MeshImporter::is_flat_face(uint *nind, COLLADAFW::MeshVertexData &nor, int count)
{
  float a[3], b[3];

  get_vector(a, nor, *nind, 3);
  normalize_v3(a);

  nind++;

  for (int i = 1; i < count; i++, nind++) {
    get_vector(b, nor, *nind, 3);
    normalize_v3(b);

    float dp = dot_v3v3(a, b);

    if (dp < 0.99999f || dp > 1.00001f) {
      return false;
    }
  }

  return true;
}

Object *MeshImporter::get_object_by_geom_uid(const COLLADAFW::UniqueId &geom_uid)
{
  if (uid_object_map.find(geom_uid) != uid_object_map.end()) {
    return uid_object_map[geom_uid];
  }
  return nullptr;
}

Mesh *MeshImporter::get_mesh_by_geom_uid(const COLLADAFW::UniqueId &geom_uid)
{
  if (uid_mesh_map.find(geom_uid) != uid_mesh_map.end()) {
    return uid_mesh_map[geom_uid];
  }
  return nullptr;
}

std::string *MeshImporter::get_geometry_name(const std::string &mesh_name)
{
  if (this->mesh_geom_map.find(mesh_name) != this->mesh_geom_map.end()) {
    return &this->mesh_geom_map[mesh_name];
  }
  return nullptr;
}

static bool bc_has_out_of_bound_indices(Mesh *me)
{
  for (const int vert_i : me->corner_verts()) {
    if (vert_i >= me->totvert) {
      return true;
    }
  }
  return false;
}

/**
 * this function checks if both objects have the same
 * materials assigned to Object (in the same order)
 * returns true if condition matches, otherwise false;
 */
static bool bc_has_same_material_configuration(Object *ob1, Object *ob2)
{
  if (ob1->totcol != ob2->totcol) {
    return false; /* not same number of materials */
  }
  if (ob1->totcol == 0) {
    return false; /* no material at all */
  }

  for (int index = 0; index < ob1->totcol; index++) {
    if (ob1->matbits[index] != ob2->matbits[index]) {
      return false; /* shouldn't happen */
    }
    if (ob1->matbits[index] == 0) {
      return false; /* shouldn't happen */
    }
    if (ob1->mat[index] != ob2->mat[index]) {
      return false; /* different material assignment */
    }
  }
  return true;
}

/**
 * Caution here: This code assumes that all materials are assigned to Object
 * and no material is assigned to Data.
 * That is true right after the objects have been imported.
 */
static void bc_copy_materials_to_data(Object *ob, Mesh *me)
{
  for (int index = 0; index < ob->totcol; index++) {
    ob->matbits[index] = 0;
    me->mat[index] = ob->mat[index];
  }
}

/**
 * Remove all references to materials from the object.
 */
static void bc_remove_materials_from_object(Object *ob, Mesh *me)
{
  for (int index = 0; index < ob->totcol; index++) {
    ob->matbits[index] = 0;
    ob->mat[index] = nullptr;
  }
}

std::vector<Object *> MeshImporter::get_all_users_of(Mesh *reference_mesh)
{
  std::vector<Object *> mesh_users;
  for (Object *ob : imported_objects) {
    if (bc_is_marked(ob)) {
      bc_remove_mark(ob);
      Mesh *me = (Mesh *)ob->data;
      if (me == reference_mesh) {
        mesh_users.push_back(ob);
      }
    }
  }
  return mesh_users;
}

void MeshImporter::optimize_material_assignements()
{
  for (Object *ob : imported_objects) {
    Mesh *me = (Mesh *)ob->data;
    if (ID_REAL_USERS(&me->id) == 1) {
      bc_copy_materials_to_data(ob, me);
      bc_remove_materials_from_object(ob, me);
      bc_remove_mark(ob);
    }
    else if (ID_REAL_USERS(&me->id) > 1) {
      bool can_move = true;
      std::vector<Object *> mesh_users = get_all_users_of(me);
      if (mesh_users.size() > 1) {
        Object *ref_ob = mesh_users[0];
        for (int index = 1; index < mesh_users.size(); index++) {
          if (!bc_has_same_material_configuration(ref_ob, mesh_users[index])) {
            can_move = false;
            break;
          }
        }
        if (can_move) {
          bc_copy_materials_to_data(ref_ob, me);
          for (Object *object : mesh_users) {
            bc_remove_materials_from_object(object, me);
            bc_remove_mark(object);
          }
        }
      }
    }
  }
}

void MeshImporter::assign_material_to_geom(
    COLLADAFW::MaterialBinding cmaterial,
    std::map<COLLADAFW::UniqueId, Material *> &uid_material_map,
    Object *ob,
    const COLLADAFW::UniqueId *geom_uid,
    short mat_index)
{
  const COLLADAFW::UniqueId &ma_uid = cmaterial.getReferencedMaterial();

  /* do we know this material? */
  if (uid_material_map.find(ma_uid) == uid_material_map.end()) {

    fprintf(stderr, "Cannot find material by UID.\n");
    return;
  }

  /* first time we get geom_uid, ma_uid pair. Save for later check. */
  materials_mapped_to_geom.insert(
      std::pair<COLLADAFW::UniqueId, COLLADAFW::UniqueId>(*geom_uid, ma_uid));

  Material *ma = uid_material_map[ma_uid];

  /* Attention! This temporarily assigns material to object on purpose!
   * See note above. */
  ob->actcol = 0;
  BKE_object_material_assign(m_bmain, ob, ma, mat_index + 1, BKE_MAT_ASSIGN_OBJECT);

  MaterialIdPrimitiveArrayMap &mat_prim_map = geom_uid_mat_mapping_map[*geom_uid];
  COLLADAFW::MaterialId mat_id = cmaterial.getMaterialId();

  /* assign material indices to mesh faces */
  if (mat_prim_map.find(mat_id) != mat_prim_map.end()) {

    std::vector<Primitive> &prims = mat_prim_map[mat_id];

    std::vector<Primitive>::iterator it;

    for (it = prims.begin(); it != prims.end(); it++) {
      Primitive &prim = *it;

      for (int i = 0; i < prim.faces_num; i++) {
        prim.material_indices[i] = mat_index;
      }
    }
  }
}

Object *MeshImporter::create_mesh_object(
    COLLADAFW::Node *node,
    COLLADAFW::InstanceGeometry *geom,
    bool isController,
    std::map<COLLADAFW::UniqueId, Material *> &uid_material_map)
{
  const COLLADAFW::UniqueId *geom_uid = &geom->getInstanciatedObjectId();

  /* check if node instantiates controller or geometry */
  if (isController) {

    geom_uid = armature_importer->get_geometry_uid(*geom_uid);

    if (!geom_uid) {
      fprintf(stderr, "Couldn't find a mesh UID by controller's UID.\n");
      return nullptr;
    }
  }
  else {

    if (uid_mesh_map.find(*geom_uid) == uid_mesh_map.end()) {
      /* this could happen if a mesh was not created
       * (e.g. if it contains unsupported geometry) */
      fprintf(stderr, "Couldn't find a mesh by UID.\n");
      return nullptr;
    }
  }
  if (!uid_mesh_map[*geom_uid]) {
    return nullptr;
  }

  /* name Object */
  const std::string &id = node->getName().empty() ? node->getOriginalId() : node->getName();
  const char *name = id.length() ? id.c_str() : nullptr;

  /* add object */
  Object *ob = bc_add_object(m_bmain, scene, view_layer, OB_MESH, name);
  bc_set_mark(ob); /* used later for material assignment optimization */

  /* store object pointer for ArmatureImporter */
  uid_object_map[*geom_uid] = ob;
  imported_objects.push_back(ob);

  /* replace ob->data freeing the old one */
  Mesh *old_mesh = (Mesh *)ob->data;
  Mesh *new_mesh = uid_mesh_map[*geom_uid];

  BKE_mesh_assign_object(m_bmain, ob, new_mesh);

  /* Because BKE_mesh_assign_object would have already decreased it... */
  id_us_plus(&old_mesh->id);

  BKE_id_free_us(m_bmain, old_mesh);

  COLLADAFW::MaterialBindingArray &mat_array = geom->getMaterialBindings();

  /* loop through geom's materials */
  for (uint i = 0; i < mat_array.getCount(); i++) {

    if (mat_array[i].getReferencedMaterial().isValid()) {
      assign_material_to_geom(mat_array[i], uid_material_map, ob, geom_uid, i);
    }
    else {
      fprintf(stderr, "invalid referenced material for %s\n", mat_array[i].getName().c_str());
    }
  }

  /* clean up the mesh */
  BKE_mesh_validate((Mesh *)ob->data, false, false);

  return ob;
}

bool MeshImporter::write_geometry(const COLLADAFW::Geometry *geom)
{

  if (geom->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH) {
    /* TODO: report warning */
    fprintf(stderr, "Mesh type %s is not supported\n", bc_geomTypeToStr(geom->getType()));
    return true;
  }

  COLLADAFW::Mesh *mesh = (COLLADAFW::Mesh *)geom;

  if (!is_nice_mesh(mesh)) {
    fprintf(stderr, "Ignoring mesh %s\n", bc_get_dae_name(mesh).c_str());
    return true;
  }

  const std::string &str_geom_id = mesh->getName().empty() ? mesh->getOriginalId() :
                                                             mesh->getName();
  Mesh *me = BKE_mesh_add(m_bmain, (char *)str_geom_id.c_str());
  id_us_min(&me->id); /* is already 1 here, but will be set later in BKE_mesh_assign_object */

  /* store the Mesh pointer to link it later with an Object
   * mesh_geom_map needed to map mesh to its geometry name (for shape key naming) */
  this->uid_mesh_map[mesh->getUniqueId()] = me;
  this->mesh_geom_map[std::string(me->id.name)] = str_geom_id;

  read_vertices(mesh, me);

  blender::Vector<blender::float3> loop_normals;
  read_polys(mesh, me, loop_normals);

  BKE_mesh_calc_edges(me, false, false);

  /* We must apply custom normals after edges have been calculated, because
   * BKE_mesh_set_custom_normals()'s internals expect me->medge to be populated
   * and for the MLoops to have correct edge indices. */
  if (use_custom_normals && !loop_normals.is_empty()) {
    /* BKE_mesh_set_custom_normals()'s internals also expect that each corner
     * has a valid vertex index, which may not be the case due to the existing
     * logic in read_faces(). This check isn't necessary in the no-custom-normals
     * case because the invalid MLoops get stripped in a later step. */
    if (bc_has_out_of_bound_indices(me)) {
      fprintf(stderr, "Can't apply custom normals, encountered invalid loop vert indices!\n");
    }
    /* There may be a mismatch in lengths if one or more of the MeshPrimitives in
     * the Geometry had missing or otherwise invalid normals. */
    else if (me->totloop != loop_normals.size()) {
      fprintf(stderr,
              "Can't apply custom normals, me->totloop != loop_normals.size() (%d != %d)\n",
              me->totloop,
              int(loop_normals.size()));
    }
    else {
      BKE_mesh_set_custom_normals(me, reinterpret_cast<float(*)[3]>(loop_normals.data()));
      me->flag |= ME_AUTOSMOOTH;
    }
  }

  /* read_lines() must be called after the face edges have been generated.
   * Otherwise the loose edges will be silently deleted again. */
  read_lines(mesh, me);

  return true;
}
