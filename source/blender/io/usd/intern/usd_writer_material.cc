/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_material.hh"

#include "usd_exporter_context.hh"
#include "usd_hook.hh"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"

#include "IMB_colormanagement.hh"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_memory_utils.hh"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "MEM_guardedalloc.h"

#include "WM_types.hh"

#include <pxr/base/tf/stringUtils.h>

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
static const pxr::TfToken mirror("mirror", pxr::TfToken::Immortal);
static const pxr::TfToken wrapS("wrapS", pxr::TfToken::Immortal);
static const pxr::TfToken wrapT("wrapT", pxr::TfToken::Immortal);
static const pxr::TfToken in("in", pxr::TfToken::Immortal);
static const pxr::TfToken translation("translation", pxr::TfToken::Immortal);
static const pxr::TfToken rotation("rotation", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* Cycles specific tokens. */
namespace cyclestokens {
static const std::string UVMap("UVMap");
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
                            const std::string &default_uv,
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
  std::string default_uv_sampler = default_uv.empty() ? cyclestokens::UVMap : default_uv;

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
      bNodeSocket *emission_strength_sock = bke::nodeFindSocket(
          node, SOCK_IN, "Emission Strength");

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
        /* If the input is a float, we check if there is also a Separate Color node in between, if
         * there is use the output channel from that, otherwise connect either the texture alpha or
         * red channels. */
        bNodeLink *input_link_sep_color = traverse_channel(sock, SH_NODE_SEPARATE_COLOR);
        if (input_link_sep_color) {
          if (STREQ(input_link_sep_color->fromsock->identifier, "Red")) {
            source_name = usdtokens::r;
          }
          if (STREQ(input_link_sep_color->fromsock->identifier, "Green")) {
            source_name = usdtokens::g;
          }
          if (STREQ(input_link_sep_color->fromsock->identifier, "Blue")) {
            source_name = usdtokens::b;
          }
        }
        else {
          source_name = STREQ(input_link->fromsock->identifier, "Alpha") ? usdtokens::a :
                                                                           usdtokens::r;
        }
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
          bNodeSocket *sock_current = bke::nodeFindSocket(vector_math_node, SOCK_IN, "Vector");
          bNodeLink *temp_link = traverse_channel(sock_current, SH_NODE_VECTOR_MATH);
          if (temp_link && temp_link->fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY_ADD) {
            vector_math_node = temp_link->fromnode;
          }

          bNodeSocket *sock_scale = bke::nodeFindSocket(vector_math_node, SOCK_IN, "Vector_001");
          bNodeSocket *sock_bias = bke::nodeFindSocket(vector_math_node, SOCK_IN, "Vector_002");
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
      if (bNodeSocket *socket = bke::nodeFindSocket(input_node, SOCK_IN, "Vector")) {
        if (pxr::UsdShadeInput st_input = usd_shader.CreateInput(usdtokens::st,
                                                                 pxr::SdfValueTypeNames->Float2))
        {
          create_uv_input(
              usd_export_context, socket, usd_material, st_input, default_uv_sampler, reports);
        }
      }

      /* Set opacityThreshold if an alpha cutout is used. */
      if (input_spec.input_name == usdtokens::opacity) {
        float threshold = 0.0f;

        /* The immediate upstream node should either be a Math Round or a Math 1-minus. */
        bNodeLink *math_link = traverse_channel(sock, SH_NODE_MATH);
        if (math_link && math_link->fromnode) {
          bNode *math_node = math_link->fromnode;

          if (math_node->custom1 == NODE_MATH_ROUND) {
            threshold = 0.5f;
          }
          else if (math_node->custom1 == NODE_MATH_SUBTRACT) {
            /* If this is the 1-minus node, we need to search upstream to find the less-than. */
            bNodeSocket *sock = blender::bke::nodeFindSocket(math_node, SOCK_IN, "Value");
            if (((bNodeSocketValueFloat *)sock->default_value)->value == 1.0f) {
              sock = blender::bke::nodeFindSocket(math_node, SOCK_IN, "Value_001");
              math_link = traverse_channel(sock, SH_NODE_MATH);
              if (math_link && math_link->fromnode) {
                math_node = math_link->fromnode;

                if (math_node->custom1 == NODE_MATH_LESS_THAN) {
                  /* We found the upstream less-than with the threshold value. */
                  bNodeSocket *threshold_sock = blender::bke::nodeFindSocket(
                      math_node, SOCK_IN, "Value_001");
                  threshold = ((bNodeSocketValueFloat *)threshold_sock->default_value)->value;
                }
              }
            }
          }
        }

        if (threshold > 0.0f) {
          pxr::UsdShadeInput opacity_threshold_input = preview_surface.CreateInput(
              usdtokens::opacityThreshold, pxr::SdfValueTypeNames->Float);
          opacity_threshold_input.GetAttr().Set(pxr::VtValue(threshold));
        }
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
                                const std::string &default_uv,
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

  std::string uv_name = default_uv;
  if (uv_node && uv_node->storage) {
    NodeShaderUVMap *shader_uv_map = static_cast<NodeShaderUVMap *>(uv_node->storage);
    /* We need to make valid here because actual uv primvar has been. */
    uv_name = pxr::TfMakeValidIdentifier(shader_uv_map->uv_map);
  }

  uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->String).Set(uv_name);
  usd_input.ConnectToSource(uv_shader.ConnectableAPI(), usdtokens::result);
}

static void create_transform2d_shader(const USDExporterContext &usd_export_context,
                                      bNodeLink *mapping_link,
                                      pxr::UsdShadeMaterial &usd_material,
                                      pxr::UsdShadeInput &usd_input,
                                      const std::string &default_uv,
                                      ReportList *reports)

{
  bNode *mapping_node = (mapping_link && mapping_link->fromnode ? mapping_link->fromnode :
                                                                  nullptr);

  BLI_assert(mapping_node && mapping_node->type == SH_NODE_MAPPING);

  if (!mapping_node) {
    return;
  }

  if (mapping_node->custom1 != TEXMAP_TYPE_POINT) {
    if (bNodeSocket *socket = bke::nodeFindSocket(mapping_node, SOCK_IN, "Vector")) {
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

  if (bNodeSocket *scale_socket = bke::nodeFindSocket(mapping_node, SOCK_IN, "Scale")) {
    copy_v3_v3(scale, ((bNodeSocketValueVector *)scale_socket->default_value)->value);
    /* Ignore the Z scale. */
    scale[2] = 1.0f;
  }

  if (bNodeSocket *loc_socket = bke::nodeFindSocket(mapping_node, SOCK_IN, "Location")) {
    copy_v3_v3(loc, ((bNodeSocketValueVector *)loc_socket->default_value)->value);
    /* Ignore the Z translation. */
    loc[2] = 0.0f;
  }

  if (bNodeSocket *rot_socket = bke::nodeFindSocket(mapping_node, SOCK_IN, "Rotation")) {
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

  if (bNodeSocket *socket = bke::nodeFindSocket(mapping_node, SOCK_IN, "Vector")) {
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
                            const std::string &default_uv,
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
  BKE_image_format_from_imbuf(&imageFormat, imbuf);
  BKE_image_release_ibuf(ima, imbuf, nullptr);

  char file_name[FILE_MAX];
  /* Use the image name for the file name. */
  STRNCPY(file_name, ima->id.name + 2);

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
  if (ima->filepath[0]) {
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

  if (!allow_overwrite && BLI_exists(export_path)) {
    return;
  }

  if ((BLI_path_cmp_normalized(export_path, image_abs_path) == 0) && BLI_exists(image_abs_path)) {
    /* As a precaution, don't overwrite the original path. */
    return;
  }

  CLOG_INFO(&LOG, 2, "Exporting in-memory texture to '%s'", export_path);

  if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == 0) {
    BKE_reportf(
        reports, RPT_WARNING, "USD export: couldn't export in-memory texture to %s", export_path);
  }
}

/* Get the absolute filepath of the given image.  Assumes
 * r_path result array is of length FILE_MAX. */
static void get_absolute_path(Image *ima, char *r_path)
{
  /* Make absolute source path. */
  BLI_strncpy(r_path, ima->filepath, FILE_MAX);
  BLI_path_abs(r_path, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));
  BLI_path_normalize(r_path);
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
    case SHD_IMAGE_EXTENSION_MIRROR:
      wrap = usdtokens::mirror;
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
      pxr::TfToken(pxr::TfMakeValidIdentifier(name)));
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

std::string get_tex_image_asset_filepath(bNode *node,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params)
{
  std::string stage_path = stage->GetRootLayer()->GetRealPath();

  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return "";
  }

  std::string path;

  if (ima->filepath[0]) {
    /* Get absolute path. */
    path = get_tex_image_asset_filepath(ima);
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
    BLI_path_split_file_part(path.c_str(), file_path, FILE_MAX);

    if (export_params.relative_paths) {
      BLI_path_join(exp_path, FILE_MAX, ".", "textures", file_path);
    }
    else {
      /* Create absolute path in the textures directory. */
      char dir_path[FILE_MAX];
      BLI_path_split_dir_part(stage_path.c_str(), dir_path, FILE_MAX);
      BLI_path_join(exp_path, FILE_MAX, dir_path, "textures", file_path);
    }
    BLI_string_replace_char(exp_path, '\\', '/');
    return exp_path;
  }

  if (export_params.relative_paths) {
    /* Get the path relative to the USD. */
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

  /* Copy all tiles. */
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    char src_tile_path[FILE_MAX];
    BKE_image_set_filepath_from_tile_number(
        src_tile_path, udim_pattern, tile_format, tile->tile_number);

    char dest_filename[FILE_MAXFILE];
    BLI_path_split_file_part(src_tile_path, dest_filename, sizeof(dest_filename));

    char dest_tile_path[FILE_MAX];
    BLI_path_join(dest_tile_path, FILE_MAX, dest_dir.c_str(), dest_filename);

    if (!allow_overwrite && BLI_exists(dest_tile_path)) {
      continue;
    }

    if (BLI_path_cmp_normalized(src_tile_path, dest_tile_path) == 0) {
      /* Source and destination paths are the same, don't copy. */
      continue;
    }

    CLOG_INFO(&LOG, 2, "Copying texture tile from '%s' to '%s'", src_tile_path, dest_tile_path);

    /* Copy the file. */
    if (BLI_copy(src_tile_path, dest_tile_path) != 0) {
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

  if (!allow_overwrite && BLI_exists(dest_path)) {
    return;
  }

  if (BLI_path_cmp_normalized(source_path, dest_path) == 0) {
    /* Source and destination paths are the same, don't copy. */
    return;
  }

  CLOG_INFO(&LOG, 2, "Copying texture from '%s' to '%s'", source_path, dest_path);

  /* Copy the file. */
  if (BLI_copy(source_path, dest_path) != 0) {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD export: could not copy texture from %s to %s",
                source_path,
                dest_path);
  }
}

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

  std::string export_path = stage->GetRootLayer()->GetRealPath();
  if (export_path.empty()) {
    return;
  }

  char usd_dir_path[FILE_MAX];
  BLI_path_split_dir_part(export_path.c_str(), usd_dir_path, FILE_MAX);

  char tex_dir_path[FILE_MAX];
  BLI_path_join(tex_dir_path, FILE_MAX, usd_dir_path, "textures", SEP_STR);

  BLI_dir_create_recursive(tex_dir_path);

  const bool is_dirty = BKE_image_is_dirty(ima);
  const bool is_generated = ima->source == IMA_SRC_GENERATED;
  const bool is_packed = BKE_image_has_packedfile(ima);

  std::string dest_dir(tex_dir_path);

  if (is_generated || is_dirty || is_packed) {
    export_in_memory_texture(ima, dest_dir, allow_overwrite, reports);
  }
  else if (ima->source == IMA_SRC_TILED) {
    copy_tiled_textures(ima, dest_dir, allow_overwrite, reports);
  }
  else {
    copy_single_file(ima, dest_dir, allow_overwrite, reports);
  }
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
