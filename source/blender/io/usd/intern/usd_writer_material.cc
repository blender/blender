/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_material.h"

#include "usd.h"
#include "usd_asset_utils.h"
#include "usd_exporter_context.h"
#include "usd_hook.h"
#include "usd_umm.h"

#include "BKE_appdir.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_curve.hh"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.h"

#include "IMB_colormanagement.hh"

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_memory_utils.hh"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DNA_material_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/scope.h>

#include <cctype>
#include <iostream>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

/* `TfToken` objects are not cheap to construct, so we do it once. */
namespace usdtokens {
/* Materials. */
static const pxr::TfToken clearcoat("clearcoat", pxr::TfToken::Immortal);
static const pxr::TfToken clearcoatRoughness("clearcoatRoughness", pxr::TfToken::Immortal);
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken emissive_color("emissiveColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken UsdTransform2d("UsdTransform2d", pxr::TfToken::Immortal);
static const pxr::TfToken uv_texture("UsdUVTexture", pxr::TfToken::Immortal);
static const pxr::TfToken primvar_float2("UsdPrimvarReader_float2", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken specular("specular", pxr::TfToken::Immortal);
static const pxr::TfToken opacity("opacity", pxr::TfToken::Immortal);
static const pxr::TfToken opacityThreshold("opacityThreshold", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
static const pxr::TfToken perspective("perspective", pxr::TfToken::Immortal);
static const pxr::TfToken orthographic("orthographic", pxr::TfToken::Immortal);
static const pxr::TfToken rgb("rgb", pxr::TfToken::Immortal);
static const pxr::TfToken r("r", pxr::TfToken::Immortal);
static const pxr::TfToken g("g", pxr::TfToken::Immortal);
static const pxr::TfToken b("b", pxr::TfToken::Immortal);
static const pxr::TfToken a("a", pxr::TfToken::Immortal);
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken result("result", pxr::TfToken::Immortal);
static const pxr::TfToken varname("varname", pxr::TfToken::Immortal);
static const pxr::TfToken mdl("mdl", pxr::TfToken::Immortal);
static const pxr::TfToken out("out", pxr::TfToken::Immortal);
static const pxr::TfToken normal("normal", pxr::TfToken::Immortal);
static const pxr::TfToken ior("ior", pxr::TfToken::Immortal);
static const pxr::TfToken file("file", pxr::TfToken::Immortal);
static const pxr::TfToken raw("raw", pxr::TfToken::Immortal);
static const pxr::TfToken scale("scale", pxr::TfToken::Immortal);
static const pxr::TfToken bias("bias", pxr::TfToken::Immortal);
static const pxr::TfToken sRGB("sRGB", pxr::TfToken::Immortal);
static const pxr::TfToken sourceColorSpace("sourceColorSpace", pxr::TfToken::Immortal);
static const pxr::TfToken Shader("Shader", pxr::TfToken::Immortal);
static const pxr::TfToken black("black", pxr::TfToken::Immortal);
static const pxr::TfToken clamp("clamp", pxr::TfToken::Immortal);
static const pxr::TfToken repeat("repeat", pxr::TfToken::Immortal);
static const pxr::TfToken wrapS("wrapS", pxr::TfToken::Immortal);
static const pxr::TfToken wrapT("wrapT", pxr::TfToken::Immortal);
static const pxr::TfToken emissiveColor("emissiveColor", pxr::TfToken::Immortal);
static const pxr::TfToken in("in", pxr::TfToken::Immortal);
static const pxr::TfToken translation("translation", pxr::TfToken::Immortal);
static const pxr::TfToken rotation("rotation", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* Cycles specific tokens (Blender Importer and HdCycles) */
namespace cyclestokens {
static const pxr::TfToken cycles("cycles", pxr::TfToken::Immortal);
static const pxr::TfToken UVMap("UVMap", pxr::TfToken::Immortal);
static const pxr::TfToken filename("filename", pxr::TfToken::Immortal);
static const pxr::TfToken interpolation("interpolation", pxr::TfToken::Immortal);
static const pxr::TfToken projection("projection", pxr::TfToken::Immortal);
static const pxr::TfToken extension("extension", pxr::TfToken::Immortal);
static const pxr::TfToken colorspace("colorspace", pxr::TfToken::Immortal);
static const pxr::TfToken attribute("attribute", pxr::TfToken::Immortal);
static const pxr::TfToken bsdf("bsdf", pxr::TfToken::Immortal);
static const pxr::TfToken closure("closure", pxr::TfToken::Immortal);
static const pxr::TfToken vector("vector", pxr::TfToken::Immortal);
}  // namespace cyclestokens

namespace blender::io::usd {

/* Preview surface input specification. */
struct InputSpec {
  pxr::TfToken input_name;
  pxr::SdfValueTypeName input_type;
  /* Whether a default value should be set
   * if the node socket has not input. Usually
   * false for the Normal input. */
  bool set_default_value;
};

/* Map Blender socket names to USD Preview Surface InputSpec structs. */
using InputSpecMap = blender::Map<std::string, InputSpec>;

/* Static function forward declarations. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     const char *name,
                                                     int type);
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     pxr::UsdShadeMaterial &material,
                                                     bNode *node);
static void create_uv_input(const USDExporterContext &usd_export_context,
                            bNodeSocket *input_socket,
                            pxr::UsdShadeMaterial &usd_material,
                            pxr::UsdShadeInput &usd_input,
                            const pxr::TfToken &default_uv,
                            ReportList *reports);
static void create_uvmap_shader(const USDExporterContext &usd_export_context,
                                bNode *tex_node,
                                pxr::UsdShadeMaterial &usd_material,
                                pxr::UsdShadeShader &usd_tex_shader,
                                const pxr::TfToken &default_uv,
                                ReportList *reports);
static void export_texture(const USDExporterContext &usd_export_context, bNode *node);
static bNode *find_bsdf_node(Material *material);
static void get_absolute_path(Image *ima, char *r_path);
static std::string get_tex_image_asset_filepath(const USDExporterContext &usd_export_context,
                                                bNode *node);
static const InputSpecMap &preview_surface_input_map();
static bNodeLink *traverse_channel(bNodeSocket *input, short target_type);

void set_normal_texture_range(pxr::UsdShadeShader &usd_shader, const InputSpec &input_spec);

/* Create an input on the given shader with name and type
 * provided by the InputSpec and assign the given value to the
 * input.  Parameters T1 and T2 indicate the Blender and USD
 * value types, respectively. */
template<typename T1, typename T2>
void create_input(pxr::UsdShadeShader &shader,
                  const InputSpec &spec,
                  const void *value,
                  float scale)
{
  const T1 *cast_value = static_cast<const T1 *>(value);
  shader.CreateInput(spec.input_name, spec.input_type).Set(scale * T2(cast_value->value));
}

static void create_usd_preview_surface_material(const USDExporterContext &usd_export_context,
                                                Material *material,
                                                pxr::UsdShadeMaterial &usd_material,
                                                const std::string &default_uv,
                                                ReportList *reports)
{
  if (!material) {
    return;
  }

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
    const InputSpec *spec = input_map.lookup_ptr(sock->name);
    if (spec == nullptr) {
      continue;
    }

    /* Allow scaling inputs. */
    float scale = 1.0;

    const InputSpec &input_spec = *spec;
    bNodeLink *input_link = traverse_channel(sock, SH_NODE_TEX_IMAGE);

    if (input_spec.input_name == usdtokens::emissive_color) {
      /* Don't export emission color if strength is zero. */
      bNodeSocket *emission_strength_sock = nodeFindSocket(node, SOCK_IN, "Emission Strength");

      if (!emission_strength_sock) {
        continue;
      }

      scale = ((bNodeSocketValueFloat *)emission_strength_sock->default_value)->value;

      if (scale == 0.0f) {
        continue;
      }
    }

    if (input_link) {
      /* Convert the texture image node connected to this input. */
      bNode *input_node = input_link->fromnode;
      pxr::UsdShadeShader usd_shader = create_usd_preview_shader(
          usd_export_context, usd_material, input_node);

      /* Create the UsdUVTexture node output attribute that should be connected to this input. */
      pxr::TfToken source_name;
      if (input_spec.input_type == pxr::SdfValueTypeNames->Float) {
        /* If the input is a float, we connect it to either the texture alpha or red channels. */
        source_name = STREQ(input_link->fromsock->identifier, "Alpha") ? usdtokens::a :
                                                                         usdtokens::r;
        usd_shader.CreateOutput(source_name, pxr::SdfValueTypeNames->Float);
      }
      else {
        source_name = usdtokens::rgb;
        usd_shader.CreateOutput(usdtokens::rgb, pxr::SdfValueTypeNames->Float3);
      }

      /* Create the preview surface input and connect it to the shader. */
      pxr::UsdShadeConnectionSourceInfo source_info(
          usd_shader.ConnectableAPI(), source_name, pxr::UsdShadeAttributeType::Output);
      preview_surface.CreateInput(input_spec.input_name, input_spec.input_type)
          .ConnectToSource(source_info);

      set_normal_texture_range(usd_shader, input_spec);

      /* Export the texture, if necessary. */
      if (usd_export_context.export_params.export_textures) {
        export_texture(usd_export_context, input_node);
      }

      /* If a Vector Math node was detected ahead of the texture node, and it has
       * the correct type, NODE_VECTOR_MATH_MULTIPLY_ADD, assume it's meant to be
       * used for scale-bias. */
      bNodeLink *scale_link = traverse_channel(sock, SH_NODE_VECTOR_MATH);
      if (scale_link) {
        bNode *vector_math_node = scale_link->fromnode;
        if (vector_math_node->custom1 == NODE_VECTOR_MATH_MULTIPLY_ADD) {
          /* Attempt one more traversal in case the current node is not not the
           * correct NODE_VECTOR_MATH_MULTIPLY_ADD (see code in usd_reader_material). */
          bNodeSocket *sock_current = nodeFindSocket(vector_math_node, SOCK_IN, "Vector");
          bNodeLink *temp_link = traverse_channel(sock_current, SH_NODE_VECTOR_MATH);
          if (temp_link && temp_link->fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY_ADD) {
            vector_math_node = temp_link->fromnode;
          }

          bNodeSocket *sock_scale = nodeFindSocket(vector_math_node, SOCK_IN, "Vector_001");
          bNodeSocket *sock_bias = nodeFindSocket(vector_math_node, SOCK_IN, "Vector_002");
          const float *scale_value =
              static_cast<bNodeSocketValueVector *>(sock_scale->default_value)->value;
          const float *bias_value =
              static_cast<bNodeSocketValueVector *>(sock_bias->default_value)->value;

          const pxr::GfVec4f scale(scale_value[0], scale_value[1], scale_value[2], 1.0f);
          const pxr::GfVec4f bias(bias_value[0], bias_value[1], bias_value[2], 0.0f);

          pxr::UsdShadeInput scale_attr = usd_shader.GetInput(usdtokens::scale);
          if (!scale_attr) {
            scale_attr = usd_shader.CreateInput(usdtokens::scale, pxr::SdfValueTypeNames->Float4);
          }
          scale_attr.Set(scale);

          pxr::UsdShadeInput bias_attr = usd_shader.GetInput(usdtokens::bias);
          if (!bias_attr) {
            bias_attr = usd_shader.CreateInput(usdtokens::bias, pxr::SdfValueTypeNames->Float4);
          }
          bias_attr.Set(bias);
        }
      }

      /* Look for a connected uvmap node. */
      if (bNodeSocket *socket = nodeFindSocket(input_node, SOCK_IN, "Vector")) {
        if (pxr::UsdShadeInput st_input = usd_shader.CreateInput(usdtokens::st,
                                                                 pxr::SdfValueTypeNames->Float2))
        {
          create_uv_input(
              usd_export_context, socket, usd_material, st_input, default_uv_sampler, reports);
        }
      }

      /* Set opacityThreshold if an alpha cutout is used. */
      if ((input_spec.input_name == usdtokens::opacity) &&
          (material->blend_method == MA_BM_CLIP) && (material->alpha_threshold > 0.0))
      {
        pxr::UsdShadeInput opacity_threshold_input = preview_surface.CreateInput(
            usdtokens::opacityThreshold, pxr::SdfValueTypeNames->Float);
        opacity_threshold_input.GetAttr().Set(pxr::VtValue(material->alpha_threshold));
      }
    }
    else if (input_spec.set_default_value) {
      /* Set hardcoded value. */

      switch (sock->type) {
        case SOCK_FLOAT: {
          create_input<bNodeSocketValueFloat, float>(
              preview_surface, input_spec, sock->default_value, scale);
        } break;
        case SOCK_VECTOR: {
          create_input<bNodeSocketValueVector, pxr::GfVec3f>(
              preview_surface, input_spec, sock->default_value, scale);
        } break;
        case SOCK_RGBA: {
          create_input<bNodeSocketValueRGBA, pxr::GfVec3f>(
              preview_surface, input_spec, sock->default_value, scale);
        } break;
        default:
          break;
      }
    }
  }
}

void set_normal_texture_range(pxr::UsdShadeShader &usd_shader, const InputSpec &input_spec)
{
  /* Set the scale and bias for normal map textures
   * The USD spec requires them to be within the -1 to 1 space. */

  /* Only run if this input_spec is for a normal. */
  if (input_spec.input_name != usdtokens::normal) {
    return;
  }

  /* Make sure this is a texture shader prim. */
  pxr::TfToken shader_id;
  if (!usd_shader.GetIdAttr().Get(&shader_id) || shader_id != usdtokens::uv_texture) {
    return;
  }

  /* We should only be setting this if the colorspace is raw. sRGB will not map the same. */
  pxr::TfToken colorspace;
  auto colorspace_attr = usd_shader.GetInput(usdtokens::sourceColorSpace);
  if (!colorspace_attr || !colorspace_attr.Get(&colorspace) || colorspace != usdtokens::raw) {
    return;
  }

  /* Get or Create the scale attribute and set it. */
  auto scale_attr = usd_shader.GetInput(usdtokens::scale);
  if (!scale_attr) {
    scale_attr = usd_shader.CreateInput(usdtokens::scale, pxr::SdfValueTypeNames->Float4);
  }
  scale_attr.Set(pxr::GfVec4f(2.0f, 2.0f, 2.0f, 2.0f));

  /* Get or Create the bias attribute and set it. */
  auto bias_attr = usd_shader.GetInput(usdtokens::bias);
  if (!bias_attr) {
    bias_attr = usd_shader.CreateInput(usdtokens::bias, pxr::SdfValueTypeNames->Float4);
  }
  bias_attr.Set(pxr::GfVec4f(-1.0f, -1.0f, -1.0f, -1.0f));
}

/* Create USD Shade Material network from Blender viewport display settings. */
static void create_usd_viewport_material(const USDExporterContext &usd_export_context,
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
  usd_material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), usdtokens::surface);
}

/* Return USD Preview Surface input map singleton. */
static const InputSpecMap &preview_surface_input_map()
{
  static const InputSpecMap input_map = []() {
    InputSpecMap map;
    map.add_new("Base Color", {usdtokens::diffuse_color, pxr::SdfValueTypeNames->Color3f, true});
    map.add_new("Emission Color",
                {usdtokens::emissive_color, pxr::SdfValueTypeNames->Color3f, true});
    map.add_new("Color", {usdtokens::diffuse_color, pxr::SdfValueTypeNames->Color3f, true});
    map.add_new("Roughness", {usdtokens::roughness, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Metallic", {usdtokens::metallic, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Specular IOR Level", {usdtokens::specular, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Alpha", {usdtokens::opacity, pxr::SdfValueTypeNames->Float, true});
    map.add_new("IOR", {usdtokens::ior, pxr::SdfValueTypeNames->Float, true});

    /* Note that for the Normal input set_default_value is false. */
    map.add_new("Normal", {usdtokens::normal, pxr::SdfValueTypeNames->Float3, false});
    map.add_new("Coat Weight", {usdtokens::clearcoat, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Coat Roughness",
                {usdtokens::clearcoatRoughness, pxr::SdfValueTypeNames->Float, true});
    return map;
  }();

  return input_map;
}

/* Find the UVMAP node input to the given texture image node and convert it
 * to a USD primvar reader shader. If no UVMAP node is found, create a primvar
 * reader for the given default uv set. The primvar reader will be attached to
 * the 'st' input of the given USD texture shader. */
static void create_uvmap_shader(const USDExporterContext &usd_export_context,
                                bNodeLink *uvmap_link,
                                pxr::UsdShadeMaterial &usd_material,
                                pxr::UsdShadeInput &usd_input,
                                const pxr::TfToken &default_uv,
                                ReportList *reports)

{
  bNode *uv_node = (uvmap_link && uvmap_link->fromnode ? uvmap_link->fromnode : nullptr);

  BLI_assert(!uv_node || uv_node->type == SH_NODE_UVMAP);

  const char *shader_name = uv_node ? uv_node->name : "uvmap";

  pxr::UsdShadeShader uv_shader = create_usd_preview_shader(
      usd_export_context, usd_material, shader_name, SH_NODE_UVMAP);

  if (!uv_shader) {
    BKE_reportf(reports, RPT_WARNING, "%s: Couldn't create USD shader for UV map", __func__);
    return;
  }

  pxr::TfToken uv_name = default_uv;
  if (uv_node && uv_node->storage) {
    NodeShaderUVMap *shader_uv_map = static_cast<NodeShaderUVMap *>(uv_node->storage);
    /* We need to make valid here because actual uv primvar has been. */
    uv_name = pxr::TfToken(pxr::TfMakeValidIdentifier(shader_uv_map->uv_map));
  }

  if (usd_export_context.export_params.convert_uv_to_st && uv_name == default_uv) {
    uv_name = usdtokens::st;
  }

  uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->Token).Set(uv_name);
  usd_input.ConnectToSource(uv_shader.ConnectableAPI(), usdtokens::result);
}

static void create_transform2d_shader(const USDExporterContext &usd_export_context,
                                      bNodeLink *mapping_link,
                                      pxr::UsdShadeMaterial &usd_material,
                                      pxr::UsdShadeInput &usd_input,
                                      const pxr::TfToken &default_uv,
                                      ReportList *reports)
{
  bNode *mapping_node = (mapping_link && mapping_link->fromnode ? mapping_link->fromnode :
                                                                  nullptr);

  BLI_assert(mapping_node && mapping_node->type == SH_NODE_MAPPING);

  if (!mapping_node) {
    return;
  }

  if (mapping_node->custom1 != TEXMAP_TYPE_POINT) {
    if (bNodeSocket *socket = nodeFindSocket(mapping_node, SOCK_IN, "Vector")) {
      create_uv_input(usd_export_context, socket, usd_material, usd_input, default_uv, reports);
    }
    return;
  }

  pxr::UsdShadeShader transform2d_shader = create_usd_preview_shader(
      usd_export_context, usd_material, mapping_node);

  if (!transform2d_shader) {
    BKE_reportf(reports, RPT_WARNING, "%s: Couldn't create USD shader for mapping node", __func__);
    return;
  }

  usd_input.ConnectToSource(transform2d_shader.ConnectableAPI(), usdtokens::result);

  float scale[3] = {1.0f, 1.0f, 1.0f};
  float loc[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};

  if (bNodeSocket *scale_socket = nodeFindSocket(mapping_node, SOCK_IN, "Scale")) {
    copy_v3_v3(scale, ((bNodeSocketValueVector *)scale_socket->default_value)->value);
    /* Ignore the Z scale. */
    scale[2] = 1.0f;
  }

  if (bNodeSocket *loc_socket = nodeFindSocket(mapping_node, SOCK_IN, "Location")) {
    copy_v3_v3(loc, ((bNodeSocketValueVector *)loc_socket->default_value)->value);
    /* Ignore the Z translation. */
    loc[2] = 0.0f;
  }

  if (bNodeSocket *rot_socket = nodeFindSocket(mapping_node, SOCK_IN, "Rotation")) {
    copy_v3_v3(rot, ((bNodeSocketValueVector *)rot_socket->default_value)->value);
    /* Ignore the X and Y rotations. */
    rot[0] = 0.0f;
    rot[1] = 0.0f;
  }

  if (pxr::UsdShadeInput scale_input = transform2d_shader.CreateInput(
          usdtokens::scale, pxr::SdfValueTypeNames->Float2))
  {
    pxr::GfVec2f scale_val(scale[0], scale[1]);
    scale_input.Set(scale_val);
  }

  if (pxr::UsdShadeInput trans_input = transform2d_shader.CreateInput(
          usdtokens::translation, pxr::SdfValueTypeNames->Float2))
  {
    pxr::GfVec2f trans_val(loc[0], loc[1]);
    trans_input.Set(trans_val);
  }

  if (pxr::UsdShadeInput rot_input = transform2d_shader.CreateInput(usdtokens::rotation,
                                                                    pxr::SdfValueTypeNames->Float))
  {
    /* Convert to degrees. */
    float rot_val = rot[2] * 180.0f / M_PI;
    rot_input.Set(rot_val);
  }

  if (bNodeSocket *socket = nodeFindSocket(mapping_node, SOCK_IN, "Vector")) {
    if (pxr::UsdShadeInput in_input = transform2d_shader.CreateInput(
            usdtokens::in, pxr::SdfValueTypeNames->Float2))
    {
      create_uv_input(usd_export_context, socket, usd_material, in_input, default_uv, reports);
    }
  }
}

static void create_uv_input(const USDExporterContext &usd_export_context,
                            bNodeSocket *input_socket,
                            pxr::UsdShadeMaterial &usd_material,
                            pxr::UsdShadeInput &usd_input,
                            const pxr::TfToken &default_uv,
                            ReportList *reports)
{
  if (!(usd_material && usd_input)) {
    return;
  }

  if (bNodeLink *mapping_link = traverse_channel(input_socket, SH_NODE_MAPPING)) {
    create_transform2d_shader(
        usd_export_context, mapping_link, usd_material, usd_input, default_uv, reports);
    return;
  }

  bNodeLink *uvmap_link = traverse_channel(input_socket, SH_NODE_UVMAP);

  /* Note that uvmap_link might be null, but create_uv_shader() can handle this case. */
  create_uvmap_shader(
      usd_export_context, uvmap_link, usd_material, usd_input, default_uv, reports);
}

static bool is_in_memory_texture(Image *ima)
{
  return BKE_image_is_dirty(ima) || ima->source == IMA_SRC_GENERATED ||
         BKE_image_has_packedfile(ima);
}

/* Generate a file name for an in-memory image that doesn't have a
 * filepath already defined. */
static std::string get_in_memory_texture_filename(Image *ima)
{
  /* Determine the correct file extension from the image format. */
  ImBuf *imbuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
  if (!imbuf) {
    return "";
  }

  ImageFormatData imageFormat;
  BKE_image_format_from_imbuf(&imageFormat, imbuf);
  BKE_image_release_ibuf(ima, imbuf, nullptr);

  char file_name[FILE_MAX];
  if (strlen(ima->filepath) > 0) {
    BLI_path_split_file_part(ima->filepath, file_name, FILE_MAX);
  }
  else {
    /* Use the image name for the file name. */
    STRNCPY(file_name, ima->id.name + 2);
  }

  BKE_image_path_ext_from_imformat_ensure(file_name, sizeof(file_name), &imageFormat);

  return file_name;
}

static void export_in_memory_texture(Image *ima,
                                     const std::string &export_dir,
                                     const bool allow_overwrite,
                                     ReportList *reports)
{
  char image_abs_path[FILE_MAX];

  char file_name[FILE_MAX];
  if (strlen(ima->filepath) > 0) {
    get_absolute_path(ima, image_abs_path);
    BLI_path_split_file_part(image_abs_path, file_name, FILE_MAX);
  }
  else {
    /* Use the image name for the file name. */
    STRNCPY(file_name, ima->id.name + 2);
  }

  ImBuf *imbuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
  BLI_SCOPED_DEFER([&]() { BKE_image_release_ibuf(ima, imbuf, nullptr); });
  if (!imbuf) {
    return;
  }

  ImageFormatData imageFormat;
  BKE_image_format_from_imbuf(&imageFormat, imbuf);

  /* This image in its current state only exists in Blender memory.
   * So we have to export it. The export will keep the image state intact,
   * so the exported file will not be associated with the image. */

  BKE_image_path_ext_from_imformat_ensure(file_name, sizeof(file_name), &imageFormat);

  char export_path[FILE_MAX];
  BLI_path_join(export_path, FILE_MAX, export_dir.c_str(), file_name);
  BLI_string_replace_char(export_path, '\\', '/');

  if (!allow_overwrite && asset_exists(export_path)) {
    return;
  }

  if (paths_equal(export_path, image_abs_path) && asset_exists(image_abs_path)) {
    /* As a precaution, don't overwrite the original path. */
    return;
  }

  CLOG_INFO(&LOG, 2, "Exporting in-memory texture to '%s'", export_path);

  if (BLI_is_dir(export_dir.c_str())) {
    /* We are copying to a file system directory, so we can write the image buffer
     * directly to the destination. */
    if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == 0) {
      BKE_reportf(
          reports, RPT_WARNING, "USD export: couldn't save in-memory texture to %s", export_path);
    }
    return;
  }

  /* If we got here, the export directory path is not on the file system, which
   * would be the case if we the path is a URI.  We therefore can't save the image
   * directly (because BKE_imbuf_write_as() can't resolve URIs) and must save
   * the image to a temporary location on disk before copyig it to its final
   * destination. */

  char temp_filepath[FILE_MAX];
  BLI_path_join(temp_filepath, FILE_MAX, BKE_tempdir_session(), file_name);

  std::cout << "Saving in-memory texture to temporary location " << temp_filepath << std::endl;

  if (BKE_imbuf_write_as(imbuf, temp_filepath, &imageFormat, true) == 0) {
    WM_reportf(RPT_WARNING,
               "USD export: couldn't save in-memory texture to temporary location %s",
               temp_filepath);
  }

  /* Copy to destination. */
  if (!copy_asset(temp_filepath,
                  export_path,
                  allow_overwrite ? USD_TEX_NAME_COLLISION_OVERWRITE :
                                    USD_TEX_NAME_COLLISION_USE_EXISTING,
                  reports))
  {
    WM_reportf(RPT_WARNING, "USD export: couldn't export in-memory texture to %s", temp_filepath);
  }

  BLI_delete(temp_filepath, false, false);
}

/* Get the absolute filepath of the given image.  Assumes
 * r_path result array is of length FILE_MAX. */
static void get_absolute_path(Image *ima, char *r_path)
{
  /* Make absolute source path. */
  BLI_strncpy(r_path, ima->filepath, FILE_MAX);
  USD_path_abs(r_path, ID_BLEND_PATH_FROM_GLOBAL(&ima->id), false /* Not for import */);
}

/* ===== Functions copied from inacessible source file
 * blender/nodes/shader/node_shader_tree.c */

static void localize(bNodeTree *localtree, bNodeTree * /*ntree*/)
{
  bNode *node, *node_next;

  /* replace muted nodes and reroute nodes by internal links */
  for (node = (bNode *)localtree->nodes.first; node; node = node_next) {
    node_next = node->next;

    if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
      blender::bke::nodeInternalRelink(localtree, node);
      blender::bke::ntreeFreeLocalNode(localtree, node);
    }
  }
}

/* Find socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_socket(ListBase *sockets, const char *identifier)
{
  for (bNodeSocket *sock = (bNodeSocket *)sockets->first; sock != NULL; sock = sock->next) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return NULL;
}

/* Find output socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_output(bNode *node, const char *identifier)
{
  return ntree_shader_node_find_socket(&node->outputs, identifier);
}

/* Return true on success. */
static bool ntree_shader_expand_socket_default(bNodeTree *localtree,
                                               bNode *node,
                                               bNodeSocket *socket)
{
  bNode *value_node;
  bNodeSocket *value_socket;
  bNodeSocketValueVector *src_vector;
  bNodeSocketValueRGBA *src_rgba, *dst_rgba;
  bNodeSocketValueFloat *src_float, *dst_float;
  bNodeSocketValueInt *src_int;

  switch (socket->type) {
    case SOCK_VECTOR:
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != NULL);
      src_vector = (bNodeSocketValueVector *)socket->default_value;
      dst_rgba = (bNodeSocketValueRGBA *)value_socket->default_value;
      copy_v3_v3(dst_rgba->value, src_vector->value);
      dst_rgba->value[3] = 1.0f; /* should never be read */
      break;
    case SOCK_RGBA:
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != NULL);
      src_rgba = (bNodeSocketValueRGBA *)socket->default_value;
      dst_rgba = (bNodeSocketValueRGBA *)value_socket->default_value;
      copy_v4_v4(dst_rgba->value, src_rgba->value);
      break;
    case SOCK_INT:
      /* HACK: Support as float. */
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != NULL);
      src_int = (bNodeSocketValueInt *)socket->default_value;
      dst_float = (bNodeSocketValueFloat *)value_socket->default_value;
      dst_float->value = (float)(src_int->value);
      break;
    case SOCK_FLOAT:
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != NULL);
      src_float = (bNodeSocketValueFloat *)socket->default_value;
      dst_float = (bNodeSocketValueFloat *)value_socket->default_value;
      dst_float->value = src_float->value;
      break;
    default:
      return false;
  }
  nodeAddLink(localtree, value_node, value_socket, node, socket);
  return true;
}

