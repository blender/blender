/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mantaflow
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <zlib.h>

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

MANTA::MANTA(int *res, FluidModifierData *fmd)
    : mCurrentID(++solverID), mMaxRes(fmd->domain->maxres)
{
  if (with_debug)
    cout << "FLUID: " << mCurrentID << " with res(" << res[0] << ", " << res[1] << ", " << res[2]
         << ")" << endl;

  FluidDomainSettings *fds = fmd->domain;
  fds->fluid = this;

  mUsingLiquid = (fds->type == FLUID_DOMAIN_TYPE_LIQUID);
  mUsingSmoke = (fds->type == FLUID_DOMAIN_TYPE_GAS);
  mUsingNoise = (fds->flags & FLUID_DOMAIN_USE_NOISE) && mUsingSmoke;
  mUsingFractions = (fds->flags & FLUID_DOMAIN_USE_FRACTIONS) && mUsingLiquid;
  mUsingMesh = (fds->flags & FLUID_DOMAIN_USE_MESH) && mUsingLiquid;
  mUsingDiffusion = (fds->flags & FLUID_DOMAIN_USE_DIFFUSION) && mUsingLiquid;
  mUsingViscosity = (fds->flags & FLUID_DOMAIN_USE_VISCOSITY) && mUsingLiquid;
  mUsingMVel = (fds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS) && mUsingLiquid;
  mUsingDrops = (fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY) && mUsingLiquid;
  mUsingBubbles = (fds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) && mUsingLiquid;
  mUsingFloats = (fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) && mUsingLiquid;
  mUsingTracers = (fds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER) && mUsingLiquid;

  mUsingHeat = (fds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT) && mUsingSmoke;
  mUsingFire = (fds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE) && mUsingSmoke;
  mUsingColors = (fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) && mUsingSmoke;
  mUsingObstacle = (fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE);
  mUsingGuiding = (fds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE);
  mUsingInvel = (fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL);
  mUsingOutflow = (fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW);

  /* Simulation constants */
  mResX = res[0]; /* Current size of domain (will adjust with adaptive domain). */
  mResY = res[1];
  mResZ = res[2];
  mTotalCells = mResX * mResY * mResZ;
  mResGuiding = fds->res;

  /* Smoke low res grids. */
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
  mPressure = nullptr;

  /* Smoke high res grids. */
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

  /* Fluid low res grids. */
  mPhiIn = nullptr;
  mPhiStaticIn = nullptr;
  mPhiOutIn = nullptr;
  mPhiOutStaticIn = nullptr;
  mPhi = nullptr;

  /* Mesh. */
  mMeshNodes = nullptr;
  mMeshTriangles = nullptr;
  mMeshVelocities = nullptr;

  /* Fluid obstacle. */
  mPhiObsIn = nullptr;
  mPhiObsStaticIn = nullptr;
  mNumObstacle = nullptr;
  mObVelocityX = nullptr;
  mObVelocityY = nullptr;
  mObVelocityZ = nullptr;

  /* Fluid guiding. */
  mPhiGuideIn = nullptr;
  mNumGuide = nullptr;
  mGuideVelocityX = nullptr;
  mGuideVelocityY = nullptr;
  mGuideVelocityZ = nullptr;

  /* Fluid initial velocity. */
  mInVelocityX = nullptr;
  mInVelocityY = nullptr;
  mInVelocityZ = nullptr;

  /* Secondary particles. */
  mFlipParticleData = nullptr;
  mFlipParticleVelocity = nullptr;
  mParticleData = nullptr;
  mParticleVelocity = nullptr;
  mParticleLife = nullptr;

  /* Cache read success indicators. */
  mFlipFromFile = false;
  mMeshFromFile = false;
  mParticlesFromFile = false;

  /* Setup Mantaflow in Python. */
  initializeMantaflow();

  /* Initializa RNA map with values that Python will need. */
  initializeRNAMap(fmd);

  bool initSuccess = true;
  /* Initialize Mantaflow variables in Python. */
  /* Liquid. */
  if (mUsingLiquid) {
    initSuccess &= initDomain();
    initSuccess &= initLiquid();
    if (mUsingObstacle)
      initSuccess &= initObstacle();
    if (mUsingInvel)
      initSuccess &= initInVelocity();
    if (mUsingOutflow)
      initSuccess &= initOutflow();

    if (mUsingDrops || mUsingBubbles || mUsingFloats || mUsingTracers) {
      mUpresParticle = fds->particle_scale;
      mResXParticle = mUpresParticle * mResX;
      mResYParticle = mUpresParticle * mResY;
      mResZParticle = mUpresParticle * mResZ;
      mTotalCellsParticles = mResXParticle * mResYParticle * mResZParticle;

      initSuccess &= initSndParts();
      initSuccess &= initLiquidSndParts();
    }

    if (mUsingMesh) {
      mUpresMesh = fds->mesh_scale;
      mResXMesh = mUpresMesh * mResX;
      mResYMesh = mUpresMesh * mResY;
      mResZMesh = mUpresMesh * mResZ;
      mTotalCellsMesh = mResXMesh * mResYMesh * mResZMesh;

      /* Initialize Mantaflow variables in Python. */
      initSuccess &= initMesh();
      initSuccess &= initLiquidMesh();
    }

    if (mUsingViscosity) {
      initSuccess &= initLiquidViscosity();
    }

    if (mUsingDiffusion) {
      initSuccess &= initCurvature();
    }

    if (mUsingGuiding) {
      mResGuiding = (fds->guide_parent) ? fds->guide_res : fds->res;
      initSuccess &= initGuiding();
    }
    if (mUsingFractions) {
      initSuccess &= initFractions();
    }
  }

  /* Smoke. */
  if (mUsingSmoke) {
    initSuccess &= initDomain();
    initSuccess &= initSmoke();
    if (mUsingHeat)
      initSuccess &= initHeat();
    if (mUsingFire)
      initSuccess &= initFire();
    if (mUsingColors)
      initSuccess &= initColors();
    if (mUsingObstacle)
      initSuccess &= initObstacle();
    if (mUsingInvel)
      initSuccess &= initInVelocity();
    if (mUsingOutflow)
      initSuccess &= initOutflow();

    if (mUsingGuiding) {
      mResGuiding = (fds->guide_parent) ? fds->guide_res : fds->res;
      initSuccess &= initGuiding();
    }

    if (mUsingNoise) {
      int amplify = fds->noise_scale;
      mResXNoise = amplify * mResX;
      mResYNoise = amplify * mResY;
      mResZNoise = amplify * mResZ;
      mTotalCellsHigh = mResXNoise * mResYNoise * mResZNoise;

      /* Initialize Mantaflow variables in Python. */
      initSuccess &= initNoise();
      initSuccess &= initSmokeNoise();
      if (mUsingFire)
        initSuccess &= initFireHigh();
      if (mUsingColors)
        initSuccess &= initColorsHigh();
    }
  }
  /* All requested initializations must not fail in constructor. */
  BLI_assert(initSuccess);
  (void)initSuccess; /* Ignored in release. */

  updatePointers(fmd);
}

bool MANTA::initDomain(FluidModifierData *fmd)
{
  /* Vector will hold all python commands that are to be executed. */
  vector<string> pythonCommands;

  /* Set manta debug level first. */
  pythonCommands.push_back(manta_import + manta_debuglevel);

  ostringstream ss;
  ss << "set_manta_debuglevel(" << with_debug << ")";
  pythonCommands.push_back(ss.str());

  /* Now init basic fluid domain. */
  string tmpString = fluid_variables + fluid_solver + fluid_alloc + fluid_cache_helper +
                     fluid_bake_multiprocessing + fluid_bake_data + fluid_bake_noise +
                     fluid_bake_mesh + fluid_bake_particles + fluid_bake_guiding +
                     fluid_file_import + fluid_file_export + fluid_pre_step + fluid_post_step +
                     fluid_adapt_time_step + fluid_time_stepping;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);
  return runPythonString(pythonCommands);
}

bool MANTA::initNoise(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_noise + fluid_solver_noise;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  return runPythonString(pythonCommands);
}

bool MANTA::initSmoke(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = smoke_variables + smoke_alloc + smoke_adaptive_step + smoke_save_data +
                     smoke_load_data + smoke_step;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  return runPythonString(pythonCommands);
}

