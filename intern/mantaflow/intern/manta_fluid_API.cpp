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
MANTA *manta_init(int *res, struct FluidModifierData *mmd)
{
  return new MANTA(res, mmd);
}
void manta_free(MANTA *fluid)
{
  delete fluid;
  fluid = nullptr;
}

void manta_ensure_obstacle(MANTA *fluid, struct FluidModifierData *mmd)
{
  if (!fluid)
    return;
  fluid->initObstacle(mmd);
  fluid->updatePointers();
}
void manta_ensure_guiding(MANTA *fluid, struct FluidModifierData *mmd)
{
  if (!fluid)
    return;
  fluid->initGuiding(mmd);
  fluid->updatePointers();
}
void manta_ensure_invelocity(MANTA *fluid, struct FluidModifierData *mmd)
{
  if (!fluid)
    return;
  fluid->initInVelocity(mmd);
  fluid->updatePointers();
}
void manta_ensure_outflow(MANTA *fluid, struct FluidModifierData *mmd)
{
  if (!fluid)
    return;
  fluid->initOutflow(mmd);
  fluid->updatePointers();
}

int manta_write_config(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->writeConfiguration(mmd, framenr);
}

int manta_write_data(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->writeData(mmd, framenr);
}

int manta_write_noise(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->writeNoise(mmd, framenr);
}

int manta_read_config(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readConfiguration(mmd, framenr);
}

int manta_read_data(MANTA *fluid, FluidModifierData *mmd, int framenr, bool resumable)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readData(mmd, framenr, resumable);
}

int manta_read_noise(MANTA *fluid, FluidModifierData *mmd, int framenr, bool resumable)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readNoise(mmd, framenr, resumable);
}

int manta_read_mesh(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readMesh(mmd, framenr);
}

int manta_read_particles(MANTA *fluid, FluidModifierData *mmd, int framenr, bool resumable)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readParticles(mmd, framenr, resumable);
}

int manta_read_guiding(MANTA *fluid, FluidModifierData *mmd, int framenr, bool sourceDomain)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->readGuiding(mmd, framenr, sourceDomain);
}

int manta_bake_data(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->bakeData(mmd, framenr);
}

int manta_bake_noise(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->bakeNoise(mmd, framenr);
}

int manta_bake_mesh(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->bakeMesh(mmd, framenr);
}

int manta_bake_particles(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->bakeParticles(mmd, framenr);
}

int manta_bake_guiding(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->bakeGuiding(mmd, framenr);
}

int manta_has_data(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->hasData(mmd, framenr);
}

int manta_has_noise(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->hasNoise(mmd, framenr);
}

int manta_has_mesh(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->hasMesh(mmd, framenr);
}

int manta_has_particles(MANTA *fluid, FluidModifierData *mmd, int framenr)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->hasParticles(mmd, framenr);
}

int manta_has_guiding(MANTA *fluid, FluidModifierData *mmd, int framenr, bool domain)
{
  if (!fluid || !mmd)
    return 0;
  return fluid->hasGuiding(mmd, framenr, domain);
}

