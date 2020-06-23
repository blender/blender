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
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_abstract.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/base/tf/stringUtils.h>

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
// Materials
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace blender {
namespace io {
namespace usd {

USDAbstractWriter::USDAbstractWriter(const USDExporterContext &usd_export_context)
    : usd_export_context_(usd_export_context),
      usd_value_writer_(),
      frame_has_been_written_(false),
      is_animated_(false)
{
}

USDAbstractWriter::~USDAbstractWriter()
{
}

bool USDAbstractWriter::is_supported(const HierarchyContext * /*context*/) const
{
  return true;
}

pxr::UsdTimeCode USDAbstractWriter::get_export_time_code() const
{
  if (is_animated_) {
    return usd_export_context_.hierarchy_iterator->get_export_time_code();
  }
  // By using the default timecode USD won't even write a single `timeSample` for non-animated
  // data. Instead, it writes it as non-timesampled.
  static pxr::UsdTimeCode default_timecode = pxr::UsdTimeCode::Default();
  return default_timecode;
}

void USDAbstractWriter::write(HierarchyContext &context)
{
  if (!frame_has_been_written_) {
    is_animated_ = usd_export_context_.export_params.export_animation &&
                   check_is_animated(context);
  }
  else if (!is_animated_) {
    /* A frame has already been written, and without animation one frame is enough. */
    return;
  }

  do_write(context);

  frame_has_been_written_ = true;
}

const pxr::SdfPath &USDAbstractWriter::usd_path() const
{
  return usd_export_context_.usd_path;
}

pxr::UsdShadeMaterial USDAbstractWriter::ensure_usd_material(Material *material)
{
  static pxr::SdfPath material_library_path("/_materials");
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  // Construct the material.
  pxr::TfToken material_name(usd_export_context_.hierarchy_iterator->get_id_name(&material->id));
  pxr::SdfPath usd_path = material_library_path.AppendChild(material_name);
  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Get(stage, usd_path);
  if (usd_material) {
    return usd_material;
  }
  usd_material = pxr::UsdShadeMaterial::Define(stage, usd_path);

  // Construct the shader.
  pxr::SdfPath shader_path = usd_path.AppendChild(usdtokens::preview_shader);
  pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(stage, shader_path);
  shader.CreateIdAttr(pxr::VtValue(usdtokens::preview_surface));
  shader.CreateInput(usdtokens::diffuse_color, pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(material->r, material->g, material->b));
  shader.CreateInput(usdtokens::roughness, pxr::SdfValueTypeNames->Float).Set(material->roughness);
  shader.CreateInput(usdtokens::metallic, pxr::SdfValueTypeNames->Float).Set(material->metallic);

  // Connect the shader and the material together.
  usd_material.CreateSurfaceOutput().ConnectToSource(shader, usdtokens::surface);

  return usd_material;
}

}  // namespace usd
}  // namespace io
}  // namespace blender
