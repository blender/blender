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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup mantaflow
 */

#include <cmath>

#include "MANTA_main.h"
#include "manta_fluid_API.h"

/* Fluid functions */
MANTA *manta_init(int *res, struct FluidModifierData *fmd)
{
  return new MANTA(res, fmd);
}
void manta_free(MANTA *fluid)
{
  delete fluid;
  fluid = nullptr;
}

int manta_ensure_obstacle(MANTA *fluid, struct FluidModifierData *fmd)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->initObstacle(fmd);
}
int manta_ensure_guiding(MANTA *fluid, struct FluidModifierData *fmd)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->initGuiding(fmd);
}
int manta_ensure_invelocity(MANTA *fluid, struct FluidModifierData *fmd)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->initInVelocity(fmd);
}
int manta_ensure_outflow(MANTA *fluid, struct FluidModifierData *fmd)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->initOutflow(fmd);
}

int manta_write_config(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->writeConfiguration(fmd, framenr);
}

int manta_write_data(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->writeData(fmd, framenr);
}

int manta_write_noise(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->writeNoise(fmd, framenr);
}

int manta_read_config(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readConfiguration(fmd, framenr);
}

int manta_read_data(MANTA *fluid, FluidModifierData *fmd, int framenr, bool resumable)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readData(fmd, framenr, resumable);
}

int manta_read_noise(MANTA *fluid, FluidModifierData *fmd, int framenr, bool resumable)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readNoise(fmd, framenr, resumable);
}

int manta_read_mesh(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readMesh(fmd, framenr);
}

int manta_read_particles(MANTA *fluid, FluidModifierData *fmd, int framenr, bool resumable)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readParticles(fmd, framenr, resumable);
}

int manta_read_guiding(MANTA *fluid, FluidModifierData *fmd, int framenr, bool sourceDomain)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->readGuiding(fmd, framenr, sourceDomain);
}

int manta_bake_data(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->bakeData(fmd, framenr);
}

int manta_bake_noise(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->bakeNoise(fmd, framenr);
}

int manta_bake_mesh(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->bakeMesh(fmd, framenr);
}

int manta_bake_particles(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->bakeParticles(fmd, framenr);
}

int manta_bake_guiding(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->bakeGuiding(fmd, framenr);
}

int manta_has_data(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->hasData(fmd, framenr);
}

int manta_has_noise(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->hasNoise(fmd, framenr);
}

int manta_has_mesh(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->hasMesh(fmd, framenr);
}

int manta_has_particles(MANTA *fluid, FluidModifierData *fmd, int framenr)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->hasParticles(fmd, framenr);
}

int manta_has_guiding(MANTA *fluid, FluidModifierData *fmd, int framenr, bool domain)
{
  if (!fluid || !fmd)
    return 0;
  return fluid->hasGuiding(fmd, framenr, domain);
}

void manta_update_variables(MANTA *fluid, FluidModifierData *fmd)
{
  if (!fluid)
    return;
  fluid->updateVariables(fmd);
}

int manta_get_frame(MANTA *fluid)
{
  if (!fluid)
    return 0;
  return fluid->getFrame();
}

float manta_get_timestep(MANTA *fluid)
{
  if (!fluid)
    return 0;
  return fluid->getTimestep();
}

void manta_adapt_timestep(MANTA *fluid)
{
  if (!fluid)
    return;
  fluid->adaptTimestep();
}

bool manta_needs_realloc(MANTA *fluid, FluidModifierData *fmd)
{
  if (!fluid || !fmd)
    return false;
  return fluid->needsRealloc(fmd);
}

void manta_update_pointers(struct MANTA *fluid, struct FluidModifierData *fmd, bool flush)
{
  if (!fluid || !fmd)
    return;
  fluid->updatePointers(fmd, flush);
}

