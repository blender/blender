

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
 * note: this is only a temporary solution, details are bound to change
 *        long term goal is integration with Split&Merge code by Wojtan et al.
 *
 ******************************************************************************/

#include "mesh.h"
#include "integrator.h"
#include "mantaio.h"
#include "kernel.h"
#include "shapes.h"
#include "noisefield.h"
//#include "grid.h"
#include <stack>
#include <cstring>

using namespace std;
namespace Manta {

Mesh::Mesh(FluidSolver *parent) : PbClass(parent)
{
}

Mesh::~Mesh()
{
  for (IndexInt i = 0; i < (IndexInt)mMeshData.size(); ++i)
    mMeshData[i]->setMesh(NULL);

  if (mFreeMdata) {
    for (IndexInt i = 0; i < (IndexInt)mMeshData.size(); ++i)
      delete mMeshData[i];
  }
}

Mesh *Mesh::clone()
{
  Mesh *nm = new Mesh(mParent);
  *nm = *this;
  nm->setName(getName());
  return nm;
}

void Mesh::deregister(MeshDataBase *mdata)
{
  bool done = false;
  // remove pointer from mesh data list
  for (IndexInt i = 0; i < (IndexInt)mMeshData.size(); ++i) {
    if (mMeshData[i] == mdata) {
      if (i < (IndexInt)mMeshData.size() - 1)
        mMeshData[i] = mMeshData[mMeshData.size() - 1];
      mMeshData.pop_back();
      done = true;
    }
  }
  if (!done)
    errMsg("Invalid pointer given, not registered!");
}

// create and attach a new mdata field to this mesh
PbClass *Mesh::create(PbType t, PbTypeVec T, const string &name)
{
#if NOPYTHON != 1
  _args.add("nocheck", true);
  if (t.str() == "")
    errMsg("Specify mesh data type to create");
  // debMsg( "Mdata creating '"<< t.str <<" with size "<< this->getSizeSlow(), 5 );

  PbClass *pyObj = PbClass::createPyObject(t.str() + T.str(), name, _args, this->getParent());

  MeshDataBase *mdata = dynamic_cast<MeshDataBase *>(pyObj);
  if (!mdata) {
    errMsg(
        "Unable to get mesh data pointer from newly created object. Only create MeshData type "
        "with a Mesh.creat() call, eg, MdataReal, MdataVec3 etc.");
    delete pyObj;
    return NULL;
  }
  else {
    this->registerMdata(mdata);
  }

  // directly init size of new mdata field:
  mdata->resize(this->getSizeSlow());
#else
  PbClass *pyObj = NULL;
#endif
  return pyObj;
}

void Mesh::registerMdata(MeshDataBase *mdata)
{
  mdata->setMesh(this);
  mMeshData.push_back(mdata);

  if (mdata->getType() == MeshDataBase::TypeReal) {
    MeshDataImpl<Real> *pd = dynamic_cast<MeshDataImpl<Real> *>(mdata);
    if (!pd)
      errMsg("Invalid mdata object posing as real!");
    this->registerMdataReal(pd);
  }
  else if (mdata->getType() == MeshDataBase::TypeInt) {
    MeshDataImpl<int> *pd = dynamic_cast<MeshDataImpl<int> *>(mdata);
    if (!pd)
      errMsg("Invalid mdata object posing as int!");
    this->registerMdataInt(pd);
  }
  else if (mdata->getType() == MeshDataBase::TypeVec3) {
    MeshDataImpl<Vec3> *pd = dynamic_cast<MeshDataImpl<Vec3> *>(mdata);
    if (!pd)
      errMsg("Invalid mdata object posing as vec3!");
    this->registerMdataVec3(pd);
  }
}
void Mesh::registerMdataReal(MeshDataImpl<Real> *pd)
{
  mMdataReal.push_back(pd);
}
void Mesh::registerMdataVec3(MeshDataImpl<Vec3> *pd)
{
  mMdataVec3.push_back(pd);
}
void Mesh::registerMdataInt(MeshDataImpl<int> *pd)
{
  mMdataInt.push_back(pd);
}

void Mesh::addAllMdata()
{
  for (IndexInt i = 0; i < (IndexInt)mMeshData.size(); ++i) {
    mMeshData[i]->addEntry();
  }
}

Real Mesh::computeCenterOfMass(Vec3 &cm) const
{

  // use double precision for summation, otherwise too much error accumulation
  double vol = 0;
  Vector3D<double> cmd(0.0);
  for (size_t tri = 0; tri < mTris.size(); tri++) {
    Vector3D<double> p1(toVec3d(getNode(tri, 0)));
    Vector3D<double> p2(toVec3d(getNode(tri, 1)));
    Vector3D<double> p3(toVec3d(getNode(tri, 2)));

    double cvol = dot(cross(p1, p2), p3) / 6.0;
    cmd += (p1 + p2 + p3) * (cvol / 4.0);
    vol += cvol;
  }
  if (vol != 0.0)
    cmd /= vol;

  cm = toVec3(cmd);
  return (Real)vol;
}

void Mesh::clear()
{
  mNodes.clear();
  mTris.clear();
  mCorners.clear();
  m1RingLookup.clear();
  for (size_t i = 0; i < mNodeChannels.size(); i++)
    mNodeChannels[i]->resize(0);
  for (size_t i = 0; i < mTriChannels.size(); i++)
    mTriChannels[i]->resize(0);

  // clear mdata fields as well
  for (size_t i = 0; i < mMdataReal.size(); i++)
    mMdataReal[i]->resize(0);
  for (size_t i = 0; i < mMdataVec3.size(); i++)
    mMdataVec3[i]->resize(0);
  for (size_t i = 0; i < mMdataInt.size(); i++)
    mMdataInt[i]->resize(0);
}

Mesh &Mesh::operator=(const Mesh &o)
{
  // wipe current data
  clear();
  if (mNodeChannels.size() != o.mNodeChannels.size() ||
      mTriChannels.size() != o.mTriChannels.size())
    errMsg("can't copy mesh, channels not identical");
  mNodeChannels.clear();
  mTriChannels.clear();

  // copy corner, nodes, tris
  mCorners = o.mCorners;
  mNodes = o.mNodes;
  mTris = o.mTris;
  m1RingLookup = o.m1RingLookup;

  // copy channels
  for (size_t i = 0; i < mNodeChannels.size(); i++)
    mNodeChannels[i] = o.mNodeChannels[i];
  for (size_t i = 0; i < o.mTriChannels.size(); i++)
    mTriChannels[i] = o.mTriChannels[i];

  return *this;
}

int Mesh::load(string name, bool append)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".gz")  // assume bobj gz
    return readBobjFile(name, this, append);
  else if (ext == ".obj")
    return readObjFile(name, this, append);
  else
    errMsg("file '" + name + "' filetype not supported");

  // dont always rebuild...
  // rebuildCorners();
  // rebuildLookup();
  return 0;
}

int Mesh::save(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".obj")
    return writeObjFile(name, this);
  else if (ext == ".gz")
    return writeBobjFile(name, this);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}

void Mesh::fromShape(Shape &shape, bool append)
{
  if (!append)
    clear();
  shape.generateMesh(this);
}

void Mesh::resizeTris(int numTris)
{
  mTris.resize(numTris);
  rebuildChannels();
}
void Mesh::resizeNodes(int numNodes)
{
  mNodes.resize(numNodes);
  rebuildChannels();
}

//! do a quick check whether a rebuild is necessary, and if yes do rebuild
void Mesh::rebuildQuickCheck()
{
  if (mCorners.size() != 3 * mTris.size())
    rebuildCorners();
  if (m1RingLookup.size() != mNodes.size())
    rebuildLookup();
}

