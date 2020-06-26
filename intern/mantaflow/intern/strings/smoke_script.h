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

#include <string>

//////////////////////////////////////////////////////////////////////
// VARIABLES
//////////////////////////////////////////////////////////////////////

const std::string smoke_variables =
    "\n\
mantaMsg('Smoke variables low')\n\
preconditioner_s$ID$    = PcMGStatic\n\
using_colors_s$ID$      = $USING_COLORS$\n\
using_heat_s$ID$        = $USING_HEAT$\n\
using_fire_s$ID$        = $USING_FIRE$\n\
using_noise_s$ID$       = $USING_NOISE$\n\
vorticity_s$ID$         = $VORTICITY$\n\
buoyancy_dens_s$ID$     = float($BUOYANCY_ALPHA$) / float($FLUID_DOMAIN_SIZE$)\n\
buoyancy_heat_s$ID$     = float($BUOYANCY_BETA$) / float($FLUID_DOMAIN_SIZE$)\n\
dissolveSpeed_s$ID$     = $DISSOLVE_SPEED$\n\
using_logdissolve_s$ID$ = $USING_LOG_DISSOLVE$\n\
using_dissolve_s$ID$    = $USING_DISSOLVE$\n\
flameVorticity_s$ID$    = $FLAME_VORTICITY$\n\
burningRate_s$ID$       = $BURNING_RATE$\n\
flameSmoke_s$ID$        = $FLAME_SMOKE$\n\
ignitionTemp_s$ID$      = $IGNITION_TEMP$\n\
maxTemp_s$ID$           = $MAX_TEMP$\n\
flameSmokeColor_s$ID$   = vec3($FLAME_SMOKE_COLOR_X$,$FLAME_SMOKE_COLOR_Y$,$FLAME_SMOKE_COLOR_Z$)\n";

const std::string smoke_variables_noise =
    "\n\
mantaMsg('Smoke variables noise')\n\
wltStrength_s$ID$ = $WLT_STR$\n\
uvs_s$ID$         = 2\n\
uvs_offset_s$ID$  = vec3($MIN_RESX$, $MIN_RESY$, $MIN_RESZ$)\n\
octaves_s$ID$     = int(math.log(upres_sn$ID$) / math.log(2.0) + 0.5) if (upres_sn$ID$ > 1) else 1\n";

const std::string smoke_wavelet_noise =
    "\n\
# wavelet noise params\n\
wltnoise_sn$ID$.posScale = vec3(int($BASE_RESX$), int($BASE_RESY$), int($BASE_RESZ$)) * (1. / $NOISE_POSSCALE$)\n\
wltnoise_sn$ID$.timeAnim = $NOISE_TIMEANIM$\n";

const std::string smoke_with_heat =
    "\n\
using_heat_s$ID$ = True\n";

const std::string smoke_with_colors =
    "\n\
using_colors_s$ID$ = True\n";

const std::string smoke_with_fire =
    "\n\
using_fire_s$ID$ = True\n";

//////////////////////////////////////////////////////////////////////
// GRIDS
//////////////////////////////////////////////////////////////////////

const std::string smoke_alloc =
    "\n\
mantaMsg('Smoke alloc')\n\
shadow_s$ID$     = s$ID$.create(RealGrid, name='$NAME_SHADOW$')\n\
emission_s$ID$   = s$ID$.create(RealGrid, name='$NAME_EMISSION$')\n\
emissionIn_s$ID$ = s$ID$.create(RealGrid, name='$NAME_EMISSIONIN$')\n\
density_s$ID$    = s$ID$.create(RealGrid, name='$NAME_DENSITY$')\n\
densityIn_s$ID$  = s$ID$.create(RealGrid, name='$NAME_DENSITYIN$')\n\
heat_s$ID$       = None # allocated dynamically\n\
heatIn_s$ID$     = None\n\
flame_s$ID$      = None\n\
fuel_s$ID$       = None\n\
react_s$ID$      = None\n\
fuelIn_s$ID$     = None\n\
reactIn_s$ID$    = None\n\
color_r_s$ID$    = None\n\
color_g_s$ID$    = None\n\
color_b_s$ID$    = None\n\
color_r_in_s$ID$ = None\n\
color_g_in_s$ID$ = None\n\
color_b_in_s$ID$ = None\n\
\n\
# Keep track of important objects in dict to load them later on\n\
smoke_data_dict_final_s$ID$ = { 'density' : density_s$ID$, 'shadow' : shadow_s$ID$ }\n\
smoke_data_dict_resume_s$ID$ = { 'densityIn' : densityIn_s$ID$, 'emission' : emission_s$ID$ }\n";

