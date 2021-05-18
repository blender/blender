

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Vortex sheets
 * (warning, the vortex methods are currently experimental, and not fully supported!)
 *
 ******************************************************************************/

#ifndef _VORTEXSHEET_H
#define _VORTEXSHEET_H

#include "mesh.h"

namespace Manta {

//! Stores vortex sheet info
struct VortexSheetInfo {
  VortexSheetInfo()
      : vorticity(0.0),
        vorticitySmoothed(0.0),
        circulation(0.0),
        smokeAmount(1.0),
        smokeParticles(0.0)
  {
  }

  Vec3 vorticity;
  Vec3 vorticitySmoothed;
  Vec3 circulation;
  Real smokeAmount, smokeParticles;
};

//! Manages vortex sheet info
struct VorticityChannel : public SimpleTriChannel<VortexSheetInfo> {
  virtual TriChannel *clone()
  {
    VorticityChannel *vc = new VorticityChannel();
    *vc = *this;
    return vc;
  }
};

//! Manages 3D texture coordinates
struct TexCoord3Channel : public SimpleNodeChannel<Vec3> {
  virtual NodeChannel *clone()
  {
    TexCoord3Channel *tc = new TexCoord3Channel();
    *tc = *this;
    return tc;
  }

  void addInterpol(int a, int b, Real alpha)
  {
    data.push_back((1.0 - alpha) * data[a] + alpha * data[b]);
  }
  void mergeWith(int node, int delnode, Real alpha)
  {
    data[node] = 0.5 * (data[node] + data[delnode]);
  }
};

struct TurbulenceInfo {
  TurbulenceInfo() : k(0.0), epsilon(0.0)
  {
  }
  TurbulenceInfo(const TurbulenceInfo &a, const TurbulenceInfo &b, Real alpha)
      : k((1.0 - alpha) * a.k + alpha * b.k),
        epsilon((1.0 - alpha) * a.epsilon + alpha * b.epsilon)
  {
  }
  Real k, epsilon;
};

//! Manages k-epsilon information
struct TurbulenceChannel : public SimpleNodeChannel<TurbulenceInfo> {
  virtual NodeChannel *clone()
  {
    TurbulenceChannel *tc = new TurbulenceChannel();
    *tc = *this;
    return tc;
  }

  void addInterpol(int a, int b, Real alpha)
  {
    data.push_back(TurbulenceInfo(data[a], data[b], alpha));
  }
  void mergeWith(int node, int delnode, Real alpha)
  {
    data[node] = TurbulenceInfo(data[node], data[delnode], 0.5);
  }
};

//! Typed Mesh with a vorticity and 2 texcoord3 channels
class VortexSheetMesh : public Mesh {
 public:
  VortexSheetMesh(FluidSolver *parent);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "VortexSheetMesh::VortexSheetMesh", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new VortexSheetMesh(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "VortexSheetMesh::VortexSheetMesh", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("VortexSheetMesh::VortexSheetMesh", e.what());
      return -1;
    }
  }

  virtual Mesh *clone();

  virtual MeshType getType()
  {
    return TypeVortexSheet;
  }

  inline VortexSheetInfo &sheet(int i)
  {
    return mVorticity.data[i];
  };
  inline Vec3 &tex1(int i)
  {
    return mTex1.data[i];
  }
  inline Vec3 &tex2(int i)
  {
    return mTex2.data[i];
  }
  inline TurbulenceInfo &turb(int i)
  {
    return mTurb.data[i];
  }
  void setReferenceTexOffset(const Vec3 &ref)
  {
    mTexOffset = ref;
  }
  void resetTex1();
  void resetTex2();

  void calcCirculation();
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      VortexSheetMesh *pbo = dynamic_cast<VortexSheetMesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "VortexSheetMesh::calcCirculation", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->calcCirculation();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "VortexSheetMesh::calcCirculation", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("VortexSheetMesh::calcCirculation", e.what());
      return 0;
    }
  }

  void calcVorticity();
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      VortexSheetMesh *pbo = dynamic_cast<VortexSheetMesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "VortexSheetMesh::calcVorticity", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->calcVorticity();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "VortexSheetMesh::calcVorticity", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("VortexSheetMesh::calcVorticity", e.what());
      return 0;
    }
  }

  void reinitTexCoords();
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      VortexSheetMesh *pbo = dynamic_cast<VortexSheetMesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "VortexSheetMesh::reinitTexCoords", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->reinitTexCoords();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "VortexSheetMesh::reinitTexCoords", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("VortexSheetMesh::reinitTexCoords", e.what());
      return 0;
    }
  }

 protected:
  Vec3 mTexOffset;
  VorticityChannel mVorticity;
  TexCoord3Channel mTex1, mTex2;
  TurbulenceChannel mTurb;

 public:
  PbArgs _args;
}
#define _C_VortexSheetMesh
;

};  // namespace Manta

#endif
