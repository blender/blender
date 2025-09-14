/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_material.hh"
#include "usd_asset_utils.hh"
#include "usd_exporter_context.hh"
#include "usd_hook.hh"
#include "usd_utils.hh"

#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"

#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_packedFile_types.h"

#include "MEM_guardedalloc.h"

#include "WM_types.hh"

#include <pxr/base/tf/stringUtils.h>

#ifdef WITH_MATERIALX
#  include "shader/materialx/material.h"
#  include <pxr/usd/sdf/copyUtils.h>
#  include <pxr/usd/usdMtlx/materialXConfigAPI.h>
#  include <pxr/usd/usdMtlx/reader.h>
#endif

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
static const pxr::TfToken primvar_float("UsdPrimvarReader_float", pxr::TfToken::Immortal);
static const pxr::TfToken primvar_float2("UsdPrimvarReader_float2", pxr::TfToken::Immortal);
static const pxr::TfToken primvar_float3("UsdPrimvarReader_float3", pxr::TfToken::Immortal);
static const pxr::TfToken primvar_vector("UsdPrimvarReader_vector", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken specular("specular", pxr::TfToken::Immortal);
static const pxr::TfToken opacity("opacity", pxr::TfToken::Immortal);
static const pxr::TfToken opacityThreshold("opacityThreshold", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
static const pxr::TfToken displacement("displacement", pxr::TfToken::Immortal);
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
using InputSpecMap = blender::Map<StringRef, InputSpec>;

/* Static function forward declarations. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     const pxr::UsdShadeMaterial &material,
                                                     const StringRef name,
                                                     int type);
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     const pxr::UsdShadeMaterial &material,
                                                     bNode *node);
static pxr::UsdShadeShader create_primvar_reader_shader(
    const USDExporterContext &usd_export_context,
    const pxr::UsdShadeMaterial &material,
    const pxr::TfToken &primvar_type,
    const bNode *node);
static void create_uv_input(const USDExporterContext &usd_export_context,
                            bNodeSocket *input_socket,
                            pxr::UsdShadeMaterial &usd_material,
                            pxr::UsdShadeInput &usd_input,
                            const std::string &active_uvmap_name,
                            ReportList *reports);
static void export_texture(const USDExporterContext &usd_export_context, bNode *node);
static bNode *find_bsdf_node(Material *material);
static bNode *find_displacement_node(Material *material);
static void get_absolute_path(const Image *ima, char *r_path);
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

static void set_scale_bias(pxr::UsdShadeShader &usd_shader,
                           const pxr::GfVec4f scale,
                           const pxr::GfVec4f bias)
{
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

static void process_inputs(const USDExporterContext &usd_export_context,
                           pxr::UsdShadeMaterial &usd_material,
                           pxr::UsdShadeShader &shader,
                           const bNode *node,
                           const std::string &active_uvmap_name,
                           ReportList *reports)
{
  const InputSpecMap &input_map = preview_surface_input_map();

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    /* Check if this socket is mapped to a USD preview shader input. */
    const InputSpec *spec = input_map.lookup_ptr(sock->name);
    if (spec == nullptr) {
      continue;
    }

    const InputSpec &input_spec = *spec;

    /* Allow scaling inputs. */
    float input_scale = 1.0;

    /* Don't export emission color if strength is zero. */
    if (input_spec.input_name == usdtokens::emissive_color) {
      const bNodeSocket *emission_strength_sock = bke::node_find_socket(
          *node, SOCK_IN, "Emission Strength");
      if (!emission_strength_sock) {
        continue;
      }

      input_scale = ((bNodeSocketValueFloat *)emission_strength_sock->default_value)->value;
      if (input_scale == 0.0f) {
        continue;
      }
    }

    bool processed = false;

    /* Check for an upstream Image node. */
    const bNodeLink *input_link = traverse_channel(sock, SH_NODE_TEX_IMAGE);
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
        const bNodeLink *input_link_sep_color = traverse_channel(sock, SH_NODE_SEPARATE_COLOR);
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
      shader.CreateInput(input_spec.input_name, input_spec.input_type)
          .ConnectToSource(source_info);

      set_normal_texture_range(usd_shader, input_spec);

      /* Export the texture, if necessary. */
      if (usd_export_context.export_params.export_textures) {
        export_texture(usd_export_context, input_node);
      }

      /* Scale-Bias processing.
       * Ordinary: If a Vector Math node was detected ahead of the texture node, and it has
       *   the correct type, NODE_VECTOR_MATH_MULTIPLY_ADD, assume it's meant to be
       *   used for scale-bias.
       * Displacement: The scale-bias values come from the Midlevel and Scale sockets.
       */
      if (input_spec.input_name != usdtokens::displacement) {
        bNodeLink *scale_link = traverse_channel(sock, SH_NODE_VECTOR_MATH);
        if (scale_link) {
          bNode *vector_math_node = scale_link->fromnode;
          if (vector_math_node->custom1 == NODE_VECTOR_MATH_MULTIPLY_ADD) {
            /* Attempt one more traversal in case the current node is not the
             * correct NODE_VECTOR_MATH_MULTIPLY_ADD (see code in usd_reader_material). */
            bNodeSocket *sock_current = bke::node_find_socket(
                *vector_math_node, SOCK_IN, "Vector");
            bNodeLink *temp_link = traverse_channel(sock_current, SH_NODE_VECTOR_MATH);
            if (temp_link && temp_link->fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY_ADD) {
              vector_math_node = temp_link->fromnode;
            }

            bNodeSocket *sock_scale = bke::node_find_socket(
                *vector_math_node, SOCK_IN, "Vector_001");
            bNodeSocket *sock_bias = bke::node_find_socket(
                *vector_math_node, SOCK_IN, "Vector_002");
            const float *scale_value =
                static_cast<bNodeSocketValueVector *>(sock_scale->default_value)->value;
            const float *bias_value =
                static_cast<bNodeSocketValueVector *>(sock_bias->default_value)->value;

            const pxr::GfVec4f scale(scale_value[0], scale_value[1], scale_value[2], 1.0f);
            const pxr::GfVec4f bias(bias_value[0], bias_value[1], bias_value[2], 0.0f);
            set_scale_bias(usd_shader, scale, bias);
          }
        }
      }
      else {
        const bNodeSocket *sock_midlevel = bke::node_find_socket(*node, SOCK_IN, "Midlevel");
        const bNodeSocket *sock_scale = bke::node_find_socket(*node, SOCK_IN, "Scale");
        const float midlevel_value =
            sock_midlevel->default_value_typed<bNodeSocketValueFloat>()->value;
        const float scale_value = sock_scale->default_value_typed<bNodeSocketValueFloat>()->value;

        const float adjusted_bias = -midlevel_value * scale_value;
        const pxr::GfVec4f scale(scale_value, scale_value, scale_value, 1.0f);
        const pxr::GfVec4f bias(adjusted_bias, adjusted_bias, adjusted_bias, 0.0f);
        set_scale_bias(usd_shader, scale, bias);
      }

      /* Look for a connected uvmap node. */
      if (bNodeSocket *socket = bke::node_find_socket(*input_node, SOCK_IN, "Vector")) {
        if (pxr::UsdShadeInput st_input = usd_shader.CreateInput(usdtokens::st,
                                                                 pxr::SdfValueTypeNames->Float2))
        {
          create_uv_input(
              usd_export_context, socket, usd_material, st_input, active_uvmap_name, reports);
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
            bNodeSocket *math_sock = blender::bke::node_find_socket(*math_node, SOCK_IN, "Value");
            if (((bNodeSocketValueFloat *)math_sock->default_value)->value == 1.0f) {
              math_sock = blender::bke::node_find_socket(*math_node, SOCK_IN, "Value_001");
              math_link = traverse_channel(math_sock, SH_NODE_MATH);
              if (math_link && math_link->fromnode) {
                math_node = math_link->fromnode;

                if (math_node->custom1 == NODE_MATH_LESS_THAN) {
                  /* We found the upstream less-than with the threshold value. */
                  bNodeSocket *threshold_sock = blender::bke::node_find_socket(
                      *math_node, SOCK_IN, "Value_001");
                  threshold = ((bNodeSocketValueFloat *)threshold_sock->default_value)->value;
                }
              }
            }
          }
        }

        if (threshold > 0.0f) {
          pxr::UsdShadeInput opacity_threshold_input = shader.CreateInput(
              usdtokens::opacityThreshold, pxr::SdfValueTypeNames->Float);
          opacity_threshold_input.GetAttr().Set(pxr::VtValue(threshold));
        }
      }

      processed = true;
    }

    if (processed) {
      continue;
    }

    /* No upstream Image was found. Check for an Attribute node instead */
    input_link = traverse_channel(sock, SH_NODE_ATTRIBUTE);
    if (input_link) {
      const bNode *attr_node = input_link->fromnode;
      const NodeShaderAttribute *storage = (NodeShaderAttribute *)attr_node->storage;

      if (storage->type == SHD_ATTRIBUTE_GEOMETRY) {
        pxr::SdfValueTypeName output_type;
        pxr::UsdShadeShader usd_shader;
        if (STREQ(input_link->fromsock->identifier, "Color")) {
          output_type = pxr::SdfValueTypeNames->Float3;
          usd_shader = create_primvar_reader_shader(
              usd_export_context, usd_material, usdtokens::primvar_float3, attr_node);
        }
        else if (STREQ(input_link->fromsock->identifier, "Vector")) {
          output_type = pxr::SdfValueTypeNames->Float3;
          usd_shader = create_primvar_reader_shader(
              usd_export_context, usd_material, usdtokens::primvar_vector, attr_node);
        }
        else if (STREQ(input_link->fromsock->identifier, "Fac")) {
          output_type = pxr::SdfValueTypeNames->Float;
          usd_shader = create_primvar_reader_shader(
              usd_export_context, usd_material, usdtokens::primvar_float, attr_node);
        }

        std::string attr_name = make_safe_name(storage->name,
                                               usd_export_context.export_params.allow_unicode);
        usd_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->String).Set(attr_name);

        pxr::UsdShadeConnectionSourceInfo source_info(usd_shader.ConnectableAPI(),
                                                      usdtokens::result,
                                                      pxr::UsdShadeAttributeType::Output,
                                                      output_type);
        shader.CreateInput(input_spec.input_name, input_spec.input_type)
            .ConnectToSource(source_info);

        processed = true;
      }
    }

    if (processed) {
      continue;
    }

    /* No upstream nodes, just set a default constant. */
    if (input_spec.set_default_value) {
      switch (sock->type) {
        case SOCK_FLOAT: {
          create_input<bNodeSocketValueFloat, float>(
              shader, input_spec, sock->default_value, input_scale);
        } break;
        case SOCK_VECTOR: {
          create_input<bNodeSocketValueVector, pxr::GfVec3f>(
              shader, input_spec, sock->default_value, input_scale);
        } break;
        case SOCK_RGBA: {
          create_input<bNodeSocketValueRGBA, pxr::GfVec3f>(
              shader, input_spec, sock->default_value, input_scale);
        } break;
        default:
          break;
      }
    }
  }
}

