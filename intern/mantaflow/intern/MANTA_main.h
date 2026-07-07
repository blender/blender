/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mantaflow
 */

#pragma once

#include <atomic>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

using std::atomic;
using std::string;
using std::unordered_map;
using std::vector;

namespace blender {

struct MANTA {
 public:
  MANTA(int *res, struct FluidModifierData *fmd);
  virtual ~MANTA();

  /* Mirroring Mantaflow structures for particle data (pVel also used for mesh vert vels). */
  struct pData {
    float pos[3];
    int flag;
  };
  struct pVel {
    float pos[3];
  };

  /* Mirroring Mantaflow structures for meshes. */
  struct Node {
    int flags;
    float pos[3], normal[3];
  };
  struct Triangle {
    int c[3];
    int flags;
  };

  /* Grid initialization functions. */
  bool initHeat(struct FluidModifierData *fmd = nullptr);
  bool initFire(struct FluidModifierData *fmd = nullptr);
  bool initColors(struct FluidModifierData *fmd = nullptr);
  bool initFireHigh(struct FluidModifierData *fmd = nullptr);
  bool initColorsHigh(struct FluidModifierData *fmd = nullptr);
  bool initLiquid(FluidModifierData *fmd = nullptr);
  bool initLiquidMesh(FluidModifierData *fmd = nullptr);
  bool initLiquidViscosity(FluidModifierData *fmd = nullptr);
  bool initObstacle(FluidModifierData *fmd = nullptr);
  bool initCurvature(FluidModifierData *fmd = nullptr);
  bool initGuiding(FluidModifierData *fmd = nullptr);
  bool initFractions(FluidModifierData *fmd = nullptr);
  bool initInVelocity(FluidModifierData *fmd = nullptr);
  bool initOutflow(FluidModifierData *fmd = nullptr);
  bool initSndParts(FluidModifierData *fmd = nullptr);
  bool initLiquidSndParts(FluidModifierData *fmd = nullptr);

  /* Pointer transfer: Mantaflow -> Blender. Use flush to reset all pointers to nullptr. */
  void updatePointers(FluidModifierData *fmd, bool flush = false);

  /* Write cache. */
  bool writeConfiguration(FluidModifierData *fmd, int framenr);
  bool writeData(FluidModifierData *fmd, int framenr);
  bool writeNoise(FluidModifierData *fmd, int framenr);
  /* Write calls for mesh and particles were left in bake calls for now. */

  /* Read cache (via Python). */
  bool readConfiguration(FluidModifierData *fmd, int framenr);
  bool readData(FluidModifierData *fmd, int framenr, bool resumable);
  bool readNoise(FluidModifierData *fmd, int framenr, bool resumable);
  bool readMesh(FluidModifierData *fmd, int framenr);
  bool readParticles(FluidModifierData *fmd, int framenr, bool resumable);
  bool readGuiding(FluidModifierData *fmd, int framenr, bool sourceDomain);

  /* Propagate variable changes from RNA to Python. */
  bool updateVariables(FluidModifierData *fmd);

  /* Bake cache. */
  bool bakeData(FluidModifierData *fmd, int framenr);
  bool bakeNoise(FluidModifierData *fmd, int framenr);
  bool bakeMesh(FluidModifierData *fmd, int framenr);
  bool bakeParticles(FluidModifierData *fmd, int framenr);
  bool bakeGuiding(FluidModifierData *fmd, int framenr);

  /* IO for Mantaflow scene script. */
  bool exportSmokeScript(struct FluidModifierData *fmd);
  bool exportLiquidScript(struct FluidModifierData *fmd);

  /* Check cache status by frame. */
  bool hasConfig(FluidModifierData *fmd, int framenr);
  bool hasData(FluidModifierData *fmd, int framenr);
  bool hasNoise(FluidModifierData *fmd, int framenr);
  bool hasMesh(FluidModifierData *fmd, int framenr);
  bool hasParticles(FluidModifierData *fmd, int framenr);
  bool hasGuiding(FluidModifierData *fmd, int framenr, bool sourceDomain);

