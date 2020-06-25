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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <zlib.h>

#if OPENVDB == 1
#  include "openvdb/openvdb.h"
#endif

#include "MANTA_main.h"
#include "Python.h"
#include "fluid_script.h"
#include "liquid_script.h"
#include "manta.h"
#include "smoke_script.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::istringstream;
using std::ofstream;
using std::ostringstream;
using std::to_string;

atomic<int> MANTA::solverID(0);
int MANTA::with_debug(0);

/* Number of particles that the cache reads at once (with zlib). */
#define PARTICLE_CHUNK 20000
/* Number of mesh nodes that the cache reads at once (with zlib). */
#define NODE_CHUNK 20000
/* Number of mesh triangles that the cache reads at once (with zlib). */
#define TRIANGLE_CHUNK 20000

MANTA::MANTA(int *res, FluidModifierData *mmd) : mCurrentID(++solverID)
{
  if (with_debug)
    cout << "FLUID: " << mCurrentID << " with res(" << res[0] << ", " << res[1] << ", " << res[2]
         << ")" << endl;

  FluidDomainSettings *mds = mmd->domain;
  mds->fluid = this;

  mUsingLiquid = (mds->type == FLUID_DOMAIN_TYPE_LIQUID);
  mUsingSmoke = (mds->type == FLUID_DOMAIN_TYPE_GAS);
  mUsingNoise = (mds->flags & FLUID_DOMAIN_USE_NOISE) && mUsingSmoke;
  mUsingFractions = (mds->flags & FLUID_DOMAIN_USE_FRACTIONS) && mUsingLiquid;
  mUsingMesh = (mds->flags & FLUID_DOMAIN_USE_MESH) && mUsingLiquid;
  mUsingDiffusion = (mds->flags & FLUID_DOMAIN_USE_DIFFUSION) && mUsingLiquid;
  mUsingMVel = (mds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS) && mUsingLiquid;
  mUsingGuiding = (mds->flags & FLUID_DOMAIN_USE_GUIDE);
  mUsingDrops = (mds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY) && mUsingLiquid;
  mUsingBubbles = (mds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) && mUsingLiquid;
  mUsingFloats = (mds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) && mUsingLiquid;
  mUsingTracers = (mds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER) && mUsingLiquid;

  mUsingHeat = (mds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT) && mUsingSmoke;
  mUsingFire = (mds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE) && mUsingSmoke;
  mUsingColors = (mds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) && mUsingSmoke;
  mUsingObstacle = (mds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE);
  mUsingInvel = (mds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL);
  mUsingOutflow = (mds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW);

  // Simulation constants
  mTempAmb = 0;  // TODO: Maybe use this later for buoyancy calculation
  mResX = res[0];
  mResY = res[1];
  mResZ = res[2];
  mMaxRes = MAX3(mResX, mResY, mResZ);
  mTotalCells = mResX * mResY * mResZ;
  mResGuiding = mds->res;

  // Smoke low res grids
  mDensity = nullptr;
  mShadow = nullptr;
  mHeat = nullptr;
  mVelocityX = nullptr;
  mVelocityY = nullptr;
  mVelocityZ = nullptr;
  mForceX = nullptr;
  mForceY = nullptr;
  mForceZ = nullptr;
  mFlame = nullptr;
  mFuel = nullptr;
  mReact = nullptr;
  mColorR = nullptr;
  mColorG = nullptr;
  mColorB = nullptr;
  mFlags = nullptr;
  mDensityIn = nullptr;
  mHeatIn = nullptr;
  mColorRIn = nullptr;
  mColorGIn = nullptr;
  mColorBIn = nullptr;
  mFuelIn = nullptr;
  mReactIn = nullptr;
  mEmissionIn = nullptr;

  // Smoke high res grids
  mDensityHigh = nullptr;
  mFlameHigh = nullptr;
  mFuelHigh = nullptr;
  mReactHigh = nullptr;
  mColorRHigh = nullptr;
  mColorGHigh = nullptr;
  mColorBHigh = nullptr;
  mTextureU = nullptr;
  mTextureV = nullptr;
  mTextureW = nullptr;
  mTextureU2 = nullptr;
  mTextureV2 = nullptr;
  mTextureW2 = nullptr;

  // Fluid low res grids
  mPhiIn = nullptr;
  mPhiStaticIn = nullptr;
  mPhiOutIn = nullptr;
  mPhiOutStaticIn = nullptr;
  mPhi = nullptr;

  // Mesh
  mMeshNodes = nullptr;
  mMeshTriangles = nullptr;
  mMeshVelocities = nullptr;

  // Fluid obstacle
  mPhiObsIn = nullptr;
  mPhiObsStaticIn = nullptr;
  mNumObstacle = nullptr;
  mObVelocityX = nullptr;
  mObVelocityY = nullptr;
  mObVelocityZ = nullptr;

  // Fluid guiding
  mPhiGuideIn = nullptr;
  mNumGuide = nullptr;
  mGuideVelocityX = nullptr;
  mGuideVelocityY = nullptr;
  mGuideVelocityZ = nullptr;

  // Fluid initial velocity
  mInVelocityX = nullptr;
  mInVelocityY = nullptr;
  mInVelocityZ = nullptr;

  // Secondary particles
  mFlipParticleData = nullptr;
  mFlipParticleVelocity = nullptr;
  mSndParticleData = nullptr;
  mSndParticleVelocity = nullptr;
  mSndParticleLife = nullptr;

  // Cache read success indicators
  mFlipFromFile = false;
  mMeshFromFile = false;
  mParticlesFromFile = false;

  // Setup Mantaflow in Python
  initializeMantaflow();

  // Initializa RNA map with values that Python will need
  initializeRNAMap(mmd);

  // Initialize Mantaflow variables in Python
  // Liquid
  if (mUsingLiquid) {
    initDomain();
    initLiquid();
    if (mUsingObstacle)
      initObstacle();
    if (mUsingInvel)
      initInVelocity();
    if (mUsingOutflow)
      initOutflow();

    if (mUsingDrops || mUsingBubbles || mUsingFloats || mUsingTracers) {
      mUpresParticle = mds->particle_scale;
      mResXParticle = mUpresParticle * mResX;
      mResYParticle = mUpresParticle * mResY;
      mResZParticle = mUpresParticle * mResZ;
      mTotalCellsParticles = mResXParticle * mResYParticle * mResZParticle;

      initSndParts();
      initLiquidSndParts();
    }

    if (mUsingMesh) {
      mUpresMesh = mds->mesh_scale;
      mResXMesh = mUpresMesh * mResX;
      mResYMesh = mUpresMesh * mResY;
      mResZMesh = mUpresMesh * mResZ;
      mTotalCellsMesh = mResXMesh * mResYMesh * mResZMesh;

      // Initialize Mantaflow variables in Python
      initMesh();
      initLiquidMesh();
    }

    if (mUsingDiffusion) {
      initCurvature();
    }

    if (mUsingGuiding) {
      mResGuiding = (mds->guide_parent) ? mds->guide_res : mds->res;
      initGuiding();
    }
    if (mUsingFractions) {
      initFractions();
    }
  }

  // Smoke
  if (mUsingSmoke) {
    initDomain();
    initSmoke();
    if (mUsingHeat)
      initHeat();
    if (mUsingFire)
      initFire();
    if (mUsingColors)
      initColors();
    if (mUsingObstacle)
      initObstacle();
    if (mUsingInvel)
      initInVelocity();
    if (mUsingOutflow)
      initOutflow();

    if (mUsingGuiding) {
      mResGuiding = (mds->guide_parent) ? mds->guide_res : mds->res;
      initGuiding();
    }

    if (mUsingNoise) {
      int amplify = mds->noise_scale;
      mResXNoise = amplify * mResX;
      mResYNoise = amplify * mResY;
      mResZNoise = amplify * mResZ;
      mTotalCellsHigh = mResXNoise * mResYNoise * mResZNoise;

      // Initialize Mantaflow variables in Python
      initNoise();
      initSmokeNoise();
      if (mUsingFire)
        initFireHigh();
      if (mUsingColors)
        initColorsHigh();
    }
  }
  updatePointers();
}

void MANTA::initDomain(FluidModifierData *mmd)
{
  // Vector will hold all python commands that are to be executed
  vector<string> pythonCommands;

  // Set manta debug level first
  pythonCommands.push_back(manta_import + manta_debuglevel);

  ostringstream ss;
  ss << "set_manta_debuglevel(" << with_debug << ")";
  pythonCommands.push_back(ss.str());

  // Now init basic fluid domain
  string tmpString = fluid_variables + fluid_solver + fluid_alloc + fluid_cache_helper +
                     fluid_bake_multiprocessing + fluid_bake_data + fluid_bake_noise +
                     fluid_bake_mesh + fluid_bake_particles + fluid_bake_guiding +
                     fluid_file_import + fluid_file_export + fluid_pre_step + fluid_post_step +
                     fluid_adapt_time_step + fluid_time_stepping;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);
  runPythonString(pythonCommands);
}

void MANTA::initNoise(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_noise + fluid_solver_noise;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
}

void MANTA::initSmoke(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = smoke_variables + smoke_alloc + smoke_adaptive_step + smoke_save_data +
                     smoke_load_data + smoke_step;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
}

void MANTA::initSmokeNoise(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = smoke_variables_noise + smoke_alloc_noise + smoke_wavelet_noise +
                     smoke_save_noise + smoke_load_noise + smoke_step_noise;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
  mUsingNoise = true;
}

void MANTA::initHeat(FluidModifierData *mmd)
{
  if (!mHeat) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_heat + smoke_with_heat;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingHeat = true;
  }
}

void MANTA::initFire(FluidModifierData *mmd)
{
  if (!mFuel) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_fire + smoke_with_fire;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingFire = true;
  }
}

void MANTA::initFireHigh(FluidModifierData *mmd)
{
  if (!mFuelHigh) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_fire_noise + smoke_with_fire;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingFire = true;
  }
}

void MANTA::initColors(FluidModifierData *mmd)
{
  if (!mColorR) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_colors + smoke_init_colors + smoke_with_colors;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingColors = true;
  }
}

void MANTA::initColorsHigh(FluidModifierData *mmd)
{
  if (!mColorRHigh) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_colors_noise + smoke_init_colors_noise + smoke_with_colors;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingColors = true;
  }
}

void MANTA::initLiquid(FluidModifierData *mmd)
{
  if (!mPhiIn) {
    vector<string> pythonCommands;
    string tmpString = liquid_variables + liquid_alloc + liquid_init_phi + liquid_save_data +
                       liquid_load_data + liquid_adaptive_step + liquid_step;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingLiquid = true;
  }
}

