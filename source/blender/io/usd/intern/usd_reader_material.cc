/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_material.h"


#include "usd_umm.h"

#include "usd_asset_utils.h"

#include "BKE_appdir.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "BKE_node_tree_update.h"

#include "BLI_fileops.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_material_types.h"

#include "WM_api.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/packageUtils.h>

#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include <iostream>
#include <vector>

namespace usdtokens {

/* Parameter names. */
static const pxr::TfToken a("a", pxr::TfToken::Immortal);
static const pxr::TfToken b("b", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoat("clearcoat", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoatRoughness("clearcoatRoughness", pxr::TfToken::Immortal);
static const pxr::TfToken diffuseColor("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken emissiveColor("emissiveColor", pxr::TfToken::Immortal);
static const pxr::TfToken file("file", pxr::TfToken::Immortal);
static const pxr::TfToken g("g", pxr::TfToken::Immortal);
static const pxr::TfToken ior("ior", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken normal("normal", pxr::TfToken::Immortal);
static const pxr::TfToken occlusion("occlusion", pxr::TfToken::Immortal);
static const pxr::TfToken opacity("opacity", pxr::TfToken::Immortal);
static const pxr::TfToken opacityThreshold("opacityThreshold", pxr::TfToken::Immortal);
static const pxr::TfToken r("r", pxr::TfToken::Immortal);
static const pxr::TfToken result("result", pxr::TfToken::Immortal);
static const pxr::TfToken rgb("rgb", pxr::TfToken::Immortal);
static const pxr::TfToken rgba("rgba", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken sourceColorSpace("sourceColorSpace", pxr::TfToken::Immortal);
static const pxr::TfToken specularColor("specularColor", pxr::TfToken::Immortal);
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken varname("varname", pxr::TfToken::Immortal);

/* Color space names. */
static const pxr::TfToken raw("raw", pxr::TfToken::Immortal);
static const pxr::TfToken RAW("RAW", pxr::TfToken::Immortal);

/* Wrap mode names. */
static const pxr::TfToken black("black", pxr::TfToken::Immortal);
static const pxr::TfToken clamp("clamp", pxr::TfToken::Immortal);
static const pxr::TfToken repeat("repeat", pxr::TfToken::Immortal);
static const pxr::TfToken wrapS("wrapS", pxr::TfToken::Immortal);
static const pxr::TfToken wrapT("wrapT", pxr::TfToken::Immortal);

/* USD shader names. */
static const pxr::TfToken UsdPreviewSurface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken UsdPrimvarReader_float2("UsdPrimvarReader_float2",
                                                  pxr::TfToken::Immortal);
static const pxr::TfToken UsdUVTexture("UsdUVTexture", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* Temporary folder for saving imported textures prior to packing.
 * CAUTION: this directory is recursively deleted after material
 * import. */
static const char *temp_textures_dir()
{
  static bool inited = false;

  static char temp_dir[FILE_MAXDIR] = {'\0'};

  if (!inited) {
    BLI_path_join(temp_dir, sizeof(temp_dir), BKE_tempdir_session(), "usd_textures_tmp", SEP_STR);
    inited = true;
  }

  return temp_dir;
}

using blender::io::usd::ShaderToNodeMap;

/* Returns the Blender node previously cached for
 * the given USD shader in the given map.  Returns
 * null if no cached shader was found. */
static bNode *get_cached_node(const ShaderToNodeMap &node_cache,
                              const pxr::UsdShadeShader &usd_shader)
{
  if (bNode *const *node_ptr = node_cache.lookup_ptr(usd_shader.GetPath().GetAsString())) {
    return *node_ptr;
  }
  return nullptr;
}

/* Cache the Blender node translated from the given USD shader
 * in the given map. */
static void cache_node(ShaderToNodeMap &node_cache,
                       const pxr::UsdShadeShader &usd_shader,
                       bNode *node)
{
  node_cache.add(usd_shader.GetPath().GetAsString(), node);
}

/* Add a node of the given type at the given location coordinates. */
static bNode *add_node(
    const bContext *C, bNodeTree *ntree, const int type, const float locx, const float locy)
{
  bNode *new_node = nodeAddStaticNode(C, ntree, type);

  if (new_node) {
    new_node->locx = locx;
    new_node->locy = locy;
  }

  return new_node;
}

/* Connect the output socket of node 'source' to the input socket of node 'dest'. */
static void link_nodes(
    bNodeTree *ntree, bNode *source, const char *sock_out, bNode *dest, const char *sock_in)
{
  bNodeSocket *source_socket = nodeFindSocket(source, SOCK_OUT, sock_out);

  if (!source_socket) {
    std::cerr << "PROGRAMMER ERROR: Couldn't find output socket " << sock_out << std::endl;
    return;
  }

  bNodeSocket *dest_socket = nodeFindSocket(dest, SOCK_IN, sock_in);

  if (!dest_socket) {
    std::cerr << "PROGRAMMER ERROR: Couldn't find input socket " << sock_in << std::endl;
    return;
  }

  nodeAddLink(ntree, source, source_socket, dest, dest_socket);
}

/* Returns a layer handle retrieved from the given attribute's property specs.
 * Note that the returned handle may be invalid if no layer could be found. */
static pxr::SdfLayerHandle get_layer_handle(const pxr::UsdAttribute &attribute)
{
  for (auto PropertySpec : attribute.GetPropertyStack(pxr::UsdTimeCode::EarliestTime())) {
    if (PropertySpec->HasDefaultValue() ||
        PropertySpec->GetLayer()->GetNumTimeSamplesForPath(PropertySpec->GetPath()) > 0)
    {
      return PropertySpec->GetLayer();
    }
  }

  return pxr::SdfLayerHandle();
}

/* For the given UDIM path (assumed to contain the UDIM token), returns an array
 * containing valid tile indices. */
static blender::Vector<int> get_udim_tiles(const std::string &file_path)
{
  char base_udim_path[FILE_MAX];
  BLI_strncpy(base_udim_path, file_path.c_str(), sizeof(base_udim_path));

  blender::Vector<int> udim_tiles;

  /* Extract the tile numbers from all files on disk. */
  ListBase tiles = {nullptr, nullptr};
  int tile_start, tile_range;
  bool result = BKE_image_get_tile_info(base_udim_path, &tiles, &tile_start, &tile_range);
  if (result) {
    LISTBASE_FOREACH (LinkData *, tile, &tiles) {
      int tile_number = POINTER_AS_INT(tile->data);
      udim_tiles.append(tile_number);
    }
  }

  BLI_freelistN(&tiles);

  return udim_tiles;
}

/* Add tiles with the given indices to the given image. */
static void add_udim_tiles(Image *image, const blender::Vector<int> &indices)
{
  image->source = IMA_SRC_TILED;

  for (int tile_number : indices) {
    BKE_image_add_tile(image, tile_number, nullptr);
  }
}

/* Returns true if the given shader may have opacity < 1.0, based
 * on heuristics. */
static bool needs_blend(const pxr::UsdShadeShader &usd_shader)
{
  if (!usd_shader) {
    return false;
  }

  bool needs_blend = false;

  if (pxr::UsdShadeInput opacity_input = usd_shader.GetInput(usdtokens::opacity)) {

    if (opacity_input.HasConnectedSource()) {
      needs_blend = true;
    }
    else {
      pxr::VtValue val;
      if (opacity_input.GetAttr().HasAuthoredValue() && opacity_input.GetAttr().Get(&val)) {
        float opacity = val.Get<float>();
        needs_blend = opacity < 1.0f;
      }
    }
  }

  return needs_blend;
}

/* Returns the given shader's opacityThreshold input value, if this input has an
 * authored value. Otherwise, returns the given default value. */
static float get_opacity_threshold(const pxr::UsdShadeShader &usd_shader,
                                   float default_value = 0.0f)
{
  if (!usd_shader) {
    return default_value;
  }

  pxr::UsdShadeInput opacity_threshold_input = usd_shader.GetInput(usdtokens::opacityThreshold);

  if (!opacity_threshold_input) {
    return default_value;
  }

  pxr::VtValue val;
  if (opacity_threshold_input.GetAttr().HasAuthoredValue() &&
      opacity_threshold_input.GetAttr().Get(&val))
  {
    return val.Get<float>();
  }

  return default_value;
}

static pxr::TfToken get_source_color_space(const pxr::UsdShadeShader &usd_shader)
{
  if (!usd_shader) {
    return pxr::TfToken();
  }

  pxr::UsdShadeInput color_space_input = usd_shader.GetInput(usdtokens::sourceColorSpace);

  if (!color_space_input) {
    return pxr::TfToken();
  }

  pxr::VtValue color_space_val;
  if (color_space_input.Get(&color_space_val) && color_space_val.IsHolding<pxr::TfToken>()) {
    return color_space_val.Get<pxr::TfToken>();
  }

  return pxr::TfToken();
}

static int get_image_extension(const pxr::UsdShadeShader &usd_shader, const int default_value)
{
  pxr::UsdShadeInput wrap_input = usd_shader.GetInput(usdtokens::wrapS);

  if (!wrap_input) {
    wrap_input = usd_shader.GetInput(usdtokens::wrapT);
  }

  if (!wrap_input) {
    return default_value;
  }

  pxr::VtValue wrap_input_val;
  if (!(wrap_input.Get(&wrap_input_val) && wrap_input_val.IsHolding<pxr::TfToken>())) {
    return default_value;
  }

  pxr::TfToken wrap_val = wrap_input_val.Get<pxr::TfToken>();

  if (wrap_val == usdtokens::repeat) {
    return SHD_IMAGE_EXTENSION_REPEAT;
  }

  if (wrap_val == usdtokens::clamp) {
    return SHD_IMAGE_EXTENSION_EXTEND;
  }

  if (wrap_val == usdtokens::black) {
    return SHD_IMAGE_EXTENSION_CLIP;
  }

  return default_value;
}

/* Attempts to return in r_preview_surface the UsdPreviewSurface shader source
 * of the given material.  Returns true if a UsdPreviewSurface source was found
 * and returns false otherwise. */
static bool get_usd_preview_surface(const pxr::UsdShadeMaterial &usd_material,
                                    pxr::UsdShadeShader &r_preview_surface)
{
  if (!usd_material) {
    return false;
  }

  if (pxr::UsdShadeShader surf_shader = usd_material.ComputeSurfaceSource()) {
    /* Check if we have a UsdPreviewSurface shader. */
    pxr::TfToken shader_id;
    if (surf_shader.GetShaderId(&shader_id) && shader_id == usdtokens::UsdPreviewSurface) {
      r_preview_surface = surf_shader;
      return true;
    }
  }

  return false;
}

/* Set the Blender material's viewport display color, metallic and roughness
 * properties from the given USD preview surface shader's inputs. */
static void set_viewport_material_props(Material *mtl, const pxr::UsdShadeShader &usd_preview)
{
  if (!(mtl && usd_preview)) {
    return;
  }

  if (pxr::UsdShadeInput diffuse_color_input = usd_preview.GetInput(usdtokens::diffuseColor)) {
    pxr::VtValue val;
    if (diffuse_color_input.GetAttr().HasAuthoredValue() &&
        diffuse_color_input.GetAttr().Get(&val) && val.IsHolding<pxr::GfVec3f>())
    {
      pxr::GfVec3f color = val.UncheckedGet<pxr::GfVec3f>();
      mtl->r = color[0];
      mtl->g = color[1];
      mtl->b = color[2];
    }
  }

  if (pxr::UsdShadeInput metallic_input = usd_preview.GetInput(usdtokens::metallic)) {
    pxr::VtValue val;
    if (metallic_input.GetAttr().HasAuthoredValue() && metallic_input.GetAttr().Get(&val) &&
        val.IsHolding<float>())
    {
      mtl->metallic = val.Get<float>();
    }
  }

  if (pxr::UsdShadeInput roughness_input = usd_preview.GetInput(usdtokens::roughness)) {
    pxr::VtValue val;
    if (roughness_input.GetAttr().HasAuthoredValue() && roughness_input.GetAttr().Get(&val) &&
        val.IsHolding<float>())
    {
      mtl->roughness = val.Get<float>();
    }
  }
}

namespace blender::io::usd {

namespace {

/* Compute the x- and y-coordinates for placing a new node in an unoccupied region of
 * the column with the given index.  Returns the coordinates in r_locx and r_locy and
 * updates the column-occupancy information in r_ctx. */
void compute_node_loc(const int column, float *r_locx, float *r_locy, NodePlacementContext *r_ctx)
{
  if (!(r_locx && r_locy && r_ctx)) {
    return;
  }

  (*r_locx) = r_ctx->origx - column * r_ctx->horizontal_step;

  if (column >= r_ctx->column_offsets.size()) {
    r_ctx->column_offsets.push_back(0.0f);
  }

  (*r_locy) = r_ctx->origy - r_ctx->column_offsets[column];

  /* Record the y-offset of the occupied region in
   * the column, including padding. */
  r_ctx->column_offsets[column] += r_ctx->vertical_step + 10.0f;
}

}  // namespace

USDMaterialReader::USDMaterialReader(const USDImportParams &params, Main *bmain)
    : params_(params), bmain_(bmain)
{
}

Material *USDMaterialReader::add_material(const pxr::UsdShadeMaterial &usd_material) const
{
  if (!(bmain_ && usd_material)) {
    return nullptr;
  }

  std::string mtl_name = usd_material.GetPrim().GetName().GetString();

  /* Create the material. */
  Material *mtl = BKE_material_add(bmain_, mtl_name.c_str());
  id_us_min(&mtl->id);

  /* Get the UsdPreviewSurface shader source for the material,
   * if there is one. */
  pxr::UsdShadeShader usd_preview;
  if (get_usd_preview_surface(usd_material, usd_preview)) {
    /* Always set the viewport material properties from the USD
     * Preview Surface settings. */
    set_viewport_material_props(mtl, usd_preview);
  }

  if (params_.import_shaders_mode == USD_IMPORT_USD_PREVIEW_SURFACE && usd_preview) {
    /* Create shader nodes to represent a UsdPreviewSurface. */
    import_usd_preview(mtl, usd_preview);
  }
  else if (params_.import_shaders_mode == USD_IMPORT_MDL) {
    bool mdl_imported = false;
#ifdef WITH_PYTHON
    /* Invoke UMM to convert to MDL. */
    mdl_imported = umm_import_material(params_, mtl, usd_material, "MDL");
    if (params_.import_textures_mode == USD_TEX_IMPORT_PACK) {
      /* Process the imported material to pack the textures.  */
      pack_imported_textures(mtl);
    }
#endif
    if (!mdl_imported && usd_preview) {
      /* The material has no MDL shader or we couldn't convert the MDL,
       * so fall back on importing UsdPreviewSuface. */
      WM_reportf(RPT_INFO, "Couldn't import MDL shader for material %s, importing USD Preview Surface shaders instead",
                 mtl_name.c_str());
      import_usd_preview(mtl, usd_preview);
    }
  }

  return mtl;
}

void USDMaterialReader::import_usd_preview(Material *mtl,
                                           const pxr::UsdShadeShader &usd_shader) const
{
  if (!(bmain_ && mtl && usd_shader)) {
    return;
  }

  /* Create the Material's node tree containing the principled BSDF
   * and output shaders. */

  /* Add the node tree. */
  bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &mtl->id, "Shader Nodetree", "ShaderNodeTree");
  mtl->use_nodes = true;

  /* Create the Principled BSDF shader node. */
  bNode *principled = add_node(nullptr, ntree, SH_NODE_BSDF_PRINCIPLED, 0.0f, 300.0f);

  if (!principled) {
    std::cerr << "ERROR: Couldn't create SH_NODE_BSDF_PRINCIPLED node for USD shader "
              << usd_shader.GetPath() << std::endl;
    return;
  }

  /* Create the material output node. */
  bNode *output = add_node(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL, 300.0f, 300.0f);

  if (!output) {
    std::cerr << "ERROR: Couldn't create SH_NODE_OUTPUT_MATERIAL node for USD shader "
              << usd_shader.GetPath() << std::endl;
    return;
  }

  /* Connect the Principled BSDF node to the output node. */
  link_nodes(ntree, principled, "BSDF", output, "Surface");

  /* Recursively create the principled shader input networks. */
  set_principled_node_inputs(principled, ntree, usd_shader);

  nodeSetActive(ntree, output);

  BKE_ntree_update_main_tree(bmain_, ntree, nullptr);

  /* Optionally, set the material blend mode. */

  if (params_.set_material_blend) {
    if (needs_blend(usd_shader)) {
      float opacity_threshold = get_opacity_threshold(usd_shader, 0.0f);
      if (opacity_threshold > 0.0f) {
        mtl->blend_method = MA_BM_CLIP;
        mtl->alpha_threshold = opacity_threshold;
      }
      else {
        mtl->blend_method = MA_BM_BLEND;
      }
    }
  }
}

void USDMaterialReader::set_principled_node_inputs(bNode *principled,
                                                   bNodeTree *ntree,
                                                   const pxr::UsdShadeShader &usd_shader) const
{
  /* The context struct keeps track of the locations for adding
   * input nodes. */
  NodePlacementContext context(0.0f, 300.0);

  /* The column index (from right to left relative to the principled
   * node) where we're adding the nodes. */
  int column = 0;

  /* Recursively set the principled shader inputs. */

  if (pxr::UsdShadeInput diffuse_input = usd_shader.GetInput(usdtokens::diffuseColor)) {
    set_node_input(diffuse_input, principled, "Base Color", ntree, column, &context);
  }

  if (pxr::UsdShadeInput emissive_input = usd_shader.GetInput(usdtokens::emissiveColor)) {
    set_node_input(emissive_input, principled, "Emission", ntree, column, &context);
  }

  if (pxr::UsdShadeInput specular_input = usd_shader.GetInput(usdtokens::specularColor)) {
    set_node_input(specular_input, principled, "Specular", ntree, column, &context);
  }

  if (pxr::UsdShadeInput metallic_input = usd_shader.GetInput(usdtokens::metallic)) {
    ;
    set_node_input(metallic_input, principled, "Metallic", ntree, column, &context);
  }

  if (pxr::UsdShadeInput roughness_input = usd_shader.GetInput(usdtokens::roughness)) {
    set_node_input(roughness_input, principled, "Roughness", ntree, column, &context);
  }

  if (pxr::UsdShadeInput clearcoat_input = usd_shader.GetInput(usdtokens::clearcoat)) {
    set_node_input(clearcoat_input, principled, "Clearcoat", ntree, column, &context);
  }

  if (pxr::UsdShadeInput clearcoat_roughness_input = usd_shader.GetInput(
          usdtokens::clearcoatRoughness))
  {
    set_node_input(
        clearcoat_roughness_input, principled, "Clearcoat Roughness", ntree, column, &context);
  }

  if (pxr::UsdShadeInput opacity_input = usd_shader.GetInput(usdtokens::opacity)) {
    set_node_input(opacity_input, principled, "Alpha", ntree, column, &context);
  }

  if (pxr::UsdShadeInput ior_input = usd_shader.GetInput(usdtokens::ior)) {
    set_node_input(ior_input, principled, "IOR", ntree, column, &context);
  }

  if (pxr::UsdShadeInput normal_input = usd_shader.GetInput(usdtokens::normal)) {
    set_node_input(normal_input, principled, "Normal", ntree, column, &context);
  }
}

void USDMaterialReader::set_node_input(const pxr::UsdShadeInput &usd_input,
                                       bNode *dest_node,
                                       const char *dest_socket_name,
                                       bNodeTree *ntree,
                                       const int column,
                                       NodePlacementContext *r_ctx) const
{
  if (!(usd_input && dest_node && r_ctx)) {
    return;
  }

  if (usd_input.HasConnectedSource()) {
    /* The USD shader input has a connected source shader. Follow the connection
     * and attempt to convert the connected USD shader to a Blender node. */
    follow_connection(usd_input, dest_node, dest_socket_name, ntree, column, r_ctx);
  }
  else {
    /* Set the destination node socket value from the USD shader input value. */

    bNodeSocket *sock = nodeFindSocket(dest_node, SOCK_IN, dest_socket_name);
    if (!sock) {
      std::cerr << "ERROR: couldn't get destination node socket " << dest_socket_name << std::endl;
      return;
    }

    pxr::VtValue val;
    if (!usd_input.Get(&val)) {
      std::cerr << "ERROR: couldn't get value for usd shader input "
                << usd_input.GetPrim().GetPath() << std::endl;
      return;
    }

    switch (sock->type) {
      case SOCK_FLOAT:
        if (val.IsHolding<float>()) {
          ((bNodeSocketValueFloat *)sock->default_value)->value = val.UncheckedGet<float>();
        }
        else if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          float average = (v3f[0] + v3f[1] + v3f[2]) / 3.0f;
          ((bNodeSocketValueFloat *)sock->default_value)->value = average;
        }
        break;
      case SOCK_RGBA:
        if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, v3f.data());
        }
        break;
      case SOCK_VECTOR:
        if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          copy_v3_v3(((bNodeSocketValueVector *)sock->default_value)->value, v3f.data());
        }
        else if (val.IsHolding<pxr::GfVec2f>()) {
          pxr::GfVec2f v2f = val.UncheckedGet<pxr::GfVec2f>();
          copy_v2_v2(((bNodeSocketValueVector *)sock->default_value)->value, v2f.data());
        }
        break;
      default:
        std::cerr << "WARNING: unexpected type " << sock->idname << " for destination node socket "
                  << dest_socket_name << std::endl;
        break;
    }
  }
}

void USDMaterialReader::follow_connection(const pxr::UsdShadeInput &usd_input,
                                          bNode *dest_node,
                                          const char *dest_socket_name,
                                          bNodeTree *ntree,
                                          int column,
                                          NodePlacementContext *r_ctx) const
{
  if (!(usd_input && dest_node && dest_socket_name && ntree && r_ctx)) {
    return;
  }

  pxr::UsdShadeConnectableAPI source;
  pxr::TfToken source_name;
  pxr::UsdShadeAttributeType source_type;

  usd_input.GetConnectedSource(&source, &source_name, &source_type);

  if (!(source && source.GetPrim().IsA<pxr::UsdShadeShader>())) {
    return;
  }

  pxr::UsdShadeShader source_shader(source.GetPrim());

  if (!source_shader) {
    return;
  }

  pxr::TfToken shader_id;
  if (!source_shader.GetShaderId(&shader_id)) {
    std::cerr << "ERROR: couldn't get shader id for source shader "
              << source_shader.GetPrim().GetPath() << std::endl;
    return;
  }

  /* For now, only convert UsdUVTexture and UsdPrimvarReader_float2 inputs. */
  if (shader_id == usdtokens::UsdUVTexture) {

    if (STREQ(dest_socket_name, "Normal")) {

      /* The normal texture input requires creating a normal map node. */
      float locx = 0.0f;
      float locy = 0.0f;
      compute_node_loc(column + 1, &locx, &locy, r_ctx);

      bNode *normal_map = add_node(nullptr, ntree, SH_NODE_NORMAL_MAP, locx, locy);

      /* Currently, the Normal Map node has Tangent Space as the default,
       * which is what we need, so we don't need to explicitly set it. */

      /* Connect the Normal Map to the Normal input. */
      link_nodes(ntree, normal_map, "Normal", dest_node, "Normal");

      /* Now, create the Texture Image node input to the Normal Map "Color" input. */
      convert_usd_uv_texture(
          source_shader, source_name, normal_map, "Color", ntree, column + 2, r_ctx);
    }
    else {
      convert_usd_uv_texture(
          source_shader, source_name, dest_node, dest_socket_name, ntree, column + 1, r_ctx);
    }
  }
  else if (shader_id == usdtokens::UsdPrimvarReader_float2) {
    convert_usd_primvar_reader_float2(
        source_shader, source_name, dest_node, dest_socket_name, ntree, column + 1, r_ctx);
  }
}

void USDMaterialReader::convert_usd_uv_texture(const pxr::UsdShadeShader &usd_shader,
                                               const pxr::TfToken &usd_source_name,
                                               bNode *dest_node,
                                               const char *dest_socket_name,
                                               bNodeTree *ntree,
                                               const int column,
                                               NodePlacementContext *r_ctx) const
{
  if (!usd_shader || !dest_node || !ntree || !dest_socket_name || !bmain_ || !r_ctx) {
    return;
  }

  bNode *tex_image = get_cached_node(r_ctx->node_cache, usd_shader);

  if (tex_image == nullptr) {
    float locx = 0.0f;
    float locy = 0.0f;
    compute_node_loc(column, &locx, &locy, r_ctx);

    /* Create the Texture Image node. */
    tex_image = add_node(nullptr, ntree, SH_NODE_TEX_IMAGE, locx, locy);

    if (!tex_image) {
      std::cerr << "ERROR: Couldn't create SH_NODE_TEX_IMAGE for node input " << dest_socket_name
                << std::endl;
      return;
    }

    /* Cache newly created node. */
    cache_node(r_ctx->node_cache, usd_shader, tex_image);

    /* Load the texture image. */
    load_tex_image(usd_shader, tex_image);
  }

  /* Connect to destination node input. */

  /* Get the source socket name. */
  std::string source_socket_name = usd_source_name == usdtokens::a ? "Alpha" : "Color";

  link_nodes(ntree, tex_image, source_socket_name.c_str(), dest_node, dest_socket_name);

  /* Connect the texture image node "Vector" input. */
  if (pxr::UsdShadeInput st_input = usd_shader.GetInput(usdtokens::st)) {
    set_node_input(st_input, tex_image, "Vector", ntree, column, r_ctx);
  }
}

void USDMaterialReader::load_tex_image(const pxr::UsdShadeShader &usd_shader,
                                       bNode *tex_image) const
{
  if (!(usd_shader && tex_image && tex_image->type == SH_NODE_TEX_IMAGE)) {
    return;
  }

  /* Try to load the texture image. */
  pxr::UsdShadeInput file_input = usd_shader.GetInput(usdtokens::file);

  if (!file_input) {
    std::cerr << "WARNING: Couldn't get file input for USD shader " << usd_shader.GetPath()
              << std::endl;
    return;
  }

  /* File input may have a connected source, e.g., if it's been overridden by
   * an input on the mateial. */
  if (file_input.HasConnectedSource()) {
    pxr::UsdShadeConnectableAPI source;
    pxr::TfToken source_name;
    pxr::UsdShadeAttributeType source_type;

    if (file_input.GetConnectedSource(&source, &source_name, &source_type)) {
      file_input = source.GetInput(source_name);
    }
    else {
      std::cerr << "ERROR: couldn't get connected source for file input "
        << file_input.GetPrim().GetPath() << " " << file_input.GetFullName() << std::endl;
    }
  }

  pxr::VtValue file_val;
  if (!file_input.Get(&file_val) || !file_val.IsHolding<pxr::SdfAssetPath>()) {
    std::cerr << "WARNING: Couldn't get file input value for USD shader " << usd_shader.GetPath()
              << std::endl;
    return;
  }

  const pxr::SdfAssetPath &asset_path = file_val.Get<pxr::SdfAssetPath>();

  std::string file_path = asset_path.GetResolvedPath();

  if (file_path.empty()) {
    /* No resolved path, so use the asset path (usually
     * necessary for UDIM paths). */
    file_path = asset_path.GetAssetPath();

    if (!file_path.empty() && is_udim_path(file_path)) {
      /* Texture paths are frequently relative to the USD, so get
       * the absolute path. */
      if (pxr::SdfLayerHandle layer_handle = get_layer_handle(file_input.GetAttr())) {

        /* SdfLayer::ComputeAbsolutePath() doesn' work for context-dependent paths
         * where the file name has a UDIM token (e.g., '0/foo.<UDIM>.png').
         * We therefore compute the absolube path of the parent directory of the
         * UDIM file. */

        char file[FILE_MAXFILE];
        char dir[FILE_MAXDIR];
        BLI_path_split_dir_file(file_path.c_str(), dir, sizeof(dir), file, sizeof(file));

        if (strlen(dir) == 0) {
          /* No directory in path, assume asset is a sibling of the layer. */
          dir[0] = '.';
          dir[1] = '\0';
        }

        /* Get the absolute path of the directory relative to the layer. */
        std::string dir_abs_path = layer_handle->ComputeAbsolutePath(dir);

        char result[FILE_MAX];
        /* Finally, join the original file name with the absolute path. */
        BLI_path_join(result, FILE_MAX, dir_abs_path.c_str(), file);

        /* Use forward slashes. */
        BLI_str_replace_char(result, SEP, ALTSEP);
        file_path = result;
      }
    }
  }

  if (file_path.empty()) {
    std::cerr << "WARNING: Couldn't resolve image asset '" << asset_path
              << "' for Texture Image node." << std::endl;
    return;
  }

  /* Optionally copy the asset if it's inside a USDZ package. */

  const bool import_textures = params_.import_textures_mode != USD_TEX_IMPORT_NONE &&
                               should_import_asset(file_path);

  if (import_textures) {
    /* If we are packing the imported textures, we first write them
     * to a temporary directory. */
    const char *textures_dir = params_.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                   temp_textures_dir() :
                                   params_.import_textures_dir;

    const eUSDTexNameCollisionMode name_collision_mode = params_.import_textures_mode ==
                                                                 USD_TEX_IMPORT_PACK ?
                                                             USD_TEX_NAME_COLLISION_OVERWRITE :
                                                             params_.tex_name_collision_mode;

    file_path = import_asset(file_path.c_str(), textures_dir, name_collision_mode);
  }

  /* If this is a UDIM texture, this will store the
   * UDIM tile indices. */
  blender::Vector<int> udim_tiles;

  if (is_udim_path(file_path)) {
    udim_tiles = get_udim_tiles(file_path);
  }

  const char *im_file = file_path.c_str();

  Image *image = BKE_image_load_exists(bmain_, im_file);

  if (!image) {
    std::cerr << "WARNING: Couldn't open image file '" << im_file << "' for Texture Image node."
              << std::endl;
    return;
  }

  if (udim_tiles.size() > 0) {
    add_udim_tiles(image, udim_tiles);
  }

  tex_image->id = &image->id;

  /* Set texture color space.
   * TODO(makowalski): For now, just checking for RAW color space,
   * assuming sRGB otherwise, but more complex logic might be
   * required if the color space is "auto". */

  pxr::TfToken color_space = get_source_color_space(usd_shader);

  if (color_space.IsEmpty()) {
    color_space = file_input.GetAttr().GetColorSpace();
    /* TODO(makowalski): if the input is from a connected source
     * and fails to return a color space, should we also check the
     * color space on the current shader's file input? */
  }

  if (ELEM(color_space, usdtokens::RAW, usdtokens::raw)) {
    STRNCPY(image->colorspace_settings.name, "Raw");
  }

  NodeTexImage *storage = static_cast<NodeTexImage *>(tex_image->storage);
  storage->extension = get_image_extension(usd_shader, storage->extension);
  if (import_textures && params_.import_textures_mode == USD_TEX_IMPORT_PACK &&
      !BKE_image_has_packedfile(image))
  {
    BKE_image_packfiles(nullptr, image, ID_BLEND_PATH(bmain_, &image->id));
    if (BLI_is_dir(temp_textures_dir())) {
      BLI_delete(temp_textures_dir(), true, true);
    }
  }
}

void USDMaterialReader::convert_usd_primvar_reader_float2(
    const pxr::UsdShadeShader &usd_shader,
    const pxr::TfToken & /* usd_source_name */,
    bNode *dest_node,
    const char *dest_socket_name,
    bNodeTree *ntree,
    const int column,
    NodePlacementContext *r_ctx) const
{
  if (!usd_shader || !dest_node || !ntree || !dest_socket_name || !bmain_ || !r_ctx) {
    return;
  }

  bNode *uv_map = get_cached_node(r_ctx->node_cache, usd_shader);

  if (uv_map == nullptr) {
    float locx = 0.0f;
    float locy = 0.0f;
    compute_node_loc(column, &locx, &locy, r_ctx);

    /* Create the UV Map node. */
    uv_map = add_node(nullptr, ntree, SH_NODE_UVMAP, locx, locy);

    if (!uv_map) {
      std::cerr << "ERROR: Couldn't create SH_NODE_UVMAP for node input " << dest_socket_name
                << std::endl;
      return;
    }

    /* Cache newly created node. */
    cache_node(r_ctx->node_cache, usd_shader, uv_map);

    /* Set the texmap name. */
    pxr::UsdShadeInput varname_input = usd_shader.GetInput(usdtokens::varname);

    /* First check if the shader's "varname" input is connected to another source,
     * and use that instead if so. */
    if (varname_input) {
      for (const pxr::UsdShadeConnectionSourceInfo &source_info :
           varname_input.GetConnectedSources()) {
        pxr::UsdShadeShader shader = pxr::UsdShadeShader(source_info.source.GetPrim());
        pxr::UsdShadeInput secondary_varname_input = shader.GetInput(source_info.sourceName);
        if (secondary_varname_input) {
          varname_input = secondary_varname_input;
          break;
        }
      }
    }

    if (varname_input) {
      pxr::VtValue varname_val;
      /* The varname input may be a string or TfToken, so just cast it to a string.
       * The Cast function is defined to provide an empty result if it fails. */
      if (varname_input.Get(&varname_val) && varname_val.CanCastToTypeid(typeid(std::string))) {
        std::string varname = varname_val.Cast<std::string>().Get<std::string>();
        if (!varname.empty()) {
          NodeShaderUVMap *storage = (NodeShaderUVMap *)uv_map->storage;
          BLI_strncpy(storage->uv_map, varname.c_str(), sizeof(storage->uv_map));
        }
      }
    }
  }

  /* Connect to destination node input. */
  link_nodes(ntree, uv_map, "UV", dest_node, dest_socket_name);
}

void USDMaterialReader::pack_imported_textures(Material *material, bool delete_temp_textures_dir) const
{
  if (!(material && material->use_nodes)) {
    return;
  }

  for (bNode *node = (bNode *)material->nodetree->nodes.first; node; node = node->next) {
    if (!(ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT))) {
      continue;
    }
    Image *image = reinterpret_cast<Image *>(node->id);
    if (!image || BKE_image_has_packedfile(image)) {
      continue;
    }

    if (image->filepath[0] == '\0') {
      continue;
    }

    char dir_path[FILE_MAXDIR];
    BLI_path_split_dir_part(image->filepath, dir_path, sizeof(dir_path));

    if (BLI_path_cmp_normalized(dir_path, temp_textures_dir()) == 0) {
      /* Texture was saved to the temporary import directory, so pack it. */
      BKE_image_packfiles(nullptr, image, ID_BLEND_PATH(bmain_, &image->id));
    }
  }

  if (delete_temp_textures_dir && BLI_is_dir(temp_textures_dir())) {
    BLI_delete(temp_textures_dir(), true, true);
  }
}

void build_material_map(const Main *bmain, std::map<std::string, Material *> *r_mat_map)
{
  BLI_assert_msg(r_mat_map, "...");

  LISTBASE_FOREACH (Material *, material, &bmain->materials) {
    std::string usd_name = pxr::TfMakeValidIdentifier(material->id.name + 2);
    (*r_mat_map)[usd_name] = material;
  }
}

Material *find_existing_material(const pxr::SdfPath &usd_mat_path,
                                 const USDImportParams &params,
                                 const std::map<std::string, Material *> &mat_map,
                                 const std::map<std::string, std::string> &usd_path_to_mat_name)
{
  if (params.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
    /* Check if we've already created the Blender material with a modified name. */
    std::map<std::string, std::string>::const_iterator path_to_name_iter =
        usd_path_to_mat_name.find(usd_mat_path.GetAsString());

    if (path_to_name_iter == usd_path_to_mat_name.end()) {
      return nullptr;
    }

    std::string mat_name = path_to_name_iter->second;
    std::map<std::string, Material *>::const_iterator mat_iter = mat_map.find(mat_name);
    BLI_assert_msg(mat_iter != mat_map.end(),
                   "Previously created material cannot be found any more");
    return mat_iter->second;
  }

  std::string mat_name = usd_mat_path.GetName();
  std::map<std::string, Material *>::const_iterator mat_iter = mat_map.find(mat_name);

  if (mat_iter == mat_map.end()) {
    return nullptr;
  }

  return mat_iter->second;
}

}  // namespace blender::io::usd