bool MANTA::initSmokeNoise(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = smoke_variables_noise + smoke_alloc_noise + smoke_wavelet_noise +
                     smoke_save_noise + smoke_load_noise + smoke_step_noise;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  mUsingNoise = true;
  return runPythonString(pythonCommands);
}

bool MANTA::initHeat(FluidModifierData *fmd)
{
  if (!mHeat) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_heat + smoke_with_heat;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingHeat = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initFire(FluidModifierData *fmd)
{
  if (!mFuel) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_fire + smoke_with_fire;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingFire = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initFireHigh(FluidModifierData *fmd)
{
  if (!mFuelHigh) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_fire_noise + smoke_with_fire;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingFire = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initColors(FluidModifierData *fmd)
{
  if (!mColorR) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_colors + smoke_init_colors + smoke_with_colors;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingColors = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initColorsHigh(FluidModifierData *fmd)
{
  if (!mColorRHigh) {
    vector<string> pythonCommands;
    string tmpString = smoke_alloc_colors_noise + smoke_init_colors_noise + smoke_with_colors;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingColors = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initLiquid(FluidModifierData *fmd)
{
  if (!mPhiIn) {
    vector<string> pythonCommands;
    string tmpString = liquid_variables + liquid_alloc + liquid_init_phi + liquid_save_data +
                       liquid_load_data + liquid_adaptive_step + liquid_step;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    mUsingLiquid = true;
    return runPythonString(pythonCommands);
  }
  return false;
}

bool MANTA::initMesh(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_mesh + fluid_solver_mesh + liquid_load_mesh;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  mUsingMesh = true;
  return runPythonString(pythonCommands);
}

bool MANTA::initLiquidMesh(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = liquid_alloc_mesh + liquid_step_mesh + liquid_save_mesh;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  mUsingMesh = true;
  return runPythonString(pythonCommands);
}

bool MANTA::initLiquidViscosity(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_viscosity + fluid_solver_viscosity + liquid_alloc_viscosity;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  mUsingViscosity = true;
  return runPythonString(pythonCommands);
}

bool MANTA::initCurvature(FluidModifierData *fmd)
{
  std::vector<std::string> pythonCommands;
  std::string finalString = parseScript(liquid_alloc_curvature, fmd);
  pythonCommands.push_back(finalString);

  mUsingDiffusion = true;
  return runPythonString(pythonCommands);
}

bool MANTA::initObstacle(FluidModifierData *fmd)
{
  if (!mPhiObsIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_obstacle + fluid_with_obstacle;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    return (mUsingObstacle = runPythonString(pythonCommands));
  }
  return false;
}

bool MANTA::initGuiding(FluidModifierData *fmd)
{
  if (!mPhiGuideIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_variables_guiding + fluid_solver_guiding + fluid_alloc_guiding +
                       fluid_save_guiding + fluid_load_vel + fluid_load_guiding;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    return (mUsingGuiding = runPythonString(pythonCommands));
  }
  return false;
}

bool MANTA::initFractions(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_alloc_fractions + fluid_with_fractions;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  return (mUsingFractions = runPythonString(pythonCommands));
}

bool MANTA::initInVelocity(FluidModifierData *fmd)
{
  if (!mInVelocityX) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_invel + fluid_with_invel;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    return (mUsingInvel = runPythonString(pythonCommands));
  }
  return false;
}

bool MANTA::initOutflow(FluidModifierData *fmd)
{
  if (!mPhiOutIn) {
    vector<string> pythonCommands;
    string tmpString = fluid_alloc_outflow + fluid_with_outflow;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    return (mUsingOutflow = runPythonString(pythonCommands));
  }
  return false;
}

bool MANTA::initSndParts(FluidModifierData *fmd)
{
  vector<string> pythonCommands;
  string tmpString = fluid_variables_particles + fluid_solver_particles;
  string finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  return runPythonString(pythonCommands);
}

bool MANTA::initLiquidSndParts(FluidModifierData *fmd)
{
  if (!mParticleData) {
    vector<string> pythonCommands;
    string tmpString = liquid_alloc_particles + liquid_variables_particles +
                       liquid_step_particles + fluid_with_sndparts + liquid_load_particles +
                       liquid_save_particles;
    string finalString = parseScript(tmpString, fmd);
    pythonCommands.push_back(finalString);

    return runPythonString(pythonCommands);
  }
  return false;
}

MANTA::~MANTA()
{
  if (with_debug)
    cout << "~FLUID: " << mCurrentID << " with res(" << mResX << ", " << mResY << ", " << mResZ
         << ")" << endl;

  /* Destruction string for Python. */
  string tmpString = "";
  vector<string> pythonCommands;
  bool result = false;

  tmpString += manta_import;
  tmpString += fluid_delete_all;

  /* Initializa RNA map with values that Python will need. */
  initializeRNAMap();

  /* Leave out fmd argument in parseScript since only looking up IDs. */
  string finalString = parseScript(tmpString);
  pythonCommands.push_back(finalString);
  result = runPythonString(pythonCommands);

  /* WARNING: this causes crash on exit in the `cycles_volume_cpu/smoke_color` test,
   * freeing a single modifier ends up clearing the shared module.
   * For this to be handled properly there would need to be a initialize/free
   * function for global data. */
#if 0
  MANTA::terminateMantaflow();
#endif

  BLI_assert(result);
  UNUSED_VARS(result);
}

/**
 * Copied from `PyC_DefaultNameSpace` in Blender.
 * with some differences:
 * - Doesn't touch `sys.modules`, use #manta_python_main_module_activate instead.
 * - Returns the module instead of the modules `dict`.
 * */
static PyObject *manta_python_main_module_create(const char *filename)
{
  PyObject *builtins = PyEval_GetBuiltins();
  PyObject *mod_main = PyModule_New("__main__");
  PyModule_AddStringConstant(mod_main, "__name__", "__main__");
  if (filename) {
    /* __file__ mainly for nice UI'ness
     * NOTE: this won't map to a real file when executing text-blocks and buttons. */
    PyModule_AddObject(mod_main, "__file__", PyUnicode_InternFromString(filename));
  }
  PyModule_AddObject(mod_main, "__builtins__", builtins);
  Py_INCREF(builtins); /* AddObject steals a reference */
  return mod_main;
}

static void manta_python_main_module_activate(PyObject *mod_main)
{
  PyObject *modules = PyImport_GetModuleDict();
  PyObject *main_mod_cmp = PyDict_GetItemString(modules, "__main__");
  if (mod_main == main_mod_cmp) {
    return;
  }
  /* NOTE: we could remove the reference to `mod_main` here, but as it's know to be removed
   * accept that there is temporarily an extra reference. */
  PyDict_SetItemString(modules, "__main__", mod_main);
}

static void manta_python_main_module_backup(PyObject **r_main_mod)
{
  PyObject *modules = PyImport_GetModuleDict();
  *r_main_mod = PyDict_GetItemString(modules, "__main__");
  Py_XINCREF(*r_main_mod); /* don't free */
}

static void manta_python_main_module_restore(PyObject *main_mod)
{
  PyObject *modules = PyImport_GetModuleDict();
  PyDict_SetItemString(modules, "__main__", main_mod);
  Py_XDECREF(main_mod);
}

/**
 * Mantaflow stores many variables in the globals() dict of the __main__ module. To be able to
 * access these variables, the same __main__ module has to be used every time.
 *
 * Unfortunately, we also depend on the fact that mantaflow dumps variables into this module using
 * #PyRun_String. So we can't easily create a separate module without changing mantaflow.
 */
static PyObject *manta_main_module = nullptr;

static void manta_python_main_module_clear()
{
  if (manta_main_module) {
    Py_DECREF(manta_main_module);
    manta_main_module = nullptr;
  }
}

static PyObject *manta_python_main_module_ensure()
{
  if (!manta_main_module) {
    manta_main_module = manta_python_main_module_create("<manta_namespace>");
  }
  return manta_main_module;
}

bool MANTA::runPythonString(vector<string> commands)
{
  bool success = true;
  PyGILState_STATE gilstate = PyGILState_Ensure();

  /* Temporarily set `sys.modules["__main__"]` as some Python modules expect this. */
  PyObject *main_mod_backup;
  manta_python_main_module_backup(&main_mod_backup);

  /* If we never want to run this when the module isn't initialize,
   * assign with `manta_python_main_module_ensure()`. */
  BLI_assert(manta_main_module != nullptr);
  manta_python_main_module_activate(manta_main_module);

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

  manta_python_main_module_restore(main_mod_backup);

  PyGILState_Release(gilstate);

  BLI_assert(success);
  return success;
}

void MANTA::initializeMantaflow()
{
  if (with_debug)
    cout << "Fluid: Initializing Mantaflow framework" << endl;

  string filename = "manta_scene_" + to_string(mCurrentID) + ".py";
  vector<string> fill = vector<string>();

  /* Initialize extension classes and wrappers. */
  srand(0);
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *manta_main_module = manta_python_main_module_ensure();
  PyObject *globals_dict = PyModule_GetDict(manta_main_module);
  Pb::setup(false, filename, fill, globals_dict); /* Namespace from Mantaflow (registry). */
  PyGILState_Release(gilstate);
}

void MANTA::terminateMantaflow()
{
  if (with_debug)
    cout << "Fluid: Releasing Mantaflow framework" << endl;

  PyGILState_STATE gilstate = PyGILState_Ensure();
  Pb::finalize(false); /* Namespace from Mantaflow (registry). */
  manta_python_main_module_clear();
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

void MANTA::initializeRNAMap(FluidModifierData *fmd)
{
  if (with_debug)
    cout << "MANTA::initializeRNAMap()" << endl;

  mRNAMap["ID"] = to_string(mCurrentID);

  if (!fmd) {
    if (with_debug)
      cout << "Fluid: No modifier data given in RNA map setup - returning early" << endl;
    return;
  }

  FluidDomainSettings *fds = fmd->domain;
  bool is2D = (fds->solver_res == 2);

  string borderCollisions = "";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_LEFT) == 0)
    borderCollisions += "x";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_RIGHT) == 0)
    borderCollisions += "X";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_FRONT) == 0)
    borderCollisions += "y";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_BACK) == 0)
    borderCollisions += "Y";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_BOTTOM) == 0)
    borderCollisions += "z";
  if ((fds->border_collisions & FLUID_DOMAIN_BORDER_TOP) == 0)
    borderCollisions += "Z";

  string particleTypesStr = "";
  if (fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY)
    particleTypesStr += "PtypeSpray";
  if (fds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeBubble";
  }
  if (fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeFoam";
  }
  if (fds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER) {
    if (!particleTypesStr.empty())
      particleTypesStr += "|";
    particleTypesStr += "PtypeTracer";
  }
  if (particleTypesStr.empty())
    particleTypesStr = "0";

  int particleTypes = (FLUID_DOMAIN_PARTICLE_SPRAY | FLUID_DOMAIN_PARTICLE_BUBBLE |
                       FLUID_DOMAIN_PARTICLE_FOAM | FLUID_DOMAIN_PARTICLE_TRACER);

  string cacheDirectory(fds->cache_directory);

  float viscosity = fds->viscosity_base * pow(10.0f, -fds->viscosity_exponent);
  float domainSize = MAX3(fds->global_size[0], fds->global_size[1], fds->global_size[2]);

  string vdbCompressionMethod = "Compression_None";
  if (fds->openvdb_compression == VDB_COMPRESSION_NONE)
    vdbCompressionMethod = "Compression_None";
  else if (fds->openvdb_compression == VDB_COMPRESSION_ZIP)
    vdbCompressionMethod = "Compression_Zip";
  else if (fds->openvdb_compression == VDB_COMPRESSION_BLOSC)
    vdbCompressionMethod = "Compression_Blosc";

  string vdbPrecisionHalf = "Precision_Half";
  if (fds->openvdb_data_depth == VDB_PRECISION_FULL_FLOAT)
    vdbPrecisionHalf = "Precision_Full";
  else if (fds->openvdb_data_depth == VDB_PRECISION_HALF_FLOAT)
    vdbPrecisionHalf = "Precision_Half";
  else if (fds->openvdb_data_depth == VDB_PRECISION_MINI_FLOAT)
    vdbPrecisionHalf = "Precision_Mini";

  mRNAMap["USING_SMOKE"] = getBooleanString(fds->type == FLUID_DOMAIN_TYPE_GAS);
  mRNAMap["USING_LIQUID"] = getBooleanString(fds->type == FLUID_DOMAIN_TYPE_LIQUID);
  mRNAMap["USING_COLORS"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS);
  mRNAMap["USING_HEAT"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT);
  mRNAMap["USING_FIRE"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE);
  mRNAMap["USING_NOISE"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_NOISE);
  mRNAMap["USING_OBSTACLE"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE);
  mRNAMap["USING_GUIDING"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_GUIDE);
  mRNAMap["USING_INVEL"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL);
  mRNAMap["USING_OUTFLOW"] = getBooleanString(fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW);
  mRNAMap["USING_LOG_DISSOLVE"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_DISSOLVE_LOG);
  mRNAMap["USING_DISSOLVE"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_DISSOLVE);
  mRNAMap["DOMAIN_CLOSED"] = getBooleanString(borderCollisions.compare("") == 0);
  mRNAMap["CACHE_RESUMABLE"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE);
  mRNAMap["USING_ADAPTIVETIME"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_TIME);
  mRNAMap["USING_SPEEDVECTORS"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS);
  mRNAMap["USING_FRACTIONS"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_FRACTIONS);
  mRNAMap["DELETE_IN_OBSTACLE"] = getBooleanString(fds->flags & FLUID_DOMAIN_DELETE_IN_OBSTACLE);
  mRNAMap["USING_DIFFUSION"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_DIFFUSION);
  mRNAMap["USING_MESH"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_MESH);
  mRNAMap["USING_IMPROVED_MESH"] = getBooleanString(fds->mesh_generator ==
                                                    FLUID_DOMAIN_MESH_IMPROVED);
  mRNAMap["USING_SNDPARTS"] = getBooleanString(fds->particle_type & particleTypes);
  mRNAMap["SNDPARTICLE_BOUNDARY_DELETE"] = getBooleanString(fds->sndparticle_boundary ==
                                                            SNDPARTICLE_BOUNDARY_DELETE);
  mRNAMap["SNDPARTICLE_BOUNDARY_PUSHOUT"] = getBooleanString(fds->sndparticle_boundary ==
                                                             SNDPARTICLE_BOUNDARY_PUSHOUT);

  mRNAMap["SOLVER_DIM"] = to_string(fds->solver_res);
  mRNAMap["BOUND_CONDITIONS"] = borderCollisions;
  mRNAMap["BOUNDARY_WIDTH"] = to_string(fds->boundary_width);
  mRNAMap["RES"] = to_string(mMaxRes);
  mRNAMap["RESX"] = to_string(mResX);
  mRNAMap["RESY"] = (is2D) ? to_string(mResZ) : to_string(mResY);
  mRNAMap["RESZ"] = (is2D) ? to_string(1) : to_string(mResZ);
  mRNAMap["TIME_SCALE"] = to_string(fds->time_scale);
  mRNAMap["FRAME_LENGTH"] = to_string(fds->frame_length);
  mRNAMap["CFL"] = to_string(fds->cfl_condition);
  mRNAMap["DT"] = to_string(fds->dt);
  mRNAMap["TIMESTEPS_MIN"] = to_string(fds->timesteps_minimum);
  mRNAMap["TIMESTEPS_MAX"] = to_string(fds->timesteps_maximum);
  mRNAMap["TIME_TOTAL"] = to_string(fds->time_total);
  mRNAMap["TIME_PER_FRAME"] = to_string(fds->time_per_frame);
  mRNAMap["VORTICITY"] = to_string(fds->vorticity);
  mRNAMap["FLAME_VORTICITY"] = to_string(fds->flame_vorticity);
  mRNAMap["NOISE_SCALE"] = to_string(fds->noise_scale);
  mRNAMap["MESH_SCALE"] = to_string(fds->mesh_scale);
  mRNAMap["PARTICLE_SCALE"] = to_string(fds->particle_scale);
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
  mRNAMap["MIN_RESX"] = to_string(fds->res_min[0]);
  mRNAMap["MIN_RESY"] = to_string(fds->res_min[1]);
  mRNAMap["MIN_RESZ"] = to_string(fds->res_min[2]);
  mRNAMap["BASE_RESX"] = to_string(fds->base_res[0]);
  mRNAMap["BASE_RESY"] = to_string(fds->base_res[1]);
  mRNAMap["BASE_RESZ"] = to_string(fds->base_res[2]);
  mRNAMap["WLT_STR"] = to_string(fds->noise_strength);
  mRNAMap["NOISE_POSSCALE"] = to_string(fds->noise_pos_scale);
  mRNAMap["NOISE_TIMEANIM"] = to_string(fds->noise_time_anim);
  mRNAMap["COLOR_R"] = to_string(fds->active_color[0]);
  mRNAMap["COLOR_G"] = to_string(fds->active_color[1]);
  mRNAMap["COLOR_B"] = to_string(fds->active_color[2]);
  mRNAMap["BUOYANCY_ALPHA"] = to_string(fds->alpha);
  mRNAMap["BUOYANCY_BETA"] = to_string(fds->beta);
  mRNAMap["DISSOLVE_SPEED"] = to_string(fds->diss_speed);
  mRNAMap["BURNING_RATE"] = to_string(fds->burning_rate);
  mRNAMap["FLAME_SMOKE"] = to_string(fds->flame_smoke);
  mRNAMap["IGNITION_TEMP"] = to_string(fds->flame_ignition);
  mRNAMap["MAX_TEMP"] = to_string(fds->flame_max_temp);
  mRNAMap["FLAME_SMOKE_COLOR_X"] = to_string(fds->flame_smoke_color[0]);
  mRNAMap["FLAME_SMOKE_COLOR_Y"] = to_string(fds->flame_smoke_color[1]);
  mRNAMap["FLAME_SMOKE_COLOR_Z"] = to_string(fds->flame_smoke_color[2]);
  mRNAMap["CURRENT_FRAME"] = to_string(int(fmd->time));
  mRNAMap["START_FRAME"] = to_string(fds->cache_frame_start);
  mRNAMap["END_FRAME"] = to_string(fds->cache_frame_end);
  mRNAMap["CACHE_DATA_FORMAT"] = getCacheFileEnding(fds->cache_data_format);
  mRNAMap["CACHE_MESH_FORMAT"] = getCacheFileEnding(fds->cache_mesh_format);
  mRNAMap["CACHE_NOISE_FORMAT"] = getCacheFileEnding(fds->cache_noise_format);
  mRNAMap["CACHE_PARTICLE_FORMAT"] = getCacheFileEnding(fds->cache_particle_format);
  mRNAMap["USING_APIC"] = getBooleanString(fds->simulation_method == FLUID_DOMAIN_METHOD_APIC);
  mRNAMap["FLIP_RATIO"] = to_string(fds->flip_ratio);
  mRNAMap["PARTICLE_RANDOMNESS"] = to_string(fds->particle_randomness);
  mRNAMap["PARTICLE_NUMBER"] = to_string(fds->particle_number);
  mRNAMap["PARTICLE_MINIMUM"] = to_string(fds->particle_minimum);
  mRNAMap["PARTICLE_MAXIMUM"] = to_string(fds->particle_maximum);
  mRNAMap["PARTICLE_RADIUS"] = to_string(fds->particle_radius);
  mRNAMap["FRACTIONS_THRESHOLD"] = to_string(fds->fractions_threshold);
  mRNAMap["FRACTIONS_DISTANCE"] = to_string(fds->fractions_distance);
  mRNAMap["MESH_CONCAVE_UPPER"] = to_string(fds->mesh_concave_upper);
  mRNAMap["MESH_CONCAVE_LOWER"] = to_string(fds->mesh_concave_lower);
  mRNAMap["MESH_PARTICLE_RADIUS"] = to_string(fds->mesh_particle_radius);
  mRNAMap["MESH_SMOOTHEN_POS"] = to_string(fds->mesh_smoothen_pos);
  mRNAMap["MESH_SMOOTHEN_NEG"] = to_string(fds->mesh_smoothen_neg);
  mRNAMap["PARTICLE_BAND_WIDTH"] = to_string(fds->particle_band_width);
  mRNAMap["SNDPARTICLE_TAU_MIN_WC"] = to_string(fds->sndparticle_tau_min_wc);
  mRNAMap["SNDPARTICLE_TAU_MAX_WC"] = to_string(fds->sndparticle_tau_max_wc);
  mRNAMap["SNDPARTICLE_TAU_MIN_TA"] = to_string(fds->sndparticle_tau_min_ta);
  mRNAMap["SNDPARTICLE_TAU_MAX_TA"] = to_string(fds->sndparticle_tau_max_ta);
  mRNAMap["SNDPARTICLE_TAU_MIN_K"] = to_string(fds->sndparticle_tau_min_k);
  mRNAMap["SNDPARTICLE_TAU_MAX_K"] = to_string(fds->sndparticle_tau_max_k);
  mRNAMap["SNDPARTICLE_K_WC"] = to_string(fds->sndparticle_k_wc);
  mRNAMap["SNDPARTICLE_K_TA"] = to_string(fds->sndparticle_k_ta);
  mRNAMap["SNDPARTICLE_K_B"] = to_string(fds->sndparticle_k_b);
  mRNAMap["SNDPARTICLE_K_D"] = to_string(fds->sndparticle_k_d);
  mRNAMap["SNDPARTICLE_L_MIN"] = to_string(fds->sndparticle_l_min);
  mRNAMap["SNDPARTICLE_L_MAX"] = to_string(fds->sndparticle_l_max);
  mRNAMap["SNDPARTICLE_POTENTIAL_RADIUS"] = to_string(fds->sndparticle_potential_radius);
  mRNAMap["SNDPARTICLE_UPDATE_RADIUS"] = to_string(fds->sndparticle_update_radius);
  mRNAMap["LIQUID_SURFACE_TENSION"] = to_string(fds->surface_tension);
  mRNAMap["FLUID_VISCOSITY"] = to_string(viscosity);
  mRNAMap["FLUID_DOMAIN_SIZE"] = to_string(domainSize);
  mRNAMap["FLUID_DOMAIN_SIZE_X"] = to_string(fds->global_size[0]);
  mRNAMap["FLUID_DOMAIN_SIZE_Y"] = to_string(fds->global_size[1]);
  mRNAMap["FLUID_DOMAIN_SIZE_Z"] = to_string(fds->global_size[2]);
  mRNAMap["SNDPARTICLE_TYPES"] = particleTypesStr;
  mRNAMap["GUIDING_ALPHA"] = to_string(fds->guide_alpha);
  mRNAMap["GUIDING_BETA"] = to_string(fds->guide_beta);
  mRNAMap["GUIDING_FACTOR"] = to_string(fds->guide_vel_factor);
  mRNAMap["GRAVITY_X"] = to_string(fds->gravity_final[0]);
  mRNAMap["GRAVITY_Y"] = to_string(fds->gravity_final[1]);
  mRNAMap["GRAVITY_Z"] = to_string(fds->gravity_final[2]);
  mRNAMap["CACHE_DIR"] = cacheDirectory;
  mRNAMap["COMPRESSION_OPENVDB"] = vdbCompressionMethod;
  mRNAMap["PRECISION_OPENVDB"] = vdbPrecisionHalf;
  mRNAMap["CLIP_OPENVDB"] = to_string(fds->clipping);
  mRNAMap["PP_PARTICLE_MAXIMUM"] = to_string(fds->sys_particle_maximum);
  mRNAMap["USING_VISCOSITY"] = getBooleanString(fds->flags & FLUID_DOMAIN_USE_VISCOSITY);
  mRNAMap["VISCOSITY_VALUE"] = to_string(fds->viscosity_value);

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
  mRNAMap["NAME_VELOCITY_GUIDE"] = FLUID_NAME_VELOCITY_GUIDE;

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