static void ntree_shader_unlink_hidden_value_sockets(bNode *group_node, bNodeSocket *isock)
{
  bNodeTree *group_ntree = (bNodeTree *)group_node->id;
  bNode *node;
  bool removed_link = false;

  for (node = (bNode *)group_ntree->nodes.first; node; node = node->next) {
    for (bNodeSocket *sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
      if ((sock->flag & SOCK_HIDE_VALUE) == 0) {
        continue;
      }
      /* If socket is linked to a group input node and sockets id match. */
      if (sock && sock->link && sock->link->fromnode->type == NODE_GROUP_INPUT) {
        if (STREQ(isock->identifier, sock->link->fromsock->identifier)) {
          nodeRemLink(group_ntree, sock->link);
          removed_link = true;
        }
      }
    }
  }

  if (removed_link) {
    BKE_ntree_update_main_tree(G.main, group_ntree, nullptr);
  }
}

/* Node groups once expanded looses their input sockets values.
 * To fix this, link value/rgba nodes into the sockets and copy the group sockets values. */
static void ntree_shader_groups_expand_inputs(bNodeTree *localtree)
{
  bool link_added = false;

  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    const bool is_group = ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && (node->id != NULL);
    const bool is_group_output = node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT);

    if (is_group) {
      /* Do it recursively. */
      ntree_shader_groups_expand_inputs((bNodeTree *)node->id);
    }

    if (is_group || is_group_output) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (socket->link != NULL) {
          bNodeLink *link = socket->link;
          /* Fix the case where the socket is actually converting the data. (see T71374)
           * We only do the case of lossy conversion to float.*/
          if ((socket->type == SOCK_FLOAT) && (link->fromsock->type != link->tosock->type)) {
            bNode *tmp = nodeAddStaticNode(NULL, localtree, SH_NODE_RGBTOBW);
            nodeAddLink(
                localtree, link->fromnode, link->fromsock, tmp, (bNodeSocket *)tmp->inputs.first);
            nodeAddLink(localtree, tmp, (bNodeSocket *)tmp->outputs.first, node, socket);
          }
          continue;
        }

        if (is_group) {
          /* Detect the case where an input is plugged into a hidden value socket.
           * In this case we should just remove the link to trigger the socket default override. */
          ntree_shader_unlink_hidden_value_sockets(node, socket);
        }

        if (ntree_shader_expand_socket_default(localtree, node, socket)) {
          link_added = true;
        }
      }
    }
  }

  if (link_added) {
    BKE_ntree_update_main_tree(G.main, localtree, nullptr);
  }
}