  size_t getTotalCells()
  {
    return mTotalCells;
  }
  size_t getTotalCellsHigh()
  {
    return mTotalCellsHigh;
  }
  bool usingNoise()
  {
    return mUsingNoise;
  }
  int getResX()
  {
    return mResX;
  }
  int getResY()
  {
    return mResY;
  }
  int getResZ()
  {
    return mResZ;
  }
  int getParticleResX()
  {
    return mResXParticle;
  }
  int getParticleResY()
  {
    return mResYParticle;
  }
  int getParticleResZ()
  {
    return mResZParticle;
  }
  int getMeshResX()
  {
    return mResXMesh;
  }
  int getMeshResY()
  {
    return mResYMesh;
  }
  int getMeshResZ()
  {
    return mResZMesh;
  }
  int getResXHigh()
  {
    return mResXNoise;
  }
  int getResYHigh()
  {
    return mResYNoise;
  }
  int getResZHigh()
  {
    return mResZNoise;
  }
  int getMeshUpres()
  {
    return mUpresMesh;
  }
  int getParticleUpres()
  {
    return mUpresParticle;
  }

  /* Smoke getters. */
  float *getDensity()
  {
    return mDensity;
  }
  float *getHeat()
  {
    return mHeat;
  }
  float *getVelocityX()
  {
    return mVelocityX;
  }
  float *getVelocityY()
  {
    return mVelocityY;
  }
  float *getVelocityZ()
  {
    return mVelocityZ;
  }
  float *getObVelocityX()
  {
    return mObVelocityX;
  }
  float *getObVelocityY()
  {
    return mObVelocityY;
  }
  float *getObVelocityZ()
  {
    return mObVelocityZ;
  }
  float *getGuideVelocityX()
  {
    return mGuideVelocityX;
  }
  float *getGuideVelocityY()
  {
    return mGuideVelocityY;
  }
  float *getGuideVelocityZ()
  {
    return mGuideVelocityZ;
  }
  float *getInVelocityX()
  {
    return mInVelocityX;
  }
  float *getInVelocityY()
  {
    return mInVelocityY;
  }
  float *getInVelocityZ()
  {
    return mInVelocityZ;
  }
  float *getForceX()
  {
    return mForceX;
  }
  float *getForceY()
  {
    return mForceY;
  }
  float *getForceZ()
  {
    return mForceZ;
  }
  int *getFlags()
  {
    return mFlags;
  }
  float *getNumObstacle()
  {
    return mNumObstacle;
  }
  float *getNumGuide()
  {
    return mNumGuide;
  }
  float *getFlame()
  {
    return mFlame;
  }
  float *getFuel()
  {
    return mFuel;
  }
  float *getReact()
  {
    return mReact;
  }
  float *getColorR()
  {
    return mColorR;
  }
  float *getColorG()
  {
    return mColorG;
  }
  float *getColorB()
  {
    return mColorB;
  }
  float *getShadow()
  {
    return mShadow;
  }
  float *getDensityIn()
  {
    return mDensityIn;
  }
  float *getHeatIn()
  {
    return mHeatIn;
  }
  float *getColorRIn()
  {
    return mColorRIn;
  }
  float *getColorGIn()
  {
    return mColorGIn;
  }
  float *getColorBIn()
  {
    return mColorBIn;
  }
  float *getFuelIn()
  {
    return mFuelIn;
  }
  float *getReactIn()
  {
    return mReactIn;
  }
  float *getEmissionIn()
  {
    return mEmissionIn;
  }

  float *getDensityHigh()
  {
    return mDensityHigh;
  }
  float *getFlameHigh()
  {
    return mFlameHigh;
  }
  float *getFuelHigh()
  {
    return mFuelHigh;
  }
  float *getReactHigh()
  {
    return mReactHigh;
  }
  float *getColorRHigh()
  {
    return mColorRHigh;
  }
  float *getColorGHigh()
  {
    return mColorGHigh;
  }
  float *getColorBHigh()
  {
    return mColorBHigh;
  }
  float *getTextureU()
  {
    return mTextureU;
  }
  float *getTextureV()
  {
    return mTextureV;
  }
  float *getTextureW()
  {
    return mTextureW;
  }
  float *getTextureU2()
  {
    return mTextureU2;
  }
  float *getTextureV2()
  {
    return mTextureV2;
  }
  float *getTextureW2()
  {
    return mTextureW2;
  }