string MANTA::parseScript(const string &setup_string, FluidModifierData *fmd)
{
  if (MANTA::with_debug)
    cout << "MANTA::parseScript()" << endl;

  istringstream f(setup_string);
  ostringstream res;
  string line = "";

  /* Update RNA map if modifier data is handed over. */
  if (fmd) {
    initializeRNAMap(fmd);
  }
  while (getline(f, line)) {
    res << parseLine(line) << "\n";
  }
  return res.str();
}

/** Dirty hack: Needed to format paths from python code that is run via #PyRun_String. */
static string escapePath(string const &s)
{
  string result = "";
  for (char c : s) {
    if (c == '\\') {
      result += "\\\\";
    }
    else if (c == '\'') {
      result += "\\\'";
    }
    else {
      result += c;
    }
  }
  return result;
}

bool MANTA::writeConfiguration(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeConfiguration()" << endl;

  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_CONFIG);
  string format = FLUID_DOMAIN_EXTENSION_UNI;
  string file = getFile(fmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, format, framenr);

  /* Create 'config' subdir if it does not exist already. */
  BLI_dir_create_recursive(directory.c_str());

  /* Open new file with some compression. */
  gzFile gzf = (gzFile)BLI_gzopen(file.c_str(), "wb1");
  if (!gzf) {
    cerr << "Fluid Error -- Cannot open file " << file << endl;
    return false;
  }

  gzwrite(gzf, &fds->active_fields, sizeof(int));
  gzwrite(gzf, &fds->res, 3 * sizeof(int));
  gzwrite(gzf, &fds->dx, sizeof(float));
  gzwrite(gzf, &fds->dt, sizeof(float));
  gzwrite(gzf, &fds->p0, 3 * sizeof(float));
  gzwrite(gzf, &fds->p1, 3 * sizeof(float));
  gzwrite(gzf, &fds->dp0, 3 * sizeof(float));
  gzwrite(gzf, &fds->shift, 3 * sizeof(int));
  gzwrite(gzf, &fds->obj_shift_f, 3 * sizeof(float));
  gzwrite(gzf, &fds->obmat, 16 * sizeof(float));
  gzwrite(gzf, &fds->base_res, 3 * sizeof(int));
  gzwrite(gzf, &fds->res_min, 3 * sizeof(int));
  gzwrite(gzf, &fds->res_max, 3 * sizeof(int));
  gzwrite(gzf, &fds->active_color, 3 * sizeof(float));
  gzwrite(gzf, &fds->time_total, sizeof(int));
  gzwrite(gzf, &FLUID_CACHE_VERSION, 4 * sizeof(char));

  return (gzclose(gzf) == Z_OK);
}