const std::string smoke_alloc_noise =
    "\n\
mantaMsg('Smoke alloc noise')\n\
vel_sn$ID$        = sn$ID$.create(MACGrid, name='$NAME_VELOCITY_NOISE$')\n\
density_sn$ID$    = sn$ID$.create(RealGrid, name='$NAME_DENSITY_NOISE$')\n\
phiIn_sn$ID$      = sn$ID$.create(LevelsetGrid, name='$NAME_PHIIN_NOISE$')\n\
phiOut_sn$ID$     = sn$ID$.create(LevelsetGrid, name='$NAME_PHIOUT_NOISE$')\n\
phiObs_sn$ID$     = sn$ID$.create(LevelsetGrid, name='$NAME_PHIOBS_NOISE$')\n\
flags_sn$ID$      = sn$ID$.create(FlagGrid, name='$NAME_FLAGS_NOISE$')\n\
tmpIn_sn$ID$      = sn$ID$.create(RealGrid, name='$NAME_TMPIN_NOISE$')\n\
emissionIn_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_EMISSIONIN_NOISE$')\n\
energy_s$ID$      = s$ID$.create(RealGrid, name='$NAME_ENERGY$')\n\
tmpFlags_s$ID$    = s$ID$.create(FlagGrid, name='$NAME_TMPFLAGS$')\n\
texture_u_s$ID$   = s$ID$.create(RealGrid, name='$NAME_TEXTURE_U$')\n\
texture_v_s$ID$   = s$ID$.create(RealGrid, name='$NAME_TEXTURE_V$')\n\
texture_w_s$ID$   = s$ID$.create(RealGrid, name='$NAME_TEXTURE_W$')\n\
texture_u2_s$ID$  = s$ID$.create(RealGrid, name='$NAME_TEXTURE_U2$')\n\
texture_v2_s$ID$  = s$ID$.create(RealGrid, name='$NAME_TEXTURE_V2$')\n\
texture_w2_s$ID$  = s$ID$.create(RealGrid, name='$NAME_TEXTURE_W2$')\n\
flame_sn$ID$      = None\n\
fuel_sn$ID$       = None\n\
react_sn$ID$      = None\n\
color_r_sn$ID$    = None\n\
color_g_sn$ID$    = None\n\
color_b_sn$ID$    = None\n\
wltnoise_sn$ID$   = sn$ID$.create(NoiseField, fixedSeed=265, loadFromFile=True)\n\
\n\
mantaMsg('Initializing UV Grids')\n\
uvGrid0_s$ID$ = s$ID$.create(VecGrid, name='$NAME_UV0$')\n\
uvGrid1_s$ID$ = s$ID$.create(VecGrid, name='$NAME_UV1$')\n\
resetUvGrid(target=uvGrid0_s$ID$, offset=uvs_offset_s$ID$)\n\
resetUvGrid(target=uvGrid1_s$ID$, offset=uvs_offset_s$ID$)\n\
\n\
# Sync UV and texture grids\n\
copyVec3ToReal(source=uvGrid0_s$ID$, targetX=texture_u_s$ID$, targetY=texture_v_s$ID$, targetZ=texture_w_s$ID$)\n\
copyVec3ToReal(source=uvGrid1_s$ID$, targetX=texture_u2_s$ID$, targetY=texture_v2_s$ID$, targetZ=texture_w2_s$ID$)\n\
\n\
# Keep track of important objects in dict to load them later on\n\
smoke_noise_dict_final_s$ID$ = dict(density_noise=density_sn$ID$)\n\
smoke_noise_dict_resume_s$ID$ = dict(uv0_noise=uvGrid0_s$ID$, uv1_noise=uvGrid1_s$ID$)\n";

//////////////////////////////////////////////////////////////////////
// ADDITIONAL GRIDS
//////////////////////////////////////////////////////////////////////

const std::string smoke_alloc_colors =
    "\n\
# Sanity check, clear grids first\n\
if 'color_r_s$ID$' in globals(): del color_r_s$ID$\n\
if 'color_g_s$ID$' in globals(): del color_g_s$ID$\n\
if 'color_b_s$ID$' in globals(): del color_b_s$ID$\n\
\n\
mantaMsg('Allocating colors')\n\
color_r_s$ID$    = s$ID$.create(RealGrid, name='$NAME_COLORR$')\n\
color_g_s$ID$    = s$ID$.create(RealGrid, name='$NAME_COLORG$')\n\
color_b_s$ID$    = s$ID$.create(RealGrid, name='$NAME_COLORB$')\n\
color_r_in_s$ID$ = s$ID$.create(RealGrid, name='$NAME_COLORRIN$')\n\
color_g_in_s$ID$ = s$ID$.create(RealGrid, name='$NAME_COLORGIN$')\n\
color_b_in_s$ID$ = s$ID$.create(RealGrid, name='$NAME_COLORBIN$')\n\
\n\
# Add objects to dict to load them later on\n\
if 'smoke_data_dict_final_s$ID$' in globals():\n\
    smoke_data_dict_final_s$ID$.update(color_r=color_r_s$ID$, color_g=color_g_s$ID$, color_b=color_b_s$ID$)\n\
if 'smoke_data_dict_resume_s$ID$' in globals():\n\
    smoke_data_dict_resume_s$ID$.update(color_r_in=color_r_in_s$ID$, color_g_in=color_g_in_s$ID$, color_b_in=color_b_in_s$ID$)\n";

