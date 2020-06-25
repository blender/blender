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

const std::string liquid_variables =
    "\n\
mantaMsg('Liquid variables')\n\
narrowBandWidth_s$ID$         = 3\n\
combineBandWidth_s$ID$        = narrowBandWidth_s$ID$ - 1\n\
adjustedNarrowBandWidth_s$ID$ = $PARTICLE_BAND_WIDTH$ # only used in adjustNumber to control band width\n\
particleNumber_s$ID$   = $PARTICLE_NUMBER$\n\
minParticles_s$ID$     = $PARTICLE_MINIMUM$\n\
maxParticles_s$ID$     = $PARTICLE_MAXIMUM$\n\
radiusFactor_s$ID$     = $PARTICLE_RADIUS$\n\
using_mesh_s$ID$       = $USING_MESH$\n\
using_final_mesh_s$ID$ = $USING_IMPROVED_MESH$\n\
using_fractions_s$ID$  = $USING_FRACTIONS$\n\
fracThreshold_s$ID$    = $FRACTIONS_THRESHOLD$\n\
flipRatio_s$ID$        = $FLIP_RATIO$\n\
concaveUpper_s$ID$     = $MESH_CONCAVE_UPPER$\n\
concaveLower_s$ID$     = $MESH_CONCAVE_LOWER$\n\
meshRadiusFactor_s$ID$ = $MESH_PARTICLE_RADIUS$\n\
smoothenPos_s$ID$      = $MESH_SMOOTHEN_POS$\n\
smoothenNeg_s$ID$      = $MESH_SMOOTHEN_NEG$\n\
randomness_s$ID$       = $PARTICLE_RANDOMNESS$\n\
surfaceTension_s$ID$   = $LIQUID_SURFACE_TENSION$\n";

const std::string liquid_variables_particles =
    "\n\
tauMin_wc_sp$ID$ = $SNDPARTICLE_TAU_MIN_WC$\n\
tauMax_wc_sp$ID$ = $SNDPARTICLE_TAU_MAX_WC$\n\
tauMin_ta_sp$ID$ = $SNDPARTICLE_TAU_MIN_TA$\n\
tauMax_ta_sp$ID$ = $SNDPARTICLE_TAU_MAX_TA$\n\
tauMin_k_sp$ID$ = $SNDPARTICLE_TAU_MIN_K$\n\
tauMax_k_sp$ID$ = $SNDPARTICLE_TAU_MAX_K$\n\
k_wc_sp$ID$ = $SNDPARTICLE_K_WC$\n\
k_ta_sp$ID$ = $SNDPARTICLE_K_TA$\n\
k_b_sp$ID$ = $SNDPARTICLE_K_B$\n\
k_d_sp$ID$ = $SNDPARTICLE_K_D$\n\
lMin_sp$ID$ = $SNDPARTICLE_L_MIN$\n\
lMax_sp$ID$ = $SNDPARTICLE_L_MAX$\n\
c_s_sp$ID$ = 0.4   # classification constant for snd parts\n\
c_b_sp$ID$ = 0.77  # classification constant for snd parts\n\
pot_radius_sp$ID$ = $SNDPARTICLE_POTENTIAL_RADIUS$\n\
update_radius_sp$ID$ = $SNDPARTICLE_UPDATE_RADIUS$\n\
using_snd_pushout_sp$ID$ = $SNDPARTICLE_BOUNDARY_PUSHOUT$\n";

//////////////////////////////////////////////////////////////////////
// GRIDS & MESH & PARTICLESYSTEM
//////////////////////////////////////////////////////////////////////

