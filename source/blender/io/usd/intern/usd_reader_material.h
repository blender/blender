/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"

#include "BLI_map.hh"

#include <pxr/usd/usdShade/material.h>

#include <string>

struct Main;
struct Material;
struct bNode;
struct bNodeTree;

namespace blender::io::usd {

using ShaderToNodeMap = blender::Map<std::string, bNode *>;

/* Helper struct used when arranging nodes in columns, keeping track the
 * occupancy information for a given column.  I.e., for column n,
 * column_offsets[n] is the y-offset (from top to bottom) of the occupied
 * region in that column. */
struct NodePlacementContext {
  float origx;
  float origy;
  std::vector<float> column_offsets;
  const float horizontal_step;
  const float vertical_step;

  /* Map a USD shader prim path to the Blender node converted
   * from that shader.  This map is updated during shader
   * conversion and is used to avoid creating duplicate nodes
   * for a given shader.  */
  ShaderToNodeMap node_cache;

  NodePlacementContext(float in_origx,
                       float in_origy,
                       float in_horizontal_step = 300.0f,
                       float in_vertical_step = 300.0f)
      : origx(in_origx),
        origy(in_origy),
        column_offsets(64, 0.0f),
        horizontal_step(in_horizontal_step),
        vertical_step(in_vertical_step)
  {
  }
};

/* Converts USD materials to Blender representation. */

/**
 * By default, the #USDMaterialReader creates a Blender material with
 * the same name as the USD material.  If the USD material has a
 * #UsdPreviewSurface source, the Blender material's viewport display
 * color, roughness and metallic properties are set to the corresponding
 * #UsdPreoviewSurface inputs.
 *
 * If the Import USD Preview option is enabled, the current implementation
 * converts #UsdPreviewSurface to Blender nodes as follows:
 *
 * - #UsdPreviewSurface -> Principled BSDF
 * - #UsdUVTexture -> Texture Image + Normal Map
 * - UsdPrimvarReader_float2 -> UV Map
 *
 * Limitations: arbitrary primvar readers or UsdTransform2d not yet
 * supported. For #UsdUVTexture, only the file, st and #sourceColorSpace
 * inputs are handled.
 *
 * TODO(makowalski):  Investigate adding support for converting additional
 * shaders and inputs.  Supporting certain types of inputs, such as texture
 * scale and bias, will probably require creating Blender Group nodes with
 * the corresponding inputs.
 */
class USDMaterialReader {
 protected:
  USDImportParams params_;

  Main *bmain_;

 public:
  USDMaterialReader(const USDImportParams &params, Main *bmain);

  Material *add_material(const pxr::UsdShadeMaterial &usd_material) const;

 protected:
  /** Create the Principled BSDF shader node network. */
  void import_usd_preview(Material *mtl, const pxr::UsdShadeShader &usd_shader) const;

  void set_principled_node_inputs(bNode *principled_node,
                                  bNodeTree *ntree,
                                  const pxr::UsdShadeShader &usd_shader) const;

  /** Convert the given USD shader input to an input on the given Blender node. */
  void set_node_input(const pxr::UsdShadeInput &usd_input,
                      bNode *dest_node,
                      const char *dest_socket_name,
                      bNodeTree *ntree,
                      int column,
                      NodePlacementContext *r_ctx) const;

  /**
   * Follow the connected source of the USD input to create corresponding inputs
   * for the given Blender node.
   */
  void follow_connection(const pxr::UsdShadeInput &usd_input,
                         bNode *dest_node,
                         const char *dest_socket_name,
                         bNodeTree *ntree,
                         int column,
                         NodePlacementContext *r_ctx) const;

  void convert_usd_uv_texture(const pxr::UsdShadeShader &usd_shader,
                              const pxr::TfToken &usd_source_name,
                              bNode *dest_node,
                              const char *dest_socket_name,
                              bNodeTree *ntree,
                              int column,
                              NodePlacementContext *r_ctx) const;

  /**
   * Load the texture image node's texture from the path given by the USD shader's
   * file input value.
   */
  void load_tex_image(const pxr::UsdShadeShader &usd_shader, bNode *tex_image) const;

  /**
   * This function creates a Blender UV Map node, under the simplifying assumption that
   * UsdPrimvarReader_float2 shaders output UV coordinates.
   * TODO(makowalski): investigate supporting conversion to other Blender node types
   * (e.g., Attribute Nodes) if needed.
   */
  void convert_usd_primvar_reader_float2(const pxr::UsdShadeShader &usd_shader,
                                         const pxr::TfToken &usd_source_name,
                                         bNode *dest_node,
                                         const char *dest_socket_name,
                                         bNodeTree *ntree,
                                         int column,
                                         NodePlacementContext *r_ctx) const;
};

/* Utility functions. */

/**
 * Returns a map containing all the Blender materials which allows a fast
 * lookup of the material by name.  Note that the material name key
 * might be modified to be a valid USD identifier, to match material
 * names in the imported USD.
 */
void build_material_map(const Main *bmain, std::map<std::string, Material *> *r_mat_map);

/**
 * Returns an existing Blender material that corresponds to the USD material with the given path.
 * Returns null if no such material exists.
 *
 * \param mat_map: Map a material name to a Blender material.  Note that the name key
 * might be the Blender material name modified to be a valid USD identifier,
 * to match the material names in the imported USD.
 * \param usd_path_to_mat_name: Map a USD material path to the imported Blender material name.
 *
 * The usd_path_to_mat_name is needed to determine the name of the Blender
 * material imported from a USD path in the case when a unique name was generated
 * for the material due to a name collision.
 */
Material *find_existing_material(const pxr::SdfPath &usd_mat_path,
                                 const USDImportParams &params,
                                 const std::map<std::string, Material *> &mat_map,
                                 const std::map<std::string, std::string> &usd_path_to_mat_name);

}  // namespace blender::io::usd
