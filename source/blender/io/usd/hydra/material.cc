/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "material.h"

#include <Python.h>
#include <unicodeobject.h>

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/tokens.h>

#include "MEM_guardedalloc.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

#include "bpy_rna.h"

#include "hydra_scene_delegate.h"

namespace blender::io::hydra {

MaterialData::MaterialData(HydraSceneDelegate *scene_delegate,
                           Material *material,
                           pxr::SdfPath const &prim_id)
    : IdData(scene_delegate, (ID *)material, prim_id)
{
}

void MaterialData::init()
{
  ID_LOGN(1, "");
  double_sided = (((Material *)id)->blend_flag & MA_BL_CULL_BACKFACE) == 0;
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

pxr::VtValue MaterialData::get_data(pxr::TfToken const & /* key */) const
{
  return pxr::VtValue();
}

pxr::VtValue MaterialData::get_material_resource() const
{
  return pxr::VtValue();
}

pxr::HdCullStyle MaterialData::cull_style() const
{
  return double_sided ? pxr::HdCullStyle::HdCullStyleNothing : pxr::HdCullStyle::HdCullStyleBack;
}

}  // namespace blender::io::hydra