const std::string liquid_alloc =
    "\n\
mantaMsg('Liquid alloc')\n\
phiParts_s$ID$   = s$ID$.create(LevelsetGrid, name='$NAME_PHIPARTS$')\n\
phi_s$ID$        = s$ID$.create(LevelsetGrid, name='$NAME_PHI$')\n\
phiTmp_s$ID$     = s$ID$.create(LevelsetGrid, name='$NAME_PHITMP$')\n\
velOld_s$ID$     = s$ID$.create(MACGrid, name='$NAME_VELOLD$')\n\
velParts_s$ID$   = s$ID$.create(MACGrid, name='$NAME_VELPARTS$')\n\
mapWeights_s$ID$ = s$ID$.create(MACGrid, name='$NAME_MAPWEIGHTS$')\n\
fractions_s$ID$  = None # allocated dynamically\n\
curvature_s$ID$  = None\n\
\n\
pp_s$ID$         = s$ID$.create(BasicParticleSystem, name='$NAME_PARTS$')\n\
pVel_pp$ID$      = pp_s$ID$.create(PdataVec3, name='$NAME_PARTSVELOCITY$')\n\
\n\
# Acceleration data for particle nbs\n\
pindex_s$ID$     = s$ID$.create(ParticleIndexSystem, name='$NAME_PINDEX$')\n\
gpi_s$ID$        = s$ID$.create(IntGrid, name='$NAME_GPI$')\n\
\n\
# Keep track of important objects in dict to load them later on\n\
liquid_data_dict_final_s$ID$ = { 'pVel' : pVel_pp$ID$, 'pp' : pp_s$ID$ }\n\
liquid_data_dict_resume_s$ID$ = { 'phiParts' : phiParts_s$ID$, 'phi' : phi_s$ID$, 'phiTmp' : phiTmp_s$ID$ }\n";

const std::string liquid_alloc_mesh =
    "\n\
mantaMsg('Liquid alloc mesh')\n\
phiParts_sm$ID$ = sm$ID$.create(LevelsetGrid, name='$NAME_PHIPARTS_MESH$')\n\
phi_sm$ID$      = sm$ID$.create(LevelsetGrid, name='$NAME_PHI_MESH$')\n\
pp_sm$ID$       = sm$ID$.create(BasicParticleSystem, name='$NAME_PP_MESH$')\n\
flags_sm$ID$    = sm$ID$.create(FlagGrid, name='$NAME_FLAGS_MESH$')\n\
mesh_sm$ID$     = sm$ID$.create(Mesh, name='$NAME_MESH$')\n\
\n\
if using_speedvectors_s$ID$:\n\
    mVel_mesh$ID$ = mesh_sm$ID$.create(MdataVec3, name='$NAME_VELOCITYVEC_MESH$')\n\
    vel_sm$ID$    = sm$ID$.create(MACGrid, name='$NAME_VELOCITY_MESH$')\n\
\n\
# Acceleration data for particle nbs\n\
pindex_sm$ID$  = sm$ID$.create(ParticleIndexSystem, name='$NAME_PINDEX_MESH$')\n\
gpi_sm$ID$     = sm$ID$.create(IntGrid, name='$NAME_GPI_MESH$')\n\
\n\
# Set some initial values\n\
phiParts_sm$ID$.setConst(9999)\n\
phi_sm$ID$.setConst(9999)\n\
\n\
# Keep track of important objects in dict to load them later on\n\
liquid_mesh_dict_s$ID$ = { 'lMesh' : mesh_sm$ID$ }\n\
\n\
if using_speedvectors_s$ID$:\n\
    liquid_meshvel_dict_s$ID$ = { 'lVelMesh' : mVel_mesh$ID$ }\n";

const std::string liquid_alloc_curvature =
    "\n\
mantaMsg('Liquid alloc curvature')\n\
curvature_s$ID$  = s$ID$.create(RealGrid, name='$NAME_CURVATURE$')\n";

