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

#ifndef MANTA_A_H
#define MANTA_A_H

#include <atomic>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

using std::atomic;
using std::string;
using std::unordered_map;
using std::vector;

struct MANTA {
 public:
  MANTA(int *res, struct FluidModifierData *mmd);
  MANTA(){};
  virtual ~MANTA();

  // Mirroring Mantaflow structures for particle data (pVel also used for mesh vert vels)
  typedef struct PData {
    float pos[3];
    int flag;
  } pData;
  typedef struct PVel {
    float pos[3];
  } pVel;

  // Mirroring Mantaflow structures for meshes
  typedef struct Node {
    int flags;
    float pos[3], normal[3];
  } Node;
  typedef struct Triangle {
    int c[3];
    int flags;
  } Triangle;

  // Manta step, handling everything
  void step(struct FluidModifierData *mmd, int startFrame);

  // Grid initialization functions
  void initHeat(struct FluidModifierData *mmd = NULL);
  void initFire(struct FluidModifierData *mmd = NULL);
  void initColors(struct FluidModifierData *mmd = NULL);
  void initFireHigh(struct FluidModifierData *mmd = NULL);
  void initColorsHigh(struct FluidModifierData *mmd = NULL);
  void initLiquid(FluidModifierData *mmd = NULL);
  void initLiquidMesh(FluidModifierData *mmd = NULL);
  void initObstacle(FluidModifierData *mmd = NULL);
  void initCurvature(FluidModifierData *mmd = NULL);
  void initGuiding(FluidModifierData *mmd = NULL);
  void initFractions(FluidModifierData *mmd = NULL);
  void initInVelocity(FluidModifierData *mmd = NULL);
  void initOutflow(FluidModifierData *mmd = NULL);
  void initSndParts(FluidModifierData *mmd = NULL);
  void initLiquidSndParts(FluidModifierData *mmd = NULL);

  // Pointer transfer: Mantaflow -> Blender
  void updatePointers();

  // Write cache
  bool writeConfiguration(FluidModifierData *mmd, int framenr);
  bool writeData(FluidModifierData *mmd, int framenr);
  bool writeNoise(FluidModifierData *mmd, int framenr);
  // write calls for mesh and particles were left in bake calls for now

  // Read cache (via Manta save/load)
  bool readConfiguration(FluidModifierData *mmd, int framenr);
  bool readData(FluidModifierData *mmd, int framenr, bool resumable);
  bool readNoise(FluidModifierData *mmd, int framenr, bool resumable);
  bool readMesh(FluidModifierData *mmd, int framenr);
  bool readParticles(FluidModifierData *mmd, int framenr, bool resumable);
  bool readGuiding(FluidModifierData *mmd, int framenr, bool sourceDomain);

  // Read cache (via file read functions in MANTA - e.g. read .bobj.gz meshes, .uni particles)
  bool updateMeshStructures(FluidModifierData *mmd, int framenr);
  bool updateFlipStructures(FluidModifierData *mmd, int framenr);
  bool updateParticleStructures(FluidModifierData *mmd, int framenr);
  bool updateSmokeStructures(FluidModifierData *mmd, int framenr);
  bool updateNoiseStructures(FluidModifierData *mmd, int framenr);
  bool updateVariables(FluidModifierData *mmd);

  // Bake cache
  bool bakeData(FluidModifierData *mmd, int framenr);
  bool bakeNoise(FluidModifierData *mmd, int framenr);
  bool bakeMesh(FluidModifierData *mmd, int framenr);
  bool bakeParticles(FluidModifierData *mmd, int framenr);
  bool bakeGuiding(FluidModifierData *mmd, int framenr);

  // IO for Mantaflow scene script
  void exportSmokeScript(struct FluidModifierData *mmd);
  void exportLiquidScript(struct FluidModifierData *mmd);

