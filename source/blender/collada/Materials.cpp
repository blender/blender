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

#include "Materials.h"

MaterialNode::MaterialNode(bContext *C, Material *ma, KeyImageMap &key_image_map)
    : mContext(C), material(ma), effect(nullptr), key_image_map(&key_image_map)
{
  bNodeTree *new_ntree = prepare_material_nodetree();
  setShaderType();
  if (new_ntree) {
    shader_node = add_node(SH_NODE_BSDF_PRINCIPLED, 0, 300, "");
    output_node = add_node(SH_NODE_OUTPUT_MATERIAL, 300, 300, "");
    add_link(shader_node, 0, output_node, 0);
  }
}

MaterialNode::MaterialNode(bContext *C,
                           COLLADAFW::EffectCommon *ef,
                           Material *ma,
                           UidImageMap &uid_image_map)
    : mContext(C), material(ma), effect(ef), uid_image_map(&uid_image_map)
{
  prepare_material_nodetree();
  setShaderType();

  std::map<std::string, bNode *> nmap;
#if 0
  nmap["main"] = add_node(C, ntree, SH_NODE_BSDF_PRINCIPLED, -300, 300);
  nmap["emission"] = add_node(C, ntree, SH_NODE_EMISSION, -300, 500, "emission");
  nmap["add"] = add_node(C, ntree, SH_NODE_ADD_SHADER, 100, 400);
  nmap["transparent"] = add_node(C, ntree, SH_NODE_BSDF_TRANSPARENT, 100, 200);
  nmap["mix"] = add_node(C, ntree, SH_NODE_MIX_SHADER, 400, 300, "transparency");
  nmap["out"] = add_node(C, ntree, SH_NODE_OUTPUT_MATERIAL, 600, 300);
  nmap["out"]->flag &= ~NODE_SELECT;

  add_link(ntree, nmap["emission"], 0, nmap["add"], 0);
  add_link(ntree, nmap["main"], 0, nmap["add"], 1);
  add_link(ntree, nmap["add"], 0, nmap["mix"], 1);
  add_link(ntree, nmap["transparent"], 0, nmap["mix"], 2);

  add_link(ntree, nmap["mix"], 0, nmap["out"], 0);
  // experimental, probably not used.
  make_group(C, ntree, nmap);
#else
  shader_node = add_node(SH_NODE_BSDF_PRINCIPLED, 0, 300, "");
  output_node = add_node(SH_NODE_OUTPUT_MATERIAL, 300, 300, "");
  add_link(shader_node, 0, output_node, 0);
#endif
}

void MaterialNode::setShaderType()
{
#if 0
  COLLADAFW::EffectCommon::ShaderType shader = ef->getShaderType();
  // Currently we only support PBR based shaders
  // TODO: simulate the effects with PBR

  // blinn
  if (shader == COLLADAFW::EffectCommon::SHADER_BLINN) {
    ma->spec_shader = MA_SPEC_BLINN;
    ma->spec = ef->getShininess().getFloatValue();
  }
  // phong
  else if (shader == COLLADAFW::EffectCommon::SHADER_PHONG) {
    ma->spec_shader = MA_SPEC_PHONG;
    ma->har = ef->getShininess().getFloatValue();
  }
  // lambert
  else if (shader == COLLADAFW::EffectCommon::SHADER_LAMBERT) {
    ma->diff_shader = MA_DIFF_LAMBERT;
  }
  // default - lambert
  else {
    ma->diff_shader = MA_DIFF_LAMBERT;
    fprintf(stderr, "Current shader type is not supported, default to lambert.\n");
  }
#endif
}

// returns null if material already has a node tree
bNodeTree *MaterialNode::prepare_material_nodetree()
{
  if (material->nodetree) {
    ntree = material->nodetree;
    return NULL;
  }

  material->nodetree = ntreeAddTree(NULL, "Shader Nodetree", "ShaderNodeTree");
  material->use_nodes = true;
  ntree = material->nodetree;
  return ntree;
}

