/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes to define a Winged Edge data structure.
 */

#include <iterator>
#include <vector>

#include "../geometry/Geom.h"

#include "../scene_graph/FrsMaterial.h"

#include "../system/FreestyleConfig.h"

#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

using namespace std;

namespace Freestyle {

using namespace Geometry;

/**********************************
 *                                *
 *                                *
 *             WVertex            *
 *                                *
 *                                *
 **********************************/

class WOEdge;
class WEdge;
class WShape;
class WFace;

class WVertex {
 protected:
  int _Id;  // an identificator
  Vec3f _Vertex;
  vector<WEdge *> _EdgeList;
  WShape *_Shape;  // the shape to which the vertex belongs
  bool _Smooth;    // flag to indicate whether the Vertex belongs to a smooth edge or not
  short _Border;   // 1 -> border, 0 -> no border, -1 -> not set

 public:
  void *userdata;  // designed to store specific user data
  inline WVertex(const Vec3f &v)
  {
    _Id = 0;
    _Vertex = v;
    userdata = nullptr;
    _Shape = nullptr;
    _Smooth = true;
    _Border = -1;
  }

  /** Copy constructor */
  WVertex(WVertex &iBrother);
  virtual WVertex *duplicate();
  virtual ~WVertex() {}

  /** accessors */
  inline Vec3f &GetVertex()
  {
    return _Vertex;
  }

  inline vector<WEdge *> &GetEdges()
  {
    return _EdgeList;
  }

  inline int GetId()
  {
    return _Id;
  }

  inline WShape *shape() const
  {
    return _Shape;
  }

  inline bool isSmooth() const
  {
    return _Smooth;
  }

  bool isBoundary();

  /** modifiers */
  inline void setVertex(const Vec3f &v)
  {
    _Vertex = v;
  }

  inline void setEdges(const vector<WEdge *> &iEdgeList)
  {
    _EdgeList = iEdgeList;
  }

  inline void setId(int id)
  {
    _Id = id;
  }

  inline void setShape(WShape *iShape)
  {
    _Shape = iShape;
  }

  inline void setSmooth(bool b)
  {
    _Smooth = b;
  }

  inline void setBorder(bool b)
  {
    if (b) {
      _Border = 1;
    }
    else {
      _Border = 0;
    }
  }

  /** Adds an edge to the edges list */
  void AddEdge(WEdge *iEdge);

  virtual void ResetUserData()
  {
    userdata = nullptr;
  }

 public:
  /** Iterator to iterate over a vertex incoming edges in the CCW order. */
  class incoming_edge_iterator {
   public:
    using iterator_category = input_iterator_tag;
    using value_type = WOEdge *;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

   private:
    WVertex *_vertex;
    //
    WOEdge *_begin;
    WOEdge *_current;

   public:
    inline incoming_edge_iterator() = default;
    virtual ~incoming_edge_iterator() = default;

   protected:
    friend class WVertex;
    inline incoming_edge_iterator(WVertex *iVertex, WOEdge *iBegin, WOEdge *iCurrent)
    {
      _vertex = iVertex;
      _begin = iBegin;
      _current = iCurrent;
    }

   public:
    inline incoming_edge_iterator(const incoming_edge_iterator &iBrother)
    {
      _vertex = iBrother._vertex;
      _begin = iBrother._begin;
      _current = iBrother._current;
    }

   public:
    // operators
    // operator corresponding to ++i
    virtual incoming_edge_iterator &operator++()
    {
      increment();
      return *this;
    }

    // operator corresponding to i++
    virtual incoming_edge_iterator operator++(int)
    {
      incoming_edge_iterator tmp = *this;
      increment();
      return tmp;
    }

    // comparability
    virtual bool operator!=(const incoming_edge_iterator &b) const
    {
      return ((_current) != (b._current));
    }

    virtual bool operator==(const incoming_edge_iterator &b) const
    {
      return ((_current) == (b._current));
    }

