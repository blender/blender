

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
 * Mesh edge collapse and subdivision
 *
 ******************************************************************************/

/******************************************************************************/
// Copyright note:
//
// These functions (C) Chris Wojtan
// Long-term goal is to unify with his split&merge codebase
//
/******************************************************************************/

#include "edgecollapse.h"
#include <queue>

using namespace std;

namespace Manta {

// 8-point butterfly subdivision scheme (as described by Brochu&Bridson 2009)
Vec3 ButterflySubdivision(Mesh &m, const Corner &ca, const Corner &cb)
{
  Vec3 p = m.nodes(m.corners(ca.prev).node).pos + m.nodes(m.corners(ca.next).node).pos;
  Vec3 q = m.nodes(ca.node).pos + m.nodes(cb.node).pos;
  Vec3 r = m.nodes(m.corners(m.corners(ca.next).opposite).node).pos +
           m.nodes(m.corners(m.corners(ca.prev).opposite).node).pos +
           m.nodes(m.corners(m.corners(cb.next).opposite).node).pos +
           m.nodes(m.corners(m.corners(cb.prev).opposite).node).pos;
  return (8 * p + 2 * q - r) / 16.0;
}

// Modified Butterfly Subdivision Scheme from:
// Interpolating Subdivision for Meshes with Arbitrary Topology
// Denis Zorin, Peter Schroder, and Wim Sweldens
// input the Corner that satisfies the following:
//      c.prev.node is the extraordinary vertex,
//      and c.next.node is the other vertex involved in the subdivision
Vec3 OneSidedButterflySubdivision(Mesh &m, const int valence, const Corner &c)
{
  Vec3 out;
  Vec3 p0 = m.nodes(m.corners(c.prev).node).pos;
  Vec3 p1 = m.nodes(m.corners(c.next).node).pos;

  if (valence == 3) {
    Vec3 p2 = m.nodes(c.node).pos;
    Vec3 p3 = m.nodes(m.corners(m.corners(c.next).opposite).node).pos;
    out = (5.0 / 12.0) * p1 - (1.0 / 12.0) * (p2 + p3) + 0.75 * p0;
  }
  else if (valence == 4) {
    Vec3 p2 = m.nodes(m.corners(m.corners(c.next).opposite).node).pos;
    out = 0.375 * p1 - 0.125 * p2 + 0.75 * p0;
  }
  else {
    // rotate around extraordinary vertex,
    // calculate subdivision weights,
    // and interpolate vertex position
    double rv = 1.0 / (double)valence;
    out = 0.0;
    int current = c.prev;
    for (int j = 0; j < valence; j++) {
      double s = (0.25 + cos(2 * M_PI * j * rv) + 0.5 * cos(4 * M_PI * j * rv)) * rv;
      Vec3 p = m.nodes(m.corners(m.corners(current).prev).node).pos;

      out += s * p;
      current = m.corners(m.corners(m.corners(current).next).opposite).next;
    }
    out += 0.75 * m.nodes(m.corners(c.prev).node).pos;
  }
  return out;
}

// Modified Butterfly Subdivision Scheme from:
// Interpolating Subdivision for Meshes with Arbitrary Topology
// Denis Zorin, Peter Schroder, and Wim Sweldens
Vec3 ModifiedButterflySubdivision(Mesh &m,
                                  const Corner &ca,
                                  const Corner &cb,
                                  const Vec3 &fallback)
{
  // calculate the valence of the two parent vertices
  int start = ca.prev;
  int current = start;
  int valenceA = 0;
  do {
    valenceA++;
    int op = m.corners(m.corners(current).next).opposite;
    if (op < 0)
      return fallback;
    current = m.corners(op).next;
  } while (current != start);
  start = ca.next;
  current = start;
  int valenceB = 0;
  do {
    valenceB++;
    int op = m.corners(m.corners(current).next).opposite;
    if (op < 0)
      return fallback;
    current = m.corners(op).next;
  } while (current != start);

  // if both vertices have valence 6, use butterfly subdivision
  if (valenceA == 6 && valenceB == 6) {
    return ButterflySubdivision(m, ca, cb);
  }
  else if (valenceA == 6)  // use a one-sided scheme
  {
    return OneSidedButterflySubdivision(m, valenceB, cb);
  }
  else if (valenceB == 6)  // use a one-sided scheme
  {
    return OneSidedButterflySubdivision(m, valenceA, ca);
  }
  else  // average the results from two one-sided schemes
  {
    return 0.5 * (OneSidedButterflySubdivision(m, valenceA, ca) +
                  OneSidedButterflySubdivision(m, valenceB, cb));
  }
}

bool gAbort = false;

// collapse an edge on triangle "trinum".
// "which" is 0,1, or 2,
// where which==0 is the triangle edge from p0 to p1,
// which==1 is the triangle edge from p1 to p2,
// and which==2 is the triangle edge from p2 to p0,
void CollapseEdge(Mesh &m,
                  const int trinum,
                  const int which,
                  const Vec3 &edgevect,
                  const Vec3 &endpoint,
                  vector<int> &deletedNodes,
                  std::map<int, bool> &taintedTris,
                  int &numCollapses,
                  bool doTubeCutting)
{
  if (gAbort)
    return;
  // I wanted to draw a pretty picture of an edge collapse,
  // but I don't know how to make wacky angled lines in ASCII.
  // Instead, I will show the before case and tell you what needs to be done.

  //      BEFORE:
  //              *
  //             / \.
  //            /C0 \.
  //           /     \.
  //          /       \.
  //         /    B    \.
  //        /           \.
  //       /C1        C2 \.
  // P0 *---------------* P1
  //       \C2        C1 /
  //        \           /
  //         \    A    /
  //          \       /
  //           \     /
  //            \C0 /
  //             \ /
  //              *
  //
  //  We are going to collapse the edge between P0 and P1
  //      by deleting P1,
  //      and taking all references to P1,
  //          and rerouting them to P0 instead
  //
  //  What we need to do:
  //      Move position of P0
  //      Preserve connectivity in both triangles:
  //          (C1.opposite).opposite = C2.o
  //          (C2.opposite).opposite = C1.o
  //      Delete references to Corners of deleted triangles in both P0 and P1's Corner list
  //      Reassign references to P1:
  //          loop through P1 triangles:
  //              rename P1 references to P0 in p lists.
  //              rename Corner.v references
  //      Copy P1's list of Corners over to P0's list of Corners
  //      Delete P1

  Corner ca_old[3], cb_old[3];
  ca_old[0] = m.corners(trinum, which);
  ca_old[1] = m.corners(ca_old[0].next);
  ca_old[2] = m.corners(ca_old[0].prev);
  bool haveB = false;
  if (ca_old[0].opposite >= 0) {
    cb_old[0] = m.corners(ca_old[0].opposite);
    cb_old[1] = m.corners(cb_old[0].next);
    cb_old[2] = m.corners(cb_old[0].prev);
    haveB = true;
  }
  if (!haveB) {
    // for now, don't collapse
    return;
  }

  int P0 = ca_old[2].node;
  int P1 = ca_old[1].node;

  ///////////////
  // avoid creating nonmanifold edges
  bool nonmanifold = false;
  bool nonmanifold2 = false;

  set<int> &ring0 = m.get1Ring(P0).nodes;
  set<int> &ring1 = m.get1Ring(P1).nodes;

  // check for intersections of the 1-rings of P0,P1
  int cl = 0, commonVert = -1;
  for (set<int>::iterator it = ring1.begin(); it != ring1.end(); ++it)
    if (ring0.find(*it) != ring0.end()) {
      cl++;
      if (*it != ca_old[0].node && *it != cb_old[0].node)
        commonVert = *it;
    }

  nonmanifold = cl > 2;
  nonmanifold2 = cl > 3;

  if (nonmanifold && ca_old[1].opposite >= 0 && cb_old[1].opposite >= 0 &&
      ca_old[2].opposite >= 0 &&
      cb_old[2].opposite >= 0)  // collapsing this edge would create a non-manifold edge
  {
    if (nonmanifold2)
      return;

    bool topTet = false;
    bool botTet = false;
    // check if collapsing this edge will collapse a tet.
    if (m.corners(ca_old[1].opposite).node == m.corners(ca_old[2].opposite).node)
      botTet = true;

    if (m.corners(cb_old[1].opposite).node == m.corners(cb_old[2].opposite).node)
      topTet = true;

    if (topTet ^ botTet) {

      // safe pyramid case.
      // collapse the whole tet!
      // First collapse the top of the pyramid,
      // then carry on collapsing the original verts.
      Corner cc_old[3], cd_old[3];
      if (botTet)
        cc_old[0] = m.corners(ca_old[1].opposite);
      else  // topTet
        cc_old[0] = cb_old[2];
      cc_old[1] = m.corners(cc_old[0].next);
      cc_old[2] = m.corners(cc_old[0].prev);
      if (cc_old[0].opposite < 0)
        return;
      cd_old[0] = m.corners(cc_old[0].opposite);
      cd_old[1] = m.corners(cd_old[0].next);
      cd_old[2] = m.corners(cd_old[0].prev);
      int P2 = cc_old[2].node;
      int P3 = cc_old[1].node;

      // update tri props of all adjacent triangles of P0,P1 (do before CT updates!)
      for (int i = 0; i < m.numTriChannels(); i++) {
      };  // TODO: handleTriPropertyEdgeCollapse(trinum, P2,P3,  cc_old[0], cd_old[0]);

      m.mergeNode(P2, P3);

      // Preserve connectivity in both triangles
      if (cc_old[1].opposite >= 0)
        m.corners(cc_old[1].opposite).opposite = cc_old[2].opposite;
      if (cc_old[2].opposite >= 0)
        m.corners(cc_old[2].opposite).opposite = cc_old[1].opposite;
      if (cd_old[1].opposite >= 0)
        m.corners(cd_old[1].opposite).opposite = cd_old[2].opposite;
      if (cd_old[2].opposite >= 0)
        m.corners(cd_old[2].opposite).opposite = cd_old[1].opposite;

      ////////////////////
      // mark the two triangles and the one node for deletion
      int tmpTrinum = cc_old[0].tri;
      int tmpOthertri = cd_old[0].tri;
      m.removeTriFromLookup(tmpTrinum);
      m.removeTriFromLookup(tmpOthertri);
      taintedTris[tmpTrinum] = true;
      taintedTris[tmpOthertri] = true;
      deletedNodes.push_back(P3);

      numCollapses++;

      // recompute Corners for triangles A and B
      if (botTet)
        ca_old[0] = m.corners(ca_old[2].opposite);
      else
        ca_old[0] = m.corners(ca_old[1].prev);
      ca_old[1] = m.corners(ca_old[0].next);
      ca_old[2] = m.corners(ca_old[0].prev);
      cb_old[0] = m.corners(ca_old[0].opposite);
      cb_old[1] = m.corners(cb_old[0].next);
      cb_old[2] = m.corners(cb_old[0].prev);

      ///////////////
      // avoid creating nonmanifold edges... again
      ring0 = m.get1Ring(ca_old[2].node).nodes;
      ring1 = m.get1Ring(ca_old[1].node).nodes;

      // check for intersections of the 1-rings of P0,P1
      cl = 0;
      for (set<int>::iterator it = ring1.begin(); it != ring1.end(); ++it)
        if (*it != ca_old[0].node && ring0.find(*it) != ring0.end())
          cl++;

      if (cl > 2) {  // nonmanifold
        // this can happen if collapsing the first tet leads to another similar collapse that
        // requires the collapse of a tet. for now, just move on and pick this up later.

        // if the original component was very small, this first collapse could have led to a tiny
        // piece of nonmanifold geometry. in this case, just delete everything that remains.
        if (m.corners(ca_old[0].opposite).tri == cb_old[0].tri &&
            m.corners(ca_old[1].opposite).tri == cb_old[0].tri &&
            m.corners(ca_old[2].opposite).tri == cb_old[0].tri) {
          taintedTris[ca_old[0].tri] = true;
          taintedTris[cb_old[0].tri] = true;
          m.removeTriFromLookup(ca_old[0].tri);
          m.removeTriFromLookup(cb_old[0].tri);
          deletedNodes.push_back(ca_old[0].node);
          deletedNodes.push_back(ca_old[1].node);
          deletedNodes.push_back(ca_old[2].node);
        }
        return;
      }
    }
    else if (topTet && botTet && ca_old[1].opposite >= 0 && ca_old[2].opposite >= 0 &&
             cb_old[1].opposite >= 0 && cb_old[2].opposite >= 0) {
      if (!(m.corners(ca_old[1].opposite).node == m.corners(ca_old[2].opposite).node &&
            m.corners(cb_old[1].opposite).node == m.corners(cb_old[2].opposite).node &&
            (m.corners(ca_old[1].opposite).node == m.corners(cb_old[1].opposite).node ||
             (m.corners(ca_old[1].opposite).node == cb_old[0].node &&
              m.corners(cb_old[1].opposite).node == ca_old[0].node)))) {
        // just collapse one for now.

        // collapse the whole tet!
        // First collapse the top of the pyramid,
        // then carry on collapsing the original verts.
        Corner cc_old[3], cd_old[3];

        // collapse top
        {
          cc_old[0] = m.corners(ca_old[1].opposite);
          cc_old[1] = m.corners(cc_old[0].next);
          cc_old[2] = m.corners(cc_old[0].prev);
          if (cc_old[0].opposite < 0)
            return;
          cd_old[0] = m.corners(cc_old[0].opposite);
          cd_old[1] = m.corners(cd_old[0].next);
          cd_old[2] = m.corners(cd_old[0].prev);
          int P2 = cc_old[2].node;
          int P3 = cc_old[1].node;

          // update tri props of all adjacent triangles of P0,P1 (do before CT updates!)
          // TODO: handleTriPropertyEdgeCollapse(trinum, P2,P3,  cc_old[0], cd_old[0]);

          m.mergeNode(P2, P3);

          // Preserve connectivity in both triangles
          if (cc_old[1].opposite >= 0)
            m.corners(cc_old[1].opposite).opposite = cc_old[2].opposite;
          if (cc_old[2].opposite >= 0)
            m.corners(cc_old[2].opposite).opposite = cc_old[1].opposite;
          if (cd_old[1].opposite >= 0)
            m.corners(cd_old[1].opposite).opposite = cd_old[2].opposite;
          if (cd_old[2].opposite >= 0)
            m.corners(cd_old[2].opposite).opposite = cd_old[1].opposite;

          ////////////////////
          // mark the two triangles and the one node for deletion
          int tmpTrinum = cc_old[0].tri;
          int tmpOthertri = cd_old[0].tri;
          taintedTris[tmpTrinum] = true;
          taintedTris[tmpOthertri] = true;
          m.removeTriFromLookup(tmpTrinum);
          m.removeTriFromLookup(tmpOthertri);
          deletedNodes.push_back(P3);

          numCollapses++;
        }
        // then collapse bottom
        {
          // cc_old[0] = [ca_old[1].opposite;
          cc_old[0] = cb_old[2];
          cc_old[1] = m.corners(cc_old[0].next);
          cc_old[2] = m.corners(cc_old[0].prev);
          if (cc_old[0].opposite < 0)
            return;
          cd_old[0] = m.corners(cc_old[0].opposite);
          cd_old[1] = m.corners(cd_old[0].next);
          cd_old[2] = m.corners(cd_old[0].prev);
          int P2 = cc_old[2].node;
          int P3 = cc_old[1].node;

          // update tri props of all adjacent triangles of P0,P1 (do before CT updates!)
          // TODO: handleTriPropertyEdgeCollapse(trinum, P2,P3,  cc_old[0], cd_old[0]);

          m.mergeNode(P2, P3);

          // Preserve connectivity in both triangles
          if (cc_old[1].opposite >= 0)
            m.corners(cc_old[1].opposite).opposite = cc_old[2].opposite;
          if (cc_old[2].opposite >= 0)
            m.corners(cc_old[2].opposite).opposite = cc_old[1].opposite;
          if (cd_old[1].opposite >= 0)
            m.corners(cd_old[1].opposite).opposite = cd_old[2].opposite;
          if (cd_old[2].opposite >= 0)
            m.corners(cd_old[2].opposite).opposite = cd_old[1].opposite;

          ////////////////////
          // mark the two triangles and the one node for deletion
          int tmpTrinum = cc_old[0].tri;
          int tmpOthertri = cd_old[0].tri;
          taintedTris[tmpTrinum] = true;
          taintedTris[tmpOthertri] = true;
          deletedNodes.push_back(P3);

          numCollapses++;
        }

        // Though we've collapsed a lot of stuff, we still haven't collapsed the original edge.
        // At this point we still haven't guaranteed that this original collapse weill be safe.
        // quit for now, and we'll catch the remaining short edges the next time this function is
        // called.
        return;
      }
    }
    else if (doTubeCutting) {
      // tube case
      // cout<<"CollapseEdge:tube case" << endl;

      // find the edges that touch the common vert
      int P2 = commonVert;
      int P1P2 = -1, P2P1, P2P0 = -1, P0P2 = -1;  // corners across from the cutting seam
      int start = ca_old[0].next;
      int end = cb_old[0].prev;
      int current = start;
      do {
        // rotate around vertex P1 counter-clockwise
        int op = m.corners(m.corners(current).next).opposite;
        if (op < 0)
          errMsg("tube cutting failed, no opposite");
        current = m.corners(op).next;

        if (m.corners(m.corners(current).prev).node == commonVert)
          P1P2 = m.corners(current).next;
      } while (current != end);

      start = ca_old[0].prev;
      end = cb_old[0].next;
      current = start;
      do {
        // rotate around vertex P0 clockwise
        int op = m.corners(m.corners(current).prev).opposite;
        if (op < 0)
          errMsg("tube cutting failed, no opposite");

        current = m.corners(op).prev;
        if (m.corners(m.corners(current).next).node == commonVert)
          P2P0 = m.corners(current).prev;
      } while (current != end);

      if (P1P2 < 0 || P2P0 < 0)
        errMsg("tube cutting failed, ill geometry");

      P2P1 = m.corners(P1P2).opposite;
      P0P2 = m.corners(P2P0).opposite;

      // duplicate vertices on the top half of the cut,
      // and use them to split the tube at this seam
      int P0b = m.addNode(Node(m.nodes(P0).pos));
      int P1b = m.addNode(Node(m.nodes(P1).pos));
      int P2b = m.addNode(Node(m.nodes(P2).pos));
      for (int i = 0; i < m.numNodeChannels(); i++) {
        m.nodeChannel(i)->addInterpol(P0, P0, 0.5);
        m.nodeChannel(i)->addInterpol(P1, P1, 0.5);
        m.nodeChannel(i)->addInterpol(P2, P2, 0.5);
      }

      // offset the verts in the normal directions to avoid self intersections
      Vec3 offsetVec = cross(m.nodes(P1).pos - m.nodes(P0).pos, m.nodes(P2).pos - m.nodes(P0).pos);
      normalize(offsetVec);
      offsetVec *= 0.01;  // HACK:
      m.nodes(P0).pos -= offsetVec;
      m.nodes(P1).pos -= offsetVec;
      m.nodes(P2).pos -= offsetVec;
      m.nodes(P0b).pos += offsetVec;
      m.nodes(P1b).pos += offsetVec;
      m.nodes(P2b).pos += offsetVec;

      // create a list of all triangles which touch P0, P1, and P2 from the top,
      map<int, bool> topTris;
      start = cb_old[0].next;
      end = m.corners(P0P2).prev;
      current = start;
      topTris[start / 3] = true;
      do {
        // rotate around vertex P0 counter-clockwise
        current = m.corners(m.corners(m.corners(current).next).opposite).next;
        topTris[current / 3] = true;
      } while (current != end);
      start = m.corners(P0P2).next;
      end = m.corners(P2P1).prev;
      current = start;
      topTris[start / 3] = true;
      do {
        // rotate around vertex P0 counter-clockwise
        current = m.corners(m.corners(m.corners(current).next).opposite).next;
        topTris[current / 3] = true;
      } while (current != end);
      start = m.corners(P2P1).next;
      end = cb_old[0].prev;
      current = start;
      topTris[start / 3] = true;
      do {
        // rotate around vertex P0 counter-clockwise
        current = m.corners(m.corners(m.corners(current).next).opposite).next;
        topTris[current / 3] = true;
      } while (current != end);

      // create two new triangles,
      int Ta = m.addTri(Triangle(P0, P1, P2));
      int Tb = m.addTri(Triangle(P1b, P0b, P2b));
      for (int i = 0; i < m.numTriChannels(); i++) {
        m.triChannel(i)->addNew();
        m.triChannel(i)->addNew();
      }

      // sew the tris to close the cut on each side
      for (int c = 0; c < 3; c++)
        m.addCorner(Corner(Ta, m.tris(Ta).c[c]));
      for (int c = 0; c < 3; c++)
        m.addCorner(Corner(Tb, m.tris(Tb).c[c]));
      for (int c = 0; c < 3; c++) {
        m.corners(Ta, c).next = 3 * Ta + ((c + 1) % 3);
        m.corners(Ta, c).prev = 3 * Ta + ((c + 2) % 3);
        m.corners(Tb, c).next = 3 * Tb + ((c + 1) % 3);
        m.corners(Tb, c).prev = 3 * Tb + ((c + 2) % 3);
      }
      m.corners(Ta, 0).opposite = P1P2;
      m.corners(Ta, 1).opposite = P2P0;
      m.corners(Ta, 2).opposite = ca_old[1].prev;
      m.corners(Tb, 0).opposite = P0P2;
      m.corners(Tb, 1).opposite = P2P1;
      m.corners(Tb, 2).opposite = cb_old[1].prev;
      for (int c = 0; c < 3; c++) {
        m.corners(m.corners(Ta, c).opposite).opposite = 3 * Ta + c;
        m.corners(m.corners(Tb, c).opposite).opposite = 3 * Tb + c;
      }
      // replace P0,P1,P2 on the top with P0b,P1b,P2b.
      for (map<int, bool>::iterator tti = topTris.begin(); tti != topTris.end(); tti++) {
        // cout << "H " << tti->first << " : " << m.tris(tti->first).c[0] << " " <<
        // m.tris(tti->first).c[1] << " " << m.tris(tti->first).c[2] << " " << endl;
        for (int i = 0; i < 3; i++) {
          int cn = m.tris(tti->first).c[i];
          set<int> &ring = m.get1Ring(cn).nodes;

          if (ring.find(P0) != ring.end() && cn != P0 && cn != P1 && cn != P2 && cn != P0b &&
              cn != P1b && cn != P2b) {
            ring.erase(P0);
            ring.insert(P0b);
            m.get1Ring(P0).nodes.erase(cn);
            m.get1Ring(P0b).nodes.insert(cn);
          }
          if (ring.find(P1) != ring.end() && cn != P0 && cn != P1 && cn != P2 && cn != P0b &&
              cn != P1b && cn != P2b) {
            ring.erase(P1);
            ring.insert(P1b);
            m.get1Ring(P1).nodes.erase(cn);
            m.get1Ring(P1b).nodes.insert(cn);
          }
          if (ring.find(P2) != ring.end() && cn != P0 && cn != P1 && cn != P2 && cn != P0b &&
              cn != P1b && cn != P2b) {
            ring.erase(P2);
            ring.insert(P2b);
            m.get1Ring(P2).nodes.erase(cn);
            m.get1Ring(P2b).nodes.insert(cn);
          }
          if (cn == P0) {
            m.tris(tti->first).c[i] = P0b;
            m.corners(tti->first, i).node = P0b;
            m.get1Ring(P0).tris.erase(tti->first);
            m.get1Ring(P0b).tris.insert(tti->first);
          }
          else if (cn == P1) {
            m.tris(tti->first).c[i] = P1b;
            m.corners(tti->first, i).node = P1b;
            m.get1Ring(P1).tris.erase(tti->first);
            m.get1Ring(P1b).tris.insert(tti->first);
          }
          else if (cn == P2) {
            m.tris(tti->first).c[i] = P2b;
            m.corners(tti->first, i).node = P2b;
            m.get1Ring(P2).tris.erase(tti->first);
            m.get1Ring(P2b).tris.insert(tti->first);
          }
        }
      }

      // m.sanityCheck(true, &deletedNodes, &taintedTris);

      return;
    }
    return;
  }
  if (ca_old[1].opposite >= 0 && ca_old[2].opposite >= 0 && cb_old[1].opposite >= 0 &&
      cb_old[2].opposite >= 0 && ca_old[0].opposite >= 0 && cb_old[0].opposite >= 0 &&
      ((m.corners(ca_old[1].opposite).node ==
            m.corners(ca_old[2].opposite).node &&  // two-pyramid tubey case (6 tris, 5 verts)
        m.corners(cb_old[1].opposite).node == m.corners(cb_old[2].opposite).node &&
        (m.corners(ca_old[1].opposite).node == m.corners(cb_old[1].opposite).node ||
         (m.corners(ca_old[1].opposite).node == cb_old[0].node &&  // single tetrahedron case
          m.corners(cb_old[1].opposite).node == ca_old[0].node))) ||
       (m.corners(ca_old[0].opposite).tri == m.corners(cb_old[0].opposite).tri &&
        m.corners(ca_old[1].opposite).tri == m.corners(cb_old[0].opposite).tri &&
        m.corners(ca_old[2].opposite).tri ==
            m.corners(cb_old[0].opposite).tri  // nonmanifold: 2 tris, 3 verts
        && m.corners(cb_old[0].opposite).tri == m.corners(ca_old[0].opposite).tri &&
        m.corners(cb_old[1].opposite).tri == m.corners(ca_old[0].opposite).tri &&
        m.corners(cb_old[2].opposite).tri == m.corners(ca_old[0].opposite).tri))) {
    // both top and bottom are closed pyramid caps, or it is a single tet
    // delete the whole component!
    // flood fill to mark all triangles in the component
    map<int, bool> markedTris;
    queue<int> triQ;
    triQ.push(trinum);
    markedTris[trinum] = true;
    int iters = 0;
    while (!triQ.empty()) {
      int trival = triQ.front();
      triQ.pop();
      for (int i = 0; i < 3; i++) {
        int newtri = m.corners(m.corners(trival, i).opposite).tri;
        if (markedTris.find(newtri) == markedTris.end()) {
          triQ.push(newtri);
          markedTris[newtri] = true;
        }
      }
      iters++;
    }
    map<int, bool> markedverts;
    for (map<int, bool>::iterator mit = markedTris.begin(); mit != markedTris.end(); mit++) {
      taintedTris[mit->first] = true;
      markedverts[m.tris(mit->first).c[0]] = true;
      markedverts[m.tris(mit->first).c[1]] = true;
      markedverts[m.tris(mit->first).c[2]] = true;
    }
    for (map<int, bool>::iterator mit = markedverts.begin(); mit != markedverts.end(); mit++)
      deletedNodes.push_back(mit->first);
    return;
  }

  //////////////////////////
  // begin original edge collapse

  // update tri props of all adjacent triangles of P0,P1 (do before CT updates!)
  // TODO: handleTriPropertyEdgeCollapse(trinum, P0,P1,  ca_old[0], cb_old[0]);

  m.mergeNode(P0, P1);

  // Move position of P0
  m.nodes(P0).pos = endpoint + 0.5 * edgevect;

  // Preserve connectivity in both triangles
  if (ca_old[1].opposite >= 0)
    m.corners(ca_old[1].opposite).opposite = ca_old[2].opposite;
  if (ca_old[2].opposite >= 0)
    m.corners(ca_old[2].opposite).opposite = ca_old[1].opposite;
  if (haveB && cb_old[1].opposite >= 0)
    m.corners(cb_old[1].opposite).opposite = cb_old[2].opposite;
  if (haveB && cb_old[2].opposite >= 0)
    m.corners(cb_old[2].opposite).opposite = cb_old[1].opposite;

  ////////////////////
  // mark the two triangles and the one node for deletion
  taintedTris[ca_old[0].tri] = true;
  m.removeTriFromLookup(ca_old[0].tri);
  if (haveB) {
    taintedTris[cb_old[0].tri] = true;
    m.removeTriFromLookup(cb_old[0].tri);
  }
  deletedNodes.push_back(P1);
  numCollapses++;
}

}  // namespace Manta