void Mesh::rebuildCorners(int from, int to)
{
  mCorners.resize(3 * mTris.size());
  if (to < 0)
    to = mTris.size();

  // fill in basic info
  for (int tri = from; tri < to; tri++) {
    for (int c = 0; c < 3; c++) {
      const int idx = tri * 3 + c;
      mCorners[idx].tri = tri;
      mCorners[idx].node = mTris[tri].c[c];
      mCorners[idx].next = 3 * tri + ((c + 1) % 3);
      mCorners[idx].prev = 3 * tri + ((c + 2) % 3);
      mCorners[idx].opposite = -1;
    }
  }

  // set opposite info
  int maxc = to * 3;
  for (int c = from * 3; c < maxc; c++) {
    int next = mCorners[mCorners[c].next].node;
    int prev = mCorners[mCorners[c].prev].node;

    // find corner with same next/prev nodes
    for (int c2 = c + 1; c2 < maxc; c2++) {
      int next2 = mCorners[mCorners[c2].next].node;
      if (next2 != next && next2 != prev)
        continue;
      int prev2 = mCorners[mCorners[c2].prev].node;
      if (prev2 != next && prev2 != prev)
        continue;

      // found
      mCorners[c].opposite = c2;
      mCorners[c2].opposite = c;
      break;
    }
    if (mCorners[c].opposite < 0) {
      // didn't find opposite
      errMsg("can't rebuild corners, index without an opposite");
    }
  }

  rebuildChannels();
}

void Mesh::rebuildLookup(int from, int to)
{
  if (from == 0 && to < 0)
    m1RingLookup.clear();
  m1RingLookup.resize(mNodes.size());
  if (to < 0)
    to = mTris.size();
  from *= 3;
  to *= 3;
  for (int i = from; i < to; i++) {
    const int node = mCorners[i].node;
    m1RingLookup[node].nodes.insert(mCorners[mCorners[i].next].node);
    m1RingLookup[node].nodes.insert(mCorners[mCorners[i].prev].node);
    m1RingLookup[node].tris.insert(mCorners[i].tri);
  }
}

void Mesh::rebuildChannels()
{
  for (size_t i = 0; i < mTriChannels.size(); i++)
    mTriChannels[i]->resize(mTris.size());
  for (size_t i = 0; i < mNodeChannels.size(); i++)
    mNodeChannels[i]->resize(mNodes.size());
}

