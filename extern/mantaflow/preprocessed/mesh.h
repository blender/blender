

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
 * Meshes
 *
 *  note: this is only a temporary solution, details are bound to change
 *        long term goal is integration with Split&Merge code by Wojtan et al.
 *
 ******************************************************************************/

#ifndef _MESH_H
#define _MESH_H

#include <vector>
#include "manta.h"
#include "vectorbase.h"
#include <set>
#include "levelset.h"

namespace Manta {

// fwd decl
class GridBase;
// class LevelsetGrid;
class FlagGrid;
class MACGrid;
class Shape;
class MeshDataBase;
template<class T> class MeshDataImpl;

//! Node position and flags
struct Node {
  Node() : flags(0), pos(Vec3::Zero), normal(Vec3::Zero)
  {
  }
  Node(const Vec3 &p) : flags(0), pos(p)
  {
  }
  int flags;
  Vec3 pos, normal;
};

//! Carries indices of its nodes
struct Triangle {
  Triangle() : flags(0)
  {
    c[0] = c[1] = c[2] = 0;
  }
  Triangle(int n0, int n1, int n2) : flags(0)
  {
    c[0] = n0;
    c[1] = n1;
    c[2] = n2;
  }

  int c[3];
  int flags;
};

//! For fast access to nodes and neighboring triangles
struct Corner {
  Corner() : tri(-1), node(-1), opposite(-1), next(-1), prev(-1){};
  Corner(int t, int n) : tri(t), node(n), opposite(-1), next(-1), prev(-1)
  {
  }

  int tri;
  int node;
  int opposite;
  int next;
  int prev;
};

//! Base class for mesh data channels (texture coords, vorticity, ...)
struct NodeChannel {
  virtual ~NodeChannel(){};
  virtual void resize(int num) = 0;
  virtual int size() = 0;
  virtual NodeChannel *clone() = 0;

  virtual void addInterpol(int a, int b, Real alpha) = 0;
  virtual void mergeWith(int node, int delnode, Real alpha) = 0;
  virtual void renumber(const std::vector<int> &newIndex, int newsize) = 0;
};

//! Node channel using only a vector
template<class T> struct SimpleNodeChannel : public NodeChannel {
  SimpleNodeChannel(){};
  SimpleNodeChannel(const SimpleNodeChannel<T> &a) : data(a.data)
  {
  }
  void resize(int num)
  {
    data.resize(num);
  }
  virtual int size()
  {
    return data.size();
  }
  virtual void renumber(const std::vector<int> &newIndex, int newsize);

  // virtual void addSplit(int from, Real alpha) { data.push_back(data[from]); }

  std::vector<T> data;
};

//! Base class for mesh data channels (texture coords, vorticity, ...)
struct TriChannel {
  virtual ~TriChannel(){};
  virtual void resize(int num) = 0;
  virtual TriChannel *clone() = 0;
  virtual int size() = 0;

  virtual void addNew() = 0;
  virtual void addSplit(int from, Real alpha) = 0;
  virtual void remove(int tri) = 0;
};

//! Tri channel using only a vector
template<class T> struct SimpleTriChannel : public TriChannel {
  SimpleTriChannel(){};
  SimpleTriChannel(const SimpleTriChannel<T> &a) : data(a.data)
  {
  }
  void resize(int num)
  {
    data.resize(num);
  }
  void remove(int tri)
  {
    if (tri != (int)data.size() - 1)
      data[tri] = *data.rbegin();
    data.pop_back();
  }
  virtual int size()
  {
    return data.size();
  }

  virtual void addSplit(int from, Real alpha)
  {
    data.push_back(data[from]);
  }
  virtual void addNew()
  {
    data.push_back(T());
  }

  std::vector<T> data;
};

struct OneRing {
  OneRing()
  {
  }
  std::set<int> nodes;
  std::set<int> tris;
};

//! Triangle mesh class
/*! note: this is only a temporary solution, details are bound to change
          long term goal is integration with Split&Merge code by Wojtan et al.*/
class Mesh : public PbClass {
 public:
  Mesh(FluidSolver *parent);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "Mesh::Mesh", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new Mesh(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "Mesh::Mesh", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::Mesh", e.what());
      return -1;
    }
  }

  virtual ~Mesh();
  virtual Mesh *clone();