const std::string smoke_alloc_colors_noise =
    "\n\
# Sanity check, clear grids first\n\
if 'color_r_sn$ID$' in globals(): del color_r_sn$ID$\n\
if 'color_g_sn$ID$' in globals(): del color_g_sn$ID$\n\
if 'color_b_sn$ID$' in globals(): del color_b_sn$ID$\n\
\n\
mantaMsg('Allocating colors noise')\n\
color_r_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_COLORR_NOISE$')\n\
color_g_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_COLORG_NOISE$')\n\
color_b_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_COLORB_NOISE$')\n\
\n\
# Add objects to dict to load them later on\n\
if 'smoke_noise_dict_final_s$ID$' in globals():\n\
    smoke_noise_dict_final_s$ID$.update(color_r_noise=color_r_sn$ID$, color_g_noise=color_g_sn$ID$, color_b_noise=color_b_sn$ID$)\n";

const std::string smoke_init_colors =
    "\n\
mantaMsg('Initializing colors')\n\
color_r_s$ID$.copyFrom(density_s$ID$) \n\
color_r_s$ID$.multConst($COLOR_R$) \n\
color_g_s$ID$.copyFrom(density_s$ID$) \n\
color_g_s$ID$.multConst($COLOR_G$) \n\
color_b_s$ID$.copyFrom(density_s$ID$) \n\
color_b_s$ID$.multConst($COLOR_B$)\n";

const std::string smoke_init_colors_noise =
    "\n\
mantaMsg('Initializing colors noise')\n\
color_r_sn$ID$.copyFrom(density_sn$ID$) \n\
color_r_sn$ID$.multConst($COLOR_R$) \n\
color_g_sn$ID$.copyFrom(density_sn$ID$) \n\
color_g_sn$ID$.multConst($COLOR_G$) \n\
color_b_sn$ID$.copyFrom(density_sn$ID$) \n\
color_b_sn$ID$.multConst($COLOR_B$)\n";

const std::string smoke_alloc_heat =
    "\n\
# Sanity check, clear grids first\n\
if 'heat_s$ID$' in globals(): del heat_s$ID$\n\
if 'heatIn_s$ID$' in globals(): del heatIn_s$ID$\n\
\n\
mantaMsg('Allocating heat')\n\
heat_s$ID$   = s$ID$.create(RealGrid, name='$NAME_TEMPERATURE$')\n\
heatIn_s$ID$ = s$ID$.create(RealGrid, name='$NAME_TEMPERATUREIN$')\n\
\n\
# Add objects to dict to load them later on\n\
if 'smoke_data_dict_final_s$ID$' in globals():\n\
    smoke_data_dict_final_s$ID$.update(heat=heat_s$ID$)\n\
if 'smoke_data_dict_resume_s$ID$' in globals():\n\
    smoke_data_dict_resume_s$ID$.update(heatIn=heatIn_s$ID$)\n";

const std::string smoke_alloc_fire =
    "\n\
# Sanity check, clear grids first\n\
if 'flame_s$ID$' in globals(): del flame_s$ID$\n\
if 'fuel_s$ID$' in globals(): del fuel_s$ID$\n\
if 'react_s$ID$' in globals(): del react_s$ID$\n\
if 'fuelIn_s$ID$' in globals(): del fuelIn_s$ID$\n\
if 'reactIn_s$ID$' in globals(): del reactIn_s$ID$\n\
\n\
mantaMsg('Allocating fire')\n\
flame_s$ID$   = s$ID$.create(RealGrid, name='$NAME_FLAME$')\n\
fuel_s$ID$    = s$ID$.create(RealGrid, name='$NAME_FUEL$')\n\
react_s$ID$   = s$ID$.create(RealGrid, name='$NAME_REACT$')\n\
fuelIn_s$ID$  = s$ID$.create(RealGrid, name='$NAME_FUELIN$')\n\
reactIn_s$ID$ = s$ID$.create(RealGrid, name='$NAME_REACTIN$')\n\
\n\
# Add objects to dict to load them later on\n\
if 'smoke_data_dict_final_s$ID$' in globals():\n\
    smoke_data_dict_final_s$ID$.update(flame=flame_s$ID$)\n\
if 'smoke_data_dict_resume_s$ID$' in globals():\n\
    smoke_data_dict_resume_s$ID$.update(fuel=fuel_s$ID$, react=react_s$ID$, fuelIn=fuelIn_s$ID$, reactIn=reactIn_s$ID$)\n";