/* Fluid accessors */
size_t manta_get_index(int x, int max_x, int y, int max_y, int z /*, int max_z */)
{
  return x + y * max_x + z * max_x * max_y;
}
size_t manta_get_index2d(int x, int max_x, int y /*, int max_y, int z, int max_z */)
{
  return x + y * max_x;
}
float *manta_get_velocity_x(MANTA *fluid)
{
  return fluid->getVelocityX();
}
float *manta_get_velocity_y(MANTA *fluid)
{
  return fluid->getVelocityY();
}
float *manta_get_velocity_z(MANTA *fluid)
{
  return fluid->getVelocityZ();
}

float *manta_get_ob_velocity_x(MANTA *fluid)
{
  return fluid->getObVelocityX();
}
float *manta_get_ob_velocity_y(MANTA *fluid)
{
  return fluid->getObVelocityY();
}
float *manta_get_ob_velocity_z(MANTA *fluid)
{
  return fluid->getObVelocityZ();
}

float *manta_get_guide_velocity_x(MANTA *fluid)
{
  return fluid->getGuideVelocityX();
}
float *manta_get_guide_velocity_y(MANTA *fluid)
{
  return fluid->getGuideVelocityY();
}
float *manta_get_guide_velocity_z(MANTA *fluid)
{
  return fluid->getGuideVelocityZ();
}

float *manta_get_in_velocity_x(MANTA *fluid)
{
  return fluid->getInVelocityX();
}
float *manta_get_in_velocity_y(MANTA *fluid)
{
  return fluid->getInVelocityY();
}
float *manta_get_in_velocity_z(MANTA *fluid)
{
  return fluid->getInVelocityZ();
}

float *manta_get_force_x(MANTA *fluid)
{
  return fluid->getForceX();
}
float *manta_get_force_y(MANTA *fluid)
{
  return fluid->getForceY();
}
float *manta_get_force_z(MANTA *fluid)
{
  return fluid->getForceZ();
}

float *manta_get_phiguide_in(MANTA *fluid)
{
  return fluid->getPhiGuideIn();
}

float *manta_get_num_obstacle(MANTA *fluid)
{
  return fluid->getNumObstacle();
}
float *manta_get_num_guide(MANTA *fluid)
{
  return fluid->getNumGuide();
}

int manta_get_res_x(MANTA *fluid)
{
  return fluid->getResX();
}
int manta_get_res_y(MANTA *fluid)
{
  return fluid->getResY();
}
int manta_get_res_z(MANTA *fluid)
{
  return fluid->getResZ();
}

float *manta_get_phi_in(MANTA *fluid)
{
  return fluid->getPhiIn();
}
float *manta_get_phistatic_in(MANTA *fluid)
{
  return fluid->getPhiStaticIn();
}
float *manta_get_phiobs_in(MANTA *fluid)
{
  return fluid->getPhiObsIn();
}
float *manta_get_phiobsstatic_in(MANTA *fluid)
{
  return fluid->getPhiObsStaticIn();
}
float *manta_get_phiout_in(MANTA *fluid)
{
  return fluid->getPhiOutIn();
}
float *manta_get_phioutstatic_in(MANTA *fluid)
{
  return fluid->getPhiOutStaticIn();
}

/* Smoke functions */
void manta_smoke_export_script(MANTA *smoke, FluidModifierData *fmd)
{
  if (!smoke || !fmd)
    return;
  smoke->exportSmokeScript(fmd);
}

static void get_rgba(
    float *r, float *g, float *b, float *a, int total_cells, float *data, int sequential)
{
  int i;
  /* Use offsets to map RGB grids to to correct location in data grid. */
  int m = 4, i_g = 1, i_b = 2, i_a = 3;
  if (sequential) {
    m = 1;
    i_g *= total_cells;
    i_b *= total_cells;
    i_a *= total_cells;
  }

  for (i = 0; i < total_cells; i++) {
    float alpha = a[i];
    data[i * m] = r[i] * alpha;
    data[i * m + i_g] = g[i] * alpha;
    data[i * m + i_b] = b[i] * alpha;
    data[i * m + i_a] = alpha;
  }
}

void manta_smoke_get_rgba(MANTA *smoke, float *data, int sequential)
{
  get_rgba(smoke->getColorR(),
           smoke->getColorG(),
           smoke->getColorB(),
           smoke->getDensity(),
           smoke->getTotalCells(),
           data,
           sequential);
}