    // dereferencing
    virtual WOEdge *operator*();
    // virtual WOEdge **operator->();
   protected:
    virtual void increment();

    MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex:incoming_edge_iterator")
  };

  class face_iterator {
   public:
    using iterator_category = input_iterator_tag;
    using value_type = WFace *;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

   private:
    incoming_edge_iterator _edge_it;

   public:
    inline face_iterator() = default;
    virtual ~face_iterator() = default;

   protected:
    friend class WVertex;
    inline face_iterator(incoming_edge_iterator it)
    {
      _edge_it = it;
    }

   public:
    inline face_iterator(const face_iterator &iBrother)
    {
      _edge_it = iBrother._edge_it;
    }

   public:
    // operators
    // operator corresponding to ++i
    virtual face_iterator &operator++()
    {
      increment();
      return *this;
    }

    // operator corresponding to i++
    virtual face_iterator operator++(int)
    {
      face_iterator tmp = *this;
      increment();
      return tmp;
    }

    // comparability
    virtual bool operator!=(const face_iterator &b) const
    {
      return ((_edge_it) != (b._edge_it));
    }

    virtual bool operator==(const face_iterator &b) const
    {
      return ((_edge_it) == (b._edge_it));
    }

    // dereferencing
    virtual WFace *operator*();
    // virtual WOEdge **operator->();

   protected:
    inline void increment()
    {
      ++_edge_it;
    }

    MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex:face_iterator")
  };

 public:
  /** iterators access */
  virtual incoming_edge_iterator incoming_edges_begin();
  virtual incoming_edge_iterator incoming_edges_end();

  virtual face_iterator faces_begin()
  {
    return face_iterator(incoming_edges_begin());
  }

  virtual face_iterator faces_end()
  {
    return face_iterator(incoming_edges_end());
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex")
};

/**********************************
 *                                *
 *                                *
 *             WOEdge             *
 *                                *
 *                                *
 **********************************/

class WFace;
class WEdge;

class WOEdge {
 protected:
#if 0
  WOEdge *_paCWEdge;   // edge reached when traveling clockwise on aFace from the edge
  WOEdge *_pbCWEdge;   // edge reached when traveling clockwise on bFace from the edge
  WOEdge *_paCCWEdge;  // edge reached when traveling counterclockwise on aFace from the edge
  WOEdge *_pbCCWEdge;  // edge reached when traveling counterclockwise on bFace from the edge
#endif
  WVertex *_paVertex;  // starting vertex
  WVertex *_pbVertex;  // ending vertex
  WFace *_paFace;      // when following the edge, face on the right
  WFace *_pbFace;      // when following the edge, face on the left
  WEdge *_pOwner;      // Edge

  Vec3f _vec;
  float _angle;

 public:
  void *userdata;

  inline WOEdge()
  {
#if 0
    _paCWEdge = nullptr;
    _pbCWEdge = nullptr;
    _paCCWEdge = nullptr;
    _pbCCWEdge = nullptr;
#endif
    _paVertex = nullptr;
    _pbVertex = nullptr;
    _paFace = nullptr;
    _pbFace = nullptr;
    _pOwner = nullptr;
    userdata = nullptr;
  }

  virtual ~WOEdge() {};  // soc

  /** copy constructor */
  WOEdge(WOEdge &iBrother);
  virtual WOEdge *duplicate();

  /** accessors */
#if 0
  inline WOEdge *GetaCWEdge()
  {
    return _paCWEdge;
  }

  inline WOEdge *GetbCWEdge()
  {
    return _pbCWEdge;
  }

  inline WOEdge *GetaCCWEdge()
  {
    return _paCCWEdge;
  }

  inline WOEdge *GetbCCWEdge()
  {
    return _pbCCWEdge;
  }
#endif

  inline WVertex *GetaVertex()
  {
    return _paVertex;
  }

