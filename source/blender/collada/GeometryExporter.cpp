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

/** \file
 * \ingroup collada
 */

#include <sstream>

#include "COLLADASWPrimitves.h"
#include "COLLADASWSource.h"
#include "COLLADASWVertices.h"
#include "COLLADABUUtils.h"

#include "GeometryExporter.h"

#include "DNA_meshdata_types.h"

extern "C" {
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
}

#include "collada_internal.h"
#include "collada_utils.h"

void GeometryExporter::exportGeom()
{
  Scene *sce = blender_context.get_scene();
  openLibrary();

  GeometryFunctor gf;
  gf.forEachMeshObjectInExportSet<GeometryExporter>(sce, *this, this->export_settings->export_set);

  closeLibrary();
}

void GeometryExporter::operator()(Object *ob)
{
  bool use_instantiation = this->export_settings->use_object_instantiation;
  Mesh *me = bc_get_mesh_copy(blender_context,
                              ob,
                              this->export_settings->export_mesh_type,
                              this->export_settings->apply_modifiers,
                              this->export_settings->triangulate);

  std::string geom_id = get_geometry_id(ob, use_instantiation);
  std::vector<Normal> nor;
  std::vector<BCPolygonNormalsIndices> norind;

  /* Skip if linked geometry was already exported from another reference */
  if (use_instantiation && exportedGeometry.find(geom_id) != exportedGeometry.end()) {
    return;
  }

  std::string geom_name = (use_instantiation) ? id_name(ob->data) : id_name(ob);
  geom_name = encode_xml(geom_name);

  exportedGeometry.insert(geom_id);

  bool has_color = (bool)CustomData_has_layer(&me->fdata, CD_MCOL);

  create_normals(nor, norind, me);

  /* openMesh(geoId, geoName, meshId) */
  openMesh(geom_id, geom_name);

  /* writes <source> for vertex coords */
  createVertsSource(geom_id, me);

  /* writes <source> for normal coords */
  createNormalsSource(geom_id, me, nor);

  bool has_uvs = (bool)CustomData_has_layer(&me->ldata, CD_MLOOPUV);

  /* writes <source> for uv coords if mesh has uv coords */
  if (has_uvs) {
    createTexcoordsSource(geom_id, me);
  }

  if (has_color) {
    createVertexColorSource(geom_id, me);
  }
  /* <vertices> */

  COLLADASW::Vertices verts(mSW);
  verts.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX));
  COLLADASW::InputList &input_list = verts.getInputList();
  COLLADASW::Input input(COLLADASW::InputSemantic::POSITION,
                         getUrlBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
  input_list.push_back(input);
  verts.add();

  createLooseEdgeList(ob, me, geom_id);

  /* Only create Polylists if number of faces > 0 */
  if (me->totface > 0) {
    /* XXX slow */
    if (ob->totcol) {
      for (int a = 0; a < ob->totcol; a++) {
        create_mesh_primitive_list(a, has_uvs, has_color, ob, me, geom_id, norind);
      }
    }
    else {
      create_mesh_primitive_list(0, has_uvs, has_color, ob, me, geom_id, norind);
    }
  }

  closeMesh();

  closeGeometry();

  if (this->export_settings->include_shapekeys) {
    Key *key = BKE_key_from_object(ob);
    if (key) {
      KeyBlock *kb = (KeyBlock *)key->block.first;
      /* skip the basis */
      kb = kb->next;
      for (; kb; kb = kb->next) {
        BKE_keyblock_convert_to_mesh(kb, me);
        export_key_mesh(ob, me, kb);
      }
    }
  }

  BKE_id_free(NULL, me);
}