void manta_noise_get_rgba(MANTA *smoke, float *data, int sequential)
{
  get_rgba(smoke->getColorRHigh(),
           smoke->getColorGHigh(),
           smoke->getColorBHigh(),
           smoke->getDensityHigh(),
           smoke->getTotalCellsHigh(),
           data,
           sequential);
}

static void get_rgba_fixed_color(float color[3], int total_cells, float *data, int sequential)
{
  int i;
  int m = 4, i_g = 1, i_b = 2, i_a = 3;
  if (sequential) {
    m = 1;
    i_g *= total_cells;
    i_b *= total_cells;
    i_a *= total_cells;
  }

  for (i = 0; i < total_cells; i++) {
    data[i * m] = color[0];
    data[i * m + i_g] = color[1];
    data[i * m + i_b] = color[2];
    data[i * m + i_a] = 1.0f;
  }
}

void manta_smoke_get_rgba_fixed_color(MANTA *smoke, float color[3], float *data, int sequential)
{
  get_rgba_fixed_color(color, smoke->getTotalCells(), data, sequential);
}

void manta_noise_get_rgba_fixed_color(MANTA *smoke, float color[3], float *data, int sequential)
{
  get_rgba_fixed_color(color, smoke->getTotalCellsHigh(), data, sequential);
}

int manta_smoke_ensure_heat(MANTA *smoke, struct FluidModifierData *fmd)
{
  if (!smoke || !fmd)
    return 0;
  return smoke->initHeat(fmd);
}

int manta_smoke_ensure_fire(MANTA *smoke, struct FluidModifierData *fmd)
{
  if (!smoke || !fmd)
    return 0;

  int result = smoke->initFire(fmd);
  if (smoke->usingNoise()) {
    result &= smoke->initFireHigh(fmd);
  }
  return result;
}

int manta_smoke_ensure_colors(MANTA *smoke, struct FluidModifierData *fmd)
{
  if (!smoke || !fmd)
    return 0;

  int result = smoke->initColors(fmd);
  if (smoke->usingNoise()) {
    result &= smoke->initColorsHigh(fmd);
  }
  return result;
}

/* Smoke accessors */
float *manta_smoke_get_density(MANTA *smoke)
{
  return smoke->getDensity();
}
float *manta_smoke_get_fuel(MANTA *smoke)
{
  return smoke->getFuel();
}
float *manta_smoke_get_react(MANTA *smoke)
{
  return smoke->getReact();
}
float *manta_smoke_get_heat(MANTA *smoke)
{
  return smoke->getHeat();
}
float *manta_smoke_get_flame(MANTA *smoke)
{
  return smoke->getFlame();
}
float *manta_smoke_get_shadow(MANTA *smoke)
{
  return smoke->getShadow();
}

float *manta_smoke_get_color_r(MANTA *smoke)
{
  return smoke->getColorR();
}
float *manta_smoke_get_color_g(MANTA *smoke)
{
  return smoke->getColorG();
}
float *manta_smoke_get_color_b(MANTA *smoke)
{
  return smoke->getColorB();
}

int *manta_smoke_get_flags(MANTA *smoke)
{
  return smoke->getFlags();
}

float *manta_smoke_get_density_in(MANTA *smoke)
{
  return smoke->getDensityIn();
}
float *manta_smoke_get_heat_in(MANTA *smoke)
{
  return smoke->getHeatIn();
}
float *manta_smoke_get_color_r_in(MANTA *smoke)
{
  return smoke->getColorRIn();
}
float *manta_smoke_get_color_g_in(MANTA *smoke)
{
  return smoke->getColorGIn();
}
float *manta_smoke_get_color_b_in(MANTA *smoke)
{
  return smoke->getColorBIn();
}
float *manta_smoke_get_fuel_in(MANTA *smoke)
{
  return smoke->getFuelIn();
}
float *manta_smoke_get_react_in(MANTA *smoke)
{
  return smoke->getReactIn();
}
float *manta_smoke_get_emission_in(MANTA *smoke)
{
  return smoke->getEmissionIn();
}