bNode *MaterialNode::add_node(int node_type, int locx, int locy, std::string label)
{
  bNode *node = nodeAddStaticNode(mContext, ntree, node_type);
  if (node) {
    if (label.length() > 0) {
      strcpy(node->label, label.c_str());
    }
    node->locx = locx;
    node->locy = locy;
    node->flag |= NODE_SELECT;
  }
  node_map[label] = node;
  return node;
}

void MaterialNode::add_link(bNode *from_node, int from_index, bNode *to_node, int to_index)
{
  bNodeSocket *from_socket = (bNodeSocket *)BLI_findlink(&from_node->outputs, from_index);
  bNodeSocket *to_socket = (bNodeSocket *)BLI_findlink(&to_node->inputs, to_index);

  nodeAddLink(ntree, from_node, from_socket, to_node, to_socket);
}

void MaterialNode::set_reflectivity(COLLADAFW::FloatOrParam &val)
{
  float reflectivity = val.getFloatValue();
  bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Metallic");
  ((bNodeSocketValueFloat *)socket->default_value)->value = reflectivity;

  material->metallic = reflectivity;
}

void MaterialNode::set_shininess(COLLADAFW::FloatOrParam &val)
{
  float roughness = val.getFloatValue();
  bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Roughness");
  ((bNodeSocketValueFloat *)socket->default_value)->value = roughness;
}

void MaterialNode::set_ior(COLLADAFW::FloatOrParam &val)
{
  float ior = val.getFloatValue();
  if (ior < 0) {
    fprintf(stderr,
            "IOR of negative value is not allowed for materials (using Blender default value "
            "instead)");
    return;
  }

  bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "IOR");
  ((bNodeSocketValueFloat *)socket->default_value)->value = ior;
}

void MaterialNode::set_alpha(COLLADAFW::EffectCommon::OpaqueMode mode,
                             COLLADAFW::ColorOrTexture &cot,
                             COLLADAFW::FloatOrParam &val)
{
  if (effect == nullptr) {
    return;
  }

  if (cot.isColor() || !cot.isValid()) {
    COLLADAFW::Color col = (cot.isValid()) ? cot.getColor() : COLLADAFW::Color(1, 1, 1, 1);
    float alpha = val.getFloatValue() * col.getAlpha();  // Assuming A_ONE opaque mode

    bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Alpha");
    ((bNodeSocketValueFloat *)socket->default_value)->value = alpha;
  }
  else if (cot.isTexture()) {
    int locy = -300 * (node_map.size() - 2);
    add_texture_node(cot, -300, locy, "Alpha");
    // TODO: Connect node
  }
}

void MaterialNode::set_diffuse(COLLADAFW::ColorOrTexture &cot)
{
  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = cot.getColor();
    bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Base Color");
    float *fcol = (float *)socket->default_value;

    fcol[0] = material->r = col.getRed();
    fcol[1] = material->g = col.getGreen();
    fcol[2] = material->b = col.getBlue();
    fcol[3] = material->a = col.getAlpha();
  }
  else if (cot.isTexture()) {
    bNode *texture_node = add_texture_node(cot, -300, locy, "Base Color");
    if (texture_node != NULL) {
      add_link(texture_node, 0, shader_node, 0);
    }
  }
}

Image *MaterialNode::get_diffuse_image()
{
  bNode *shader = ntreeFindType(ntree, SH_NODE_BSDF_PRINCIPLED);
  if (shader == nullptr) {
    return nullptr;
  }

  bNodeSocket *in_socket = nodeFindSocket(shader, SOCK_IN, "Base Color");
  if (in_socket == nullptr) {
    return nullptr;
  }

  bNodeLink *link = in_socket->link;
  if (link == nullptr) {
    return nullptr;
  }

  bNode *texture = link->fromnode;
  if (texture == nullptr) {
    return nullptr;
  }

  if (texture->type != SH_NODE_TEX_IMAGE) {
    return nullptr;
  }

  Image *image = (Image *)texture->id;
  return image;
}