struct _KnAdvectMeshInGrid : public KernelBase {
  _KnAdvectMeshInGrid(const KernelBase &base,
                      vector<Node> &nodes,
                      const FlagGrid &flags,
                      const MACGrid &vel,
                      const Real dt,
                      vector<Vec3> &u)
      : KernelBase(base), nodes(nodes), flags(flags), vel(vel), dt(dt), u(u)
  {
  }
  inline void op(IndexInt idx,
                 vector<Node> &nodes,
                 const FlagGrid &flags,
                 const MACGrid &vel,
                 const Real dt,
                 vector<Vec3> &u) const
  {
    if (nodes[idx].flags & Mesh::NfFixed)
      u[idx] = 0.0;
    else if (!flags.isInBounds(nodes[idx].pos, 1))
      u[idx] = 0.0;
    else
      u[idx] = vel.getInterpolated(nodes[idx].pos) * dt;
  }
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, nodes, flags, vel, dt, u);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  vector<Node> &nodes;
  const FlagGrid &flags;
  const MACGrid &vel;
  const Real dt;
  vector<Vec3> &u;
};
struct KnAdvectMeshInGrid : public KernelBase {
  KnAdvectMeshInGrid(vector<Node> &nodes, const FlagGrid &flags, const MACGrid &vel, const Real dt)
      : KernelBase(nodes.size()),
        _inner(KernelBase(nodes.size()), nodes, flags, vel, dt, u),
        nodes(nodes),
        flags(flags),
        vel(vel),
        dt(dt),
        u((size))
  {
    runMessage();
    run();
  }
  void run()
  {
    _inner.run();
  }
  inline operator vector<Vec3>()
  {
    return u;
  }
  inline vector<Vec3> &getRet()
  {
    return u;
  }
  inline vector<Node> &getArg0()
  {
    return nodes;
  }
  typedef vector<Node> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline const Real &getArg3()
  {
    return dt;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel KnAdvectMeshInGrid ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  _KnAdvectMeshInGrid _inner;
  vector<Node> &nodes;
  const FlagGrid &flags;
  const MACGrid &vel;
  const Real dt;
  vector<Vec3> u;
};

// advection plugin
void Mesh::advectInGrid(FlagGrid &flags, MACGrid &vel, int integrationMode)
{
  KnAdvectMeshInGrid kernel(mNodes, flags, vel, getParent()->getDt());
  integratePointSet(kernel, integrationMode);
}

void Mesh::scale(Vec3 s)
{
  for (size_t i = 0; i < mNodes.size(); i++)
    mNodes[i].pos *= s;
}

void Mesh::offset(Vec3 o)
{
  for (size_t i = 0; i < mNodes.size(); i++)
    mNodes[i].pos += o;
}

void Mesh::rotate(Vec3 thetas)
{
  // rotation thetas are in radians (e.g. pi is equal to 180 degrees)
  auto rotate = [&](Real theta, unsigned int first_axis, unsigned int second_axis) {
    if (theta == 0.0f)
      return;

    Real sin_t = sin(theta);
    Real cos_t = cos(theta);

    Real sin_sign = first_axis == 0u && second_axis == 2u ? -1.0f : 1.0f;
    sin_t *= sin_sign;

    size_t length = mNodes.size();
    for (size_t n = 0; n < length; ++n) {
      Vec3 &node = mNodes[n].pos;
      Real first_axis_val = node[first_axis];
      Real second_axis_val = node[second_axis];
      node[first_axis] = first_axis_val * cos_t - second_axis_val * sin_t;
      node[second_axis] = second_axis_val * cos_t + first_axis_val * sin_t;
    }
  };

  // rotate x
  rotate(thetas[0], 1u, 2u);
  // rotate y
  rotate(thetas[1], 0u, 2u);
  // rotate z
  rotate(thetas[2], 0u, 1u);
}

void Mesh::computeVelocity(Mesh &oldMesh, MACGrid &vel)
{
  // Early return if sizes do not match
  if (oldMesh.mNodes.size() != mNodes.size())
    return;

  // temp grid
  Grid<Vec3> veloMeanCounter(getParent());
  veloMeanCounter.setConst(0.0f);

  bool bIs2D = getParent()->is2D();

  // calculate velocities from previous to current frame (per vertex)
  for (size_t i = 0; i < mNodes.size(); ++i) {
    // skip vertices that are not needed for 2D
    if (bIs2D && (mNodes[i].pos.z < -0.5f || mNodes[i].pos.z > 0.5f))
      continue;

    Vec3 velo = mNodes[i].pos - oldMesh.mNodes[i].pos;
    vel.setInterpolated(mNodes[i].pos, velo, &(veloMeanCounter[0]));
  }

  // discretize the vertex velocities by averaging them on the grid
  vel.safeDivide(veloMeanCounter);
}

void Mesh::removeTri(int tri)
{
  // delete triangles by overwriting them with elements from the end of the array.
  if (tri != (int)mTris.size() - 1) {
    // if this is the last element, and it is marked for deletion,
    // don't waste cycles transfering data to itself,
    // and DEFINITELY don't transfer .opposite data to other, untainted triangles.

    // old corners hold indices on the end of the corners array
    // new corners holds indices in the new spot in the middle of the array
    Corner *oldcorners[3];
    Corner *newcorners[3];
    int oldtri = mTris.size() - 1;
    for (int c = 0; c < 3; c++) {
      oldcorners[c] = &corners(oldtri, c);
      newcorners[c] = &corners(tri, c);
    }

    // move the position of the triangle
    mTris[tri] = mTris[oldtri];

    // 1) update c.node, c.opposite (c.next and c.prev should be fine as they are)
    for (int c = 0; c < 3; c++) {
      newcorners[c]->node = mTris[tri].c[c];
      newcorners[c]->opposite = oldcorners[c]->opposite;
    }

    //  2) c.opposite.opposite = c
    for (int c = 0; c < 3; c++) {
      if (newcorners[c]->opposite >= 0)
        mCorners[newcorners[c]->opposite].opposite = 3 * tri + c;
    }

    // update tri lookup
    for (int c = 0; c < 3; c++) {
      int node = mTris[tri].c[c];
      m1RingLookup[node].tris.erase(oldtri);
      m1RingLookup[node].tris.insert(tri);
    }
  }

  // transfer tri props
  for (size_t p = 0; p < mTriChannels.size(); p++)
    mTriChannels[p]->remove(tri);

  // pop the triangle and corners out of the vector
  mTris.pop_back();
  mCorners.resize(mTris.size() * 3);
}

void Mesh::removeNodes(const vector<int> &deletedNodes)
{
  // After we delete the nodes that are marked for removal,
  // the size of mNodes will be the current size - the size of the deleted array.
  // We are going to move the elements at the end of the array
  // (everything with an index >= newsize)
  // to the deleted spots.
  // We have to map all references to the last few nodes to their new locations.
  int newsize = (int)(mNodes.size() - deletedNodes.size());

  vector<int> new_index(deletedNodes.size());
  int di, ni;
  for (ni = 0; ni < (int)new_index.size(); ni++)
    new_index[ni] = 0;
  for (di = 0; di < (int)deletedNodes.size(); di++) {
    if (deletedNodes[di] >= newsize)
      new_index[deletedNodes[di] - newsize] = -1;  // tag this node as invalid
  }
  for (di = 0, ni = 0; ni < (int)new_index.size(); ni++, di++) {
    // we need to find a valid node to move
    // we marked invalid nodes in the earlier loop with a (-1),
    // so pick anything but those
    while (ni < (int)new_index.size() && new_index[ni] == -1)
      ni++;

    if (ni >= (int)new_index.size())
      break;

    // next we need to find a valid spot to move the node to.
    // we iterate through deleted[] until we find a valid spot
    while (di < (int)new_index.size() && deletedNodes[di] >= newsize)
      di++;

    // now we assign the valid node to the valid spot
    new_index[ni] = deletedNodes[di];
  }

  // Now we have a map of valid indices.
  // we move node[newsize+i] to location new_index[i].
  // We ignore the nodes with a -1 index, because they should not be moved.
  for (int i = 0; i < (int)new_index.size(); i++) {
    if (new_index[i] != -1)
      mNodes[new_index[i]] = mNodes[newsize + i];
  }
  mNodes.resize(newsize);

  // handle vertex properties
  for (size_t i = 0; i < mNodeChannels.size(); i++)
    mNodeChannels[i]->renumber(new_index, newsize);

  // finally, we reconnect everything that used to point to this vertex.
  for (size_t tri = 0, n = 0; tri < mTris.size(); tri++) {
    for (int c = 0; c < 3; c++, n++) {
      if (mCorners[n].node >= newsize) {
        int newindex = new_index[mCorners[n].node - newsize];
        mCorners[n].node = newindex;
        mTris[mCorners[n].tri].c[c] = newindex;
      }
    }
  }

  // renumber 1-ring
  for (int i = 0; i < (int)new_index.size(); i++) {
    if (new_index[i] != -1) {
      m1RingLookup[new_index[i]].nodes.swap(m1RingLookup[newsize + i].nodes);
      m1RingLookup[new_index[i]].tris.swap(m1RingLookup[newsize + i].tris);
    }
  }
  m1RingLookup.resize(newsize);
  vector<int> reStack(new_index.size());
  for (int i = 0; i < newsize; i++) {
    set<int> &cs = m1RingLookup[i].nodes;
    int reNum = 0;
    // find all nodes > newsize
    set<int>::reverse_iterator itend = cs.rend();
    for (set<int>::reverse_iterator it = cs.rbegin(); it != itend; ++it) {
      if (*it < newsize)
        break;
      reStack[reNum++] = *it;
    }
    // kill them and insert shifted values
    if (reNum > 0) {
      cs.erase(cs.find(reStack[reNum - 1]), cs.end());
      for (int j = 0; j < reNum; j++) {
        cs.insert(new_index[reStack[j] - newsize]);
#ifdef DEBUG
        if (new_index[reStack[j] - newsize] == -1)
          errMsg("invalid node present in 1-ring set");
#endif
      }
    }
  }
}

void Mesh::mergeNode(int node, int delnode)
{
  set<int> &ring = m1RingLookup[delnode].nodes;
  for (set<int>::iterator it = ring.begin(); it != ring.end(); ++it) {
    m1RingLookup[*it].nodes.erase(delnode);
    if (*it != node) {
      m1RingLookup[*it].nodes.insert(node);
      m1RingLookup[node].nodes.insert(*it);
    }
  }
  set<int> &ringt = m1RingLookup[delnode].tris;
  for (set<int>::iterator it = ringt.begin(); it != ringt.end(); ++it) {
    const int t = *it;
    for (int c = 0; c < 3; c++) {
      if (mCorners[3 * t + c].node == delnode) {
        mCorners[3 * t + c].node = node;
        mTris[t].c[c] = node;
      }
    }
    m1RingLookup[node].tris.insert(t);
  }
  for (size_t i = 0; i < mNodeChannels.size(); i++) {
    // weight is fixed to 1/2 for now
    mNodeChannels[i]->mergeWith(node, delnode, 0.5);
  }
}

void Mesh::removeTriFromLookup(int tri)
{
  for (int c = 0; c < 3; c++) {
    int node = mTris[tri].c[c];
    m1RingLookup[node].tris.erase(tri);
  }
}

void Mesh::addCorner(Corner a)
{
  mCorners.push_back(a);
}

int Mesh::addTri(Triangle a)
{
  mTris.push_back(a);
  for (int c = 0; c < 3; c++) {
    int node = a.c[c];
    int nextnode = a.c[(c + 1) % 3];
    if ((int)m1RingLookup.size() <= node)
      m1RingLookup.resize(node + 1);
    if ((int)m1RingLookup.size() <= nextnode)
      m1RingLookup.resize(nextnode + 1);
    m1RingLookup[node].nodes.insert(nextnode);
    m1RingLookup[nextnode].nodes.insert(node);
    m1RingLookup[node].tris.insert(mTris.size() - 1);
  }
  return mTris.size() - 1;
}

int Mesh::addNode(Node a)
{
  mNodes.push_back(a);
  if (m1RingLookup.size() < mNodes.size())
    m1RingLookup.resize(mNodes.size());

  // if mdata exists, add zero init for every node
  addAllMdata();

  return mNodes.size() - 1;
}

void Mesh::computeVertexNormals()
{
  for (size_t i = 0; i < mNodes.size(); i++) {
    mNodes[i].normal = 0.0;
  }
  for (size_t t = 0; t < mTris.size(); t++) {
    Vec3 p0 = getNode(t, 0), p1 = getNode(t, 1), p2 = getNode(t, 2);
    Vec3 n0 = p0 - p1, n1 = p1 - p2, n2 = p2 - p0;
    Real l0 = normSquare(n0), l1 = normSquare(n1), l2 = normSquare(n2);

    Vec3 nm = cross(n0, n1);

    mNodes[mTris[t].c[0]].normal += nm * (1.0 / (l0 * l2));
    mNodes[mTris[t].c[1]].normal += nm * (1.0 / (l0 * l1));
    mNodes[mTris[t].c[2]].normal += nm * (1.0 / (l1 * l2));
  }
  for (size_t i = 0; i < mNodes.size(); i++) {
    normalize(mNodes[i].normal);
  }
}

void Mesh::fastNodeLookupRebuild(int corner)
{
  int node = mCorners[corner].node;
  m1RingLookup[node].nodes.clear();
  m1RingLookup[node].tris.clear();
  int start = mCorners[corner].prev;
  int current = start;
  do {
    m1RingLookup[node].nodes.insert(mCorners[current].node);
    m1RingLookup[node].tris.insert(mCorners[current].tri);
    current = mCorners[mCorners[current].opposite].next;
    if (current < 0)
      errMsg("Can't use fastNodeLookupRebuild on incomplete surfaces");
  } while (current != start);
}

void Mesh::sanityCheck(bool strict, vector<int> *deletedNodes, map<int, bool> *taintedTris)
{
  const int nodes = numNodes(), tris = numTris(), corners = 3 * tris;
  for (size_t i = 0; i < mNodeChannels.size(); i++) {
    if (mNodeChannels[i]->size() != nodes)
      errMsg("Node channel size mismatch");
  }
  for (size_t i = 0; i < mTriChannels.size(); i++) {
    if (mTriChannels[i]->size() != tris)
      errMsg("Tri channel size mismatch");
  }
  if ((int)m1RingLookup.size() != nodes)
    errMsg("1Ring size wrong");
  for (size_t t = 0; t < mTris.size(); t++) {
    if (taintedTris && taintedTris->find(t) != taintedTris->end())
      continue;
    for (int c = 0; c < 3; c++) {
      int corner = t * 3 + c;
      int node = mTris[t].c[c];
      int next = mTris[t].c[(c + 1) % 3];
      int prev = mTris[t].c[(c + 2) % 3];
      int rnext = mCorners[corner].next;
      int rprev = mCorners[corner].prev;
      int ro = mCorners[corner].opposite;
      if (node < 0 || node >= nodes || next < 0 || next >= nodes || prev < 0 || prev >= nodes)
        errMsg("invalid node entry");
      if (mCorners[corner].node != node || mCorners[corner].tri != (int)t)
        errMsg("invalid basic corner entry");
      if (rnext < 0 || rnext >= corners || rprev < 0 || rprev >= corners || ro >= corners)
        errMsg("invalid corner links");
      if (mCorners[rnext].node != next || mCorners[rprev].node != prev)
        errMsg("invalid corner next/prev");
      if (strict && ro < 0)
        errMsg("opposite missing");
      if (mCorners[ro].opposite != corner)
        errMsg("invalid opposite ref");
      set<int> &rnodes = m1RingLookup[node].nodes;
      set<int> &rtris = m1RingLookup[node].tris;
      if (rnodes.find(next) == rnodes.end() || rnodes.find(prev) == rnodes.end()) {
        debMsg("Tri " << t << " " << node << " " << next << " " << prev, 1);
        for (set<int>::iterator it = rnodes.begin(); it != rnodes.end(); ++it)
          debMsg(*it, 1);
        errMsg("node missing in 1ring");
      }
      if (rtris.find(t) == rtris.end()) {
        debMsg("Tri " << t << " " << node, 1);
        errMsg("tri missing in 1ring");
      }
    }
  }
  for (int n = 0; n < nodes; n++) {
    bool docheck = true;
    if (deletedNodes)
      for (size_t e = 0; e < deletedNodes->size(); e++)
        if ((*deletedNodes)[e] == n)
          docheck = false;
    ;

    if (docheck) {
      set<int> &sn = m1RingLookup[n].nodes;
      set<int> &st = m1RingLookup[n].tris;
      set<int> sn2;

      for (set<int>::iterator it = st.begin(); it != st.end(); ++it) {
        bool found = false;
        for (int c = 0; c < 3; c++) {
          if (mTris[*it].c[c] == n)
            found = true;
          else
            sn2.insert(mTris[*it].c[c]);
        }
        if (!found) {
          cout << *it << " " << n << endl;
          for (int c = 0; c < 3; c++)
            cout << mTris[*it].c[c] << endl;
          errMsg("invalid triangle in 1ring");
        }
        if (taintedTris && taintedTris->find(*it) != taintedTris->end()) {
          cout << *it << endl;
          errMsg("tainted tri still is use");
        }
      }
      if (sn.size() != sn2.size())
        errMsg("invalid nodes in 1ring");
      for (set<int>::iterator it = sn.begin(), it2 = sn2.begin(); it != sn.end(); ++it, ++it2) {
        if (*it != *it2) {
          cout << "Node " << n << ": " << *it << " vs " << *it2 << endl;
          errMsg("node ring mismatch");
        }
      }
    }
  }
}

//*****************************************************************************
// rasterization

void meshSDF(Mesh &mesh, LevelsetGrid &levelset, Real sigma, Real cutoff = 0.);

//! helper vec3 array container
struct CVec3Ptr {
  Real *x, *y, *z;
  inline Vec3 get(int i) const
  {
    return Vec3(x[i], y[i], z[i]);
  };
  inline void set(int i, const Vec3 &v)
  {
    x[i] = v.x;
    y[i] = v.y;
    z[i] = v.z;
  };
};
//! helper vec3 array, for CUDA compatibility, remove at some point
struct CVec3Array {
  CVec3Array(int sz)
  {
    x.resize(sz);
    y.resize(sz);
    z.resize(sz);
  }
  CVec3Array(const std::vector<Vec3> &v)
  {
    x.resize(v.size());
    y.resize(v.size());
    z.resize(v.size());
    for (size_t i = 0; i < v.size(); i++) {
      x[i] = v[i].x;
      y[i] = v[i].y;
      z[i] = v[i].z;
    }
  }
  CVec3Ptr data()
  {
    CVec3Ptr a = {x.data(), y.data(), z.data()};
    return a;
  }
  inline const Vec3 operator[](int idx) const
  {
    return Vec3((Real)x[idx], (Real)y[idx], (Real)z[idx]);
  }
  inline void set(int idx, const Vec3 &v)
  {
    x[idx] = v.x;
    y[idx] = v.y;
    z[idx] = v.z;
  }
  inline int size()
  {
    return x.size();
  }
  std::vector<Real> x, y, z;
};

// void SDFKernel(const int* partStart, const int* partLen, CVec3Ptr pos, CVec3Ptr normal, Real*
// sdf, Vec3i gridRes, int intRadius, Real safeRadius2, Real cutoff2, Real isigma2);
//! helper for rasterization
static void SDFKernel(Grid<int> &partStart,
                      Grid<int> &partLen,
                      CVec3Ptr pos,
                      CVec3Ptr normal,
                      LevelsetGrid &sdf,
                      Vec3i gridRes,
                      int intRadius,
                      Real safeRadius2,
                      Real cutoff2,
                      Real isigma2)
{
  for (int cnt_x(0); cnt_x < gridRes[0]; ++cnt_x) {
    for (int cnt_y(0); cnt_y < gridRes[1]; ++cnt_y) {
      for (int cnt_z(0); cnt_z < gridRes[2]; ++cnt_z) {
        // cell index, center
        Vec3i cell = Vec3i(cnt_x, cnt_y, cnt_z);
        if (cell.x >= gridRes.x || cell.y >= gridRes.y || cell.z >= gridRes.z)
          return;
        Vec3 cpos = Vec3(cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f);
        Real sum = 0.0f;
        Real dist = 0.0f;

        // query cells within block radius
        Vec3i minBlock = Vec3i(
            max(cell.x - intRadius, 0), max(cell.y - intRadius, 0), max(cell.z - intRadius, 0));
        Vec3i maxBlock = Vec3i(min(cell.x + intRadius, gridRes.x - 1),
                               min(cell.y + intRadius, gridRes.y - 1),
                               min(cell.z + intRadius, gridRes.z - 1));
        for (int i = minBlock.x; i <= maxBlock.x; i++)
          for (int j = minBlock.y; j <= maxBlock.y; j++)
            for (int k = minBlock.z; k <= maxBlock.z; k++) {
              // test if block is within radius
              Vec3 d = Vec3(cell.x - i, cell.y - j, cell.z - k);
              Real normSqr = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
              if (normSqr > safeRadius2)
                continue;

              // find source cell, and divide it into thread blocks
              int block = i + gridRes.x * (j + gridRes.y * k);
              int slen = partLen[block];
              if (slen == 0)
                continue;
              int start = partStart[block];

              // process sources
              for (int s = 0; s < slen; s++) {

                // actual sdf kernel
                Vec3 r = cpos - pos.get(start + s);
                Real normSqr = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
                Real r2 = normSqr;
                if (r2 < cutoff2) {
                  Real w = expf(-r2 * isigma2);
                  sum += w;
                  dist += dot(normal.get(start + s), r) * w;
                }
              }
            }
        // writeback
        if (sum > 0.0f) {
          // sdf[cell.x + gridRes.x * (cell.y + gridRes.y * cell.z)] = dist / sum;
          sdf(cell.x, cell.y, cell.z) = dist / sum;
        }
      }
    }
  }
}

static inline IndexInt _cIndex(const Vec3 &pos, const Vec3i &s)
{
  Vec3i p = toVec3i(pos);
  if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= s.x || p.y >= s.y || p.z >= s.z)
    return -1;
  return p.x + s.x * (p.y + s.y * p.z);
}