int manta_smoke_has_heat(MANTA *smoke)
{
  return (smoke->getHeat()) ? 1 : 0;
}
int manta_smoke_has_fuel(MANTA *smoke)
{
  return (smoke->getFuel()) ? 1 : 0;
}
int manta_smoke_has_colors(MANTA *smoke)
{
  return (smoke->getColorR() && smoke->getColorG() && smoke->getColorB()) ? 1 : 0;
}

float *manta_noise_get_density(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getDensityHigh() : nullptr;
}
float *manta_noise_get_fuel(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getFuelHigh() : nullptr;
}
float *manta_noise_get_react(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getReactHigh() : nullptr;
}
float *manta_noise_get_color_r(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorRHigh() : nullptr;
}
float *manta_noise_get_color_g(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorGHigh() : nullptr;
}
float *manta_noise_get_color_b(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorBHigh() : nullptr;
}
float *manta_noise_get_flame(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getFlameHigh() : nullptr;
}
float *manta_noise_get_texture_u(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureU() : nullptr;
}
float *manta_noise_get_texture_v(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureV() : nullptr;
}
float *manta_noise_get_texture_w(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureW() : nullptr;
}
float *manta_noise_get_texture_u2(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureU2() : nullptr;
}
float *manta_noise_get_texture_v2(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureV2() : nullptr;
}
float *manta_noise_get_texture_w2(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getTextureW2() : nullptr;
}

int manta_noise_has_fuel(MANTA *smoke)
{
  return (smoke->getFuelHigh()) ? 1 : 0;
}
int manta_noise_has_colors(MANTA *smoke)
{
  return (smoke->getColorRHigh() && smoke->getColorGHigh() && smoke->getColorBHigh()) ? 1 : 0;
}

void manta_noise_get_res(MANTA *smoke, int *res)
{
  if (smoke && smoke->usingNoise()) {
    res[0] = smoke->getResXHigh();
    res[1] = smoke->getResYHigh();
    res[2] = smoke->getResZHigh();
  }
}
int manta_noise_get_cells(MANTA *smoke)
{
  int total_cells_high = smoke->getResXHigh() * smoke->getResYHigh() * smoke->getResZHigh();
  return (smoke && smoke->usingNoise()) ? total_cells_high : 0;
}

/* Liquid functions */
void manta_liquid_export_script(MANTA *liquid, FluidModifierData *fmd)
{
  if (!liquid || !fmd)
    return;
  liquid->exportLiquidScript(fmd);
}

int manta_liquid_ensure_sndparts(MANTA *liquid, struct FluidModifierData *fmd)
{
  if (!liquid || !fmd)
    return 0;
  return liquid->initLiquidSndParts(fmd);
}

/* Liquid accessors */
int manta_liquid_get_particle_res_x(MANTA *liquid)
{
  return liquid->getParticleResX();
}
int manta_liquid_get_particle_res_y(MANTA *liquid)
{
  return liquid->getParticleResY();
}
int manta_liquid_get_particle_res_z(MANTA *liquid)
{
  return liquid->getParticleResZ();
}

int manta_liquid_get_mesh_res_x(MANTA *liquid)
{
  return liquid->getMeshResX();
}
int manta_liquid_get_mesh_res_y(MANTA *liquid)
{
  return liquid->getMeshResY();
}
int manta_liquid_get_mesh_res_z(MANTA *liquid)
{
  return liquid->getMeshResZ();
}

int manta_liquid_get_particle_upres(MANTA *liquid)
{
  return liquid->getParticleUpres();
}
int manta_liquid_get_mesh_upres(MANTA *liquid)
{
  return liquid->getMeshUpres();
}

int manta_liquid_get_num_verts(MANTA *liquid)
{
  return liquid->getNumVertices();
}
int manta_liquid_get_num_normals(MANTA *liquid)
{
  return liquid->getNumNormals();
}
int manta_liquid_get_num_triangles(MANTA *liquid)
{
  return liquid->getNumTriangles();
}