  inline WVertex *GetbVertex()
  {
    return _pbVertex;
  }

  inline WFace *GetaFace()
  {
    return _paFace;
  }

  inline WFace *GetbFace()
  {
    return _pbFace;
  }

  inline WEdge *GetOwner()
  {
    return _pOwner;
  }

  inline const Vec3f &GetVec()
  {
    return _vec;
  }

  inline float GetAngle()
  {
    return _angle;
  }

  /** modifiers */
#if 0
  inline void SetaCWEdge(WOEdge *pe)
  {
    _paCWEdge = pe;
  }

  inline void SetbCWEdge(WOEdge *pe)
  {
    _pbCWEdge = pe;
  }

  inline void SetaCCWEdge(WOEdge *pe)
  {
    _paCCWEdge = pe;
  }

  inline void SetbCCCWEdge(WOEdge *pe)
  {
    _pbCCWEdge = pe;
  }
#endif

  inline void setVecAndAngle();

  inline void setaVertex(WVertex *pv)
  {
    _paVertex = pv;
    setVecAndAngle();
  }

  inline void setbVertex(WVertex *pv)
  {
    _pbVertex = pv;
    setVecAndAngle();
  }

  inline void setaFace(WFace *pf)
  {
    _paFace = pf;
    setVecAndAngle();
  }

  inline void setbFace(WFace *pf)
  {
    _pbFace = pf;
    setVecAndAngle();
  }

  inline void setOwner(WEdge *pe)
  {
    _pOwner = pe;
  }

  /** Retrieves the list of edges in CW order */
  inline void RetrieveCWOrderedEdges(vector<WEdge *> &oEdges);

  WOEdge *twin();
  WOEdge *getPrevOnFace();

  virtual void ResetUserData()
  {
    userdata = nullptr;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WOEdge")
};

/**********************************
 *                                *
 *                                *
 *             WEdge              *
 *                                *
 *                                *
 **********************************/

class WEdge {
 protected:
  WOEdge *_paOEdge;  // first oriented edge
  WOEdge *_pbOEdge;  // second oriented edge
  short _nOEdges;    // number of oriented edges associated with this edge. (1 means border edge)
  bool _Mark;        // user-specified edge mark for feature edge detection
  int _Id;           // Identifier for the edge

 public:
  void *userdata;  // designed to store specific user data

  inline WEdge()
  {
    _paOEdge = nullptr;
    _pbOEdge = nullptr;
    _nOEdges = 0;
    userdata = nullptr;
  }

  inline WEdge(WOEdge *iOEdge)
  {
    _paOEdge = iOEdge;
    _pbOEdge = nullptr;
    _nOEdges = 1;
    userdata = nullptr;
  }

  inline WEdge(WOEdge *iaOEdge, WOEdge *ibOEdge)
  {
    _paOEdge = iaOEdge;
    _pbOEdge = ibOEdge;
    _nOEdges = 2;
    userdata = nullptr;
  }

  /** Copy constructor */
  WEdge(WEdge &iBrother);
  virtual WEdge *duplicate();

  virtual ~WEdge()
  {
    if (_paOEdge) {
      delete _paOEdge;
      _paOEdge = nullptr;
    }

    if (_pbOEdge) {
      delete _pbOEdge;
      _pbOEdge = nullptr;
    }
  }

  /** checks whether two WEdge have a common vertex.
   *  Returns a pointer on the common vertex if it exists, nullptr otherwise.
   */
  static inline WVertex *CommonVertex(WEdge *iEdge1, WEdge *iEdge2)
  {
    if (!iEdge1 || !iEdge2) {
      return nullptr;
    }

    WVertex *wv1 = iEdge1->GetaOEdge()->GetaVertex();
    WVertex *wv2 = iEdge1->GetaOEdge()->GetbVertex();
    WVertex *wv3 = iEdge2->GetaOEdge()->GetaVertex();
    WVertex *wv4 = iEdge2->GetaOEdge()->GetbVertex();

    if ((wv1 == wv3) || (wv1 == wv4)) {
      return wv1;
    }
    else if ((wv2 == wv3) || (wv2 == wv4)) {
      return wv2;
    }
    return nullptr;
  }