static bNodeSocket *set_color(bNode *node, COLLADAFW::Color col)
{
  bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&node->outputs, 0);
  float *fcol = (float *)socket->default_value;
  fcol[0] = col.getRed();
  fcol[1] = col.getGreen();
  fcol[2] = col.getBlue();

  return socket;
}

void MaterialNode::set_ambient(COLLADAFW::ColorOrTexture &cot)
{
  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = cot.getColor();
    bNode *node = add_node(SH_NODE_RGB, -300, locy, "Ambient");
    set_color(node, col);
    // TODO: Connect node
  }
  // texture
  else if (cot.isTexture()) {
    add_texture_node(cot, -300, locy, "Ambient");
    // TODO: Connect node
  }
}

void MaterialNode::set_reflective(COLLADAFW::ColorOrTexture &cot)
{
  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = cot.getColor();
    bNode *node = add_node(SH_NODE_RGB, -300, locy, "Reflective");
    set_color(node, col);
    // TODO: Connect node
  }
  // texture
  else if (cot.isTexture()) {
    add_texture_node(cot, -300, locy, "Reflective");
    // TODO: Connect node
  }
}

void MaterialNode::set_emission(COLLADAFW::ColorOrTexture &cot)
{
  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = cot.getColor();
    bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Emission");
    float *fcol = (float *)socket->default_value;

    fcol[0] = col.getRed();
    fcol[1] = col.getGreen();
    fcol[2] = col.getBlue();
    fcol[3] = col.getAlpha();
  }
  else if (cot.isTexture()) {
    bNode *texture_node = add_texture_node(cot, -300, locy, "Emission");
    if (texture_node != NULL) {
      add_link(texture_node, 0, shader_node, 0);
    }
  }
}

void MaterialNode::set_opacity(COLLADAFW::ColorOrTexture &cot)
{
  if (effect == nullptr) {
    return;
  }

  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = effect->getTransparent().getColor();
    float alpha = effect->getTransparency().getFloatValue();

    if (col.isValid()) {
      alpha *= col.getAlpha();  // Assuming A_ONE opaque mode
    }

    bNodeSocket *socket = nodeFindSocket(shader_node, SOCK_IN, "Alpha");
    ((bNodeSocketValueFloat *)socket->default_value)->value = alpha;
  }
  // texture
  else if (cot.isTexture()) {
    add_texture_node(cot, -300, locy, "Alpha");
    // TODO: Connect node
  }
}

void MaterialNode::set_specular(COLLADAFW::ColorOrTexture &cot)
{
  int locy = -300 * (node_map.size() - 2);
  if (cot.isColor()) {
    COLLADAFW::Color col = cot.getColor();
    bNode *node = add_node(SH_NODE_RGB, -300, locy, "Specular");
    set_color(node, col);
    // TODO: Connect node
  }
  // texture
  else if (cot.isTexture()) {
    add_texture_node(cot, -300, locy, "Specular");
    // TODO: Connect node
  }
}

bNode *MaterialNode::add_texture_node(COLLADAFW::ColorOrTexture &cot,
                                      int locx,
                                      int locy,
                                      std::string label)
{
  if (effect == nullptr) {
    return nullptr;
  }

  UidImageMap &image_map = *uid_image_map;

  COLLADAFW::Texture ctex = cot.getTexture();

  COLLADAFW::SamplerPointerArray &samp_array = effect->getSamplerPointerArray();
  COLLADAFW::Sampler *sampler = samp_array[ctex.getSamplerId()];

  const COLLADAFW::UniqueId &ima_uid = sampler->getSourceImage();

  if (image_map.find(ima_uid) == image_map.end()) {
    fprintf(stderr, "Couldn't find an image by UID.\n");
    return NULL;
  }

  Image *ima = image_map[ima_uid];
  bNode *texture_node = add_node(SH_NODE_TEX_IMAGE, locx, locy, label);
  texture_node->id = &ima->id;
  return texture_node;
}