  // Check cache status by frame
  bool hasConfig(FluidModifierData *mmd, int framenr);
  bool hasData(FluidModifierData *mmd, int framenr);
  bool hasNoise(FluidModifierData *mmd, int framenr);
  bool hasMesh(FluidModifierData *mmd, int framenr);
  bool hasParticles(FluidModifierData *mmd, int framenr);
  bool hasGuiding(FluidModifierData *mmd, int framenr, bool sourceDomain);

  inline size_t getTotalCells()
  {
    return mTotalCells;
  }
  inline size_t getTotalCellsHigh()
  {
    return mTotalCellsHigh;
  }
  inline bool usingNoise()
  {
    return mUsingNoise;
  }
  inline int getResX()
  {
    return mResX;
  }
  inline int getResY()
  {
    return mResY;
  }
  inline int getResZ()
  {
    return mResZ;
  }
  inline int getParticleResX()
  {
    return mResXParticle;
  }
  inline int getParticleResY()
  {
    return mResYParticle;
  }
  inline int getParticleResZ()
  {
    return mResZParticle;
  }
  inline int getMeshResX()
  {
    return mResXMesh;
  }
  inline int getMeshResY()
  {
    return mResYMesh;
  }
  inline int getMeshResZ()
  {
    return mResZMesh;
  }
  inline int getResXHigh()
  {
    return mResXNoise;
  }
  inline int getResYHigh()
  {
    return mResYNoise;
  }
  inline int getResZHigh()
  {
    return mResZNoise;
  }
  inline int getMeshUpres()
  {
    return mUpresMesh;
  }
  inline int getParticleUpres()
  {
    return mUpresParticle;
  }

  // Smoke getters
  inline float *getDensity()
  {
    return mDensity;
  }
  inline float *getHeat()
  {
    return mHeat;
  }
  inline float *getVelocityX()
  {
    return mVelocityX;
  }
  inline float *getVelocityY()
  {
    return mVelocityY;
  }
  inline float *getVelocityZ()
  {
    return mVelocityZ;
  }
  inline float *getObVelocityX()
  {
    return mObVelocityX;
  }
  inline float *getObVelocityY()
  {
    return mObVelocityY;
  }
  inline float *getObVelocityZ()
  {
    return mObVelocityZ;
  }
  inline float *getGuideVelocityX()
  {
    return mGuideVelocityX;
  }
  inline float *getGuideVelocityY()
  {
    return mGuideVelocityY;
  }
  inline float *getGuideVelocityZ()
  {
    return mGuideVelocityZ;
  }
  inline float *getInVelocityX()
  {
    return mInVelocityX;
  }
  inline float *getInVelocityY()
  {
    return mInVelocityY;
  }
  inline float *getInVelocityZ()
  {
    return mInVelocityZ;
  }
  inline float *getForceX()
  {
    return mForceX;
  }
  inline float *getForceY()
  {
    return mForceY;
  }
  inline float *getForceZ()
  {
    return mForceZ;
  }
  inline int *getFlags()
  {
    return mFlags;
  }
  inline float *getNumObstacle()
  {
    return mNumObstacle;
  }
  inline float *getNumGuide()
  {
    return mNumGuide;
  }
  inline float *getFlame()
  {
    return mFlame;
  }
  inline float *getFuel()
  {
    return mFuel;
  }
  inline float *getReact()
  {
    return mReact;
  }
  inline float *getColorR()
  {
    return mColorR;
  }
  inline float *getColorG()
  {
    return mColorG;
  }
  inline float *getColorB()
  {
    return mColorB;
  }
  inline float *getShadow()
  {
    return mShadow;
  }
  inline float *getDensityIn()
  {
    return mDensityIn;
  }
  inline float *getHeatIn()
  {
    return mHeatIn;
  }
  inline float *getColorRIn()
  {
    return mColorRIn;
  }
  inline float *getColorGIn()
  {
    return mColorGIn;
  }
  inline float *getColorBIn()
  {
    return mColorBIn;
  }
  inline float *getFuelIn()
  {
    return mFuelIn;
  }
  inline float *getReactIn()
  {
    return mReactIn;
  }
  inline float *getEmissionIn()
  {
    return mEmissionIn;
  }