  /** accessors */
  inline WOEdge *GetaOEdge()
  {
    return _paOEdge;
  }

  inline WOEdge *GetbOEdge()
  {
    return _pbOEdge;
  }

  inline short GetNumberOfOEdges()
  {
    return _nOEdges;
  }

  inline bool GetMark()
  {
    return _Mark;
  }

  inline int GetId()
  {
    return _Id;
  }

  inline WVertex *GetaVertex()
  {
    return _paOEdge->GetaVertex();
  }

  inline WVertex *GetbVertex()
  {
    return _paOEdge->GetbVertex();
  }

  inline WFace *GetaFace()
  {
    return _paOEdge->GetaFace();
  }

  inline WFace *GetbFace()
  {
    return _paOEdge->GetbFace();
  }

  inline WOEdge *GetOtherOEdge(WOEdge *iOEdge)
  {
    if (iOEdge == _paOEdge) {
      return _pbOEdge;
    }
    else {
      return _paOEdge;
    }
  }

  /** modifiers */
  inline void setaOEdge(WOEdge *iEdge)
  {
    _paOEdge = iEdge;
  }

  inline void setbOEdge(WOEdge *iEdge)
  {
    _pbOEdge = iEdge;
  }

  inline void AddOEdge(WOEdge *iEdge)
  {
    if (!_paOEdge) {
      _paOEdge = iEdge;
      _nOEdges++;
      return;
    }
    if (!_pbOEdge) {
      _pbOEdge = iEdge;
      _nOEdges++;
      return;
    }
  }

  inline void setNumberOfOEdges(short n)
  {
    _nOEdges = n;
  }

  inline void setMark(bool mark)
  {
    _Mark = mark;
  }

  inline void setId(int id)
  {
    _Id = id;
  }

  virtual void ResetUserData()
  {
    userdata = nullptr;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WEdge")
};

/**********************************
 *                                *
 *                                *
 *             WFace              *
 *                                *
 *                                *
 **********************************/

class WFace {
 protected:
  vector<WOEdge *> _OEdgeList;  // list of oriented edges of bording the face
  Vec3f _Normal;                // normal to the face
  // in case there is a normal per vertex.
  // The normal number i corresponds to the aVertex of the oedge number i, for that face
  vector<Vec3f> _VerticesNormals;
  vector<Vec2f> _VerticesTexCoords;

  int _Id;
  uint _FrsMaterialIndex;
  bool _Mark;  // Freestyle face mark (if true, feature edges on this face are ignored)

 public:
  void *userdata;
  inline WFace()
  {
    userdata = nullptr;
    _FrsMaterialIndex = 0;
  }

  /** copy constructor */
  WFace(WFace &iBrother);
  virtual WFace *duplicate();
  virtual ~WFace() {}

  /** accessors */
  inline const vector<WOEdge *> &getEdgeList()
  {
    return _OEdgeList;
  }

  inline WOEdge *GetOEdge(int i)
  {
    return _OEdgeList[i];
  }

  inline Vec3f &GetNormal()
  {
    return _Normal;
  }

  inline int GetId()
  {
    return _Id;
  }

  inline uint frs_materialIndex() const
  {
    return _FrsMaterialIndex;
  }

  inline bool GetMark() const
  {
    return _Mark;
  }

  const FrsMaterial &frs_material();

  /** The vertex of index i corresponds to the a vertex of the edge of index i */
  inline WVertex *GetVertex(uint index)
  {
#if 0
    if (index >= _OEdgeList.size()) {
      return nullptr;
    }
#endif
    return _OEdgeList[index]->GetaVertex();
  }