void manta_update_variables(MANTA *fluid, FluidModifierData *mmd)
{
  if (!fluid)
    return;
  fluid->updateVariables(mmd);
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

bool manta_needs_realloc(MANTA *fluid, FluidModifierData *mmd)
{
  if (!fluid)
    return false;
  return fluid->needsRealloc(mmd);
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
void manta_smoke_export_script(MANTA *smoke, FluidModifierData *mmd)
{
  if (!smoke || !mmd)
    return;
  smoke->exportSmokeScript(mmd);
}

void manta_smoke_export(MANTA *smoke,
                        float *dt,
                        float *dx,
                        float **dens,
                        float **react,
                        float **flame,
                        float **fuel,
                        float **heat,
                        float **vx,
                        float **vy,
                        float **vz,
                        float **r,
                        float **g,
                        float **b,
                        int **flags,
                        float **shadow)
{
  if (dens)
    *dens = smoke->getDensity();
  if (fuel)
    *fuel = smoke->getFuel();
  if (react)
    *react = smoke->getReact();
  if (flame)
    *flame = smoke->getFlame();
  if (heat)
    *heat = smoke->getHeat();
  *vx = smoke->getVelocityX();
  *vy = smoke->getVelocityY();
  *vz = smoke->getVelocityZ();
  if (r)
    *r = smoke->getColorR();
  if (g)
    *g = smoke->getColorG();
  if (b)
    *b = smoke->getColorB();
  *flags = smoke->getFlags();
  if (shadow)
    *shadow = smoke->getShadow();
  *dt = 1;  // dummy value, not needed for smoke
  *dx = 1;  // dummy value, not needed for smoke
}

void manta_smoke_turbulence_export(MANTA *smoke,
                                   float **dens,
                                   float **react,
                                   float **flame,
                                   float **fuel,
                                   float **r,
                                   float **g,
                                   float **b,
                                   float **tcu,
                                   float **tcv,
                                   float **tcw,
                                   float **tcu2,
                                   float **tcv2,
                                   float **tcw2)
{
  if (!smoke && !(smoke->usingNoise()))
    return;

  *dens = smoke->getDensityHigh();
  if (fuel)
    *fuel = smoke->getFuelHigh();
  if (react)
    *react = smoke->getReactHigh();
  if (flame)
    *flame = smoke->getFlameHigh();
  if (r)
    *r = smoke->getColorRHigh();
  if (g)
    *g = smoke->getColorGHigh();
  if (b)
    *b = smoke->getColorBHigh();
  *tcu = smoke->getTextureU();
  *tcv = smoke->getTextureV();
  *tcw = smoke->getTextureW();

  *tcu2 = smoke->getTextureU2();
  *tcv2 = smoke->getTextureV2();
  *tcw2 = smoke->getTextureW2();
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

void manta_smoke_turbulence_get_rgba(MANTA *smoke, float *data, int sequential)
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

void manta_smoke_turbulence_get_rgba_fixed_color(MANTA *smoke,
                                                 float color[3],
                                                 float *data,
                                                 int sequential)
{
  get_rgba_fixed_color(color, smoke->getTotalCellsHigh(), data, sequential);
}

void manta_smoke_ensure_heat(MANTA *smoke, struct FluidModifierData *mmd)
{
  if (smoke) {
    smoke->initHeat(mmd);
    smoke->updatePointers();
  }
}

void manta_smoke_ensure_fire(MANTA *smoke, struct FluidModifierData *mmd)
{
  if (smoke) {
    smoke->initFire(mmd);
    if (smoke->usingNoise()) {
      smoke->initFireHigh(mmd);
    }
    smoke->updatePointers();
  }
}

void manta_smoke_ensure_colors(MANTA *smoke, struct FluidModifierData *mmd)
{
  if (smoke) {
    smoke->initColors(mmd);
    if (smoke->usingNoise()) {
      smoke->initColorsHigh(mmd);
    }
    smoke->updatePointers();
  }
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

float *manta_smoke_turbulence_get_density(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getDensityHigh() : nullptr;
}
float *manta_smoke_turbulence_get_fuel(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getFuelHigh() : nullptr;
}
float *manta_smoke_turbulence_get_react(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getReactHigh() : nullptr;
}
float *manta_smoke_turbulence_get_color_r(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorRHigh() : nullptr;
}
float *manta_smoke_turbulence_get_color_g(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorGHigh() : nullptr;
}
float *manta_smoke_turbulence_get_color_b(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getColorBHigh() : nullptr;
}
float *manta_smoke_turbulence_get_flame(MANTA *smoke)
{
  return (smoke && smoke->usingNoise()) ? smoke->getFlameHigh() : nullptr;
}

int manta_smoke_turbulence_has_fuel(MANTA *smoke)
{
  return (smoke->getFuelHigh()) ? 1 : 0;
}
int manta_smoke_turbulence_has_colors(MANTA *smoke)
{
  return (smoke->getColorRHigh() && smoke->getColorGHigh() && smoke->getColorBHigh()) ? 1 : 0;
}

void manta_smoke_turbulence_get_res(MANTA *smoke, int *res)
{
  if (smoke && smoke->usingNoise()) {
    res[0] = smoke->getResXHigh();
    res[1] = smoke->getResYHigh();
    res[2] = smoke->getResZHigh();
  }
}
int manta_smoke_turbulence_get_cells(MANTA *smoke)
{
  int total_cells_high = smoke->getResXHigh() * smoke->getResYHigh() * smoke->getResZHigh();
  return (smoke && smoke->usingNoise()) ? total_cells_high : 0;
}

/* Liquid functions */
void manta_liquid_export_script(MANTA *liquid, FluidModifierData *mmd)
{
  if (!liquid || !mmd)
    return;
  liquid->exportLiquidScript(mmd);
}

void manta_liquid_ensure_sndparts(MANTA *liquid, struct FluidModifierData *mmd)
{
  if (liquid) {
    liquid->initLiquidSndParts(mmd);
    liquid->updatePointers();
  }
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