//! Kernel: Apply a shape to a grid, setting value inside

template<class T> struct ApplyMeshToGrid : public KernelBase {
  ApplyMeshToGrid(Grid<T> *grid, Grid<Real> &sdf, T value, FlagGrid *respectFlags)
      : KernelBase(grid, 0), grid(grid), sdf(sdf), value(value), respectFlags(respectFlags)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, Grid<T> *grid, Grid<Real> &sdf, T value, FlagGrid *respectFlags) const
  {
    if (respectFlags && respectFlags->isObstacle(i, j, k))
      return;
    if (sdf(i, j, k) < 0) {
      (*grid)(i, j, k) = value;
    }
  }
  inline Grid<T> *getArg0()
  {
    return grid;
  }
  typedef Grid<T> type0;
  inline Grid<Real> &getArg1()
  {
    return sdf;
  }
  typedef Grid<Real> type1;
  inline T &getArg2()
  {
    return value;
  }
  typedef T type2;
  inline FlagGrid *getArg3()
  {
    return respectFlags;
  }
  typedef FlagGrid type3;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMeshToGrid ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, grid, sdf, value, respectFlags);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, sdf, value, respectFlags);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> *grid;
  Grid<Real> &sdf;
  T value;
  FlagGrid *respectFlags;
};

void Mesh::applyMeshToGrid(GridBase *grid, FlagGrid *respectFlags, Real cutoff, Real meshSigma)
{
  FluidSolver dummy(grid->getSize());
  LevelsetGrid mesh_sdf(&dummy, false);
  meshSDF(*this, mesh_sdf, meshSigma, cutoff);  // meshSigma=2 fixed here

#if NOPYTHON != 1
  if (grid->getType() & GridBase::TypeInt)
    ApplyMeshToGrid<int>((Grid<int> *)grid, mesh_sdf, _args.get<int>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeReal)
    ApplyMeshToGrid<Real>((Grid<Real> *)grid, mesh_sdf, _args.get<Real>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeVec3)
    ApplyMeshToGrid<Vec3>((Grid<Vec3> *)grid, mesh_sdf, _args.get<Vec3>("value"), respectFlags);
  else
    errMsg("Shape::applyToGrid(): unknown grid type");
#else
  errMsg("Not yet supported...");
#endif
}