  enum NodeFlags { NfNone = 0, NfFixed = 1, NfMarked = 2, NfKillme = 4, NfCollide = 8 };
  enum FaceFlags { FfNone = 0, FfDoubled = 1, FfMarked = 2 };
  enum MeshType { TypeNormal = 0, TypeVortexSheet };

  virtual MeshType getType()
  {
    return TypeNormal;
  }

  Real computeCenterOfMass(Vec3 &cm) const;
  void computeVertexNormals();

  // plugins
  void clear();
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::clear", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clear();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::clear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::clear", e.what());
      return 0;
    }
  }

  void fromShape(Shape &shape, bool append = false);
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::fromShape", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Shape &shape = *_args.getPtr<Shape>("shape", 0, &_lock);
        bool append = _args.getOpt<bool>("append", 1, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->fromShape(shape, append);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::fromShape", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::fromShape", e.what());
      return 0;
    }
  }

  void advectInGrid(FlagGrid &flags, MACGrid &vel, int integrationMode);
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::advectInGrid", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
        int integrationMode = _args.get<int>("integrationMode", 2, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->advectInGrid(flags, vel, integrationMode);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::advectInGrid", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::advectInGrid", e.what());
      return 0;
    }
  }

  void scale(Vec3 s);
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::scale", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Vec3 s = _args.get<Vec3>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->scale(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::scale", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::scale", e.what());
      return 0;
    }
  }

  void offset(Vec3 o);
  static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::offset", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Vec3 o = _args.get<Vec3>("o", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->offset(o);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::offset", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::offset", e.what());
      return 0;
    }
  }

  void rotate(Vec3 thetas);
  static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::rotate", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Vec3 thetas = _args.get<Vec3>("thetas", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->rotate(thetas);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::rotate", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::rotate", e.what());
      return 0;
    }
  }

  void computeVelocity(Mesh &oldMesh, MACGrid &vel);
  static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::computeVelocity", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Mesh &oldMesh = *_args.getPtr<Mesh>("oldMesh", 0, &_lock);
        MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->computeVelocity(oldMesh, vel);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::computeVelocity", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::computeVelocity", e.what());
      return 0;
    }
  }

  //! file io
  int load(std::string name, bool append = false);
  static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::load", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        std::string name = _args.get<std::string>("name", 0, &_lock);
        bool append = _args.getOpt<bool>("append", 1, false, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->load(name, append));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::load", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::load", e.what());
      return 0;
    }
  }

  int save(std::string name);
  static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::save", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->save(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::save", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::save", e.what());
      return 0;
    }
  }

  void computeLevelset(LevelsetGrid &levelset, Real sigma, Real cutoff = -1.);
  static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::computeLevelset", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        LevelsetGrid &levelset = *_args.getPtr<LevelsetGrid>("levelset", 0, &_lock);
        Real sigma = _args.get<Real>("sigma", 1, &_lock);
        Real cutoff = _args.getOpt<Real>("cutoff", 2, -1., &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->computeLevelset(levelset, sigma, cutoff);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::computeLevelset", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::computeLevelset", e.what());
      return 0;
    }
  }

  LevelsetGrid getLevelset(Real sigma, Real cutoff = -1.);
  static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::getLevelset", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real sigma = _args.get<Real>("sigma", 0, &_lock);
        Real cutoff = _args.getOpt<Real>("cutoff", 1, -1., &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getLevelset(sigma, cutoff));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::getLevelset", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::getLevelset", e.what());
      return 0;
    }
  }

  //! map mesh to grid with sdf
  void applyMeshToGrid(GridBase *grid,
                       FlagGrid *respectFlags = 0,
                       Real cutoff = -1.,
                       Real meshSigma = 2.);
  static PyObject *_W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::applyMeshToGrid", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        GridBase *grid = _args.getPtr<GridBase>("grid", 0, &_lock);
        FlagGrid *respectFlags = _args.getPtrOpt<FlagGrid>("respectFlags", 1, 0, &_lock);
        Real cutoff = _args.getOpt<Real>("cutoff", 2, -1., &_lock);
        Real meshSigma = _args.getOpt<Real>("meshSigma", 3, 2., &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->applyMeshToGrid(grid, respectFlags, cutoff, meshSigma);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::applyMeshToGrid", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::applyMeshToGrid", e.what());
      return 0;
    }
  }

  //! get data pointer of nodes
  std::string getNodesDataPointer();
  static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::getNodesDataPointer", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getNodesDataPointer());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::getNodesDataPointer", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::getNodesDataPointer", e.what());
      return 0;
    }
  }

  //! get data pointer of tris
  std::string getTrisDataPointer();
  static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::getTrisDataPointer", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getTrisDataPointer());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::getTrisDataPointer", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::getTrisDataPointer", e.what());
      return 0;
    }
  }

  // ops
  Mesh &operator=(const Mesh &o);

  // accessors
  inline int numTris() const
  {
    return mTris.size();
  }
  inline int numNodes() const
  {
    return mNodes.size();
  }
  inline int numTriChannels() const
  {
    return mTriChannels.size();
  }
  inline int numNodeChannels() const
  {
    return mNodeChannels.size();
  }

  //! return size of container
  //! note , python binding disabled for now! cannot yet deal with long-long types
  inline IndexInt size() const
  {
    return mNodes.size();
  }
  //! slow virtual function of base class, also returns size
  virtual IndexInt getSizeSlow() const
  {
    return size();
  }

  inline Triangle &tris(int i)
  {
    return mTris[i];
  }
  inline Node &nodes(int i)
  {
    return mNodes[i];
  }
  inline Corner &corners(int tri, int c)
  {
    return mCorners[tri * 3 + c];
  }
  inline Corner &corners(int c)
  {
    return mCorners[c];
  }
  inline NodeChannel *nodeChannel(int i)
  {
    return mNodeChannels[i];
  }
  inline TriChannel *triChannel(int i)
  {
    return mTriChannels[i];
  }

  // allocate memory (eg upon load)
  void resizeTris(int numTris);
  void resizeNodes(int numNodes);

  inline bool isNodeFixed(int n)
  {
    return mNodes[n].flags & NfFixed;
  }
  inline bool isTriangleFixed(int t)
  {
    return (mNodes[mTris[t].c[0]].flags & NfFixed) || (mNodes[mTris[t].c[1]].flags & NfFixed) ||
           (mNodes[mTris[t].c[2]].flags & NfFixed);
  }

  inline const Vec3 getNode(int tri, int c) const
  {
    return mNodes[mTris[tri].c[c]].pos;
  }
  inline Vec3 &getNode(int tri, int c)
  {
    return mNodes[mTris[tri].c[c]].pos;
  }
  inline const Vec3 getEdge(int tri, int e) const
  {
    return getNode(tri, (e + 1) % 3) - getNode(tri, e);
  }
  inline OneRing &get1Ring(int node)
  {
    return m1RingLookup[node];
  }
  inline Real getFaceArea(int t) const
  {
    Vec3 c0 = mNodes[mTris[t].c[0]].pos;
    return 0.5 * norm(cross(mNodes[mTris[t].c[1]].pos - c0, mNodes[mTris[t].c[2]].pos - c0));
  }
  inline Vec3 getFaceNormal(int t)
  {
    Vec3 c0 = mNodes[mTris[t].c[0]].pos;
    return getNormalized(cross(mNodes[mTris[t].c[1]].pos - c0, mNodes[mTris[t].c[2]].pos - c0));
  }
  inline Vec3 getFaceCenter(int t) const
  {
    return (mNodes[mTris[t].c[0]].pos + mNodes[mTris[t].c[1]].pos + mNodes[mTris[t].c[2]].pos) /
           3.0;
  }
  inline std::vector<Node> &getNodeData()
  {
    return mNodes;
  }

  void mergeNode(int node, int delnode);
  int addNode(Node a);
  int addTri(Triangle a);
  void addCorner(Corner a);
  void removeTri(int tri);
  void removeTriFromLookup(int tri);
  void removeNodes(const std::vector<int> &deletedNodes);
  void rebuildCorners(int from = 0, int to = -1);
  void rebuildLookup(int from = 0, int to = -1);
  void rebuildQuickCheck();
  void fastNodeLookupRebuild(int corner);
  void sanityCheck(bool strict = true,
                   std::vector<int> *deletedNodes = 0,
                   std::map<int, bool> *taintedTris = 0);

  void addTriChannel(TriChannel *c)
  {
    mTriChannels.push_back(c);
    rebuildChannels();
  }
  void addNodeChannel(NodeChannel *c)
  {
    mNodeChannels.push_back(c);
    rebuildChannels();
  }

  //! mesh data functions

  //! create a mesh data object
  PbClass *create(PbType type, PbTypeVec T = PbTypeVec(), const std::string &name = "");
  static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Mesh *pbo = dynamic_cast<Mesh *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Mesh::create", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        PbType type = _args.get<PbType>("type", 0, &_lock);
        PbTypeVec T = _args.getOpt<PbTypeVec>("T", 1, PbTypeVec(), &_lock);
        const std::string &name = _args.getOpt<std::string>("name", 2, "", &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->create(type, T, name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Mesh::create", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Mesh::create", e.what());
      return 0;
    }
  }

  //! add a mesh data field, set its parent mesh pointer
  void registerMdata(MeshDataBase *mdata);
  void registerMdataReal(MeshDataImpl<Real> *mdata);
  void registerMdataVec3(MeshDataImpl<Vec3> *mdata);
  void registerMdataInt(MeshDataImpl<int> *mdata);
  //! remove a mesh data entry
  void deregister(MeshDataBase *mdata);
  //! add one zero entry to all data fields
  void addAllMdata();
  // note - deletion of mdata is handled in compress function

  //! how many are there?
  IndexInt getNumMdata() const
  {
    return mMeshData.size();
  }
  //! access one of the fields
  MeshDataBase *getMdata(int i)
  {
    return mMeshData[i];
  }

  //! update data fields
  void updateDataFields();

 protected:
  void rebuildChannels();

  std::vector<Node> mNodes;
  std::vector<Triangle> mTris;
  std::vector<Corner> mCorners;
  std::vector<NodeChannel *> mNodeChannels;
  std::vector<TriChannel *> mTriChannels;
  std::vector<OneRing> m1RingLookup;

  //! store mesh data , each pointer has its own storage vector of a certain type (int, real, vec3)
  std::vector<MeshDataBase *> mMeshData;
  //! lists of different types, for fast operations w/o virtual function calls
  std::vector<MeshDataImpl<Real> *> mMdataReal;
  std::vector<MeshDataImpl<Vec3> *> mMdataVec3;
  std::vector<MeshDataImpl<int> *>
      mMdataInt;  //! indicate that mdata of this mesh is copied, and needs to be freed
  bool mFreeMdata;
 public:
  PbArgs _args;
}
#define _C_Mesh
;