  /** returns the index at which iVertex is stored in the array.
   * returns -1 if iVertex doesn't belong to the face.
   */
  inline int GetIndex(WVertex *iVertex)
  {
    int index = 0;
    for (vector<WOEdge *>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end();
         woe != woend;
         woe++)
    {
      if ((*woe)->GetaVertex() == iVertex) {
        return index;
      }
      ++index;
    }
    return -1;
  }

  inline void RetrieveVertexList(vector<WVertex *> &oVertices)
  {
    for (vector<WOEdge *>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end();
         woe != woend;
         woe++)
    {
      oVertices.push_back((*woe)->GetaVertex());
    }
  }

  inline void RetrieveBorderFaces(vector<const WFace *> &oWFaces)
  {
    for (vector<WOEdge *>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end();
         woe != woend;
         woe++)
    {
      WFace *af;
      if ((af = (*woe)->GetaFace())) {
        oWFaces.push_back(af);
      }
    }
  }

  inline WFace *GetBordingFace(int index)
  {
#if 0
    if (index >= _OEdgeList.size()) {
      return nullptr;
    }
#endif
    return _OEdgeList[index]->GetaFace();
  }

  inline WFace *GetBordingFace(WOEdge *iOEdge)
  {
    return iOEdge->GetaFace();
  }

  inline vector<Vec3f> &GetPerVertexNormals()
  {
    return _VerticesNormals;
  }

  inline vector<Vec2f> &GetPerVertexTexCoords()
  {
    return _VerticesTexCoords;
  }

  /** Returns the normal of the vertex of `index`. */
  inline Vec3f &GetVertexNormal(int index)
  {
    return _VerticesNormals[index];
  }

  /** Returns the texture coords of the vertex of `index`. */
  inline Vec2f &GetVertexTexCoords(int index)
  {
    return _VerticesTexCoords[index];
  }

  /** Returns the normal of the vertex iVertex for that face */
  inline Vec3f &GetVertexNormal(WVertex *iVertex)
  {
    int i = 0;
    int index = 0;
    for (vector<WOEdge *>::const_iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end();
         woe != woend;
         woe++)
    {
      if ((*woe)->GetaVertex() == iVertex) {
        index = i;
        break;
      }
      ++i;
    }

    return _VerticesNormals[index];
  }

  inline WOEdge *GetNextOEdge(WOEdge *iOEdge)
  {
    bool found = false;
    vector<WOEdge *>::iterator woe, woend, woefirst;
    woefirst = _OEdgeList.begin();
    for (woe = woefirst, woend = _OEdgeList.end(); woe != woend; ++woe) {
      if (found) {
        return (*woe);
      }

      if ((*woe) == iOEdge) {
        found = true;
      }
    }

    // We left the loop. That means that the first OEdge was the good one:
    if (found) {
      return (*woefirst);
    }

    return nullptr;
  }

  WOEdge *GetPrevOEdge(WOEdge *iOEdge);

  inline int numberOfEdges() const
  {
    return _OEdgeList.size();
  }

  inline int numberOfVertices() const
  {
    return _OEdgeList.size();
  }

  /** Returns true if the face has one ot its edge which is a border edge */
  inline bool isBorder() const
  {
    for (vector<WOEdge *>::const_iterator woe = _OEdgeList.begin(), woeend = _OEdgeList.end();
         woe != woeend;
         ++woe)
    {
      if ((*woe)->GetOwner()->GetbOEdge() == 0) {
        return true;
      }
    }
    return false;
  }

  /** modifiers */
  inline void setEdgeList(const vector<WOEdge *> &iEdgeList)
  {
    _OEdgeList = iEdgeList;
  }

  inline void setNormal(const Vec3f &iNormal)
  {
    _Normal = iNormal;
  }

  inline void setNormalList(const vector<Vec3f> &iNormalsList)
  {
    _VerticesNormals = iNormalsList;
  }

