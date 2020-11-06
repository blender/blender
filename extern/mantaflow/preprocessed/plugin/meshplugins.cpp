

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
 * Smoothing etc. for meshes
 *
 ******************************************************************************/

/******************************************************************************/
// Copyright note:
//
// These functions (C) Chris Wojtan
// Long-term goal is to unify with his split&merge codebase
//
/******************************************************************************/

#include <queue>
#include <algorithm>
#include "mesh.h"
#include "kernel.h"
#include "edgecollapse.h"
#include <mesh.h>
#include <stack>

using namespace std;

namespace Manta {

//! Mesh smoothing
/*! see Desbrun 99 "Implicit fairing of of irregular meshes using diffusion and curvature flow"*/
void smoothMesh(Mesh &mesh, Real strength, int steps = 1, Real minLength = 1e-5)
{
  const Real dt = mesh.getParent()->getDt();
  const Real str = min(dt * strength, (Real)1);
  mesh.rebuildQuickCheck();

  // calculate original mesh volume
  Vec3 origCM;
  Real origVolume = mesh.computeCenterOfMass(origCM);

  // temp vertices
  const int numCorners = mesh.numTris() * 3;
  const int numNodes = mesh.numNodes();
  vector<Vec3> temp(numNodes);
  vector<bool> visited(numNodes);

  for (int s = 0; s < steps; s++) {
    // reset markers
    for (size_t i = 0; i < visited.size(); i++)
      visited[i] = false;

    for (int c = 0; c < numCorners; c++) {
      const int node = mesh.corners(c).node;
      if (visited[node])
        continue;

      const Vec3 pos = mesh.nodes(node).pos;
      Vec3 dx(0.0);
      Real totalLen = 0;

      // rotate around vertex
      set<int> &ring = mesh.get1Ring(node).nodes;
      for (set<int>::iterator it = ring.begin(); it != ring.end(); it++) {
        Vec3 edge = mesh.nodes(*it).pos - pos;
        Real len = norm(edge);

        if (len > minLength) {
          dx += edge * (1.0 / len);
          totalLen += len;
        }
        else {
          totalLen = 0.0;
          break;
        }
      }
      visited[node] = true;
      temp[node] = pos;
      if (totalLen != 0)
        temp[node] += dx * (str / totalLen);
    }

    // copy back
    for (int n = 0; n < numNodes; n++)
      if (!mesh.isNodeFixed(n))
        mesh.nodes(n).pos = temp[n];
  }

  // calculate new mesh volume
  Vec3 newCM;
  Real newVolume = mesh.computeCenterOfMass(newCM);

  // preserve volume : scale relative to CM
  Real beta;
#if defined(WIN32) || defined(_WIN32)
  beta = pow((Real)std::abs(origVolume / newVolume), (Real)(1. / 3.));
#else
  beta = cbrt(origVolume / newVolume);
#endif

  for (int n = 0; n < numNodes; n++)
    if (!mesh.isNodeFixed(n))
      mesh.nodes(n).pos = origCM + (mesh.nodes(n).pos - newCM) * beta;
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "smoothMesh", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Mesh &mesh = *_args.getPtr<Mesh>("mesh", 0, &_lock);
      Real strength = _args.get<Real>("strength", 1, &_lock);
      int steps = _args.getOpt<int>("steps", 2, 1, &_lock);
      Real minLength = _args.getOpt<Real>("minLength", 3, 1e-5, &_lock);
      _retval = getPyNone();
      smoothMesh(mesh, strength, steps, minLength);
      _args.check();
    }
    pbFinalizePlugin(parent, "smoothMesh", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("smoothMesh", e.what());
    return 0;
  }
}
static const Pb::Register _RP_smoothMesh("", "smoothMesh", _W_0);
extern "C" {
void PbRegister_smoothMesh()
{
  KEEP_UNUSED(_RP_smoothMesh);
}
}