const std::string liquid_alloc_particles =
    "\n\
ppSnd_sp$ID$         = sp$ID$.create(BasicParticleSystem, name='$NAME_PARTS_PARTICLES$')\n\
pVelSnd_pp$ID$       = ppSnd_sp$ID$.create(PdataVec3, name='$NAME_PARTSVEL_PARTICLES$')\n\
pForceSnd_pp$ID$     = ppSnd_sp$ID$.create(PdataVec3, name='$NAME_PARTSFORCE_PARTICLES$')\n\
pLifeSnd_pp$ID$      = ppSnd_sp$ID$.create(PdataReal, name='$NAME_PARTSLIFE_PARTICLES$')\n\
vel_sp$ID$           = sp$ID$.create(MACGrid, name='$NAME_VELOCITY_PARTICLES$')\n\
flags_sp$ID$         = sp$ID$.create(FlagGrid, name='$NAME_FLAGS_PARTICLES$')\n\
phi_sp$ID$           = sp$ID$.create(LevelsetGrid, name='$NAME_PHI_PARTICLES$')\n\
phiObs_sp$ID$        = sp$ID$.create(LevelsetGrid, name='$NAME_PHIOBS_PARTICLES$')\n\
phiOut_sp$ID$        = sp$ID$.create(LevelsetGrid, name='$NAME_PHIOUT_PARTICLES$')\n\
normal_sp$ID$        = sp$ID$.create(VecGrid, name='$NAME_NORMAL_PARTICLES$')\n\
neighborRatio_sp$ID$ = sp$ID$.create(RealGrid, name='$NAME_NEIGHBORRATIO_PARTICLES$')\n\
trappedAir_sp$ID$    = sp$ID$.create(RealGrid, name='$NAME_TRAPPEDAIR_PARTICLES$')\n\
waveCrest_sp$ID$     = sp$ID$.create(RealGrid, name='$NAME_WAVECREST_PARTICLES$')\n\
kineticEnergy_sp$ID$ = sp$ID$.create(RealGrid, name='$NAME_KINETICENERGY_PARTICLES$')\n\
\n\
# Set some initial values\n\
phi_sp$ID$.setConst(9999)\n\
phiObs_sp$ID$.setConst(9999)\n\
phiOut_sp$ID$.setConst(9999)\n\
\n\
# Keep track of important objects in dict to load them later on\n\
liquid_particles_dict_final_s$ID$  = { 'pVelSnd' : pVelSnd_pp$ID$, 'pLifeSnd' : pLifeSnd_pp$ID$, 'ppSnd' : ppSnd_sp$ID$ }\n\
liquid_particles_dict_resume_s$ID$ = { 'trappedAir' : trappedAir_sp$ID$, 'waveCrest' : waveCrest_sp$ID$, 'kineticEnergy' : kineticEnergy_sp$ID$ }\n";

const std::string liquid_init_phi =
    "\n\
# Prepare domain\n\
phi_s$ID$.initFromFlags(flags_s$ID$)\n\
phiIn_s$ID$.initFromFlags(flags_s$ID$)\n";

//////////////////////////////////////////////////////////////////////
// STEP FUNCTIONS
//////////////////////////////////////////////////////////////////////

const std::string liquid_adaptive_step =
    "\n\
def liquid_adaptive_step_$ID$(framenr):\n\
    mantaMsg('Manta step, frame ' + str(framenr))\n\
    s$ID$.frame = framenr\n\
    \n\
    fluid_pre_step_$ID$()\n\
    \n\
    flags_s$ID$.initDomain(boundaryWidth=1 if using_fractions_s$ID$ else 0, phiWalls=phiObs_s$ID$, outflow=boundConditions_s$ID$)\n\
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
        phiObs_s$ID$.fillHoles(maxDepth=int(res_s$ID$), boundaryWidth=2 if using_fractions_s$ID$ else 1)\n\
        extrapolateLsSimple(phi=phiObs_s$ID$, distance=6, inside=True)\n\
        extrapolateLsSimple(phi=phiObs_s$ID$, distance=3)\n\
    \n\
    mantaMsg('Initializing fluid levelset')\n\
    phiIn_s$ID$.join(phiSIn_s$ID$) # Join static flow map\n\
    extrapolateLsSimple(phi=phiIn_s$ID$, distance=6, inside=True)\n\
    extrapolateLsSimple(phi=phiIn_s$ID$, distance=3)\n\
    phi_s$ID$.join(phiIn_s$ID$)\n\
    \n\
    if using_outflow_s$ID$:\n\
        phiOutIn_s$ID$.join(phiOutSIn_s$ID$) # Join static outflow map\n\
        phiOut_s$ID$.join(phiOutIn_s$ID$)\n\
    \n\
    if using_fractions_s$ID$:\n\
        updateFractions(flags=flags_s$ID$, phiObs=phiObs_s$ID$, fractions=fractions_s$ID$, boundaryWidth=boundaryWidth_s$ID$, fracThreshold=fracThreshold_s$ID$)\n\
    setObstacleFlags(flags=flags_s$ID$, phiObs=phiObs_s$ID$, phiOut=phiOut_s$ID$, fractions=fractions_s$ID$, phiIn=phiIn_s$ID$)\n\
    \n\
    if using_obstacle_s$ID$:\n\
        # TODO (sebbas): Enable flags check again, currently produces unstable particle behavior\n\
        phi_s$ID$.subtract(o=phiObsIn_s$ID$) #, flags=flags_s$ID$, subtractType=FlagObstacle)\n\
    \n\
    # add initial velocity: set invel as source grid to ensure const vels in inflow region, sampling makes use of this\n\
    if using_invel_s$ID$:\n\
        extrapolateVec3Simple(vel=invelC_s$ID$, phi=phiIn_s$ID$, distance=6, inside=True)\n\
        resampleVec3ToMac(source=invelC_s$ID$, target=invel_s$ID$)\n\
        pVel_pp$ID$.setSource(grid=invel_s$ID$, isMAC=True)\n\
    # reset pvel grid source before sampling new particles - ensures that new particles are initialized with 0 velocity\n\
    else:\n\
        pVel_pp$ID$.setSource(grid=None, isMAC=False)\n\
    \n\
    sampleLevelsetWithParticles(phi=phiIn_s$ID$, flags=flags_s$ID$, parts=pp_s$ID$, discretization=particleNumber_s$ID$, randomness=randomness_s$ID$)\n\
    flags_s$ID$.updateFromLevelset(phi_s$ID$)\n\
    \n\
    mantaMsg('Liquid step / s$ID$.frame: ' + str(s$ID$.frame))\n\
    liquid_step_$ID$()\n\
    \n\
    s$ID$.step()\n\
    \n\
    fluid_post_step_$ID$()\n";

