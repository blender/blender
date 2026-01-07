/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include "BLI_map.hh"

#include "../stroke/StrokeRenderer.h"
#include "../system/FreestyleConfig.h"

namespace blender {
struct Depsgraph;
struct GHash;
struct Main;
struct Material;
struct Object;
struct Render;
struct Scene;
struct bContext;
struct bNodeTree;
}  // namespace blender

namespace Freestyle {

class BlenderStrokeRenderer : public StrokeRenderer {
 public:
  BlenderStrokeRenderer(blender::Render *re, int render_count);
  virtual ~BlenderStrokeRenderer();

  /** Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

  blender::Object *NewMesh() const;

  struct StrokeGroup {
    explicit StrokeGroup() : totvert(0), totedge(0), faces_num(0), totloop(0) {}
    vector<StrokeRep *> strokes;
    blender::Map<blender::Material *, int> materials;
    int totvert;
    int totedge;
    int faces_num;
    int totloop;
  };
  vector<StrokeGroup *> strokeGroups, texturedStrokeGroups;

  int GenerateScene();
  void GenerateStrokeMesh(StrokeGroup *group, bool hasTex);
  void FreeStrokeGroups();

  blender::Render *RenderScene(blender::Render *re, bool render);

  static blender::Material *GetStrokeShader(blender::Main *bmain,
                                            blender::bNodeTree *iNodeTree,
                                            bool do_id_user);

 protected:
  blender::Main *freestyle_bmain;
  blender::Scene *old_scene;
  blender::Scene *freestyle_scene;
  blender::Depsgraph *freestyle_depsgraph;
  blender::bContext *_context;
  float _width, _height;
  float _z, _z_delta;
  uint _mesh_id;
  bool _use_shading_nodes;
  mutable blender::Map<blender::bNodeTree *, blender::Material *> _nodetree_hash;

  static const char *uvNames[];

  int get_stroke_count() const;
  float get_stroke_vertex_z(void) const;
  uint get_stroke_mesh_id(void) const;
  bool test_triangle_visibility(StrokeVertexRep *svRep[3]) const;
  void test_strip_visibility(Strip::vertex_container &strip_vertices,
                             int *visible_faces,
                             int *visible_segments) const;

  vector<StrokeRep *> _strokeReps;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderStrokeRenderer")
};

} /* namespace Freestyle */