void MANTA::initMesh(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_mesh + fluid_solver_mesh + liquid_load_mesh;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
  mUsingMesh = true;
}

void MANTA::initLiquidMesh(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = liquid_alloc_mesh + liquid_step_mesh + liquid_save_mesh;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
  mUsingMesh = true;
}

void MANTA::initCurvature(FluidModifierData *mmd)
{
  std::vector<std::string> pythonCommands;
  std::string finalString = parseScript(liquid_alloc_curvature, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
  mUsingDiffusion = true;
}

void MANTA::initObstacle(FluidModifierData *mmd)
{
  if (!mPhiObsIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_obstacle + fluid_with_obstacle;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingObstacle = true;
  }
}

void MANTA::initGuiding(FluidModifierData *mmd)
{
  if (!mPhiGuideIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_variables_guiding + fluid_solver_guiding + fluid_alloc_guiding +
                       fluid_save_guiding + fluid_load_vel + fluid_load_guiding;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingGuiding = true;
  }
}

void MANTA::initFractions(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_alloc_fractions + fluid_with_fractions;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
  mUsingFractions = true;
}

void MANTA::initInVelocity(FluidModifierData *mmd)
{
  if (!mInVelocityX) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_invel + fluid_with_invel;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingInvel = true;
  }
}

void MANTA::initOutflow(FluidModifierData *mmd)
{
  if (!mPhiOutIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_outflow + fluid_with_outflow;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
    mUsingOutflow = true;
  }
}

void MANTA::initSndParts(FluidModifierData *mmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_particles + fluid_solver_particles;
  string finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  runPythonString(pythonCommands);
}

void MANTA::initLiquidSndParts(FluidModifierData *mmd)
{
  if (!mSndParticleData) {
    vector<string> pythonCommands;
    string tmpString = liquid_alloc_particles + liquid_variables_particles +
                       liquid_step_particles + fluid_with_sndparts + liquid_load_particles +
                       liquid_save_particles;
    string finalString = parseScript(tmpString, mmd);
    pythonCommands.push_back(finalString);

    runPythonString(pythonCommands);
  }
}

MANTA::~MANTA()
{
  if (with_debug)
    cout << "~FLUID: " << mCurrentID << " with res(" << mResX << ", " << mResY << ", " << mResZ
         << ")" << endl;

  // Destruction string for Python
  string tmpString = "";
  vector<string> pythonCommands;
  bool result = false;

  tmpString += manta_import;
  tmpString += fluid_delete_all;

  // Initializa RNA map with values that Python will need
  initializeRNAMap();

  // Leave out mmd argument in parseScript since only looking up IDs
  string finalString = parseScript(tmpString);
  pythonCommands.push_back(finalString);
  result = runPythonString(pythonCommands);

  assert(result);
  UNUSED_VARS(result);
}

/**
 * Store a pointer to the __main__ module used by mantaflow. This is necessary, because sometimes
 * Blender will overwrite that module. That happens when e.g. scripts are executed in the text
 * editor.
 *
 * Mantaflow stores many variables in the globals() dict of the __main__ module. To be able to
 * access these variables, the same __main__ module has to be used every time.
 *
 * Unfortunately, we also depend on the fact that mantaflow dumps variables into this module using
 * PyRun_SimpleString. So we can't easily create a separate module without changing mantaflow.
 */
static PyObject *manta_main_module = nullptr;

bool MANTA::runPythonString(vector<string> commands)
{
  bool success = true;
  PyGILState_STATE gilstate = PyGILState_Ensure();

  if (manta_main_module == nullptr) {
    manta_main_module = PyImport_ImportModule("__main__");
  }

  for (vector<string>::iterator it = commands.begin(); it != commands.end(); ++it) {
    string command = *it;

    PyObject *globals_dict = PyModule_GetDict(manta_main_module);
    PyObject *return_value = PyRun_String(
        command.c_str(), Py_file_input, globals_dict, globals_dict);

    if (return_value == nullptr) {
      success = false;
      if (PyErr_Occurred()) {
        PyErr_Print();
      }
    }
    else {
      Py_DECREF(return_value);
    }
  }
  PyGILState_Release(gilstate);

  assert(success);
  return success;
}

void MANTA::initializeMantaflow()
{
  if (with_debug)
    cout << "Fluid: Initializing Mantaflow framework" << endl;

  string filename = "manta_scene_" + to_string(mCurrentID) + ".py";
  vector<string> fill = vector<string>();

  // Initialize extension classes and wrappers
  srand(0);
  PyGILState_STATE gilstate = PyGILState_Ensure();
  Pb::setup(filename, fill);  // Namespace from Mantaflow (registry)
  PyGILState_Release(gilstate);
}

void MANTA::terminateMantaflow()
{
  if (with_debug)
    cout << "Fluid: Releasing Mantaflow framework" << endl;

  PyGILState_STATE gilstate = PyGILState_Ensure();
  Pb::finalize();  // Namespace from Mantaflow (registry)
  PyGILState_Release(gilstate);
}

static string getCacheFileEnding(char cache_format)
{
  if (MANTA::with_debug)
    cout << "MANTA::getCacheFileEnding()" << endl;

  switch (cache_format) {
    case FLUID_DOMAIN_FILE_UNI:
      return FLUID_DOMAIN_EXTENSION_UNI;
    case FLUID_DOMAIN_FILE_OPENVDB:
      return FLUID_DOMAIN_EXTENSION_OPENVDB;
    case FLUID_DOMAIN_FILE_RAW:
      return FLUID_DOMAIN_EXTENSION_RAW;
    case FLUID_DOMAIN_FILE_BIN_OBJECT:
      return FLUID_DOMAIN_EXTENSION_BINOBJ;
    case FLUID_DOMAIN_FILE_OBJECT:
      return FLUID_DOMAIN_EXTENSION_OBJ;
    default:
      cerr << "Fluid Error -- Could not find file extension. Using default file extension."
           << endl;
      return FLUID_DOMAIN_EXTENSION_UNI;
  }
}

static string getBooleanString(int value)
{
  return (value) ? "True" : "False";
}