const std::string liquid_step =
    "\n\
def liquid_step_$ID$():\n\
    mantaMsg('Liquid step')\n\
    \n\
    mantaMsg('Advecting particles')\n\
    pp_s$ID$.advectInGrid(flags=flags_s$ID$, vel=vel_s$ID$, integrationMode=IntRK4, deleteInObstacle=deleteInObstacle_s$ID$, stopInObstacle=False, skipNew=True)\n\
    \n\
    mantaMsg('Pushing particles out of obstacles')\n\
    pushOutofObs(parts=pp_s$ID$, flags=flags_s$ID$, phiObs=phiObs_s$ID$)\n\
    \n\
    # save original states for later (used during mesh / secondary particle creation)\n\
    phiTmp_s$ID$.copyFrom(phi_s$ID$)\n\
    velTmp_s$ID$.copyFrom(vel_s$ID$)\n\
    \n\
    mantaMsg('Advecting phi')\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=phi_s$ID$, order=1) # first order is usually enough\n\
    mantaMsg('Advecting velocity')\n\
    advectSemiLagrange(flags=flags_s$ID$, vel=vel_s$ID$, grid=vel_s$ID$, order=2)\n\
    \n\
    # create level set of particles\n\
    gridParticleIndex(parts=pp_s$ID$, flags=flags_s$ID$, indexSys=pindex_s$ID$, index=gpi_s$ID$)\n\
    unionParticleLevelset(parts=pp_s$ID$, indexSys=pindex_s$ID$, flags=flags_s$ID$, index=gpi_s$ID$, phi=phiParts_s$ID$, radiusFactor=radiusFactor_s$ID$)\n\
    \n\
    # combine level set of particles with grid level set\n\
    phi_s$ID$.addConst(1.) # shrink slightly\n\
    phi_s$ID$.join(phiParts_s$ID$)\n\
    extrapolateLsSimple(phi=phi_s$ID$, distance=narrowBandWidth_s$ID$+2, inside=True)\n\
    extrapolateLsSimple(phi=phi_s$ID$, distance=3)\n\
    phi_s$ID$.setBoundNeumann(0) # make sure no particles are placed at outer boundary\n\
    \n\
    if not domainClosed_s$ID$ or using_outflow_s$ID$:\n\
        resetOutflow(flags=flags_s$ID$, phi=phi_s$ID$, parts=pp_s$ID$, index=gpi_s$ID$, indexSys=pindex_s$ID$)\n\
    flags_s$ID$.updateFromLevelset(phi_s$ID$)\n\
    \n\
    # combine particles velocities with advected grid velocities\n\
    mapPartsToMAC(vel=velParts_s$ID$, flags=flags_s$ID$, velOld=velOld_s$ID$, parts=pp_s$ID$, partVel=pVel_pp$ID$, weight=mapWeights_s$ID$)\n\
    extrapolateMACFromWeight(vel=velParts_s$ID$, distance=2, weight=mapWeights_s$ID$)\n\
    combineGridVel(vel=velParts_s$ID$, weight=mapWeights_s$ID$, combineVel=vel_s$ID$, phi=phi_s$ID$, narrowBand=combineBandWidth_s$ID$, thresh=0)\n\
    velOld_s$ID$.copyFrom(vel_s$ID$)\n\
    \n\
    # forces & pressure solve\n\
    addGravity(flags=flags_s$ID$, vel=vel_s$ID$, gravity=gravity_s$ID$, scale=False)\n\
    \n\
    mantaMsg('Adding external forces')\n\
    addForceField(flags=flags_s$ID$, vel=vel_s$ID$, force=forces_s$ID$)\n\
    \n\
    if using_obstacle_s$ID$:\n\
        mantaMsg('Extrapolating object velocity')\n\
        # ensure velocities inside of obs object, slightly add obvels outside of obs object\n\
        extrapolateVec3Simple(vel=obvelC_s$ID$, phi=phiObsIn_s$ID$, distance=6, inside=True)\n\
        extrapolateVec3Simple(vel=obvelC_s$ID$, phi=phiObsIn_s$ID$, distance=3, inside=False)\n\
        resampleVec3ToMac(source=obvelC_s$ID$, target=obvel_s$ID$)\n\
    \n\
    extrapolateMACSimple(flags=flags_s$ID$, vel=vel_s$ID$, distance=2, intoObs=True if using_fractions_s$ID$ else False)\n\
    \n\
    # vel diffusion / viscosity!\n\
    if using_diffusion_s$ID$:\n\
        mantaMsg('Viscosity')\n\
        # diffusion param for solve = const * dt / dx^2\n\
        alphaV = viscosity_s$ID$ * s$ID$.timestep * float(res_s$ID$*res_s$ID$)\n\
        setWallBcs(flags=flags_s$ID$, vel=vel_s$ID$, obvel=None if using_fractions_s$ID$ else obvel_s$ID$, phiObs=phiObs_s$ID$, fractions=fractions_s$ID$)\n\
        cgSolveDiffusion(flags_s$ID$, vel_s$ID$, alphaV)\n\
        \n\
        mantaMsg('Curvature')\n\
        getLaplacian(laplacian=curvature_s$ID$, grid=phi_s$ID$)\n\
        curvature_s$ID$.clamp(-1.0, 1.0)\n\
    \n\
    setWallBcs(flags=flags_s$ID$, vel=vel_s$ID$, obvel=None if using_fractions_s$ID$ else obvel_s$ID$, phiObs=phiObs_s$ID$, fractions=fractions_s$ID$)\n\
    \n\
    if using_guiding_s$ID$:\n\
        mantaMsg('Guiding and pressure')\n\
        PD_fluid_guiding(vel=vel_s$ID$, velT=velT_s$ID$, flags=flags_s$ID$, phi=phi_s$ID$, curv=curvature_s$ID$, surfTens=surfaceTension_s$ID$, fractions=fractions_s$ID$, weight=weightGuide_s$ID$, blurRadius=beta_sg$ID$, pressure=pressure_s$ID$, tau=tau_sg$ID$, sigma=sigma_sg$ID$, theta=theta_sg$ID$, zeroPressureFixing=domainClosed_s$ID$)\n\
    else:\n\
        mantaMsg('Pressure')\n\
        solvePressure(flags=flags_s$ID$, vel=vel_s$ID$, pressure=pressure_s$ID$, phi=phi_s$ID$, curv=curvature_s$ID$, surfTens=surfaceTension_s$ID$, fractions=fractions_s$ID$, obvel=obvel_s$ID$ if using_fractions_s$ID$ else None, zeroPressureFixing=domainClosed_s$ID$)\n\
    \n\
    extrapolateMACSimple(flags=flags_s$ID$, vel=vel_s$ID$, distance=4, intoObs=True if using_fractions_s$ID$ else False)\n\
    setWallBcs(flags=flags_s$ID$, vel=vel_s$ID$, obvel=None if using_fractions_s$ID$ else obvel_s$ID$, phiObs=phiObs_s$ID$, fractions=fractions_s$ID$)\n\
    \n\
    if not using_fractions_s$ID$:\n\
        extrapolateMACSimple(flags=flags_s$ID$, vel=vel_s$ID$)\n\
    \n\
    # set source grids for resampling, used in adjustNumber!\n\
    pVel_pp$ID$.setSource(grid=vel_s$ID$, isMAC=True)\n\
    adjustNumber(parts=pp_s$ID$, vel=vel_s$ID$, flags=flags_s$ID$, minParticles=minParticles_s$ID$, maxParticles=maxParticles_s$ID$, phi=phi_s$ID$, exclude=phiObs_s$ID$, radiusFactor=radiusFactor_s$ID$, narrowBand=adjustedNarrowBandWidth_s$ID$)\n\
    flipVelocityUpdate(vel=vel_s$ID$, velOld=velOld_s$ID$, flags=flags_s$ID$, parts=pp_s$ID$, partVel=pVel_pp$ID$, flipRatio=flipRatio_s$ID$)\n";