void Mesh::computeLevelset(LevelsetGrid &levelset, Real sigma, Real cutoff)
{
  meshSDF(*this, levelset, sigma, cutoff);
}

LevelsetGrid Mesh::getLevelset(Real sigma, Real cutoff)
{
  LevelsetGrid phi(getParent());
  meshSDF(*this, phi, sigma, cutoff);
  return phi;
}

void meshSDF(Mesh &mesh, LevelsetGrid &levelset, Real sigma, Real cutoff)
{
  if (cutoff < 0)
    cutoff = 2 * sigma;
  Real maxEdgeLength = 0.75;
  Real numSamplesPerCell = 0.75;

  Vec3i gridRes = levelset.getSize();
  Vec3 mult = toVec3(gridRes) / toVec3(mesh.getParent()->getGridSize());

  // prepare center values
  std::vector<Vec3> center;
  std::vector<Vec3> normals;
  short bigEdges(0);
  std::vector<Vec3> samplePoints;
  for (int i = 0; i < mesh.numTris(); i++) {
    center.push_back(Vec3(mesh.getFaceCenter(i) * mult));
    normals.push_back(mesh.getFaceNormal(i));
    // count big, stretched edges
    bigEdges = 0;
    for (short edge(0); edge < 3; ++edge) {
      if (norm(mesh.getEdge(i, edge)) > maxEdgeLength) {
        bigEdges += 1 << edge;
      }
    }
    if (bigEdges > 0) {
      samplePoints.clear();
      short iterA, pointA, iterB, pointB;
      int numSamples0 = norm(mesh.getEdge(i, 1)) * numSamplesPerCell;
      int numSamples1 = norm(mesh.getEdge(i, 2)) * numSamplesPerCell;
      int numSamples2 = norm(mesh.getEdge(i, 0)) * numSamplesPerCell;
      if (!(bigEdges & (1 << 0))) {
        // loop through 0,1
        iterA = numSamples1;
        pointA = 0;
        iterB = numSamples2;
        pointB = 1;
      }
      else if (!(bigEdges & (1 << 1))) {
        // loop through 1,2
        iterA = numSamples2;
        pointA = 1;
        iterB = numSamples0;
        pointB = 2;
      }
      else {
        // loop through 2,0
        iterA = numSamples0;
        pointA = 2;
        iterB = numSamples1;
        pointB = 0;
      }

      Real u(0.), v(0.), w(0.);  // barycentric uvw coords
      Vec3 samplePoint, normal;
      for (int sample0(0); sample0 < iterA; ++sample0) {
        u = Real(1. * sample0 / iterA);
        for (int sample1(0); sample1 < iterB; ++sample1) {
          v = Real(1. * sample1 / iterB);
          w = 1 - u - v;
          if (w < 0.)
            continue;
          samplePoint = mesh.getNode(i, pointA) * mult * u + mesh.getNode(i, pointB) * mult * v +
                        mesh.getNode(i, (3 - pointA - pointB)) * mult * w;
          samplePoints.push_back(samplePoint);
          normal = mesh.getFaceNormal(i);
          normals.push_back(normal);
        }
      }
      center.insert(center.end(), samplePoints.begin(), samplePoints.end());
    }
  }

  // prepare grid
  levelset.setConst(-cutoff);

  // 1. count sources per cell
  Grid<int> srcPerCell(levelset.getParent());
  for (size_t i = 0; i < center.size(); i++) {
    IndexInt idx = _cIndex(center[i], gridRes);
    if (idx >= 0)
      srcPerCell[idx]++;
  }

  // 2. create start index lookup
  Grid<int> srcCellStart(levelset.getParent());
  int cnt = 0;
  FOR_IJK(srcCellStart)
  {
    IndexInt idx = srcCellStart.index(i, j, k);
    srcCellStart[idx] = cnt;
    cnt += srcPerCell[idx];
  }

  // 3. reorder nodes
  CVec3Array reorderPos(center.size());
  CVec3Array reorderNormal(center.size());
  {
    Grid<int> curSrcCell(levelset.getParent());
    for (int i = 0; i < (int)center.size(); i++) {
      IndexInt idx = _cIndex(center[i], gridRes);
      if (idx < 0)
        continue;
      IndexInt idx2 = srcCellStart[idx] + curSrcCell[idx];
      reorderPos.set(idx2, center[i]);
      reorderNormal.set(idx2, normals[i]);
      curSrcCell[idx]++;
    }
  }

  // construct parameters
  Real safeRadius = cutoff + sqrt(3.0) * 0.5;
  Real safeRadius2 = safeRadius * safeRadius;
  Real cutoff2 = cutoff * cutoff;
  Real isigma2 = 1.0 / (sigma * sigma);
  int intRadius = (int)(cutoff + 0.5);

  SDFKernel(srcCellStart,
            srcPerCell,
            reorderPos.data(),
            reorderNormal.data(),
            levelset,
            gridRes,
            intRadius,
            safeRadius2,
            cutoff2,
            isigma2);

  // floodfill outside
  std::stack<Vec3i> outside;
  FOR_IJK(levelset)
  {
    if (levelset(i, j, k) >= cutoff - 1.0f)
      outside.push(Vec3i(i, j, k));
  }
  while (!outside.empty()) {
    Vec3i c = outside.top();
    outside.pop();
    levelset(c) = cutoff;
    if (c.x > 0 && levelset(c.x - 1, c.y, c.z) < 0)
      outside.push(Vec3i(c.x - 1, c.y, c.z));
    if (c.y > 0 && levelset(c.x, c.y - 1, c.z) < 0)
      outside.push(Vec3i(c.x, c.y - 1, c.z));
    if (c.z > 0 && levelset(c.x, c.y, c.z - 1) < 0)
      outside.push(Vec3i(c.x, c.y, c.z - 1));
    if (c.x < levelset.getSizeX() - 1 && levelset(c.x + 1, c.y, c.z) < 0)
      outside.push(Vec3i(c.x + 1, c.y, c.z));
    if (c.y < levelset.getSizeY() - 1 && levelset(c.x, c.y + 1, c.z) < 0)
      outside.push(Vec3i(c.x, c.y + 1, c.z));
    if (c.z < levelset.getSizeZ() - 1 && levelset(c.x, c.y, c.z + 1) < 0)
      outside.push(Vec3i(c.x, c.y, c.z + 1));
  };
}

