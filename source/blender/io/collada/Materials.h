/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>
#include <string>

#include "BKE_context.h"
#include "BKE_node.h"
#include "BLI_listbase.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "COLLADAFWEffectCommon.h"
#include "collada_utils.h"

typedef std::map<std::string, bNode *> NodeMap;

class MaterialNode {

 private:
  bContext *mContext;
  Material *material;
  COLLADAFW::EffectCommon *effect;
  UidImageMap *uid_image_map = nullptr;
  KeyImageMap *key_image_map = nullptr;

  NodeMap node_map;
  bNodeTree *ntree;

  bNode *shader_node;
  bNode *output_node;

  /** Returns null if material already has a node tree. */
  bNodeTree *prepare_material_nodetree();
  bNode *add_node(int node_type, int locx, int locy, std::string label);
  void add_link(bNode *from_node, int from_index, bNode *to_node, int to_index);
  void add_link(bNode *from_node, const char *from_label, bNode *to_node, const char *to_label);
  bNode *add_texture_node(COLLADAFW::ColorOrTexture &cot, int locx, int locy, std::string label);
  void setShaderType();

 public:
  MaterialNode(bContext *C, COLLADAFW::EffectCommon *ef, Material *ma, UidImageMap &uid_image_map);
  MaterialNode(bContext *C, Material *ma, KeyImageMap &key_image_map);
  Image *get_diffuse_image();

  void set_diffuse(COLLADAFW::ColorOrTexture &cot);
  void set_specular(COLLADAFW::ColorOrTexture &cot);
  void set_ambient(COLLADAFW::ColorOrTexture &cot);
  void set_reflective(COLLADAFW::ColorOrTexture &cot);
  void set_emission(COLLADAFW::ColorOrTexture &cot);
  void set_opacity(COLLADAFW::ColorOrTexture &cot);
  void set_reflectivity(COLLADAFW::FloatOrParam &val);
  void set_shininess(COLLADAFW::FloatOrParam &val);
  void set_ior(COLLADAFW::FloatOrParam &val);
  void set_alpha(COLLADAFW::EffectCommon::OpaqueMode mode,
                 COLLADAFW::ColorOrTexture &cot,
                 COLLADAFW::FloatOrParam &val);

  void update_material_nodetree();
};