  inline void setTexCoordsList(const vector<Vec2f> &iTexCoordsList)
  {
    _VerticesTexCoords = iTexCoordsList;
  }

  inline void setId(int id)
  {
    _Id = id;
  }

  inline void setFrsMaterialIndex(uint iMaterialIndex)
  {
    _FrsMaterialIndex = iMaterialIndex;
  }

  inline void setMark(bool iMark)
  {
    _Mark = iMark;
  }

  /** designed to build a specialized WEdge for use in MakeEdge */
  virtual WEdge *instanciateEdge() const
  {
    return new WEdge;
  }

  /** Builds an oriented edge
   *  Returns the built edge.
   *    v1, v2
   *      Vertices at the edge's extremities
   *      The edge is oriented from v1 to v2.
   */
  virtual WOEdge *MakeEdge(WVertex *v1, WVertex *v2);

  /** Adds an edge to the edges list */
  inline void AddEdge(WOEdge *iEdge)
  {
    _OEdgeList.push_back(iEdge);
  }

  /** For triangles, returns the edge opposite to the vertex in e.
   *  returns false if the face is not a triangle or if the vertex is not found
   */
  bool getOppositeEdge(const WVertex *v, WOEdge *&e);

  /** compute the area of the face */
  float getArea();

  WShape *getShape();
  virtual void ResetUserData()
  {
    userdata = nullptr;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WFace")
};

/**********************************
 *                                *
 *                                *
 *             WShape             *
 *                                *
 *                                *
 **********************************/

class WShape {
 protected:
  vector<WVertex *> _VertexList;
  vector<WEdge *> _EdgeList;
  vector<WFace *> _FaceList;
  int _Id;
  string _Name;
  string _LibraryPath;
  static uint _SceneCurrentId;
#if 0
  Vec3f _min;
  Vec3f _max;
#endif
  vector<FrsMaterial> _FrsMaterials;
#if 0
  float _meanEdgeSize;
#endif

 public:
  inline WShape()
  {
#if 0
    _meanEdgeSize = 0;
#endif
    _Id = _SceneCurrentId;
    _SceneCurrentId++;
  }

  /** copy constructor */
  WShape(WShape &iBrother);
  virtual WShape *duplicate();

  virtual ~WShape()
  {
    if (_EdgeList.size() != 0) {
      vector<WEdge *>::iterator e;
      for (e = _EdgeList.begin(); e != _EdgeList.end(); ++e) {
        delete (*e);
      }
      _EdgeList.clear();
    }

    if (_VertexList.size() != 0) {
      vector<WVertex *>::iterator v;
      for (v = _VertexList.begin(); v != _VertexList.end(); ++v) {
        delete (*v);
      }
      _VertexList.clear();
    }

    if (_FaceList.size() != 0) {
      vector<WFace *>::iterator f;
      for (f = _FaceList.begin(); f != _FaceList.end(); ++f) {
        delete (*f);
      }
      _FaceList.clear();
    }
  }

  /** accessors */
  inline vector<WEdge *> &getEdgeList()
  {
    return _EdgeList;
  }

  inline vector<WVertex *> &getVertexList()
  {
    return _VertexList;
  }

  inline vector<WFace *> &GetFaceList()
  {
    return _FaceList;
  }

  inline uint GetId()
  {
    return _Id;
  }

#if 0
  inline void bbox(Vec3f &min, Vec3f &max)
  {
    min = _min;
    max = _max;
  }
#endif

  inline const FrsMaterial &frs_material(uint i) const
  {
    return _FrsMaterials[i];
  }

  inline const vector<FrsMaterial> &frs_materials() const
  {
    return _FrsMaterials;
  }

#if 0
  inline const float getMeanEdgeSize() const
  {
    return _meanEdgeSize;
  }
#endif

  inline const string &getName() const
  {
    return _Name;
  }

  inline const string &getLibraryPath() const
  {
    return _LibraryPath;
  }