const std::string smoke_alloc_fire_noise =
    "\n\
# Sanity check, clear grids first\n\
if 'flame_sn$ID$' in globals(): del flame_sn$ID$\n\
if 'fuel_sn$ID$' in globals(): del fuel_sn$ID$\n\
if 'react_sn$ID$' in globals(): del react_sn$ID$\n\
\n\
mantaMsg('Allocating fire noise')\n\
flame_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_FLAME_NOISE$')\n\
fuel_sn$ID$  = sn$ID$.create(RealGrid, name='$NAME_FUEL_NOISE$')\n\
react_sn$ID$ = sn$ID$.create(RealGrid, name='$NAME_REACT_NOISE$')\n\
\n\
# Add objects to dict to load them later on\n\
if 'smoke_noise_dict_final_s$ID$' in globals():\n\
    smoke_noise_dict_final_s$ID$.update(flame_noise=flame_sn$ID$)\n\
if 'smoke_noise_dict_resume_s$ID$' in globals():\n\
    smoke_noise_dict_resume_s$ID$.update(fuel_noise=fuel_sn$ID$, react_noise=react_sn$ID$)\n";

//////////////////////////////////////////////////////////////////////
// STEP FUNCTIONS
//////////////////////////////////////////////////////////////////////

const std::string smoke_adaptive_step =
    "\n\
def smoke_adaptive_step_$ID$(framenr):\n\
    mantaMsg('Manta step, frame ' + str(framenr))\n\
    s$ID$.frame = framenr\n\
    \n\
    fluid_pre_step_$ID$()\n\
    \n\
    flags_s$ID$.initDomain(boundaryWidth=0, phiWalls=phiObs_s$ID$, outflow=boundConditions_s$ID$)\n\
    \n\
    if using_obstacle_s$ID$:\n\
        mantaMsg('Initializing obstacle levelset')\n\
        phiObsIn_s$ID$.join(phiObsSIn_s$ID$) # Join static obstacle map\n\
        phiObsIn_s$ID$.fillHoles(maxDepth=int(res_s$ID$), boundaryWidth=1)\n\
        extrapolateLsSimple(phi=phiObsIn_s$ID$, distance=6, inside=True)\n\
        extrapolateLsSimple(phi=phiObsIn_s$ID$, distance=3, inside=False)\n\
        phiObs_s$ID$.join(phiObsIn_s$ID$)\n\
        \n\
        # Using boundaryWidth=2 to not search beginning from walls (just a performance optimization)\n\
        # Additional sanity check: fill holes in phiObs which can result after joining with phiObsIn\n\
        phiObs_s$ID$.fillHoles(maxDepth=int(res_s$ID$), boundaryWidth=1)\n\
        extrapolateLsSimple(phi=phiObs_s$ID$, distance=6, inside=True)\n\
        extrapolateLsSimple(phi=phiObs_s$ID$, distance=3, inside=False)\n\
    \n\
    mantaMsg('Initializing fluid levelset')\n\
    phiIn_s$ID$.join(phiSIn_s$ID$) # Join static flow map\n\
    extrapolateLsSimple(phi=phiIn_s$ID$, distance=6, inside=True)\n\
    extrapolateLsSimple(phi=phiIn_s$ID$, distance=3, inside=False)\n\
    \n\
    if using_outflow_s$ID$:\n\
        phiOutIn_s$ID$.join(phiOutSIn_s$ID$) # Join static outflow map\n\
        phiOut_s$ID$.join(phiOutIn_s$ID$)\n\
    \n\
    setObstacleFlags(flags=flags_s$ID$, phiObs=phiObs_s$ID$, phiOut=phiOut_s$ID$, phiIn=phiIn_s$ID$, boundaryWidth=1)\n\
    flags_s$ID$.fillGrid()\n\
    \n\
    # reset emission accumulation at the beginning of an adaptive frame\n\
    if not s$ID$.timePerFrame:\n\
        emission_s$ID$.setConst(0.)\n\
    # accumulate emission value per adaptive step for later use in noise computation\n\
    emission_s$ID$.join(emissionIn_s$ID$)\n\
    \n\
    applyEmission(flags=flags_s$ID$, target=density_s$ID$, source=densityIn_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
    if using_heat_s$ID$:\n\
        applyEmission(flags=flags_s$ID$, target=heat_s$ID$, source=heatIn_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    if using_colors_s$ID$:\n\
        applyEmission(flags=flags_s$ID$, target=color_r_s$ID$, source=color_r_in_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
        applyEmission(flags=flags_s$ID$, target=color_g_s$ID$, source=color_g_in_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
        applyEmission(flags=flags_s$ID$, target=color_b_s$ID$, source=color_b_in_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    if using_fire_s$ID$:\n\
        applyEmission(flags=flags_s$ID$, target=fuel_s$ID$, source=fuelIn_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
        applyEmission(flags=flags_s$ID$, target=react_s$ID$, source=reactIn_s$ID$, emissionTexture=emissionIn_s$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    mantaMsg('Smoke step / s$ID$.frame: ' + str(s$ID$.frame))\n\
    if using_fire_s$ID$:\n\
        process_burn_$ID$()\n\
    smoke_step_$ID$()\n\
    if using_fire_s$ID$:\n\
        update_flame_$ID$()\n\
    \n\
    s$ID$.step()\n\
    \n\
    fluid_post_step_$ID$()\n";

const std::string smoke_step =
    "\n\
def smoke_step_$ID$():\n\
    mantaMsg('Smoke step low')\n\
    \n\
    # save original state for later (used during noise creation)\n\
    velTmp_s$ID$.copyFrom(vel_s$ID$)\n\
    \n\
    if using_dissolve_s$ID$:\n\
        mantaMsg('Dissolving smoke')\n\
        dissolveSmoke(flags=flags_s$ID$, density=density_s$ID$, heat=heat_s$ID$, red=color_r_s$ID$, green=color_g_s$ID$, blue=color_b_s$ID$, speed=dissolveSpeed_s$ID$, logFalloff=using_logdissolve_s$ID$)\n\
    \n\
    mantaMsg('Advecting density')\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=density_s$ID$, order=2)\n\
    \n\
    if using_heat_s$ID$:\n\
        mantaMsg('Advecting heat')\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=heat_s$ID$, order=2)\n\
    \n\
    if using_fire_s$ID$:\n\
        mantaMsg('Advecting fire')\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=fuel_s$ID$, order=2)\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=react_s$ID$, order=2)\n\
    \n\
    if using_colors_s$ID$:\n\
        mantaMsg('Advecting colors')\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=color_r_s$ID$, order=2)\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=color_g_s$ID$, order=2)\n\
        advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=color_b_s$ID$, order=2)\n\
    \n\
    mantaMsg('Advecting velocity')\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=vel_s$ID$, order=2)\n\
    \n\
    if not domainClosed_s$ID$ or using_outflow_s$ID$:\n\
        resetOutflow(flags=flags_s$ID$, real=density_s$ID$)\n\
    \n\
    mantaMsg('Vorticity')\n\
    if using_fire_s$ID$:\n\
        flame_s$ID$.copyFrom(fuel_s$ID$) # temporarily misuse flame grid as vorticity storage\n\
        flame_s$ID$.multConst(flameVorticity_s$ID$)\n\
    vorticityConfinement(vel=vel_s$ID$, flags=flags_s$ID$, strength=vorticity_s$ID$, strengthCell=flame_s$ID$ if using_fire_s$ID$ else None)\n\
    \n\
    if using_heat_s$ID$:\n\
        mantaMsg('Adding heat buoyancy')\n\
        addBuoyancy(flags=flags_s$ID$, density=heat_s$ID$, vel=vel_s$ID$, gravity=gravity_s$ID$, coefficient=buoyancy_heat_s$ID$, scale=False)\n\
    mantaMsg('Adding buoyancy')\n\
    addBuoyancy(flags=flags_s$ID$, density=density_s$ID$, vel=vel_s$ID$, gravity=gravity_s$ID$, coefficient=buoyancy_dens_s$ID$, scale=False)\n\
    \n\
    mantaMsg('Adding forces')\n\
    addForceField(flags=flags_s$ID$, vel=vel_s$ID$, force=forces_s$ID$)\n\
    \n\
    if using_obstacle_s$ID$:\n\
        mantaMsg('Extrapolating object velocity')\n\
        # ensure velocities inside of obs object, slightly add obvels outside of obs object\n\
        extrapolateVec3Simple(vel=obvelC_s$ID$, phi=phiObsIn_s$ID$, distance=6, inside=True)\n\
        extrapolateVec3Simple(vel=obvelC_s$ID$, phi=phiObsIn_s$ID$, distance=3, inside=False)\n\
        resampleVec3ToMac(source=obvelC_s$ID$, target=obvel_s$ID$)\n\
    \n\
    # Cells inside obstacle should not contain any density, fire, etc.\n\
    if deleteInObstacle_s$ID$:\n\
        resetInObstacle(flags=flags_s$ID$, density=density_s$ID$, vel=vel_s$ID$, heat=heat_s$ID$, fuel=fuel_s$ID$, flame=flame_s$ID$, red=color_r_s$ID$, green=color_g_s$ID$, blue=color_b_s$ID$)\n\
    \n\
    # add initial velocity\n\
    if using_invel_s$ID$:\n\
        resampleVec3ToMac(source=invelC_s$ID$, target=invel_s$ID$)\n\
        setInitialVelocity(flags=flags_s$ID$, vel=vel_s$ID$, invel=invel_s$ID$)\n\
    \n\
    mantaMsg('Walls')\n\
    setWallBcs(flags=flags_s$ID$, vel=vel_s$ID$, obvel=obvel_s$ID$ if using_obstacle_s$ID$ else None)\n\
    \n\
    preconditioner_s$ID$ = PcMGDynamic if using_obstacle_s$ID$ and obvel_s$ID$.getMax() > 0 else PcMGStatic\n\
    mantaMsg('Using preconditioner: ' + str(preconditioner_s$ID$))\n\
    if using_guiding_s$ID$:\n\
        mantaMsg('Guiding and pressure')\n\
        PD_fluid_guiding(vel=vel_s$ID$, velT=velT_s$ID$, flags=flags_s$ID$, weight=weightGuide_s$ID$, blurRadius=beta_sg$ID$, pressure=pressure_s$ID$, tau=tau_sg$ID$, sigma=sigma_sg$ID$, theta=theta_sg$ID$, preconditioner=preconditioner_s$ID$, zeroPressureFixing=domainClosed_s$ID$)\n\
    else:\n\
        mantaMsg('Pressure')\n\
        solvePressure(flags=flags_s$ID$, vel=vel_s$ID$, pressure=pressure_s$ID$, preconditioner=preconditioner_s$ID$, zeroPressureFixing=domainClosed_s$ID$) # closed domains require pressure fixing\n\
\n\
def process_burn_$ID$():\n\
    mantaMsg('Process burn')\n\
    processBurn(fuel=fuel_s$ID$, density=density_s$ID$, react=react_s$ID$, red=color_r_s$ID$, green=color_g_s$ID$, blue=color_b_s$ID$, heat=heat_s$ID$, burningRate=burningRate_s$ID$, flameSmoke=flameSmoke_s$ID$, ignitionTemp=ignitionTemp_s$ID$, maxTemp=maxTemp_s$ID$, flameSmokeColor=flameSmokeColor_s$ID$)\n\
\n\
def update_flame_$ID$():\n\
    mantaMsg('Update flame')\n\
    updateFlame(react=react_s$ID$, flame=flame_s$ID$)\n";

const std::string smoke_step_noise =
    "\n\
def smoke_step_noise_$ID$(framenr):\n\
    mantaMsg('Manta step noise, frame ' + str(framenr))\n\
    sn$ID$.frame = framenr\n\
    \n\
    copyRealToVec3(sourceX=texture_u_s$ID$, sourceY=texture_v_s$ID$, sourceZ=texture_w_s$ID$, target=uvGrid0_s$ID$)\n\
    copyRealToVec3(sourceX=texture_u2_s$ID$, sourceY=texture_v2_s$ID$, sourceZ=texture_w2_s$ID$, target=uvGrid1_s$ID$)\n\
    \n\
    flags_sn$ID$.initDomain(boundaryWidth=0, phiWalls=phiObs_sn$ID$, outflow=boundConditions_s$ID$)\n\
    \n\
    mantaMsg('Interpolating grids')\n\
    # Join big obstacle levelset after initDomain() call as it overwrites everything in phiObs\n\
    if using_obstacle_s$ID$:\n\
        phiIn_sn$ID$.copyFrom(phiObsIn_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(target=phiIn_sn$ID$, source=phiObsIn_s$ID$) # mis-use phiIn_sn\n\
        phiObs_sn$ID$.join(phiIn_sn$ID$)\n\
    if using_outflow_s$ID$:\n\
        phiOut_sn$ID$.copyFrom(phiOut_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(target=phiOut_sn$ID$, source=phiOut_s$ID$)\n\
    phiIn_sn$ID$.copyFrom(phiIn_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(target=phiIn_sn$ID$, source=phiIn_s$ID$)\n\
    vel_sn$ID$.copyFrom(velTmp_s$ID$) if upres_sn$ID$ <= 1 else interpolateMACGrid(target=vel_sn$ID$, source=velTmp_s$ID$)\n\
    \n\
    setObstacleFlags(flags=flags_sn$ID$, phiObs=phiObs_sn$ID$, phiOut=phiOut_sn$ID$, phiIn=phiIn_sn$ID$, boundaryWidth=1)\n\
    flags_sn$ID$.fillGrid()\n\
    \n\
    # Interpolate emission grids and apply them to big noise grids\n\
    tmpIn_sn$ID$.copyFrom(densityIn_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=densityIn_s$ID$, target=tmpIn_sn$ID$)\n\
    emissionIn_sn$ID$.copyFrom(emission_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=emission_s$ID$, target=emissionIn_sn$ID$)\n\
    \n\
    # Higher-res noise grid needs scaled emission values\n\
    tmpIn_sn$ID$.multConst(float(upres_sn$ID$))\n\
    applyEmission(flags=flags_sn$ID$, target=density_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    if using_colors_s$ID$:\n\
        tmpIn_sn$ID$.copyFrom(color_r_in_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=color_r_in_s$ID$, target=tmpIn_sn$ID$)\n\
        applyEmission(flags=flags_sn$ID$, target=color_r_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
        tmpIn_sn$ID$.copyFrom(color_g_in_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=color_g_in_s$ID$, target=tmpIn_sn$ID$)\n\
        applyEmission(flags=flags_sn$ID$, target=color_g_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
        tmpIn_sn$ID$.copyFrom(color_b_in_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=color_b_in_s$ID$, target=tmpIn_sn$ID$)\n\
        applyEmission(flags=flags_sn$ID$, target=color_b_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    if using_fire_s$ID$:\n\
        tmpIn_sn$ID$.copyFrom(fuelIn_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=fuelIn_s$ID$, target=tmpIn_sn$ID$)\n\
        applyEmission(flags=flags_sn$ID$, target=fuel_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
        tmpIn_sn$ID$.copyFrom(reactIn_s$ID$) if upres_sn$ID$ <= 1 else interpolateGrid(source=reactIn_s$ID$, target=tmpIn_sn$ID$)\n\
        applyEmission(flags=flags_sn$ID$, target=react_sn$ID$, source=tmpIn_sn$ID$, emissionTexture=emissionIn_sn$ID$, type=FlagInflow|FlagOutflow)\n\
    \n\
    mantaMsg('Noise step / sn$ID$.frame: ' + str(sn$ID$.frame))\n\
    if using_fire_s$ID$:\n\
        process_burn_noise_$ID$()\n\
    step_noise_$ID$()\n\
    if using_fire_s$ID$:\n\
        update_flame_noise_$ID$()\n\
    \n\
    sn$ID$.step()\n\
    \n\
    copyVec3ToReal(source=uvGrid0_s$ID$, targetX=texture_u_s$ID$, targetY=texture_v_s$ID$, targetZ=texture_w_s$ID$)\n\
    copyVec3ToReal(source=uvGrid1_s$ID$, targetX=texture_u2_s$ID$, targetY=texture_v2_s$ID$, targetZ=texture_w2_s$ID$)\n\
\n\
def step_noise_$ID$():\n\
    mantaMsg('Smoke step noise')\n\
    \n\
    if using_dissolve_s$ID$:\n\
        mantaMsg('Dissolving noise')\n\
        dissolveSmoke(flags=flags_sn$ID$, density=density_sn$ID$, heat=None, red=color_r_sn$ID$, green=color_g_sn$ID$, blue=color_b_sn$ID$, speed=dissolveSpeed_s$ID$, logFalloff=using_logdissolve_s$ID$)\n\
    \n\
    mantaMsg('Advecting UVs and updating UV weight')\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=uvGrid0_s$ID$, order=2)\n\
    updateUvWeight(resetTime=sn$ID$.timestep*10.0 , index=0, numUvs=uvs_s$ID$, uv=uvGrid0_s$ID$, offset=uvs_offset_s$ID$)\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=uvGrid1_s$ID$, order=2)\n\
    updateUvWeight(resetTime=sn$ID$.timestep*10.0 , index=1, numUvs=uvs_s$ID$, uv=uvGrid1_s$ID$, offset=uvs_offset_s$ID$)\n\
    \n\
    mantaMsg('Energy')\n\
    computeEnergy(flags=flags_s$ID$, vel=vel_s$ID$, energy=energy_s$ID$)\n\
    \n\
    tmpFlags_s$ID$.copyFrom(flags_s$ID$)\n\
    extrapolateSimpleFlags(flags=flags_s$ID$, val=tmpFlags_s$ID$, distance=2, flagFrom=FlagObstacle, flagTo=FlagFluid)\n\
    extrapolateSimpleFlags(flags=tmpFlags_s$ID$, val=energy_s$ID$, distance=6, flagFrom=FlagFluid, flagTo=FlagObstacle)\n\
    computeWaveletCoeffs(energy_s$ID$)\n\
    \n\
    sStr_s$ID$ = 1.0 * wltStrength_s$ID$\n\
    sPos_s$ID$ = 2.0\n\
    \n\
    mantaMsg('Applying noise vec')\n\
    for o in range(octaves_s$ID$):\n\
        uvWeight_s$ID$ = getUvWeight(uvGrid0_s$ID$)\n\
        applyNoiseVec3(flags=flags_sn$ID$, target=vel_sn$ID$, noise=wltnoise_sn$ID$, scale=sStr_s$ID$ * uvWeight_s$ID$, scaleSpatial=sPos_s$ID$ , weight=energy_s$ID$, uv=uvGrid0_s$ID$)\n\
        uvWeight_s$ID$ = getUvWeight(uvGrid1_s$ID$)\n\
        applyNoiseVec3(flags=flags_sn$ID$, target=vel_sn$ID$, noise=wltnoise_sn$ID$, scale=sStr_s$ID$ * uvWeight_s$ID$, scaleSpatial=sPos_s$ID$ , weight=energy_s$ID$, uv=uvGrid1_s$ID$)\n\
        \n\
        sStr_s$ID$ *= 0.06 # magic kolmogorov factor \n\
        sPos_s$ID$ *= 2.0 \n\
    \n\
    for substep in range(int(upres_sn$ID$)):\n\
        if using_colors_s$ID$: \n\
            mantaMsg('Advecting colors noise')\n\
            advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=color_r_sn$ID$, order=2)\n\
            advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=color_g_sn$ID$, order=2)\n\
            advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=color_b_sn$ID$, order=2)\n\
        \n\
        if using_fire_s$ID$: \n\
            mantaMsg('Advecting fire noise')\n\
            advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=fuel_sn$ID$, order=2)\n\
            advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=react_sn$ID$, order=2)\n\
        \n\
        mantaMsg('Advecting density noise')\n\
        advectSemiLagrange(flags=flags_sn$ID$, vel=vel_sn$ID$, grid=density_sn$ID$, order=2)\n\
\n\
def process_burn_noise_$ID$():\n\
    mantaMsg('Process burn noise')\n\
    processBurn(fuel=fuel_sn$ID$, density=density_sn$ID$, react=react_sn$ID$, red=color_r_sn$ID$, green=color_g_sn$ID$, blue=color_b_sn$ID$, burningRate=burningRate_s$ID$, flameSmoke=flameSmoke_s$ID$, ignitionTemp=ignitionTemp_s$ID$, maxTemp=maxTemp_s$ID$, flameSmokeColor=flameSmokeColor_s$ID$)\n\
\n\
def update_flame_noise_$ID$():\n\
    mantaMsg('Update flame noise')\n\
    updateFlame(react=react_sn$ID$, flame=flame_sn$ID$)\n";

//////////////////////////////////////////////////////////////////////
// IMPORT
//////////////////////////////////////////////////////////////////////

const std::string smoke_load_data =
    "\n\
def smoke_load_data_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Smoke load data')\n\
    dict = { **fluid_data_dict_final_s$ID$, **fluid_data_dict_resume_s$ID$, **smoke_data_dict_final_s$ID$, **smoke_data_dict_resume_s$ID$ } if resumable else { **fluid_data_dict_final_s$ID$, **smoke_data_dict_final_s$ID$ }\n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_data_s$ID$)\n\
    \n\
    copyVec3ToReal(source=vel_s$ID$, targetX=x_vel_s$ID$, targetY=y_vel_s$ID$, targetZ=z_vel_s$ID$)\n";

const std::string smoke_load_noise =
    "\n\
def smoke_load_noise_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Smoke load noise')\n\
    dict = { **smoke_noise_dict_final_s$ID$, **smoke_noise_dict_resume_s$ID$ } if resumable else { **smoke_noise_dict_final_s$ID$ } \n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_noise_s$ID$)\n\
    \n\
    if resumable:\n\
        # Fill up xyz texture grids, important when resuming a bake\n\
        copyVec3ToReal(source=uvGrid0_s$ID$, targetX=texture_u_s$ID$, targetY=texture_v_s$ID$, targetZ=texture_w_s$ID$)\n\
        copyVec3ToReal(source=uvGrid1_s$ID$, targetX=texture_u2_s$ID$, targetY=texture_v2_s$ID$, targetZ=texture_w2_s$ID$)\n";

//////////////////////////////////////////////////////////////////////
// EXPORT
//////////////////////////////////////////////////////////////////////

const std::string smoke_save_data =
    "\n\
def smoke_save_data_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Smoke save data')\n\
    start_time = time.time()\n\
    dict = { **fluid_data_dict_final_s$ID$, **fluid_data_dict_resume_s$ID$, **smoke_data_dict_final_s$ID$, **smoke_data_dict_resume_s$ID$ } if resumable else { **fluid_data_dict_final_s$ID$, **smoke_data_dict_final_s$ID$ } \n\
    if not withMPSave or isWindows:\n\
        fluid_file_export_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_data_s$ID$)\n\
    else:\n\
        fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, file_name=file_data_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n\
    mantaMsg('--- Save: %s seconds ---' % (time.time() - start_time))\n";

const std::string smoke_save_noise =
    "\n\
def smoke_save_noise_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Smoke save noise')\n\
    dict = { **smoke_noise_dict_final_s$ID$, **smoke_noise_dict_resume_s$ID$ } if resumable else { **smoke_noise_dict_final_s$ID$ } \n\
    if not withMPSave or isWindows:\n\
        fluid_file_export_s$ID$(dict=dict, framenr=framenr, file_format=file_format, path=path, file_name=file_noise_s$ID$)\n\
    else:\n\
        fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, file_name=file_noise_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n";

//////////////////////////////////////////////////////////////////////
// STANDALONE MODE
//////////////////////////////////////////////////////////////////////

const std::string smoke_standalone =
    "\n\
# Helper function to call cache load functions\n\
def load(frame, cache_resumable):\n\
    smoke_load_data_$ID$(os.path.join(cache_dir, 'data'), frame, file_format_data, cache_resumable)\n\
    if using_noise_s$ID$:\n\
        smoke_load_noise_$ID$(os.path.join(cache_dir, 'noise'), frame, file_format_noise, cache_resumable)\n\
    if using_guiding_s$ID$:\n\
        fluid_load_guiding_$ID$(os.path.join(cache_dir, 'guiding'), frame, file_format_data)\n\
\n\
# Helper function to call step functions\n\
def step(frame):\n\
    smoke_adaptive_step_$ID$(frame)\n\
    if using_noise_s$ID$:\n\
        smoke_step_noise_$ID$(frame)\n";