// Blender data pointer accessors
std::string Mesh::getNodesDataPointer()
{
  std::ostringstream out;
  out << &mNodes;
  return out.str();
}
std::string Mesh::getTrisDataPointer()
{
  std::ostringstream out;
  out << &mTris;
  return out.str();
}

// mesh data

MeshDataBase::MeshDataBase(FluidSolver *parent) : PbClass(parent), mMesh(NULL)
{
}

MeshDataBase::~MeshDataBase()
{
  // notify parent of deletion
  if (mMesh)
    mMesh->deregister(this);
}

// actual data implementation

template<class T>
MeshDataImpl<T>::MeshDataImpl(FluidSolver *parent)
    : MeshDataBase(parent), mpGridSource(NULL), mGridSourceMAC(false)
{
}

template<class T>
MeshDataImpl<T>::MeshDataImpl(FluidSolver *parent, MeshDataImpl<T> *other)
    : MeshDataBase(parent), mpGridSource(NULL), mGridSourceMAC(false)
{
  this->mData = other->mData;
  setName(other->getName());
}

template<class T> MeshDataImpl<T>::~MeshDataImpl()
{
}

template<class T> IndexInt MeshDataImpl<T>::getSizeSlow() const
{
  return mData.size();
}
template<class T> void MeshDataImpl<T>::addEntry()
{
  // add zero'ed entry
  T tmp = T(0.);
  // for debugging, force init:
  // tmp = T(0.02 * mData.size()); // increasing
  // tmp = T(1.); // constant 1
  return mData.push_back(tmp);
}
template<class T> void MeshDataImpl<T>::resize(IndexInt s)
{
  mData.resize(s);
}
template<class T> void MeshDataImpl<T>::copyValueSlow(IndexInt from, IndexInt to)
{
  this->copyValue(from, to);
}
template<class T> MeshDataBase *MeshDataImpl<T>::clone()
{
  MeshDataImpl<T> *npd = new MeshDataImpl<T>(getParent(), this);
  return npd;
}

template<class T> void MeshDataImpl<T>::setSource(Grid<T> *grid, bool isMAC)
{
  mpGridSource = grid;
  mGridSourceMAC = isMAC;
  if (grid && isMAC)
    assertMsg(grid->getType() & GridBase::TypeMAC, "Given grid is not a valid MAC grid");
}

template<class T> void MeshDataImpl<T>::initNewValue(IndexInt idx, Vec3 pos)
{
  if (!mpGridSource)
    mData[idx] = 0;
  else {
    mData[idx] = mpGridSource->getInterpolated(pos);
  }
}

// special handling needed for velocities
template<> void MeshDataImpl<Vec3>::initNewValue(IndexInt idx, Vec3 pos)
{
  if (!mpGridSource)
    mData[idx] = 0;
  else {
    if (!mGridSourceMAC)
      mData[idx] = mpGridSource->getInterpolated(pos);
    else
      mData[idx] = ((MACGrid *)mpGridSource)->getInterpolated(pos);
  }
}

//! update additional mesh data
void Mesh::updateDataFields()
{
  for (size_t i = 0; i < mNodes.size(); ++i) {
    Vec3 pos = mNodes[i].pos;
    for (IndexInt md = 0; md < (IndexInt)mMdataReal.size(); ++md)
      mMdataReal[md]->initNewValue(i, pos);
    for (IndexInt md = 0; md < (IndexInt)mMdataVec3.size(); ++md)
      mMdataVec3[md]->initNewValue(i, pos);
    for (IndexInt md = 0; md < (IndexInt)mMdataInt.size(); ++md)
      mMdataInt[md]->initNewValue(i, pos);
  }
}

template<typename T> int MeshDataImpl<T>::load(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".uni")
    return readMdataUni<T>(name, this);
  else if (ext == ".raw")  // raw = uni for now
    return readMdataUni<T>(name, this);
  else
    errMsg("mesh data '" + name + "' filetype not supported for loading");
  return 0;
}

template<typename T> int MeshDataImpl<T>::save(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".uni")
    return writeMdataUni<T>(name, this);
  else if (ext == ".raw")  // raw = uni for now
    return writeMdataUni<T>(name, this);
  else
    errMsg("mesh data '" + name + "' filetype not supported for saving");
  return 0;
}

// specializations

template<> MeshDataBase::MdataType MeshDataImpl<Real>::getType() const
{
  return MeshDataBase::TypeReal;
}
template<> MeshDataBase::MdataType MeshDataImpl<int>::getType() const
{
  return MeshDataBase::TypeInt;
}
template<> MeshDataBase::MdataType MeshDataImpl<Vec3>::getType() const
{
  return MeshDataBase::TypeVec3;
}

template<class T> struct knSetMdataConst : public KernelBase {
  knSetMdataConst(MeshDataImpl<T> &mdata, T value)
      : KernelBase(mdata.size()), mdata(mdata), value(value)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &mdata, T value) const
  {
    mdata[idx] = value;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return mdata;
  }
  typedef MeshDataImpl<T> type0;
  inline T &getArg1()
  {
    return value;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knSetMdataConst ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, mdata, value);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &mdata;
  T value;
};