void MANTA::initializeRNAMap(FluidModifierData *mmd)
{
  if (with_debug)
    cout << "MANTA::initializeRNAMap()" << endl;

  mRNAMap["ID"] = to_string(mCurrentID);

  if (!mmd) {
    if (with_debug)
      cout << "Fluid: No modifier data given in RNA map setup - returning early" << endl;
    return;
  }

  FluidDomainSettings *mds = mmd->domain;
  bool is2D = (mds->solver_res == 2);

  string borderCollisions = "";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_LEFT) == 0)
    borderCollisions += "x";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_RIGHT) == 0)
    borderCollisions += "X";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_FRONT) == 0)
    borderCollisions += "y";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_BACK) == 0)
    borderCollisions += "Y";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_BOTTOM) == 0)
    borderCollisions += "z";
  if ((mds->border_collisions & FLUID_DOMAIN_BORDER_TOP) == 0)
    borderCollisions += "Z";

  string simulationMethod = "";
  if (mds->simulation_method & FLUID_DOMAIN_METHOD_FLIP)
    simulationMethod += "'FLIP'";
  else if (mds->simulation_method & FLUID_DOMAIN_METHOD_APIC)
    simulationMethod += "'APIC'";

  string particleTypesStr = "";
  if (mds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY)
    particleTypesStr += "PtypeSpray";
  if (mds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeBubble";
  }
  if (mds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeFoam";
  }
  if (mds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeTracer";
  }
  if (particleTypesStr.empty())
    particleTypesStr = "0";

  int particleTypes = (FLUID_DOMAIN_PARTICLE_SPRAY | FLUID_DOMAIN_PARTICLE_BUBBLE |
                       FLUID_DOMAIN_PARTICLE_FOAM | FLUID_DOMAIN_PARTICLE_TRACER);

  string cacheDirectory(mds->cache_directory);

  float viscosity = mds->viscosity_base * pow(10.0f, -mds->viscosity_exponent);
  float domainSize = MAX3(mds->global_size[0], mds->global_size[1], mds->global_size[2]);

  string vdbCompressionMethod = "Compression_None";
  if (mds->openvdb_compression == VDB_COMPRESSION_NONE)
    vdbCompressionMethod = "Compression_None";
  else if (mds->openvdb_compression == VDB_COMPRESSION_ZIP)
    vdbCompressionMethod = "Compression_Zip";
  else if (mds->openvdb_compression == VDB_COMPRESSION_BLOSC)
    vdbCompressionMethod = "Compression_Blosc";

  string vdbPrecisionHalf = "True";
  if (mds->openvdb_data_depth == VDB_PRECISION_HALF_FLOAT)
    vdbPrecisionHalf = "True";
  else if (mds->openvdb_data_depth == VDB_PRECISION_FULL_FLOAT)
    vdbPrecisionHalf = "False";

  mRNAMap["USING_SMOKE"] = getBooleanString(mds->type == FLUID_DOMAIN_TYPE_GAS);
  mRNAMap["USING_LIQUID"] = getBooleanString(mds->type == FLUID_DOMAIN_TYPE_LIQUID);
  mRNAMap["USING_COLORS"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS);
  mRNAMap["USING_HEAT"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT);
  mRNAMap["USING_FIRE"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE);
  mRNAMap["USING_NOISE"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_NOISE);
  mRNAMap["USING_OBSTACLE"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE);
  mRNAMap["USING_GUIDING"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_GUIDE);
  mRNAMap["USING_INVEL"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL);
  mRNAMap["USING_OUTFLOW"] = getBooleanString(mds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW);
  mRNAMap["USING_LOG_DISSOLVE"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_DISSOLVE_LOG);
  mRNAMap["USING_DISSOLVE"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_DISSOLVE);
  mRNAMap["DOMAIN_CLOSED"] = getBooleanString(borderCollisions.compare("") == 0);
  mRNAMap["CACHE_RESUMABLE"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE);
  mRNAMap["USING_ADAPTIVETIME"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_TIME);
  mRNAMap["USING_SPEEDVECTORS"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS);
  mRNAMap["USING_FRACTIONS"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_FRACTIONS);
  mRNAMap["DELETE_IN_OBSTACLE"] = getBooleanString(mds->flags & FLUID_DOMAIN_DELETE_IN_OBSTACLE);
  mRNAMap["USING_DIFFUSION"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_DIFFUSION);
  mRNAMap["USING_MESH"] = getBooleanString(mds->flags & FLUID_DOMAIN_USE_MESH);
  mRNAMap["USING_IMPROVED_MESH"] = getBooleanString(mds->mesh_generator ==
                                                    FLUID_DOMAIN_MESH_IMPROVED);
  mRNAMap["USING_SNDPARTS"] = getBooleanString(mds->particle_type & particleTypes);
  mRNAMap["SNDPARTICLE_BOUNDARY_DELETE"] = getBooleanString(mds->sndparticle_boundary ==
                                                            SNDPARTICLE_BOUNDARY_DELETE);
  mRNAMap["SNDPARTICLE_BOUNDARY_PUSHOUT"] = getBooleanString(mds->sndparticle_boundary ==
                                                             SNDPARTICLE_BOUNDARY_PUSHOUT);

  mRNAMap["SOLVER_DIM"] = to_string(mds->solver_res);
  mRNAMap["BOUND_CONDITIONS"] = borderCollisions;
  mRNAMap["BOUNDARY_WIDTH"] = to_string(mds->boundary_width);
  mRNAMap["RES"] = to_string(mMaxRes);
  mRNAMap["RESX"] = to_string(mResX);
  mRNAMap["RESY"] = (is2D) ? to_string(mResZ) : to_string(mResY);
  mRNAMap["RESZ"] = (is2D) ? to_string(1) : to_string(mResZ);
  mRNAMap["TIME_SCALE"] = to_string(mds->time_scale);
  mRNAMap["FRAME_LENGTH"] = to_string(mds->frame_length);
  mRNAMap["CFL"] = to_string(mds->cfl_condition);
  mRNAMap["DT"] = to_string(mds->dt);
  mRNAMap["TIMESTEPS_MIN"] = to_string(mds->timesteps_minimum);
  mRNAMap["TIMESTEPS_MAX"] = to_string(mds->timesteps_maximum);
  mRNAMap["TIME_TOTAL"] = to_string(mds->time_total);
  mRNAMap["TIME_PER_FRAME"] = to_string(mds->time_per_frame);
  mRNAMap["VORTICITY"] = to_string(mds->vorticity);
  mRNAMap["FLAME_VORTICITY"] = to_string(mds->flame_vorticity);
  mRNAMap["NOISE_SCALE"] = to_string(mds->noise_scale);
  mRNAMap["MESH_SCALE"] = to_string(mds->mesh_scale);
  mRNAMap["PARTICLE_SCALE"] = to_string(mds->particle_scale);
  mRNAMap["NOISE_RESX"] = to_string(mResXNoise);
  mRNAMap["NOISE_RESY"] = (is2D) ? to_string(mResZNoise) : to_string(mResYNoise);
  mRNAMap["NOISE_RESZ"] = (is2D) ? to_string(1) : to_string(mResZNoise);
  mRNAMap["MESH_RESX"] = to_string(mResXMesh);
  mRNAMap["MESH_RESY"] = (is2D) ? to_string(mResZMesh) : to_string(mResYMesh);
  mRNAMap["MESH_RESZ"] = (is2D) ? to_string(1) : to_string(mResZMesh);
  mRNAMap["PARTICLE_RESX"] = to_string(mResXParticle);
  mRNAMap["PARTICLE_RESY"] = (is2D) ? to_string(mResZParticle) : to_string(mResYParticle);
  mRNAMap["PARTICLE_RESZ"] = (is2D) ? to_string(1) : to_string(mResZParticle);
  mRNAMap["GUIDING_RESX"] = to_string(mResGuiding[0]);
  mRNAMap["GUIDING_RESY"] = (is2D) ? to_string(mResGuiding[2]) : to_string(mResGuiding[1]);
  mRNAMap["GUIDING_RESZ"] = (is2D) ? to_string(1) : to_string(mResGuiding[2]);
  mRNAMap["MIN_RESX"] = to_string(mds->res_min[0]);
  mRNAMap["MIN_RESY"] = to_string(mds->res_min[1]);
  mRNAMap["MIN_RESZ"] = to_string(mds->res_min[2]);
  mRNAMap["BASE_RESX"] = to_string(mds->base_res[0]);
  mRNAMap["BASE_RESY"] = to_string(mds->base_res[1]);
  mRNAMap["BASE_RESZ"] = to_string(mds->base_res[2]);
  mRNAMap["WLT_STR"] = to_string(mds->noise_strength);
  mRNAMap["NOISE_POSSCALE"] = to_string(mds->noise_pos_scale);
  mRNAMap["NOISE_TIMEANIM"] = to_string(mds->noise_time_anim);
  mRNAMap["COLOR_R"] = to_string(mds->active_color[0]);
  mRNAMap["COLOR_G"] = to_string(mds->active_color[1]);
  mRNAMap["COLOR_B"] = to_string(mds->active_color[2]);
  mRNAMap["BUOYANCY_ALPHA"] = to_string(mds->alpha);
  mRNAMap["BUOYANCY_BETA"] = to_string(mds->beta);
  mRNAMap["DISSOLVE_SPEED"] = to_string(mds->diss_speed);
  mRNAMap["BURNING_RATE"] = to_string(mds->burning_rate);
  mRNAMap["FLAME_SMOKE"] = to_string(mds->flame_smoke);
  mRNAMap["IGNITION_TEMP"] = to_string(mds->flame_ignition);
  mRNAMap["MAX_TEMP"] = to_string(mds->flame_max_temp);
  mRNAMap["FLAME_SMOKE_COLOR_X"] = to_string(mds->flame_smoke_color[0]);
  mRNAMap["FLAME_SMOKE_COLOR_Y"] = to_string(mds->flame_smoke_color[1]);
  mRNAMap["FLAME_SMOKE_COLOR_Z"] = to_string(mds->flame_smoke_color[2]);
  mRNAMap["CURRENT_FRAME"] = to_string(int(mmd->time));
  mRNAMap["START_FRAME"] = to_string(mds->cache_frame_start);
  mRNAMap["END_FRAME"] = to_string(mds->cache_frame_end);
  mRNAMap["CACHE_DATA_FORMAT"] = getCacheFileEnding(mds->cache_data_format);
  mRNAMap["CACHE_MESH_FORMAT"] = getCacheFileEnding(mds->cache_mesh_format);
  mRNAMap["CACHE_NOISE_FORMAT"] = getCacheFileEnding(mds->cache_noise_format);
  mRNAMap["CACHE_PARTICLE_FORMAT"] = getCacheFileEnding(mds->cache_particle_format);
  mRNAMap["SIMULATION_METHOD"] = simulationMethod;
  mRNAMap["FLIP_RATIO"] = to_string(mds->flip_ratio);
  mRNAMap["PARTICLE_RANDOMNESS"] = to_string(mds->particle_randomness);
  mRNAMap["PARTICLE_NUMBER"] = to_string(mds->particle_number);
  mRNAMap["PARTICLE_MINIMUM"] = to_string(mds->particle_minimum);
  mRNAMap["PARTICLE_MAXIMUM"] = to_string(mds->particle_maximum);
  mRNAMap["PARTICLE_RADIUS"] = to_string(mds->particle_radius);
  mRNAMap["FRACTIONS_THRESHOLD"] = to_string(mds->fractions_threshold);
  mRNAMap["MESH_CONCAVE_UPPER"] = to_string(mds->mesh_concave_upper);
  mRNAMap["MESH_CONCAVE_LOWER"] = to_string(mds->mesh_concave_lower);
  mRNAMap["MESH_PARTICLE_RADIUS"] = to_string(mds->mesh_particle_radius);
  mRNAMap["MESH_SMOOTHEN_POS"] = to_string(mds->mesh_smoothen_pos);
  mRNAMap["MESH_SMOOTHEN_NEG"] = to_string(mds->mesh_smoothen_neg);
  mRNAMap["PARTICLE_BAND_WIDTH"] = to_string(mds->particle_band_width);
  mRNAMap["SNDPARTICLE_TAU_MIN_WC"] = to_string(mds->sndparticle_tau_min_wc);
  mRNAMap["SNDPARTICLE_TAU_MAX_WC"] = to_string(mds->sndparticle_tau_max_wc);
  mRNAMap["SNDPARTICLE_TAU_MIN_TA"] = to_string(mds->sndparticle_tau_min_ta);
  mRNAMap["SNDPARTICLE_TAU_MAX_TA"] = to_string(mds->sndparticle_tau_max_ta);
  mRNAMap["SNDPARTICLE_TAU_MIN_K"] = to_string(mds->sndparticle_tau_min_k);
  mRNAMap["SNDPARTICLE_TAU_MAX_K"] = to_string(mds->sndparticle_tau_max_k);
  mRNAMap["SNDPARTICLE_K_WC"] = to_string(mds->sndparticle_k_wc);
  mRNAMap["SNDPARTICLE_K_TA"] = to_string(mds->sndparticle_k_ta);
  mRNAMap["SNDPARTICLE_K_B"] = to_string(mds->sndparticle_k_b);
  mRNAMap["SNDPARTICLE_K_D"] = to_string(mds->sndparticle_k_d);
  mRNAMap["SNDPARTICLE_L_MIN"] = to_string(mds->sndparticle_l_min);
  mRNAMap["SNDPARTICLE_L_MAX"] = to_string(mds->sndparticle_l_max);
  mRNAMap["SNDPARTICLE_POTENTIAL_RADIUS"] = to_string(mds->sndparticle_potential_radius);
  mRNAMap["SNDPARTICLE_UPDATE_RADIUS"] = to_string(mds->sndparticle_update_radius);
  mRNAMap["LIQUID_SURFACE_TENSION"] = to_string(mds->surface_tension);
  mRNAMap["FLUID_VISCOSITY"] = to_string(viscosity);
  mRNAMap["FLUID_DOMAIN_SIZE"] = to_string(domainSize);
  mRNAMap["FLUID_DOMAIN_SIZE_X"] = to_string(mds->global_size[0]);
  mRNAMap["FLUID_DOMAIN_SIZE_Y"] = to_string(mds->global_size[1]);
  mRNAMap["FLUID_DOMAIN_SIZE_Z"] = to_string(mds->global_size[2]);
  mRNAMap["SNDPARTICLE_TYPES"] = particleTypesStr;
  mRNAMap["GUIDING_ALPHA"] = to_string(mds->guide_alpha);
  mRNAMap["GUIDING_BETA"] = to_string(mds->guide_beta);
  mRNAMap["GUIDING_FACTOR"] = to_string(mds->guide_vel_factor);
  mRNAMap["GRAVITY_X"] = to_string(mds->gravity[0]);
  mRNAMap["GRAVITY_Y"] = to_string(mds->gravity[1]);
  mRNAMap["GRAVITY_Z"] = to_string(mds->gravity[2]);
  mRNAMap["CACHE_DIR"] = cacheDirectory;
  mRNAMap["COMPRESSION_OPENVDB"] = vdbCompressionMethod;
  mRNAMap["PRECISION_OPENVDB"] = vdbPrecisionHalf;

  /* Fluid object names. */
  mRNAMap["NAME_FLAGS"] = FLUID_NAME_FLAGS;
  mRNAMap["NAME_VELOCITY"] = FLUID_NAME_VELOCITY;
  mRNAMap["NAME_VELOCITYTMP"] = FLUID_NAME_VELOCITYTMP;
  mRNAMap["NAME_VELOCITY_X"] = FLUID_NAME_VELOCITYX;
  mRNAMap["NAME_VELOCITY_Y"] = FLUID_NAME_VELOCITYY;
  mRNAMap["NAME_VELOCITY_Z"] = FLUID_NAME_VELOCITYZ;
  mRNAMap["NAME_PRESSURE"] = FLUID_NAME_PRESSURE;
  mRNAMap["NAME_PHIOBS"] = FLUID_NAME_PHIOBS;
  mRNAMap["NAME_PHISIN"] = FLUID_NAME_PHISIN;
  mRNAMap["NAME_PHIIN"] = FLUID_NAME_PHIIN;
  mRNAMap["NAME_PHIOUT"] = FLUID_NAME_PHIOUT;
  mRNAMap["NAME_FORCES"] = FLUID_NAME_FORCES;
  mRNAMap["NAME_FORCES_X"] = FLUID_NAME_FORCE_X;
  mRNAMap["NAME_FORCES_Y"] = FLUID_NAME_FORCE_Y;
  mRNAMap["NAME_FORCES_Z"] = FLUID_NAME_FORCE_Z;
  mRNAMap["NAME_NUMOBS"] = FLUID_NAME_NUMOBS;
  mRNAMap["NAME_PHIOBSSIN"] = FLUID_NAME_PHIOBSSIN;
  mRNAMap["NAME_PHIOBSIN"] = FLUID_NAME_PHIOBSIN;
  mRNAMap["NAME_OBVEL"] = FLUID_NAME_OBVEL;
  mRNAMap["NAME_OBVELC"] = FLUID_NAME_OBVELC;
  mRNAMap["NAME_OBVEL_X"] = FLUID_NAME_OBVEL_X;
  mRNAMap["NAME_OBVEL_Y"] = FLUID_NAME_OBVEL_Y;
  mRNAMap["NAME_OBVEL_Z"] = FLUID_NAME_OBVEL_Z;
  mRNAMap["NAME_FRACTIONS"] = FLUID_NAME_FRACTIONS;
  mRNAMap["NAME_INVELC"] = FLUID_NAME_INVELC;
  mRNAMap["NAME_INVEL"] = FLUID_NAME_INVEL;
  mRNAMap["NAME_INVEL_X"] = FLUID_NAME_INVEL_X;
  mRNAMap["NAME_INVEL_Y"] = FLUID_NAME_INVEL_Y;
  mRNAMap["NAME_INVEL_Z"] = FLUID_NAME_INVEL_Z;
  mRNAMap["NAME_PHIOUTSIN"] = FLUID_NAME_PHIOUTSIN;
  mRNAMap["NAME_PHIOUTIN"] = FLUID_NAME_PHIOUTIN;

  /* Smoke object names. */
  mRNAMap["NAME_SHADOW"] = FLUID_NAME_SHADOW;
  mRNAMap["NAME_EMISSION"] = FLUID_NAME_EMISSION;
  mRNAMap["NAME_EMISSIONIN"] = FLUID_NAME_EMISSIONIN;
  mRNAMap["NAME_DENSITY"] = FLUID_NAME_DENSITY;
  mRNAMap["NAME_DENSITYIN"] = FLUID_NAME_DENSITYIN;
  mRNAMap["NAME_HEAT"] = FLUID_NAME_HEAT;
  mRNAMap["NAME_HEATIN"] = FLUID_NAME_HEATIN;
  mRNAMap["NAME_TEMPERATURE"] = FLUID_NAME_TEMPERATURE;
  mRNAMap["NAME_TEMPERATUREIN"] = FLUID_NAME_TEMPERATUREIN;
  mRNAMap["NAME_COLORR"] = FLUID_NAME_COLORR;
  mRNAMap["NAME_COLORG"] = FLUID_NAME_COLORG;
  mRNAMap["NAME_COLORB"] = FLUID_NAME_COLORB;
  mRNAMap["NAME_COLORRIN"] = FLUID_NAME_COLORRIN;
  mRNAMap["NAME_COLORGIN"] = FLUID_NAME_COLORGIN;
  mRNAMap["NAME_COLORBIN"] = FLUID_NAME_COLORBIN;
  mRNAMap["NAME_FLAME"] = FLUID_NAME_FLAME;
  mRNAMap["NAME_FUEL"] = FLUID_NAME_FUEL;
  mRNAMap["NAME_REACT"] = FLUID_NAME_REACT;
  mRNAMap["NAME_FUELIN"] = FLUID_NAME_FUELIN;
  mRNAMap["NAME_REACTIN"] = FLUID_NAME_REACTIN;

  /* Liquid object names. */
  mRNAMap["NAME_PHIPARTS"] = FLUID_NAME_PHIPARTS;
  mRNAMap["NAME_PHI"] = FLUID_NAME_PHI;
  mRNAMap["NAME_PHITMP"] = FLUID_NAME_PHITMP;
  mRNAMap["NAME_VELOLD"] = FLUID_NAME_VELOCITYOLD;
  mRNAMap["NAME_VELPARTS"] = FLUID_NAME_VELOCITYPARTS;
  mRNAMap["NAME_MAPWEIGHTS"] = FLUID_NAME_MAPWEIGHTS;
  mRNAMap["NAME_PP"] = FLUID_NAME_PP;
  mRNAMap["NAME_PVEL"] = FLUID_NAME_PVEL;
  mRNAMap["NAME_PARTS"] = FLUID_NAME_PARTS;
  mRNAMap["NAME_PARTSVELOCITY"] = FLUID_NAME_PARTSVELOCITY;
  mRNAMap["NAME_PINDEX"] = FLUID_NAME_PINDEX;
  mRNAMap["NAME_GPI"] = FLUID_NAME_GPI;
  mRNAMap["NAME_CURVATURE"] = FLUID_NAME_CURVATURE;

  /* Noise object names. */
  mRNAMap["NAME_VELOCITY_NOISE"] = FLUID_NAME_VELOCITY_NOISE;
  mRNAMap["NAME_DENSITY_NOISE"] = FLUID_NAME_DENSITY_NOISE;
  mRNAMap["NAME_PHIIN_NOISE"] = FLUID_NAME_PHIIN_NOISE;
  mRNAMap["NAME_PHIOUT_NOISE"] = FLUID_NAME_PHIOUT_NOISE;
  mRNAMap["NAME_PHIOBS_NOISE"] = FLUID_NAME_PHIOBS_NOISE;
  mRNAMap["NAME_FLAGS_NOISE"] = FLUID_NAME_FLAGS_NOISE;
  mRNAMap["NAME_TMPIN_NOISE"] = FLUID_NAME_TMPIN_NOISE;
  mRNAMap["NAME_EMISSIONIN_NOISE"] = FLUID_NAME_EMISSIONIN_NOISE;
  mRNAMap["NAME_ENERGY"] = FLUID_NAME_ENERGY;
  mRNAMap["NAME_TMPFLAGS"] = FLUID_NAME_TMPFLAGS;
  mRNAMap["NAME_TEXTURE_U"] = FLUID_NAME_TEXTURE_U;
  mRNAMap["NAME_TEXTURE_V"] = FLUID_NAME_TEXTURE_V;
  mRNAMap["NAME_TEXTURE_W"] = FLUID_NAME_TEXTURE_W;
  mRNAMap["NAME_TEXTURE_U2"] = FLUID_NAME_TEXTURE_U2;
  mRNAMap["NAME_TEXTURE_V2"] = FLUID_NAME_TEXTURE_V2;
  mRNAMap["NAME_TEXTURE_W2"] = FLUID_NAME_TEXTURE_W2;
  mRNAMap["NAME_UV0"] = FLUID_NAME_UV0;
  mRNAMap["NAME_UV1"] = FLUID_NAME_UV1;
  mRNAMap["NAME_COLORR_NOISE"] = FLUID_NAME_COLORR_NOISE;
  mRNAMap["NAME_COLORG_NOISE"] = FLUID_NAME_COLORG_NOISE;
  mRNAMap["NAME_COLORB_NOISE"] = FLUID_NAME_COLORB_NOISE;
  mRNAMap["NAME_FLAME_NOISE"] = FLUID_NAME_FLAME_NOISE;
  mRNAMap["NAME_FUEL_NOISE"] = FLUID_NAME_FUEL_NOISE;
  mRNAMap["NAME_REACT_NOISE"] = FLUID_NAME_REACT_NOISE;

  /* Mesh object names. */
  mRNAMap["NAME_PHIPARTS_MESH"] = FLUID_NAME_PHIPARTS_MESH;
  mRNAMap["NAME_PHI_MESH"] = FLUID_NAME_PHI_MESH;
  mRNAMap["NAME_PP_MESH"] = FLUID_NAME_PP_MESH;
  mRNAMap["NAME_FLAGS_MESH"] = FLUID_NAME_FLAGS_MESH;
  mRNAMap["NAME_LMESH"] = FLUID_NAME_LMESH;
  mRNAMap["NAME_VELOCITYVEC_MESH"] = FLUID_NAME_VELOCITYVEC_MESH;
  mRNAMap["NAME_VELOCITY_MESH"] = FLUID_NAME_VELOCITY_MESH;
  mRNAMap["NAME_PINDEX_MESH"] = FLUID_NAME_PINDEX_MESH;
  mRNAMap["NAME_GPI_MESH"] = FLUID_NAME_GPI_MESH;

  /* Particles object names. */
  mRNAMap["NAME_PP_PARTICLES"] = FLUID_NAME_PP_PARTICLES;
  mRNAMap["NAME_PVEL_PARTICLES"] = FLUID_NAME_PVEL_PARTICLES;
  mRNAMap["NAME_PFORCE_PARTICLES"] = FLUID_NAME_PFORCE_PARTICLES;
  mRNAMap["NAME_PLIFE_PARTICLES"] = FLUID_NAME_PLIFE_PARTICLES;
  mRNAMap["NAME_PARTS_PARTICLES"] = FLUID_NAME_PARTS_PARTICLES;
  mRNAMap["NAME_PARTSVEL_PARTICLES"] = FLUID_NAME_PARTSVEL_PARTICLES;
  mRNAMap["NAME_PARTSFORCE_PARTICLES"] = FLUID_NAME_PARTSFORCE_PARTICLES;
  mRNAMap["NAME_PARTSLIFE_PARTICLES"] = FLUID_NAME_PARTSLIFE_PARTICLES;
  mRNAMap["NAME_VELOCITY_PARTICLES"] = FLUID_NAME_VELOCITY_PARTICLES;
  mRNAMap["NAME_FLAGS_PARTICLES"] = FLUID_NAME_FLAGS_PARTICLES;
  mRNAMap["NAME_PHI_PARTICLES"] = FLUID_NAME_PHI_PARTICLES;
  mRNAMap["NAME_PHIOBS_PARTICLES"] = FLUID_NAME_PHIOBS_PARTICLES;
  mRNAMap["NAME_PHIOUT_PARTICLES"] = FLUID_NAME_PHIOUT_PARTICLES;
  mRNAMap["NAME_NORMAL_PARTICLES"] = FLUID_NAME_NORMAL_PARTICLES;
  mRNAMap["NAME_NEIGHBORRATIO_PARTICLES"] = FLUID_NAME_NEIGHBORRATIO_PARTICLES;
  mRNAMap["NAME_TRAPPEDAIR_PARTICLES"] = FLUID_NAME_TRAPPEDAIR_PARTICLES;
  mRNAMap["NAME_WAVECREST_PARTICLES"] = FLUID_NAME_WAVECREST_PARTICLES;
  mRNAMap["NAME_KINETICENERGY_PARTICLES"] = FLUID_NAME_KINETICENERGY_PARTICLES;

  /* Guiding object names. */
  mRNAMap["NAME_VELT"] = FLUID_NAME_VELT;
  mRNAMap["NAME_WEIGHTGUIDE"] = FLUID_NAME_WEIGHTGUIDE;
  mRNAMap["NAME_NUMGUIDES"] = FLUID_NAME_NUMGUIDES;
  mRNAMap["NAME_PHIGUIDEIN"] = FLUID_NAME_PHIGUIDEIN;
  mRNAMap["NAME_GUIDEVELC"] = FLUID_NAME_GUIDEVELC;
  mRNAMap["NAME_GUIDEVEL_X"] = FLUID_NAME_GUIDEVEL_X;
  mRNAMap["NAME_GUIDEVEL_Y"] = FLUID_NAME_GUIDEVEL_Y;
  mRNAMap["NAME_GUIDEVEL_Z"] = FLUID_NAME_GUIDEVEL_Z;
  mRNAMap["NAME_GUIDEVEL"] = FLUID_NAME_GUIDEVEL;

  /* Cache file names. */
  mRNAMap["NAME_CONFIG"] = FLUID_NAME_CONFIG;
  mRNAMap["NAME_DATA"] = FLUID_NAME_DATA;
  mRNAMap["NAME_NOISE"] = FLUID_NAME_NOISE;
  mRNAMap["NAME_MESH"] = FLUID_NAME_MESH;
  mRNAMap["NAME_PARTICLES"] = FLUID_NAME_PARTICLES;
  mRNAMap["NAME_GUIDING"] = FLUID_NAME_GUIDING;
}