bool MANTA::writeData(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeData()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_DATA);
  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string resumable_cache = !(fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  if (mUsingSmoke) {
    ss.str("");
    ss << "smoke_save_data_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  if (mUsingLiquid) {
    ss.str("");
    ss << "liquid_save_data_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  return runPythonString(pythonCommands);
}

bool MANTA::writeNoise(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::writeNoise()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_NOISE);
  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string resumable_cache = !(fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  if (mUsingSmoke && mUsingNoise) {
    ss.str("");
    ss << "smoke_save_noise_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
  }
  return runPythonString(pythonCommands);
}

bool MANTA::readConfiguration(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::readConfiguration()" << endl;

  FluidDomainSettings *fds = fmd->domain;
  float dummy;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_CONFIG);
  string format = FLUID_DOMAIN_EXTENSION_UNI;
  string file = getFile(fmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, format, framenr);

  if (!hasConfig(fmd, framenr))
    return false;

  gzFile gzf = (gzFile)BLI_gzopen(file.c_str(), "rb"); /* Do some compression. */
  if (!gzf) {
    cerr << "Fluid Error -- Cannot open file " << file << endl;
    return false;
  }

  gzread(gzf, &fds->active_fields, sizeof(int));
  gzread(gzf, &fds->res, 3 * sizeof(int));
  gzread(gzf, &fds->dx, sizeof(float));
  gzread(gzf, &dummy, sizeof(float)); /* dt not needed right now. */
  gzread(gzf, &fds->p0, 3 * sizeof(float));
  gzread(gzf, &fds->p1, 3 * sizeof(float));
  gzread(gzf, &fds->dp0, 3 * sizeof(float));
  gzread(gzf, &fds->shift, 3 * sizeof(int));
  gzread(gzf, &fds->obj_shift_f, 3 * sizeof(float));
  gzread(gzf, &fds->obmat, 16 * sizeof(float));
  gzread(gzf, &fds->base_res, 3 * sizeof(int));
  gzread(gzf, &fds->res_min, 3 * sizeof(int));
  gzread(gzf, &fds->res_max, 3 * sizeof(int));
  gzread(gzf, &fds->active_color, 3 * sizeof(float));
  gzread(gzf, &fds->time_total, sizeof(int));
  gzread(gzf, &fds->cache_id, 4 * sizeof(char)); /* Older caches might have no id. */

  fds->total_cells = fds->res[0] * fds->res[1] * fds->res[2];

  return (gzclose(gzf) == Z_OK);
}