static void flatten_group_do(bNodeTree *ntree, bNode *gnode)
{
  bNodeLink *link, *linkn, *tlink;
  bNode *node, *nextnode;
  bNodeTree *ngroup;
  LinkNode *group_interface_nodes = NULL;

  ngroup = (bNodeTree *)gnode->id;

  /* Add the nodes into the ntree */
  for (node = (bNode *)ngroup->nodes.first; node; node = nextnode) {
    nextnode = node->next;
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     * We must delay removal since sockets will reference this node. see: T52092 */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      BLI_linklist_prepend(&group_interface_nodes, node);
    }
    /* migrate node */
    BLI_remlink(&ngroup->nodes, node);
    BLI_addtail(&ntree->nodes, node);
    /* ensure unique node name in the node tree */
    /* This is very slow and it has no use for GPU nodetree. (see T70609) */
    nodeUniqueName(ntree, node);
  }

  /* Save first and last link to iterate over flattened group links. */
  bNodeLink *glinks_first = (bNodeLink *)ntree->links.last;

  /* Add internal links to the ntree */
  for (link = (bNodeLink *)ngroup->links.first; link; link = linkn) {
    linkn = link->next;
    BLI_remlink(&ngroup->links, link);
    BLI_addtail(&ntree->links, link);
  }

  bNodeLink *glinks_last = (bNodeLink *)ntree->links.last;

  /* restore external links to and from the gnode */
  if (glinks_first != NULL) {
    /* input links */
    for (link = (bNodeLink *)glinks_first->next; link != glinks_last->next; link = link->next) {
      if (link->fromnode->type == NODE_GROUP_INPUT) {
        const char *identifier = link->fromsock->identifier;
        /* find external links to this input */
        for (tlink = (bNodeLink *)ntree->links.first; tlink != glinks_first->next;
             tlink = tlink->next) {
          if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
            nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode, link->tosock);
          }
        }
      }
    }
    /* Also iterate over the new links to cover passthrough links. */
    glinks_last = (bNodeLink *)ntree->links.last;
    /* output links */
    for (tlink = (bNodeLink *)ntree->links.first; tlink != glinks_first->next; tlink = tlink->next)
    {
      if (tlink->fromnode == gnode) {
        const char *identifier = tlink->fromsock->identifier;
        /* find internal links to this output */
        for (link = glinks_first->next; link != glinks_last->next; link = link->next) {
          /* only use active output node */
          if (link->tonode->type == NODE_GROUP_OUTPUT && (link->tonode->flag & NODE_DO_OUTPUT)) {
            if (STREQ(link->tosock->identifier, identifier)) {
              nodeAddLink(ntree, link->fromnode, link->fromsock, tlink->tonode, tlink->tosock);
            }
          }
        }
      }
    }
  }

  while (group_interface_nodes) {
    node = (bNode *)BLI_linklist_pop(&group_interface_nodes);
    blender::bke::ntreeFreeLocalNode(ntree, node);
  }

  BKE_ntree_update_tag_all(ntree);
}

/* Flatten group to only have a simple single tree */
static void ntree_shader_groups_flatten(bNodeTree *localtree)
{
  /* This is effectively recursive as the flattened groups will add
   * nodes at the end of the list, which will also get evaluated. */
  for (bNode *node = (bNode *)localtree->nodes.first, *node_next; node; node = node_next) {
    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id != NULL) {
      flatten_group_do(localtree, node);
      /* Continue even on new flattened nodes. */
      node_next = node->next;
      /* delete the group instance and its localtree. */
      bNodeTree *ngroup = (bNodeTree *)node->id;
      blender::bke::ntreeFreeLocalNode(localtree, node);
      blender::bke::ntreeFreeTree(ngroup);
      MEM_freeN(ngroup);
    }
    else {
      node_next = node->next;
    }
  }

  BKE_ntree_update_main_tree(G.main, localtree, nullptr);
}

/* ===== USD/Blender Material Interchange ===== */

static const int HD_CYCLES_CURVE_EXPORT_RES = 256;

/**
 * We need to encode cycles shader node enums as strings.
 * There seems to be no way to get these directly from the Cycles
 * API. So we have to store these for now.
 * Update: /source/blender/makesrna/intern/rna_nodetree.c
 * this looks suspiciously like we could use this to avoid these maps
 *
 */

// This helper wraps the conversion maps and in case of future features, or missing map entries
// we encode the index. HdCycles can ingest enums as strings or integers. The trouble with ints
// is that the order of enums is different from Blender to Cycles. Aguably, adding this ingeger
// fallback will 'hide' missing future features, and 'may' work. However this code should be
// considered 'live' and require tweaking with each new version until we can share this conversion
// somehow. (Perhaps as mentioned above with rna_nodetree.c)
static bool usd_handle_shader_enum(pxr::TfToken a_token,
                                   const std::map<int, std::string> &a_conversion_table,
                                   pxr::UsdShadeShader &a_shader,
                                   const int a_value)
{
  std::map<int, std::string>::const_iterator it = a_conversion_table.find(a_value);
  if (it != a_conversion_table.end()) {
    a_shader.CreateInput(pxr::TfToken(a_token), pxr::SdfValueTypeNames->String).Set(it->second);
    return true;
  }
  else {
    a_shader.CreateInput(pxr::TfToken(a_token), pxr::SdfValueTypeNames->Int).Set(a_value);
  }
  return false;
}