string MANTA::getRealValue(const string &varName)
{
  unordered_map<string, string>::iterator it;
  it = mRNAMap.find(varName);

  if (it == mRNAMap.end()) {
    cerr << "Fluid Error -- variable " << varName << " not found in RNA map " << it->second
         << endl;
    return "";
  }

  return it->second;
}

string MANTA::parseLine(const string &line)
{
  if (line.size() == 0)
    return "";
  string res = "";
  int currPos = 0, start_del = 0, end_del = -1;
  bool readingVar = false;
  const char delimiter = '$';
  while (currPos < line.size()) {
    if (line[currPos] == delimiter && !readingVar) {
      readingVar = true;
      start_del = currPos + 1;
      res += line.substr(end_del + 1, currPos - end_del - 1);
    }
    else if (line[currPos] == delimiter && readingVar) {
      readingVar = false;
      end_del = currPos;
      res += getRealValue(line.substr(start_del, currPos - start_del));
    }
    currPos++;
  }
  res += line.substr(end_del + 1, line.size() - end_del);
  return res;
}

string MANTA::parseScript(const string &setup_string, FluidModifierData *mmd)
{
  if (MANTA::with_debug)
    cout << "MANTA::parseScript()" << endl;

  istringstream f(setup_string);
  ostringstream res;
  string line = "";

  // Update RNA map if modifier data is handed over
  if (mmd) {
    initializeRNAMap(mmd);
  }
  while (getline(f, line)) {
    res << parseLine(line) << "\n";
  }
  return res.str();
}