//! Subdivide and edgecollapse to guarantee mesh with edgelengths between
//! min/maxLength and an angle below minAngle
void subdivideMesh(
    Mesh &mesh, Real minAngle, Real minLength, Real maxLength, bool cutTubes = false)
{
  // gather some statistics
  int edgeSubdivs = 0, edgeCollsAngle = 0, edgeCollsLen = 0, edgeKill = 0;
  mesh.rebuildQuickCheck();

  vector<int> deletedNodes;
  map<int, bool> taintedTris;
  priority_queue<pair<Real, int>> pq;

  //////////////////////////////////////////
  // EDGE COLLAPSE                        //
  //    - particles marked for deletation //
  //////////////////////////////////////////

  for (int t = 0; t < mesh.numTris(); t++) {
    if (taintedTris.find(t) != taintedTris.end())
      continue;

    // check if at least 2 nodes are marked for delete
    bool k[3];
    int numKill = 0;
    for (int i = 0; i < 3; i++) {
      k[i] = mesh.nodes(mesh.tris(t).c[i]).flags & Mesh::NfKillme;
      if (k[i])
        numKill++;
    }
    if (numKill < 2)
      continue;

    if (k[0] && k[1])
      CollapseEdge(mesh,
                   t,
                   2,
                   mesh.getEdge(t, 0),
                   mesh.getNode(t, 0),
                   deletedNodes,
                   taintedTris,
                   edgeKill,
                   cutTubes);
    else if (k[1] && k[2])
      CollapseEdge(mesh,
                   t,
                   0,
                   mesh.getEdge(t, 1),
                   mesh.getNode(t, 1),
                   deletedNodes,
                   taintedTris,
                   edgeKill,
                   cutTubes);
    else if (k[2] && k[0])
      CollapseEdge(mesh,
                   t,
                   1,
                   mesh.getEdge(t, 2),
                   mesh.getNode(t, 2),
                   deletedNodes,
                   taintedTris,
                   edgeKill,
                   cutTubes);
  }

  //////////////////////////////////////////
  // EDGE COLLAPSING                      //
  //      - based on small triangle angle //
  //////////////////////////////////////////

  if (minAngle > 0) {
    for (int t = 0; t < mesh.numTris(); t++) {
      // we only want to run through the edge list ONCE.
      // we achieve this in a method very similar to the above subdivision method.

      // if this triangle has already been deleted, ignore it
      if (taintedTris.find(t) != taintedTris.end())
        continue;

      // first we find the angles of this triangle
      Vec3 e0 = mesh.getEdge(t, 0), e1 = mesh.getEdge(t, 1), e2 = mesh.getEdge(t, 2);
      Vec3 ne0 = e0;
      Vec3 ne1 = e1;
      Vec3 ne2 = e2;
      normalize(ne0);
      normalize(ne1);
      normalize(ne2);

      // Real thisArea = sqrMag(cross(-e2,e0));
      // small angle approximation says sin(x) = arcsin(x) = x,
      // arccos(x) = pi/2 - arcsin(x),
      // cos(x) = dot(A,B),
      // so angle is approximately 1 - dot(A,B).
      Real angle[3];
      angle[0] = 1.0 - dot(ne0, -ne2);
      angle[1] = 1.0 - dot(ne1, -ne0);
      angle[2] = 1.0 - dot(ne2, -ne1);
      Real worstAngle = angle[0];
      int which = 0;
      if (angle[1] < worstAngle) {
        worstAngle = angle[1];
        which = 1;
      }
      if (angle[2] < worstAngle) {
        worstAngle = angle[2];
        which = 2;
      }

      // then we see if the angle is too small
      if (worstAngle < minAngle) {
        Vec3 edgevect;
        Vec3 endpoint;
        switch (which) {
          case 0:
            endpoint = mesh.getNode(t, 1);
            edgevect = e1;
            break;
          case 1:
            endpoint = mesh.getNode(t, 2);
            edgevect = e2;
            break;
          case 2:
            endpoint = mesh.getNode(t, 0);
            edgevect = e0;
            break;
          default:
            break;
        }

        CollapseEdge(mesh,
                     t,
                     which,
                     edgevect,
                     endpoint,
                     deletedNodes,
                     taintedTris,
                     edgeCollsAngle,
                     cutTubes);
      }
    }
  }

  //////////////////////
  // EDGE SUBDIVISION //
  //////////////////////

  Real maxLength2 = maxLength * maxLength;
  for (int t = 0; t < mesh.numTris(); t++) {
    // first we find the maximum length edge in this triangle
    Vec3 e0 = mesh.getEdge(t, 0), e1 = mesh.getEdge(t, 1), e2 = mesh.getEdge(t, 2);
    Real d0 = normSquare(e0);
    Real d1 = normSquare(e1);
    Real d2 = normSquare(e2);

    Real longest = max(d0, max(d1, d2));
    if (longest > maxLength2) {
      pq.push(pair<Real, int>(longest, t));
    }
  }
  if (maxLength > 0) {

    while (!pq.empty() && pq.top().first > maxLength2) {
      // we only want to run through the edge list ONCE
      // and we want to subdivide the original edges before we subdivide any newer, shorter edges,
      // so whenever we subdivide, we add the 2 new triangles on the end of the SurfaceTri vector
      // and mark the original subdivided triangles for deletion.
      //  when we are done subdividing, we delete the obsolete triangles

      int triA = pq.top().second;
      pq.pop();

      if (taintedTris.find(triA) != taintedTris.end())
        continue;

      // first we find the maximum length edge in this triangle
      Vec3 e0 = mesh.getEdge(triA, 0), e1 = mesh.getEdge(triA, 1), e2 = mesh.getEdge(triA, 2);
      Real d0 = normSquare(e0);
      Real d1 = normSquare(e1);
      Real d2 = normSquare(e2);

      Vec3 edgevect;
      Vec3 endpoint;
      int which;
      if (d0 > d1) {
        if (d0 > d2) {
          edgevect = e0;
          endpoint = mesh.getNode(triA, 0);
          ;
          which = 2;  // 2 opposite of edge 0-1
        }
        else {
          edgevect = e2;
          endpoint = mesh.getNode(triA, 2);
          which = 1;  // 1 opposite of edge 2-0
        }
      }
      else {
        if (d1 > d2) {
          edgevect = e1;
          endpoint = mesh.getNode(triA, 1);
          which = 0;  // 0 opposite of edge 1-2
        }
        else {
          edgevect = e2;
          endpoint = mesh.getNode(triA, 2);
          which = 1;  // 1 opposite of edge 2-0
        }
      }
      // This edge is too long, so we split it in the middle

      //         *
      //        / \.
      //       /C0 \.
      //      /     \.
      //     /       \.
      //    /    B    \.
      //   /           \.
      //  /C1        C2 \.
      // *---------------*
      //  \C2        C1 /
      //   \           /
      //    \    A    /
      //     \       /
      //      \     /
      //       \C0 /
      //        \ /
      //         *
      //
      //      BECOMES
      //
      //         *
      //        /|\.
      //       / | \.
      //      /C0|C0\.
      //     /   |   \.
      //    / B1 | B2 \.
      //   /     |     \.
      //  /C1  C2|C1 C2 \.
      // *-------*-------*
      //  \C2  C1|C2  C1/
      //   \     |     /
      //    \ A2 | A1 /
      //     \   |   /
      //      \C0|C0/
      //       \ | /
      //        \|/
      //         *

      int triB = -1;
      bool haveB = false;
      Corner ca_old[3], cb_old[3];
      ca_old[0] = mesh.corners(triA, which);
      ca_old[1] = mesh.corners(ca_old[0].next);
      ca_old[2] = mesh.corners(ca_old[0].prev);
      if (ca_old[0].opposite >= 0) {
        cb_old[0] = mesh.corners(ca_old[0].opposite);
        cb_old[1] = mesh.corners(cb_old[0].next);
        cb_old[2] = mesh.corners(cb_old[0].prev);
        triB = cb_old[0].tri;
        haveB = true;
      }
      // else throw Error("nonmanifold");

      // subdivide in the middle of the edge and create new triangles
      Node newNode;
      newNode.flags = 0;

      newNode.pos = endpoint + 0.5 * edgevect;  // fallback: linear average
      // default: use butterfly
      if (haveB)
        newNode.pos = ModifiedButterflySubdivision(mesh, ca_old[0], cb_old[0], newNode.pos);

      // find indices of two points of 'which'-edge
      // merge flags
      int P0 = ca_old[1].node;
      int P1 = ca_old[2].node;
      newNode.flags = mesh.nodes(P0).flags | mesh.nodes(P1).flags;

      Real len0 = norm(mesh.nodes(P0).pos - newNode.pos);
      Real len1 = norm(mesh.nodes(P1).pos - newNode.pos);

      // remove P0/P1 1-ring connection
      mesh.get1Ring(P0).nodes.erase(P1);
      mesh.get1Ring(P1).nodes.erase(P0);
      mesh.get1Ring(P0).tris.erase(triA);
      mesh.get1Ring(P1).tris.erase(triA);
      mesh.get1Ring(ca_old[0].node).tris.erase(triA);
      if (haveB) {
        mesh.get1Ring(P0).tris.erase(triB);
        mesh.get1Ring(P1).tris.erase(triB);
        mesh.get1Ring(cb_old[0].node).tris.erase(triB);
      }

      // init channel properties for new node
      for (int i = 0; i < mesh.numNodeChannels(); i++) {
        mesh.nodeChannel(i)->addInterpol(P0, P1, len0 / (len0 + len1));
      }

      // write to array
      mesh.addTri(Triangle(ca_old[0].node, ca_old[1].node, mesh.numNodes()));
      mesh.addTri(Triangle(ca_old[0].node, mesh.numNodes(), ca_old[2].node));
      if (haveB) {
        mesh.addTri(Triangle(cb_old[0].node, cb_old[1].node, mesh.numNodes()));
        mesh.addTri(Triangle(cb_old[0].node, mesh.numNodes(), cb_old[2].node));
      }
      mesh.addNode(newNode);

      const int nt = haveB ? 4 : 2;
      int triA1 = mesh.numTris() - nt;
      int triA2 = mesh.numTris() - nt + 1;
      int triB1 = 0, triB2 = 0;
      if (haveB) {
        triB1 = mesh.numTris() - nt + 2;
        triB2 = mesh.numTris() - nt + 3;
      }
      mesh.tris(triA1).flags = mesh.tris(triA).flags;
      mesh.tris(triA2).flags = mesh.tris(triA).flags;
      mesh.tris(triB1).flags = mesh.tris(triB).flags;
      mesh.tris(triB2).flags = mesh.tris(triB).flags;

      // connect new triangles to outside triangles,
      // and connect outside triangles to these new ones
      for (int c = 0; c < 3; c++)
        mesh.addCorner(Corner(triA1, mesh.tris(triA1).c[c]));
      for (int c = 0; c < 3; c++)
        mesh.addCorner(Corner(triA2, mesh.tris(triA2).c[c]));
      if (haveB) {
        for (int c = 0; c < 3; c++)
          mesh.addCorner(Corner(triB1, mesh.tris(triB1).c[c]));
        for (int c = 0; c < 3; c++)
          mesh.addCorner(Corner(triB2, mesh.tris(triB2).c[c]));
      }

      int baseIdx = 3 * (mesh.numTris() - nt);
      Corner *cBase = &mesh.corners(baseIdx);

      // set next/prev
      for (int t = 0; t < nt; t++)
        for (int c = 0; c < 3; c++) {
          cBase[t * 3 + c].next = baseIdx + t * 3 + ((c + 1) % 3);
          cBase[t * 3 + c].prev = baseIdx + t * 3 + ((c + 2) % 3);
        }

      // set opposites
      // A1
      cBase[0].opposite = haveB ? (baseIdx + 9) : -1;
      cBase[1].opposite = baseIdx + 5;
      cBase[2].opposite = -1;
      if (ca_old[2].opposite >= 0) {
        cBase[2].opposite = ca_old[2].opposite;
        mesh.corners(cBase[2].opposite).opposite = baseIdx + 2;
      }
      // A2
      cBase[3].opposite = haveB ? (baseIdx + 6) : -1;
      cBase[4].opposite = -1;
      if (ca_old[1].opposite >= 0) {
        cBase[4].opposite = ca_old[1].opposite;
        mesh.corners(cBase[4].opposite).opposite = baseIdx + 4;
      }
      cBase[5].opposite = baseIdx + 1;
      if (haveB) {
        // B1
        cBase[6].opposite = baseIdx + 3;
        cBase[7].opposite = baseIdx + 11;
        cBase[8].opposite = -1;
        if (cb_old[2].opposite >= 0) {
          cBase[8].opposite = cb_old[2].opposite;
          mesh.corners(cBase[8].opposite).opposite = baseIdx + 8;
        }
        // B2
        cBase[9].opposite = baseIdx + 0;
        cBase[10].opposite = -1;
        if (cb_old[1].opposite >= 0) {
          cBase[10].opposite = cb_old[1].opposite;
          mesh.corners(cBase[10].opposite).opposite = baseIdx + 10;
        }
        cBase[11].opposite = baseIdx + 7;
      }

      ////////////////////
      // mark the two original triangles for deletion
      taintedTris[triA] = true;
      mesh.removeTriFromLookup(triA);
      if (haveB) {
        taintedTris[triB] = true;
        mesh.removeTriFromLookup(triB);
      }

      Real areaA1 = mesh.getFaceArea(triA1), areaA2 = mesh.getFaceArea(triA2);
      Real areaB1 = 0, areaB2 = 0;
      if (haveB) {
        areaB1 = mesh.getFaceArea(triB1);
        areaB2 = mesh.getFaceArea(triB2);
      }

      // add channel props for new triangles
      for (int i = 0; i < mesh.numTriChannels(); i++) {
        mesh.triChannel(i)->addSplit(triA, areaA1 / (areaA1 + areaA2));
        mesh.triChannel(i)->addSplit(triA, areaA2 / (areaA1 + areaA2));
        if (haveB) {
          mesh.triChannel(i)->addSplit(triB, areaB1 / (areaB1 + areaB2));
          mesh.triChannel(i)->addSplit(triB, areaB2 / (areaB1 + areaB2));
        }
      }

      // add the four new triangles to the prority queue
      for (int i = mesh.numTris() - nt; i < mesh.numTris(); i++) {
        // find the maximum length edge in this triangle
        Vec3 ne0 = mesh.getEdge(i, 0), ne1 = mesh.getEdge(i, 1), ne2 = mesh.getEdge(i, 2);
        Real nd0 = normSquare(ne0);
        Real nd1 = normSquare(ne1);
        Real nd2 = normSquare(ne2);
        Real longest = max(nd0, max(nd1, nd2));
        // longest = (int)(longest * 1e2) / 1e2; // HACK: truncate
        pq.push(pair<Real, int>(longest, i));
      }
      edgeSubdivs++;
    }
  }

  //////////////////////////////////////////
  // EDGE COLLAPSING                      //
  //      - based on short edge length    //
  //////////////////////////////////////////
  if (minLength > 0) {
    const Real minLength2 = minLength * minLength;
    for (int t = 0; t < mesh.numTris(); t++) {
      // we only want to run through the edge list ONCE.
      // we achieve this in a method very similar to the above subdivision method.

      // NOTE:
      // priority queue does not work so great in the edge collapse case,
      // because collapsing one triangle affects the edge lengths
      // of many neighbor triangles,
      // and we do not update their maximum edge length in the queue.

      // if this triangle has already been deleted, ignore it
      // if(taintedTris[t])
      //  continue;

      if (taintedTris.find(t) != taintedTris.end())
        continue;

      // first we find the minimum length edge in this triangle
      Vec3 e0 = mesh.getEdge(t, 0), e1 = mesh.getEdge(t, 1), e2 = mesh.getEdge(t, 2);
      Real d0 = normSquare(e0);
      Real d1 = normSquare(e1);
      Real d2 = normSquare(e2);

      Vec3 edgevect;
      Vec3 endpoint;
      Real dist2;
      int which;
      if (d0 < d1) {
        if (d0 < d2) {
          dist2 = d0;
          edgevect = e0;
          endpoint = mesh.getNode(t, 0);
          which = 2;  // 2 opposite of edge 0-1
        }
        else {
          dist2 = d2;
          edgevect = e2;
          endpoint = mesh.getNode(t, 2);
          which = 1;  // 1 opposite of edge 2-0
        }
      }
      else {
        if (d1 < d2) {
          dist2 = d1;
          edgevect = e1;
          endpoint = mesh.getNode(t, 1);
          which = 0;  // 0 opposite of edge 1-2
        }
        else {
          dist2 = d2;
          edgevect = e2;
          endpoint = mesh.getNode(t, 2);
          which = 1;  // 1 opposite of edge 2-0
        }
      }
      // then we see if the min length edge is too short
      if (dist2 < minLength2) {
        CollapseEdge(
            mesh, t, which, edgevect, endpoint, deletedNodes, taintedTris, edgeCollsLen, cutTubes);
      }
    }
  }
  // cleanup nodes and triangles marked for deletion

  //  we run backwards through the deleted array,
  //  replacing triangles with ones from the back
  //          (this avoids the potential problem of overwriting a triangle
  //              with a to-be-deleted triangle)
  std::map<int, bool>::reverse_iterator tti = taintedTris.rbegin();
  for (; tti != taintedTris.rend(); tti++)
    mesh.removeTri(tti->first);

  mesh.removeNodes(deletedNodes);
  cout << "Surface subdivision finished with " << mesh.numNodes() << " surface nodes and "
       << mesh.numTris();
  cout << " surface triangles, edgeSubdivs:" << edgeSubdivs << ", edgeCollapses: " << edgeCollsLen;
  cout << " + " << edgeCollsAngle << " + " << edgeKill << endl;
  // mesh.sanityCheck();
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "subdivideMesh", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Mesh &mesh = *_args.getPtr<Mesh>("mesh", 0, &_lock);
      Real minAngle = _args.get<Real>("minAngle", 1, &_lock);
      Real minLength = _args.get<Real>("minLength", 2, &_lock);
      Real maxLength = _args.get<Real>("maxLength", 3, &_lock);
      bool cutTubes = _args.getOpt<bool>("cutTubes", 4, false, &_lock);
      _retval = getPyNone();
      subdivideMesh(mesh, minAngle, minLength, maxLength, cutTubes);
      _args.check();
    }
    pbFinalizePlugin(parent, "subdivideMesh", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("subdivideMesh", e.what());
    return 0;
  }
}
static const Pb::Register _RP_subdivideMesh("", "subdivideMesh", _W_1);
extern "C" {
void PbRegister_subdivideMesh()
{
  KEEP_UNUSED(_RP_subdivideMesh);
}
}