void GeometryExporter::export_key_mesh(Object *ob, Mesh *me, KeyBlock *kb)
{
  std::string geom_id = get_geometry_id(ob, false) + "_morph_" + translate_id(kb->name);
  std::vector<Normal> nor;
  std::vector<BCPolygonNormalsIndices> norind;

  if (exportedGeometry.find(geom_id) != exportedGeometry.end()) {
    return;
  }

  std::string geom_name = kb->name;

  exportedGeometry.insert(geom_id);

  bool has_color = (bool)CustomData_has_layer(&me->fdata, CD_MCOL);

  create_normals(nor, norind, me);

  // openMesh(geoId, geoName, meshId)
  openMesh(geom_id, geom_name);

  /* writes <source> for vertex coords */
  createVertsSource(geom_id, me);

  /* writes <source> for normal coords */
  createNormalsSource(geom_id, me, nor);

  bool has_uvs = (bool)CustomData_has_layer(&me->ldata, CD_MLOOPUV);

  /* writes <source> for uv coords if mesh has uv coords */
  if (has_uvs) {
    createTexcoordsSource(geom_id, me);
  }

  if (has_color) {
    createVertexColorSource(geom_id, me);
  }

  /* <vertices> */

  COLLADASW::Vertices verts(mSW);
  verts.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX));
  COLLADASW::InputList &input_list = verts.getInputList();
  COLLADASW::Input input(COLLADASW::InputSemantic::POSITION,
                         getUrlBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
  input_list.push_back(input);
  verts.add();

  // createLooseEdgeList(ob, me, geom_id, norind);

  /* XXX slow */
  if (ob->totcol) {
    for (int a = 0; a < ob->totcol; a++) {
      create_mesh_primitive_list(a, has_uvs, has_color, ob, me, geom_id, norind);
    }
  }
  else {
    create_mesh_primitive_list(0, has_uvs, has_color, ob, me, geom_id, norind);
  }

  closeMesh();

  closeGeometry();
}

void GeometryExporter::createLooseEdgeList(Object *ob, Mesh *me, std::string &geom_id)
{

  MEdge *medges = me->medge;
  int totedges = me->totedge;
  int edges_in_linelist = 0;
  std::vector<unsigned int> edge_list;
  int index;

  /* Find all loose edges in Mesh
   * and save vertex indices in edge_list */
  for (index = 0; index < totedges; index++) {
    MEdge *edge = &medges[index];

    if (edge->flag & ME_LOOSEEDGE) {
      edges_in_linelist += 1;
      edge_list.push_back(edge->v1);
      edge_list.push_back(edge->v2);
    }
  }

  if (edges_in_linelist > 0) {
    /* Create the list of loose edges */
    COLLADASW::Lines lines(mSW);

    lines.setCount(edges_in_linelist);

    COLLADASW::InputList &til = lines.getInputList();

    /* creates <input> in <lines> for vertices */
    COLLADASW::Input input1(COLLADASW::InputSemantic::VERTEX,
                            getUrlBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX),
                            0);
    til.push_back(input1);

    lines.prepareToAppendValues();

    for (index = 0; index < edges_in_linelist; index++) {
      lines.appendValues(edge_list[2 * index + 1]);
      lines.appendValues(edge_list[2 * index]);
    }
    lines.finish();
  }
}

static void prepareToAppendValues(bool is_triangulated,
                                  COLLADASW::PrimitivesBase &primitive_list,
                                  std::vector<unsigned long> &vcount_list)
{
  /* performs the actual writing */
  if (is_triangulated) {
    ((COLLADASW::Triangles &)primitive_list).prepareToAppendValues();
  }
  else {
    /* sets <vcount> */
    primitive_list.setVCountList(vcount_list);
    ((COLLADASW::Polylist &)primitive_list).prepareToAppendValues();
  }
}

static void finish_and_delete_primitive_List(bool is_triangulated,
                                             COLLADASW::PrimitivesBase *primitive_list)
{
  if (is_triangulated) {
    ((COLLADASW::Triangles *)primitive_list)->finish();
  }
  else {
    ((COLLADASW::Polylist *)primitive_list)->finish();
  }
  delete primitive_list;
}

static COLLADASW::PrimitivesBase *create_primitive_list(bool is_triangulated,
                                                        COLLADASW::StreamWriter *mSW)
{
  COLLADASW::PrimitivesBase *primitive_list;

  if (is_triangulated) {
    primitive_list = new COLLADASW::Triangles(mSW);
  }
  else {
    primitive_list = new COLLADASW::Polylist(mSW);
  }
  return primitive_list;
}

static bool collect_vertex_counts_per_poly(Mesh *me,
                                           int material_index,
                                           std::vector<unsigned long> &vcount_list)
{
  MPoly *mpolys = me->mpoly;
  int totpolys = me->totpoly;
  bool is_triangulated = true;

  int i;
  /* Expecting that p->mat_nr is always 0 if the mesh has no materials assigned */
  for (i = 0; i < totpolys; i++) {
    MPoly *p = &mpolys[i];
    if (p->mat_nr == material_index) {
      int vertex_count = p->totloop;
      vcount_list.push_back(vertex_count);
      if (vertex_count != 3)
        is_triangulated = false;
    }
  }
  return is_triangulated;
}