//******************************************************************************

//! abstract interface for mesh data
class MeshDataBase : public PbClass {
 public:
  MeshDataBase(FluidSolver *parent);
  static int _W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "MeshDataBase::MeshDataBase", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new MeshDataBase(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "MeshDataBase::MeshDataBase", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataBase::MeshDataBase", e.what());
      return -1;
    }
  }

  virtual ~MeshDataBase();

  //! data type IDs, in line with those for grids
  enum MdataType { TypeNone = 0, TypeReal = 1, TypeInt = 2, TypeVec3 = 4 };

  //! interface functions, using assert instead of pure virtual for python compatibility
  virtual IndexInt getSizeSlow() const
  {
    assertMsg(false, "Dont use, override...");
    return 0;
  }
  virtual void addEntry()
  {
    assertMsg(false, "Dont use, override...");
    return;
  }
  virtual MeshDataBase *clone()
  {
    assertMsg(false, "Dont use, override...");
    return nullptr;
  }
  virtual MdataType getType() const
  {
    assertMsg(false, "Dont use, override...");
    return TypeNone;
  }
  virtual void resize(IndexInt size)
  {
    assertMsg(false, "Dont use, override...");
    return;
  }
  virtual void copyValueSlow(IndexInt from, IndexInt to)
  {
    assertMsg(false, "Dont use, override...");
    return;
  }

  //! set base pointer
  void setMesh(Mesh *set)
  {
    mMesh = set;
  }

  //! debugging
  inline void checkNodeIndex(IndexInt idx) const;

 protected:
  Mesh *mMesh;
 public:
  PbArgs _args;
}
#define _C_MeshDataBase
;