const std::string liquid_step_mesh =
    "\n\
def liquid_step_mesh_$ID$():\n\
    mantaMsg('Liquid step mesh')\n\
    \n\
    # no upres: just use the loaded grids\n\
    if upres_sm$ID$ <= 1:\n\
        phi_sm$ID$.copyFrom(phi_s$ID$)\n\
    \n\
    # with upres: recreate grids\n\
    else:\n\
        interpolateGrid(target=phi_sm$ID$, source=phi_s$ID$)\n\
    \n\
    # create surface\n\
    pp_sm$ID$.readParticles(pp_s$ID$)\n\
    gridParticleIndex(parts=pp_sm$ID$, flags=flags_sm$ID$, indexSys=pindex_sm$ID$, index=gpi_sm$ID$)\n\
    \n\
    if using_final_mesh_s$ID$:\n\
        mantaMsg('Liquid using improved particle levelset')\n\
        improvedParticleLevelset(pp_sm$ID$, pindex_sm$ID$, flags_sm$ID$, gpi_sm$ID$, phiParts_sm$ID$, meshRadiusFactor_s$ID$, smoothenPos_s$ID$, smoothenNeg_s$ID$, concaveLower_s$ID$, concaveUpper_s$ID$)\n\
    else:\n\
        mantaMsg('Liquid using union particle levelset')\n\
        unionParticleLevelset(pp_sm$ID$, pindex_sm$ID$, flags_sm$ID$, gpi_sm$ID$, phiParts_sm$ID$, meshRadiusFactor_s$ID$)\n\
    \n\
    phi_sm$ID$.addConst(1.) # shrink slightly\n\
    phi_sm$ID$.join(phiParts_sm$ID$)\n\
    extrapolateLsSimple(phi=phi_sm$ID$, distance=narrowBandWidth_s$ID$+2, inside=True)\n\
    extrapolateLsSimple(phi=phi_sm$ID$, distance=3)\n\
    phi_sm$ID$.setBoundNeumann(0) # make sure no particles are placed at outer boundary\n\
    \n\
    # Vert vel vector needs to pull data from vel grid with correct dim\n\
    if using_speedvectors_s$ID$:\n\
        interpolateMACGrid(target=vel_sm$ID$, source=vel_s$ID$)\n\
        mVel_mesh$ID$.setSource(grid=vel_sm$ID$, isMAC=True)\n\
    \n\
    # Set 0.5 boundary at walls + account for extra wall thickness in fractions mode + account for grid scaling:\n\
    # E.g. at upres=1 we expect 1 cell border (or 2 with fractions), at upres=2 we expect 2 cell border (or 4 with fractions), etc.\n\
    # Use -1 since setBound() starts counting at 0 (and additional -1 for fractions to account for solid/fluid interface cells)\n\
    phi_sm$ID$.setBound(value=0.5, boundaryWidth=(upres_sm$ID$*2)-2 if using_fractions_s$ID$ else upres_sm$ID$-1)\n\
    phi_sm$ID$.createMesh(mesh_sm$ID$)\n";