std::string GeometryExporter::makeVertexColorSourceId(std::string &geom_id, char *layer_name)
{
  std::string result = getIdBySemantics(geom_id, COLLADASW::InputSemantic::COLOR) + "-" +
                       layer_name;
  return result;
}

/* powerful because it handles both cases when there is material and when there's not */
void GeometryExporter::create_mesh_primitive_list(short material_index,
                                                  bool has_uvs,
                                                  bool has_color,
                                                  Object *ob,
                                                  Mesh *me,
                                                  std::string &geom_id,
                                                  std::vector<BCPolygonNormalsIndices> &norind)
{

  MPoly *mpolys = me->mpoly;
  MLoop *mloops = me->mloop;
  int totpolys = me->totpoly;

  std::vector<unsigned long> vcount_list;

  bool is_triangulated = collect_vertex_counts_per_poly(me, material_index, vcount_list);
  int polygon_count = vcount_list.size();

  /* no faces using this material */
  if (polygon_count == 0) {
    fprintf(
        stderr, "%s: material with index %d is not used.\n", id_name(ob).c_str(), material_index);
    return;
  }

  Material *ma = ob->totcol ? give_current_material(ob, material_index + 1) : NULL;
  COLLADASW::PrimitivesBase *primitive_list = create_primitive_list(is_triangulated, mSW);

  /* sets count attribute in <polylist> */
  primitive_list->setCount(polygon_count);

  /* sets material name */
  if (ma) {
    std::string material_id = get_material_id(ma);
    std::ostringstream ostr;
    ostr << translate_id(material_id);
    primitive_list->setMaterial(ostr.str());
  }

  COLLADASW::Input vertex_input(COLLADASW::InputSemantic::VERTEX,
                                getUrlBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX),
                                0);
  COLLADASW::Input normals_input(COLLADASW::InputSemantic::NORMAL,
                                 getUrlBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL),
                                 1);

  COLLADASW::InputList &til = primitive_list->getInputList();
  til.push_back(vertex_input);
  til.push_back(normals_input);

  /* if mesh has uv coords writes <input> for TEXCOORD */
  int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);
  int active_uv_index = CustomData_get_active_layer_index(&me->ldata, CD_MLOOPUV);
  for (int i = 0; i < num_layers; i++) {
    int layer_index = CustomData_get_layer_index_n(&me->ldata, CD_MLOOPUV, i);
    if (!this->export_settings->active_uv_only || layer_index == active_uv_index) {

      // char *name = CustomData_get_layer_name(&me->ldata, CD_MLOOPUV, i);
      COLLADASW::Input texcoord_input(
          COLLADASW::InputSemantic::TEXCOORD,
          makeUrl(makeTexcoordSourceId(geom_id, i, this->export_settings->active_uv_only)),
          2,  // this is only until we have optimized UV sets
          (this->export_settings->active_uv_only) ? 0 : layer_index - 1 /* set (0,1,2,...) */
      );
      til.push_back(texcoord_input);
    }
  }

  int totlayer_mcol = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);
  if (totlayer_mcol > 0) {
    int map_index = 0;

    for (int a = 0; a < totlayer_mcol; a++) {
      char *layer_name = bc_CustomData_get_layer_name(&me->ldata, CD_MLOOPCOL, a);
      COLLADASW::Input input4(COLLADASW::InputSemantic::COLOR,
                              makeUrl(makeVertexColorSourceId(geom_id, layer_name)),
                              (has_uvs) ? 3 : 2,  // all color layers have same index order
                              map_index           // set number equals color map index
      );
      til.push_back(input4);
      map_index++;
    }
  }

  /* performs the actual writing */
  prepareToAppendValues(is_triangulated, *primitive_list, vcount_list);

  /* <p> */
  int texindex = 0;
  for (int i = 0; i < totpolys; i++) {
    MPoly *p = &mpolys[i];
    int loop_count = p->totloop;

    if (p->mat_nr == material_index) {
      MLoop *l = &mloops[p->loopstart];
      BCPolygonNormalsIndices normal_indices = norind[i];

      for (int j = 0; j < loop_count; j++) {
        primitive_list->appendValues(l[j].v);
        primitive_list->appendValues(normal_indices[j]);
        if (has_uvs)
          primitive_list->appendValues(texindex + j);

        if (has_color)
          primitive_list->appendValues(texindex + j);
      }
    }

    texindex += loop_count;
  }

  finish_and_delete_primitive_List(is_triangulated, primitive_list);
}