  float *getPhiIn()
  {
    return mPhiIn;
  }
  float *getPhiStaticIn()
  {
    return mPhiStaticIn;
  }
  float *getPhiObsIn()
  {
    return mPhiObsIn;
  }
  float *getPhiObsStaticIn()
  {
    return mPhiObsStaticIn;
  }
  float *getPhiGuideIn()
  {
    return mPhiGuideIn;
  }
  float *getPhiOutIn()
  {
    return mPhiOutIn;
  }
  float *getPhiOutStaticIn()
  {
    return mPhiOutStaticIn;
  }
  float *getPhi()
  {
    return mPhi;
  }
  float *getPressure()
  {
    return mPressure;
  }

  static atomic<int> solverID;
  static int with_debug; /* On or off (1 or 0), also sets manta debug level. */

  /* Mesh getters. */
  int getNumVertices()
  {
    return (mMeshNodes && !mMeshNodes->empty()) ? mMeshNodes->size() : 0;
  }
  int getNumNormals()
  {
    return (mMeshNodes && !mMeshNodes->empty()) ? mMeshNodes->size() : 0;
  }
  int getNumTriangles()
  {
    return (mMeshTriangles && !mMeshTriangles->empty()) ? mMeshTriangles->size() : 0;
  }

  float getVertexXAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[0];
    }
    return 0.0f;
  }
  float getVertexYAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[1];
    }
    return 0.0f;
  }
  float getVertexZAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[2];
    }
    return 0.0f;
  }

  float getNormalXAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[0];
    }
    return 0.0f;
  }
  float getNormalYAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[1];
    }
    return 0.0f;
  }
  float getNormalZAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[2];
    }
    return 0.0f;
  }

  int getTriangleXAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[0];
    }
    return 0;
  }
  int getTriangleYAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[1];
    }
    return 0;
  }
  int getTriangleZAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[2];
    }
    return 0;
  }

  float getVertVelXAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[0];
    }
    return 0.0f;
  }
  float getVertVelYAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[1];
    }
    return 0.0f;
  }
  float getVertVelZAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[2];
    }
    return 0.0f;
  }

  // Particle getters
  int getFlipParticleFlagAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].flag;
    }
    return 0;
  }
  int getSndParticleFlagAt(int i)
  {
    assert(i >= 0);
    if (mParticleData && !mParticleData->empty()) {
      assert(i < mParticleData->size());
      return (*mParticleData)[i].flag;
    }
    return 0;
  }

  float getFlipParticlePositionXAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[0];
    }
    return 0.0f;
  }
  float getFlipParticlePositionYAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[1];
    }
    return 0.0f;
  }
  float getFlipParticlePositionZAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[2];
    }
    return 0.0f;
  }

  float getSndParticlePositionXAt(int i)
  {
    assert(i >= 0);
    if (mParticleData && !mParticleData->empty()) {
      assert(i < mParticleData->size());
      return (*mParticleData)[i].pos[0];
    }
    return 0.0f;
  }
  float getSndParticlePositionYAt(int i)
  {
    assert(i >= 0);
    if (mParticleData && !mParticleData->empty()) {
      assert(i < mParticleData->size());
      return (*mParticleData)[i].pos[1];
    }
    return 0.0f;
  }
  float getSndParticlePositionZAt(int i)
  {
    assert(i >= 0);
    if (mParticleData && !mParticleData->empty()) {
      assert(i < mParticleData->size());
      return (*mParticleData)[i].pos[2];
    }
    return 0.0f;
  }

  float getFlipParticleVelocityXAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[0];
    }
    return 0.0f;
  }
  float getFlipParticleVelocityYAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[1];
    }
    return 0.0f;
  }
  float getFlipParticleVelocityZAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[2];
    }
    return 0.0f;
  }

  float getSndParticleVelocityXAt(int i)
  {
    assert(i >= 0);
    if (mParticleVelocity && !mParticleVelocity->empty()) {
      assert(i < mParticleVelocity->size());
      return (*mParticleVelocity)[i].pos[0];
    }
    return 0.0f;
  }
  float getSndParticleVelocityYAt(int i)
  {
    assert(i >= 0);
    if (mParticleVelocity && !mParticleVelocity->empty()) {
      assert(i < mParticleVelocity->size());
      return (*mParticleVelocity)[i].pos[1];
    }
    return 0.0f;
  }
  float getSndParticleVelocityZAt(int i)
  {
    assert(i >= 0);
    if (mParticleVelocity && !mParticleVelocity->empty()) {
      assert(i < mParticleVelocity->size());
      return (*mParticleVelocity)[i].pos[2];
    }
    return 0.0f;
  }

  float *getFlipParticleData()
  {
    return (mFlipParticleData && !mFlipParticleData->empty()) ?
               (float *)&mFlipParticleData->front() :
               nullptr;
  }
  float *getSndParticleData()
  {
    return (mParticleData && !mParticleData->empty()) ? (float *)&mParticleData->front() : nullptr;
  }

  float *getFlipParticleVelocity()
  {
    return (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) ?
               (float *)&mFlipParticleVelocity->front() :
               nullptr;
  }
  float *getSndParticleVelocity()
  {
    return (mParticleVelocity && !mParticleVelocity->empty()) ?
               (float *)&mParticleVelocity->front() :
               nullptr;
  }
  float *getSndParticleLife()
  {
    return (mParticleLife && !mParticleLife->empty()) ? (float *)&mParticleLife->front() : nullptr;
  }

  int getNumFlipParticles()
  {
    return (mFlipParticleData && !mFlipParticleData->empty()) ? mFlipParticleData->size() : 0;
  }
  int getNumSndParticles()
  {
    return (mParticleData && !mParticleData->empty()) ? mParticleData->size() : 0;
  }

  bool usingFlipFromFile()
  {
    return mFlipFromFile;
  }
  bool usingMeshFromFile()
  {
    return mMeshFromFile;
  }
  bool usingParticleFromFile()
  {
    return mParticlesFromFile;
  }

  /* Direct access to solver time attributes. */
  int getFrame();
  float getTimestep();
  void adaptTimestep();

  bool needsRealloc(FluidModifierData *fmd);

 private:
  /* Simulation constants. */
  size_t mTotalCells = -1;
  size_t mTotalCellsHigh = -1;
  size_t mTotalCellsMesh = -1;
  size_t mTotalCellsParticles = -1;

  unordered_map<string, string> mRNAMap;

  /* The ID of the solver objects will be incremented for every new object. */
  const int mCurrentID;

  bool mUsingHeat = false;
  bool mUsingColors = false;
  bool mUsingFire = false;
  bool mUsingObstacle = false;
  bool mUsingGuiding = false;
  bool mUsingFractions = false;
  bool mUsingInvel = false;
  bool mUsingOutflow = false;
  bool mUsingNoise = false;
  bool mUsingMesh = false;
  bool mUsingDiffusion = false;
  bool mUsingViscosity = false;
  bool mUsingMVel = false;
  bool mUsingLiquid = false;
  bool mUsingSmoke = false;
  bool mUsingDrops = false;
  bool mUsingBubbles = false;
  bool mUsingFloats = false;
  bool mUsingTracers = false;

  bool mFlipFromFile = false;
  bool mMeshFromFile = false;
  bool mParticlesFromFile = false;
  bool mSmokeFromFile = false;
  bool mNoiseFromFile = false;

  int mResX = -1;
  int mResY = -1;
  int mResZ = -1;
  const int mMaxRes;

  int mResXNoise = -1;
  int mResYNoise = -1;
  int mResZNoise = -1;
  int mResXMesh = -1;
  int mResYMesh = -1;
  int mResZMesh = -1;
  int mResXParticle = -1;
  int mResYParticle = -1;
  int mResZParticle = -1;
  int *mResGuiding = nullptr;

  int mUpresMesh = -1;
  int mUpresParticle = -1;

  /* Fluid grids. */
  float *mVelocityX = nullptr;
  float *mVelocityY = nullptr;
  float *mVelocityZ = nullptr;
  float *mObVelocityX = nullptr;
  float *mObVelocityY = nullptr;
  float *mObVelocityZ = nullptr;
  float *mGuideVelocityX = nullptr;
  float *mGuideVelocityY = nullptr;
  float *mGuideVelocityZ = nullptr;
  float *mInVelocityX = nullptr;
  float *mInVelocityY = nullptr;
  float *mInVelocityZ = nullptr;
  float *mForceX = nullptr;
  float *mForceY = nullptr;
  float *mForceZ = nullptr;
  int *mFlags = nullptr;
  float *mNumObstacle = nullptr;
  float *mNumGuide = nullptr;
  float *mPressure = nullptr;

  /* Smoke grids. */
  float *mDensity = nullptr;
  float *mHeat = nullptr;
  float *mFlame = nullptr;
  float *mFuel = nullptr;
  float *mReact = nullptr;
  float *mColorR = nullptr;
  float *mColorG = nullptr;
  float *mColorB = nullptr;
  float *mShadow = nullptr;
  float *mDensityIn = nullptr;
  float *mHeatIn = nullptr;
  float *mFuelIn = nullptr;
  float *mReactIn = nullptr;
  float *mEmissionIn = nullptr;
  float *mColorRIn = nullptr;
  float *mColorGIn = nullptr;
  float *mColorBIn = nullptr;
  float *mDensityHigh = nullptr;
  float *mFlameHigh = nullptr;
  float *mFuelHigh = nullptr;
  float *mReactHigh = nullptr;
  float *mColorRHigh = nullptr;
  float *mColorGHigh = nullptr;
  float *mColorBHigh = nullptr;
  float *mTextureU = nullptr;
  float *mTextureV = nullptr;
  float *mTextureW = nullptr;
  float *mTextureU2 = nullptr;
  float *mTextureV2 = nullptr;
  float *mTextureW2 = nullptr;

  /* Liquid grids. */
  float *mPhiIn = nullptr;
  float *mPhiStaticIn = nullptr;
  float *mPhiObsIn = nullptr;
  float *mPhiObsStaticIn = nullptr;
  float *mPhiGuideIn = nullptr;
  float *mPhiOutIn = nullptr;
  float *mPhiOutStaticIn = nullptr;
  float *mPhi = nullptr;

  /* Mesh fields. */
  vector<Node> *mMeshNodes;
  vector<Triangle> *mMeshTriangles;
  vector<pVel> *mMeshVelocities;

  /* Particle fields. */
  vector<pData> *mFlipParticleData;
  vector<pVel> *mFlipParticleVelocity;

  vector<pData> *mParticleData;
  vector<pVel> *mParticleVelocity;
  vector<float> *mParticleLife;

  void initializeRNAMap(struct FluidModifierData *fmd = nullptr);
  bool initDomain(struct FluidModifierData *fmd = nullptr);
  bool initNoise(struct FluidModifierData *fmd = nullptr);
  bool initMesh(struct FluidModifierData *fmd = nullptr);
  bool initSmoke(struct FluidModifierData *fmd = nullptr);
  bool initSmokeNoise(struct FluidModifierData *fmd = nullptr);
  void initializeMantaflow();
  void terminateMantaflow();
  bool runPythonString(vector<string> commands);
  string getRealValue(const string &varName);
  string parseLine(const string &line);
  string parseScript(const string &setup_string, FluidModifierData *fmd = nullptr);
  string getDirectory(struct FluidModifierData *fmd, string subdirectory);
  string getFile(struct FluidModifierData *fmd,
                 string subdirectory,
                 string fname,
                 string extension,
                 int framenr);
};

}  // namespace blender