/* Dirty hack: Needed to format paths from python code that is run via PyRun_SimpleString */
static string escapeSlashes(string const &s)
{
  string result = "";
  for (string::const_iterator i = s.begin(), end = s.end(); i != end; ++i) {
    unsigned char c = *i;
    if (c == '\\')
      result += "\\\\";
    else
      result += c;
  }
  return result;
}

bool MANTA::writeConfiguration(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeConfiguration()" << endl;

  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_CONFIG);
  string format = FLUID_DOMAIN_EXTENSION_UNI;
  string file = getFile(mmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, format, framenr);

  /* Create 'config' subdir if it does not exist already. */
  BLI_dir_create_recursive(directory.c_str());

  gzFile gzf = (gzFile)BLI_gzopen(file.c_str(), "wb1");  // do some compression
  if (!gzf) {
    cerr << "Fluid Error -- Cannot open file " << file << endl;
    return false;
  }

  gzwrite(gzf, &mds->active_fields, sizeof(int));
  gzwrite(gzf, &mds->res, 3 * sizeof(int));
  gzwrite(gzf, &mds->dx, sizeof(float));
  gzwrite(gzf, &mds->dt, sizeof(float));
  gzwrite(gzf, &mds->p0, 3 * sizeof(float));
  gzwrite(gzf, &mds->p1, 3 * sizeof(float));
  gzwrite(gzf, &mds->dp0, 3 * sizeof(float));
  gzwrite(gzf, &mds->shift, 3 * sizeof(int));
  gzwrite(gzf, &mds->obj_shift_f, 3 * sizeof(float));
  gzwrite(gzf, &mds->obmat, 16 * sizeof(float));
  gzwrite(gzf, &mds->base_res, 3 * sizeof(int));
  gzwrite(gzf, &mds->res_min, 3 * sizeof(int));
  gzwrite(gzf, &mds->res_max, 3 * sizeof(int));
  gzwrite(gzf, &mds->active_color, 3 * sizeof(float));
  gzwrite(gzf, &mds->time_total, sizeof(int));
  gzwrite(gzf, &FLUID_CACHE_VERSION, 4 * sizeof(char));

  return (gzclose(gzf) == Z_OK);
}

bool MANTA::writeData(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeData()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_DATA);
  string volume_format = getCacheFileEnding(mds->cache_data_format);
  string resumable_cache = !(mds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  if (mUsingSmoke) {
    ss.str("");
    ss << "smoke_save_data_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  if (mUsingLiquid) {
    ss.str("");
    ss << "liquid_save_data_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  return runPythonString(pythonCommands);
}

bool MANTA::writeNoise(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeNoise()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_NOISE);
  string volume_format = getCacheFileEnding(mds->cache_data_format);
  string resumable_cache = !(mds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  if (mUsingSmoke && mUsingNoise) {
    ss.str("");
    ss << "smoke_save_noise_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  return runPythonString(pythonCommands);
}

bool MANTA::readConfiguration(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::readConfiguration()" << endl;

  FluidDomainSettings *mds = mmd->domain;
  float dummy;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_CONFIG);
  string format = FLUID_DOMAIN_EXTENSION_UNI;
  string file = getFile(mmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, format, framenr);

  if (!hasConfig(mmd, framenr))
    return false;

  gzFile gzf = (gzFile)BLI_gzopen(file.c_str(), "rb"); /* Do some compression. */
  if (!gzf) {
    cerr << "Fluid Error -- Cannot open file " << file << endl;
    return false;
  }

  gzread(gzf, &mds->active_fields, sizeof(int));
  gzread(gzf, &mds->res, 3 * sizeof(int));
  gzread(gzf, &mds->dx, sizeof(float));
  gzread(gzf, &dummy, sizeof(float)); /* dt not needed right now. */
  gzread(gzf, &mds->p0, 3 * sizeof(float));
  gzread(gzf, &mds->p1, 3 * sizeof(float));
  gzread(gzf, &mds->dp0, 3 * sizeof(float));
  gzread(gzf, &mds->shift, 3 * sizeof(int));
  gzread(gzf, &mds->obj_shift_f, 3 * sizeof(float));
  gzread(gzf, &mds->obmat, 16 * sizeof(float));
  gzread(gzf, &mds->base_res, 3 * sizeof(int));
  gzread(gzf, &mds->res_min, 3 * sizeof(int));
  gzread(gzf, &mds->res_max, 3 * sizeof(int));
  gzread(gzf, &mds->active_color, 3 * sizeof(float));
  gzread(gzf, &mds->time_total, sizeof(int));
  gzread(gzf, &mds->cache_id, 4 * sizeof(char)); /* Older caches might have no id. */

  mds->total_cells = mds->res[0] * mds->res[1] * mds->res[2];

  return (gzclose(gzf) == Z_OK);
}

bool MANTA::readData(FluidModifierData *mmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readData()" << endl;

  if (!mUsingSmoke && !mUsingLiquid)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;
  bool result = true;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_DATA);
  string volume_format = getCacheFileEnding(mds->cache_data_format);
  string resumable_cache = (!resumable) ? "False" : "True";

  /* Sanity check: Are cache files present? */
  if (!hasData(mmd, framenr))
    return false;

  if (mUsingSmoke) {
    ss.str("");
    ss << "smoke_load_data_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
    result &= runPythonString(pythonCommands);
  }
  if (mUsingLiquid) {
    ss.str("");
    ss << "liquid_load_data_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
    result &= runPythonString(pythonCommands);
  }
  return result;
}

