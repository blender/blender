/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_material.hh"

#include "usd_asset_utils.hh"

#include "BKE_appdir.hh"
#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.h"

#include "BLI_fileops.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_material_types.h"

#include "WM_api.hh"

#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace usdtokens {

/* Parameter names. */
static const pxr::TfToken a("a", pxr::TfToken::Immortal);
static const pxr::TfToken b("b", pxr::TfToken::Immortal);
static const pxr::TfToken bias("bias", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoat("clearcoat", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoatRoughness("clearcoatRoughness", pxr::TfToken::Immortal);
static const pxr::TfToken diffuseColor("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken emissiveColor("emissiveColor", pxr::TfToken::Immortal);
static const pxr::TfToken file("file", pxr::TfToken::Immortal);
static const pxr::TfToken g("g", pxr::TfToken::Immortal);
static const pxr::TfToken ior("ior", pxr::TfToken::Immortal);
static const pxr::TfToken in("in", pxr::TfToken::Immortal);
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
static const pxr::TfToken scale("scale", pxr::TfToken::Immortal);
static const pxr::TfToken sourceColorSpace("sourceColorSpace", pxr::TfToken::Immortal);
static const pxr::TfToken specularColor("specularColor", pxr::TfToken::Immortal);
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken varname("varname", pxr::TfToken::Immortal);

/* Color space names. */
static const pxr::TfToken auto_("auto", pxr::TfToken::Immortal);
static const pxr::TfToken sRGB("sRGB", pxr::TfToken::Immortal);
static const pxr::TfToken raw("raw", pxr::TfToken::Immortal);
static const pxr::TfToken RAW("RAW", pxr::TfToken::Immortal);

/* Wrap mode names. */
static const pxr::TfToken black("black", pxr::TfToken::Immortal);
static const pxr::TfToken clamp("clamp", pxr::TfToken::Immortal);
static const pxr::TfToken repeat("repeat", pxr::TfToken::Immortal);
static const pxr::TfToken wrapS("wrapS", pxr::TfToken::Immortal);
static const pxr::TfToken wrapT("wrapT", pxr::TfToken::Immortal);

/* Transform 2d names. */
static const pxr::TfToken rotation("rotation", pxr::TfToken::Immortal);
static const pxr::TfToken translation("translation", pxr::TfToken::Immortal);

/* USD shader names. */
static const pxr::TfToken UsdPreviewSurface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken UsdPrimvarReader_float2("UsdPrimvarReader_float2",
                                                  pxr::TfToken::Immortal);
static const pxr::TfToken UsdUVTexture("UsdUVTexture", pxr::TfToken::Immortal);
static const pxr::TfToken UsdTransform2d("UsdTransform2d", pxr::TfToken::Immortal);
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

/**
 * Generate a key for caching a Blender node created for a given USD shader
 * by returning the shader prim path with an optional tag suffix.  The tag can
 * be specified in order to generate a unique key when more than one Blender
 * node is created for the USD shader.
 */
static std::string get_key(const pxr::UsdShadeShader &usd_shader, const char *tag)
{
  std::string key = usd_shader.GetPath().GetAsString();
  if (tag) {
    key += ":";
    key += tag;
  }
  return key;
}

/* Returns the Blender node previously cached for
 * the given USD shader in the given map.  Returns
 * null if no cached shader was found. */
static bNode *get_cached_node(const ShaderToNodeMap &node_cache,
                              const pxr::UsdShadeShader &usd_shader,
                              const char *tag = nullptr)
{
  return node_cache.lookup_default(get_key(usd_shader, tag), nullptr);
}

/* Cache the Blender node translated from the given USD shader
 * in the given map. */
static void cache_node(ShaderToNodeMap &node_cache,
                       const pxr::UsdShadeShader &usd_shader,
                       bNode *node,
                       const char *tag = nullptr)
{
  node_cache.add(get_key(usd_shader, tag), node);
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
    CLOG_ERROR(&LOG, "Couldn't find output socket %s", sock_out);
    return;
  }

  bNodeSocket *dest_socket = nodeFindSocket(dest, SOCK_IN, sock_in);

  if (!dest_socket) {
    CLOG_ERROR(&LOG, "Couldn't find input socket %s", sock_in);
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
  STRNCPY(base_udim_path, file_path.c_str());

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

static pxr::UsdShadeInput get_input(const pxr::UsdShadeShader &usd_shader,
                                    const pxr::TfToken &input_name)
{
  pxr::UsdShadeInput input = usd_shader.GetInput(input_name);

  /* Check if the shader's input is connected to another source,
   * and use that instead if so. */
  if (input) {
    for (const pxr::UsdShadeConnectionSourceInfo &source_info : input.GetConnectedSources()) {
      pxr::UsdShadeShader shader = pxr::UsdShadeShader(source_info.source.GetPrim());
      pxr::UsdShadeInput secondary_input = shader.GetInput(source_info.sourceName);
      if (secondary_input) {
        input = secondary_input;
        break;
      }
    }
  }

  return input;
}

static bNodeSocket *get_input_socket(bNode *node, const char *identifier, ReportList *reports)
{
  bNodeSocket *sock = nodeFindSocket(node, SOCK_IN, identifier);
  if (!sock) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Error: Couldn't get input socket %s for node %s",
                __func__,
                identifier,
                node->idname);
  }

  return sock;
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
    r_ctx->column_offsets.append(0.0f);
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

    set_viewport_material_props(mtl, usd_preview);

    /* Optionally, create shader nodes to represent a UsdPreviewSurface. */
    if (params_.import_usd_preview) {
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
    CLOG_ERROR(&LOG,
               "Couldn't create SH_NODE_BSDF_PRINCIPLED node for USD shader %s",
               usd_shader.GetPath().GetAsString().c_str());
    return;
  }

  /* Create the material output node. */
  bNode *output = add_node(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL, 300.0f, 300.0f);

  if (!output) {
    CLOG_ERROR(&LOG,
               "Couldn't create SH_NODE_OUTPUT_MATERIAL node for USD shader %s",
               usd_shader.GetPath().GetAsString().c_str());
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
    set_node_input(diffuse_input, principled, "Base Color", ntree, column, &context, true);
  }

  float emission_strength = 0.0f;
  if (pxr::UsdShadeInput emissive_input = usd_shader.GetInput(usdtokens::emissiveColor)) {
    if (set_node_input(
            emissive_input, principled, "Emission Color", ntree, column, &context, true))
    {
      emission_strength = 1.0f;
    }
  }

  bNodeSocket *emission_strength_sock = nodeFindSocket(principled, SOCK_IN, "Emission Strength");
  ((bNodeSocketValueFloat *)emission_strength_sock->default_value)->value = emission_strength;

  if (pxr::UsdShadeInput specular_input = usd_shader.GetInput(usdtokens::specularColor)) {
    set_node_input(specular_input, principled, "Specular Tint", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput metallic_input = usd_shader.GetInput(usdtokens::metallic)) {
    set_node_input(metallic_input, principled, "Metallic", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput roughness_input = usd_shader.GetInput(usdtokens::roughness)) {
    set_node_input(roughness_input, principled, "Roughness", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput coat_input = usd_shader.GetInput(usdtokens::clearcoat)) {
    set_node_input(coat_input, principled, "Coat Weight", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput coat_roughness_input = usd_shader.GetInput(usdtokens::clearcoatRoughness))
  {
    set_node_input(
        coat_roughness_input, principled, "Coat Roughness", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput opacity_input = usd_shader.GetInput(usdtokens::opacity)) {
    set_node_input(opacity_input, principled, "Alpha", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput ior_input = usd_shader.GetInput(usdtokens::ior)) {
    set_node_input(ior_input, principled, "IOR", ntree, column, &context, false);
  }

  if (pxr::UsdShadeInput normal_input = usd_shader.GetInput(usdtokens::normal)) {
    set_node_input(normal_input, principled, "Normal", ntree, column, &context, false);
  }
}

bool USDMaterialReader::set_node_input(const pxr::UsdShadeInput &usd_input,
                                       bNode *dest_node,
                                       const char *dest_socket_name,
                                       bNodeTree *ntree,
                                       const int column,
                                       NodePlacementContext *r_ctx,
                                       bool is_color_corrected) const
{
  if (!(usd_input && dest_node && r_ctx)) {
    return false;
  }

  if (usd_input.HasConnectedSource()) {
    /* The USD shader input has a connected source shader. Follow the connection
     * and attempt to convert the connected USD shader to a Blender node. */
    return follow_connection(
        usd_input, dest_node, dest_socket_name, ntree, column, r_ctx, is_color_corrected);
  }
  else {
    /* Set the destination node socket value from the USD shader input value. */

    bNodeSocket *sock = nodeFindSocket(dest_node, SOCK_IN, dest_socket_name);
    if (!sock) {
      CLOG_ERROR(&LOG, "Couldn't get destination node socket %s", dest_socket_name);
      return false;
    }

    pxr::VtValue val;
    if (!usd_input.Get(&val)) {
      CLOG_ERROR(&LOG,
                 "Couldn't get value for usd shader input %s",
                 usd_input.GetPrim().GetPath().GetAsString().c_str());
      return false;
    }

    switch (sock->type) {
      case SOCK_FLOAT:
        if (val.IsHolding<float>()) {
          ((bNodeSocketValueFloat *)sock->default_value)->value = val.UncheckedGet<float>();
          return true;
        }
        else if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          float average = (v3f[0] + v3f[1] + v3f[2]) / 3.0f;
          ((bNodeSocketValueFloat *)sock->default_value)->value = average;
          return true;
        }
        break;
      case SOCK_RGBA:
        if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, v3f.data());
          return true;
        }
        break;
      case SOCK_VECTOR:
        if (val.IsHolding<pxr::GfVec3f>()) {
          pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
          copy_v3_v3(((bNodeSocketValueVector *)sock->default_value)->value, v3f.data());
          return true;
        }
        else if (val.IsHolding<pxr::GfVec2f>()) {
          pxr::GfVec2f v2f = val.UncheckedGet<pxr::GfVec2f>();
          copy_v2_v2(((bNodeSocketValueVector *)sock->default_value)->value, v2f.data());
          return true;
        }
        break;
      default:
        CLOG_WARN(&LOG,
                  "Unexpected type %s for destination node socket %s",
                  sock->idname,
                  dest_socket_name);
        break;
    }
  }

  return false;
}

struct IntermediateNode {
  bNode *node;
  const char *sock_input_name;
  const char *sock_output_name;
};

static IntermediateNode add_normal_map(bNodeTree *ntree, int column, NodePlacementContext *r_ctx)
{
  float locx = 0.0f;
  float locy = 0.0f;
  compute_node_loc(column, &locx, &locy, r_ctx);

  /* Currently, the Normal Map node has Tangent Space as the default,
   * which is what we need, so we don't need to explicitly set it. */
  IntermediateNode normal_map{};
  normal_map.node = add_node(nullptr, ntree, SH_NODE_NORMAL_MAP, locx, locy);
  normal_map.sock_input_name = "Color";
  normal_map.sock_output_name = "Normal";

  return normal_map;
}

static IntermediateNode add_scale_bias(const pxr::UsdShadeShader &usd_shader,
                                       bNodeTree *ntree,
                                       int column,
                                       bool feeds_normal_map,
                                       NodePlacementContext *r_ctx)
{
  /* Handle the scale-bias inputs if present. */
  pxr::UsdShadeInput scale_input = usd_shader.GetInput(usdtokens::scale);
  pxr::UsdShadeInput bias_input = usd_shader.GetInput(usdtokens::bias);
  pxr::GfVec4f scale(1.0f, 1.0f, 1.0f, 1.0f);
  pxr::GfVec4f bias(0.0f, 0.0f, 0.0f, 0.0f);

  pxr::VtValue val;
  if (scale_input.Get(&val) && val.CanCast<pxr::GfVec4f>()) {
    scale = val.Cast<pxr::GfVec4f>(val).UncheckedGet<pxr::GfVec4f>();
  }
  if (bias_input.Get(&val) && val.CanCast<pxr::GfVec4f>()) {
    bias = val.Cast<pxr::GfVec4f>(val).UncheckedGet<pxr::GfVec4f>();
  }

  /* Nothing to be done if the values match their defaults. */
  if (scale == pxr::GfVec4f{1.0f, 1.0f, 1.0f, 1.0f} &&
      bias == pxr::GfVec4f{0.0f, 0.0f, 0.0f, 0.0f})
  {
    return {};
  }

  /* Nothing to be done if this feeds a Normal Map and the values match those defaults. */
  if (feeds_normal_map && (scale[0] == 2.0f && scale[1] == 2.0f && scale[2] == 2.0f) &&
      (bias[0] == -1.0f && bias[1] == -1.0f && bias[2] == -1.0f))
  {
    return {};
  }

  float locx = 0.0f;
  float locy = 0.0f;
  /* If we know a Normal Map node will be involved, leave room for the another
   * adjustment node which will be added later. */
  compute_node_loc(feeds_normal_map ? column + 1 : column, &locx, &locy, r_ctx);

  IntermediateNode scale_bias{};

  const char *tag = "scale_bias";
  bNode *node = get_cached_node(r_ctx->node_cache, usd_shader, tag);

  if (!node) {
    node = add_node(nullptr, ntree, SH_NODE_VECTOR_MATH, locx, locy);
    cache_node(r_ctx->node_cache, usd_shader, node, tag);
  }

  scale_bias.node = node;
  scale_bias.node->custom1 = NODE_VECTOR_MATH_MULTIPLY_ADD;
  scale_bias.sock_input_name = "Vector";
  scale_bias.sock_output_name = "Vector";

  bNodeSocket *sock_scale = nodeFindSocket(scale_bias.node, SOCK_IN, "Vector_001");
  bNodeSocket *sock_bias = nodeFindSocket(scale_bias.node, SOCK_IN, "Vector_002");
  copy_v3_v3(((bNodeSocketValueVector *)sock_scale->default_value)->value, scale.data());
  copy_v3_v3(((bNodeSocketValueVector *)sock_bias->default_value)->value, bias.data());

  return scale_bias;
}

static IntermediateNode add_scale_bias_adjust(bNodeTree *ntree,
                                              int column,
                                              NodePlacementContext *r_ctx)
{
  float locx = 0.0f;
  float locy = 0.0f;
  compute_node_loc(column, &locx, &locy, r_ctx);

  IntermediateNode adjust{};
  adjust.node = add_node(nullptr, ntree, SH_NODE_VECTOR_MATH, locx, locy);
  adjust.node->custom1 = NODE_VECTOR_MATH_MULTIPLY_ADD;
  adjust.sock_input_name = "Vector";
  adjust.sock_output_name = "Vector";

  bNodeSocket *sock_scale = nodeFindSocket(adjust.node, SOCK_IN, "Vector_001");
  bNodeSocket *sock_bias = nodeFindSocket(adjust.node, SOCK_IN, "Vector_002");
  copy_v3_fl3(((bNodeSocketValueVector *)sock_scale->default_value)->value, 0.5f, 0.5f, 0.5f);
  copy_v3_fl3(((bNodeSocketValueVector *)sock_bias->default_value)->value, 0.5f, 0.5f, 0.5f);

  return adjust;
}

static IntermediateNode add_separate_color(const pxr::UsdShadeShader &usd_shader,
                                           const pxr::TfToken &usd_source_name,
                                           bNodeTree *ntree,
                                           int column,
                                           NodePlacementContext *r_ctx)
{
  IntermediateNode separate_color{};

  if (usd_source_name == usdtokens::r || usd_source_name == usdtokens::g ||
      usd_source_name == usdtokens::b)
  {
    const char *tag = "separate_color";
    bNode *node = get_cached_node(r_ctx->node_cache, usd_shader, tag);

    if (!node) {
      float locx = 0.0f;
      float locy = 0.0f;
      compute_node_loc(column, &locx, &locy, r_ctx);

      node = add_node(nullptr, ntree, SH_NODE_SEPARATE_COLOR, locx, locy);
      cache_node(r_ctx->node_cache, usd_shader, node, tag);
    }

    separate_color.node = node;
    separate_color.sock_input_name = "Color";

    if (usd_source_name == usdtokens::r) {
      separate_color.sock_output_name = "Red";
    }
    if (usd_source_name == usdtokens::g) {
      separate_color.sock_output_name = "Green";
    }
    if (usd_source_name == usdtokens::b) {
      separate_color.sock_output_name = "Blue";
    }
  }

  return separate_color;
}

bool USDMaterialReader::follow_connection(const pxr::UsdShadeInput &usd_input,
                                          bNode *dest_node,
                                          const char *dest_socket_name,
                                          bNodeTree *ntree,
                                          int column,
                                          NodePlacementContext *r_ctx,
                                          bool is_color_corrected) const
{
  if (!(usd_input && dest_node && dest_socket_name && ntree && r_ctx)) {
    return false;
  }

  pxr::UsdShadeConnectableAPI source;
  pxr::TfToken source_name;
  pxr::UsdShadeAttributeType source_type;

  usd_input.GetConnectedSource(&source, &source_name, &source_type);

  if (!(source && source.GetPrim().IsA<pxr::UsdShadeShader>())) {
    return false;
  }

  pxr::UsdShadeShader source_shader(source.GetPrim());

  if (!source_shader) {
    return false;
  }

  pxr::TfToken shader_id;
  if (!source_shader.GetShaderId(&shader_id)) {
    CLOG_ERROR(&LOG,
               "Couldn't get shader id for source shader %s",
               source_shader.GetPath().GetAsString().c_str());
    return false;
  }

  /* For now, only convert UsdUVTexture, UsdTransform2d and UsdPrimvarReader_float2 inputs. */
  if (shader_id == usdtokens::UsdUVTexture) {
    int shift = 1;

    /* Create a Normal Map node if the source is flowing into a 'Normal' socket. */
    IntermediateNode normal_map{};
    const bool is_normal_map = STREQ(dest_socket_name, "Normal");
    if (is_normal_map) {
      normal_map = add_normal_map(ntree, column + shift, r_ctx);
      shift++;
    }

    /* Create a Separate Color node if necessary. */
    IntermediateNode separate_color = add_separate_color(
        source_shader, source_name, ntree, column + shift, r_ctx);
    if (separate_color.node) {
      shift++;
    }

    /* Create a Scale-Bias adjustment node if necessary. */
    IntermediateNode scale_bias = add_scale_bias(
        source_shader, ntree, column + shift, is_normal_map, r_ctx);

    /* Wire up any intermediate nodes that are present. Keep track of the
     * final "target" destination for the Image link. */
    bNode *target_node = dest_node;
    const char *target_sock_name = dest_socket_name;
    if (normal_map.node) {
      /* If a scale-bias node is required, we need to re-adjust the output
       * so it can be passed into the NormalMap node properly. */
      if (scale_bias.node) {
        IntermediateNode re_adjust = add_scale_bias_adjust(ntree, column + shift, r_ctx);
        link_nodes(ntree,
                   scale_bias.node,
                   scale_bias.sock_output_name,
                   re_adjust.node,
                   re_adjust.sock_input_name);
        link_nodes(ntree,
                   re_adjust.node,
                   re_adjust.sock_output_name,
                   normal_map.node,
                   normal_map.sock_input_name);

        target_node = scale_bias.node;
        target_sock_name = scale_bias.sock_input_name;
        shift += 2;
      }
      else {
        target_node = normal_map.node;
        target_sock_name = normal_map.sock_input_name;
      }

      link_nodes(ntree, normal_map.node, normal_map.sock_output_name, dest_node, dest_socket_name);
    }
    else if (scale_bias.node) {
      if (separate_color.node) {
        link_nodes(ntree,
                   separate_color.node,
                   separate_color.sock_output_name,
                   dest_node,
                   dest_socket_name);
        link_nodes(ntree,
                   scale_bias.node,
                   scale_bias.sock_output_name,
                   separate_color.node,
                   separate_color.sock_input_name);
      }
      else {
        link_nodes(
            ntree, scale_bias.node, scale_bias.sock_output_name, dest_node, dest_socket_name);
      }
      target_node = scale_bias.node;
      target_sock_name = scale_bias.sock_input_name;
      shift++;
    }
    else if (separate_color.node) {
      link_nodes(ntree,
                 separate_color.node,
                 separate_color.sock_output_name,
                 dest_node,
                 dest_socket_name);
      target_node = separate_color.node;
      target_sock_name = separate_color.sock_input_name;
    }

    convert_usd_uv_texture(source_shader,
                           source_name,
                           target_node,
                           target_sock_name,
                           ntree,
                           column + shift,
                           r_ctx,
                           is_color_corrected);
  }
  else if (shader_id == usdtokens::UsdPrimvarReader_float2) {
    convert_usd_primvar_reader_float2(
        source_shader, source_name, dest_node, dest_socket_name, ntree, column + 1, r_ctx);
  }
  else if (shader_id == usdtokens::UsdTransform2d) {
    convert_usd_transform_2d(source_shader, dest_node, dest_socket_name, ntree, column + 1, r_ctx);
  }

  return true;
}

void USDMaterialReader::convert_usd_uv_texture(const pxr::UsdShadeShader &usd_shader,
                                               const pxr::TfToken &usd_source_name,
                                               bNode *dest_node,
                                               const char *dest_socket_name,
                                               bNodeTree *ntree,
                                               const int column,
                                               NodePlacementContext *r_ctx,
                                               bool is_color_corrected) const
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
      CLOG_ERROR(&LOG, "Couldn't create SH_NODE_TEX_IMAGE for node input %s", dest_socket_name);
      return;
    }

    /* Cache newly created node. */
    cache_node(r_ctx->node_cache, usd_shader, tex_image);

    /* Load the texture image. */
    load_tex_image(usd_shader, tex_image, is_color_corrected);
  }

  /* Connect to destination node input. */

  /* Get the source socket name. */
  std::string source_socket_name = usd_source_name == usdtokens::a ? "Alpha" : "Color";

  link_nodes(ntree, tex_image, source_socket_name.c_str(), dest_node, dest_socket_name);

  /* Connect the texture image node "Vector" input. */
  if (pxr::UsdShadeInput st_input = usd_shader.GetInput(usdtokens::st)) {
    set_node_input(st_input, tex_image, "Vector", ntree, column, r_ctx, false);
  }
}

void USDMaterialReader::convert_usd_transform_2d(const pxr::UsdShadeShader &usd_shader,
                                                 bNode *dest_node,
                                                 const char *dest_socket_name,
                                                 bNodeTree *ntree,
                                                 int column,
                                                 NodePlacementContext *r_ctx) const
{
  if (!usd_shader || !dest_node || !ntree || !dest_socket_name || !bmain_ || !r_ctx) {
    return;
  }

  bNode *mapping = get_cached_node(r_ctx->node_cache, usd_shader);

  if (mapping == nullptr) {
    float locx = 0.0f;
    float locy = 0.0f;
    compute_node_loc(column, &locx, &locy, r_ctx);

    /* Create the MAPPING node. */
    mapping = add_node(nullptr, ntree, SH_NODE_MAPPING, locx, locy);

    if (!mapping) {
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "%s: Couldn't create SH_NODE_MAPPING for node input %s",
                  __func__,
                  dest_socket_name);
      return;
    }

    /* Cache newly created node. */
    cache_node(r_ctx->node_cache, usd_shader, mapping);

    mapping->custom1 = TEXMAP_TYPE_POINT;

    if (bNodeSocket *scale_socket = get_input_socket(mapping, "Scale", reports())) {
      if (pxr::UsdShadeInput scale_input = get_input(usd_shader, usdtokens::scale)) {
        pxr::VtValue val;
        if (scale_input.Get(&val) && val.CanCast<pxr::GfVec2f>()) {
          pxr::GfVec2f scale_val = val.Cast<pxr::GfVec2f>().UncheckedGet<pxr::GfVec2f>();
          float scale[3] = {scale_val[0], scale_val[1], 1.0f};
          copy_v3_v3(((bNodeSocketValueVector *)scale_socket->default_value)->value, scale);
        }
      }
    }

    if (bNodeSocket *loc_socket = get_input_socket(mapping, "Location", reports())) {
      if (pxr::UsdShadeInput trans_input = get_input(usd_shader, usdtokens::translation)) {
        pxr::VtValue val;
        if (trans_input.Get(&val) && val.CanCast<pxr::GfVec2f>()) {
          pxr::GfVec2f trans_val = val.Cast<pxr::GfVec2f>().UncheckedGet<pxr::GfVec2f>();
          float loc[3] = {trans_val[0], trans_val[1], 0.0f};
          copy_v3_v3(((bNodeSocketValueVector *)loc_socket->default_value)->value, loc);
        }
      }
    }

    if (bNodeSocket *rot_socket = get_input_socket(mapping, "Rotation", reports())) {
      if (pxr::UsdShadeInput rot_input = get_input(usd_shader, usdtokens::rotation)) {
        pxr::VtValue val;
        if (rot_input.Get(&val) && val.CanCast<float>()) {
          float rot_val = val.Cast<float>().UncheckedGet<float>() * M_PI / 180.0f;
          float rot[3] = {0.0f, 0.0f, rot_val};
          copy_v3_v3(((bNodeSocketValueVector *)rot_socket->default_value)->value, rot);
        }
      }
    }
  }

  /* Connect to destination node input. */
  link_nodes(ntree, mapping, "Vector", dest_node, dest_socket_name);

  /* Connect the mapping node "Vector" input. */
  if (pxr::UsdShadeInput in_input = usd_shader.GetInput(usdtokens::in)) {
    set_node_input(in_input, mapping, "Vector", ntree, column, r_ctx, false);
  }
}

void USDMaterialReader::load_tex_image(const pxr::UsdShadeShader &usd_shader,
                                       bNode *tex_image,
                                       bool is_color_corrected) const
{
  if (!(usd_shader && tex_image && tex_image->type == SH_NODE_TEX_IMAGE)) {
    return;
  }

  /* Try to load the texture image. */
  pxr::UsdShadeInput file_input = usd_shader.GetInput(usdtokens::file);

  if (!file_input) {
    CLOG_WARN(&LOG,
              "Couldn't get file input for USD shader %s",
              usd_shader.GetPath().GetAsString().c_str());
    return;
  }

  pxr::VtValue file_val;
  if (!file_input.Get(&file_val) || !file_val.IsHolding<pxr::SdfAssetPath>()) {
    CLOG_WARN(&LOG,
              "Couldn't get file input value for USD shader %s",
              usd_shader.GetPath().GetAsString().c_str());
    return;
  }

  const pxr::SdfAssetPath &asset_path = file_val.Get<pxr::SdfAssetPath>();
  std::string file_path = asset_path.GetResolvedPath();
  if (file_path.empty()) {
    /* No resolved path, so use the asset path (usually
     * necessary for UDIM paths). */
    file_path = asset_path.GetAssetPath();

    /* Texture paths are frequently relative to the USD, so get
     * the absolute path. */
    if (pxr::SdfLayerHandle layer_handle = get_layer_handle(file_input.GetAttr())) {
      file_path = layer_handle->ComputeAbsolutePath(file_path);
    }
  }

  if (file_path.empty()) {
    CLOG_WARN(&LOG,
              " Couldn't resolve image asset '%s' for Texture Image node",
              asset_path.GetAssetPath().c_str());
    return;
  }

  /* Optionally copy the asset if it's inside a USDZ package. */

  const bool import_textures = params_.import_textures_mode != USD_TEX_IMPORT_NONE &&
                               pxr::ArIsPackageRelativePath(file_path);

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

    file_path = import_asset(file_path.c_str(), textures_dir, name_collision_mode, reports());
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
    CLOG_WARN(&LOG, "Couldn't open image file '%s' for Texture Image node", im_file);
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
  }

  if (color_space.IsEmpty()) {
    /* At this point, assume the "auto" space and translate accordingly. */
    color_space = usdtokens::auto_;
  }

  if (color_space == usdtokens::auto_) {
    /* If it's auto, determine whether to apply color correction based
     * on incoming connection (passed in from outer functions). */
    STRNCPY(image->colorspace_settings.name, is_color_corrected ? "sRGB" : "Non-Color");
  }

  else if (color_space == usdtokens::sRGB) {
    STRNCPY(image->colorspace_settings.name, "sRGB");
  }

  /*
   * Due to there being a lot of non-compliant USD assets out there, this is
   * a special case where we need to check for different spellings here.
   * On write, we are *only* using the correct, lower-case "raw" token.
   */
  else if (ELEM(color_space, usdtokens::RAW, usdtokens::raw)) {
    STRNCPY(image->colorspace_settings.name, "Non-Color");
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

void USDMaterialReader::convert_usd_primvar_reader_float2(const pxr::UsdShadeShader &usd_shader,
                                                          const pxr::TfToken & /*usd_source_name*/,
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
      CLOG_ERROR(&LOG, "Couldn't create SH_NODE_UVMAP for node input %s", dest_socket_name);
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
           varname_input.GetConnectedSources())
      {
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
          STRNCPY(storage->uv_map, varname.c_str());
        }
      }
    }
  }

  /* Connect to destination node input. */
  link_nodes(ntree, uv_map, "UV", dest_node, dest_socket_name);
}

void build_material_map(const Main *bmain, blender::Map<std::string, Material *> *r_mat_map)
{
  BLI_assert_msg(r_mat_map, "...");

  LISTBASE_FOREACH (Material *, material, &bmain->materials) {
    std::string usd_name = pxr::TfMakeValidIdentifier(material->id.name + 2);
    r_mat_map->lookup_or_add_default(usd_name) = material;
  }
}

Material *find_existing_material(
    const pxr::SdfPath &usd_mat_path,
    const USDImportParams &params,
    const blender::Map<std::string, Material *> &mat_map,
    const blender::Map<std::string, std::string> &usd_path_to_mat_name)
{
  if (params.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
    /* Check if we've already created the Blender material with a modified name. */
    const std::string *mat_name = usd_path_to_mat_name.lookup_ptr(usd_mat_path.GetAsString());
    if (mat_name == nullptr) {
      return nullptr;
    }

    Material *mat = mat_map.lookup_default(*mat_name, nullptr);
    BLI_assert_msg(mat != nullptr, "Previously created material cannot be found any more");
    return mat;
  }

  return mat_map.lookup_default(usd_mat_path.GetName(), nullptr);
}

}  // namespace blender::io::usd
