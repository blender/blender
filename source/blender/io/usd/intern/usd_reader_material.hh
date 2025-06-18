/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include "WM_types.hh"

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include <pxr/usd/usdShade/material.h>

#include <string>

struct Main;
struct Material;
struct bNode;
struct bNodeTree;
struct ReportList;

namespace blender::io::usd {

using ShaderToNodeMap = Map<std::string, bNode *>;

/* Helper struct used when arranging nodes in columns, keeping track the
 * occupancy information for a given column.  I.e., for column n,
 * column_offsets[n] is the y-offset (from top to bottom) of the occupied
 * region in that column. */
struct NodePlacementContext {
  const float origx_;
  const float origy_;
  const float horizontal_step_;
  const float vertical_step_;
  Vector<float, 8> column_offsets_ = Vector<float, 8>(8, 0.0f);

  /* Map a USD shader prim path to the Blender node converted
   * from that shader.  This map is updated during shader
   * conversion and is used to avoid creating duplicate nodes
   * for a given shader. */
  ShaderToNodeMap node_cache_;

  NodePlacementContext(float origx,
                       float origy,
                       float horizontal_step = 300.0f,
                       float vertical_step = 300.0f)
      : origx_(origx),
        origy_(origy),
        horizontal_step_(horizontal_step),
        vertical_step_(vertical_step)
  {
  }

  /* Compute the x- and y-coordinates for placing a new node in an unoccupied region of
   * the column with the given index. */
  float2 compute_node_loc(int column);

  /**
   * Generate a key for caching a Blender node created for a given USD shader by returning the
   * shader prim path with an optional tag suffix. The tag can be specified in order to generate a
   * unique key when more than one Blender node is created for the USD shader. */
  std::string get_key(const pxr::UsdShadeShader &usd_shader, const blender::StringRef tag) const;

  /* Returns the Blender node previously cached for the given USD shader. Returns null if no cached
   * shader was found. */
  bNode *get_cached_node(const pxr::UsdShadeShader &usd_shader,
                         const blender::StringRef tag = {}) const;

  /* Cache the Blender node translated from the given USD shader. */
  void cache_node(const pxr::UsdShadeShader &usd_shader,
                  bNode *node,
                  const blender::StringRef tag = {});
};

/* Helper struct which carries an assortment of optional
 * information that is sometimes required when linking
 * nodes together. */
struct ExtraLinkInfo {
  bool is_color_corrected = false;

  float opacity_threshold = 0.0f;
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
 private:
  const USDImportParams &params_;
  Main &bmain_;

 public:
  USDMaterialReader(const USDImportParams &params, Main &bmain);

  Material *add_material(const pxr::UsdShadeMaterial &usd_material,
                         bool read_usd_preview = true) const;

  void import_usd_preview(Material *mtl, const pxr::UsdShadeMaterial &usd_material) const;

  /** Get the wmJobWorkerStatus-provided `reports` list pointer, to use with the BKE_report API. */
  ReportList *reports() const
  {
    return params_.worker_status ? params_.worker_status->reports : nullptr;
  }

 protected:
  /** Create the Principled BSDF shader node network. */
  void import_usd_preview_nodes(Material *mtl,
                                const pxr::UsdShadeMaterial &usd_material,
                                const pxr::UsdShadeShader &usd_shader) const;

  void set_principled_node_inputs(bNode *principled_node,
                                  bNodeTree *ntree,
                                  const pxr::UsdShadeShader &usd_shader) const;

  bool set_displacement_node_inputs(bNodeTree *ntree,
                                    bNode *output,
                                    const pxr::UsdShadeShader &usd_shader) const;

  /** Convert the given USD shader input to an input on the given Blender node. */
  bool set_node_input(const pxr::UsdShadeInput &usd_input,
                      bNode *dest_node,
                      const StringRefNull dest_socket_name,
                      bNodeTree *ntree,
                      int column,
                      NodePlacementContext &ctx,
                      const ExtraLinkInfo &extra = {}) const;

  /**
   * Follow the connected source of the USD input to create corresponding inputs
   * for the given Blender node.
   */
  bool follow_connection(const pxr::UsdShadeInput &usd_input,
                         bNode *dest_node,
                         const StringRefNull dest_socket_name,
                         bNodeTree *ntree,
                         int column,
                         NodePlacementContext &ctx,
                         const ExtraLinkInfo &extra = {}) const;

  void convert_usd_uv_texture(const pxr::UsdShadeShader &usd_shader,
                              const pxr::TfToken &usd_source_name,
                              bNode *dest_node,
                              const StringRefNull dest_socket_name,
                              bNodeTree *ntree,
                              int column,
                              NodePlacementContext &ctx,
                              const ExtraLinkInfo &extra = {}) const;

  void convert_usd_transform_2d(const pxr::UsdShadeShader &usd_shader,
                                bNode *dest_node,
                                const StringRefNull dest_socket_name,
                                bNodeTree *ntree,
                                int column,
                                NodePlacementContext &ctx) const;

  /**
   * Load the texture image node's texture from the path given by the USD shader's
   * file input value.
   */
  void load_tex_image(const pxr::UsdShadeShader &usd_shader,
                      bNode *tex_image,
                      const ExtraLinkInfo &extra = {}) const;

  /**
   * This function creates a Blender UV Map node, under the simplifying assumption that
   * UsdPrimvarReader_float2 shaders output UV coordinates.
   */
  void convert_usd_primvar_reader_float2(const pxr::UsdShadeShader &usd_shader,
                                         const pxr::TfToken &usd_source_name,
                                         bNode *dest_node,
                                         const StringRefNull dest_socket_name,
                                         bNodeTree *ntree,
                                         int column,
                                         NodePlacementContext &ctx) const;
  void convert_usd_primvar_reader_generic(const pxr::UsdShadeShader &usd_shader,
                                          StringRef output_type,
                                          bNode *dest_node,
                                          const StringRefNull dest_socket_name,
                                          bNodeTree *ntree,
                                          int column,
                                          NodePlacementContext &ctx) const;
};

/* Utility functions. */

/**
 * Returns a map containing all the Blender materials which allows a fast
 * lookup of the material by name.  Note that the material name key
 * might be modified to be a valid USD identifier, to match material
 * names in the imported USD.
 */
void build_material_map(const Main *bmain, blender::Map<std::string, Material *> &r_mat_map);

/**
 * Returns an existing Blender material that corresponds to the USD material with the given path.
 * Returns null if no such material exists.
 *
 * \param mat_map: Map a material name to a Blender material.  Note that the name key
 * might be the Blender material name modified to be a valid USD identifier,
 * to match the material names in the imported USD.
 * \param usd_path_to_mat: Map a USD material path to the imported Blender material.
 *
 * The usd_path_to_mat is needed to determine the name of the Blender
 * material imported from a USD path in the case when a unique name was generated
 * for the material due to a name collision.
 */
Material *find_existing_material(const pxr::SdfPath &usd_mat_path,
                                 const USDImportParams &params,
                                 const blender::Map<std::string, Material *> &mat_map,
                                 const blender::Map<pxr::SdfPath, Material *> &usd_path_to_mat);

}  // namespace blender::io::usd