static const std::map<int, std::string> node_noise_dimensions_conversion = {
    {1, "1D"},
    {2, "2D"},
    {3, "3D"},
    {4, "4D"},
};
static const std::map<int, std::string> node_voronoi_feature_conversion = {
    {SHD_VORONOI_F1, "f1"},
    {SHD_VORONOI_F2, "f2"},
    {SHD_VORONOI_SMOOTH_F1, "smooth_f1"},
    {SHD_VORONOI_DISTANCE_TO_EDGE, "distance_to_edge"},
    {SHD_VORONOI_N_SPHERE_RADIUS, "n_sphere_radius"},
};
static const std::map<int, std::string> node_voronoi_distance_conversion = {
    {SHD_VORONOI_EUCLIDEAN, "euclidean"},
    {SHD_VORONOI_MANHATTAN, "manhattan"},
    {SHD_VORONOI_CHEBYCHEV, "chebychev"},
    {SHD_VORONOI_MINKOWSKI, "minkowski"},
};
static const std::map<int, std::string> node_musgrave_type_conversion = {
    {SHD_MUSGRAVE_MULTIFRACTAL, "multifractal"},
    {SHD_MUSGRAVE_FBM, "fBM"},
    {SHD_MUSGRAVE_HYBRID_MULTIFRACTAL, "hybrid_multifractal"},
    {SHD_MUSGRAVE_RIDGED_MULTIFRACTAL, "ridged_multifractal"},
    {SHD_MUSGRAVE_HETERO_TERRAIN, "hetero_terrain"},
};
static const std::map<int, std::string> node_wave_type_conversion = {
    {SHD_WAVE_BANDS, "bands"},
    {SHD_WAVE_RINGS, "rings"},
};
static const std::map<int, std::string> node_wave_bands_direction_conversion = {
    {SHD_WAVE_BANDS_DIRECTION_X, "x"},
    {SHD_WAVE_BANDS_DIRECTION_Y, "y"},
    {SHD_WAVE_BANDS_DIRECTION_Z, "z"},
    {SHD_WAVE_BANDS_DIRECTION_DIAGONAL, "diagonal"},
};
static const std::map<int, std::string> node_wave_rings_direction_conversion = {
    {SHD_WAVE_RINGS_DIRECTION_X, "x"},
    {SHD_WAVE_RINGS_DIRECTION_Y, "y"},
    {SHD_WAVE_RINGS_DIRECTION_Z, "z"},
    {SHD_WAVE_RINGS_DIRECTION_SPHERICAL, "spherical"},
};
static const std::map<int, std::string> node_wave_profile_conversion = {
    {SHD_WAVE_PROFILE_SIN, "sine"},
    {SHD_WAVE_PROFILE_SAW, "saw"},
    {SHD_WAVE_PROFILE_TRI, "tri"},
};
static const std::map<int, std::string> node_point_density_space_conversion = {
    {SHD_POINTDENSITY_SPACE_OBJECT, "object"},
    {SHD_POINTDENSITY_SPACE_WORLD, "world"},
};
static const std::map<int, std::string> node_point_density_interpolation_conversion = {
    {SHD_INTERP_CLOSEST, "closest"},
    {SHD_INTERP_LINEAR, "linear"},
    {SHD_INTERP_CUBIC, "cubic"},
    {SHD_INTERP_SMART, "smart"},
};
static const std::map<int, std::string> node_mapping_type_conversion = {
    {NODE_MAPPING_TYPE_POINT, "point"},
    {NODE_MAPPING_TYPE_TEXTURE, "texture"},
    {NODE_MAPPING_TYPE_VECTOR, "vector"},
    {NODE_MAPPING_TYPE_NORMAL, "normal"},
};
// No defines exist for these, we create our own?
static const std::map<int, std::string> node_mix_rgb_type_conversion = {
    {0, "mix"},
    {1, "add"},
    {2, "multiply"},
    {3, "subtract"},
    {4, "screen"},
    {5, "divide"},
    {6, "difference"},
    {7, "darken"},
    {8, "lighten"},
    {9, "overlay"},
    {10, "dodge"},
    {11, "burn"},
    {12, "hue"},
    {13, "saturation"},
    {14, "value"},
    {15, "color"},
    {16, "soft_light"},
    {17, "linear_light"},
};
static const std::map<int, std::string> node_displacement_conversion = {
    {SHD_SPACE_TANGENT, "tangent"},
    {SHD_SPACE_OBJECT, "object"},
    {SHD_SPACE_WORLD, "world"},
    {SHD_SPACE_BLENDER_OBJECT, "blender_object"},
    {SHD_SPACE_BLENDER_WORLD, "blender_world"},
};
static const std::map<int, std::string> node_sss_falloff_conversion = {
#ifdef DNA_DEPRECATED_ALLOW
    {SHD_SUBSURFACE_CUBIC, "cubic"},
    {SHD_SUBSURFACE_GAUSSIAN, "gaussian"},
#endif
    {SHD_SUBSURFACE_BURLEY, "burley"},
    {SHD_SUBSURFACE_RANDOM_WALK, "random_walk"},
    {SHD_SUBSURFACE_RANDOM_WALK_SKIN, "random_walk"},
};
static const std::map<int, std::string> node_principled_hair_parametrization_conversion = {
    {SHD_PRINCIPLED_HAIR_REFLECTANCE, "Direct coloring"},
    {SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION, "Melanin concentration"},
    {SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION, "Absorption coefficient"},
};
static const std::map<int, std::string> node_clamp_type_conversion = {
    {NODE_CLAMP_MINMAX, "minmax"},
    {NODE_CLAMP_RANGE, "range"},
};
static const std::map<int, std::string> node_math_type_conversion = {
    {NODE_MATH_ADD, "add"},
    {NODE_MATH_SUBTRACT, "subtract"},
    {NODE_MATH_MULTIPLY, "multiply"},
    {NODE_MATH_DIVIDE, "divide"},
    {NODE_MATH_MULTIPLY_ADD, "multiply_add"},
    {NODE_MATH_SINE, "sine"},
    {NODE_MATH_COSINE, "cosine"},
    {NODE_MATH_TANGENT, "tangent"},
    {NODE_MATH_SINH, "sinh"},
    {NODE_MATH_COSH, "cosh"},
    {NODE_MATH_TANH, "tanh"},
    {NODE_MATH_ARCSINE, "arcsine"},
    {NODE_MATH_ARCCOSINE, "arccosine"},
    {NODE_MATH_ARCTANGENT, "arctangent"},
    {NODE_MATH_POWER, "power"},
    {NODE_MATH_LOGARITHM, "logarithm"},
    {NODE_MATH_MINIMUM, "minimum"},
    {NODE_MATH_MAXIMUM, "maximum"},
    {NODE_MATH_ROUND, "round"},
    {NODE_MATH_LESS_THAN, "less_than"},
    {NODE_MATH_GREATER_THAN, "greater_than"},
    {NODE_MATH_MODULO, "modulo"},
    {NODE_MATH_ABSOLUTE, "absolute"},
    {NODE_MATH_ARCTAN2, "arctan2"},
    {NODE_MATH_FLOOR, "floor"},
    {NODE_MATH_CEIL, "ceil"},
    {NODE_MATH_FRACTION, "fraction"},
    {NODE_MATH_TRUNC, "trunc"},
    {NODE_MATH_SNAP, "snap"},
    {NODE_MATH_WRAP, "wrap"},
    {NODE_MATH_PINGPONG, "pingpong"},
    {NODE_MATH_SQRT, "sqrt"},
    {NODE_MATH_INV_SQRT, "inversesqrt"},
    {NODE_MATH_SIGN, "sign"},
    {NODE_MATH_EXPONENT, "exponent"},
    {NODE_MATH_RADIANS, "radians"},
    {NODE_MATH_DEGREES, "degrees"},
    {NODE_MATH_SMOOTH_MIN, "smoothmin"},
    {NODE_MATH_SMOOTH_MAX, "smoothmax"},
    {NODE_MATH_COMPARE, "compare"},
};
static const std::map<int, std::string> node_vector_math_type_conversion = {
    {NODE_VECTOR_MATH_ADD, "add"},
    {NODE_VECTOR_MATH_SUBTRACT, "subtract"},
    {NODE_VECTOR_MATH_MULTIPLY, "multiply"},
    {NODE_VECTOR_MATH_DIVIDE, "divide"},

    {NODE_VECTOR_MATH_CROSS_PRODUCT, "cross_product"},
    {NODE_VECTOR_MATH_PROJECT, "project"},
    {NODE_VECTOR_MATH_REFLECT, "reflect"},
    {NODE_VECTOR_MATH_DOT_PRODUCT, "dot_product"},

    {NODE_VECTOR_MATH_DISTANCE, "distance"},
    {NODE_VECTOR_MATH_LENGTH, "length"},
    {NODE_VECTOR_MATH_SCALE, "scale"},
    {NODE_VECTOR_MATH_NORMALIZE, "normalize"},

    {NODE_VECTOR_MATH_SNAP, "snap"},
    {NODE_VECTOR_MATH_FLOOR, "floor"},
    {NODE_VECTOR_MATH_CEIL, "ceil"},
    {NODE_VECTOR_MATH_MODULO, "modulo"},
    {NODE_VECTOR_MATH_FRACTION, "fraction"},
    {NODE_VECTOR_MATH_ABSOLUTE, "absolute"},
    {NODE_VECTOR_MATH_MINIMUM, "minimum"},
    {NODE_VECTOR_MATH_MAXIMUM, "maximum"},
    {NODE_VECTOR_MATH_WRAP, "wrap"},
    {NODE_VECTOR_MATH_SINE, "sine"},
    {NODE_VECTOR_MATH_COSINE, "cosine"},
    {NODE_VECTOR_MATH_TANGENT, "tangent"},
};
static const std::map<int, std::string> node_vector_rotate_type_conversion = {
    {NODE_VECTOR_ROTATE_TYPE_AXIS, "axis"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_X, "x_axis"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_Y, "y_axis"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_Z, "z_axis"},
    {NODE_VECTOR_ROTATE_TYPE_EULER_XYZ, "euler_xyz"},
};
static const std::map<int, std::string> node_vector_transform_type_conversion = {
    {SHD_VECT_TRANSFORM_TYPE_VECTOR, "vector"},
    {SHD_VECT_TRANSFORM_TYPE_POINT, "point"},
    {SHD_VECT_TRANSFORM_TYPE_NORMAL, "normal"},
};
static const std::map<int, std::string> node_vector_transform_space_conversion = {
    {SHD_VECT_TRANSFORM_SPACE_WORLD, "world"},
    {SHD_VECT_TRANSFORM_SPACE_OBJECT, "object"},
    {SHD_VECT_TRANSFORM_SPACE_CAMERA, "camera"},
};
static const std::map<int, std::string> node_normal_map_space_conversion = {
    {SHD_SPACE_TANGENT, "tangent"},
    {SHD_SPACE_OBJECT, "object"},
    {SHD_SPACE_WORLD, "world"},
    {SHD_SPACE_BLENDER_OBJECT, "blender_object"},
    {SHD_SPACE_BLENDER_WORLD, "blender_world"},
};
static const std::map<int, std::string> node_tangent_direction_type_conversion = {
    {SHD_TANGENT_RADIAL, "radial"},
    {SHD_TANGENT_UVMAP, "uv_map"},
};
static const std::map<int, std::string> node_tangent_axis_conversion = {
    {SHD_TANGENT_AXIS_X, "x"},
    {SHD_TANGENT_AXIS_Y, "y"},
    {SHD_TANGENT_AXIS_Z, "z"},
};
static const std::map<int, std::string> node_image_tex_alpha_type_conversion = {
    {IMA_ALPHA_STRAIGHT, "unassociated"},
    {IMA_ALPHA_PREMUL, "associated"},
    {IMA_ALPHA_CHANNEL_PACKED, "channel_packed"},
    {IMA_ALPHA_IGNORE, "ignore"},
    //{IMAGE_ALPHA_AUTO, "auto"},
};
static const std::map<int, std::string> node_image_tex_interpolation_conversion = {
    {SHD_INTERP_CLOSEST, "closest"},
    {SHD_INTERP_LINEAR, "linear"},
    {SHD_INTERP_CUBIC, "cubic"},
    {SHD_INTERP_SMART, "smart"},
};
static const std::map<int, std::string> node_image_tex_extension_conversion = {
    {SHD_IMAGE_EXTENSION_REPEAT, "periodic"},
    {SHD_IMAGE_EXTENSION_EXTEND, "clamp"},
    {SHD_IMAGE_EXTENSION_CLIP, "black"},
};
static const std::map<int, std::string> node_image_tex_projection_conversion = {
    {SHD_PROJ_FLAT, "flat"},
    {SHD_PROJ_BOX, "box"},
    {SHD_PROJ_SPHERE, "sphere"},
    {SHD_PROJ_TUBE, "tube"},
};
static const std::map<int, std::string> node_env_tex_projection_conversion = {
    {SHD_PROJ_EQUIRECTANGULAR, "equirectangular"},
    {SHD_PROJ_MIRROR_BALL, "mirror_ball"},
};
// TODO: 2.90 introduced enums
/*static const std::map<int, std::string> node_sky_tex_type_conversion = {
    {SHD_SKY_PREETHAM, "preetham"},
    {SHD_SKY_HOSEK, "hosek_wilkie"},
    {SHD_SKY_NISHITA, "nishita_improved"},
};*/
static const std::map<int, std::string> node_sky_tex_type_conversion = {
    {0, "preetham"},
    {1, "hosek_wilkie"},
    {2, "nishita_improved"},
};