const std::string liquid_step_particles =
    "\n\
def liquid_step_particles_$ID$():\n\
    mantaMsg('Secondary particles step')\n\
    \n\
    # no upres: just use the loaded grids\n\
    if upres_sp$ID$ <= 1:\n\
        vel_sp$ID$.copyFrom(velTmp_s$ID$)\n\
        phiObs_sp$ID$.copyFrom(phiObs_s$ID$)\n\
        phi_sp$ID$.copyFrom(phiTmp_s$ID$)\n\
        phiOut_sp$ID$.copyFrom(phiOut_s$ID$)\n\
    \n\
    # with upres: recreate grids\n\
    else:\n\
        # create highres grids by interpolation\n\
        interpolateMACGrid(target=vel_sp$ID$, source=velTmp_s$ID$)\n\
        interpolateGrid(target=phiObs_sp$ID$, source=phiObs_s$ID$)\n\
        interpolateGrid(target=phi_sp$ID$, source=phiTmp_s$ID$)\n\
        interpolateGrid(target=phiOut_sp$ID$, source=phiOut_s$ID$)\n\
    \n\
    # phiIn not needed, bwidth to 0 because we are omitting flags.initDomain()\n\
    setObstacleFlags(flags=flags_sp$ID$, phiObs=phiObs_sp$ID$, phiOut=phiOut_sp$ID$, phiIn=None, boundaryWidth=0)\n\
    flags_sp$ID$.updateFromLevelset(levelset=phi_sp$ID$)\n\
    \n\
    # Actual secondary particle simulation\n\
    flipComputeSecondaryParticlePotentials(potTA=trappedAir_sp$ID$, potWC=waveCrest_sp$ID$, potKE=kineticEnergy_sp$ID$, neighborRatio=neighborRatio_sp$ID$, flags=flags_sp$ID$, v=vel_sp$ID$, normal=normal_sp$ID$, phi=phi_sp$ID$, radius=pot_radius_sp$ID$, tauMinTA=tauMin_ta_sp$ID$, tauMaxTA=tauMax_ta_sp$ID$, tauMinWC=tauMin_wc_sp$ID$, tauMaxWC=tauMax_wc_sp$ID$, tauMinKE=tauMin_k_sp$ID$, tauMaxKE=tauMax_k_sp$ID$, scaleFromManta=ratioMetersToRes_s$ID$)\n\
    flipSampleSecondaryParticles(mode='single', flags=flags_sp$ID$, v=vel_sp$ID$, pts_sec=ppSnd_sp$ID$, v_sec=pVelSnd_pp$ID$, l_sec=pLifeSnd_pp$ID$, lMin=lMin_sp$ID$, lMax=lMax_sp$ID$, potTA=trappedAir_sp$ID$, potWC=waveCrest_sp$ID$, potKE=kineticEnergy_sp$ID$, neighborRatio=neighborRatio_sp$ID$, c_s=c_s_sp$ID$, c_b=c_b_sp$ID$, k_ta=k_ta_sp$ID$, k_wc=k_wc_sp$ID$)\n\
    flipUpdateSecondaryParticles(mode='linear', pts_sec=ppSnd_sp$ID$, v_sec=pVelSnd_pp$ID$, l_sec=pLifeSnd_pp$ID$, f_sec=pForceSnd_pp$ID$, flags=flags_sp$ID$, v=vel_sp$ID$, neighborRatio=neighborRatio_sp$ID$, radius=update_radius_sp$ID$, gravity=gravity_s$ID$, scale=False, k_b=k_b_sp$ID$, k_d=k_d_sp$ID$, c_s=c_s_sp$ID$, c_b=c_b_sp$ID$)\n\
    if using_snd_pushout_sp$ID$:\n\
        pushOutofObs(parts=ppSnd_sp$ID$, flags=flags_sp$ID$, phiObs=phiObs_sp$ID$, shift=1.0)\n\
    flipDeleteParticlesInObstacle(pts=ppSnd_sp$ID$, flags=flags_sp$ID$) # delete particles inside obstacle and outflow cells\n\
    \n\
    # Print debug information in the console\n\
    if 0:\n\
        debugGridInfo(flags=flags_sp$ID$, grid=trappedAir_sp$ID$, name='Trapped Air')\n\
        debugGridInfo(flags=flags_sp$ID$, grid=waveCrest_sp$ID$, name='Wave Crest')\n\
        debugGridInfo(flags=flags_sp$ID$, grid=kineticEnergy_sp$ID$, name='Kinetic Energy')\n";

