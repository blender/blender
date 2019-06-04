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

#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <map>
#include <string>

extern "C" {
#include "BKE_context.h"
#include "BKE_node.h"
#include "BLI_listbase.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
}

#include "collada_utils.h"
#include "COLLADAFWEffectCommon.h"

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

  bNodeTree *prepare_material_nodetree();
  bNode *add_node(int node_type, int locx, int locy, std::string label);
  void add_link(bNode *from_node, int from_index, bNode *to_node, int to_index);
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
};

#endif
