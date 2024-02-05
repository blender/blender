/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "material.h"

#include <Python.h>
#include <unicodeobject.h>

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/materialParamUtils.h>

#ifdef WITH_MATERIALX
#  include <pxr/usd/usdMtlx/reader.h>
#  include <pxr/usd/usdMtlx/utils.h>
#endif

#include "MEM_guardedalloc.h"

#include "BKE_lib_id.hh"
#include "BKE_material.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"

#include "DEG_depsgraph_query.hh"

#include "bpy_rna.h"

#include "hydra_scene_delegate.h"
#include "image.h"

#include "intern/usd_exporter_context.hh"
#include "intern/usd_writer_material.hh"

#ifdef WITH_MATERIALX
#  include "shader/materialx/node_parser.h"

#  include "shader/materialx/material.h"
#endif

namespace blender::io::hydra {

MaterialData::MaterialData(HydraSceneDelegate *scene_delegate,
                           const Material *material,
                           pxr::SdfPath const &prim_id)
    : IdData(scene_delegate, &material->id, prim_id)
{
}

void MaterialData::init()
{
  ID_LOGN(1, "");
  double_sided = (((Material *)id)->blend_flag & MA_BL_CULL_BACKFACE) == 0;
  material_network_map_ = pxr::VtValue();

  /* Create temporary in memory stage. */
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdTimeCode time = pxr::UsdTimeCode::Default();
  auto get_time_code = [time]() { return time; };
  pxr::SdfPath material_library_path("/_materials");
  pxr::SdfPath material_path = material_library_path.AppendChild(
      pxr::TfToken(prim_id.GetElementString()));

  /* Create USD export content to reuse USD file export code. */
  USDExportParams export_params;
  export_params.relative_paths = false;
  export_params.export_textures = false; /* Don't copy all textures, is slow. */
  export_params.evaluation_mode = DEG_get_mode(scene_delegate_->depsgraph);

  usd::USDExporterContext export_context{scene_delegate_->bmain,
                                         scene_delegate_->depsgraph,
                                         stage,
                                         material_library_path,
                                         get_time_code,
                                         export_params,
                                         image_cache_file_path()};
  /* Create USD material. */
  pxr::UsdShadeMaterial usd_material;
#ifdef WITH_MATERIALX
  if (scene_delegate_->use_materialx) {
    MaterialX::DocumentPtr doc = blender::nodes::materialx::export_to_materialx(
        scene_delegate_->depsgraph, (Material *)id, cache_or_get_image_file);
    pxr::UsdMtlxRead(doc, stage);

    /* Logging stage: creating lambda stage_str() to not call stage->ExportToString()
     * if log won't be printed. */
    auto stage_str = [&stage]() {
      std::string str;
      stage->ExportToString(&str);
      return str;
    };
    ID_LOGN(2, "Stage:\n%s", stage_str().c_str());

    if (pxr::UsdPrim materials = stage->GetPrimAtPath(pxr::SdfPath("/MaterialX/Materials"))) {
      pxr::UsdPrimSiblingRange children = materials.GetChildren();
      if (!children.empty()) {
        usd_material = pxr::UsdShadeMaterial(*children.begin());
      }
    }
  }
  else
#endif
  {
    usd_material = usd::create_usd_material(
        export_context, material_path, (Material *)id, "st", nullptr);
  }

  /* Convert USD material to Hydra material network map, adapted for render delegate. */
  const pxr::HdRenderDelegate *render_delegate =
      scene_delegate_->GetRenderIndex().GetRenderDelegate();
  const pxr::TfTokenVector contextVector = render_delegate->GetMaterialRenderContexts();
  pxr::TfTokenVector shaderSourceTypes = render_delegate->GetShaderSourceTypes();

  pxr::HdMaterialNetworkMap network_map;

  if (pxr::UsdShadeShader surface = usd_material.ComputeSurfaceSource(contextVector)) {
    pxr::UsdImagingBuildHdMaterialNetworkFromTerminal(surface.GetPrim(),
                                                      pxr::HdMaterialTerminalTokens->surface,
                                                      shaderSourceTypes,
                                                      contextVector,
                                                      &network_map,
                                                      time);
  }

  material_network_map_ = pxr::VtValue(network_map);
}

void MaterialData::insert()
{
  ID_LOGN(1, "");
  scene_delegate_->GetRenderIndex().InsertSprim(
      pxr::HdPrimTypeTokens->material, scene_delegate_, prim_id);
}

void MaterialData::remove()
{
  ID_LOG(1, "");
  scene_delegate_->GetRenderIndex().RemoveSprim(pxr::HdPrimTypeTokens->material, prim_id);
}

void MaterialData::update()
{
  ID_LOGN(1, "");
  bool prev_double_sided = double_sided;
  init();
  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkSprimDirty(prim_id,
                                                                      pxr::HdMaterial::AllDirty);
  if (prev_double_sided != double_sided) {
    for (auto &obj_data : scene_delegate_->objects_.values()) {
      MeshData *m_data = dynamic_cast<MeshData *>(obj_data.get());
      if (m_data) {
        m_data->update_double_sided(this);
      }
    }
    scene_delegate_->instancer_data_->update_double_sided(this);
  }
}

pxr::VtValue MaterialData::get_data(pxr::TfToken const & /*key*/) const
{
  return pxr::VtValue();
}

pxr::VtValue MaterialData::get_material_resource() const
{
  return material_network_map_;
}

pxr::HdCullStyle MaterialData::cull_style() const
{
  return double_sided ? pxr::HdCullStyle::HdCullStyleNothing : pxr::HdCullStyle::HdCullStyleBack;
}

}  // namespace blender::io::hydra
