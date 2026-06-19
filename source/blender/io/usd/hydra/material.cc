/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "material.hh"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usdImaging/usdImaging/materialParamUtils.h>

#ifdef WITH_MATERIALX
#  include <pxr/usd/usdMtlx/reader.h>
#endif

#include "DEG_depsgraph_query.hh"

#include "DNA_material_types.h"

#include "CLG_log.h"

#include "image.hh"
#include "scene_index.hh"

#include "intern/usd_exporter_context.hh"
#include "intern/usd_writer_material.hh"
#include "usd_private.hh"

#ifdef WITH_MATERIALX
#  include "shader/materialx/material.h"
#endif

namespace blender::io::hydra {

static pxr::HdContainerDataSourceHandle build_material_node_data_source(
    const pxr::HdMaterialNode2 &node)
{
  /* Parameters. */
  HdContainerBuilder params;
  params.reserve(node.parameters.size());
  for (const auto &param : node.parameters) {
    params.add(param.first,
               pxr::HdMaterialNodeParameterSchema::Builder()
                   .SetValue(pxr::HdRetainedSampledDataSource::New(param.second))
                   .Build());
  }

  /* Input connections. */
  HdContainerBuilder inputs;
  inputs.reserve(node.inputConnections.size());
  for (const auto &input : node.inputConnections) {
    Vector<pxr::HdDataSourceBaseHandle> conn_values;
    conn_values.reserve(input.second.size());
    for (const pxr::HdMaterialConnection2 &conn : input.second) {
      conn_values.append(
          pxr::HdMaterialConnectionSchema::Builder()
              .SetUpstreamNodePath(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(
                  conn.upstreamNode.GetToken()))
              .SetUpstreamNodeOutputName(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(
                  conn.upstreamOutputName))
              .Build());
    }
    inputs.add(input.first,
               pxr::HdRetainedSmallVectorDataSource::New(conn_values.size(), conn_values.data()));
  }

  return pxr::HdMaterialNodeSchema::Builder()
      .SetParameters(params.build())
      .SetInputConnections(inputs.build())
      .SetNodeIdentifier(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(node.nodeTypeId))
      .Build();
}

static pxr::HdContainerDataSourceHandle build_material_network_data_source(
    const pxr::HdMaterialNetworkMap &network_map)
{
  const pxr::HdMaterialNetwork2 net2 = pxr::HdConvertToHdMaterialNetwork2(network_map);

  /* Nodes. */
  HdContainerBuilder nodes;
  nodes.reserve(net2.nodes.size());
  for (const auto &entry : net2.nodes) {
    nodes.add(entry.first.GetToken(), build_material_node_data_source(entry.second));
  }

  /* Terminals. */
  HdContainerBuilder terminals;
  terminals.reserve(net2.terminals.size());
  for (const auto &entry : net2.terminals) {
    terminals.add(
        entry.first,
        pxr::HdMaterialConnectionSchema::Builder()
            .SetUpstreamNodePath(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(
                entry.second.upstreamNode.GetToken()))
            .SetUpstreamNodeOutputName(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(
                entry.second.upstreamOutputName))
            .Build());
  }

  return pxr::HdMaterialNetworkSchema::Builder()
      .SetNodes(nodes.build())
      .SetTerminals(terminals.build())
      .Build();
}

EmittedMaterial build_emitted_material(const PopulateContext &ctx, const Material *material)
{
  /* Create temporary in memory stage to avoid collisions with other materials. */
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();

  const pxr::SdfPath material_library_path("/_materials");
  const pxr::SdfPath path = ctx.material_prim_id(material);
  const pxr::SdfPath usd_material_path = material_library_path.AppendChild(
      pxr::TfToken(path.GetElementString()));

  /* Create USD export content to reuse USD file export code. */
  io::usd::USDExportParams export_params;
  export_params.relative_paths = false;
  export_params.export_textures = false; /* Don't copy all textures, is slow. */
  export_params.evaluation_mode = DEG_get_mode(ctx.depsgraph);

  const pxr::UsdTimeCode time = pxr::UsdTimeCode::Default();
  auto get_time_code = [time]() { return time; };

  io::usd::USDExporterContext export_context{ctx.bmain,
                                             ctx.depsgraph,
                                             stage,
                                             material_library_path,
                                             get_time_code,
                                             export_params,
                                             io::usd::image_cache_file_path(),
                                             cache_or_get_image_file};
  /* Create USD material. */
  pxr::UsdShadeMaterial usd_material;
#ifdef WITH_MATERIALX
  if (ctx.use_materialx) {
    const std::string material_name = pxr::TfMakeValidIdentifier(material->id.name);
    nodes::materialx::ExportParams materialx_export_params{
        material_name, cache_or_get_image_file, "st", "UVMap"};
    MaterialX::DocumentPtr doc = nodes::materialx::export_to_materialx(
        ctx.depsgraph, material, materialx_export_params);
    pxr::UsdMtlxRead(doc, stage);

    if (pxr::UsdPrim materials = stage->GetPrimAtPath(pxr::SdfPath("/MaterialX/Materials"))) {
      pxr::UsdPrimSiblingRange children = materials.GetChildren();
      if (!children.empty()) {
        usd_material = pxr::UsdShadeMaterial(*children.begin());
      }
    }
    if (!usd_material) {
      CLOG_WARN(LOG_HYDRA_SCENE_INDEX, "MaterialX export failed for '%s'", material->id.name + 2);
    }
  }
  else
#endif
  {
    usd_material = io::usd::create_usd_material(
        export_context, usd_material_path, material, "st", nullptr);
  }

  EmittedMaterial entry;
  entry.path = path;
  /* Double side is a geometry level parameter in USD. */
  entry.double_sided = (material->blend_flag & MA_BL_CULL_BACKFACE) == 0;

  if (!usd_material) {
    return entry;
  }

  /* Build the material network from the surface, volume and displacement terminals. */
  const std::vector<pxr::TfToken> render_contexts(ctx.material_render_contexts.begin(),
                                                  ctx.material_render_contexts.end());
  const std::vector<pxr::TfToken> source_types(ctx.shader_source_types.begin(),
                                               ctx.shader_source_types.end());
  pxr::HdMaterialNetworkMap network_map;
  if (pxr::UsdShadeShader surface = usd_material.ComputeSurfaceSource(render_contexts)) {
    pxr::UsdImagingBuildHdMaterialNetworkFromTerminal(surface.GetPrim(),
                                                      pxr::HdMaterialTerminalTokens->surface,
                                                      source_types,
                                                      render_contexts,
                                                      &network_map,
                                                      time);
  }
  if (pxr::UsdShadeShader volume = usd_material.ComputeVolumeSource(render_contexts)) {
    pxr::UsdImagingBuildHdMaterialNetworkFromTerminal(volume.GetPrim(),
                                                      pxr::HdMaterialTerminalTokens->volume,
                                                      source_types,
                                                      render_contexts,
                                                      &network_map,
                                                      time);
  }
  if (pxr::UsdShadeShader displacement = usd_material.ComputeDisplacementSource(render_contexts)) {
    pxr::UsdImagingBuildHdMaterialNetworkFromTerminal(displacement.GetPrim(),
                                                      pxr::HdMaterialTerminalTokens->displacement,
                                                      source_types,
                                                      render_contexts,
                                                      &network_map,
                                                      time);
  }

  if (network_map.terminals.empty()) {
    return entry;
  }

  /* Publish the network under every render context plus the universal fallback.
   * Blender does have solid viewport material parameters but Hydra is only used for
   * the rendered display mode. */
  pxr::HdContainerDataSourceHandle network_ds = build_material_network_data_source(network_map);
  HdContainerBuilder b;
  b.reserve(render_contexts.size() + 1);
  for (const pxr::TfToken &c : render_contexts) {
    b.add(c, network_ds);
  }
  b.add(pxr::HdMaterialSchemaTokens->universalRenderContext, network_ds);
  entry.data_source = pxr::HdRetainedContainerDataSource::New(
      pxr::HdMaterialSchema::GetSchemaToken(), b.build());
  return entry;
}

}  // namespace blender::io::hydra