// END TODO
static const std::map<int, std::string> node_gradient_tex_type_conversion = {
    {SHD_BLEND_LINEAR, "linear"},
    {SHD_BLEND_LINEAR, "quadratic"},
    {SHD_BLEND_EASING, "easing"},
    {SHD_BLEND_DIAGONAL, "diagonal"},
    {SHD_BLEND_RADIAL, "radial"},
    {SHD_BLEND_QUADRATIC_SPHERE, "quadratic_sphere"},
    {SHD_BLEND_SPHERICAL, "spherical"},
};
static const std::map<int, std::string> node_glossy_distribution_conversion = {
    {SHD_GLOSSY_SHARP_DEPRECATED, "sharp"},
    {SHD_GLOSSY_BECKMANN, "beckmann"},
    {SHD_GLOSSY_GGX, "GGX"},
    {SHD_GLOSSY_ASHIKHMIN_SHIRLEY, "ashikhmin_shirley"},
    {SHD_GLOSSY_MULTI_GGX, "Multiscatter GGX"},
};
static const std::map<int, std::string> node_anisotropic_distribution_conversion = {
    {SHD_GLOSSY_BECKMANN, "beckmann"},
    {SHD_GLOSSY_GGX, "GGX"},
    {SHD_GLOSSY_MULTI_GGX, "Multiscatter GGX"},
    {SHD_GLOSSY_ASHIKHMIN_SHIRLEY, "ashikhmin_shirley"},
};
static const std::map<int, std::string> node_glass_distribution_conversion = {
    {SHD_GLOSSY_SHARP_DEPRECATED, "sharp"},
    {SHD_GLOSSY_BECKMANN, "beckmann"},
    {SHD_GLOSSY_GGX, "GGX"},
    {SHD_GLOSSY_MULTI_GGX, "Multiscatter GGX"},
};
static const std::map<int, std::string> node_refraction_distribution_conversion = {
    {SHD_GLOSSY_SHARP_DEPRECATED, "sharp"},
    {SHD_GLOSSY_BECKMANN, "beckmann"},
    {SHD_GLOSSY_GGX, "GGX"},
};
static const std::map<int, std::string> node_toon_component_conversion = {
    {SHD_TOON_DIFFUSE, "diffuse"},
    {SHD_TOON_GLOSSY, "glossy"},
};
static const std::map<int, std::string> node_hair_component_conversion = {
    {SHD_HAIR_REFLECTION, "reflection"},
    {SHD_HAIR_TRANSMISSION, "transmission"},
};

static const std::map<int, std::string> node_principled_distribution_conversion = {
    {SHD_GLOSSY_GGX, "GGX"},
    {SHD_GLOSSY_MULTI_GGX, "Multiscatter GGX"},
};
static const std::map<int, std::string> node_principled_subsurface_method_conversion = {
    {SHD_SUBSURFACE_BURLEY, "burley"},
    {SHD_SUBSURFACE_RANDOM_WALK, "random_walk"},
};

static void to_lower(std::string &string)
{
  std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
}

static void set_default(bNode *node,
                        bNodeSocket *socketValue,
                        bNodeSocket *socketName,
                        pxr::UsdShadeShader usd_shader)
{
  std::string inputName = socketName->identifier;

  switch (node->type) {
    case SH_NODE_MATH: {
      if (inputName == "Value_001")
        inputName = "Value2";
      else
        inputName = "Value1";
    } break;
    case SH_NODE_VECTOR_MATH: {
      if (inputName == "Vector_001")
        inputName = "Vector2";
      else if (inputName == "Vector_002")
        inputName = "Vector3";
      else
        inputName = "Vector1";
    } break;
    case SH_NODE_SEPRGB_LEGACY: {
      if (inputName == "Image")
        inputName = "color";
    } break;
  }

  to_lower(inputName);

  pxr::TfToken sock_in = pxr::TfToken(pxr::TfMakeValidIdentifier(inputName));
  switch (socketValue->type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *float_data = (bNodeSocketValueFloat *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Float)
          .Set(pxr::VtValue(float_data->value));
      break;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *vector_data = (bNodeSocketValueVector *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Float3)
          .Set(pxr::GfVec3f(vector_data->value[0], vector_data->value[1], vector_data->value[2]));
      break;
    }
    case SOCK_RGBA: {
      bNodeSocketValueRGBA *rgba_data = (bNodeSocketValueRGBA *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Float4)
          .Set(pxr::GfVec4f(
              rgba_data->value[0], rgba_data->value[1], rgba_data->value[2], rgba_data->value[2]));
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *bool_data = (bNodeSocketValueBoolean *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Bool)
          .Set(pxr::VtValue(bool_data->value));
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *int_data = (bNodeSocketValueInt *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Int)
          .Set(pxr::VtValue(int_data->value));
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *string_data = (bNodeSocketValueString *)socketValue->default_value;
      usd_shader.CreateInput(sock_in, pxr::SdfValueTypeNames->Token)
          .Set(pxr::TfToken(pxr::TfMakeValidIdentifier(string_data->value)));
      break;
    }
    default:
      // Unsupported data type
      break;
  }
}