//////////////////////////////////////////////////////////////////////
// IMPORT
//////////////////////////////////////////////////////////////////////

const std::string liquid_load_data =
    "\n\
def liquid_load_data_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Liquid load data')\n\
    dict = { **fluid_data_dict_final_s$ID$, **fluid_data_dict_resume_s$ID$, **liquid_data_dict_final_s$ID$, **liquid_data_dict_resume_s$ID$ } if resumable else { **fluid_data_dict_final_s$ID$, **liquid_data_dict_final_s$ID$ }\n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_data_s$ID$)\n\
    \n\
    copyVec3ToReal(source=vel_s$ID$, targetX=x_vel_s$ID$, targetY=y_vel_s$ID$, targetZ=z_vel_s$ID$)\n";

const std::string liquid_load_mesh =
    "\n\
def liquid_load_mesh_$ID$(path, framenr, file_format):\n\
    mantaMsg('Liquid load mesh')\n\
    dict = liquid_mesh_dict_s$ID$\n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_mesh_s$ID$)\n\
\n\
def liquid_load_meshvel_$ID$(path, framenr, file_format):\n\
    mantaMsg('Liquid load meshvel')\n\
    dict = liquid_meshvel_dict_s$ID$\n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_meshvel_s$ID$)\n";

const std::string liquid_load_particles =
    "\n\
def liquid_load_particles_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Liquid load particles')\n\
    dict = { **liquid_particles_dict_final_s$ID$, **liquid_particles_dict_resume_s$ID$ } if resumable else { **liquid_particles_dict_final_s$ID$ }\n\
    fluid_file_import_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_particles_s$ID$)\n";