template<class T, class S> struct knMdataSet : public KernelBase {
  knMdataSet(MeshDataImpl<T> &me, const MeshDataImpl<S> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<S> &other) const
  {
    me[idx] += other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<S> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSet ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<S> &other;
};
template<class T, class S> struct knMdataAdd : public KernelBase {
  knMdataAdd(MeshDataImpl<T> &me, const MeshDataImpl<S> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<S> &other) const
  {
    me[idx] += other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<S> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataAdd ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<S> &other;
};
template<class T, class S> struct knMdataSub : public KernelBase {
  knMdataSub(MeshDataImpl<T> &me, const MeshDataImpl<S> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<S> &other) const
  {
    me[idx] -= other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<S> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSub ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<S> &other;
};
template<class T, class S> struct knMdataMult : public KernelBase {
  knMdataMult(MeshDataImpl<T> &me, const MeshDataImpl<S> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<S> &other) const
  {
    me[idx] *= other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<S> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataMult ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<S> &other;
};
template<class T, class S> struct knMdataDiv : public KernelBase {
  knMdataDiv(MeshDataImpl<T> &me, const MeshDataImpl<S> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<S> &other) const
  {
    me[idx] /= other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<S> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataDiv ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<S> &other;
};

template<class T, class S> struct knMdataSetScalar : public KernelBase {
  knMdataSetScalar(MeshDataImpl<T> &me, const S &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const S &other) const
  {
    me[idx] = other;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSetScalar ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const S &other;
};
template<class T, class S> struct knMdataAddScalar : public KernelBase {
  knMdataAddScalar(MeshDataImpl<T> &me, const S &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const S &other) const
  {
    me[idx] += other;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataAddScalar ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const S &other;
};
template<class T, class S> struct knMdataMultScalar : public KernelBase {
  knMdataMultScalar(MeshDataImpl<T> &me, const S &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const S &other) const
  {
    me[idx] *= other;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataMultScalar ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const S &other;
};
template<class T, class S> struct knMdataScaledAdd : public KernelBase {
  knMdataScaledAdd(MeshDataImpl<T> &me, const MeshDataImpl<T> &other, const S &factor)
      : KernelBase(me.size()), me(me), other(other), factor(factor)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 MeshDataImpl<T> &me,
                 const MeshDataImpl<T> &other,
                 const S &factor) const
  {
    me[idx] += factor * other[idx];
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<T> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<T> type1;
  inline const S &getArg2()
  {
    return factor;
  }
  typedef S type2;
  void runMessage()
  {
    debMsg("Executing kernel knMdataScaledAdd ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other, factor);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<T> &other;
  const S &factor;
};

template<class T> struct knMdataSafeDiv : public KernelBase {
  knMdataSafeDiv(MeshDataImpl<T> &me, const MeshDataImpl<T> &other)
      : KernelBase(me.size()), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const MeshDataImpl<T> &other) const
  {
    me[idx] = safeDivide(me[idx], other[idx]);
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<T> &getArg1()
  {
    return other;
  }
  typedef MeshDataImpl<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSafeDiv ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const MeshDataImpl<T> &other;
};
template<class T> struct knMdataSetConst : public KernelBase {
  knMdataSetConst(MeshDataImpl<T> &mdata, T value)
      : KernelBase(mdata.size()), mdata(mdata), value(value)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &mdata, T value) const
  {
    mdata[idx] = value;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return mdata;
  }
  typedef MeshDataImpl<T> type0;
  inline T &getArg1()
  {
    return value;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSetConst ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, mdata, value);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &mdata;
  T value;
};

template<class T> struct knMdataClamp : public KernelBase {
  knMdataClamp(MeshDataImpl<T> &me, T min, T max)
      : KernelBase(me.size()), me(me), min(min), max(max)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, T min, T max) const
  {
    me[idx] = clamp(me[idx], min, max);
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline T &getArg1()
  {
    return min;
  }
  typedef T type1;
  inline T &getArg2()
  {
    return max;
  }
  typedef T type2;
  void runMessage()
  {
    debMsg("Executing kernel knMdataClamp ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, min, max);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  T min;
  T max;
};
template<class T> struct knMdataClampMin : public KernelBase {
  knMdataClampMin(MeshDataImpl<T> &me, const T vmin) : KernelBase(me.size()), me(me), vmin(vmin)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const T vmin) const
  {
    me[idx] = std::max(vmin, me[idx]);
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const T &getArg1()
  {
    return vmin;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataClampMin ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, vmin);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const T vmin;
};
template<class T> struct knMdataClampMax : public KernelBase {
  knMdataClampMax(MeshDataImpl<T> &me, const T vmax) : KernelBase(me.size()), me(me), vmax(vmax)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<T> &me, const T vmax) const
  {
    me[idx] = std::min(vmax, me[idx]);
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const T &getArg1()
  {
    return vmax;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataClampMax ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, vmax);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const T vmax;
};
struct knMdataClampMinVec3 : public KernelBase {
  knMdataClampMinVec3(MeshDataImpl<Vec3> &me, const Real vmin)
      : KernelBase(me.size()), me(me), vmin(vmin)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<Vec3> &me, const Real vmin) const
  {
    me[idx].x = std::max(vmin, me[idx].x);
    me[idx].y = std::max(vmin, me[idx].y);
    me[idx].z = std::max(vmin, me[idx].z);
  }
  inline MeshDataImpl<Vec3> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<Vec3> type0;
  inline const Real &getArg1()
  {
    return vmin;
  }
  typedef Real type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataClampMinVec3 ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, vmin);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<Vec3> &me;
  const Real vmin;
};
struct knMdataClampMaxVec3 : public KernelBase {
  knMdataClampMaxVec3(MeshDataImpl<Vec3> &me, const Real vmax)
      : KernelBase(me.size()), me(me), vmax(vmax)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, MeshDataImpl<Vec3> &me, const Real vmax) const
  {
    me[idx].x = std::min(vmax, me[idx].x);
    me[idx].y = std::min(vmax, me[idx].y);
    me[idx].z = std::min(vmax, me[idx].z);
  }
  inline MeshDataImpl<Vec3> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<Vec3> type0;
  inline const Real &getArg1()
  {
    return vmax;
  }
  typedef Real type1;
  void runMessage()
  {
    debMsg("Executing kernel knMdataClampMaxVec3 ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, vmax);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<Vec3> &me;
  const Real vmax;
};

// python operators

template<typename T> MeshDataImpl<T> &MeshDataImpl<T>::copyFrom(const MeshDataImpl<T> &a)
{
  assertMsg(a.mData.size() == mData.size(),
            "different mdata size " << a.mData.size() << " vs " << this->mData.size());
  memcpy(&mData[0], &a.mData[0], sizeof(T) * mData.size());
  return *this;
}

template<typename T> void MeshDataImpl<T>::setConst(T s)
{
  knMdataSetScalar<T, T> op(*this, s);
}

template<typename T> void MeshDataImpl<T>::setConstRange(T s, const int begin, const int end)
{
  for (int i = begin; i < end; ++i)
    (*this)[i] = s;
}

// special set by flag
template<class T, class S> struct knMdataSetScalarIntFlag : public KernelBase {
  knMdataSetScalarIntFlag(MeshDataImpl<T> &me,
                          const S &other,
                          const MeshDataImpl<int> &t,
                          const int itype)
      : KernelBase(me.size()), me(me), other(other), t(t), itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 MeshDataImpl<T> &me,
                 const S &other,
                 const MeshDataImpl<int> &t,
                 const int itype) const
  {
    if (t[idx] & itype)
      me[idx] = other;
  }
  inline MeshDataImpl<T> &getArg0()
  {
    return me;
  }
  typedef MeshDataImpl<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  inline const MeshDataImpl<int> &getArg2()
  {
    return t;
  }
  typedef MeshDataImpl<int> type2;
  inline const int &getArg3()
  {
    return itype;
  }
  typedef int type3;
  void runMessage()
  {
    debMsg("Executing kernel knMdataSetScalarIntFlag ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other, t, itype);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  MeshDataImpl<T> &me;
  const S &other;
  const MeshDataImpl<int> &t;
  const int itype;
};
template<typename T>
void MeshDataImpl<T>::setConstIntFlag(T s, const MeshDataImpl<int> &t, const int itype)
{
  knMdataSetScalarIntFlag<T, T> op(*this, s, t, itype);
}

template<typename T> void MeshDataImpl<T>::add(const MeshDataImpl<T> &a)
{
  knMdataAdd<T, T> op(*this, a);
}
template<typename T> void MeshDataImpl<T>::sub(const MeshDataImpl<T> &a)
{
  knMdataSub<T, T> op(*this, a);
}

template<typename T> void MeshDataImpl<T>::addConst(T s)
{
  knMdataAddScalar<T, T> op(*this, s);
}

template<typename T> void MeshDataImpl<T>::addScaled(const MeshDataImpl<T> &a, const T &factor)
{
  knMdataScaledAdd<T, T> op(*this, a, factor);
}

template<typename T> void MeshDataImpl<T>::mult(const MeshDataImpl<T> &a)
{
  knMdataMult<T, T> op(*this, a);
}

template<typename T> void MeshDataImpl<T>::safeDiv(const MeshDataImpl<T> &a)
{
  knMdataSafeDiv<T> op(*this, a);
}

template<typename T> void MeshDataImpl<T>::multConst(T s)
{
  knMdataMultScalar<T, T> op(*this, s);
}

template<typename T> void MeshDataImpl<T>::clamp(Real vmin, Real vmax)
{
  knMdataClamp<T> op(*this, vmin, vmax);
}

template<typename T> void MeshDataImpl<T>::clampMin(Real vmin)
{
  knMdataClampMin<T> op(*this, vmin);
}
template<typename T> void MeshDataImpl<T>::clampMax(Real vmax)
{
  knMdataClampMax<T> op(*this, vmax);
}

template<> void MeshDataImpl<Vec3>::clampMin(Real vmin)
{
  knMdataClampMinVec3 op(*this, vmin);
}
template<> void MeshDataImpl<Vec3>::clampMax(Real vmax)
{
  knMdataClampMaxVec3 op(*this, vmax);
}

template<typename T> struct KnPtsSum : public KernelBase {
  KnPtsSum(const MeshDataImpl<T> &val, const MeshDataImpl<int> *t, const int itype)
      : KernelBase(val.size()), val(val), t(t), itype(itype), result(T(0.))
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const MeshDataImpl<T> &val,
                 const MeshDataImpl<int> *t,
                 const int itype,
                 T &result)
  {
    if (t && !((*t)[idx] & itype))
      return;
    result += val[idx];
  }
  inline operator T()
  {
    return result;
  }
  inline T &getRet()
  {
    return result;
  }
  inline const MeshDataImpl<T> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<T> type0;
  inline const MeshDataImpl<int> *getArg1()
  {
    return t;
  }
  typedef MeshDataImpl<int> type1;
  inline const int &getArg2()
  {
    return itype;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel KnPtsSum ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, t, itype, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  KnPtsSum(KnPtsSum &o, tbb::split)
      : KernelBase(o), val(o.val), t(o.t), itype(o.itype), result(T(0.))
  {
  }
  void join(const KnPtsSum &o)
  {
    result += o.result;
  }
  const MeshDataImpl<T> &val;
  const MeshDataImpl<int> *t;
  const int itype;
  T result;
};
template<typename T> struct KnPtsSumSquare : public KernelBase {
  KnPtsSumSquare(const MeshDataImpl<T> &val) : KernelBase(val.size()), val(val), result(0.)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<T> &val, Real &result)
  {
    result += normSquare(val[idx]);
  }
  inline operator Real()
  {
    return result;
  }
  inline Real &getRet()
  {
    return result;
  }
  inline const MeshDataImpl<T> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<T> type0;
  void runMessage()
  {
    debMsg("Executing kernel KnPtsSumSquare ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  KnPtsSumSquare(KnPtsSumSquare &o, tbb::split) : KernelBase(o), val(o.val), result(0.)
  {
  }
  void join(const KnPtsSumSquare &o)
  {
    result += o.result;
  }
  const MeshDataImpl<T> &val;
  Real result;
};
template<typename T> struct KnPtsSumMagnitude : public KernelBase {
  KnPtsSumMagnitude(const MeshDataImpl<T> &val) : KernelBase(val.size()), val(val), result(0.)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<T> &val, Real &result)
  {
    result += norm(val[idx]);
  }
  inline operator Real()
  {
    return result;
  }
  inline Real &getRet()
  {
    return result;
  }
  inline const MeshDataImpl<T> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<T> type0;
  void runMessage()
  {
    debMsg("Executing kernel KnPtsSumMagnitude ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  KnPtsSumMagnitude(KnPtsSumMagnitude &o, tbb::split) : KernelBase(o), val(o.val), result(0.)
  {
  }
  void join(const KnPtsSumMagnitude &o)
  {
    result += o.result;
  }
  const MeshDataImpl<T> &val;
  Real result;
};

template<typename T> T MeshDataImpl<T>::sum(const MeshDataImpl<int> *t, const int itype) const
{
  return KnPtsSum<T>(*this, t, itype);
}
template<typename T> Real MeshDataImpl<T>::sumSquare() const
{
  return KnPtsSumSquare<T>(*this);
}
template<typename T> Real MeshDataImpl<T>::sumMagnitude() const
{
  return KnPtsSumMagnitude<T>(*this);
}

template<typename T>

struct CompMdata_Min : public KernelBase {
  CompMdata_Min(const MeshDataImpl<T> &val)
      : KernelBase(val.size()), val(val), minVal(std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<T> &val, Real &minVal)
  {
    if (val[idx] < minVal)
      minVal = val[idx];
  }
  inline operator Real()
  {
    return minVal;
  }
  inline Real &getRet()
  {
    return minVal;
  }
  inline const MeshDataImpl<T> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<T> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMdata_Min ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, minVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMdata_Min(CompMdata_Min &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMdata_Min &o)
  {
    minVal = min(minVal, o.minVal);
  }
  const MeshDataImpl<T> &val;
  Real minVal;
};

template<typename T>

struct CompMdata_Max : public KernelBase {
  CompMdata_Max(const MeshDataImpl<T> &val)
      : KernelBase(val.size()), val(val), maxVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<T> &val, Real &maxVal)
  {
    if (val[idx] > maxVal)
      maxVal = val[idx];
  }
  inline operator Real()
  {
    return maxVal;
  }
  inline Real &getRet()
  {
    return maxVal;
  }
  inline const MeshDataImpl<T> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<T> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMdata_Max ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, maxVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMdata_Max(CompMdata_Max &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMdata_Max &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  const MeshDataImpl<T> &val;
  Real maxVal;
};

template<typename T> Real MeshDataImpl<T>::getMin()
{
  return CompMdata_Min<T>(*this);
}

template<typename T> Real MeshDataImpl<T>::getMaxAbs()
{
  Real amin = CompMdata_Min<T>(*this);
  Real amax = CompMdata_Max<T>(*this);
  return max(fabs(amin), fabs(amax));
}

template<typename T> Real MeshDataImpl<T>::getMax()
{
  return CompMdata_Max<T>(*this);
}

template<typename T>
void MeshDataImpl<T>::printMdata(IndexInt start, IndexInt stop, bool printIndex)
{
  std::ostringstream sstr;
  IndexInt s = (start > 0 ? start : 0);
  IndexInt e = (stop > 0 ? stop : (IndexInt)mData.size());
  s = Manta::clamp(s, (IndexInt)0, (IndexInt)mData.size());
  e = Manta::clamp(e, (IndexInt)0, (IndexInt)mData.size());

  for (IndexInt i = s; i < e; ++i) {
    if (printIndex)
      sstr << i << ": ";
    sstr << mData[i] << " "
         << "\n";
  }
  debMsg(sstr.str(), 1);
}
template<class T> std::string MeshDataImpl<T>::getDataPointer()
{
  std::ostringstream out;
  out << &mData;
  return out.str();
}

// specials for vec3
// work on length values, ie, always positive (in contrast to scalar versions above)

struct CompMdata_MinVec3 : public KernelBase {
  CompMdata_MinVec3(const MeshDataImpl<Vec3> &val)
      : KernelBase(val.size()), val(val), minVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<Vec3> &val, Real &minVal)
  {
    const Real s = normSquare(val[idx]);
    if (s < minVal)
      minVal = s;
  }
  inline operator Real()
  {
    return minVal;
  }
  inline Real &getRet()
  {
    return minVal;
  }
  inline const MeshDataImpl<Vec3> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<Vec3> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMdata_MinVec3 ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, minVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMdata_MinVec3(CompMdata_MinVec3 &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMdata_MinVec3 &o)
  {
    minVal = min(minVal, o.minVal);
  }
  const MeshDataImpl<Vec3> &val;
  Real minVal;
};

struct CompMdata_MaxVec3 : public KernelBase {
  CompMdata_MaxVec3(const MeshDataImpl<Vec3> &val)
      : KernelBase(val.size()), val(val), maxVal(-std::numeric_limits<Real>::min())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const MeshDataImpl<Vec3> &val, Real &maxVal)
  {
    const Real s = normSquare(val[idx]);
    if (s > maxVal)
      maxVal = s;
  }
  inline operator Real()
  {
    return maxVal;
  }
  inline Real &getRet()
  {
    return maxVal;
  }
  inline const MeshDataImpl<Vec3> &getArg0()
  {
    return val;
  }
  typedef MeshDataImpl<Vec3> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMdata_MaxVec3 ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, maxVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMdata_MaxVec3(CompMdata_MaxVec3 &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::min())
  {
  }
  void join(const CompMdata_MaxVec3 &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  const MeshDataImpl<Vec3> &val;
  Real maxVal;
};

template<> Real MeshDataImpl<Vec3>::getMin()
{
  return sqrt(CompMdata_MinVec3(*this));
}

template<> Real MeshDataImpl<Vec3>::getMaxAbs()
{
  return sqrt(CompMdata_MaxVec3(*this));  // no minimum necessary here
}

template<> Real MeshDataImpl<Vec3>::getMax()
{
  return sqrt(CompMdata_MaxVec3(*this));
}

// explicit instantiation
template class MeshDataImpl<int>;
template class MeshDataImpl<Real>;
template class MeshDataImpl<Vec3>;

}  // namespace Manta