void killSmallComponents(Mesh &mesh, int elements = 10)
{
  const int num = mesh.numTris();
  vector<int> comp(num);
  vector<int> numEl;
  vector<int> deletedNodes;
  vector<bool> isNodeDel(mesh.numNodes());
  map<int, bool> taintedTris;
  // enumerate components
  int cur = 0;
  for (int i = 0; i < num; i++) {
    if (comp[i] == 0) {
      cur++;
      comp[i] = cur;

      stack<int> stack;
      stack.push(i);
      int cnt = 1;
      while (!stack.empty()) {
        int tri = stack.top();
        stack.pop();
        for (int c = 0; c < 3; c++) {
          int op = mesh.corners(tri, c).opposite;
          if (op < 0)
            continue;
          int ntri = mesh.corners(op).tri;
          if (comp[ntri] == 0) {
            comp[ntri] = cur;
            stack.push(ntri);
            cnt++;
          }
        }
      }
      numEl.push_back(cnt);
    }
  }
  // kill small components
  for (int j = 0; j < num; j++) {
    if (numEl[comp[j] - 1] < elements) {
      taintedTris[j] = true;
      for (int c = 0; c < 3; c++) {
        int n = mesh.tris(j).c[c];
        if (!isNodeDel[n]) {
          isNodeDel[n] = true;
          deletedNodes.push_back(n);
        }
      }
    }
  }

  std::map<int, bool>::reverse_iterator tti = taintedTris.rbegin();
  for (; tti != taintedTris.rend(); tti++)
    mesh.removeTri(tti->first);

  mesh.removeNodes(deletedNodes);

  if (!taintedTris.empty())
    cout << "Killed small components : " << deletedNodes.size() << " nodes, " << taintedTris.size()
         << " tris deleted." << endl;
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "killSmallComponents", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Mesh &mesh = *_args.getPtr<Mesh>("mesh", 0, &_lock);
      int elements = _args.getOpt<int>("elements", 1, 10, &_lock);
      _retval = getPyNone();
      killSmallComponents(mesh, elements);
      _args.check();
    }
    pbFinalizePlugin(parent, "killSmallComponents", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("killSmallComponents", e.what());
    return 0;
  }
}
static const Pb::Register _RP_killSmallComponents("", "killSmallComponents", _W_2);
extern "C" {
void PbRegister_killSmallComponents()
{
  KEEP_UNUSED(_RP_killSmallComponents);
}
}

}  // namespace Manta