//////////////////////////////////////////////////////////////////////
// EXPORT
//////////////////////////////////////////////////////////////////////

const std::string liquid_save_data =
    "\n\
def liquid_save_data_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Liquid save data')\n\
    dict = { **fluid_data_dict_final_s$ID$, **fluid_data_dict_resume_s$ID$, **liquid_data_dict_final_s$ID$, **liquid_data_dict_resume_s$ID$ } if resumable else { **fluid_data_dict_final_s$ID$, **liquid_data_dict_final_s$ID$ }\n\
    if not withMPSave or isWindows:\n\
        fluid_file_export_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_data_s$ID$)\n\
    else:\n\
        fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, file_name=file_data_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n";

const std::string liquid_save_mesh =
    "\n\
def liquid_save_mesh_$ID$(path, framenr, file_format):\n\
    mantaMsg('Liquid save mesh')\n\
    dict = liquid_mesh_dict_s$ID$\n\
    if not withMPSave or isWindows:\n\
         fluid_file_export_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_mesh_s$ID$)\n\
    else:\n\
         fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, file_name=file_mesh_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n\
\n\
def liquid_save_meshvel_$ID$(path, framenr, file_format):\n\
    mantaMsg('Liquid save mesh vel')\n\
    dict = liquid_meshvel_dict_s$ID$\n\
    if not withMPSave or isWindows:\n\
        fluid_file_export_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format)\n\
    else:\n\
        fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n";

const std::string liquid_save_particles =
    "\n\
def liquid_save_particles_$ID$(path, framenr, file_format, resumable):\n\
    mantaMsg('Liquid save particles')\n\
    dict = { **liquid_particles_dict_final_s$ID$, **liquid_particles_dict_resume_s$ID$ } if resumable else { **liquid_particles_dict_final_s$ID$ }\n\
    if not withMPSave or isWindows:\n\
        fluid_file_export_s$ID$(dict=dict, path=path, framenr=framenr, file_format=file_format, file_name=file_particles_s$ID$)\n\
    else:\n\
        fluid_cache_multiprocessing_start_$ID$(function=fluid_file_export_s$ID$, file_name=file_particles_s$ID$, framenr=framenr, format_data=file_format, path_data=path, dict=dict, do_join=False)\n";

//////////////////////////////////////////////////////////////////////
// STANDALONE MODE
//////////////////////////////////////////////////////////////////////

const std::string liquid_standalone =
    "\n\
# Helper function to call cache load functions\n\
def load(frame, cache_resumable):\n\
    liquid_load_data_$ID$(os.path.join(cache_dir, 'data'), frame, file_format_data, cache_resumable)\n\
    if using_sndparts_s$ID$:\n\
        liquid_load_particles_$ID$(os.path.join(cache_dir, 'particles'), frame, file_format_particles, cache_resumable)\n\
    if using_mesh_s$ID$:\n\
        liquid_load_mesh_$ID$(os.path.join(cache_dir, 'mesh'), frame, file_format_mesh)\n\
    if using_guiding_s$ID$:\n\
        fluid_load_guiding_$ID$(os.path.join(cache_dir, 'guiding'), frame, file_format_data)\n\
\n\
# Helper function to call step functions\n\
def step(frame):\n\
    liquid_adaptive_step_$ID$(frame)\n\
    if using_mesh_s$ID$:\n\
        liquid_step_mesh_$ID$()\n\
    if using_sndparts_s$ID$:\n\
        liquid_step_particles_$ID$()\n";