  /** modifiers */
  static inline void setCurrentId(const uint id)
  {
    _SceneCurrentId = id;
  }

  inline void setEdgeList(const vector<WEdge *> &iEdgeList)
  {
    _EdgeList = iEdgeList;
  }

  inline void setVertexList(const vector<WVertex *> &iVertexList)
  {
    _VertexList = iVertexList;
  }

  inline void setFaceList(const vector<WFace *> &iFaceList)
  {
    _FaceList = iFaceList;
  }

  inline void setId(int id)
  {
    _Id = id;
  }

#if 0
  inline void setBBox(const Vec3f &min, const Vec3f &max)
  {
    _min = min;
    _max = max;
  }
#endif

  inline void setFrsMaterial(const FrsMaterial &frs_material, uint i)
  {
    _FrsMaterials[i] = frs_material;
  }

  inline void setFrsMaterials(const vector<FrsMaterial> &iMaterials)
  {
    _FrsMaterials = iMaterials;
  }

  inline void setName(const string &name)
  {
    _Name = name;
  }

  inline void setLibraryPath(const string &path)
  {
    _LibraryPath = path;
  }

  /** designed to build a specialized WFace for use in MakeFace */
  virtual WFace *instanciateFace() const
  {
    return new WFace;
  }

  /** adds a new face to the shape
   *  returns the built face.
   *   iVertexList
   *      List of face's vertices. These vertices are not added to the WShape vertex list; they are
   * supposed to be already stored when calling MakeFace. The order in which the vertices are
   * stored in the list determines the face's edges orientation and (so) the face orientation.
   *   iMaterialIndex
   *      The material index for this face
   */
  virtual WFace *MakeFace(vector<WVertex *> &iVertexList,
                          vector<bool> &iFaceEdgeMarksList,
                          uint iMaterialIndex);

  /** adds a new face to the shape. The difference with the previous method is that this one is
   * designed to build a WingedEdge structure for which there are per vertex normals, opposed to
   * per face normals. returns the built face. iVertexList List of face's vertices. These vertices
   * are not added to the WShape vertex list; they are supposed to be already stored when calling
   * MakeFace. The order in which the vertices are stored in the list determines the face's edges
   * orientation and (so) the face orientation. iMaterialIndex The materialIndex for this face
   *   iNormalsList
   *     The list of normals, iNormalsList[i] corresponding to the normal of the vertex
   * iVertexList[i] for that face. iTexCoordsList The list of texture coords, iTexCoordsList[i]
   * corresponding to the normal of the vertex iVertexList[i] for that face.
   */
  virtual WFace *MakeFace(vector<WVertex *> &iVertexList,
                          vector<Vec3f> &iNormalsList,
                          vector<Vec2f> &iTexCoordsList,
                          vector<bool> &iFaceEdgeMarksList,
                          uint iMaterialIndex);

  inline void AddEdge(WEdge *iEdge)
  {
    _EdgeList.push_back(iEdge);
  }

  inline void AddFace(WFace *iFace)
  {
    _FaceList.push_back(iFace);
  }

  inline void AddVertex(WVertex *iVertex)
  {
    iVertex->setShape(this);
    _VertexList.push_back(iVertex);
  }