//! abstract interface for mesh data

template<class T> class MeshDataImpl : public MeshDataBase {
 public:
  MeshDataImpl(FluidSolver *parent);
  static int _W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "MeshDataImpl::MeshDataImpl", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new MeshDataImpl(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "MeshDataImpl::MeshDataImpl", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::MeshDataImpl", e.what());
      return -1;
    }
  }

  MeshDataImpl(FluidSolver *parent, MeshDataImpl<T> *other);
  virtual ~MeshDataImpl();

  //! access data
  inline T &get(IndexInt idx)
  {
    DEBUG_ONLY(checkNodeIndex(idx));
    return mData[idx];
  }
  inline const T &get(IndexInt idx) const
  {
    DEBUG_ONLY(checkNodeIndex(idx));
    return mData[idx];
  }
  inline T &operator[](IndexInt idx)
  {
    DEBUG_ONLY(checkNodeIndex(idx));
    return mData[idx];
  }
  inline const T &operator[](IndexInt idx) const
  {
    DEBUG_ONLY(checkNodeIndex(idx));
    return mData[idx];
  }

  //! set all values to 0, note - different from meshSystem::clear! doesnt modify size of array
  //! (has to stay in sync with parent system)
  void clear();
  static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::clear", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clear();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::clear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::clear", e.what());
      return 0;
    }
  }

  //! set grid from which to get data...
  void setSource(Grid<T> *grid, bool isMAC = false);
  static PyObject *_W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::setSource", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Grid<T> *grid = _args.getPtr<Grid<T>>("grid", 0, &_lock);
        bool isMAC = _args.getOpt<bool>("isMAC", 1, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setSource(grid, isMAC);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::setSource", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::setSource", e.what());
      return 0;
    }
  }

  //! mesh data base interface
  virtual IndexInt getSizeSlow() const;
  virtual void addEntry();
  virtual MeshDataBase *clone();
  virtual MdataType getType() const;
  virtual void resize(IndexInt s);
  virtual void copyValueSlow(IndexInt from, IndexInt to);

  IndexInt size() const
  {
    return mData.size();
  }

  //! fast inlined functions for per mesh operations
  inline void copyValue(IndexInt from, IndexInt to)
  {
    get(to) = get(from);
  }
  void initNewValue(IndexInt idx, Vec3 pos);

  //! python interface (similar to grid data)
  void setConst(T s);
  static PyObject *_W_20(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::setConst", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::setConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::setConst", e.what());
      return 0;
    }
  }

  void setConstRange(T s, const int begin, const int end);
  static PyObject *_W_21(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::setConstRange", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        const int begin = _args.get<int>("begin", 1, &_lock);
        const int end = _args.get<int>("end", 2, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConstRange(s, begin, end);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::setConstRange", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::setConstRange", e.what());
      return 0;
    }
  }

  MeshDataImpl<T> &copyFrom(const MeshDataImpl<T> &a);
  static PyObject *_W_22(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::copyFrom", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->copyFrom(a));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::copyFrom", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::copyFrom", e.what());
      return 0;
    }
  }

  void add(const MeshDataImpl<T> &a);
  static PyObject *_W_23(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::add", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->add(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::add", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::add", e.what());
      return 0;
    }
  }

  void sub(const MeshDataImpl<T> &a);
  static PyObject *_W_24(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::sub", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->sub(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::sub", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::sub", e.what());
      return 0;
    }
  }

  void addConst(T s);
  static PyObject *_W_25(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::addConst", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::addConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::addConst", e.what());
      return 0;
    }
  }

  void addScaled(const MeshDataImpl<T> &a, const T &factor);
  static PyObject *_W_26(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::addScaled", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        const T &factor = *_args.getPtr<T>("factor", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addScaled(a, factor);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::addScaled", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::addScaled", e.what());
      return 0;
    }
  }

  void mult(const MeshDataImpl<T> &a);
  static PyObject *_W_27(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::mult", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->mult(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::mult", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::mult", e.what());
      return 0;
    }
  }

  void multConst(T s);
  static PyObject *_W_28(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::multConst", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->multConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::multConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::multConst", e.what());
      return 0;
    }
  }

  void safeDiv(const MeshDataImpl<T> &a);
  static PyObject *_W_29(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::safeDiv", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<T> &a = *_args.getPtr<MeshDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->safeDiv(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::safeDiv", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::safeDiv", e.what());
      return 0;
    }
  }

  void clamp(Real min, Real max);
  static PyObject *_W_30(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::clamp", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real min = _args.get<Real>("min", 0, &_lock);
        Real max = _args.get<Real>("max", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clamp(min, max);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::clamp", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::clamp", e.what());
      return 0;
    }
  }

  void clampMin(Real vmin);
  static PyObject *_W_31(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::clampMin", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real vmin = _args.get<Real>("vmin", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clampMin(vmin);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::clampMin", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::clampMin", e.what());
      return 0;
    }
  }

  void clampMax(Real vmax);
  static PyObject *_W_32(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::clampMax", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real vmax = _args.get<Real>("vmax", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clampMax(vmax);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::clampMax", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::clampMax", e.what());
      return 0;
    }
  }

  Real getMaxAbs();
  static PyObject *_W_33(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::getMaxAbs", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMaxAbs());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::getMaxAbs", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::getMaxAbs", e.what());
      return 0;
    }
  }

  Real getMax();
  static PyObject *_W_34(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::getMax", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMax());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::getMax", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::getMax", e.what());
      return 0;
    }
  }

  Real getMin();
  static PyObject *_W_35(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::getMin", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMin());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::getMin", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::getMin", e.what());
      return 0;
    }
  }

  T sum(const MeshDataImpl<int> *t = nullptr, const int itype = 0) const;
  static PyObject *_W_36(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::sum", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const MeshDataImpl<int> *t = _args.getPtrOpt<MeshDataImpl<int>>("t", 0, nullptr, &_lock);
        const int itype = _args.getOpt<int>("itype", 1, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sum(t, itype));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::sum", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::sum", e.what());
      return 0;
    }
  }

  Real sumSquare() const;
  static PyObject *_W_37(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::sumSquare", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sumSquare());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::sumSquare", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::sumSquare", e.what());
      return 0;
    }
  }

  Real sumMagnitude() const;
  static PyObject *_W_38(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::sumMagnitude", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sumMagnitude());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::sumMagnitude", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::sumMagnitude", e.what());
      return 0;
    }
  }

  //! special, set if int flag in t has "flag"
  void setConstIntFlag(T s, const MeshDataImpl<int> &t, const int flag);
  static PyObject *_W_39(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::setConstIntFlag", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        const MeshDataImpl<int> &t = *_args.getPtr<MeshDataImpl<int>>("t", 1, &_lock);
        const int flag = _args.get<int>("flag", 2, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConstIntFlag(s, t, flag);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::setConstIntFlag", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::setConstIntFlag", e.what());
      return 0;
    }
  }

  void printMdata(IndexInt start = -1, IndexInt stop = -1, bool printIndex = false);
  static PyObject *_W_40(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::printMdata", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        IndexInt start = _args.getOpt<IndexInt>("start", 0, -1, &_lock);
        IndexInt stop = _args.getOpt<IndexInt>("stop", 1, -1, &_lock);
        bool printIndex = _args.getOpt<bool>("printIndex", 2, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->printMdata(start, stop, printIndex);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::printMdata", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::printMdata", e.what());
      return 0;
    }
  }

  //! file io
  int save(const std::string name);
  static PyObject *_W_41(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::save", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->save(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::save", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::save", e.what());
      return 0;
    }
  }

  int load(const std::string name);
  static PyObject *_W_42(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::load", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->load(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::load", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::load", e.what());
      return 0;
    }
  }

  //! get data pointer of mesh data
  std::string getDataPointer();
  static PyObject *_W_43(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MeshDataImpl *pbo = dynamic_cast<MeshDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MeshDataImpl::getDataPointer", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getDataPointer());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MeshDataImpl::getDataPointer", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MeshDataImpl::getDataPointer", e.what());
      return 0;
    }
  }

 protected:
  //! data storage
  std::vector<T> mData;

  //! optionally , we might have an associated grid from which to grab new data
  Grid<T> *mpGridSource;  //! unfortunately , we need to distinguish mac vs regular vec3
  bool mGridSourceMAC;
 public:
  PbArgs _args;
}
#define _C_MeshDataImpl
;

// ***************************************************************************************************************
// Implementation

template<class T>
void SimpleNodeChannel<T>::renumber(const std::vector<int> &newIndex, int newsize)
{
  for (size_t i = 0; i < newIndex.size(); i++) {
    if (newIndex[i] != -1)
      data[newIndex[i]] = data[newsize + i];
  }
  data.resize(newsize);
}

inline void MeshDataBase::checkNodeIndex(IndexInt idx) const
{
  IndexInt mySize = this->getSizeSlow();
  if (idx < 0 || idx > mySize) {
    errMsg("MeshData "
           << " size " << mySize << " : index " << idx << " out of bound ");
  }
  if (mMesh && mMesh->getSizeSlow() != mySize) {
    errMsg("MeshData "
           << " size " << mySize << " does not match parent! (" << mMesh->getSizeSlow() << ") ");
  }
}

template<class T> void MeshDataImpl<T>::clear()
{
  for (IndexInt i = 0; i < (IndexInt)mData.size(); ++i)
    mData[i] = 0.;
}

}  // namespace Manta
#endif
