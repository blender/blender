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

#pragma once

#include "simulation_solver_influences.hh"

#include "FN_multi_function.hh"

namespace blender::sim {

class ParticleMeshEmitter final : public ParticleEmitter {
 private:
  std::string own_state_name_;
  Array<std::string> particle_names_;
  const fn::MultiFunction &inputs_fn_;
  const ParticleAction *action_;

 public:
  ParticleMeshEmitter(std::string own_state_name,
                      Array<std::string> particle_names,
                      const fn::MultiFunction &inputs_fn,
                      const ParticleAction *action)
      : own_state_name_(std::move(own_state_name)),
        particle_names_(particle_names),
        inputs_fn_(inputs_fn),
        action_(action)
  {
  }

  ~ParticleMeshEmitter();

  void emit(ParticleEmitterContext &context) const override;
};

}  // namespace blender::sim