/* creates <source> for positions */
void GeometryExporter::createVertsSource(std::string geom_id, Mesh *me)
{
#if 0
  int totverts = dm->getNumVerts(dm);
  MVert *verts = dm->getVertArray(dm);
#endif
  int totverts = me->totvert;
  MVert *verts = me->mvert;

  COLLADASW::FloatSourceF source(mSW);
  source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
  source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::POSITION) +
                    ARRAY_ID_SUFFIX);
  source.setAccessorCount(totverts);
  source.setAccessorStride(3);

  COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
  param.push_back("X");
  param.push_back("Y");
  param.push_back("Z");
  /* main function, it creates <source id = "">, <float_array id = ""
   * count = ""> */
  source.prepareToAppendValues();
  /* appends data to <float_array> */
  int i = 0;
  for (i = 0; i < totverts; i++) {
    source.appendValues(verts[i].co[0], verts[i].co[1], verts[i].co[2]);
  }

  source.finish();
}

void GeometryExporter::createVertexColorSource(std::string geom_id, Mesh *me)
{
  /* Find number of vertex color layers */
  int totlayer_mcol = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);
  if (totlayer_mcol == 0)
    return;

  int map_index = 0;
  for (int a = 0; a < totlayer_mcol; a++) {

    map_index++;
    MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer_n(&me->ldata, CD_MLOOPCOL, a);

    COLLADASW::FloatSourceF source(mSW);

    char *layer_name = bc_CustomData_get_layer_name(&me->ldata, CD_MLOOPCOL, a);
    std::string layer_id = makeVertexColorSourceId(geom_id, layer_name);
    source.setId(layer_id);

    source.setNodeName(layer_name);

    source.setArrayId(layer_id + ARRAY_ID_SUFFIX);
    source.setAccessorCount(me->totloop);
    source.setAccessorStride(4);

    COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
    param.push_back("R");
    param.push_back("G");
    param.push_back("B");
    param.push_back("A");

    source.prepareToAppendValues();

    MPoly *mpoly;
    int i;
    for (i = 0, mpoly = me->mpoly; i < me->totpoly; i++, mpoly++) {
      MLoopCol *mlc = mloopcol + mpoly->loopstart;
      for (int j = 0; j < mpoly->totloop; j++, mlc++) {
        source.appendValues(mlc->r / 255.0f, mlc->g / 255.0f, mlc->b / 255.0f, mlc->a / 255.0f);
      }
    }

    source.finish();
  }
}

std::string GeometryExporter::makeTexcoordSourceId(std::string &geom_id,
                                                   int layer_index,
                                                   bool is_single_layer)
{
  char suffix[20];
  if (is_single_layer) {
    suffix[0] = '\0';
  }
  else {
    sprintf(suffix, "-%d", layer_index);
  }
  return getIdBySemantics(geom_id, COLLADASW::InputSemantic::TEXCOORD) + suffix;
}

/* creates <source> for texcoords */
void GeometryExporter::createTexcoordsSource(std::string geom_id, Mesh *me)
{

  int totpoly = me->totpoly;
  int totuv = me->totloop;
  MPoly *mpolys = me->mpoly;

  int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);

  /* write <source> for each layer
   * each <source> will get id like meshName + "map-channel-1" */
  int active_uv_index = CustomData_get_active_layer_index(&me->ldata, CD_MLOOPUV);
  for (int a = 0; a < num_layers; a++) {
    int layer_index = CustomData_get_layer_index_n(&me->ldata, CD_MLOOPUV, a);
    if (!this->export_settings->active_uv_only || layer_index == active_uv_index) {
      MLoopUV *mloops = (MLoopUV *)CustomData_get_layer_n(&me->ldata, CD_MLOOPUV, a);

      COLLADASW::FloatSourceF source(mSW);
      std::string layer_id = makeTexcoordSourceId(
          geom_id, a, this->export_settings->active_uv_only);
      source.setId(layer_id);
      source.setArrayId(layer_id + ARRAY_ID_SUFFIX);

      source.setAccessorCount(totuv);
      source.setAccessorStride(2);
      COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
      param.push_back("S");
      param.push_back("T");

      source.prepareToAppendValues();

      for (int index = 0; index < totpoly; index++) {
        MPoly *mpoly = mpolys + index;
        MLoopUV *mloop = mloops + mpoly->loopstart;
        for (int j = 0; j < mpoly->totloop; j++) {
          source.appendValues(mloop[j].uv[0], mloop[j].uv[1]);
        }
      }

      source.finish();
    }
  }
}