bool MANTA::readNoise(FluidModifierData *mmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readNoise()" << endl;

  if (!mUsingSmoke || !mUsingNoise)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_NOISE);
  string resumable_cache = (resumable) ? "False" : "True";

  /* Support older caches which had more granular file format control. */
  char format = (!strcmp(mds->cache_id, FLUID_CACHE_VERSION)) ? mds->cache_data_format :
                                                                mds->cache_noise_format;
  string volume_format = getCacheFileEnding(format);

  /* Sanity check: Are cache files present? */
  if (!hasNoise(mmd, framenr))
    return false;

  ss.str("");
  ss << "smoke_load_noise_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
     << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::readMesh(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::readMesh()" << endl;

  if (!mUsingLiquid || !mUsingMesh)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_MESH);
  string mesh_format = getCacheFileEnding(mds->cache_mesh_format);
  string volume_format = getCacheFileEnding(mds->cache_data_format);

  /* Sanity check: Are cache files present? */
  if (!hasMesh(mmd, framenr))
    return false;

  ss.str("");
  ss << "liquid_load_mesh_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
     << ", '" << mesh_format << "')";
  pythonCommands.push_back(ss.str());

  if (mUsingMVel) {
    ss.str("");
    ss << "liquid_load_meshvel_" << mCurrentID << "('" << escapeSlashes(directory) << "', "
       << framenr << ", '" << volume_format << "')";
    pythonCommands.push_back(ss.str());
  }

  return runPythonString(pythonCommands);
}