static void create_usd_preview_surface_material(const USDExporterContext &usd_export_context,
                                                Material *material,
                                                pxr::UsdShadeMaterial &usd_material,
                                                const std::string &active_uvmap_name,
                                                ReportList *reports)
{
  if (!material) {
    return;
  }

  /* We only handle the first instance of either principled or
   * diffuse bsdf nodes in the material's node tree, because
   * USD Preview Surface has no concept of layering materials. */
  bNode *surface_node = find_bsdf_node(material);
  if (!surface_node) {
    return;
  }

  pxr::UsdShadeShader preview_surface = create_usd_preview_shader(
      usd_export_context, usd_material, surface_node);

  /* Handle the primary "surface" output. */
  process_inputs(
      usd_export_context, usd_material, preview_surface, surface_node, active_uvmap_name, reports);

  /* Handle the "displacement" output if it meets our requirements. */
  if (bNode *displacement_node = find_displacement_node(material)) {
    if (displacement_node->custom1 != SHD_SPACE_OBJECT) {
      CLOG_WARN(&LOG,
                "Skipping displacement. Only Object Space displacement is supported by the "
                "UsdPreviewSurface.");
      return;
    }

    bNodeSocket *sock_mid = bke::node_find_socket(*displacement_node, SOCK_IN, "Midlevel");
    bNodeSocket *sock_scale = bke::node_find_socket(*displacement_node, SOCK_IN, "Scale");
    if (sock_mid->link || sock_scale->link) {
      CLOG_WARN(&LOG, "Skipping displacement. Midlevel and Scale must be constants.");
      return;
    }

    usd_material.CreateDisplacementOutput().ConnectToSource(preview_surface.ConnectableAPI(),
                                                            usdtokens::displacement);

    bNodeSocket *sock_height = bke::node_find_socket(*displacement_node, SOCK_IN, "Height");
    if (sock_height->link) {
      process_inputs(usd_export_context,
                     usd_material,
                     preview_surface,
                     displacement_node,
                     active_uvmap_name,
                     reports);
    }
    else {
      /* The Height itself was also a constant. Odd but still valid. As there's only 1 value that
       * can be written to USD, this will be a lossy conversion upon reading back in. The reader
       * will calculate the node's parameters assuming default values for Midlevel and Scale. */
      const float mid_value = sock_mid->default_value_typed<bNodeSocketValueFloat>()->value;
      const float scale_value = sock_scale->default_value_typed<bNodeSocketValueFloat>()->value;
      const float height_value = sock_height->default_value_typed<bNodeSocketValueFloat>()->value;
      const float displacement_value = (height_value - mid_value) * scale_value;
      const InputSpec &spec = preview_surface_input_map().lookup("Height");
      preview_surface.CreateInput(spec.input_name, spec.input_type).Set(displacement_value);
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
                                         const Material *material,
                                         const pxr::UsdShadeMaterial &usd_material)
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
    map.add_new("Normal", {usdtokens::normal, pxr::SdfValueTypeNames->Normal3f, false});
    map.add_new("Coat Weight", {usdtokens::clearcoat, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Coat Roughness",
                {usdtokens::clearcoatRoughness, pxr::SdfValueTypeNames->Float, true});
    map.add_new("Height", {usdtokens::displacement, pxr::SdfValueTypeNames->Float, false});
    return map;
  }();

  return input_map;
}

/* Find the UVMAP node input to the given texture image node and convert it
 * to a USD primvar reader shader. If no UVMAP node is found, create a primvar
 * reader for the given default uv set. The primvar reader will be attached to
 * the 'st' input of the given USD texture shader. */
static void create_uvmap_shader(const USDExporterContext &usd_export_context,
                                const bNodeLink *uvmap_link,
                                const pxr::UsdShadeMaterial &usd_material,
                                const pxr::UsdShadeInput &usd_input,
                                const std::string &active_uvmap_name,
                                ReportList *reports)

{
  const bNode *uv_node = (uvmap_link && uvmap_link->fromnode ? uvmap_link->fromnode : nullptr);

  BLI_assert(!uv_node || uv_node->type_legacy == SH_NODE_UVMAP);

  const StringRef shader_name = uv_node ? uv_node->name : "uvmap";

  pxr::UsdShadeShader uv_shader = create_usd_preview_shader(
      usd_export_context, usd_material, shader_name, SH_NODE_UVMAP);

  if (!uv_shader) {
    BKE_reportf(reports, RPT_WARNING, "%s: Couldn't create USD shader for UV map", __func__);
    return;
  }

  std::string uv_name = active_uvmap_name;
  if (uv_node && uv_node->storage) {
    const NodeShaderUVMap *shader_uv_map = static_cast<const NodeShaderUVMap *>(uv_node->storage);
    uv_name = shader_uv_map->uv_map;
  }
  if (usd_export_context.export_params.rename_uvmaps && uv_name == active_uvmap_name) {
    uv_name = usdtokens::st;
  }
  /* We need to make valid, same as was done when exporting UV primvar. */
  uv_name = make_safe_name(uv_name, usd_export_context.export_params.allow_unicode);

  uv_shader.CreateInput(usdtokens::varname, pxr::SdfValueTypeNames->String).Set(uv_name);
  usd_input.ConnectToSource(uv_shader.ConnectableAPI(), usdtokens::result);
}

static void create_transform2d_shader(const USDExporterContext &usd_export_context,
                                      bNodeLink *mapping_link,
                                      pxr::UsdShadeMaterial &usd_material,
                                      pxr::UsdShadeInput &usd_input,
                                      const std::string &uvmap_name,
                                      ReportList *reports)

{
  bNode *mapping_node = (mapping_link && mapping_link->fromnode ? mapping_link->fromnode :
                                                                  nullptr);

  BLI_assert(mapping_node && mapping_node->type_legacy == SH_NODE_MAPPING);

  if (!mapping_node) {
    return;
  }

  if (mapping_node->custom1 != TEXMAP_TYPE_POINT) {
    if (bNodeSocket *socket = bke::node_find_socket(*mapping_node, SOCK_IN, "Vector")) {
      create_uv_input(usd_export_context, socket, usd_material, usd_input, uvmap_name, reports);
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

  if (bNodeSocket *scale_socket = bke::node_find_socket(*mapping_node, SOCK_IN, "Scale")) {
    copy_v3_v3(scale, ((bNodeSocketValueVector *)scale_socket->default_value)->value);
    /* Ignore the Z scale. */
    scale[2] = 1.0f;
  }

  if (bNodeSocket *loc_socket = bke::node_find_socket(*mapping_node, SOCK_IN, "Location")) {
    copy_v3_v3(loc, ((bNodeSocketValueVector *)loc_socket->default_value)->value);
    /* Ignore the Z translation. */
    loc[2] = 0.0f;
  }

  if (bNodeSocket *rot_socket = bke::node_find_socket(*mapping_node, SOCK_IN, "Rotation")) {
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

  if (bNodeSocket *socket = bke::node_find_socket(*mapping_node, SOCK_IN, "Vector")) {
    if (pxr::UsdShadeInput in_input = transform2d_shader.CreateInput(
            usdtokens::in, pxr::SdfValueTypeNames->Float2))
    {
      create_uv_input(usd_export_context, socket, usd_material, in_input, uvmap_name, reports);
    }
  }
}

static void create_uv_input(const USDExporterContext &usd_export_context,
                            bNodeSocket *input_socket,
                            pxr::UsdShadeMaterial &usd_material,
                            pxr::UsdShadeInput &usd_input,
                            const std::string &active_uvmap_name,
                            ReportList *reports)
{
  if (!(usd_material && usd_input)) {
    return;
  }

  if (bNodeLink *mapping_link = traverse_channel(input_socket, SH_NODE_MAPPING)) {
    /* Use either "st" or active UV map name from mesh, depending if it was renamed. */
    std::string uvmap_name = (usd_export_context.export_params.rename_uvmaps) ? usdtokens::st :
                                                                                active_uvmap_name;
    create_transform2d_shader(
        usd_export_context, mapping_link, usd_material, usd_input, uvmap_name, reports);
    return;
  }

  const bNodeLink *uvmap_link = traverse_channel(input_socket, SH_NODE_UVMAP);

  /* Note that uvmap_link might be null, but create_uv_shader() can handle this case. */
  create_uvmap_shader(
      usd_export_context, uvmap_link, usd_material, usd_input, active_uvmap_name, reports);
}

static bool has_generated_tiles(const Image *ima)
{
  bool any_generated = false;
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    if ((tile->gen_flag & IMA_GEN_TILE) != 0) {
      any_generated = true;
      break;
    }
  }
  return any_generated;
}

static bool is_in_memory_texture(Image *ima)
{
  return has_generated_tiles(ima) || BKE_image_is_dirty(ima);
}

static bool is_packed_texture(const Image *ima)
{
  return BKE_image_has_packedfile(ima);
}

/* Generate a file name for an in-memory image that doesn't have a
 * filepath already defined. */
static std::string get_in_memory_texture_filename(Image *ima)
{
  bool is_dirty = BKE_image_is_dirty(ima);
  bool is_generated = has_generated_tiles(ima);
  bool is_packed = BKE_image_has_packedfile(ima);
  bool is_tiled = ima->source == IMA_SRC_TILED;
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

  /* NOTE: Any changes in packed filepath handling here should be considered alongside potential
   * changes in `export_packed_texture`. The file name returned needs to match. */
  if (is_packed && ima->filepath[0] != '\0') {
    BLI_path_split_file_part(ima->filepath, file_name, FILE_MAX);
    return file_name;
  }

  /* Use the image name for the file name. */
  STRNCPY(file_name, ima->id.name + 2);

  BKE_image_path_ext_from_imformat_ensure(file_name, sizeof(file_name), &imageFormat);

  if (is_tiled && !BKE_image_is_filename_tokenized(file_name)) {
    /* Ensure that the UDIM tag is in. */
    char file_body[FILE_MAX];
    char file_ext[FILE_MAX];
    BLI_string_split_suffix(file_name, FILE_MAX, file_body, file_ext);
    SNPRINTF(file_name, "%s.<UDIM>%s", file_body, file_ext);
  }

  return file_name;
}

static void export_in_memory_imbuf(ImBuf *imbuf,
                                   const std::string &export_dir,
                                   const char image_abs_path[FILE_MAX],
                                   const char file_name[FILE_MAX],
                                   const bool allow_overwrite,
                                   ReportList *reports)
{
  ImageFormatData imageFormat;
  BKE_image_format_from_imbuf(&imageFormat, imbuf);

  char export_path[FILE_MAX];
  BLI_path_join(export_path, FILE_MAX, export_dir.c_str(), file_name);

  if (!allow_overwrite && BLI_exists(export_path)) {
    return;
  }

  if ((BLI_path_cmp_normalized(export_path, image_abs_path) == 0) && BLI_exists(image_abs_path)) {
    /* As a precaution, don't overwrite the original path. */
    return;
  }

  CLOG_DEBUG(&LOG, "Exporting in-memory texture to '%s'", export_path);

  if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == false) {
    BKE_reportf(
        reports, RPT_WARNING, "USD export: couldn't export in-memory texture to %s", export_path);
  }
}

static void export_in_memory_texture(Image *ima,
                                     const std::string &export_dir,
                                     const bool allow_overwrite,
                                     ReportList *reports)
{
  char image_abs_path[FILE_MAX] = {};

  char file_name[FILE_MAX];
  if (ima->filepath[0]) {
    get_absolute_path(ima, image_abs_path);
    BLI_path_split_file_part(image_abs_path, file_name, FILE_MAX);
  }
  else {
    /* Use the image name for the file name. */
    std::string file = get_in_memory_texture_filename(ima);
    STRNCPY(file_name, file.c_str());
  }

  /* This image in its current state only exists in Blender memory.
   * So we have to export it. The export will keep the image state intact,
   * so the exported file will not be associated with the image. */
  if (ima->source != IMA_SRC_TILED) {
    ImBuf *imbuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
    if (!imbuf) {
      return;
    }

    export_in_memory_imbuf(imbuf, export_dir, image_abs_path, file_name, allow_overwrite, reports);
    BKE_image_release_ibuf(ima, imbuf, nullptr);
  }
  else {
    eUDIM_TILE_FORMAT tile_format;
    char *udim_pattern = nullptr;
    udim_pattern = BKE_image_get_tile_strformat(file_name, &tile_format);
    if (tile_format == UDIM_TILE_FORMAT_NONE) {
      return;
    }

    /* Save all the tiles. */
    ImageUser iuser{};
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      char tile_filepath[FILE_MAX];
      BKE_image_set_filepath_from_tile_number(
          tile_filepath, udim_pattern, tile_format, tile->tile_number);
      iuser.tile = tile->tile_number;

      ImBuf *imbuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
      if (!imbuf) {
        continue;
      }

      export_in_memory_imbuf(
          imbuf, export_dir, image_abs_path, tile_filepath, allow_overwrite, reports);
      BKE_image_release_ibuf(ima, imbuf, nullptr);
    }
    MEM_freeN(udim_pattern);
  }
}

static void export_packed_texture(Image *ima,
                                  const std::string &export_dir,
                                  const bool allow_overwrite,
                                  ReportList *reports)
{
  LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
    if (!imapf || !imapf->packedfile || !imapf->packedfile->data || !imapf->packedfile->size) {
      continue;
    }

    const PackedFile *pf = imapf->packedfile;

    char image_abs_path[FILE_MAX] = {};
    char file_name[FILE_MAX];

    if (imapf->filepath[0] != '\0') {
      /* Get the file name from the original path. */
      /* Make absolute source path. */
      STRNCPY(image_abs_path, imapf->filepath);
      USD_path_abs(
          image_abs_path, ID_BLEND_PATH_FROM_GLOBAL(&ima->id), false /* Not for import */);
      BLI_path_split_file_part(image_abs_path, file_name, FILE_MAX);
    }
    else {
      /* The following logic is taken from unpack_generate_paths() in packedFile.cc. */

      /* NOTE: we generally do not have any real way to re-create extension out of data. */
      const size_t len = STRNCPY_RLEN(file_name, ima->id.name + 2);

      /* For images ensure that the temporary filename contains tile number information as well as
       * a file extension based on the file magic. */

      enum eImbFileType ftype = eImbFileType(
          IMB_test_image_type_from_memory(static_cast<const uchar *>(pf->data), pf->size));
      if (ima->source == IMA_SRC_TILED) {
        char tile_number[6];
        SNPRINTF(tile_number, ".%d", imapf->tile_number);
        BLI_strncpy(file_name + len, tile_number, sizeof(file_name) - len);
      }
      if (ftype != IMB_FTYPE_NONE) {
        const int imtype = BKE_ftype_to_imtype(ftype, nullptr);
        BKE_image_path_ext_from_imtype_ensure(file_name, sizeof(file_name), imtype);
      }
    }

    char export_path_buf[FILE_MAX];
    BLI_path_join(export_path_buf, FILE_MAX, export_dir.c_str(), file_name);
    BLI_string_replace_char(export_path_buf, '\\', '/');

    const std::string export_path(export_path_buf);
    if (!allow_overwrite && asset_exists(export_path)) {
      return;
    }

    const std::string image_path(image_abs_path);
    if (paths_equal(export_path, image_path) && asset_exists(image_path)) {
      /* As a precaution, don't overwrite the original path. */
      return;
    }

    CLOG_DEBUG(&LOG, "Exporting packed texture to '%s'", export_path.c_str());

    write_to_path(pf->data, pf->size, export_path, reports);
  }
}