bool operator<(const Normal &a, const Normal &b)
{
  /* only needed to sort normal vectors and find() them later in a map.*/
  return a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
}

/* creates <source> for normals */
void GeometryExporter::createNormalsSource(std::string geom_id, Mesh *me, std::vector<Normal> &nor)
{
#if 0
  int totverts = dm->getNumVerts(dm);
  MVert *verts = dm->getVertArray(dm);
#endif

  COLLADASW::FloatSourceF source(mSW);
  source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL));
  source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL) + ARRAY_ID_SUFFIX);
  source.setAccessorCount((unsigned long)nor.size());
  source.setAccessorStride(3);
  COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
  param.push_back("X");
  param.push_back("Y");
  param.push_back("Z");

  source.prepareToAppendValues();

  std::vector<Normal>::iterator it;
  for (it = nor.begin(); it != nor.end(); it++) {
    Normal &n = *it;
    source.appendValues(n.x, n.y, n.z);
  }

  source.finish();
}

void GeometryExporter::create_normals(std::vector<Normal> &normals,
                                      std::vector<BCPolygonNormalsIndices> &polygons_normals,
                                      Mesh *me)
{
  std::map<Normal, unsigned int> shared_normal_indices;
  int last_normal_index = -1;

  MVert *verts = me->mvert;
  MLoop *mloops = me->mloop;
  float(*lnors)[3] = NULL;
  bool use_custom_normals = false;

  BKE_mesh_calc_normals_split(me);
  if (CustomData_has_layer(&me->ldata, CD_NORMAL)) {
    lnors = (float(*)[3])CustomData_get_layer(&me->ldata, CD_NORMAL);
    use_custom_normals = true;
  }

  for (int poly_index = 0; poly_index < me->totpoly; poly_index++) {
    MPoly *mpoly = &me->mpoly[poly_index];
    bool use_vertex_normals = use_custom_normals || mpoly->flag & ME_SMOOTH;

    if (!use_vertex_normals) {
      /* For flat faces use face normal as vertex normal: */

      float vector[3];
      BKE_mesh_calc_poly_normal(mpoly, mloops + mpoly->loopstart, verts, vector);

      Normal n = {vector[0], vector[1], vector[2]};
      normals.push_back(n);
      last_normal_index++;
    }

    BCPolygonNormalsIndices poly_indices;
    for (int loop_index = 0; loop_index < mpoly->totloop; loop_index++) {
      unsigned int loop_idx = mpoly->loopstart + loop_index;
      if (use_vertex_normals) {
        float normalized[3];

        if (use_custom_normals) {
          normalize_v3_v3(normalized, lnors[loop_idx]);
        }
        else {
          normal_short_to_float_v3(normalized, verts[mloops[loop_index].v].no);
          normalize_v3(normalized);
        }
        Normal n = {normalized[0], normalized[1], normalized[2]};

        if (shared_normal_indices.find(n) != shared_normal_indices.end()) {
          poly_indices.add_index(shared_normal_indices[n]);
        }
        else {
          last_normal_index++;
          poly_indices.add_index(last_normal_index);
          shared_normal_indices[n] = last_normal_index;
          normals.push_back(n);
        }
      }
      else {
        poly_indices.add_index(last_normal_index);
      }
    }

    polygons_normals.push_back(poly_indices);
  }
}

std::string GeometryExporter::getIdBySemantics(std::string geom_id,
                                               COLLADASW::InputSemantic::Semantics type,
                                               std::string other_suffix)
{
  return geom_id + getSuffixBySemantic(type) + other_suffix;
}

COLLADASW::URI GeometryExporter::getUrlBySemantics(std::string geom_id,
                                                   COLLADASW::InputSemantic::Semantics type,
                                                   std::string other_suffix)
{

  std::string id(getIdBySemantics(geom_id, type, other_suffix));
  return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
}

COLLADASW::URI GeometryExporter::makeUrl(std::string id)
{
  return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
}