bool MANTA::readParticles(FluidModifierData *mmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readParticles()" << endl;

  if (!mUsingLiquid)
    return false;
  if (!mUsingDrops && !mUsingBubbles && !mUsingFloats && !mUsingTracers)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  string directory = getDirectory(mmd, FLUID_DOMAIN_DIR_PARTICLES);
  string resumable_cache = (!resumable) ? "False" : "True";

  /* Support older caches which had more granular file format control. */
  char format = (!strcmp(mds->cache_id, FLUID_CACHE_VERSION)) ? mds->cache_data_format :
                                                                mds->cache_particle_format;
  string volume_format = getCacheFileEnding(format);

  /* Sanity check: Are cache files present? */
  if (!hasParticles(mmd, framenr))
    return false;

  ss.str("");
  ss << "liquid_load_particles_" << mCurrentID << "('" << escapeSlashes(directory) << "', "
     << framenr << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::readGuiding(FluidModifierData *mmd, int framenr, bool sourceDomain)
{
  if (with_debug)
    cout << "MANTA::readGuiding()" << endl;

  FluidDomainSettings *mds = mmd->domain;

  if (!mUsingGuiding)
    return false;
  if (!mds)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;

  string directory = (sourceDomain) ? getDirectory(mmd, FLUID_DOMAIN_DIR_DATA) :
                                      getDirectory(mmd, FLUID_DOMAIN_DIR_GUIDE);
  string volume_format = getCacheFileEnding(mds->cache_data_format);

  /* Sanity check: Are cache files present? */
  if (!hasGuiding(mmd, framenr, sourceDomain))
    return false;

  if (sourceDomain) {
    ss.str("");
    ss << "fluid_load_vel_" << mCurrentID << "('" << escapeSlashes(directory) << "', " << framenr
       << ", '" << volume_format << "')";
  }
  else {
    ss.str("");
    ss << "fluid_load_guiding_" << mCurrentID << "('" << escapeSlashes(directory) << "', "
       << framenr << ", '" << volume_format << "')";
  }
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeData(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeData()" << endl;

  string tmpString, finalString;
  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  char cacheDirData[FILE_MAX], cacheDirGuiding[FILE_MAX];
  cacheDirData[0] = '\0';
  cacheDirGuiding[0] = '\0';

  string volume_format = getCacheFileEnding(mds->cache_data_format);

  BLI_path_join(
      cacheDirData, sizeof(cacheDirData), mds->cache_directory, FLUID_DOMAIN_DIR_DATA, nullptr);
  BLI_path_join(cacheDirGuiding,
                sizeof(cacheDirGuiding),
                mds->cache_directory,
                FLUID_DOMAIN_DIR_GUIDE,
                nullptr);
  BLI_path_make_safe(cacheDirData);
  BLI_path_make_safe(cacheDirGuiding);

  ss.str("");
  ss << "bake_fluid_data_" << mCurrentID << "('" << escapeSlashes(cacheDirData) << "', " << framenr
     << ", '" << volume_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeNoise(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeNoise()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  char cacheDirNoise[FILE_MAX];
  cacheDirNoise[0] = '\0';

  string volume_format = getCacheFileEnding(mds->cache_data_format);

  BLI_path_join(
      cacheDirNoise, sizeof(cacheDirNoise), mds->cache_directory, FLUID_DOMAIN_DIR_NOISE, nullptr);
  BLI_path_make_safe(cacheDirNoise);

  ss.str("");
  ss << "bake_noise_" << mCurrentID << "('" << escapeSlashes(cacheDirNoise) << "', " << framenr
     << ", '" << volume_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeMesh(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeMesh()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  char cacheDirMesh[FILE_MAX];
  cacheDirMesh[0] = '\0';

  string volume_format = getCacheFileEnding(mds->cache_data_format);
  string mesh_format = getCacheFileEnding(mds->cache_mesh_format);

  BLI_path_join(
      cacheDirMesh, sizeof(cacheDirMesh), mds->cache_directory, FLUID_DOMAIN_DIR_MESH, nullptr);
  BLI_path_make_safe(cacheDirMesh);

  ss.str("");
  ss << "bake_mesh_" << mCurrentID << "('" << escapeSlashes(cacheDirMesh) << "', " << framenr
     << ", '" << volume_format << "', '" << mesh_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeParticles(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeParticles()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  char cacheDirParticles[FILE_MAX];
  cacheDirParticles[0] = '\0';

  string volume_format = getCacheFileEnding(mds->cache_data_format);
  string resumable_cache = !(mds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  BLI_path_join(cacheDirParticles,
                sizeof(cacheDirParticles),
                mds->cache_directory,
                FLUID_DOMAIN_DIR_PARTICLES,
                nullptr);
  BLI_path_make_safe(cacheDirParticles);

  ss.str("");
  ss << "bake_particles_" << mCurrentID << "('" << escapeSlashes(cacheDirParticles) << "', "
     << framenr << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeGuiding(FluidModifierData *mmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeGuiding()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *mds = mmd->domain;

  char cacheDirGuiding[FILE_MAX];
  cacheDirGuiding[0] = '\0';

  string volume_format = getCacheFileEnding(mds->cache_data_format);

  BLI_path_join(cacheDirGuiding,
                sizeof(cacheDirGuiding),
                mds->cache_directory,
                FLUID_DOMAIN_DIR_GUIDE,
                nullptr);
  BLI_path_make_safe(cacheDirGuiding);

  ss.str("");
  ss << "bake_guiding_" << mCurrentID << "('" << escapeSlashes(cacheDirGuiding) << "', " << framenr
     << ", '" << volume_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::updateVariables(FluidModifierData *mmd)
{
  string tmpString, finalString;
  vector<string> pythonCommands;

  tmpString += fluid_variables;
  if (mUsingSmoke)
    tmpString += smoke_variables;
  if (mUsingLiquid)
    tmpString += liquid_variables;
  if (mUsingGuiding)
    tmpString += fluid_variables_guiding;
  if (mUsingNoise) {
    tmpString += fluid_variables_noise;
    tmpString += smoke_variables_noise;
    tmpString += smoke_wavelet_noise;
  }
  if (mUsingDrops || mUsingBubbles || mUsingFloats || mUsingTracers) {
    tmpString += fluid_variables_particles;
    tmpString += liquid_variables_particles;
  }
  if (mUsingMesh)
    tmpString += fluid_variables_mesh;

  finalString = parseScript(tmpString, mmd);
  pythonCommands.push_back(finalString);

  return runPythonString(pythonCommands);
}

void MANTA::exportSmokeScript(FluidModifierData *mmd)
{
  if (with_debug)
    cout << "MANTA::exportSmokeScript()" << endl;

  char cacheDir[FILE_MAX] = "\0";
  char cacheDirScript[FILE_MAX] = "\0";

  FluidDomainSettings *mds = mmd->domain;

  BLI_path_join(
      cacheDir, sizeof(cacheDir), mds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT, nullptr);
  BLI_path_make_safe(cacheDir);
  /* Create 'script' subdir if it does not exist already */
  BLI_dir_create_recursive(cacheDir);
  BLI_path_join(
      cacheDirScript, sizeof(cacheDirScript), cacheDir, FLUID_DOMAIN_SMOKE_SCRIPT, nullptr);
  BLI_path_make_safe(cacheDir);

  bool noise = mds->flags & FLUID_DOMAIN_USE_NOISE;
  bool heat = mds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT;
  bool colors = mds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS;
  bool fire = mds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE;
  bool obstacle = mds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE;
  bool guiding = mds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE;
  bool invel = mds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL;
  bool outflow = mds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW;

  string manta_script;

  // Libraries
  manta_script += header_libraries + manta_import;

  // Variables
  manta_script += header_variables + fluid_variables + smoke_variables;
  if (noise) {
    manta_script += fluid_variables_noise + smoke_variables_noise;
  }
  if (guiding)
    manta_script += fluid_variables_guiding;

  // Solvers
  manta_script += header_solvers + fluid_solver;
  if (noise)
    manta_script += fluid_solver_noise;
  if (guiding)
    manta_script += fluid_solver_guiding;

  // Grids
  manta_script += header_grids + fluid_alloc + smoke_alloc;
  if (noise) {
    manta_script += smoke_alloc_noise;
    if (colors)
      manta_script += smoke_alloc_colors_noise;
    if (fire)
      manta_script += smoke_alloc_fire_noise;
  }
  if (heat)
    manta_script += smoke_alloc_heat;
  if (colors)
    manta_script += smoke_alloc_colors;
  if (fire)
    manta_script += smoke_alloc_fire;
  if (guiding)
    manta_script += fluid_alloc_guiding;
  if (obstacle)
    manta_script += fluid_alloc_obstacle;
  if (invel)
    manta_script += fluid_alloc_invel;
  if (outflow)
    manta_script += fluid_alloc_outflow;

  // Noise field
  if (noise)
    manta_script += smoke_wavelet_noise;

  // Time
  manta_script += header_time + fluid_time_stepping + fluid_adapt_time_step;

  // Import
  manta_script += header_import + fluid_file_import + fluid_cache_helper + smoke_load_data;
  if (noise)
    manta_script += smoke_load_noise;
  if (guiding)
    manta_script += fluid_load_guiding;

  // Pre/Post Steps
  manta_script += header_prepost + fluid_pre_step + fluid_post_step;

  // Steps
  manta_script += header_steps + smoke_adaptive_step + smoke_step;
  if (noise) {
    manta_script += smoke_step_noise;
  }

  // Main
  manta_script += header_main + smoke_standalone + fluid_standalone;

  // Fill in missing variables in script
  string final_script = MANTA::parseScript(manta_script, mmd);

  // Write script
  ofstream myfile;
  myfile.open(cacheDirScript);
  myfile << final_script;
  myfile.close();
}

void MANTA::exportLiquidScript(FluidModifierData *mmd)
{
  if (with_debug)
    cout << "MANTA::exportLiquidScript()" << endl;

  char cacheDir[FILE_MAX] = "\0";
  char cacheDirScript[FILE_MAX] = "\0";

  FluidDomainSettings *mds = mmd->domain;

  BLI_path_join(
      cacheDir, sizeof(cacheDir), mds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT, nullptr);
  BLI_path_make_safe(cacheDir);
  /* Create 'script' subdir if it does not exist already */
  BLI_dir_create_recursive(cacheDir);
  BLI_path_join(
      cacheDirScript, sizeof(cacheDirScript), cacheDir, FLUID_DOMAIN_LIQUID_SCRIPT, nullptr);
  BLI_path_make_safe(cacheDirScript);

  bool mesh = mds->flags & FLUID_DOMAIN_USE_MESH;
  bool drops = mds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY;
  bool bubble = mds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE;
  bool floater = mds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM;
  bool tracer = mds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER;
  bool obstacle = mds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE;
  bool fractions = mds->flags & FLUID_DOMAIN_USE_FRACTIONS;
  bool guiding = mds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE;
  bool invel = mds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL;
  bool outflow = mds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW;

  string manta_script;

  // Libraries
  manta_script += header_libraries + manta_import;

  // Variables
  manta_script += header_variables + fluid_variables + liquid_variables;
  if (mesh)
    manta_script += fluid_variables_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += fluid_variables_particles + liquid_variables_particles;
  if (guiding)
    manta_script += fluid_variables_guiding;

  // Solvers
  manta_script += header_solvers + fluid_solver;
  if (mesh)
    manta_script += fluid_solver_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += fluid_solver_particles;
  if (guiding)
    manta_script += fluid_solver_guiding;

  // Grids
  manta_script += header_grids + fluid_alloc + liquid_alloc;
  if (mesh)
    manta_script += liquid_alloc_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += liquid_alloc_particles;
  if (guiding)
    manta_script += fluid_alloc_guiding;
  if (obstacle)
    manta_script += fluid_alloc_obstacle;
  if (fractions)
    manta_script += fluid_alloc_fractions;
  if (invel)
    manta_script += fluid_alloc_invel;
  if (outflow)
    manta_script += fluid_alloc_outflow;

  // Domain init
  manta_script += header_gridinit + liquid_init_phi;

  // Time
  manta_script += header_time + fluid_time_stepping + fluid_adapt_time_step;

  // Import
  manta_script += header_import + fluid_file_import + fluid_cache_helper + liquid_load_data;
  if (mesh)
    manta_script += liquid_load_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += liquid_load_particles;
  if (guiding)
    manta_script += fluid_load_guiding;

  // Pre/Post Steps
  manta_script += header_prepost + fluid_pre_step + fluid_post_step;

  // Steps
  manta_script += header_steps + liquid_adaptive_step + liquid_step;
  if (mesh)
    manta_script += liquid_step_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += liquid_step_particles;

  // Main
  manta_script += header_main + liquid_standalone + fluid_standalone;

  // Fill in missing variables in script
  string final_script = MANTA::parseScript(manta_script, mmd);

  // Write script
  ofstream myfile;
  myfile.open(cacheDirScript);
  myfile << final_script;
  myfile.close();
}

/* Call Mantaflow Python functions through this function. Use isAttribute for object attributes,
 * e.g. s.cfl (here 's' is varname, 'cfl' functionName, and isAttribute true) or
 *      grid.getDataPointer (here 's' is varname, 'getDataPointer' functionName, and isAttribute
 * false)
 *
 * Important! Return value: New reference or nullptr
 * Caller of this function needs to handle reference count of returned object. */
static PyObject *callPythonFunction(string varName, string functionName, bool isAttribute = false)
{
  if ((varName == "") || (functionName == "")) {
    if (MANTA::with_debug)
      cout << "Fluid: Missing Python variable name and/or function name -- name is: " << varName
           << ", function name is: " << functionName << endl;
    return nullptr;
  }

  PyGILState_STATE gilstate = PyGILState_Ensure();
  PyObject *var = nullptr, *func = nullptr, *returnedValue = nullptr;

  /* Be sure to initialize Python before using it. */
  Py_Initialize();

  // Get pyobject that holds result value
  if (!manta_main_module) {
    PyGILState_Release(gilstate);
    return nullptr;
  }

  var = PyObject_GetAttrString(manta_main_module, varName.c_str());
  if (!var) {
    PyGILState_Release(gilstate);
    return nullptr;
  }

  func = PyObject_GetAttrString(var, functionName.c_str());

  Py_DECREF(var);
  if (!func) {
    PyGILState_Release(gilstate);
    return nullptr;
  }

  if (!isAttribute) {
    returnedValue = PyObject_CallObject(func, nullptr);
    Py_DECREF(func);
  }

  PyGILState_Release(gilstate);
  return (!isAttribute) ? returnedValue : func;
}

/* Argument of this function may be a nullptr.
 * If it's not function will handle the reference count decrement of that argument. */
static void *pyObjectToPointer(PyObject *inputObject)
{
  if (!inputObject)
    return nullptr;

  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *encoded = PyUnicode_AsUTF8String(inputObject);
  char *result = PyBytes_AsString(encoded);

  Py_DECREF(inputObject);

  string str(result);
  istringstream in(str);
  void *dataPointer = nullptr;
  in >> dataPointer;

  Py_DECREF(encoded);

  PyGILState_Release(gilstate);
  return dataPointer;
}

/* Argument of this function may be a nullptr.
 * If it's not function will handle the reference count decrement of that argument. */
static double pyObjectToDouble(PyObject *inputObject)
{
  if (!inputObject)
    return 0.0;

  PyGILState_STATE gilstate = PyGILState_Ensure();

  /* Cannot use PyFloat_AsDouble() since its error check crashes.
   * Likely because of typedef 'Real' for 'float' types in Mantaflow. */
  double result = PyFloat_AS_DOUBLE(inputObject);
  Py_DECREF(inputObject);

  PyGILState_Release(gilstate);
  return result;
}

/* Argument of this function may be a nullptr.
 * If it's not function will handle the reference count decrement of that argument. */
static long pyObjectToLong(PyObject *inputObject)
{
  if (!inputObject)
    return 0;

  PyGILState_STATE gilstate = PyGILState_Ensure();

  long result = PyLong_AsLong(inputObject);
  Py_DECREF(inputObject);

  PyGILState_Release(gilstate);
  return result;
}

int MANTA::getFrame()
{
  if (with_debug)
    cout << "MANTA::getFrame()" << endl;

  string func = "frame";
  string id = to_string(mCurrentID);
  string solver = "s" + id;

  return pyObjectToLong(callPythonFunction(solver, func, true));
}

float MANTA::getTimestep()
{
  if (with_debug)
    cout << "MANTA::getTimestep()" << endl;

  string func = "timestep";
  string id = to_string(mCurrentID);
  string solver = "s" + id;

  return (float)pyObjectToDouble(callPythonFunction(solver, func, true));
}

bool MANTA::needsRealloc(FluidModifierData *mmd)
{
  FluidDomainSettings *mds = mmd->domain;
  return (mds->res[0] != mResX || mds->res[1] != mResY || mds->res[2] != mResZ);
}

void MANTA::adaptTimestep()
{
  if (with_debug)
    cout << "MANTA::adaptTimestep()" << endl;

  vector<string> pythonCommands;
  ostringstream ss;

  ss << "fluid_adapt_time_step_" << mCurrentID << "()";
  pythonCommands.push_back(ss.str());

  runPythonString(pythonCommands);
}

void MANTA::updatePointers()
{
  if (with_debug)
    cout << "MANTA::updatePointers()" << endl;

  string func = "getDataPointer";
  string funcNodes = "getNodesDataPointer";
  string funcTris = "getTrisDataPointer";

  string id = to_string(mCurrentID);
  string solver = "s" + id;
  string parts = "pp" + id;
  string snd = "sp" + id;
  string mesh = "sm" + id;
  string mesh2 = "mesh" + id;
  string noise = "sn" + id;
  string solver_ext = "_" + solver;
  string parts_ext = "_" + parts;
  string snd_ext = "_" + snd;
  string mesh_ext = "_" + mesh;
  string mesh_ext2 = "_" + mesh2;
  string noise_ext = "_" + noise;

  mFlags = (int *)pyObjectToPointer(callPythonFunction("flags" + solver_ext, func));
  mPhiIn = (float *)pyObjectToPointer(callPythonFunction("phiIn" + solver_ext, func));
  mPhiStaticIn = (float *)pyObjectToPointer(callPythonFunction("phiSIn" + solver_ext, func));
  mVelocityX = (float *)pyObjectToPointer(callPythonFunction("x_vel" + solver_ext, func));
  mVelocityY = (float *)pyObjectToPointer(callPythonFunction("y_vel" + solver_ext, func));
  mVelocityZ = (float *)pyObjectToPointer(callPythonFunction("z_vel" + solver_ext, func));
  mForceX = (float *)pyObjectToPointer(callPythonFunction("x_force" + solver_ext, func));
  mForceY = (float *)pyObjectToPointer(callPythonFunction("y_force" + solver_ext, func));
  mForceZ = (float *)pyObjectToPointer(callPythonFunction("z_force" + solver_ext, func));

  if (mUsingOutflow) {
    mPhiOutIn = (float *)pyObjectToPointer(callPythonFunction("phiOutIn" + solver_ext, func));
    mPhiOutStaticIn = (float *)pyObjectToPointer(
        callPythonFunction("phiOutSIn" + solver_ext, func));
  }
  if (mUsingObstacle) {
    mPhiObsIn = (float *)pyObjectToPointer(callPythonFunction("phiObsIn" + solver_ext, func));
    mPhiObsStaticIn = (float *)pyObjectToPointer(
        callPythonFunction("phiObsSIn" + solver_ext, func));
    mObVelocityX = (float *)pyObjectToPointer(callPythonFunction("x_obvel" + solver_ext, func));
    mObVelocityY = (float *)pyObjectToPointer(callPythonFunction("y_obvel" + solver_ext, func));
    mObVelocityZ = (float *)pyObjectToPointer(callPythonFunction("z_obvel" + solver_ext, func));
    mNumObstacle = (float *)pyObjectToPointer(callPythonFunction("numObs" + solver_ext, func));
  }
  if (mUsingGuiding) {
    mPhiGuideIn = (float *)pyObjectToPointer(callPythonFunction("phiGuideIn" + solver_ext, func));
    mGuideVelocityX = (float *)pyObjectToPointer(
        callPythonFunction("x_guidevel" + solver_ext, func));
    mGuideVelocityY = (float *)pyObjectToPointer(
        callPythonFunction("y_guidevel" + solver_ext, func));
    mGuideVelocityZ = (float *)pyObjectToPointer(
        callPythonFunction("z_guidevel" + solver_ext, func));
    mNumGuide = (float *)pyObjectToPointer(callPythonFunction("numGuides" + solver_ext, func));
  }
  if (mUsingInvel) {
    mInVelocityX = (float *)pyObjectToPointer(callPythonFunction("x_invel" + solver_ext, func));
    mInVelocityY = (float *)pyObjectToPointer(callPythonFunction("y_invel" + solver_ext, func));
    mInVelocityZ = (float *)pyObjectToPointer(callPythonFunction("z_invel" + solver_ext, func));
  }
  if (mUsingSmoke) {
    mDensity = (float *)pyObjectToPointer(callPythonFunction("density" + solver_ext, func));
    mDensityIn = (float *)pyObjectToPointer(callPythonFunction("densityIn" + solver_ext, func));
    mShadow = (float *)pyObjectToPointer(callPythonFunction("shadow" + solver_ext, func));
    mEmissionIn = (float *)pyObjectToPointer(callPythonFunction("emissionIn" + solver_ext, func));
  }
  if (mUsingSmoke && mUsingHeat) {
    mHeat = (float *)pyObjectToPointer(callPythonFunction("heat" + solver_ext, func));
    mHeatIn = (float *)pyObjectToPointer(callPythonFunction("heatIn" + solver_ext, func));
  }
  if (mUsingSmoke && mUsingFire) {
    mFlame = (float *)pyObjectToPointer(callPythonFunction("flame" + solver_ext, func));
    mFuel = (float *)pyObjectToPointer(callPythonFunction("fuel" + solver_ext, func));
    mReact = (float *)pyObjectToPointer(callPythonFunction("react" + solver_ext, func));
    mFuelIn = (float *)pyObjectToPointer(callPythonFunction("fuelIn" + solver_ext, func));
    mReactIn = (float *)pyObjectToPointer(callPythonFunction("reactIn" + solver_ext, func));
  }
  if (mUsingSmoke && mUsingColors) {
    mColorR = (float *)pyObjectToPointer(callPythonFunction("color_r" + solver_ext, func));
    mColorG = (float *)pyObjectToPointer(callPythonFunction("color_g" + solver_ext, func));
    mColorB = (float *)pyObjectToPointer(callPythonFunction("color_b" + solver_ext, func));
    mColorRIn = (float *)pyObjectToPointer(callPythonFunction("color_r_in" + solver_ext, func));
    mColorGIn = (float *)pyObjectToPointer(callPythonFunction("color_g_in" + solver_ext, func));
    mColorBIn = (float *)pyObjectToPointer(callPythonFunction("color_b_in" + solver_ext, func));
  }
  if (mUsingSmoke && mUsingNoise) {
    mDensityHigh = (float *)pyObjectToPointer(callPythonFunction("density" + noise_ext, func));
    mTextureU = (float *)pyObjectToPointer(callPythonFunction("texture_u" + solver_ext, func));
    mTextureV = (float *)pyObjectToPointer(callPythonFunction("texture_v" + solver_ext, func));
    mTextureW = (float *)pyObjectToPointer(callPythonFunction("texture_w" + solver_ext, func));
    mTextureU2 = (float *)pyObjectToPointer(callPythonFunction("texture_u2" + solver_ext, func));
    mTextureV2 = (float *)pyObjectToPointer(callPythonFunction("texture_v2" + solver_ext, func));
    mTextureW2 = (float *)pyObjectToPointer(callPythonFunction("texture_w2" + solver_ext, func));
  }
  if (mUsingSmoke && mUsingNoise && mUsingFire) {
    mFlameHigh = (float *)pyObjectToPointer(callPythonFunction("flame" + noise_ext, func));
    mFuelHigh = (float *)pyObjectToPointer(callPythonFunction("fuel" + noise_ext, func));
    mReactHigh = (float *)pyObjectToPointer(callPythonFunction("react" + noise_ext, func));
  }
  if (mUsingSmoke && mUsingNoise && mUsingColors) {
    mColorRHigh = (float *)pyObjectToPointer(callPythonFunction("color_r" + noise_ext, func));
    mColorGHigh = (float *)pyObjectToPointer(callPythonFunction("color_g" + noise_ext, func));
    mColorBHigh = (float *)pyObjectToPointer(callPythonFunction("color_b" + noise_ext, func));
  }
  if (mUsingLiquid) {
    mPhi = (float *)pyObjectToPointer(callPythonFunction("phi" + solver_ext, func));
    mFlipParticleData = (vector<pData> *)pyObjectToPointer(
        callPythonFunction("pp" + solver_ext, func));
    mFlipParticleVelocity = (vector<pVel> *)pyObjectToPointer(
        callPythonFunction("pVel" + parts_ext, func));
  }
  if (mUsingLiquid && mUsingMesh) {
    mMeshNodes = (vector<Node> *)pyObjectToPointer(
        callPythonFunction("mesh" + mesh_ext, funcNodes));
    mMeshTriangles = (vector<Triangle> *)pyObjectToPointer(
        callPythonFunction("mesh" + mesh_ext, funcTris));
  }
  if (mUsingLiquid && mUsingMVel) {
    mMeshVelocities = (vector<pVel> *)pyObjectToPointer(
        callPythonFunction("mVel" + mesh_ext2, func));
  }
  if (mUsingLiquid && (mUsingDrops | mUsingBubbles | mUsingFloats | mUsingTracers)) {
    mSndParticleData = (vector<pData> *)pyObjectToPointer(
        callPythonFunction("ppSnd" + snd_ext, func));
    mSndParticleVelocity = (vector<pVel> *)pyObjectToPointer(
        callPythonFunction("pVelSnd" + parts_ext, func));
    mSndParticleLife = (vector<float> *)pyObjectToPointer(
        callPythonFunction("pLifeSnd" + parts_ext, func));
  }

  mFlipFromFile = false;
  mMeshFromFile = false;
  mParticlesFromFile = false;
  mSmokeFromFile = false;
  mNoiseFromFile = false;
}

bool MANTA::hasConfig(FluidModifierData *mmd, int framenr)
{
  string extension = FLUID_DOMAIN_EXTENSION_UNI;
  return BLI_exists(
      getFile(mmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, extension, framenr).c_str());
}

bool MANTA::hasData(FluidModifierData *mmd, int framenr)
{
  string extension = getCacheFileEnding(mmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(mmd, FLUID_DOMAIN_DIR_DATA, FLUID_NAME_DATA, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    string filename = (mUsingSmoke) ? FLUID_NAME_DENSITY : FLUID_NAME_PP;
    exists = BLI_exists(getFile(mmd, FLUID_DOMAIN_DIR_DATA, filename, extension, framenr).c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Data: " << exists << endl;

  return exists;
}

bool MANTA::hasNoise(FluidModifierData *mmd, int framenr)
{
  string extension = getCacheFileEnding(mmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(mmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_NOISE, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    extension = getCacheFileEnding(mmd->domain->cache_data_format);
    exists = BLI_exists(
        getFile(mmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_DENSITY_NOISE, extension, framenr)
            .c_str());
  }
  /* Check single file naming with deprecated extension. */
  if (!exists) {
    extension = getCacheFileEnding(mmd->domain->cache_noise_format);
    exists = BLI_exists(
        getFile(mmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_DENSITY_NOISE, extension, framenr)
            .c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Noise: " << exists << endl;

  return exists;
}

bool MANTA::hasMesh(FluidModifierData *mmd, int framenr)
{
  string extension = getCacheFileEnding(mmd->domain->cache_mesh_format);
  bool exists = BLI_exists(
      getFile(mmd, FLUID_DOMAIN_DIR_MESH, FLUID_NAME_MESH, extension, framenr).c_str());

  /* Check old file naming. */
  if (!exists) {
    exists = BLI_exists(
        getFile(mmd, FLUID_DOMAIN_DIR_MESH, FLUID_NAME_LMESH, extension, framenr).c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Mesh: " << exists << endl;

  return exists;
}

bool MANTA::hasParticles(FluidModifierData *mmd, int framenr)
{
  string extension = getCacheFileEnding(mmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(mmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PARTICLES, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    extension = getCacheFileEnding(mmd->domain->cache_data_format);
    exists = BLI_exists(
        getFile(mmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PP_PARTICLES, extension, framenr)
            .c_str());
  }
  /* Check single file naming with deprecated extension. */
  if (!exists) {
    extension = getCacheFileEnding(mmd->domain->cache_particle_format);
    exists = BLI_exists(
        getFile(mmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PP_PARTICLES, extension, framenr)
            .c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Particles: " << exists << endl;

  return exists;
}

bool MANTA::hasGuiding(FluidModifierData *mmd, int framenr, bool sourceDomain)
{
  string subdirectory = (sourceDomain) ? FLUID_DOMAIN_DIR_DATA : FLUID_DOMAIN_DIR_GUIDE;
  string filename = (sourceDomain) ? FLUID_NAME_VELOCITY : FLUID_NAME_GUIDEVEL;
  string extension = getCacheFileEnding(mmd->domain->cache_data_format);
  bool exists = BLI_exists(getFile(mmd, subdirectory, filename, extension, framenr).c_str());
  if (with_debug)
    cout << "Fluid: Has Guiding: " << exists << endl;

  return exists;
}

string MANTA::getDirectory(FluidModifierData *mmd, string subdirectory)
{
  char directory[FILE_MAX];
  BLI_path_join(
      directory, sizeof(directory), mmd->domain->cache_directory, subdirectory.c_str(), nullptr);
  BLI_path_make_safe(directory);
  return directory;
}

string MANTA::getFile(
    FluidModifierData *mmd, string subdirectory, string fname, string extension, int framenr)
{
  char targetFile[FILE_MAX];
  string path = getDirectory(mmd, subdirectory);
  string filename = fname + "_####" + extension;
  BLI_join_dirfile(targetFile, sizeof(targetFile), path.c_str(), filename.c_str());
  BLI_path_frame(targetFile, framenr, 0);
  return targetFile;
}