float manta_liquid_get_vertex_x_at(MANTA *liquid, int i)
{
  return liquid->getVertexXAt(i);
}
float manta_liquid_get_vertex_y_at(MANTA *liquid, int i)
{
  return liquid->getVertexYAt(i);
}
float manta_liquid_get_vertex_z_at(MANTA *liquid, int i)
{
  return liquid->getVertexZAt(i);
}

float manta_liquid_get_normal_x_at(MANTA *liquid, int i)
{
  return liquid->getNormalXAt(i);
}
float manta_liquid_get_normal_y_at(MANTA *liquid, int i)
{
  return liquid->getNormalYAt(i);
}
float manta_liquid_get_normal_z_at(MANTA *liquid, int i)
{
  return liquid->getNormalZAt(i);
}

int manta_liquid_get_triangle_x_at(MANTA *liquid, int i)
{
  return liquid->getTriangleXAt(i);
}
int manta_liquid_get_triangle_y_at(MANTA *liquid, int i)
{
  return liquid->getTriangleYAt(i);
}
int manta_liquid_get_triangle_z_at(MANTA *liquid, int i)
{
  return liquid->getTriangleZAt(i);
}

float manta_liquid_get_vertvel_x_at(MANTA *liquid, int i)
{
  return liquid->getVertVelXAt(i);
}
float manta_liquid_get_vertvel_y_at(MANTA *liquid, int i)
{
  return liquid->getVertVelYAt(i);
}
float manta_liquid_get_vertvel_z_at(MANTA *liquid, int i)
{
  return liquid->getVertVelZAt(i);
}

int manta_liquid_get_num_flip_particles(MANTA *liquid)
{
  return liquid->getNumFlipParticles();
}
int manta_liquid_get_num_snd_particles(MANTA *liquid)
{
  return liquid->getNumSndParticles();
}

int manta_liquid_get_flip_particle_flag_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticleFlagAt(i);
}
int manta_liquid_get_snd_particle_flag_at(MANTA *liquid, int i)
{
  return liquid->getSndParticleFlagAt(i);
}

float manta_liquid_get_flip_particle_position_x_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticlePositionXAt(i);
}
float manta_liquid_get_flip_particle_position_y_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticlePositionYAt(i);
}
float manta_liquid_get_flip_particle_position_z_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticlePositionZAt(i);
}

float manta_liquid_get_flip_particle_velocity_x_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticleVelocityXAt(i);
}
float manta_liquid_get_flip_particle_velocity_y_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticleVelocityYAt(i);
}
float manta_liquid_get_flip_particle_velocity_z_at(MANTA *liquid, int i)
{
  return liquid->getFlipParticleVelocityZAt(i);
}

float manta_liquid_get_snd_particle_position_x_at(MANTA *liquid, int i)
{
  return liquid->getSndParticlePositionXAt(i);
}
float manta_liquid_get_snd_particle_position_y_at(MANTA *liquid, int i)
{
  return liquid->getSndParticlePositionYAt(i);
}
float manta_liquid_get_snd_particle_position_z_at(MANTA *liquid, int i)
{
  return liquid->getSndParticlePositionZAt(i);
}

float manta_liquid_get_snd_particle_velocity_x_at(MANTA *liquid, int i)
{
  return liquid->getSndParticleVelocityXAt(i);
}
float manta_liquid_get_snd_particle_velocity_y_at(MANTA *liquid, int i)
{
  return liquid->getSndParticleVelocityYAt(i);
}
float manta_liquid_get_snd_particle_velocity_z_at(MANTA *liquid, int i)
{
  return liquid->getSndParticleVelocityZAt(i);
}

bool manta_liquid_flip_from_file(MANTA *liquid)
{
  return liquid->usingFlipFromFile();
}
bool manta_liquid_mesh_from_file(MANTA *liquid)
{
  return liquid->usingMeshFromFile();
}
bool manta_liquid_particle_from_file(MANTA *liquid)
{
  return liquid->usingParticleFromFile();
}