/* Get the absolute filepath of the given image.  Assumes
 * r_path result array is of length FILE_MAX. */
static void get_absolute_path(const Image *ima, char *r_path)
{
  /* Make absolute source path. */
  BLI_strncpy(r_path, ima->filepath, FILE_MAX);
  BLI_path_abs(r_path, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));
  BLI_path_normalize(r_path);
}

static pxr::TfToken get_node_tex_image_color_space(const bNode *node)
{
  if (!node->id) {
    return pxr::TfToken();
  }

  const Image *ima = reinterpret_cast<const Image *>(node->id);

  if (IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
    return usdtokens::raw;
  }
  if (IMB_colormanagement_space_name_is_srgb(ima->colorspace_settings.name)) {
    return usdtokens::sRGB;
  }

  return pxr::TfToken();
}

static pxr::TfToken get_node_tex_image_wrap(const bNode *node)
{
  if (node->type_legacy != SH_NODE_TEX_IMAGE) {
    return pxr::TfToken();
  }

  if (node->storage == nullptr) {
    return pxr::TfToken();
  }

  const NodeTexImage *tex_image = static_cast<const NodeTexImage *>(node->storage);

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
  if (linked_node->type_legacy == target_type) {
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
    if (ELEM(node->type_legacy, SH_NODE_BSDF_PRINCIPLED, SH_NODE_BSDF_DIFFUSE)) {
      return node;
    }
  }

  return nullptr;
}