bool MANTA::readData(FluidModifierData *fmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readData()" << endl;

  if (!mUsingSmoke && !mUsingLiquid)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;
  bool result = true;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_DATA);
  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string resumable_cache = (!resumable) ? "False" : "True";

  /* Sanity check: Are cache files present? */
  if (!hasData(fmd, framenr))
    return false;

  if (mUsingSmoke) {
    ss.str("");
    ss << "smoke_load_data_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
    result &= runPythonString(pythonCommands);
    return (mSmokeFromFile = result);
  }
  if (mUsingLiquid) {
    ss.str("");
    ss << "liquid_load_data_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "', " << resumable_cache << ")";
    pythonCommands.push_back(ss.str());
    result &= runPythonString(pythonCommands);
    return (mFlipFromFile = result);
  }
  return result;
}

bool MANTA::readNoise(FluidModifierData *fmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readNoise()" << endl;

  if (!mUsingSmoke || !mUsingNoise)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_NOISE);
  string resumable_cache = (!resumable) ? "False" : "True";

  /* Support older caches which had more granular file format control. */
  char format = (!strcmp(fds->cache_id, FLUID_CACHE_VERSION)) ? fds->cache_data_format :
                                                                fds->cache_noise_format;
  string volume_format = getCacheFileEnding(format);

  /* Sanity check: Are cache files present? */
  if (!hasNoise(fmd, framenr))
    return false;

  ss.str("");
  ss << "smoke_load_noise_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
     << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return (mNoiseFromFile = runPythonString(pythonCommands));
}

bool MANTA::readMesh(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::readMesh()" << endl;

  if (!mUsingLiquid || !mUsingMesh)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_MESH);
  string mesh_format = getCacheFileEnding(fds->cache_mesh_format);
  string volume_format = getCacheFileEnding(fds->cache_data_format);

  /* Sanity check: Are cache files present? */
  if (!hasMesh(fmd, framenr))
    return false;

  ss.str("");
  ss << "liquid_load_mesh_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
     << ", '" << mesh_format << "')";
  pythonCommands.push_back(ss.str());

  if (mUsingMVel) {
    ss.str("");
    ss << "liquid_load_meshvel_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "')";
    pythonCommands.push_back(ss.str());
  }

  return (mMeshFromFile = runPythonString(pythonCommands));
}

bool MANTA::readParticles(FluidModifierData *fmd, int framenr, bool resumable)
{
  if (with_debug)
    cout << "MANTA::readParticles()" << endl;

  if (!mUsingLiquid)
    return false;
  if (!mUsingDrops && !mUsingBubbles && !mUsingFloats && !mUsingTracers)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = getDirectory(fmd, FLUID_DOMAIN_DIR_PARTICLES);
  string resumable_cache = (!resumable) ? "False" : "True";

  /* Support older caches which had more granular file format control. */
  char format = (!strcmp(fds->cache_id, FLUID_CACHE_VERSION)) ? fds->cache_data_format :
                                                                fds->cache_particle_format;
  string volume_format = getCacheFileEnding(format);

  /* Sanity check: Are cache files present? */
  if (!hasParticles(fmd, framenr))
    return false;

  ss.str("");
  ss << "liquid_load_particles_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
     << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return (mParticlesFromFile = runPythonString(pythonCommands));
}