  inline float *getDensityHigh()
  {
    return mDensityHigh;
  }
  inline float *getFlameHigh()
  {
    return mFlameHigh;
  }
  inline float *getFuelHigh()
  {
    return mFuelHigh;
  }
  inline float *getReactHigh()
  {
    return mReactHigh;
  }
  inline float *getColorRHigh()
  {
    return mColorRHigh;
  }
  inline float *getColorGHigh()
  {
    return mColorGHigh;
  }
  inline float *getColorBHigh()
  {
    return mColorBHigh;
  }
  inline float *getTextureU()
  {
    return mTextureU;
  }
  inline float *getTextureV()
  {
    return mTextureV;
  }
  inline float *getTextureW()
  {
    return mTextureW;
  }
  inline float *getTextureU2()
  {
    return mTextureU2;
  }
  inline float *getTextureV2()
  {
    return mTextureV2;
  }
  inline float *getTextureW2()
  {
    return mTextureW2;
  }

  inline float *getPhiIn()
  {
    return mPhiIn;
  }
  inline float *getPhiStaticIn()
  {
    return mPhiStaticIn;
  }
  inline float *getPhiObsIn()
  {
    return mPhiObsIn;
  }
  inline float *getPhiObsStaticIn()
  {
    return mPhiObsStaticIn;
  }
  inline float *getPhiGuideIn()
  {
    return mPhiGuideIn;
  }
  inline float *getPhiOutIn()
  {
    return mPhiOutIn;
  }
  inline float *getPhiOutStaticIn()
  {
    return mPhiOutStaticIn;
  }
  inline float *getPhi()
  {
    return mPhi;
  }

  static atomic<int> solverID;
  static int with_debug;  // on or off (1 or 0), also sets manta debug level

  // Mesh getters
  inline int getNumVertices()
  {
    return (mMeshNodes && !mMeshNodes->empty()) ? mMeshNodes->size() : 0;
  }
  inline int getNumNormals()
  {
    return (mMeshNodes && !mMeshNodes->empty()) ? mMeshNodes->size() : 0;
  }
  inline int getNumTriangles()
  {
    return (mMeshTriangles && !mMeshTriangles->empty()) ? mMeshTriangles->size() : 0;
  }

