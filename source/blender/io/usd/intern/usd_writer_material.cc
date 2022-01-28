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

#include "usd_writer_material.h"

#include "usd.h"
#include "usd_exporter_context.h"

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_material_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/scope.h>

#include <iostream>

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
// Materials
static const pxr::TfToken clearcoat("clearcoat", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoatRoughness("clearcoatRoughness", pxr::TfToken::Immortal);
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken uv_texture("UsdUVTexture", pxr::TfToken::Immortal);
static const pxr::TfToken primvar_float2("UsdPrimvarReader_float2", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken specular("specular", pxr::TfToken::Immortal);
static const pxr::TfToken opacity("opacity", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
static const pxr::TfToken perspective("perspective", pxr::TfToken::Immortal);
static const pxr::TfToken orthographic("orthographic", pxr::TfToken::Immortal);
static const pxr::TfToken rgb("rgb", pxr::TfToken::Immortal);
static const pxr::TfToken r("r", pxr::TfToken::Immortal);
static const pxr::TfToken g("g", pxr::TfToken::Immortal);
static const pxr::TfToken b("b", pxr::TfToken::Immortal);
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken result("result", pxr::TfToken::Immortal);
static const pxr::TfToken varname("varname", pxr::TfToken::Immortal);
static const pxr::TfToken out("out", pxr::TfToken::Immortal);
static const pxr::TfToken normal("normal", pxr::TfToken::Immortal);
static const pxr::TfToken ior("ior", pxr::TfToken::Immortal);
static const pxr::TfToken file("file", pxr::TfToken::Immortal);
static const pxr::TfToken preview("preview", pxr::TfToken::Immortal);
static const pxr::TfToken raw("raw", pxr::TfToken::Immortal);
static const pxr::TfToken sRGB("sRGB", pxr::TfToken::Immortal);
static const pxr::TfToken sourceColorSpace("sourceColorSpace", pxr::TfToken::Immortal);
static const pxr::TfToken Shader("Shader", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* Cycles specific tokens. */
namespace cyclestokens {
static const pxr::TfToken UVMap("UVMap", pxr::TfToken::Immortal);
}  // namespace cyclestokens

namespace blender::io::usd {

/* Preview surface input specification. */
struct InputSpec {
  pxr::TfToken input_name;
  pxr::SdfValueTypeName input_type;
  pxr::TfToken source_name;
  /* Whether a default value should be set
   * if the node socket has not input. Usually
   * false for the Normal input. */
  bool set_default_value;
};

/* Map Blender socket names to USD Preview Surface InputSpec structs. */
typedef std::map<std::string, InputSpec> InputSpecMap;

/* Static function forward declarations. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     const char *name,
                                                     int type);
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     bNode *node);
static void create_uvmap_shader(const USDExporterContext &usd_export_context,
                                bNode *tex_node,
                                pxr::UsdShadeMaterial &usd_material,
                                pxr::UsdShadeShader &usd_tex_shader,
                                const pxr::TfToken &default_uv);
static void export_texture(bNode *node,
                           const pxr::UsdStageRefPtr stage,
                           const bool allow_overwrite = false);
static bNode *find_bsdf_node(Material *material);
static void get_absolute_path(Image *ima, char *r_path);
static std::string get_tex_image_asset_path(bNode *node,
                                            const pxr::UsdStageRefPtr stage,
                                            const USDExportParams &export_params);
static InputSpecMap &preview_surface_input_map();
static bNode *traverse_channel(bNodeSocket *input, short target_type);

template<typename T1, typename T2>
void create_input(pxr::UsdShadeShader &shader, const InputSpec &spec, const void *value);

void create_usd_preview_surface_material(const USDExporterContext &usd_export_context,
                                         Material *material,
                                         pxr::UsdShadeMaterial &usd_material,
                                         const std::string &default_uv)
{
  if (!material) {
    return;
  }

  /* Define a 'preview' scope beneath the material which will contain the preview shaders. */
  pxr::UsdGeomScope::Define(usd_export_context.stage,
                            usd_material.GetPath().AppendChild(usdtokens::preview));

  /* Default map when creating UV primvar reader shaders. */
  pxr::TfToken default_uv_sampler = default_uv.empty() ? cyclestokens::UVMap :
                                                         pxr::TfToken(default_uv);

  /* We only handle the first instance of either principled or
   * diffuse bsdf nodes in the material's node tree, because
   * USD Preview Surface has no concept of layering materials. */
  bNode *node = find_bsdf_node(material);
  if (!node) {
    return;
  }

  pxr::UsdShadeShader preview_surface = create_usd_preview_shader(
      usd_export_context, usd_material, node);

  const InputSpecMap &input_map = preview_surface_input_map();

  /* Set the preview surface inputs. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {

    /* Check if this socket is mapped to a USD preview shader input. */
    const InputSpecMap::const_iterator it = input_map.find(sock->name);

    if (it == input_map.end()) {
      continue;
    }

    pxr::UsdShadeShader created_shader;

    bNode *input_node = traverse_channel(sock, SH_NODE_TEX_IMAGE);

    const InputSpec &input_spec = it->second;

    if (input_node) {
      /* Create connection. */
      created_shader = create_usd_preview_shader(usd_export_context, usd_material, input_node);

      preview_surface.CreateInput(input_spec.input_name, input_spec.input_type)
          .ConnectToSource(created_shader, input_spec.source_name);
    }
    else if (input_spec.set_default_value) {
      /* Set hardcoded value. */
      switch (sock->type) {
        case SOCK_FLOAT: {
          create_input<bNodeSocketValueFloat, float>(
              preview_surface, input_spec, sock->default_value);
        } break;
        case SOCK_VECTOR: {
          create_input<bNodeSocketValueVector, pxr::GfVec3f>(
              preview_surface, input_spec, sock->default_value);
        } break;
        case SOCK_RGBA: {
          create_input<bNodeSocketValueRGBA, pxr::GfVec3f>(
              preview_surface, input_spec, sock->default_value);
        } break;
        default:
          break;
      }
    }

    /* If any input texture node has been found, export the texture, if necessary,
     * and look for a connected uv node. */
    if (!(created_shader && input_node && input_node->type == SH_NODE_TEX_IMAGE)) {
      continue;
    }

    if (usd_export_context.export_params.export_textures) {
      export_texture(input_node,
                     usd_export_context.stage,
                     usd_export_context.export_params.overwrite_textures);
    }

    create_uvmap_shader(
        usd_export_context, input_node, usd_material, created_shader, default_uv_sampler);
  }
}

void create_usd_viewport_material(const USDExporterContext &usd_export_context,
                                  Material *material,
                                  pxr::UsdShadeMaterial &usd_material)
{
  /* Construct the shader. */
  pxr::SdfPath shader_path = usd_material.GetPath().AppendChild(usdtokens::preview_shader);
  pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(usd_export_context.stage, shader_path);

  shader.CreateIdAttr(pxr::VtValue(usdtokens::preview_surface));
  shader.CreateInput(usdtokens::diffuse_color, pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(material->r, material->g, material->b));
  shader.CreateInput(usdtokens::roughness, pxr::SdfValueTypeNames->Float).Set(material->roughness);
  shader.CreateInput(usdtokens::metallic, pxr::SdfValueTypeNames->Float).Set(material->metallic);

  /* Connect the shader and the material together. */
  usd_material.CreateSurfaceOutput().ConnectToSource(shader, usdtokens::surface);
}

/* Return USD Preview Surface input map singleton. */
static InputSpecMap &preview_surface_input_map()
{
  static InputSpecMap input_map = {
      {"Base Color",
       {usdtokens::diffuse_color, pxr::SdfValueTypeNames->Float3, usdtokens::rgb, true}},
      {"Color", {usdtokens::diffuse_color, pxr::SdfValueTypeNames->Float3, usdtokens::rgb, true}},
      {"Roughness", {usdtokens::roughness, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      {"Metallic", {usdtokens::metallic, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      {"Specular", {usdtokens::specular, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      {"Alpha", {usdtokens::opacity, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      {"IOR", {usdtokens::ior, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      /* Note that for the Normal input set_default_value is false. */
      {"Normal", {usdtokens::normal, pxr::SdfValueTypeNames->Float3, usdtokens::rgb, false}},
      {"Clearcoat", {usdtokens::clearcoat, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
      {"Clearcoat Roughness",
       {usdtokens::clearcoatRoughness, pxr::SdfValueTypeNames->Float, usdtokens::r, true}},
  };

  return input_map;
}

/* Create an input on the given shader with name and type
 * provided by the InputSpec and assign the given value to the
 * input.  Parameters T1 and T2 indicate the Blender and USD
 * value types, respectively. */
template<typename T1, typename T2>
void create_input(pxr::UsdShadeShader &shader, const InputSpec &spec, const void *value)
{
  const T1 *cast_value = static_cast<const T1 *>(value);
  shader.CreateInput(spec.input_name, spec.input_type).Set(T2(cast_value->value));
}

/* Find the UVMAP node input to the given texture image node and convert it
 * to a USD primvar reader shader. If no UVMAP node is found, create a primvar
 * reader for the given default uv set.  The primvar reader will be attached to
 * the 'st' input of the given USD texture shader.  */
static void create_uvmap_shader(const USDExporterContext &usd_export_context,
                                bNode *tex_node,
                                pxr::UsdShadeMaterial &usd_material,
                                pxr::UsdShadeShader &usd_tex_shader,
                                const pxr::TfToken &default_uv)
{
  bool found_uv_node = false;

  /* Find UV input to the texture node. */
  LISTBASE_FOREACH (bNodeSocket *, tex_node_sock, &tex_node->inputs) {

    if (!tex_node_sock->link || !STREQ(tex_node_sock->name, "Vector")) {
      continue;
    }

    bNode *uv_node = traverse_channel(tex_node_sock, SH_NODE_UVMAP);
    if (uv_node == NULL) {
      continue;
    }

    pxr::UsdShadeShader uv_shader = create_usd_preview_shader(
        usd_export_context, usd_material, uv_node);

    if (!uv_shader.GetPrim().IsValid()) {
      continue;
    }

    found_uv_node = true;

    if (NodeShaderUVMap *shader_uv_map = static_cast<NodeShaderUVMap *>(uv_node->storage)) {
      /* We need to make valid here because actual uv primvar has been. */
      std::string uv_set = pxr::TfMakeValidIdentifier(shader_uv_map->uv_map);

      uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->Token)
          .Set(pxr::TfToken(uv_set));
      usd_tex_shader.CreateInput(usdtokens::st, pxr::SdfValueTypeNames->Float2)
          .ConnectToSource(uv_shader, usdtokens::result);
    }
    else {
      uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->Token).Set(default_uv);
      usd_tex_shader.CreateInput(usdtokens::st, pxr::SdfValueTypeNames->Float2)
          .ConnectToSource(uv_shader, usdtokens::result);
    }
  }

  if (!found_uv_node) {
    /* No UVMAP node was linked to the texture node. However, we generate
     * a primvar reader node that specifies the UV set to sample, as some
     * DCCs require this. */

    pxr::UsdShadeShader uv_shader = create_usd_preview_shader(
        usd_export_context, usd_material, "uvmap", SH_NODE_TEX_COORD);

    if (uv_shader.GetPrim().IsValid()) {
      uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->Token).Set(default_uv);
      usd_tex_shader.CreateInput(usdtokens::st, pxr::SdfValueTypeNames->Float2)
          .ConnectToSource(uv_shader, usdtokens::result);
    }
  }
}

/* Generate a file name for an in-memory image that doesn't have a
 * filepath already defined. */
static std::string get_in_memory_texture_filename(Image *ima)
{
  bool is_dirty = BKE_image_is_dirty(ima);
  bool is_generated = ima->source == IMA_SRC_GENERATED;
  bool is_packed = BKE_image_has_packedfile(ima);
  if (!(is_generated || is_dirty || is_packed)) {
    return "";
  }

  /* Determine the correct file extension from the image format. */
  ImBuf *imbuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
  if (!imbuf) {
    return "";
  }

  ImageFormatData imageFormat;
  BKE_imbuf_to_image_format(&imageFormat, imbuf);

  char file_name[FILE_MAX];
  /* Use the image name for the file name. */
  strcpy(file_name, ima->id.name + 2);

  BKE_image_path_ensure_ext_from_imformat(file_name, &imageFormat);

  return file_name;
}

static void export_in_memory_texture(Image *ima,
                                     const std::string &export_dir,
                                     const bool allow_overwrite)
{
  char image_abs_path[FILE_MAX];

  char file_name[FILE_MAX];
  if (strlen(ima->filepath) > 0) {
    get_absolute_path(ima, image_abs_path);
    BLI_split_file_part(image_abs_path, file_name, FILE_MAX);
  }
  else {
    /* Use the image name for the file name.  */
    strcpy(file_name, ima->id.name + 2);
  }

  ImBuf *imbuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
  if (!imbuf) {
    return;
  }

  ImageFormatData imageFormat;
  BKE_imbuf_to_image_format(&imageFormat, imbuf);

  /* This image in its current state only exists in Blender memory.
   * So we have to export it. The export will keep the image state intact,
   * so the exported file will not be associated with the image. */

  BKE_image_path_ensure_ext_from_imformat(file_name, &imageFormat);

  char export_path[FILE_MAX];
  BLI_path_join(export_path, FILE_MAX, export_dir.c_str(), file_name, NULL);

  if (!allow_overwrite && BLI_exists(export_path)) {
    return;
  }

  if ((BLI_path_cmp_normalized(export_path, image_abs_path) == 0) && BLI_exists(image_abs_path)) {
    /* As a precaution, don't overwrite the original path. */
    return;
  }

  std::cout << "Exporting in-memory texture to " << export_path << std::endl;

  if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == 0) {
    WM_reportf(RPT_WARNING, "USD export: couldn't export in-memory texture to %s", export_path);
  }
}

/* Get the absolute filepath of the given image.  Assumes
 * r_path result array is of length FILE_MAX. */
static void get_absolute_path(Image *ima, char *r_path)
{
  /* Make absolute source path. */
  BLI_strncpy(r_path, ima->filepath, FILE_MAX);
  BLI_path_abs(r_path, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));
  BLI_path_normalize(nullptr, r_path);
}

static pxr::TfToken get_node_tex_image_color_space(bNode *node)
{
  if (!node->id) {
    return pxr::TfToken();
  }

  Image *ima = reinterpret_cast<Image *>(node->id);

  if (strcmp(ima->colorspace_settings.name, "Raw") == 0) {
    return usdtokens::raw;
  }
  if (strcmp(ima->colorspace_settings.name, "Non-Color") == 0) {
    return usdtokens::raw;
  }
  if (strcmp(ima->colorspace_settings.name, "sRGB") == 0) {
    return usdtokens::sRGB;
  }

  return pxr::TfToken();
}

/* Search the upstream nodes connected to the given socket and return the first occurrence
 * of the node of the given type. Return null if no node of this type was found. */
static bNode *traverse_channel(bNodeSocket *input, const short target_type)
{
  if (!input->link) {
    return nullptr;
  }

  bNode *linked_node = input->link->fromnode;
  if (linked_node->type == target_type) {
    /* Return match. */
    return linked_node;
  }

  /* Recursively traverse the linked node's sockets. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &linked_node->inputs) {
    if (bNode *found_node = traverse_channel(sock, target_type)) {
      return found_node;
    }
  }

  return nullptr;
}

/* Returns the first occurrence of a principled BSDF or a diffuse BSDF node found in the given
 * material's node tree.  Returns null if no instance of either type was found.*/
static bNode *find_bsdf_node(Material *material)
{
  LISTBASE_FOREACH (bNode *, node, &material->nodetree->nodes) {
    if (node->type == SH_NODE_BSDF_PRINCIPLED || node->type == SH_NODE_BSDF_DIFFUSE) {
      return node;
    }
  }

  return nullptr;
}

/* Creates a USD Preview Surface shader based on the given cycles node name and type. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     const char *name,
                                                     const int type)
{
  pxr::SdfPath shader_path = material.GetPath()
                                 .AppendChild(usdtokens::preview)
                                 .AppendChild(pxr::TfToken(pxr::TfMakeValidIdentifier(name)));
  pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(usd_export_context.stage, shader_path);

  switch (type) {
    case SH_NODE_TEX_IMAGE: {
      shader.CreateIdAttr(pxr::VtValue(usdtokens::uv_texture));
      break;
    }
    case SH_NODE_TEX_COORD:
    case SH_NODE_UVMAP: {
      shader.CreateIdAttr(pxr::VtValue(usdtokens::primvar_float2));
      break;
    }
    case SH_NODE_BSDF_DIFFUSE:
    case SH_NODE_BSDF_PRINCIPLED: {
      shader.CreateIdAttr(pxr::VtValue(usdtokens::preview_surface));
      material.CreateSurfaceOutput().ConnectToSource(shader, usdtokens::surface);
      break;
    }

    default:
      break;
  }

  return shader;
}

/* Creates a USD Preview Surface shader based on the given cycles shading node. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     bNode *node)
{
  pxr::UsdShadeShader shader = create_usd_preview_shader(
      usd_export_context, material, node->name, node->type);

  if (node->type != SH_NODE_TEX_IMAGE) {
    return shader;
  }

  /* For texture image nodes we set the image path and color space. */
  std::string imagePath = get_tex_image_asset_path(
      node, usd_export_context.stage, usd_export_context.export_params);
  if (!imagePath.empty()) {
    shader.CreateInput(usdtokens::file, pxr::SdfValueTypeNames->Asset)
        .Set(pxr::SdfAssetPath(imagePath));
  }

  pxr::TfToken colorSpace = get_node_tex_image_color_space(node);
  if (!colorSpace.IsEmpty()) {
    shader.CreateInput(usdtokens::sourceColorSpace, pxr::SdfValueTypeNames->Token).Set(colorSpace);
  }

  return shader;
}

static std::string get_tex_image_asset_path(Image *ima)
{
  char filepath[FILE_MAX];
  get_absolute_path(ima, filepath);

  return std::string(filepath);
}

/* Gets an asset path for the given texture image node. The resulting path
 * may be absolute, relative to the USD file, or in a 'textures' directory
 * in the same directory as the USD file, depending on the export parameters.
 * The filename is typically the image filepath but might also be automatically
 * generated based on the image name for in-memory textures when exporting textures.
 * This function may return an empty string if the image does not have a filepath
 * assigned and no asset path could be determined. */
static std::string get_tex_image_asset_path(bNode *node,
                                            const pxr::UsdStageRefPtr stage,
                                            const USDExportParams &export_params)
{
  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return "";
  }

  std::string path;

  if (strlen(ima->filepath) > 0) {
    /* Get absolute path. */
    path = get_tex_image_asset_path(ima);
  }
  else if (export_params.export_textures) {
    /* Image has no filepath, but since we are exporting textures,
     * check if this is an in-memory texture for which we can
     * generate a file name. */
    path = get_in_memory_texture_filename(ima);
  }

  if (path.empty()) {
    return path;
  }

  if (export_params.export_textures) {
    /* The texture is exported to a 'textures' directory next to the
     * USD root layer. */

    char exp_path[FILE_MAX];
    char file_path[FILE_MAX];
    BLI_split_file_part(path.c_str(), file_path, FILE_MAX);

    if (export_params.relative_texture_paths) {
      BLI_path_join(exp_path, FILE_MAX, ".", "textures", file_path, NULL);
    }
    else {
      /* Create absolute path in the textures directory. */
      pxr::SdfLayerHandle layer = stage->GetRootLayer();
      std::string stage_path = layer->GetRealPath();
      if (stage_path.empty()) {
        return path;
      }

      char dir_path[FILE_MAX];
      BLI_split_dir_part(stage_path.c_str(), dir_path, FILE_MAX);
      BLI_path_join(exp_path, FILE_MAX, dir_path, "textures", file_path, NULL);
    }
    return exp_path;
  }

  if (export_params.relative_texture_paths) {
    /* Get the path relative to the USD. */
    pxr::SdfLayerHandle layer = stage->GetRootLayer();
    std::string stage_path = layer->GetRealPath();
    if (stage_path.empty()) {
      return path;
    }

    char rel_path[FILE_MAX];
    strcpy(rel_path, path.c_str());

    BLI_path_rel(rel_path, stage_path.c_str());

    /* BLI_path_rel adds '//' as a prefix to the path, if
     * generating the relative path was successful. */
    if (rel_path[0] != '/' || rel_path[1] != '/') {
      /* No relative path generated. */
      return path;
    }

    return rel_path + 2;
  }

  return path;
}

/* If the given image is tiled, copy the image tiles to the given
 * destination directory. */
static void copy_tiled_textures(Image *ima,
                                const std::string &dest_dir,
                                const bool allow_overwrite)
{
  char src_path[FILE_MAX];
  get_absolute_path(ima, src_path);

  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern = BKE_image_get_tile_strformat(src_path, &tile_format);

  /* Only <UDIM> tile formats are supported by USD right now. */
  if (tile_format != UDIM_TILE_FORMAT_UDIM) {
    std::cout << "WARNING: unsupported tile format for `" << src_path << "`" << std::endl;
    MEM_SAFE_FREE(udim_pattern);
    return;
  }

  /* Copy all tiles. */
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    char src_tile_path[FILE_MAX];
    BKE_image_set_filepath_from_tile_number(
        src_tile_path, udim_pattern, tile_format, tile->tile_number);

    char dest_filename[FILE_MAXFILE];
    BLI_split_file_part(src_tile_path, dest_filename, sizeof(dest_filename));

    char dest_tile_path[FILE_MAX];
    BLI_path_join(dest_tile_path, FILE_MAX, dest_dir.c_str(), dest_filename, nullptr);

    if (!allow_overwrite && BLI_exists(dest_tile_path)) {
      continue;
    }

    if (BLI_path_cmp_normalized(src_tile_path, dest_tile_path) == 0) {
      /* Source and destination paths are the same, don't copy. */
      continue;
    }

    std::cout << "Copying texture tile from " << src_tile_path << " to " << dest_tile_path
              << std::endl;

    /* Copy the file. */
    if (BLI_copy(src_tile_path, dest_tile_path) != 0) {
      WM_reportf(RPT_WARNING,
                 "USD export:  couldn't copy texture tile from %s to %s",
                 src_tile_path,
                 dest_tile_path);
    }
  }
  MEM_SAFE_FREE(udim_pattern);
}

/* Copy the given image to the destination directory. */
static void copy_single_file(Image *ima, const std::string &dest_dir, const bool allow_overwrite)
{
  char source_path[FILE_MAX];
  get_absolute_path(ima, source_path);

  char file_name[FILE_MAX];
  BLI_split_file_part(source_path, file_name, FILE_MAX);

  char dest_path[FILE_MAX];
  BLI_path_join(dest_path, FILE_MAX, dest_dir.c_str(), file_name, NULL);

  if (!allow_overwrite && BLI_exists(dest_path)) {
    return;
  }

  if (BLI_path_cmp_normalized(source_path, dest_path) == 0) {
    /* Source and destination paths are the same, don't copy. */
    return;
  }

  std::cout << "Copying texture from " << source_path << " to " << dest_path << std::endl;

  /* Copy the file. */
  if (BLI_copy(source_path, dest_path) != 0) {
    WM_reportf(
        RPT_WARNING, "USD export:  couldn't copy texture from %s to %s", source_path, dest_path);
  }
}

/* Export the given texture node's image to a 'textures' directory
 * next to given stage's root layer USD.
 * Based on ImagesExporter::export_UV_Image() */
static void export_texture(bNode *node,
                           const pxr::UsdStageRefPtr stage,
                           const bool allow_overwrite)
{
  if (node->type != SH_NODE_TEX_IMAGE && node->type != SH_NODE_TEX_ENVIRONMENT) {
    return;
  }

  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return;
  }

  pxr::SdfLayerHandle layer = stage->GetRootLayer();
  std::string stage_path = layer->GetRealPath();
  if (stage_path.empty()) {
    return;
  }

  char usd_dir_path[FILE_MAX];
  BLI_split_dir_part(stage_path.c_str(), usd_dir_path, FILE_MAX);

  char tex_dir_path[FILE_MAX];
  BLI_path_join(tex_dir_path, FILE_MAX, usd_dir_path, "textures", SEP_STR, NULL);

  BLI_dir_create_recursive(tex_dir_path);

  const bool is_dirty = BKE_image_is_dirty(ima);
  const bool is_generated = ima->source == IMA_SRC_GENERATED;
  const bool is_packed = BKE_image_has_packedfile(ima);

  std::string dest_dir(tex_dir_path);

  if (is_generated || is_dirty || is_packed) {
    export_in_memory_texture(ima, dest_dir, allow_overwrite);
  }
  else if (ima->source == IMA_SRC_TILED) {
    copy_tiled_textures(ima, dest_dir, allow_overwrite);
  }
  else {
    copy_single_file(ima, dest_dir, allow_overwrite);
  }
}

}  // namespace blender::io::usd
