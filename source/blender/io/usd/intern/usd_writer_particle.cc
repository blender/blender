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
#include "usd_writer_particle.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/sphere.h>

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_particle.h"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::io::usd {

USDParticleWriter::USDParticleWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDParticleWriter::is_supported(const HierarchyContext *context) const
{
  return true;
}

void USDParticleWriter::do_write(HierarchyContext &context)
{
  ParticleSystem *psys = context.particle_system;
  ParticleSettings *psettings = psys->part;

  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  pxr::UsdTimeCode timecode = get_export_time_code();

  pxr::UsdGeomPointInstancer usd_pi =
      (usd_export_context_.export_params.export_as_overs) ?
          pxr::UsdGeomPointInstancer(
              usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
          pxr::UsdGeomPointInstancer::Define(usd_export_context_.stage,
                                             usd_export_context_.usd_path);

  // Prototypes

  Object *instance_object = psettings->instance_object;
  if (instance_object) {
    std::string full_path = "/" +
                            pxr::TfMakeValidIdentifier(std::string(instance_object->id.name + 2));
    Object *obj_parent = instance_object->parent;

    for (; obj_parent != nullptr; obj_parent = obj_parent->parent) {
      if (obj_parent != nullptr) {
        full_path = "/" + pxr::TfMakeValidIdentifier(std::string(obj_parent->id.name + 2)) +
                    full_path;
      }
    }

    pxr::SdfPath inst_path = pxr::SdfPath(
        std::string(usd_export_context_.export_params.root_prim_path) + full_path);

    pxr::UsdRelationship prototypes = usd_pi.CreatePrototypesRel();
    prototypes.AddTarget(inst_path);
  }

  pxr::UsdAttribute protoIndicesAttr = usd_pi.CreateProtoIndicesAttr();
  pxr::VtIntArray indices;

  // Attributes

  pxr::UsdAttribute positionAttr = usd_pi.CreatePositionsAttr();
  pxr::UsdAttribute scaleAttr = usd_pi.CreateScalesAttr();
  pxr::UsdAttribute orientationAttr = usd_pi.CreateOrientationsAttr();
  pxr::UsdAttribute velAttr = usd_pi.CreateVelocitiesAttr();
  pxr::UsdAttribute angVelAttr = usd_pi.CreateAngularVelocitiesAttr();
  pxr::UsdAttribute invisibleIdsAttr = usd_pi.CreateInvisibleIdsAttr();

  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtArray<pxr::GfVec3f> scales;
  pxr::VtArray<pxr::GfVec3f> velocities;
  pxr::VtArray<pxr::GfVec3f> angularVelocities;
  pxr::VtArray<pxr::GfQuath> orientations;
  pxr::VtInt64Array invisibleIndices;

  ParticleData *pa;
  int p;
  for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) {
    indices.push_back(0);

    points.push_back(pxr::GfVec3f(pa->state.co[0], pa->state.co[1], pa->state.co[2]));

    // Apply blender rand...
    float size = psys->part->size;
    size *= 1.0f - psys->part->randsize * psys_frand(psys, p + 1);
    scales.push_back(pxr::GfVec3f(size, size, size));

    orientations.push_back(
        pxr::GfQuath(pa->state.rot[0], pa->state.rot[1], pa->state.rot[2], pa->state.rot[3]));

    velocities.push_back(pxr::GfVec3f(pa->state.vel[0], pa->state.vel[1], pa->state.vel[2]));
    angularVelocities.push_back(
        pxr::GfVec3f(pa->state.ave[0], pa->state.ave[1], pa->state.ave[2]));

    if (pa->alive == PARS_DEAD || pa->alive == PARS_UNBORN) {
      invisibleIndices.push_back(
          p);  // TODO, seems to be a USD point instance problem with freezing particles
    }
  }

  // TODO: add simple particle child export
  /*if(usd_export_context_.export_params.export_child_particles) {

  }*/

  protoIndicesAttr.Set(indices, timecode);

  positionAttr.Set(points, timecode);
  scaleAttr.Set(scales, timecode);
  velAttr.Set(velocities, timecode);
  angVelAttr.Set(angularVelocities, timecode);
  orientationAttr.Set(orientations, timecode);
  invisibleIdsAttr.Set(invisibleIndices, timecode);

  if (usd_export_context_.export_params.export_custom_properties && context.particle_system) {
    auto prim = usd_pi.GetPrim();
    write_id_properties(prim, context.particle_system->part->id, timecode);
  }
}

}  // namespace blender::io::usd