/* Creates a USDShadeShader based on given cycles shading node */
static pxr::UsdShadeShader create_cycles_shader_node(pxr::UsdStageRefPtr a_stage,
                                                     pxr::SdfPath &shaderPath,
                                                     bNode *node,
                                                     const USDExportParams &export_params)
{
  pxr::SdfPath primpath = shaderPath.AppendChild(
      pxr::TfToken(pxr::TfMakeValidIdentifier(node->name)));

  // Early out if already created
  if (a_stage->GetPrimAtPath(primpath).IsValid())
    return pxr::UsdShadeShader::Get(a_stage, primpath);

  pxr::UsdShadeShader shader = (export_params.export_as_overs) ?
                                   pxr::UsdShadeShader(a_stage->OverridePrim(primpath)) :
                                   pxr::UsdShadeShader::Define(a_stage, primpath);

  // Author Cycles Shader Node ID
  // For now we convert spaces to _ and transform to lowercase.
  // This isn't a 1:1 gaurantee it will be in the format for cycles standalone.
  // e.g. Blender: ShaderNodeBsdfPrincipled. Cycles_principled_bsdf
  // But works for now. We should also author idname to easier import directly
  // to Blender.
  bNodeType *ntype = node->typeinfo;
  std::string usd_shade_type_name(ntype->ui_name);
  to_lower(usd_shade_type_name);

  // TODO Move this to a more generic conversion map?
  if (usd_shade_type_name == "rgb")
    usd_shade_type_name = "color";
  if (node->type == SH_NODE_MIX_SHADER)
    usd_shade_type_name = "mix_closure";
  if (node->type == SH_NODE_ADD_SHADER)
    usd_shade_type_name = "add_closure";
  if (node->type == SH_NODE_OUTPUT_MATERIAL)
    usd_shade_type_name = "output";
  if (node->type == SH_NODE_OUTPUT_WORLD)
    usd_shade_type_name = "output";
  if (node->type == SH_NODE_OUTPUT_LIGHT)
    usd_shade_type_name = "output";
  if (node->type == SH_NODE_UVMAP)
    usd_shade_type_name = "uvmap";
  if (node->type == SH_NODE_VALTORGB)
    usd_shade_type_name = "rgb_ramp";
  if (node->type == SH_NODE_HUE_SAT)
    usd_shade_type_name = "hsv";
  if (node->type == SH_NODE_BRIGHTCONTRAST)
    usd_shade_type_name = "brightness_contrast";
  if (node->type == SH_NODE_BACKGROUND)
    usd_shade_type_name = "background_shader";
  if (node->type == SH_NODE_VOLUME_SCATTER)
    usd_shade_type_name = "scatter_volume";
  if (node->type == SH_NODE_VOLUME_ABSORPTION)
    usd_shade_type_name = "absorption_volume";

  shader.CreateIdAttr(
      pxr::VtValue(pxr::TfToken("cycles_" + pxr::TfMakeValidIdentifier(usd_shade_type_name))));

  // Store custom1-4

  switch (node->type) {
    case SH_NODE_TEX_WHITE_NOISE: {
      usd_handle_shader_enum(pxr::TfToken("Dimensions"),
                             node_noise_dimensions_conversion,
                             shader,
                             (int)node->custom1);
    } break;
    case SH_NODE_MATH: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_math_type_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_VECTOR_MATH: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_vector_math_type_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_MAPPING: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_mapping_type_conversion, shader, (int)node->custom1);
    } break;
    /* TODO(makowalski): find replacement for the following legacy node. */
    case SH_NODE_MIX_RGB_LEGACY: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_mix_rgb_type_conversion, shader, (int)node->custom1);
      shader.CreateInput(pxr::TfToken("Use_Clamp"), pxr::SdfValueTypeNames->Bool)
          .Set(static_cast<bool>(node->custom1 & SHD_MIXRGB_CLAMP));
    } break;
    case SH_NODE_VECTOR_DISPLACEMENT: {
      usd_handle_shader_enum(
          pxr::TfToken("Space"), node_displacement_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_VECTOR_ROTATE: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_vector_rotate_type_conversion, shader, (int)node->custom1);
      shader.CreateInput(pxr::TfToken("Invert"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom2);
    } break;
    case SH_NODE_VECT_TRANSFORM: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_vector_transform_type_conversion, shader, (int)node->custom1);
      usd_handle_shader_enum(pxr::TfToken("Space"),
                             node_vector_transform_space_conversion,
                             shader,
                             (int)node->custom2);
    } break;
    case SH_NODE_SUBSURFACE_SCATTERING: {
      usd_handle_shader_enum(
          pxr::TfToken("Falloff"), node_sss_falloff_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_CLAMP: {
      usd_handle_shader_enum(
          pxr::TfToken("Type"), node_clamp_type_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_WIREFRAME: {
      shader.CreateInput(pxr::TfToken("Use_Pixel_Size"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom1);
    } break;
    case SH_NODE_BSDF_GLOSSY: {
      // Cycles Standalone uses a different enum for distribution and subsurface, we encode strings
      // instead
      usd_handle_shader_enum(pxr::TfToken("Distribution"),
                             node_glossy_distribution_conversion,
                             shader,
                             (int)node->custom1);
    } break;
    case SH_NODE_BSDF_REFRACTION: {
      // Cycles Standalone uses a different enum for distribution and subsurface, we encode strings
      // instead
      usd_handle_shader_enum(pxr::TfToken("Distribution"),
                             node_refraction_distribution_conversion,
                             shader,
                             (int)node->custom1);
    } break;
    case SH_NODE_BSDF_TOON: {
      usd_handle_shader_enum(
          pxr::TfToken("component"), node_toon_component_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_DISPLACEMENT: {
      usd_handle_shader_enum(
          pxr::TfToken("Space"), node_displacement_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_BSDF_HAIR: {
      usd_handle_shader_enum(
          pxr::TfToken("component"), node_hair_component_conversion, shader, (int)node->custom1);
    } break;
    case SH_NODE_BSDF_HAIR_PRINCIPLED: {
      usd_handle_shader_enum(pxr::TfToken("parametrization"),
                             node_principled_hair_parametrization_conversion,
                             shader,
                             (int)node->custom1);
    } break;
    case SH_NODE_MAP_RANGE: {
      shader.CreateInput(pxr::TfToken("Use_Clamp"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom1);
      shader.CreateInput(pxr::TfToken("Type"), pxr::SdfValueTypeNames->Int)
          .Set((int)node->custom2);
    } break;
    case SH_NODE_BEVEL: {
      shader.CreateInput(pxr::TfToken("Samples"), pxr::SdfValueTypeNames->Int)
          .Set((int)node->custom1);
    } break;
    case SH_NODE_AMBIENT_OCCLUSION: {
      shader.CreateInput(pxr::TfToken("Samples"), pxr::SdfValueTypeNames->Int)
          .Set((int)node->custom1);
      // TODO: Format?
      shader.CreateInput(pxr::TfToken("Inside"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom2);
      shader.CreateInput(pxr::TfToken("Only_Local"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom3);
    } break;
    case SH_NODE_BSDF_GLASS: {
      // Cycles Standalone uses a different enum for distribution and subsurface, we encode strings
      // instead
      usd_handle_shader_enum(pxr::TfToken("Distribution"),
                             node_glass_distribution_conversion,
                             shader,
                             (int)node->custom1);
    } break;
    case SH_NODE_BUMP: {
      shader.CreateInput(pxr::TfToken("Invert"), pxr::SdfValueTypeNames->Bool)
          .Set((bool)node->custom1);
    } break;
    case SH_NODE_BSDF_PRINCIPLED: {
      // Cycles Standalone uses a different enum for distribution and subsurface, we encode strings
      // instead

      // Commenting out unused to prevent compiler warning.
      // int distribution = ((node->custom1) & 6);

      usd_handle_shader_enum(pxr::TfToken("Distribution"),
                             node_principled_distribution_conversion,
                             shader,
                             (int)node->custom1);
      usd_handle_shader_enum(pxr::TfToken("Subsurface_Method"),
                             node_principled_subsurface_method_conversion,
                             shader,
                             (int)node->custom2);

      // Removed in 2.82+?
      bool sss_diffuse_blend_get = (((node->custom1) & 8) != 0);
      shader.CreateInput(pxr::TfToken("Blend_SSS_Diffuse"), pxr::SdfValueTypeNames->Bool)
          .Set(sss_diffuse_blend_get);
    } break;
  }

  // Convert all internal storage
  switch (node->type) {

      // -- Texture Node Storage

    case SH_NODE_TEX_SKY: {
      NodeTexSky *sky_storage = (NodeTexSky *)node->storage;
      if (!sky_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      usd_handle_shader_enum(
          pxr::TfToken("type"), node_sky_tex_type_conversion, shader, sky_storage->sky_model);
      shader.CreateInput(pxr::TfToken("sun_direction"), pxr::SdfValueTypeNames->Vector3f)
          .Set(pxr::GfVec3f(sky_storage->sun_direction[0],
                            sky_storage->sun_direction[1],
                            sky_storage->sun_direction[2]));
      shader.CreateInput(pxr::TfToken("turbidity"), pxr::SdfValueTypeNames->Float)
          .Set(sky_storage->turbidity);
      shader.CreateInput(pxr::TfToken("ground_albedo"), pxr::SdfValueTypeNames->Float)
          .Set(sky_storage->ground_albedo);
    } break;

    case SH_NODE_TEX_IMAGE: {
      NodeTexImage *tex_original = (NodeTexImage *)node->storage;
      if (!tex_original)
        break;
      std::string imagePath = get_tex_image_asset_filepath(node, a_stage, export_params);
      if (imagePath.size() > 0)
        shader.CreateInput(cyclestokens::filename, pxr::SdfValueTypeNames->Asset)
            .Set(pxr::SdfAssetPath(imagePath));

      usd_handle_shader_enum(cyclestokens::interpolation,
                             node_image_tex_interpolation_conversion,
                             shader,
                             tex_original->interpolation);
      usd_handle_shader_enum(cyclestokens::projection,
                             node_image_tex_projection_conversion,
                             shader,
                             tex_original->projection);
      usd_handle_shader_enum(cyclestokens::extension,
                             node_image_tex_extension_conversion,
                             shader,
                             tex_original->extension);

      if (node->id) {
        Image *ima = (Image *)node->id;
        usd_handle_shader_enum(pxr::TfToken("alpha_type"),
                               node_image_tex_alpha_type_conversion,
                               shader,
                               (int)ima->alpha_mode);

        if (strlen(ima->colorspace_settings.name) > 0) {
          shader.CreateInput(cyclestokens::colorspace, pxr::SdfValueTypeNames->String)
              .Set(std::string(ima->colorspace_settings.name));
        }
      }

      break;
    }

    case SH_NODE_TEX_CHECKER: {
      // NodeTexChecker *storage = (NodeTexChecker *)node->storage;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
    } break;

    case SH_NODE_TEX_BRICK: {
      NodeTexBrick *brick_storage = (NodeTexBrick *)node->storage;
      if (!brick_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      shader.CreateInput(pxr::TfToken("offset_freq"), pxr::SdfValueTypeNames->Int)
          .Set(brick_storage->offset_freq);
      shader.CreateInput(pxr::TfToken("squash_freq"), pxr::SdfValueTypeNames->Int)
          .Set(brick_storage->squash_freq);
      shader.CreateInput(pxr::TfToken("offset"), pxr::SdfValueTypeNames->Float)
          .Set(brick_storage->offset);
      shader.CreateInput(pxr::TfToken("squash"), pxr::SdfValueTypeNames->Float)
          .Set(brick_storage->squash);
    } break;

    case SH_NODE_TEX_ENVIRONMENT: {
      NodeTexEnvironment *env_storage = (NodeTexEnvironment *)node->storage;
      if (!env_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      std::string imagePath = get_tex_image_asset_filepath(node, a_stage, export_params);
      if (imagePath.size() > 0)
        shader.CreateInput(cyclestokens::filename, pxr::SdfValueTypeNames->Asset)
            .Set(pxr::SdfAssetPath(imagePath));
      usd_handle_shader_enum(cyclestokens::projection,
                             node_env_tex_projection_conversion,
                             shader,
                             env_storage->projection);
      usd_handle_shader_enum(cyclestokens::interpolation,
                             node_image_tex_interpolation_conversion,
                             shader,
                             env_storage->interpolation);

      if (node->id) {
        Image *ima = (Image *)node->id;
        usd_handle_shader_enum(pxr::TfToken("alpha_type"),
                               node_image_tex_alpha_type_conversion,
                               shader,
                               (int)ima->alpha_mode);
      }
    } break;

    case SH_NODE_TEX_GRADIENT: {
      NodeTexGradient *grad_storage = (NodeTexGradient *)node->storage;
      if (!grad_storage)
        break;

      usd_handle_shader_enum(pxr::TfToken("type"),
                             node_gradient_tex_type_conversion,
                             shader,
                             grad_storage->gradient_type);
    } break;

    case SH_NODE_TEX_NOISE: {
      NodeTexNoise *noise_storage = (NodeTexNoise *)node->storage;
      if (!noise_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      usd_handle_shader_enum(pxr::TfToken("dimensions"),
                             node_noise_dimensions_conversion,
                             shader,
                             noise_storage->dimensions);
    } break;

    case SH_NODE_TEX_VORONOI: {
      NodeTexVoronoi *voronoi_storage = (NodeTexVoronoi *)node->storage;
      if (!voronoi_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      usd_handle_shader_enum(pxr::TfToken("dimensions"),
                             node_noise_dimensions_conversion,
                             shader,
                             voronoi_storage->dimensions);
      usd_handle_shader_enum(pxr::TfToken("feature"),
                             node_voronoi_feature_conversion,
                             shader,
                             voronoi_storage->feature);
      usd_handle_shader_enum(pxr::TfToken("metric"),
                             node_voronoi_distance_conversion,
                             shader,
                             voronoi_storage->distance);
    } break;

    case SH_NODE_TEX_MUSGRAVE_DEPRECATED: {
      NodeTexMusgrave *musgrave_storage = (NodeTexMusgrave *)node->storage;
      if (!musgrave_storage)
        break;

      usd_handle_shader_enum(pxr::TfToken("type"),
                             node_musgrave_type_conversion,
                             shader,
                             musgrave_storage->musgrave_type);
      usd_handle_shader_enum(pxr::TfToken("dimensions"),
                             node_noise_dimensions_conversion,
                             shader,
                             musgrave_storage->dimensions);
    } break;

    case SH_NODE_TEX_WAVE: {
      NodeTexWave *wave_storage = (NodeTexWave *)node->storage;
      if (!wave_storage)
        break;

      usd_handle_shader_enum(
          pxr::TfToken("type"), node_wave_type_conversion, shader, wave_storage->wave_type);
      usd_handle_shader_enum(pxr::TfToken("profile"),
                             node_wave_profile_conversion,
                             shader,
                             wave_storage->wave_profile);
      usd_handle_shader_enum(pxr::TfToken("rings_direction"),
                             node_wave_rings_direction_conversion,
                             shader,
                             wave_storage->rings_direction);
      usd_handle_shader_enum(pxr::TfToken("bands_direction"),
                             node_wave_bands_direction_conversion,
                             shader,
                             wave_storage->bands_direction);

    } break;

    case SH_NODE_TEX_POINTDENSITY: {
      NodeShaderTexPointDensity *pd_storage = (NodeShaderTexPointDensity *)node->storage;
      if (!pd_storage)
        break;

      // TODO: Incomplete...

      usd_handle_shader_enum(pxr::TfToken("space"),
                             node_point_density_space_conversion,
                             shader,
                             (int)pd_storage->space);
      usd_handle_shader_enum(pxr::TfToken("interpolation"),
                             node_point_density_interpolation_conversion,
                             shader,
                             (int)pd_storage->interpolation);
    } break;

    case SH_NODE_TEX_MAGIC: {
      NodeTexMagic *magic_storage = (NodeTexMagic *)node->storage;
      if (!magic_storage)
        break;
      // TexMapping tex_mapping;
      // ColorMapping color_mapping;
      shader.CreateInput(pxr::TfToken("depth"), pxr::SdfValueTypeNames->Int)
          .Set(magic_storage->depth);
    } break;

      // ==== Ramp

    case SH_NODE_VALTORGB: {
      ColorBand *coba = (ColorBand *)node->storage;
      if (!coba)
        break;

      pxr::VtVec3fArray array;
      pxr::VtFloatArray alpha_array;

      int size = HD_CYCLES_CURVE_EXPORT_RES;
      for (int i = 0; i < size; i++) {

        const float in = (float)i / size;
        float out[4] = {0, 0, 0, 0};

        BKE_colorband_evaluate(coba, in, out);
        array.push_back(pxr::GfVec3f(out[0], out[1], out[2]));
        alpha_array.push_back(out[3]);
      }

      shader.CreateInput(pxr::TfToken("Interpolate"), pxr::SdfValueTypeNames->Bool)
          .Set(coba->ipotype != COLBAND_INTERP_LINEAR);

      shader.CreateInput(pxr::TfToken("Ramp"), pxr::SdfValueTypeNames->Float3Array).Set(array);
      shader.CreateInput(pxr::TfToken("Ramp_Alpha"), pxr::SdfValueTypeNames->FloatArray)
          .Set(alpha_array);
    } break;

      // ==== Curves

    case SH_NODE_CURVE_VEC: {
      CurveMapping *vec_curve_storage = (CurveMapping *)node->storage;
      if (!vec_curve_storage)
        break;

      pxr::VtVec3fArray array;

      BKE_curvemapping_init(vec_curve_storage);

      int size = HD_CYCLES_CURVE_EXPORT_RES;
      for (int i = 0; i < size; i++) {

        float out[3] = {0, 0, 0};

        const float iter[3] = {(float)i / size, (float)i / size, (float)i / size};

        BKE_curvemapping_evaluate3F(vec_curve_storage, out, iter);
        array.push_back(pxr::GfVec3f(out[0], out[1], out[2]));
      }

      // @TODO(bjs): Implement properly
      shader.CreateInput(pxr::TfToken("Min_X"), pxr::SdfValueTypeNames->Float).Set(0.0f);
      shader.CreateInput(pxr::TfToken("Max_X"), pxr::SdfValueTypeNames->Float).Set(1.0f);

      shader.CreateInput(pxr::TfToken("Curves"), pxr::SdfValueTypeNames->Float3Array).Set(array);

    } break;

    case SH_NODE_CURVE_RGB: {
      CurveMapping *col_curve_storage = (CurveMapping *)node->storage;
      if (!col_curve_storage)
        break;

      pxr::VtVec3fArray array;

      BKE_curvemapping_init(col_curve_storage);

      int size = HD_CYCLES_CURVE_EXPORT_RES;
      for (int i = 0; i < size; i++) {

        float out[3] = {0, 0, 0};

        const float iter[3] = {(float)i / size, (float)i / size, (float)i / size};

        BKE_curvemapping_evaluateRGBF(col_curve_storage, out, iter);
        array.push_back(pxr::GfVec3f(out[0], out[1], out[2]));
      }

      // @TODO(bjs): Implement properly
      shader.CreateInput(pxr::TfToken("Min_X"), pxr::SdfValueTypeNames->Float).Set(0.0f);
      shader.CreateInput(pxr::TfToken("Max_X"), pxr::SdfValueTypeNames->Float).Set(1.0f);

      shader.CreateInput(pxr::TfToken("Curves"), pxr::SdfValueTypeNames->Float3Array).Set(array);
    } break;

    // ==== Misc
    case SH_NODE_VALUE: {
      if (!node->outputs.first)
        break;
      bNodeSocket *val_sock = (bNodeSocket *)node->outputs.first;
      if (val_sock) {
        bNodeSocketValueFloat *float_data = (bNodeSocketValueFloat *)val_sock->default_value;
        shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float)
            .Set(float_data->value);
      }
    } break;

    case SH_NODE_RGB: {
      if (!node->outputs.first)
        break;
      bNodeSocket *val_sock = (bNodeSocket *)node->outputs.first;
      if (val_sock) {
        bNodeSocketValueRGBA *col_data = (bNodeSocketValueRGBA *)val_sock->default_value;
        shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(col_data->value[0], col_data->value[1], col_data->value[2]));
      }
    } break;

    case SH_NODE_UVMAP: {
      NodeShaderUVMap *uv_storage = (NodeShaderUVMap *)node->storage;
      if (!uv_storage)
        break;
      // We need to make valid here because actual uv primvar has been
      shader.CreateInput(cyclestokens::attribute, pxr::SdfValueTypeNames->String)
          .Set(pxr::TfMakeValidIdentifier(uv_storage->uv_map));
      break;
    }

    case SH_NODE_HUE_SAT: {
      NodeHueSat *hue_sat_node_str = (NodeHueSat *)node->storage;
      if (!hue_sat_node_str)
        break;
      shader.CreateInput(pxr::TfToken("hue"), pxr::SdfValueTypeNames->Float)
          .Set(hue_sat_node_str->hue);
      shader.CreateInput(pxr::TfToken("sat"), pxr::SdfValueTypeNames->Float)
          .Set(hue_sat_node_str->sat);
      shader.CreateInput(pxr::TfToken("val"), pxr::SdfValueTypeNames->Float)
          .Set(hue_sat_node_str->val);
    } break;

    case SH_NODE_TANGENT: {
      NodeShaderTangent *tangent_node_str = (NodeShaderTangent *)node->storage;
      if (!tangent_node_str)
        break;
      usd_handle_shader_enum(pxr::TfToken("direction_type"),
                             node_tangent_direction_type_conversion,
                             shader,
                             tangent_node_str->direction_type);
      usd_handle_shader_enum(
          pxr::TfToken("axis"), node_tangent_axis_conversion, shader, tangent_node_str->axis);
      shader.CreateInput(pxr::TfToken("Attribute"), pxr::SdfValueTypeNames->String)
          .Set(tangent_node_str->uv_map);
    } break;

    case SH_NODE_NORMAL_MAP: {
      NodeShaderNormalMap *normal_node_str = (NodeShaderNormalMap *)node->storage;
      if (!normal_node_str)
        break;
      usd_handle_shader_enum(
          pxr::TfToken("Space"), node_normal_map_space_conversion, shader, normal_node_str->space);

      // We need to make valid here because actual uv primvar has been
      shader.CreateInput(pxr::TfToken("Attribute"), pxr::SdfValueTypeNames->String)
          .Set(pxr::TfMakeValidIdentifier(normal_node_str->uv_map));
    } break;

    case SH_NODE_VERTEX_COLOR: {
      NodeShaderVertexColor *vert_col_node_str = (NodeShaderVertexColor *)node->storage;
      if (!vert_col_node_str)
        break;
      shader.CreateInput(pxr::TfToken("layer_name"), pxr::SdfValueTypeNames->String)
          .Set(vert_col_node_str->layer_name);
    } break;

    case SH_NODE_TEX_IES: {
      NodeShaderTexIES *ies_node_str = (NodeShaderTexIES *)node->storage;
      if (!ies_node_str)
        break;
      shader.CreateInput(pxr::TfToken("mode"), pxr::SdfValueTypeNames->Int)
          .Set(ies_node_str->mode);

      // TODO Cycles standalone expects this as "File Name" ustring...
      shader.CreateInput(cyclestokens::filename, pxr::SdfValueTypeNames->Asset)
          .Set(pxr::SdfAssetPath(ies_node_str->filepath));
    } break;

    case SH_NODE_ATTRIBUTE: {
      NodeShaderAttribute *attr_node_str = (NodeShaderAttribute *)node->storage;
      if (!attr_node_str)
        break;
      shader.CreateInput(pxr::TfToken("Attribute"), pxr::SdfValueTypeNames->String)
          .Set(attr_node_str->name);
    } break;
  }

  // Assign default input inputs
  for (bNodeSocket *nSock = (bNodeSocket *)node->inputs.first; nSock; nSock = nSock->next) {
    set_default(node, nSock, nSock, shader);
  }

  return shader;
}

static void store_cycles_nodes(pxr::UsdStageRefPtr a_stage,
                               bNodeTree *ntree,
                               pxr::SdfPath shader_path,
                               bNode **material_out,
                               const USDExportParams &export_params)
{
  for (bNode *node = (bNode *)ntree->nodes.first; node; node = node->next) {

    // Blacklist certain nodes
    if (node->flag & NODE_MUTED)
      continue;

    if (node->type == SH_NODE_OUTPUT_MATERIAL) {
      *material_out = node;
      continue;
    }

    pxr::UsdShadeShader node_shader = create_cycles_shader_node(
        a_stage, shader_path, node, export_params);
  }
}

static void link_cycles_nodes(pxr::UsdStageRefPtr a_stage,
                              pxr::UsdShadeMaterial &usd_material,
                              bNodeTree *ntree,
                              pxr::SdfPath shader_path)
{
  // for all links
  for (bNodeLink *link = (bNodeLink *)ntree->links.first; link; link = link->next) {
    bNode *from_node = link->fromnode, *to_node = link->tonode;
    bNodeSocket *from_sock = link->fromsock, *to_sock = link->tosock;

    // We should not encounter any groups, the node tree is pre-flattened.
    if (to_node->type == NODE_GROUP_OUTPUT)
      continue;

    if (from_node->type == NODE_GROUP_OUTPUT)
      continue;

    if (from_node == nullptr)
      continue;
    if (to_node == nullptr)
      continue;
    if (from_sock == nullptr)
      continue;
    if (to_sock == nullptr)
      continue;

    pxr::UsdShadeShader from_shader = pxr::UsdShadeShader::Define(
        a_stage,
        shader_path.AppendChild(pxr::TfToken(pxr::TfMakeValidIdentifier(from_node->name))));

    if (to_node->type == SH_NODE_OUTPUT_MATERIAL) {
      if (strncmp(to_sock->name, "Surface", 64) == 0) {
        if (strncmp(from_sock->name, "BSDF", 64) == 0)
          usd_material.CreateSurfaceOutput(cyclestokens::cycles)
              .ConnectToSource(from_shader.ConnectableAPI(), cyclestokens::bsdf);
        else
          usd_material.CreateSurfaceOutput(cyclestokens::cycles)
              .ConnectToSource(from_shader.ConnectableAPI(), cyclestokens::closure);
      }
      else if (strncmp(to_sock->name, "Volume", 64) == 0)
        usd_material.CreateVolumeOutput(cyclestokens::cycles)
            .ConnectToSource(from_shader.ConnectableAPI(), cyclestokens::bsdf);
      else if (strncmp(to_sock->name, "Displacement", 64) == 0)
        usd_material.CreateDisplacementOutput(cyclestokens::cycles)
            .ConnectToSource(from_shader.ConnectableAPI(), cyclestokens::vector);
      continue;
    }

    pxr::UsdShadeShader to_shader = pxr::UsdShadeShader::Define(
        a_stage, shader_path.AppendChild(pxr::TfToken(pxr::TfMakeValidIdentifier(to_node->name))));

    if (!from_shader.GetPrim().IsValid())
      continue;

    if (!to_shader.GetPrim().IsValid())
      continue;

    // TODO CLEAN
    std::string toName(to_sock->identifier);
    switch (to_node->type) {
      case SH_NODE_MATH: {
        if (toName == "Value_001")
          toName = "Value2";
        else
          toName = "Value1";
      } break;
      case SH_NODE_VECTOR_MATH: {
        if (toName == "Vector_001")
          toName = "Vector2";
        else if (toName == "Vector_002")
          toName = "Vector3";
        else
          toName = "Vector1";
      } break;
      case SH_NODE_ADD_SHADER:
      case SH_NODE_MIX_SHADER: {
        if (toName == "Shader_001")
          toName = "Closure2";
        else if (toName == "Shader")
          toName = "Closure1";
      } break;
      // Only needed in 4.21?
      case SH_NODE_CURVE_RGB: {
        if (toName == "Color")
          toName = "value";
      } break;
      case SH_NODE_SEPRGB_LEGACY: {
        if (toName == "Image")
          toName = "color";
      } break;
    }
    to_lower(toName);

    // TODO CLEAN
    std::string fromName(from_sock->identifier);
    switch (from_node->type) {
      case SH_NODE_ADD_SHADER:
      case SH_NODE_MIX_SHADER: {
        fromName = "Closure";
      } break;
      // Only needed in 4.21?
      case SH_NODE_CURVE_RGB: {
        if (fromName == "Color")
          fromName = "value";
      } break;
    }
    to_lower(fromName);

    to_shader
        .CreateInput(pxr::TfToken(pxr::TfMakeValidIdentifier(toName)),
                     pxr::SdfValueTypeNames->Float)
        .ConnectToSource(from_shader.ConnectableAPI(),
                         pxr::TfToken(pxr::TfMakeValidIdentifier(fromName)));
  }
}

void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                bNodeTree *ntree,
                                pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params)
{

  bNode *output = nullptr;

  bNodeTree *localtree = ntreeLocalize(ntree);

  ntree_shader_groups_expand_inputs(localtree);

  ntree_shader_groups_flatten(localtree);

  localize(localtree, localtree);

  store_cycles_nodes(a_stage, localtree, usd_material.GetPath(), &output, export_params);
  link_cycles_nodes(a_stage, usd_material, localtree, usd_material.GetPath());

  ntreeFreeLocalTree(localtree);
  MEM_freeN(localtree);
}

/* Entry point to create USD Shade Material network from Cycles Node Graph
 * This is needed for re-importing in to Blender and for HdCycles. */
static void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                       Material *material,
                                       pxr::UsdShadeMaterial &usd_material,
                                       const USDExportParams &export_params)
{
  create_usd_cycles_material(a_stage, material->nodetree, usd_material, export_params);
}

static void create_mdl_material(const USDExporterContext &usd_export_context,
                                Material *material,
                                pxr::UsdShadeMaterial &usd_material)
{
#ifdef WITH_PYTHON
  if (!(material && usd_material)) {
    return;
  }

  umm_export_material(usd_export_context, material, usd_material, "MDL");

#endif
}

static pxr::TfToken get_node_tex_image_color_space(bNode *node)
{
  if (!node->id) {
    return pxr::TfToken();
  }

  Image *ima = reinterpret_cast<Image *>(node->id);

  if (IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
    return usdtokens::raw;
  }
  if (IMB_colormanagement_space_name_is_srgb(ima->colorspace_settings.name)) {
    return usdtokens::sRGB;
  }

  return pxr::TfToken();
}

static pxr::TfToken get_node_tex_image_wrap(bNode *node)
{
  if (node->type != SH_NODE_TEX_IMAGE) {
    return pxr::TfToken();
  }

  if (node->storage == nullptr) {
    return pxr::TfToken();
  }

  NodeTexImage *tex_image = static_cast<NodeTexImage *>(node->storage);

  pxr::TfToken wrap;

  switch (tex_image->extension) {
    case SHD_IMAGE_EXTENSION_REPEAT:
      wrap = usdtokens::repeat;
      break;
    case SHD_IMAGE_EXTENSION_EXTEND:
      wrap = usdtokens::clamp;
      break;
    case SHD_IMAGE_EXTENSION_CLIP:
      wrap = usdtokens::black;
      break;
  }

  return wrap;
}

/* Search the upstream node links connected to the given socket and return the first occurrence
 * of the link connected to the node of the given type. Return null if no such link was found.
 * The 'fromnode' and 'fromsock' members of the returned link are guaranteed to be not null. */
static bNodeLink *traverse_channel(bNodeSocket *input, const short target_type)
{
  if (!(input->link && input->link->fromnode && input->link->fromsock)) {
    return nullptr;
  }

  bNode *linked_node = input->link->fromnode;
  if (linked_node->type == target_type) {
    /* Return match. */
    return input->link;
  }

  /* Recursively traverse the linked node's sockets. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &linked_node->inputs) {
    /* Apply heuristics to skip certain inputs. */
    if (strcmp(sock->name, "Factor") == 0) {
      continue;
    }
    if (bNodeLink *found_link = traverse_channel(sock, target_type)) {
      return found_link;
    }
  }

  return nullptr;
}

/* Returns the first occurrence of a principled BSDF or a diffuse BSDF node found in the given
 * material's node tree.  Returns null if no instance of either type was found. */
static bNode *find_bsdf_node(Material *material)
{
  for (bNode *node : material->nodetree->all_nodes()) {
    if (ELEM(node->type, SH_NODE_BSDF_PRINCIPLED, SH_NODE_BSDF_DIFFUSE)) {
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
  pxr::SdfPath shader_path = material.GetPath().AppendChild(
      pxr::TfToken("preview_" + pxr::TfMakeValidIdentifier(name)));
  pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(usd_export_context.stage, shader_path);

  switch (type) {
    case SH_NODE_TEX_IMAGE: {
      shader.CreateIdAttr(pxr::VtValue(usdtokens::uv_texture));
      break;
    }
    case SH_NODE_MAPPING: {
      shader.CreateIdAttr(pxr::VtValue(usdtokens::UsdTransform2d));
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
      material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), usdtokens::surface);
      break;
    }

    default:
      break;
  }

  return shader;
}

/* Creates a USD Preview Surface shader based on the given cycles shading node.
 * Due to the limited nodes in the USD Preview Surface specification, only the following nodes
 * are supported:
 * - UVMap
 * - Texture Coordinate
 * - Image Texture
 * - Principled BSDF
 * More may be added in the future.
 */
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
  std::string imagePath = get_tex_image_asset_filepath(usd_export_context, node);
  if (!imagePath.empty()) {
    shader.CreateInput(usdtokens::file, pxr::SdfValueTypeNames->Asset)
        .Set(pxr::SdfAssetPath(imagePath));
  }

  pxr::TfToken colorSpace = get_node_tex_image_color_space(node);
  if (!colorSpace.IsEmpty()) {
    shader.CreateInput(usdtokens::sourceColorSpace, pxr::SdfValueTypeNames->Token).Set(colorSpace);
  }

  pxr::TfToken wrap = get_node_tex_image_wrap(node);
  if (!wrap.IsEmpty()) {
    shader.CreateInput(usdtokens::wrapS, pxr::SdfValueTypeNames->Token).Set(wrap);
    shader.CreateInput(usdtokens::wrapT, pxr::SdfValueTypeNames->Token).Set(wrap);
  }

  return shader;
}

static std::string get_tex_image_asset_filepath(Image *ima)
{
  char filepath[FILE_MAX];
  get_absolute_path(ima, filepath);

  return std::string(filepath);
}

static std::string get_tex_image_asset_filepath(const USDExporterContext &usd_export_context,
                                                bNode *node)
{
  return get_tex_image_asset_filepath(
      node, usd_export_context.stage, usd_export_context.export_params);
}

/* Gets an asset path for the given texture image node. The resulting path
 * may be absolute, relative to the USD file, or in a 'textures' directory
 * in the same directory as the USD file, depending on the export parameters.
 * The filename is typically the image filepath but might also be automatically
 * generated based on the image name for in-memory textures when exporting textures.
 * This function may return an empty string if the image does not have a filepath
 * assigned and no asset path could be determined. */
std::string get_tex_image_asset_filepath(bNode *node,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params)
{
  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return "";
  }

  std::string path;

  if (is_in_memory_texture(ima)) {
    path = get_in_memory_texture_filename(ima);
  }
  else if (strlen(ima->filepath) > 0) {
    /* Get absolute path. */
    path = get_tex_image_asset_filepath(ima);
  }

  return get_tex_image_asset_filepath(path, stage, export_params);
}

/* Return a USD asset path referencing the given texture file. The resulting path
 * may be absolute, relative to the USD file, or in a 'textures' directory
 * in the same directory as the USD file, depending on the export parameters. */
std::string get_tex_image_asset_filepath(const std::string &path,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params)
{
  if (path.empty()) {
    return path;
  }

  if (export_params.export_textures) {
    /* The texture is exported to a 'textures' directory next to the
     * USD root layer. */

    char exp_path[FILE_MAX];
    char file_path[FILE_MAX];
    BLI_path_split_file_part(path.c_str(), file_path, FILE_MAX);

    if (export_params.relative_paths) {
      BLI_path_join(exp_path, FILE_MAX, ".", "textures", file_path);
    }
    else {
      /* Create absolute path in the textures directory. */
      pxr::SdfLayerHandle layer = stage->GetRootLayer();
      std::string stage_path = layer->GetRealPath();
      if (stage_path.empty()) {
        return path;
      }

      char dir_path[FILE_MAX];
      BLI_path_split_dir_part(stage_path.c_str(), dir_path, FILE_MAX);
      BLI_path_join(exp_path, FILE_MAX, dir_path, "textures", file_path);
    }
    BLI_string_replace_char(exp_path, '\\', '/');
    return exp_path;
  }

  if (export_params.relative_paths) {
    /* Get the path relative to the USD. */
    pxr::SdfLayerHandle layer = stage->GetRootLayer();
    std::string stage_path = layer->GetRealPath();
    if (stage_path.empty()) {
      return path;
    }

    char rel_path[FILE_MAX];
    STRNCPY(rel_path, path.c_str());

    BLI_path_rel(rel_path, stage_path.c_str());
    if (!BLI_path_is_rel(rel_path)) {
      return path;
    }
    BLI_string_replace_char(rel_path, '\\', '/');
    return rel_path + 2;
  }

  return path;
}

/* If the given image is tiled, copy the image tiles to the given
 * destination directory. */
static void copy_tiled_textures(Image *ima,
                                const std::string &dest_dir,
                                const bool allow_overwrite,
                                ReportList *reports)
{
  char src_path[FILE_MAX];
  get_absolute_path(ima, src_path);

  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern = BKE_image_get_tile_strformat(src_path, &tile_format);

  /* Only <UDIM> tile formats are supported by USD right now. */
  if (tile_format != UDIM_TILE_FORMAT_UDIM) {
    CLOG_WARN(&LOG, "Unsupported tile format for '%s'", src_path);
    MEM_SAFE_FREE(udim_pattern);
    return;
  }

  const eUSDTexNameCollisionMode tex_name_collision_mode = allow_overwrite ?
                                                               USD_TEX_NAME_COLLISION_OVERWRITE :
                                                               USD_TEX_NAME_COLLISION_USE_EXISTING;

  /* Copy all tiles. */
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    char src_tile_path[FILE_MAX];
    BKE_image_set_filepath_from_tile_number(
        src_tile_path, udim_pattern, tile_format, tile->tile_number);

    char dest_filename[FILE_MAXFILE];
    BLI_path_split_file_part(src_tile_path, dest_filename, sizeof(dest_filename));

    char dest_tile_path[FILE_MAX];
    BLI_path_join(dest_tile_path, FILE_MAX, dest_dir.c_str(), dest_filename);
    BLI_string_replace_char(dest_tile_path, '\\', '/');

    if (!allow_overwrite && asset_exists(dest_tile_path)) {
      continue;
    }

    if (paths_equal(src_tile_path, dest_tile_path)) {
      /* Source and destination paths are the same, don't copy. */
      continue;
    }

    CLOG_INFO(&LOG, 2, "Copying texture tile from '%s' to '%s'", src_tile_path, dest_tile_path);

    /* Copy the file. */
    if (!copy_asset(src_tile_path, dest_tile_path, tex_name_collision_mode, reports)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "USD export: could not copy texture tile from %s to %s",
                  src_tile_path,
                  dest_tile_path);
    }
  }
  MEM_SAFE_FREE(udim_pattern);
}

/* Copy the given image to the destination directory. */
static void copy_single_file(Image *ima,
                             const std::string &dest_dir,
                             const bool allow_overwrite,
                             ReportList *reports)
{
  char source_path[FILE_MAX];
  get_absolute_path(ima, source_path);

  char file_name[FILE_MAX];
  BLI_path_split_file_part(source_path, file_name, FILE_MAX);

  char dest_path[FILE_MAX];
  BLI_path_join(dest_path, FILE_MAX, dest_dir.c_str(), file_name);
  BLI_string_replace_char(dest_path, '\\', '/');

  if (!allow_overwrite && asset_exists(dest_path)) {
    return;
  }

  if (paths_equal(source_path, dest_path)) {
    /* Source and destination paths are the same, don't copy. */
    return;
  }

  CLOG_INFO(&LOG, 2, "Copying texture from '%s' to '%s'", source_path, dest_path);

  /* Copy the file. */
  if (!copy_asset(source_path,
                  dest_path,
                  allow_overwrite ? USD_TEX_NAME_COLLISION_OVERWRITE :
                                    USD_TEX_NAME_COLLISION_USE_EXISTING,
                  reports))
  {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD export: could not copy texture from %s to %s",
                source_path,
                dest_path);
  }
}

/* Export the texture of every texture image node in the given
 * node tree. */
static void export_textures(const bNodeTree *ntree,
                            const pxr::UsdStageRefPtr stage,
                            const bool allow_overwrite,
                            ReportList *reports)
{
  if (!ntree) {
    return;
  }

  if (!stage) {
    return;
  }

  ntree->ensure_topology_cache();

  for (bNode *node = (bNode *)ntree->nodes.first; node; node = node->next) {
    if (ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT)) {
      export_texture(node, stage, allow_overwrite, reports);
    }
    else if (node->is_group()) {
      if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
        export_textures(sub_tree, stage, allow_overwrite, reports);
      }
    }
  }
}

/* Export the given texture node's image to a 'textures' directory in the export path.
 * Based on ImagesExporter::export_UV_Image() */
static void export_texture(const USDExporterContext &usd_export_context, bNode *node)
{
  export_texture(node,
                 usd_export_context.stage,
                 usd_export_context.export_params.overwrite_textures,
                 usd_export_context.export_params.worker_status->reports);
}

void export_texture(bNode *node,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite,
                    ReportList *reports)
{
  if (!ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT)) {
    return;
  }

  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return;
  }

  std::string dest_dir = get_export_textures_dir(stage);

  if (dest_dir.empty()) {
    WM_reportf(RPT_WARNING, "%s: Couldn't determine textures directory path", __func__);
    return;
  }

  if (is_in_memory_texture(ima)) {
    export_in_memory_texture(
        ima, dest_dir, allow_overwrite, reports);
  }
  else if (ima->source == IMA_SRC_TILED) {
    copy_tiled_textures(
        ima, dest_dir, allow_overwrite, reports);
  }
  else {
    copy_single_file(
        ima, dest_dir, allow_overwrite, reports);
  }
}

/* Export the texture of every texture image node in the given material's node tree. */
static void export_textures(const Material *material,
                            const pxr::UsdStageRefPtr stage,
                            const bool allow_overwrite,
                            ReportList *reports)
{
  if (!(material && material->use_nodes)) {
    return;
  }

  if (!stage) {
    return;
  }

  export_textures(material->nodetree, stage, allow_overwrite, reports);
}

const pxr::TfToken token_for_input(const char *input_name)
{
  const InputSpecMap &input_map = preview_surface_input_map();
  const InputSpec *spec = input_map.lookup_ptr(input_name);

  if (spec == nullptr) {
    return {};
  }

  return spec->input_name;
}

pxr::UsdShadeMaterial create_usd_material(const USDExporterContext &usd_export_context,
                                          pxr::SdfPath usd_path,
                                          Material *material,
                                          const std::string &active_uv,
                                          ReportList *reports)
{
  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Define(usd_export_context.stage,
                                                                     usd_path);

  bool textures_exported = false;

  if (material->use_nodes && usd_export_context.export_params.generate_mdl) {
    create_mdl_material(usd_export_context, material, usd_material);
    if (usd_export_context.export_params.export_textures) {
      export_textures(
          material, usd_export_context.stage, usd_export_context.export_params.overwrite_textures, reports);
      textures_exported = true;
    }
  }
  if (material->use_nodes && usd_export_context.export_params.generate_cycles_shaders) {
    create_usd_cycles_material(
        usd_export_context.stage, material, usd_material, usd_export_context.export_params);
    if (!textures_exported && usd_export_context.export_params.export_textures) {
      export_textures(
          material, usd_export_context.stage, usd_export_context.export_params.overwrite_textures, reports);
      textures_exported = true;
    }
  }
  if (material->use_nodes && usd_export_context.export_params.generate_preview_surface) {
    create_usd_preview_surface_material(
        usd_export_context, material, usd_material, active_uv, reports);
  }
  else {
    create_usd_viewport_material(usd_export_context, material, usd_material);
  }

  call_material_export_hooks(usd_export_context.stage,
                             material,
                             usd_material,
                             usd_export_context.export_params.worker_status->reports);

  return usd_material;
}

}  // namespace blender::io::usd