/**
 * Returns the first occurrence of a scalar Displacement node found in the given
 * material's node tree. Vector Displacement is not supported in the #UsdPreviewSurface.
 * Returns null if no instance of either type was found.
 */
static bNode *find_displacement_node(Material *material)
{
  for (bNode *node : material->nodetree->all_nodes()) {
    if (node->type_legacy == SH_NODE_DISPLACEMENT) {
      return node;
    }
  }

  return nullptr;
}

/* Creates a USD Preview Surface shader based on the given cycles node name and type. */
static pxr::UsdShadeShader create_usd_preview_shader(const USDExporterContext &usd_export_context,
                                                     const pxr::UsdShadeMaterial &material,
                                                     const StringRef name,
                                                     const int type)
{
  pxr::SdfPath shader_path = material.GetPath().AppendChild(
      pxr::TfToken(make_safe_name(name, usd_export_context.export_params.allow_unicode)));
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
                                                     const pxr::UsdShadeMaterial &material,
                                                     bNode *node)
{
  pxr::UsdShadeShader shader = create_usd_preview_shader(
      usd_export_context, material, node->name, node->type_legacy);

  if (node->type_legacy != SH_NODE_TEX_IMAGE) {
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

static pxr::UsdShadeShader create_primvar_reader_shader(
    const USDExporterContext &usd_export_context,
    const pxr::UsdShadeMaterial &material,
    const pxr::TfToken &primvar_type,
    const bNode *node)
{
  pxr::SdfPath shader_path = material.GetPath().AppendChild(
      pxr::TfToken(make_safe_name(node->name, usd_export_context.export_params.allow_unicode)));
  pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(usd_export_context.stage, shader_path);

  shader.CreateIdAttr(pxr::VtValue(primvar_type));
  return shader;
}

static std::string get_tex_image_asset_filepath(const Image *ima)
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

std::string get_tex_image_asset_filepath(Image *ima,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params)
{
  std::string stage_path = stage->GetRootLayer()->GetRealPath();

  if (!ima) {
    return "";
  }

  std::string path;

  if (is_in_memory_texture(ima)) {
    path = get_in_memory_texture_filename(ima);
  }
  else {
    if (!export_params.export_textures && export_params.use_original_paths) {
      path = get_usd_source_path(&ima->id);
    }

    if (is_packed_texture(ima)) {
      if (path.empty()) {
        char file_name[FILE_MAX];
        path = get_in_memory_texture_filename(ima);
        BLI_path_join(file_name, FILE_MAX, ".", "textures", path.c_str());
        path = file_name;
      }
    }
    else if (ima->filepath[0] != '\0') {
      /* Get absolute path. */
      path = get_tex_image_asset_filepath(ima);
    }
  }

  return get_tex_image_asset_filepath(path, stage_path, export_params);
}

std::string get_tex_image_asset_filepath(const std::string &path,
                                         const std::string &stage_path,
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
    if (stage_path.empty()) {
      return path;
    }

    std::string rel_path = get_relative_path(path, stage_path);
    if (rel_path.empty()) {
      return path;
    }
    return rel_path;
  }

  return path;
}

std::string get_tex_image_asset_filepath(bNode *node,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params)
{
  Image *ima = reinterpret_cast<Image *>(node->id);
  return get_tex_image_asset_filepath(ima, stage, export_params);
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

    CLOG_DEBUG(&LOG, "Copying texture tile from '%s' to '%s'", src_tile_path, dest_tile_path);

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
static void copy_single_file(const Image *ima,
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

  CLOG_DEBUG(&LOG, "Copying texture from '%s' to '%s'", source_path, dest_path);

  /* Copy the file. */
  if (BLI_copy(source_path, dest_path) != 0) {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD export: could not copy texture from %s to %s",
                source_path,
                dest_path);
  }
}

void export_texture(Image *ima,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite,
                    ReportList *reports)
{
  std::string dest_dir = get_export_textures_dir(stage);
  if (dest_dir.empty()) {
    CLOG_ERROR(&LOG, "Couldn't determine textures directory path");
    return;
  }

  if (is_packed_texture(ima)) {
    export_packed_texture(ima, dest_dir, allow_overwrite, reports);
  }
  else if (is_in_memory_texture(ima)) {
    export_in_memory_texture(ima, dest_dir, allow_overwrite, reports);
  }
  else if (ima->source == IMA_SRC_TILED) {
    copy_tiled_textures(ima, dest_dir, allow_overwrite, reports);
  }
  else {
    copy_single_file(ima, dest_dir, allow_overwrite, reports);
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

#ifdef WITH_MATERIALX
static void export_texture(const USDExporterContext &usd_export_context, Image *ima)
{
  export_texture(ima,
                 usd_export_context.stage,
                 usd_export_context.export_params.overwrite_textures,
                 usd_export_context.export_params.worker_status->reports);
}
#endif

void export_texture(bNode *node,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite,
                    ReportList *reports)
{
  if (!ELEM(node->type_legacy, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT)) {
    return;
  }

  Image *ima = reinterpret_cast<Image *>(node->id);
  if (!ima) {
    return;
  }

  export_texture(ima, stage, allow_overwrite, reports);
}

pxr::TfToken token_for_input(const StringRef input_name)
{
  const InputSpecMap &input_map = preview_surface_input_map();
  const InputSpec *spec = input_map.lookup_ptr(input_name);

  if (spec == nullptr) {
    return {};
  }

  return spec->input_name;
}

#ifdef WITH_MATERIALX
/* A wrapper for the MaterialX code to re-use the standard Texture export code */
static std::string materialx_export_image(const USDExporterContext &usd_export_context,
                                          Main * /*main*/,
                                          Scene * /*scene*/,
                                          Image *ima,
                                          ImageUser * /*iuser*/)
{
  auto tex_path = get_tex_image_asset_filepath(
      ima, usd_export_context.stage, usd_export_context.export_params);

  export_texture(usd_export_context, ima);
  return tex_path;
}

/* Utility function to reflow connections and paths within the temporary document
 * to their final location in the USD document. */
static pxr::SdfPath reflow_materialx_paths(pxr::SdfPath input_path,
                                           pxr::SdfPath temp_path,
                                           const pxr::SdfPath &target_path,
                                           const Map<std::string, std::string> &rename_pairs)
{

  const std::string &input_path_string = input_path.GetString();
  /* First we see if the path is in the rename_pairs,
   * otherwise we check if it starts with any items in the list plus a path separator (/ or .) .
   * Checking for the path separators, removes false positives from other prefixed elements. */
  const auto *value_lookup_ptr = rename_pairs.lookup_ptr(input_path_string);
  if (value_lookup_ptr) {
    input_path = pxr::SdfPath(*value_lookup_ptr);
  }
  else {
    for (const auto &pair : rename_pairs.items()) {
      if (input_path_string.length() > pair.key.length() &&
          pxr::TfStringStartsWith(input_path_string, pair.key) &&
          (input_path_string[pair.key.length()] == '/' ||
           input_path_string[pair.key.length()] == '.'))
      {
        input_path = input_path.ReplacePrefix(pxr::SdfPath(pair.key), pxr::SdfPath(pair.value));
        break;
      }
    }
  }

  return input_path.ReplacePrefix(temp_path, target_path);
}

/* Exports the material as a MaterialX node-graph within the USD layer. */
static void create_usd_materialx_material(const USDExporterContext &usd_export_context,
                                          pxr::SdfPath usd_path,
                                          Material *material,
                                          const std::string &active_uvmap_name,
                                          const pxr::UsdShadeMaterial &usd_material)
{
  blender::nodes::materialx::ExportParams export_params = {
      /* Output surface material node will have this name. */
      usd_path.GetElementString(),
      /* We want to re-use the same MaterialX document generation code as used by the renderer.
       * While the graph is traversed, we also want it to export the textures out. */
      (usd_export_context.export_image_fn) ?
          usd_export_context.export_image_fn :
          [usd_export_context](Main *main, Scene *scene, Image *ima, ImageUser *iuser) {
            return materialx_export_image(usd_export_context, main, scene, ima, iuser);
          },
      /* Active UV map name to use for default texture coordinates. */
      (usd_export_context.export_params.rename_uvmaps) ? "st" : active_uvmap_name,
      active_uvmap_name,
  };

  MaterialX::DocumentPtr doc = blender::nodes::materialx::export_to_materialx(
      usd_export_context.depsgraph, material, export_params);

  /* We want to merge the MaterialX graph under the same Material as the USDPreviewSurface
   * This allows for the same material assignment to have two levels of complexity so other
   * applications and renderers can easily pick which one they want.
   * This does mean that we need to pre-process the resulting graph so that there are no
   * name conflicts.
   * So we first gather all the existing names in this namespace to avoid that. */
  Set<std::string> used_names;
  auto material_prim = usd_material.GetPrim();
  for (const auto &child : material_prim.GetChildren()) {
    used_names.add(child.GetName().GetString());
  }

  /* usdMtlx assumes a workflow where the mtlx file is referenced in,
   * but the resulting structure is not ideal for when the file is inlined.
   * Some of the issues include turning every shader input into a separate constant, which
   * leads to very unwieldy shader graphs in other applications. There are also extra nodes
   * that are only needed when referencing in the file that make editing the graph harder.
   * Therefore, we opt to copy just what we need over.
   *
   * To do this, we first open a temporary stage to process the structure inside */

  auto temp_stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdMtlxRead(doc, temp_stage, pxr::SdfPath("/root"));

  /* Next we need to find the Material that matches this materials name */
  auto temp_material_path = pxr::SdfPath("/root/Materials");
  temp_material_path = temp_material_path.AppendChild(material_prim.GetName());
  auto temp_material_prim = temp_stage->GetPrimAtPath(temp_material_path);
  if (!temp_material_prim) {
    return;
  }

  pxr::UsdShadeMaterial temp_material{temp_material_prim};
  if (!temp_material) {
    return;
  }

  /* Copy over the MateralXConfigAPI schema and associated attribute. */
  pxr::UsdMtlxMaterialXConfigAPI temp_config_api{temp_material_prim};
  if (temp_config_api) {
    pxr::UsdMtlxMaterialXConfigAPI materialx_config_api = pxr::UsdMtlxMaterialXConfigAPI::Apply(
        material_prim);
    pxr::UsdAttribute temp_mtlx_version_attr = temp_config_api.GetConfigMtlxVersionAttr();
    pxr::VtValue mtlx_version;
    if (temp_mtlx_version_attr && temp_mtlx_version_attr.Get(&mtlx_version)) {
      materialx_config_api.CreateConfigMtlxVersionAttr(mtlx_version);
    }
  }

  /* Once we have the material, we need to prepare for renaming any conflicts.
   * However, we must make sure any new names don't conflict with names in the temp stage either */
  Set<std::string> temp_used_names;
  for (const auto &child : temp_material_prim.GetChildren()) {
    temp_used_names.add(child.GetName().GetString());
  }

  /* We loop through the top level children of the material, and make sure that the names are
   * unique across both the destination stage, and this temporary stage.
   * This is stored for later use so that we can reflow any connections */
  Map<std::string, std::string> rename_pairs;
  for (const auto &temp_material_child : temp_material_prim.GetChildren()) {
    uint32_t conflict_counter = 0;
    const std::string &name = temp_material_child.GetName().GetString();
    std::string target_name = name;
    while (used_names.contains(target_name)) {
      ++conflict_counter;
      target_name = name + "_mtlx" + std::to_string(conflict_counter);

      while (temp_used_names.contains(target_name)) {
        ++conflict_counter;
        target_name = name + "_mtlx" + std::to_string(conflict_counter);
      }
    }

    if (conflict_counter == 0) {
      continue;
    }

    temp_used_names.add(target_name);
    const pxr::SdfPath &temp_material_child_path = temp_material_child.GetPath();
    const std::string &original_path = temp_material_child_path.GetString();
    const std::string new_path =
        temp_material_child.GetPath().ReplaceName(pxr::TfToken(target_name)).GetString();

    rename_pairs.add_overwrite(original_path, new_path);
  }

  /* We now need to find the connections from the material to the surface shader
   * and modify it to match the final target location */
  for (const auto &temp_material_output : temp_material.GetOutputs()) {
    pxr::SdfPathVector output_paths;

    temp_material_output.GetAttr().GetConnections(&output_paths);
    if (output_paths.size() == 1) {
      output_paths[0] = reflow_materialx_paths(
          output_paths[0], temp_material_path, usd_path, rename_pairs);

      auto target_material_output = usd_material.CreateOutput(temp_material_output.GetBaseName(),
                                                              temp_material_output.GetTypeName());
      target_material_output.GetAttr().SetConnections(output_paths);
    }
  }

  /* Next we need to iterate through every shader descendant recursively, to process them */
  for (const auto &temp_child : temp_material_prim.GetAllDescendants()) {
    /* We only care about shader children */
    auto temp_shader = pxr::UsdShadeShader(temp_child);
    if (!temp_shader) {
      continue;
    }

    /* First, we process any inputs */
    for (const auto &shader_input : temp_shader.GetInputs()) {
      pxr::SdfPathVector connection_paths;
      shader_input.GetAttr().GetConnections(&connection_paths);

      if (connection_paths.size() != 1) {
        continue;
      }

      const pxr::SdfPath &connection_path = connection_paths[0];

      auto connection_source = pxr::UsdShadeConnectionSourceInfo(temp_stage, connection_path);
      auto connection_source_prim = connection_source.source.GetPrim();
      if (connection_source_prim == temp_material_prim) {
        /* If it's connected to the material prim, we should just bake down the value.
         * usdMtlx connects them to constants because it wants to maximize separation between the
         * input mtlx file and the resulting graph, but this isn't the ideal structure when the
         * graph is inlined.
         * Baking the values down makes this much more usable. */
        auto connection_source_attr = temp_stage->GetAttributeAtPath(connection_path);
        if (connection_source_attr && shader_input.DisconnectSource()) {
          pxr::VtValue val;
          if (connection_source_attr.Get(&val) && !val.IsEmpty()) {
            shader_input.GetAttr().Set(val);
          }
        }
      }
      else {
        /* If it's connected to another prim, then we should fix the path to that prim
         * SdfCopySpec below will handle some cases, but only if the target path exists first
         * which is impossible to guarantee in a graph. */

        connection_paths[0] = reflow_materialx_paths(
            connection_paths[0], temp_material_path, usd_path, rename_pairs);
        shader_input.GetAttr().SetConnections(connection_paths);
      }
    }

    /* Next we iterate through the outputs */
    for (const auto &shader_output : temp_shader.GetOutputs()) {
      pxr::SdfPathVector connection_paths;
      shader_output.GetAttr().GetConnections(&connection_paths);

      if (connection_paths.size() != 1) {
        continue;
      }

      connection_paths[0] = reflow_materialx_paths(
          connection_paths[0], temp_material_path, usd_path, rename_pairs);
      shader_output.GetAttr().SetConnections(connection_paths);
    } /* Iterate through outputs */

  } /* Iterate through material prim children */

  auto temp_layer = temp_stage->Flatten();

  /* Copy the primspecs from the temporary stage over to the target stage */
  auto target_root_layer = usd_export_context.stage->GetRootLayer();
  for (const auto &temp_material_child : temp_material_prim.GetChildren()) {
    auto target_path = reflow_materialx_paths(
        temp_material_child.GetPath(), temp_material_path, usd_path, rename_pairs);
    pxr::SdfCopySpec(temp_layer, temp_material_child.GetPath(), target_root_layer, target_path);
  }
}
#endif

pxr::UsdShadeMaterial create_usd_material(const USDExporterContext &usd_export_context,
                                          pxr::SdfPath usd_path,
                                          Material *material,
                                          const std::string &active_uvmap_name,
                                          ReportList *reports)
{
  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Define(usd_export_context.stage,
                                                                     usd_path);

  if (usd_export_context.export_params.generate_preview_surface) {
    create_usd_preview_surface_material(
        usd_export_context, material, usd_material, active_uvmap_name, reports);
  }
  else {
    create_usd_viewport_material(usd_export_context, material, usd_material);
  }

#ifdef WITH_MATERIALX
  if (usd_export_context.export_params.generate_materialx_network) {
    create_usd_materialx_material(
        usd_export_context, usd_path, material, active_uvmap_name, usd_material);
  }
#endif

  call_material_export_hooks(
      usd_export_context.stage, material, usd_material, usd_export_context.export_params, reports);

  return usd_material;
}

}  // namespace blender::io::usd