bool MANTA::readGuiding(FluidModifierData *fmd, int framenr, bool sourceDomain)
{
  if (with_debug)
    cout << "MANTA::readGuiding()" << endl;

  if (!mUsingGuiding)
    return false;
  if (!fmd)
    return false;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  string directory = (sourceDomain) ? getDirectory(fmd, FLUID_DOMAIN_DIR_DATA) :
                                      getDirectory(fmd, FLUID_DOMAIN_DIR_GUIDE);
  string volume_format = getCacheFileEnding(fds->cache_data_format);

  /* Sanity check: Are cache files present? */
  if (!hasGuiding(fmd, framenr, sourceDomain))
    return false;

  if (sourceDomain) {
    ss.str("");
    ss << "fluid_load_vel_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "')";
  }
  else {
    ss.str("");
    ss << "fluid_load_guiding_" << mCurrentID << "('" << escapePath(directory) << "', " << framenr
       << ", '" << volume_format << "')";
  }
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeData(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeData()" << endl;

  string tmpString, finalString;
  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  char cacheDirData[FILE_MAX], cacheDirGuiding[FILE_MAX];
  cacheDirData[0] = '\0';
  cacheDirGuiding[0] = '\0';

  string volume_format = getCacheFileEnding(fds->cache_data_format);

  BLI_path_join(cacheDirData, sizeof(cacheDirData), fds->cache_directory, FLUID_DOMAIN_DIR_DATA);
  BLI_path_join(
      cacheDirGuiding, sizeof(cacheDirGuiding), fds->cache_directory, FLUID_DOMAIN_DIR_GUIDE);
  BLI_path_make_safe(cacheDirData);
  BLI_path_make_safe(cacheDirGuiding);

  ss.str("");
  ss << "bake_fluid_data_" << mCurrentID << "('" << escapePath(cacheDirData) << "', " << framenr
     << ", '" << volume_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeNoise(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeNoise()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  char cacheDirNoise[FILE_MAX];
  cacheDirNoise[0] = '\0';

  string volume_format = getCacheFileEnding(fds->cache_data_format);

  BLI_path_join(
      cacheDirNoise, sizeof(cacheDirNoise), fds->cache_directory, FLUID_DOMAIN_DIR_NOISE);
  BLI_path_make_safe(cacheDirNoise);

  ss.str("");
  ss << "bake_noise_" << mCurrentID << "('" << escapePath(cacheDirNoise) << "', " << framenr
     << ", '" << volume_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeMesh(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeMesh()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  char cacheDirMesh[FILE_MAX];
  cacheDirMesh[0] = '\0';

  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string mesh_format = getCacheFileEnding(fds->cache_mesh_format);

  BLI_path_join(cacheDirMesh, sizeof(cacheDirMesh), fds->cache_directory, FLUID_DOMAIN_DIR_MESH);
  BLI_path_make_safe(cacheDirMesh);

  ss.str("");
  ss << "bake_mesh_" << mCurrentID << "('" << escapePath(cacheDirMesh) << "', " << framenr << ", '"
     << volume_format << "', '" << mesh_format << "')";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeParticles(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeParticles()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  char cacheDirParticles[FILE_MAX];
  cacheDirParticles[0] = '\0';

  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string resumable_cache = !(fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  BLI_path_join(cacheDirParticles,
                sizeof(cacheDirParticles),
                fds->cache_directory,
                FLUID_DOMAIN_DIR_PARTICLES);
  BLI_path_make_safe(cacheDirParticles);

  ss.str("");
  ss << "bake_particles_" << mCurrentID << "('" << escapePath(cacheDirParticles) << "', "
     << framenr << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::bakeGuiding(FluidModifierData *fmd, int framenr)
{
  if (with_debug)
    cout << "MANTA::bakeGuiding()" << endl;

  ostringstream ss;
  vector<string> pythonCommands;
  FluidDomainSettings *fds = fmd->domain;

  char cacheDirGuiding[FILE_MAX];
  cacheDirGuiding[0] = '\0';

  string volume_format = getCacheFileEnding(fds->cache_data_format);
  string resumable_cache = !(fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE) ? "False" : "True";

  BLI_path_join(
      cacheDirGuiding, sizeof(cacheDirGuiding), fds->cache_directory, FLUID_DOMAIN_DIR_GUIDE);
  BLI_path_make_safe(cacheDirGuiding);

  ss.str("");
  ss << "bake_guiding_" << mCurrentID << "('" << escapePath(cacheDirGuiding) << "', " << framenr
     << ", '" << volume_format << "', " << resumable_cache << ")";
  pythonCommands.push_back(ss.str());

  return runPythonString(pythonCommands);
}

bool MANTA::updateVariables(FluidModifierData *fmd)
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

  finalString = parseScript(tmpString, fmd);
  pythonCommands.push_back(finalString);

  return runPythonString(pythonCommands);
}

bool MANTA::exportSmokeScript(FluidModifierData *fmd)
{
  if (with_debug)
    cout << "MANTA::exportSmokeScript()" << endl;

  char cacheDir[FILE_MAX] = "\0";
  char cacheDirScript[FILE_MAX] = "\0";

  FluidDomainSettings *fds = fmd->domain;

  BLI_path_join(cacheDir, sizeof(cacheDir), fds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT);
  BLI_path_make_safe(cacheDir);
  /* Create 'script' subdir if it does not exist already */
  BLI_dir_create_recursive(cacheDir);
  BLI_path_join(cacheDirScript, sizeof(cacheDirScript), cacheDir, FLUID_DOMAIN_SMOKE_SCRIPT);
  BLI_path_make_safe(cacheDir);

  bool noise = fds->flags & FLUID_DOMAIN_USE_NOISE;
  bool heat = fds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT;
  bool colors = fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS;
  bool fire = fds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE;
  bool obstacle = fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE;
  bool guiding = fds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE;
  bool invel = fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL;
  bool outflow = fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW;

  string manta_script;

  /* Libraries. */
  manta_script += header_libraries + manta_import;

  /* Variables. */
  manta_script += header_variables + fluid_variables + smoke_variables;
  if (noise) {
    manta_script += fluid_variables_noise + smoke_variables_noise;
  }
  if (guiding)
    manta_script += fluid_variables_guiding;

  /* Solvers. */
  manta_script += header_solvers + fluid_solver;
  if (noise)
    manta_script += fluid_solver_noise;
  if (guiding)
    manta_script += fluid_solver_guiding;

  /* Grids. */
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

  /* Noise field. */
  if (noise)
    manta_script += smoke_wavelet_noise;

  /* Time. */
  manta_script += header_time + fluid_time_stepping + fluid_adapt_time_step;

  /* Import. */
  manta_script += header_import + fluid_file_import + fluid_cache_helper + smoke_load_data;
  if (noise)
    manta_script += smoke_load_noise;
  if (guiding)
    manta_script += fluid_load_guiding;

  /* Pre/Post Steps. */
  manta_script += header_prepost + fluid_pre_step + fluid_post_step;

  /* Steps. */
  manta_script += header_steps + smoke_adaptive_step + smoke_step;
  if (noise) {
    manta_script += smoke_step_noise;
  }

  /* Main. */
  manta_script += header_main + smoke_standalone + fluid_standalone;

  /* Fill in missing variables in script. */
  string final_script = MANTA::parseScript(manta_script, fmd);

  /* Write script. */
  ofstream myfile;
  myfile.open(cacheDirScript);
  myfile << final_script;
  myfile.close();
  if (!myfile) {
    cerr << "Fluid Error -- Could not export standalone Mantaflow smoke domain script";
    return false;
  }
  return true;
}

bool MANTA::exportLiquidScript(FluidModifierData *fmd)
{
  if (with_debug)
    cout << "MANTA::exportLiquidScript()" << endl;

  char cacheDir[FILE_MAX] = "\0";
  char cacheDirScript[FILE_MAX] = "\0";

  FluidDomainSettings *fds = fmd->domain;

  BLI_path_join(cacheDir, sizeof(cacheDir), fds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT);
  BLI_path_make_safe(cacheDir);
  /* Create 'script' subdir if it does not exist already */
  BLI_dir_create_recursive(cacheDir);
  BLI_path_join(cacheDirScript, sizeof(cacheDirScript), cacheDir, FLUID_DOMAIN_LIQUID_SCRIPT);
  BLI_path_make_safe(cacheDirScript);

  bool mesh = fds->flags & FLUID_DOMAIN_USE_MESH;
  bool drops = fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY;
  bool bubble = fds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE;
  bool floater = fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM;
  bool tracer = fds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER;
  bool obstacle = fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE;
  bool fractions = fds->flags & FLUID_DOMAIN_USE_FRACTIONS;
  bool guiding = fds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE;
  bool invel = fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL;
  bool outflow = fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW;
  bool viscosity = fds->flags & FLUID_DOMAIN_USE_VISCOSITY;

  string manta_script;

  /* Libraries. */
  manta_script += header_libraries + manta_import;

  /* Variables. */
  manta_script += header_variables + fluid_variables + liquid_variables;
  if (mesh)
    manta_script += fluid_variables_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += fluid_variables_particles + liquid_variables_particles;
  if (guiding)
    manta_script += fluid_variables_guiding;
  if (viscosity)
    manta_script += fluid_variables_viscosity;

  /* Solvers. */
  manta_script += header_solvers + fluid_solver;
  if (mesh)
    manta_script += fluid_solver_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += fluid_solver_particles;
  if (guiding)
    manta_script += fluid_solver_guiding;
  if (viscosity)
    manta_script += fluid_solver_viscosity;

  /* Grids. */
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
  if (viscosity)
    manta_script += liquid_alloc_viscosity;

  /* Domain init. */
  manta_script += header_gridinit + liquid_init_phi;

  /* Time. */
  manta_script += header_time + fluid_time_stepping + fluid_adapt_time_step;

  /* Import. */
  manta_script += header_import + fluid_file_import + fluid_cache_helper + liquid_load_data;
  if (mesh)
    manta_script += liquid_load_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += liquid_load_particles;
  if (guiding)
    manta_script += fluid_load_guiding;

  /* Pre/Post Steps. */
  manta_script += header_prepost + fluid_pre_step + fluid_post_step;

  /* Steps. */
  manta_script += header_steps + liquid_adaptive_step + liquid_step;
  if (mesh)
    manta_script += liquid_step_mesh;
  if (drops || bubble || floater || tracer)
    manta_script += liquid_step_particles;

  /* Main. */
  manta_script += header_main + liquid_standalone + fluid_standalone;

  /* Fill in missing variables in script. */
  string final_script = MANTA::parseScript(manta_script, fmd);

  /* Write script. */
  ofstream myfile;
  myfile.open(cacheDirScript);
  myfile << final_script;
  myfile.close();
  if (!myfile) {
    cerr << "Fluid Error -- Could not export standalone Mantaflow liquid domain script";
    return false;
  }
  return true;
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

  /* Get pyobject that holds result value. */
  if (!manta_main_module) {
    PyGILState_Release(gilstate);
    return nullptr;
  }

  /* Ensure that requested variable is present in module - avoid attribute errors later on. */
  if (!PyObject_HasAttrString(manta_main_module, varName.c_str())) {
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

template<class T> static T *getPointer(string pyObjectName, string pyFunctionName)
{
  return static_cast<T *>(pyObjectToPointer(callPythonFunction(pyObjectName, pyFunctionName)));
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

bool MANTA::needsRealloc(FluidModifierData *fmd)
{
  FluidDomainSettings *fds = fmd->domain;
  return ((fds->res_max[0] - fds->res_min[0]) != mResX ||
          (fds->res_max[1] - fds->res_min[1]) != mResY ||
          (fds->res_max[2] - fds->res_min[2]) != mResZ);
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

void MANTA::updatePointers(FluidModifierData *fmd, bool flush)
{
  if (with_debug)
    cout << "MANTA::updatePointers()" << endl;

  FluidDomainSettings *fds = fmd->domain;

  bool liquid = !flush && (fds->type == FLUID_DOMAIN_TYPE_LIQUID);
  bool smoke = !flush && (fds->type == FLUID_DOMAIN_TYPE_GAS);
  bool noise = !flush && smoke && fds->flags & FLUID_DOMAIN_USE_NOISE;
  bool heat = !flush && smoke && fds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT;
  bool colors = !flush && smoke && fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS;
  bool fire = !flush && smoke && fds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE;
  bool obstacle = !flush && fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE;
  bool guiding = !flush && fds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE;
  bool invel = !flush && fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL;
  bool outflow = !flush && fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW;
  bool drops = !flush && liquid && fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY;
  bool bubble = !flush && liquid && fds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE;
  bool floater = !flush && liquid && fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM;
  bool tracer = !flush && liquid && fds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER;
  bool parts = !flush && liquid && (drops | bubble | floater | tracer);
  bool mesh = !flush && liquid && fds->flags & FLUID_DOMAIN_USE_MESH;
  bool meshvel = !flush && liquid && mesh && fds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS;

  string func = "getDataPointer";
  string funcNodes = "getNodesDataPointer";
  string funcTris = "getTrisDataPointer";

  string id = to_string(mCurrentID);
  string s_ext = "_s" + id;
  string pp_ext = "_pp" + id;
  string snd_ext = "_sp" + id;
  string sm_ext = "_sm" + id;
  string mesh_ext = "_mesh" + id;
  string sn_ext = "_sn" + id;

  mFlags = (smoke || liquid) ? getPointer<int>("flags" + s_ext, func) : nullptr;
  mPhiIn = (smoke || liquid) ? getPointer<float>("phiIn" + s_ext, func) : nullptr;
  mPhiStaticIn = (smoke || liquid) ? getPointer<float>("phiSIn" + s_ext, func) : nullptr;
  mVelocityX = (smoke || liquid) ? getPointer<float>("x_vel" + s_ext, func) : nullptr;
  mVelocityY = (smoke || liquid) ? getPointer<float>("y_vel" + s_ext, func) : nullptr;
  mVelocityZ = (smoke || liquid) ? getPointer<float>("z_vel" + s_ext, func) : nullptr;
  mForceX = (smoke || liquid) ? getPointer<float>("x_force" + s_ext, func) : nullptr;
  mForceY = (smoke || liquid) ? getPointer<float>("y_force" + s_ext, func) : nullptr;
  mForceZ = (smoke || liquid) ? getPointer<float>("z_force" + s_ext, func) : nullptr;
  mPressure = (smoke || liquid) ? getPointer<float>("pressure" + s_ext, func) : nullptr;

  /* Outflow. */
  mPhiOutIn = (outflow) ? getPointer<float>("phiOutIn" + s_ext, func) : nullptr;
  mPhiOutStaticIn = (outflow) ? getPointer<float>("phiOutSIn" + s_ext, func) : nullptr;

  /* Obstacles. */
  mPhiObsIn = (obstacle) ? getPointer<float>("phiObsIn" + s_ext, func) : nullptr;
  mPhiObsStaticIn = (obstacle) ? getPointer<float>("phiObsSIn" + s_ext, func) : nullptr;
  mObVelocityX = (obstacle) ? getPointer<float>("x_obvel" + s_ext, func) : nullptr;
  mObVelocityY = (obstacle) ? getPointer<float>("y_obvel" + s_ext, func) : nullptr;
  mObVelocityZ = (obstacle) ? getPointer<float>("z_obvel" + s_ext, func) : nullptr;
  mNumObstacle = (obstacle) ? getPointer<float>("numObs" + s_ext, func) : nullptr;

  /* Guiding. */
  mPhiGuideIn = (guiding) ? getPointer<float>("phiGuideIn" + s_ext, func) : nullptr;
  mGuideVelocityX = (guiding) ? getPointer<float>("x_guidevel" + s_ext, func) : nullptr;
  mGuideVelocityY = (guiding) ? getPointer<float>("y_guidevel" + s_ext, func) : nullptr;
  mGuideVelocityZ = (guiding) ? getPointer<float>("z_guidevel" + s_ext, func) : nullptr;
  mNumGuide = (guiding) ? getPointer<float>("numGuides" + s_ext, func) : nullptr;

  /* Initial velocities. */
  mInVelocityX = (invel) ? getPointer<float>("x_invel" + s_ext, func) : nullptr;
  mInVelocityY = (invel) ? getPointer<float>("y_invel" + s_ext, func) : nullptr;
  mInVelocityZ = (invel) ? getPointer<float>("z_invel" + s_ext, func) : nullptr;

  /* Smoke. */
  mDensity = (smoke) ? getPointer<float>("density" + s_ext, func) : nullptr;
  mDensityIn = (smoke) ? getPointer<float>("densityIn" + s_ext, func) : nullptr;
  mShadow = (smoke) ? getPointer<float>("shadow" + s_ext, func) : nullptr;
  mEmissionIn = (smoke) ? getPointer<float>("emissionIn" + s_ext, func) : nullptr;

  /* Heat. */
  mHeat = (heat) ? getPointer<float>("heat" + s_ext, func) : nullptr;
  mHeatIn = (heat) ? getPointer<float>("heatIn" + s_ext, func) : nullptr;

  /* Fire. */
  mFlame = (fire) ? getPointer<float>("flame" + s_ext, func) : nullptr;
  mFuel = (fire) ? getPointer<float>("fuel" + s_ext, func) : nullptr;
  mReact = (fire) ? getPointer<float>("react" + s_ext, func) : nullptr;
  mFuelIn = (fire) ? getPointer<float>("fuelIn" + s_ext, func) : nullptr;
  mReactIn = (fire) ? getPointer<float>("reactIn" + s_ext, func) : nullptr;

  /* Colors. */
  mColorR = (colors) ? getPointer<float>("color_r" + s_ext, func) : nullptr;
  mColorG = (colors) ? getPointer<float>("color_g" + s_ext, func) : nullptr;
  mColorB = (colors) ? getPointer<float>("color_b" + s_ext, func) : nullptr;
  mColorRIn = (colors) ? getPointer<float>("color_r_in" + s_ext, func) : nullptr;
  mColorGIn = (colors) ? getPointer<float>("color_g_in" + s_ext, func) : nullptr;
  mColorBIn = (colors) ? getPointer<float>("color_b_in" + s_ext, func) : nullptr;

  /* Noise. */
  mDensityHigh = (noise) ? getPointer<float>("density" + sn_ext, func) : nullptr;
  mTextureU = (noise) ? getPointer<float>("texture_u" + s_ext, func) : nullptr;
  mTextureV = (noise) ? getPointer<float>("texture_v" + s_ext, func) : nullptr;
  mTextureW = (noise) ? getPointer<float>("texture_w" + s_ext, func) : nullptr;
  mTextureU2 = (noise) ? getPointer<float>("texture_u2" + s_ext, func) : nullptr;
  mTextureV2 = (noise) ? getPointer<float>("texture_v2" + s_ext, func) : nullptr;
  mTextureW2 = (noise) ? getPointer<float>("texture_w2" + s_ext, func) : nullptr;

  /* Fire with noise. */
  mFlameHigh = (noise && fire) ? getPointer<float>("flame" + sn_ext, func) : nullptr;
  mFuelHigh = (noise && fire) ? getPointer<float>("fuel" + sn_ext, func) : nullptr;
  mReactHigh = (noise && fire) ? getPointer<float>("react" + sn_ext, func) : nullptr;

  /* Colors with noise. */
  mColorRHigh = (noise && colors) ? getPointer<float>("color_r" + sn_ext, func) : nullptr;
  mColorGHigh = (noise && colors) ? getPointer<float>("color_g" + sn_ext, func) : nullptr;
  mColorBHigh = (noise && colors) ? getPointer<float>("color_b" + sn_ext, func) : nullptr;

  /* Liquid. */
  mPhi = (liquid) ? getPointer<float>("phi" + s_ext, func) : nullptr;
  mFlipParticleData = (liquid) ? getPointer<vector<pData>>("pp" + s_ext, func) : nullptr;
  mFlipParticleVelocity = (liquid) ? getPointer<vector<pVel>>("pVel" + pp_ext, func) : nullptr;

  /* Mesh. */
  mMeshNodes = (mesh) ? getPointer<vector<Node>>("mesh" + sm_ext, funcNodes) : nullptr;
  mMeshTriangles = (mesh) ? getPointer<vector<Triangle>>("mesh" + sm_ext, funcTris) : nullptr;

  /* Mesh velocities. */
  mMeshVelocities = (meshvel) ? getPointer<vector<pVel>>("mVel" + mesh_ext, func) : nullptr;

  /* Secondary particles. */
  mParticleData = (parts) ? getPointer<vector<pData>>("ppSnd" + snd_ext, func) : nullptr;
  mParticleVelocity = (parts) ? getPointer<vector<pVel>>("pVelSnd" + pp_ext, func) : nullptr;
  mParticleLife = (parts) ? getPointer<vector<float>>("pLifeSnd" + pp_ext, func) : nullptr;

  mFlipFromFile = false;
  mMeshFromFile = false;
  mParticlesFromFile = false;
  mSmokeFromFile = false;
  mNoiseFromFile = false;
}

bool MANTA::hasConfig(FluidModifierData *fmd, int framenr)
{
  string extension = FLUID_DOMAIN_EXTENSION_UNI;
  return BLI_exists(
      getFile(fmd, FLUID_DOMAIN_DIR_CONFIG, FLUID_NAME_CONFIG, extension, framenr).c_str());
}

bool MANTA::hasData(FluidModifierData *fmd, int framenr)
{
  string extension = getCacheFileEnding(fmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(fmd, FLUID_DOMAIN_DIR_DATA, FLUID_NAME_DATA, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    string filename = (mUsingSmoke) ? FLUID_NAME_DENSITY : FLUID_NAME_PP;
    exists = BLI_exists(getFile(fmd, FLUID_DOMAIN_DIR_DATA, filename, extension, framenr).c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Data: " << exists << endl;

  return exists;
}

bool MANTA::hasNoise(FluidModifierData *fmd, int framenr)
{
  string extension = getCacheFileEnding(fmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(fmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_NOISE, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    extension = getCacheFileEnding(fmd->domain->cache_data_format);
    exists = BLI_exists(
        getFile(fmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_DENSITY_NOISE, extension, framenr)
            .c_str());
  }
  /* Check single file naming with deprecated extension. */
  if (!exists) {
    extension = getCacheFileEnding(fmd->domain->cache_noise_format);
    exists = BLI_exists(
        getFile(fmd, FLUID_DOMAIN_DIR_NOISE, FLUID_NAME_DENSITY_NOISE, extension, framenr)
            .c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Noise: " << exists << endl;

  return exists;
}

bool MANTA::hasMesh(FluidModifierData *fmd, int framenr)
{
  string extension = getCacheFileEnding(fmd->domain->cache_mesh_format);
  bool exists = BLI_exists(
      getFile(fmd, FLUID_DOMAIN_DIR_MESH, FLUID_NAME_MESH, extension, framenr).c_str());

  /* Check old file naming. */
  if (!exists) {
    exists = BLI_exists(
        getFile(fmd, FLUID_DOMAIN_DIR_MESH, FLUID_NAME_LMESH, extension, framenr).c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Mesh: " << exists << endl;

  return exists;
}

bool MANTA::hasParticles(FluidModifierData *fmd, int framenr)
{
  string extension = getCacheFileEnding(fmd->domain->cache_data_format);
  bool exists = BLI_exists(
      getFile(fmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PARTICLES, extension, framenr).c_str());

  /* Check single file naming. */
  if (!exists) {
    extension = getCacheFileEnding(fmd->domain->cache_data_format);
    exists = BLI_exists(
        getFile(fmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PP_PARTICLES, extension, framenr)
            .c_str());
  }
  /* Check single file naming with deprecated extension. */
  if (!exists) {
    extension = getCacheFileEnding(fmd->domain->cache_particle_format);
    exists = BLI_exists(
        getFile(fmd, FLUID_DOMAIN_DIR_PARTICLES, FLUID_NAME_PP_PARTICLES, extension, framenr)
            .c_str());
  }
  if (with_debug)
    cout << "Fluid: Has Particles: " << exists << endl;

  return exists;
}

bool MANTA::hasGuiding(FluidModifierData *fmd, int framenr, bool sourceDomain)
{
  string subdirectory = (sourceDomain) ? FLUID_DOMAIN_DIR_DATA : FLUID_DOMAIN_DIR_GUIDE;
  string filename = (sourceDomain) ? FLUID_NAME_DATA : FLUID_NAME_GUIDING;
  string extension = getCacheFileEnding(fmd->domain->cache_data_format);
  bool exists = BLI_exists(getFile(fmd, subdirectory, filename, extension, framenr).c_str());

  /* Check old file naming. */
  if (!exists) {
    filename = (sourceDomain) ? FLUID_NAME_VEL : FLUID_NAME_GUIDEVEL;
    exists = BLI_exists(getFile(fmd, subdirectory, filename, extension, framenr).c_str());
  }

  if (with_debug)
    cout << "Fluid: Has Guiding: " << exists << endl;

  return exists;
}

string MANTA::getDirectory(FluidModifierData *fmd, string subdirectory)
{
  char directory[FILE_MAX];
  BLI_path_join(directory, sizeof(directory), fmd->domain->cache_directory, subdirectory.c_str());
  BLI_path_make_safe(directory);
  return directory;
}

string MANTA::getFile(
    FluidModifierData *fmd, string subdirectory, string fname, string extension, int framenr)
{
  char targetFile[FILE_MAX];
  string path = getDirectory(fmd, subdirectory);
  string filename = fname + "_####" + extension;
  BLI_path_join(targetFile, sizeof(targetFile), path.c_str(), filename.c_str());
  BLI_path_frame(targetFile, sizeof(targetFile), framenr, 0);
  return targetFile;
}