  inline void ResetUserData()
  {
    for (vector<WVertex *>::iterator v = _VertexList.begin(), vend = _VertexList.end(); v != vend;
         v++)
    {
      (*v)->ResetUserData();
    }

    for (vector<WEdge *>::iterator e = _EdgeList.begin(), eend = _EdgeList.end(); e != eend; e++) {
      (*e)->ResetUserData();
      // manages WOEdge:
      WOEdge *oe = (*e)->GetaOEdge();
      if (oe) {
        oe->ResetUserData();
      }
      oe = (*e)->GetbOEdge();
      if (oe) {
        oe->ResetUserData();
      }
    }

    for (vector<WFace *>::iterator f = _FaceList.begin(), fend = _FaceList.end(); f != fend; f++) {
      (*f)->ResetUserData();
    }
  }

#if 0
  inline void ComputeBBox()
  {
    _min = _VertexList[0]->GetVertex();
    _max = _VertexList[0]->GetVertex();

    Vec3f v;
    for (vector<WVertex *>::iterator wv = _VertexList.begin(), wvend = _VertexList.end();
         wv != wvend;
         wv++)
    {
      for (uint i = 0; i < 3; i++) {
        v = (*wv)->GetVertex();
        if (v[i] < _min[i]) {
          _min[i] = v[i];
        }
        if (v[i] > _max[i]) {
          _max[i] = v[i];
        }
      }
    }
  }
#endif

#if 0
  inline float ComputeMeanEdgeSize()
  {
    _meanEdgeSize = _meanEdgeSize / _EdgeList.size();
    return _meanEdgeSize;
  }
#else
  real ComputeMeanEdgeSize() const;
#endif

 protected:
  /**
   * Builds the face passed as argument (which as already been allocated)
   * - iVertexList
   *   List of face's vertices. These vertices are not added to the WShape vertex list;
   *   they are supposed to be already stored when calling MakeFace.
   *   The order in which the vertices are stored in the list determines
   *   the face's edges orientation and (so) the face orientation.
   * - iMaterialIndex
   *   The material index for this face
   * - face
   *   The Face that is filled in
   */
  virtual WFace *MakeFace(vector<WVertex *> &iVertexList,
                          vector<bool> &iFaceEdgeMarksList,
                          uint iMaterialIndex,
                          WFace *face);

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WShape")
};

/**********************************
 *                                *
 *                                *
 *          WingedEdge            *
 *                                *
 *                                *
 **********************************/

class WingedEdge {
 public:
  WingedEdge()
  {
    _numFaces = 0;
  }

  ~WingedEdge()
  {
    clear();
  }

  void clear()
  {
    for (vector<WShape *>::iterator it = _wshapes.begin(); it != _wshapes.end(); it++) {
      delete *it;
    }
    _wshapes.clear();
    _numFaces = 0;
  }

  void addWShape(WShape *wshape)
  {
    _wshapes.push_back(wshape);
    _numFaces += wshape->GetFaceList().size();
  }

  vector<WShape *> &getWShapes()
  {
    return _wshapes;
  }

  uint getNumFaces()
  {
    return _numFaces;
  }

 private:
  vector<WShape *> _wshapes;
  uint _numFaces;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WingedEdge")
};

/*
 * #############################################
 * #############################################
 * #############################################
 * ######                                 ######
 * ######   I M P L E M E N T A T I O N   ######
 * ######                                 ######
 * #############################################
 * #############################################
 * #############################################
 */
/* for inline functions */
void WOEdge::RetrieveCWOrderedEdges(vector<WEdge *> &oEdges)
{
  WOEdge *currentOEdge = this;
  do {
    WOEdge *nextOEdge = currentOEdge->GetbFace()->GetNextOEdge(currentOEdge);
    oEdges.push_back(nextOEdge->GetOwner());
    currentOEdge = nextOEdge->GetOwner()->GetOtherOEdge(nextOEdge);
  } while (currentOEdge && (currentOEdge->GetOwner() != GetOwner()));
}

inline void WOEdge::setVecAndAngle()
{
  if (_paVertex && _pbVertex) {
    _vec = _pbVertex->GetVertex() - _paVertex->GetVertex();
    if (_paFace && _pbFace) {
      float sine = (_pbFace->GetNormal() ^ _paFace->GetNormal()) * _vec / _vec.norm();
      if (sine >= 1.0) {
        _angle = M_PI_2;
        return;
      }
      if (sine <= -1.0) {
        _angle = -M_PI_2;
        return;
      }
      _angle = ::asin(sine);
    }
  }
}

} /* namespace Freestyle */
