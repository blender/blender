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

#include "simulation_solver_influences.hh"

#include "DNA_object_types.h"

namespace blender::sim {

ParticleForce::~ParticleForce()
{
}

ParticleEmitter::~ParticleEmitter()
{
}

ParticleAction::~ParticleAction()
{
}

ParticleEvent::~ParticleEvent()
{
}

DependencyAnimations::~DependencyAnimations()
{
}

bool DependencyAnimations::is_object_transform_changing(Object &UNUSED(object)) const
{
  return false;
}

void DependencyAnimations::get_object_transforms(Object &object,
                                                 Span<float> simulation_times,
                                                 MutableSpan<float4x4> r_transforms) const
{
  assert_same_size(simulation_times, r_transforms);
  float4x4 world_matrix = object.obmat;
  r_transforms.fill(world_matrix);
}

}  // namespace blender::sim