  inline float getVertexXAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getVertexYAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getVertexZAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].pos[2];
    }
    return 0.0f;
  }

  inline float getNormalXAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[0];
    }
    return 0.0f;
  }
  inline float getNormalYAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[1];
    }
    return 0.0f;
  }
  inline float getNormalZAt(int i)
  {
    assert(i >= 0);
    if (mMeshNodes && !mMeshNodes->empty()) {
      assert(i < mMeshNodes->size());
      return (*mMeshNodes)[i].normal[2];
    }
    return 0.0f;
  }

  inline int getTriangleXAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[0];
    }
    return 0;
  }
  inline int getTriangleYAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[1];
    }
    return 0;
  }
  inline int getTriangleZAt(int i)
  {
    assert(i >= 0);
    if (mMeshTriangles && !mMeshTriangles->empty()) {
      assert(i < mMeshTriangles->size());
      return (*mMeshTriangles)[i].c[2];
    }
    return 0;
  }

  inline float getVertVelXAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getVertVelYAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getVertVelZAt(int i)
  {
    assert(i >= 0);
    if (mMeshVelocities && !mMeshVelocities->empty()) {
      assert(i < mMeshVelocities->size());
      return (*mMeshVelocities)[i].pos[2];
    }
    return 0.0f;
  }

  // Particle getters
  inline int getFlipParticleFlagAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].flag;
    }
    return 0;
  }
  inline int getSndParticleFlagAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleData && !mSndParticleData->empty()) {
      assert(i < mSndParticleData->size());
      return (*mSndParticleData)[i].flag;
    }
    return 0;
  }

  inline float getFlipParticlePositionXAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getFlipParticlePositionYAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getFlipParticlePositionZAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleData && !mFlipParticleData->empty()) {
      assert(i < mFlipParticleData->size());
      return (*mFlipParticleData)[i].pos[2];
    }
    return 0.0f;
  }

  inline float getSndParticlePositionXAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleData && !mSndParticleData->empty()) {
      assert(i < mSndParticleData->size());
      return (*mSndParticleData)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getSndParticlePositionYAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleData && !mSndParticleData->empty()) {
      assert(i < mSndParticleData->size());
      return (*mSndParticleData)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getSndParticlePositionZAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleData && !mSndParticleData->empty()) {
      assert(i < mSndParticleData->size());
      return (*mSndParticleData)[i].pos[2];
    }
    return 0.0f;
  }

  inline float getFlipParticleVelocityXAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getFlipParticleVelocityYAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getFlipParticleVelocityZAt(int i)
  {
    assert(i >= 0);
    if (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) {
      assert(i < mFlipParticleVelocity->size());
      return (*mFlipParticleVelocity)[i].pos[2];
    }
    return 0.0f;
  }

  inline float getSndParticleVelocityXAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleVelocity && !mSndParticleVelocity->empty()) {
      assert(i < mSndParticleVelocity->size());
      return (*mSndParticleVelocity)[i].pos[0];
    }
    return 0.0f;
  }
  inline float getSndParticleVelocityYAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleVelocity && !mSndParticleVelocity->empty()) {
      assert(i < mSndParticleVelocity->size());
      return (*mSndParticleVelocity)[i].pos[1];
    }
    return 0.0f;
  }
  inline float getSndParticleVelocityZAt(int i)
  {
    assert(i >= 0);
    if (mSndParticleVelocity && !mSndParticleVelocity->empty()) {
      assert(i < mSndParticleVelocity->size());
      return (*mSndParticleVelocity)[i].pos[2];
    }
    return 0.0f;
  }

  inline float *getFlipParticleData()
  {
    return (mFlipParticleData && !mFlipParticleData->empty()) ?
               (float *)&mFlipParticleData->front() :
               NULL;
  }
  inline float *getSndParticleData()
  {
    return (mSndParticleData && !mSndParticleData->empty()) ? (float *)&mSndParticleData->front() :
                                                              NULL;
  }

  inline float *getFlipParticleVelocity()
  {
    return (mFlipParticleVelocity && !mFlipParticleVelocity->empty()) ?
               (float *)&mFlipParticleVelocity->front() :
               NULL;
  }
  inline float *getSndParticleVelocity()
  {
    return (mSndParticleVelocity && !mSndParticleVelocity->empty()) ?
               (float *)&mSndParticleVelocity->front() :
               NULL;
  }
  inline float *getSndParticleLife()
  {
    return (mSndParticleLife && !mSndParticleLife->empty()) ? (float *)&mSndParticleLife->front() :
                                                              NULL;
  }

  inline int getNumFlipParticles()
  {
    return (mFlipParticleData && !mFlipParticleData->empty()) ? mFlipParticleData->size() : 0;
  }
  inline int getNumSndParticles()
  {
    return (mSndParticleData && !mSndParticleData->empty()) ? mSndParticleData->size() : 0;
  }

  inline bool usingFlipFromFile()
  {
    return mFlipFromFile;
  }
  inline bool usingMeshFromFile()
  {
    return mMeshFromFile;
  }
  inline bool usingParticleFromFile()
  {
    return mParticlesFromFile;
  }

  // Direct access to solver time attributes
  int getFrame();
  float getTimestep();
  void adaptTimestep();

  bool needsRealloc(FluidModifierData *mmd);

 private:
  // simulation constants
  size_t mTotalCells;
  size_t mTotalCellsHigh;
  size_t mTotalCellsMesh;
  size_t mTotalCellsParticles;

  unordered_map<string, string> mRNAMap;

  int mCurrentID;

  bool mUsingHeat;
  bool mUsingColors;
  bool mUsingFire;
  bool mUsingObstacle;
  bool mUsingGuiding;
  bool mUsingFractions;
  bool mUsingInvel;
  bool mUsingOutflow;
  bool mUsingNoise;
  bool mUsingMesh;
  bool mUsingDiffusion;
  bool mUsingMVel;
  bool mUsingLiquid;
  bool mUsingSmoke;
  bool mUsingDrops;
  bool mUsingBubbles;
  bool mUsingFloats;
  bool mUsingTracers;

  bool mFlipFromFile;
  bool mMeshFromFile;
  bool mParticlesFromFile;
  bool mSmokeFromFile;
  bool mNoiseFromFile;

  int mResX;
  int mResY;
  int mResZ;
  int mMaxRes;

  int mResXNoise;
  int mResYNoise;
  int mResZNoise;
  int mResXMesh;
  int mResYMesh;
  int mResZMesh;
  int mResXParticle;
  int mResYParticle;
  int mResZParticle;
  int *mResGuiding;

  int mUpresMesh;
  int mUpresParticle;

  float mTempAmb; /* ambient temperature */
  float mConstantScaling;

  // Fluid grids
  float *mVelocityX;
  float *mVelocityY;
  float *mVelocityZ;
  float *mObVelocityX;
  float *mObVelocityY;
  float *mObVelocityZ;
  float *mGuideVelocityX;
  float *mGuideVelocityY;
  float *mGuideVelocityZ;
  float *mInVelocityX;
  float *mInVelocityY;
  float *mInVelocityZ;
  float *mForceX;
  float *mForceY;
  float *mForceZ;
  int *mFlags;
  float *mNumObstacle;
  float *mNumGuide;

  // Smoke grids
  float *mDensity;
  float *mHeat;
  float *mFlame;
  float *mFuel;
  float *mReact;
  float *mColorR;
  float *mColorG;
  float *mColorB;
  float *mShadow;
  float *mDensityIn;
  float *mHeatIn;
  float *mFuelIn;
  float *mReactIn;
  float *mEmissionIn;
  float *mColorRIn;
  float *mColorGIn;
  float *mColorBIn;
  float *mDensityHigh;
  float *mFlameHigh;
  float *mFuelHigh;
  float *mReactHigh;
  float *mColorRHigh;
  float *mColorGHigh;
  float *mColorBHigh;
  float *mTextureU;
  float *mTextureV;
  float *mTextureW;
  float *mTextureU2;
  float *mTextureV2;
  float *mTextureW2;

  // Liquid grids
  float *mPhiIn;
  float *mPhiStaticIn;
  float *mPhiObsIn;
  float *mPhiObsStaticIn;
  float *mPhiGuideIn;
  float *mPhiOutIn;
  float *mPhiOutStaticIn;
  float *mPhi;

  // Mesh fields
  vector<Node> *mMeshNodes;
  vector<Triangle> *mMeshTriangles;
  vector<pVel> *mMeshVelocities;

  // Particle fields
  vector<pData> *mFlipParticleData;
  vector<pVel> *mFlipParticleVelocity;

  vector<pData> *mSndParticleData;
  vector<pVel> *mSndParticleVelocity;
  vector<float> *mSndParticleLife;

  void initializeRNAMap(struct FluidModifierData *mmd = NULL);
  void initDomain(struct FluidModifierData *mmd = NULL);
  void initNoise(struct FluidModifierData *mmd = NULL);
  void initMesh(struct FluidModifierData *mmd = NULL);
  void initSmoke(struct FluidModifierData *mmd = NULL);
  void initSmokeNoise(struct FluidModifierData *mmd = NULL);
  void initializeMantaflow();
  void terminateMantaflow();
  bool runPythonString(vector<string> commands);
  string getRealValue(const string &varName);
  string parseLine(const string &line);
  string parseScript(const string &setup_string, FluidModifierData *mmd = NULL);
  string getDirectory(struct FluidModifierData *mmd, string subdirectory);
  string getFile(struct FluidModifierData *mmd,
                 string subdirectory,
                 string fname,
                 string extension,
                 int framenr);
};

#endif
