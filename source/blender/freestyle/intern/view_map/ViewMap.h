/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes to define a View Map (ViewVertex, ViewEdge, etc.)
 */

#include <map>

#include "Interface0D.h"
#include "Interface1D.h"
#include "Silhouette.h"  // defines the embedding

#include "../geometry/GeomUtils.h"

#include "../system/BaseIterator.h"
#include "../system/FreestyleConfig.h"

#include "MEM_guardedalloc.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*             ViewMap            */
/*                                */
/*                                */
/**********************************/

class ViewVertex;
class ViewEdge;
class ViewShape;
class TVertex;

/**
 * Class defining the ViewMap.
 *
 * \note density is the mean area depth value distance to a point.
 */
class ViewMap {
 public:
  typedef vector<ViewEdge *> viewedges_container;
  typedef vector<ViewVertex *> viewvertices_container;
  typedef vector<ViewShape *> viewshapes_container;
  typedef vector<SVertex *> svertices_container;
  typedef vector<FEdge *> fedges_container;
  typedef map<int, int> id_to_index_map;

 private:
  static ViewMap *_pInstance;
  viewshapes_container _VShapes;      // view shapes
  viewedges_container _VEdges;        // view edges
  viewvertices_container _VVertices;  // view vertices
  fedges_container _FEdges;           // feature edges (embedded edges)
  svertices_container _SVertices;     // embedded vertices
  BBox<Vec3r> _scene3DBBox;
  // Mapping between the WShape or VShape id to the VShape index in the _VShapes vector. Used in
  // the method viewShape(int id) to access a shape from its id.
  id_to_index_map _shapeIdToIndex;

 public:
  /** A field that can be used by the user to store any data.
   *  This field must be reset afterwards using ResetUserData().
   */
  void *userdata;

  /** Default constructor. */
  ViewMap()
  {
    _pInstance = this;
    userdata = nullptr;
  }

  /** Destructor. */
  virtual ~ViewMap();

  /** Gets the viewedge the nearest to the 2D position specified as argument */
  const ViewEdge *getClosestViewEdge(real x, real y) const;

  /** Gets the Fedge the nearest to the 2D position specified as argument */
  const FEdge *getClosestFEdge(real x, real y) const;

  /* accessors */
  /** The ViewMap is a singleton class. This static method returns the instance of the ViewMap. */
  static inline ViewMap *getInstance()
  {
    return _pInstance;
  }

  /* Returns the list of ViewShapes of the scene. */
  inline viewshapes_container &ViewShapes()
  {
    return _VShapes;
  }

  /* Returns the list of ViewEdges of the scene. */
  inline viewedges_container &ViewEdges()
  {
    return _VEdges;
  }

  /* Returns the list of ViewVertices of the scene. */
  inline viewvertices_container &ViewVertices()
  {
    return _VVertices;
  }

  /* Returns the list of FEdges of the scene. */
  inline fedges_container &FEdges()
  {
    return _FEdges;
  }

  /* Returns the list of SVertices of the scene. */
  inline svertices_container &SVertices()
  {
    return _SVertices;
  }

  /* Returns an iterator pointing onto the first ViewEdge of the list. */
  inline viewedges_container::iterator viewedges_begin()
  {
    return _VEdges.begin();
  }

  inline viewedges_container::iterator viewedges_end()
  {
    return _VEdges.end();
  }

  inline int viewedges_size()
  {
    return _VEdges.size();
  }

  ViewShape *viewShape(uint id);

  id_to_index_map &shapeIdToIndexMap()
  {
    return _shapeIdToIndex;
  }

  /** Returns the scene 3D bounding box. */
  inline BBox<Vec3r> getScene3dBBox() const
  {
    return _scene3DBBox;
  }

  /* modifiers */
  void AddViewShape(ViewShape *iVShape);

  inline void AddViewEdge(ViewEdge *iVEdge)
  {
    _VEdges.push_back(iVEdge);
  }

  inline void AddViewVertex(ViewVertex *iVVertex)
  {
    _VVertices.push_back(iVVertex);
  }

  inline void AddFEdge(FEdge *iFEdge)
  {
    _FEdges.push_back(iFEdge);
  }

  inline void AddSVertex(SVertex *iSVertex)
  {
    _SVertices.push_back(iSVertex);
  }

  /** Sets the scene 3D bounding box. */
  inline void setScene3dBBox(const BBox<Vec3r> &bbox)
  {
    _scene3DBBox = bbox;
  }

  /* Creates a T vertex in the view map.
   *  A T vertex is the intersection between 2 FEdges (before these are split).
   *  The TVertex is a 2D intersection but it corresponds to a 3D point on each of the 2 FEdges.
   *    iA3D
   *      The 3D coordinates of the point corresponding to the intersection on the first edge.
   *    iA2D
   *      The x,y,z 2D coordinates of the projection of iA3D
   *    iFEdgeA
   *      The first FEdge
   *    iB3D
   *      The 3D coordinates of the point corresponding to the intersection on the second edge.
   *    iB2D
   *      The x,y,z 2D coordinates of the projection of iB3D
   *    iFEdgeB
   *      The second FEdge
   *    id
   *      The id that must be given to that TVertex
   */
  TVertex *CreateTVertex(const Vec3r &iA3D,
                         const Vec3r &iA2D,
                         FEdge *iFEdgeA,
                         const Vec3r &iB3D,
                         const Vec3r &iB2D,
                         FEdge *iFEdgeB,
                         const Id &id);

  /* Updates the structures to take into account the fact that a SVertex must now be considered as
   * a ViewVertex iVertex The SVertex on top of which the ViewVertex is built (it is necessarily a
   * NonTVertex because it is a SVertex) newViewEdges The new ViewEdges that must be add to the
   * ViewMap
   */
  ViewVertex *InsertViewVertex(SVertex *iVertex, vector<ViewEdge *> &newViewEdges);

  /* connects a FEdge to the graph through a SVertex */
  // FEdge *Connect(FEdge *ioEdge, SVertex *ioVertex);

  /* Clean temporary FEdges created by chaining */
  virtual void Clean();

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewMap")
};

/**********************************/
/*                                */
/*                                */
/*             ViewVertex         */
/*                                */
/*                                */
/**********************************/

class ViewEdge;
class SShape;

namespace ViewVertexInternal {

class edge_const_traits;
class edge_nonconst_traits;
template<class Traits> class edge_iterator_base;
class orientedViewEdgeIterator;

}  // namespace ViewVertexInternal

/** Class to define a view vertex.
 *  A view vertex is a feature vertex corresponding to a point of the image graph, where the
 * characteristics of an edge might change (nature, visibility, ...). A ViewVertex can be of two
 * kinds: a TVertex when it corresponds to the intersection between two ViewEdges or a NonTVertex
 * when it corresponds to a vertex of the initial input mesh (it is the case for vertices such as
 * corners for example). Thus, this class can be specialized into two classes, the TVertex class
 * and the NonTVertex class.
 */
class ViewVertex : public Interface0D {
 public:  // Implementation of Interface0D
  /** Returns the string "ViewVertex". */
  virtual string getExactTypeName() const
  {
    return "ViewVertex";
  }

 public:
  friend class ViewShape;
  typedef pair<ViewEdge *, bool> directedViewEdge;  // if bool = true, the ViewEdge is incoming

  typedef vector<directedViewEdge> edges_container;

  typedef ViewVertexInternal::edge_iterator_base<ViewVertexInternal::edge_nonconst_traits>
      edge_iterator;
  typedef ViewVertexInternal::edge_iterator_base<ViewVertexInternal::edge_const_traits>
      const_edge_iterator;

 private:
  Nature::VertexNature _Nature;

 public:
  /** A field that can be used by the user to store any data.
   *  This field must be reset afterwards using ResetUserData().
   */
  void *userdata;

  /** Default constructor. */
  inline ViewVertex()
  {
    userdata = nullptr;
    _Nature = Nature::VIEW_VERTEX;
  }

  inline ViewVertex(Nature::VertexNature nature)
  {
    userdata = nullptr;
    _Nature = Nature::VIEW_VERTEX | nature;
  }

 protected:
  /** Copy constructor. */
  inline ViewVertex(ViewVertex &iBrother)
  {
    _Nature = iBrother._Nature;
    iBrother.userdata = this;
    userdata = nullptr;
  }

  /** Cloning method. */
  virtual ViewVertex *duplicate() = 0;

 public:
  /** Destructor. */
  virtual ~ViewVertex() {}

  /* accessors */
  /** Returns the nature of the vertex. */
  virtual Nature::VertexNature getNature() const
  {
    return _Nature;
  }

  /* modifiers */
  /** Sets the nature of the vertex. */
  inline void setNature(Nature::VertexNature iNature)
  {
    _Nature = iNature;
  }

  /* Replaces old edge by new edge */
  virtual void Replace(ViewEdge *, ViewEdge *) {}

 public:
  /* iterators access */
  // allows iteration on the edges that comes from/goes to this vertex in CCW order (order defined
  // in 2D in the image plan)
  virtual edge_iterator edges_begin() = 0;
  virtual const_edge_iterator edges_begin() const = 0;
  virtual edge_iterator edges_end() = 0;
  virtual const_edge_iterator edges_end() const = 0;
  virtual edge_iterator edges_iterator(ViewEdge *iEdge) = 0;
  virtual const_edge_iterator edges_iterator(ViewEdge *iEdge) const = 0;

  // Iterator access
  /** Returns an iterator over the ViewEdges that goes to or comes from this ViewVertex pointing to
   * the first ViewEdge of the list. The orientedViewEdgeIterator allows to iterate in CCW order
   * over these ViewEdges and to get the orientation for each ViewEdge (incoming/outgoing).
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesBegin() = 0;

  /** Returns an orientedViewEdgeIterator over the ViewEdges around this ViewVertex, pointing after
   * the last ViewEdge.
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesEnd() = 0;

  /** Returns an orientedViewEdgeIterator pointing to the ViewEdge given as argument. */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesIterator(ViewEdge *iEdge) = 0;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewVertex")
};

/**********************************/
/*                                */
/*                                */
/*             TVertex            */
/*                                */
/*                                */
/**********************************/

/** class to define a T vertex, i.e. an intersection between two edges.
 *  It points towards 2 SVertex and 4 View edges.
 *  Among these ViewEdges, 2 are front and 2 are back.
 *  Basically the front edge hides part of the back edge.
 *  So, among the back edges, 1 is of invisibility n and the other of visibility n+1
 */
class TVertex : public ViewVertex {
 public:
  typedef vector<directedViewEdge *> edge_pointers_container;

 public:  // Implementation of Interface0D
  /** Returns the string "TVertex". */
  virtual string getExactTypeName() const
  {
    return "TVertex";
  }

  // Data access methods
  /* Returns the 3D x coordinate of the vertex. Ambiguous in this case. */
  virtual real getX() const
  {
    cerr << "Warning: getX() undefined for this point" << endl;
    return _FrontSVertex->point3D().x();
  }

  virtual real getY() const
  {
    cerr << "Warning: getX() undefined for this point" << endl;
    return _FrontSVertex->point3D().y();
  }

  virtual real getZ() const
  {
    cerr << "Warning: getX() undefined for this point" << endl;
    return _FrontSVertex->point3D().z();
  }

  /** Returns the 3D point. */
  virtual Vec3r getPoint3D() const
  {
    cerr << "Warning: getPoint3D() undefined for this point" << endl;
    return _FrontSVertex->getPoint3D();
  }

  /** Returns the projected 3D x coordinate of the vertex. */
  virtual real getProjectedX() const
  {
    return _FrontSVertex->point2D().x();
  }

  /** Returns the projected 3D y coordinate of the vertex. */
  virtual real getProjectedY() const
  {
    return _FrontSVertex->point2D().y();
  }

  virtual real getProjectedZ() const
  {
    return _FrontSVertex->point2D().z();
  }

  /** Returns the 2D point. */
  virtual Vec2r getPoint2D() const
  {
    return _FrontSVertex->getPoint2D();
  }

  /** Returns the Id of the TVertex. */
  virtual Id getId() const
  {
    return _Id;
  }

  /** Cast the Interface0D in SVertex if it can be. */
  // it can't
  virtual ViewVertex *castToViewVertex()
  {
    return this;
  }

  /** Cast the Interface0D in TVertex if it can be. */
  virtual TVertex *castToTVertex()
  {
    return this;
  }

 private:
  SVertex *_FrontSVertex;
  SVertex *_BackSVertex;
  directedViewEdge _FrontEdgeA;
  directedViewEdge _FrontEdgeB;
  directedViewEdge _BackEdgeA;
  directedViewEdge _BackEdgeB;

  /**
   * ID to identify t vertices.
   * these id will be negative in order not to be mixed with NonTVertex ids.
   */
  Id _Id;
  /** The list of the four ViewEdges, ordered in CCW order (in the image plan). */
  edge_pointers_container _sortedEdges;

 public:
  /** Default constructor. */
  inline TVertex() : ViewVertex(Nature::T_VERTEX)
  {
    _FrontSVertex = nullptr;
    _BackSVertex = nullptr;
    _FrontEdgeA.first = 0;
    _FrontEdgeB.first = 0;
    _BackEdgeA.first = 0;
    _BackEdgeB.first = 0;
  }

  inline TVertex(SVertex *svFront, SVertex *svBack) : ViewVertex(Nature::T_VERTEX)
  {
    _FrontSVertex = svFront;
    _BackSVertex = svBack;
    _FrontEdgeA.first = 0;
    _FrontEdgeB.first = 0;
    _BackEdgeA.first = 0;
    _BackEdgeB.first = 0;
    svFront->setViewVertex(this);
    svBack->setViewVertex(this);
  }

 protected:
  /** Copy constructor. */
  inline TVertex(TVertex &iBrother) : ViewVertex(iBrother)
  {
    _FrontSVertex = iBrother._FrontSVertex;
    _BackSVertex = iBrother._BackSVertex;
    _FrontEdgeA = iBrother._FrontEdgeA;
    _FrontEdgeB = iBrother._FrontEdgeB;
    _BackEdgeA = iBrother._BackEdgeA;
    _BackEdgeB = iBrother._BackEdgeB;
    _sortedEdges = iBrother._sortedEdges;
  }

  /** Cloning method. */
  virtual ViewVertex *duplicate()
  {
    TVertex *clone = new TVertex(*this);
    return clone;
  }

 public:
  /* accessors */
  /** Returns the SVertex that is closer to the viewpoint. */
  inline SVertex *frontSVertex()
  {
    return _FrontSVertex;
  }

  /** Returns the SVertex that is further away from the viewpoint. */
  inline SVertex *backSVertex()
  {
    return _BackSVertex;
  }

  inline directedViewEdge &frontEdgeA()
  {
    return _FrontEdgeA;
  }

  inline directedViewEdge &frontEdgeB()
  {
    return _FrontEdgeB;
  }

  inline directedViewEdge &backEdgeA()
  {
    return _BackEdgeA;
  }

  inline directedViewEdge &backEdgeB()
  {
    return _BackEdgeB;
  }

  /* modifiers */
  /** Sets the SVertex that is closer to the viewpoint. */
  inline void setFrontSVertex(SVertex *iFrontSVertex)
  {
    _FrontSVertex = iFrontSVertex;
    _FrontSVertex->setViewVertex(this);
  }

  /** Sets the SVertex that is further away from the viewpoint. */
  inline void setBackSVertex(SVertex *iBackSVertex)
  {
    _BackSVertex = iBackSVertex;
    _BackSVertex->setViewVertex(this);
  }

  void setFrontEdgeA(ViewEdge *iFrontEdgeA, bool incoming = true);
  void setFrontEdgeB(ViewEdge *iFrontEdgeB, bool incoming = true);
  void setBackEdgeA(ViewEdge *iBackEdgeA, bool incoming = true);
  void setBackEdgeB(ViewEdge *iBackEdgeB, bool incoming = true);

  /** Sets the Id. */
  inline void setId(const Id &iId)
  {
    _Id = iId;
  }

  /** Returns the SVertex (among the 2) belonging to the FEdge iFEdge */
  inline SVertex *getSVertex(FEdge *iFEdge)
  {
    const vector<FEdge *> &vfEdges = _FrontSVertex->fedges();
    vector<FEdge *>::const_iterator fe, fend;
    for (fe = vfEdges.begin(), fend = vfEdges.end(); fe != fend; fe++) {
      if ((*fe) == iFEdge) {
        return _FrontSVertex;
      }
    }

    const vector<FEdge *> &vbEdges = _BackSVertex->fedges();
    for (fe = vbEdges.begin(), fend = vbEdges.end(); fe != fend; fe++) {
      if ((*fe) == iFEdge) {
        return _BackSVertex;
      }
    }
    return nullptr;
  }

  virtual void Replace(ViewEdge *iOld, ViewEdge *iNew);

  /** returns the mate edge of iEdgeA.
   *  For example, if iEdgeA is frontEdgeA, then frontEdgeB is returned. If iEdgeA is frontEdgeB
   * then frontEdgeA is returned. Same for back edges
   */
  virtual ViewEdge *mate(ViewEdge *iEdgeA)
  {
    if (iEdgeA == _FrontEdgeA.first) {
      return _FrontEdgeB.first;
    }
    if (iEdgeA == _FrontEdgeB.first) {
      return _FrontEdgeA.first;
    }
    if (iEdgeA == _BackEdgeA.first) {
      return _BackEdgeB.first;
    }
    if (iEdgeA == _BackEdgeB.first) {
      return _BackEdgeA.first;
    }
    return nullptr;
  }

  /* Iterators access. */

  virtual edge_iterator edges_begin();
  virtual const_edge_iterator edges_begin() const;
  virtual edge_iterator edges_end();
  virtual const_edge_iterator edges_end() const;
  virtual edge_iterator edges_iterator(ViewEdge *iEdge);
  virtual const_edge_iterator edges_iterator(ViewEdge *iEdge) const;

  /** Returns an iterator over the ViewEdges that goes to or comes from this ViewVertex pointing to
   * the first ViewEdge of the list. The orientedViewEdgeIterator allows to iterate in CCW order
   * over these ViewEdges and to get the orientation for each ViewEdge (incoming/outgoing).
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesBegin();

  /** Returns an orientedViewEdgeIterator over the ViewEdges around this ViewVertex, pointing after
   * the last ViewEdge.
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesEnd();

  /** Returns an orientedViewEdgeIterator pointing to the ViewEdge given as argument. */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesIterator(ViewEdge *iEdge);

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:TVertex")
};

/**********************************/
/*                                */
/*                                */
/*             NonTVertex         */
/*                                */
/*                                */
/**********************************/

// (non T vertex)
/** View vertex for corners, cusps, etc...
 *  Associated to a single SVertex.
 *  Can be associated to 2 or several view edges
 */
class NonTVertex : public ViewVertex {
 public:
  typedef vector<directedViewEdge> edges_container;

 public:  // Implementation of Interface0D
  /** Returns the string "ViewVertex". */
  virtual string getExactTypeName() const
  {
    return "NonTVertex";
  }

  // Data access methods
  /** Returns the 3D x coordinate of the vertex. */
  virtual real getX() const
  {
    return _SVertex->point3D().x();
  }

  /** Returns the 3D y coordinate of the vertex. */
  virtual real getY() const
  {
    return _SVertex->point3D().y();
  }

  /** Returns the 3D z coordinate of the vertex. */
  virtual real getZ() const
  {
    return _SVertex->point3D().z();
  }

  /**  Returns the 3D point. */
  virtual Vec3r getPoint3D() const
  {
    return _SVertex->getPoint3D();
  }

  /** Returns the projected 3D. Y coordinate of the vertex. */
  virtual real getProjectedX() const
  {
    return _SVertex->point2D().x();
  }

  /** Returns the projected 3D. Y coordinate of the vertex. */
  virtual real getProjectedY() const
  {
    return _SVertex->point2D().y();
  }

  /** Returns the projected 3D. Z coordinate of the vertex. */
  virtual real getProjectedZ() const
  {
    return _SVertex->point2D().z();
  }

  /** Returns the 2D point. */
  virtual Vec2r getPoint2D() const
  {
    return _SVertex->getPoint2D();
  }

  /** Returns the Id of the vertex. */
  virtual Id getId() const
  {
    return _SVertex->getId();
  }

  /** Cast the Interface0D in SVertex if it can be. */
  virtual SVertex *castToSVertex()
  {
    return _SVertex;
  }

  /** Cast the Interface0D in ViewVertex if it can be. */
  virtual ViewVertex *castToViewVertex()
  {
    return this;
  }

  /** Cast the Interface0D in NonTVertex if it can be. */
  virtual NonTVertex *castToNonTVertex()
  {
    return this;
  }

 private:
  SVertex *_SVertex;
  edges_container _ViewEdges;

 public:
  /** Default constructor. */
  inline NonTVertex() : ViewVertex(Nature::NON_T_VERTEX)
  {
    _SVertex = nullptr;
  }

  /** Builds a NonTVertex from a SVertex. */
  inline NonTVertex(SVertex *iSVertex) : ViewVertex(Nature::NON_T_VERTEX)
  {
    _SVertex = iSVertex;
    _SVertex->setViewVertex(this);
  }

 protected:
  /** Copy constructor. */
  inline NonTVertex(NonTVertex &iBrother) : ViewVertex(iBrother)
  {
    _SVertex = iBrother._SVertex;
    _SVertex->setViewVertex(this);
    _ViewEdges = iBrother._ViewEdges;
  }

  /** Cloning method. */
  virtual ViewVertex *duplicate()
  {
    NonTVertex *clone = new NonTVertex(*this);
    return clone;
  }

 public:
  /** destructor. */
  virtual ~NonTVertex() {}

  /* accessors */
  /** Returns the SVertex on top of which this NonTVertex is built. */
  inline SVertex *svertex()
  {
    return _SVertex;
  }

  inline edges_container &viewedges()
  {
    return _ViewEdges;
  }

  /* modifiers */
  /** Sets the SVertex on top of which this NonTVertex is built. */
  inline void setSVertex(SVertex *iSVertex)
  {
    _SVertex = iSVertex;
    _SVertex->setViewVertex(this);
  }

  inline void setViewEdges(const vector<directedViewEdge> &iViewEdges)
  {
    _ViewEdges = iViewEdges;
  }

  void AddIncomingViewEdge(ViewEdge *iVEdge);
  void AddOutgoingViewEdge(ViewEdge *iVEdge);

  inline void AddViewEdge(ViewEdge *iVEdge, bool incoming = true)
  {
    if (incoming) {
      AddIncomingViewEdge(iVEdge);
    }
    else {
      AddOutgoingViewEdge(iVEdge);
    }
  }

  /* Replaces old edge by new edge */
  virtual void Replace(ViewEdge *iOld, ViewEdge *iNew)
  {
    edges_container::iterator insertedve;
    for (edges_container::iterator ve = _ViewEdges.begin(), vend = _ViewEdges.end(); ve != vend;
         ve++)
    {
      if ((ve)->first == iOld) {
        insertedve = _ViewEdges.insert(
            ve, directedViewEdge(iNew, ve->second));  // inserts e2 before ve.
        // returns an iterator pointing toward e2. ve is invalidated.
        // we want to remove e1, but we can't use ve anymore:
        insertedve++;  // insertedve points now to e1
        _ViewEdges.erase(insertedve);
        return;
      }
    }
  }

  /* Iterators access. */

  virtual edge_iterator edges_begin();
  virtual const_edge_iterator edges_begin() const;
  virtual edge_iterator edges_end();
  virtual const_edge_iterator edges_end() const;
  virtual edge_iterator edges_iterator(ViewEdge *iEdge);
  virtual const_edge_iterator edges_iterator(ViewEdge *iEdge) const;

  /** Returns an iterator over the ViewEdges that goes to or comes from this ViewVertex pointing to
   * the first ViewEdge of the list. The orientedViewEdgeIterator allows to iterate in CCW order
   * over these ViewEdges and to get the orientation for each ViewEdge (incoming/outgoing).
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesBegin();

  /** Returns an orientedViewEdgeIterator over the ViewEdges around this ViewVertex, pointing after
   * the last ViewEdge.
   */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesEnd();

  /** Returns an orientedViewEdgeIterator pointing to the ViewEdge given as argument. */
  virtual ViewVertexInternal::orientedViewEdgeIterator edgesIterator(ViewEdge *iEdge);

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:NonTVertex")
};

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

/* Geometry(normals...)
 * Nature of edges
 * 2D spaces (1or2, material, z...)
 * Parent Shape
 * 3D Shading, material
 * Importance
 * Occluders
 */
class ViewShape;

namespace ViewEdgeInternal {

template<class Traits> class edge_iterator_base;
template<class Traits> class fedge_iterator_base;
template<class Traits> class vertex_iterator_base;

}  // end of namespace ViewEdgeInternal

/** Class defining a ViewEdge. A ViewEdge in an edge of the image graph. it connects two
 * ViewVertex. It is made by connecting a set of FEdges.
 */
class ViewEdge : public Interface1D {
 public:  // Implementation of Interface0D
  /** Returns the string "ViewEdge". */
  virtual string getExactTypeName() const
  {
    return "ViewEdge";
  }

  // Data access methods
  /** Returns the Id of the vertex. */
  virtual Id getId() const
  {
    return _Id;
  }

  /** Returns the nature of the ViewEdge. */
  virtual Nature::EdgeNature getNature() const
  {
    return _Nature;
  }

 public:
  typedef SVertex vertex_type;
  friend class ViewShape;
  // for ViewEdge iterator
  typedef ViewEdgeInternal::edge_iterator_base<Nonconst_traits<ViewEdge *>> edge_iterator;
  typedef ViewEdgeInternal::edge_iterator_base<Const_traits<ViewEdge *>> const_edge_iterator;
  // for fedge iterator
  typedef ViewEdgeInternal::fedge_iterator_base<Nonconst_traits<FEdge *>> fedge_iterator;
  typedef ViewEdgeInternal::fedge_iterator_base<Const_traits<FEdge *>> const_fedge_iterator;
  // for svertex iterator
  typedef ViewEdgeInternal::vertex_iterator_base<Nonconst_traits<SVertex *>> vertex_iterator;
  typedef ViewEdgeInternal::vertex_iterator_base<Const_traits<SVertex *>> const_vertex_iterator;

 private:
  ViewVertex *__A;             // edge starting vertex
  ViewVertex *__B;             // edge ending vertex
  Nature::EdgeNature _Nature;  // nature of view edge
  ViewShape *_Shape;           // shape to which the view edge belongs
  FEdge *_FEdgeA;              // first edge of the embedded fedges chain
  FEdge *_FEdgeB;              // last edge of the embedded fedges chain
  Id _Id;
  uint _ChainingTimeStamp;
  // The silhouette view edge separates two 2D spaces. The one on the left is necessarily the Shape
  // _Shape (the one to which this edge belongs to) and _aShape is the one on its right NOT HANDLED
  // BY THE COPY CONSTRUCTOR
  ViewShape *_aShape;
  int _qi;
  vector<ViewShape *> _Occluders;
  bool _isInImage;

  // tmp
  Id *_splittingId;

 public:
  /** A field that can be used by the user to store any data.
   *  This field must be reset afterwards using ResetUserData().
   */
  void *userdata;

  /** Default constructor. */
  inline ViewEdge()
  {
    __A = nullptr;
    __B = nullptr;
    _FEdgeA = nullptr;
    _FEdgeB = nullptr;
    _ChainingTimeStamp = 0;
    _qi = 0;
    _aShape = nullptr;
    userdata = nullptr;
    _splittingId = nullptr;
    _isInImage = true;
  }

  inline ViewEdge(ViewVertex *iA, ViewVertex *iB)
  {
    __A = iA;
    __B = iB;
    _FEdgeA = nullptr;
    _FEdgeB = nullptr;
    _Shape = 0;
    _ChainingTimeStamp = 0;
    _qi = 0;
    _aShape = nullptr;
    userdata = nullptr;
    _splittingId = nullptr;
    _isInImage = true;
  }

  inline ViewEdge(ViewVertex *iA, ViewVertex *iB, FEdge *iFEdgeA)
  {
    __A = iA;
    __B = iB;
    _FEdgeA = iFEdgeA;
    _FEdgeB = nullptr;
    _Shape = nullptr;
    _ChainingTimeStamp = 0;
    _qi = 0;
    _aShape = nullptr;
    userdata = nullptr;
    _splittingId = nullptr;
    _isInImage = true;
  }

  inline ViewEdge(
      ViewVertex *iA, ViewVertex *iB, FEdge *iFEdgeA, FEdge *iFEdgeB, ViewShape *iShape)
  {
    __A = iA;
    __B = iB;
    _FEdgeA = iFEdgeA;
    _FEdgeB = iFEdgeB;
    _Shape = iShape;
    _ChainingTimeStamp = 0;
    _qi = 0;
    _aShape = nullptr;
    userdata = nullptr;
    _splittingId = nullptr;
    _isInImage = true;
    UpdateFEdges();  // tells every FEdge between iFEdgeA and iFEdgeB that this is theit ViewEdge
  }

  // soc protected:
  /** Copy constructor. */
  inline ViewEdge(ViewEdge &iBrother)
  {
    __A = iBrother.__A;
    __B = iBrother.__B;
    _FEdgeA = iBrother._FEdgeA;
    _FEdgeB = iBrother._FEdgeB;
    _Nature = iBrother._Nature;
    _Shape = nullptr;
    _Id = iBrother._Id;
    _ChainingTimeStamp = iBrother._ChainingTimeStamp;
    _aShape = iBrother._aShape;
    _qi = iBrother._qi;
    _splittingId = nullptr;
    _isInImage = iBrother._isInImage;
    iBrother.userdata = this;
    userdata = nullptr;
  }

  /** Cloning method. */
  virtual ViewEdge *duplicate()
  {
    ViewEdge *clone = new ViewEdge(*this);
    return clone;
  }

 public:
  /** Destructor. */
  virtual ~ViewEdge()
  {
#if 0
    if (_aFace) {
      delete _aFace;
      _aFace = nullptr;
    }
#endif
    // only the last split deletes this id
    if (_splittingId) {
      if (*_splittingId == _Id) {
        delete _splittingId;
      }
    }
  }

  /* accessors */
  /** Returns the first ViewVertex. */
  inline ViewVertex *A()
  {
    return __A;
  }

  /** Returns the second ViewVertex. */
  inline ViewVertex *B()
  {
    return __B;
  }

  /** Returns the first FEdge that constitutes this ViewEdge. */
  inline FEdge *fedgeA()
  {
    return _FEdgeA;
  }

  /** Returns the last FEdge that constitutes this ViewEdge. */
  inline FEdge *fedgeB()
  {
    return _FEdgeB;
  }

  /** Returns the ViewShape to which this ViewEdge belongs to. */
  inline ViewShape *viewShape()
  {
    return _Shape;
  }

  /** Returns the shape that is occluded by the ViewShape to which this ViewEdge belongs to. If no
   * object is occluded, nullptr is returned. \return The occluded ViewShape.
   */
  inline ViewShape *aShape()
  {
    return _aShape;
  }

  /** Tells whether this ViewEdge forms a closed loop or not. */
  inline bool isClosed()
  {
    if (!__B) {
      return true;
    }
    return false;
  }

  /** Returns the time stamp of this ViewEdge. */
  inline uint getChainingTimeStamp()
  {
    return _ChainingTimeStamp;
  }

  inline const ViewShape *aShape() const
  {
    return _aShape;
  }

  inline const ViewShape *bShape() const
  {
    return _Shape;
  }

  inline vector<ViewShape *> &occluders()
  {
    return _Occluders;
  }

  inline Id *splittingId()
  {
    return _splittingId;
  }

  inline bool isInImage() const
  {
    return _isInImage;
  }

  /* modifiers */
  /** Sets the first ViewVertex of the ViewEdge. */
  inline void setA(ViewVertex *iA)
  {
    __A = iA;
  }

  /** Sets the last ViewVertex of the ViewEdge. */
  inline void setB(ViewVertex *iB)
  {
    __B = iB;
  }

  /** Sets the nature of the ViewEdge. */
  inline void setNature(Nature::EdgeNature iNature)
  {
    _Nature = iNature;
  }

  /** Sets the first FEdge of the ViewEdge. */
  inline void setFEdgeA(FEdge *iFEdge)
  {
    _FEdgeA = iFEdge;
  }

  /** Sets the last FEdge of the ViewEdge. */
  inline void setFEdgeB(FEdge *iFEdge)
  {
    _FEdgeB = iFEdge;
  }

  /** Sets the ViewShape to which this ViewEdge belongs to. */
  inline void setShape(ViewShape *iVShape)
  {
    _Shape = iVShape;
  }

  /** Sets the ViewEdge id. */
  inline void setId(const Id &id)
  {
    _Id = id;
  }

  /** Sets Viewedge to this for all embedded fedges */
  void UpdateFEdges();

  /** Sets the occluded ViewShape */
  inline void setaShape(ViewShape *iShape)
  {
    _aShape = iShape;
  }

  /** Sets the quantitative invisibility value. */
  inline void setQI(int qi)
  {
    _qi = qi;
  }

  /** Sets the time stamp value. */
  inline void setChainingTimeStamp(uint ts)
  {
    _ChainingTimeStamp = ts;
  }

  inline void AddOccluder(ViewShape *iShape)
  {
    _Occluders.push_back(iShape);
  }

  inline void setSplittingId(Id *id)
  {
    _splittingId = id;
  }

  inline void setIsInImage(bool iFlag)
  {
    _isInImage = iFlag;
  }

  /* stroke interface definition */
  inline bool intersect_2d_area(const Vec2r &iMin, const Vec2r &iMax) const
  {
    // parse edges to check if one of them is intersection the region:
    FEdge *current = _FEdgeA;
    do {
      if (GeomUtils::intersect2dSeg2dArea(
              iMin,
              iMax,
              Vec2r(current->vertexA()->point2D()[0], current->vertexA()->point2D()[1]),
              Vec2r(current->vertexB()->point2D()[0], current->vertexB()->point2D()[1])))
      {
        return true;
      }
      current = current->nextEdge();
    } while ((current != 0) && (current != _FEdgeA));

    return false;
  }

  inline bool include_in_2d_area(const Vec2r &iMin, const Vec2r &iMax) const
  {
    // parse edges to check if all of them are intersection the region:
    FEdge *current = _FEdgeA;

    do {
      if (!GeomUtils::include2dSeg2dArea(
              iMin,
              iMax,
              Vec2r(current->vertexA()->point2D()[0], current->vertexA()->point2D()[1]),
              Vec2r(current->vertexB()->point2D()[0], current->vertexB()->point2D()[1])))
      {
        return false;
      }
      current = current->nextEdge();
    } while ((current != 0) && (current != _FEdgeA));

    return true;
  }

  /* Information access interface */

#if 0
  inline Nature::EdgeNature viewedge_nature() const
  {
    return getNature();
  }

  float viewedge_length() const;
#endif

  /** Returns the 2D length of the Viewedge. */
  real getLength2D() const;

#if 0
  inline Material material() const
  {
    return _FEdgeA->vertexA()->shape()->material();
  }
#endif

  inline int qi() const
  {
    return _qi;
  }

  inline occluder_container::const_iterator occluders_begin() const
  {
    return _Occluders.begin();
  }

  inline occluder_container::const_iterator occluders_end() const
  {
    return _Occluders.end();
  }

  inline int occluders_size() const
  {
    return _Occluders.size();
  }

  inline bool occluders_empty() const
  {
    return _Occluders.empty();
  }

  inline const Polygon3r &occludee() const
  {
    return (_FEdgeA->aFace());
  }

  inline const SShape *occluded_shape() const;

  inline bool occludee_empty() const
  {
    if (_aShape == 0) {
      return true;
    }
    return false;
  }

  // inline real z_discontinuity(int iCombination = 0) const;

  inline Id shape_id() const
  {
    return _FEdgeA->vertexA()->shape()->getId();
  }

  inline const SShape *shape() const
  {
    return _FEdgeA->vertexA()->shape();
  }

  inline float shape_importance() const
  {
    return _FEdgeA->shape_importance();
  }

  /* iterators access */
  // view edge iterator
  edge_iterator ViewEdge_iterator();
  const_edge_iterator ViewEdge_iterator() const;
  // feature edge iterator
  fedge_iterator fedge_iterator_begin();
  const_fedge_iterator fedge_iterator_begin() const;
  fedge_iterator fedge_iterator_last();
  const_fedge_iterator fedge_iterator_last() const;
  fedge_iterator fedge_iterator_end();
  const_fedge_iterator fedge_iterator_end() const;
  // embedding vertex iterator
  const_vertex_iterator vertices_begin() const;
  vertex_iterator vertices_begin();
  const_vertex_iterator vertices_last() const;
  vertex_iterator vertices_last();
  const_vertex_iterator vertices_end() const;
  vertex_iterator vertices_end();

  // Iterator access (Interface1D)
  /** Returns an Interface0DIterator to iterate over the SVertex constituting the embedding of this
   * ViewEdge. The returned Interface0DIterator points to the first SVertex of the ViewEdge.
   */
  virtual Interface0DIterator verticesBegin();

  /** Returns an Interface0DIterator to iterate over the SVertex constituting the embedding of this
   * ViewEdge. The returned Interface0DIterator points after the last SVertex of the ViewEdge.
   */
  virtual Interface0DIterator verticesEnd();

  /** Returns an Interface0DIterator to iterate over the points of this ViewEdge at a given
   * resolution. The returned Interface0DIterator points on the first Point of the ViewEdge.
   *  \param t:
   *    the sampling value.
   */
  virtual Interface0DIterator pointsBegin(float t = 0.0f);

  /** Returns an Interface0DIterator to iterate over the points of this ViewEdge at a given
   * resolution. The returned Interface0DIterator points after the last Point of the ViewEdge.
   *  \param t:
   *    the sampling value.
   */
  virtual Interface0DIterator pointsEnd(float t = 0.0f);

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewEdge")
};

/**********************************/
/*                                */
/*                                */
/*             ViewShape          */
/*                                */
/*                                */
/**********************************/

/** Class gathering the elements of the ViewMap (ViewVertex, ViewEdge) that are issued from the
 * same input shape. */
class ViewShape {
 private:
  vector<ViewVertex *> _Vertices;
  vector<ViewEdge *> _Edges;
  SShape *_SShape;

 public:
  /** A field that can be used by the user to store any data.
   *  This field must be reset afterwards using ResetUserData().
   */
  void *userdata;

  /** Default constructor. */
  inline ViewShape()
  {
    userdata = nullptr;
    _SShape = nullptr;
  }

  /** Builds a ViewShape from a SShape. */
  inline ViewShape(SShape *iSShape)
  {
    userdata = nullptr;
    _SShape = iSShape;
    //_SShape->setViewShape(this);
  }

  /** Copy constructor. */
  inline ViewShape(ViewShape &iBrother)
  {
    userdata = nullptr;
    vector<ViewVertex *>::iterator vv, vvend;
    vector<ViewEdge *>::iterator ve, veend;

    _SShape = iBrother._SShape;

    vector<ViewVertex *> &vvertices = iBrother.vertices();
    // duplicate vertices
    for (vv = vvertices.begin(), vvend = vvertices.end(); vv != vvend; vv++) {
      ViewVertex *newVertex = (*vv)->duplicate();
      AddVertex(newVertex);
    }

    vector<ViewEdge *> &vvedges = iBrother.edges();
    // duplicate edges
    for (ve = vvedges.begin(), veend = vvedges.end(); ve != veend; ve++) {
      ViewEdge *newEdge = (*ve)->duplicate();
      AddEdge(newEdge);  // here the shape is set as the edge's shape
    }

    //-------------------------
    // remap edges in vertices:
    //-------------------------
    for (vv = _Vertices.begin(), vvend = _Vertices.end(); vv != vvend; vv++) {
      switch ((*vv)->getNature()) {
        case Nature::T_VERTEX: {
          TVertex *v = (TVertex *)(*vv);
          ViewEdge *veFrontA = (ViewEdge *)(v)->frontEdgeA().first->userdata;
          ViewEdge *veFrontB = (ViewEdge *)(v)->frontEdgeB().first->userdata;
          ViewEdge *veBackA = (ViewEdge *)(v)->backEdgeA().first->userdata;
          ViewEdge *veBackB = (ViewEdge *)(v)->backEdgeB().first->userdata;

          v->setFrontEdgeA(veFrontA, v->frontEdgeA().second);
          v->setFrontEdgeB(veFrontB, v->frontEdgeB().second);
          v->setBackEdgeA(veBackA, v->backEdgeA().second);
          v->setBackEdgeB(veBackB, v->backEdgeB().second);
          break;
        }
        case Nature::NON_T_VERTEX: {
          NonTVertex *v = (NonTVertex *)(*vv);
          vector<ViewVertex::directedViewEdge> &vedges = (v)->viewedges();
          vector<ViewVertex::directedViewEdge> newEdges;
          for (vector<ViewVertex::directedViewEdge>::iterator ve = vedges.begin(),
                                                              veend = vedges.end();
               ve != veend;
               ve++)
          {
            ViewEdge *current = (ViewEdge *)((ve)->first)->userdata;
            newEdges.push_back(ViewVertex::directedViewEdge(current, ve->second));
          }
          (v)->setViewEdges(newEdges);
        } break;
        default:
          break;
      }
    }

    //-------------------------------------
    // remap vertices in edges:
    //-------------------------------------
    for (ve = _Edges.begin(), veend = _Edges.end(); ve != veend; ve++) {
      (*ve)->setA((ViewVertex *)((*ve)->A()->userdata));
      (*ve)->setB((ViewVertex *)((*ve)->B()->userdata));
      //---------------------------------------
      // Update all embedded FEdges
      //---------------------------------------
      (*ve)->UpdateFEdges();
    }

    // reset all brothers userdata to nullptr:
    //-------------------------------------
    //---------
    // vertices
    //---------
    for (vv = vvertices.begin(), vvend = vvertices.end(); vv != vvend; vv++) {
      (*vv)->userdata = nullptr;
    }

    //------
    // edges
    //------
    for (ve = vvedges.begin(), veend = vvedges.end(); ve != veend; ve++) {
      (*ve)->userdata = nullptr;
    }
  }

  /** Cloning method. */
  virtual ViewShape *duplicate()
  {
    ViewShape *clone = new ViewShape(*this);
    return clone;
  }

  /** Destructor. */
  virtual ~ViewShape();

  /* splits a view edge into several view edges.
   *    fe
   *      The FEdge that gets split
   *    iViewVertices
   *      The view vertices corresponding to the different intersections for the edge fe.
   *      This list need to be sorted such as the first view vertex is the farther away from
   * fe->vertexA. ioNewEdges The feature edges that are newly created (the initial edges are not
   * included) are added to this list. ioNewViewEdges The view edges that are newly created (the
   * initial edges are not included) are added to this list.
   */
  inline void SplitEdge(FEdge *fe,
                        const vector<TVertex *> &iViewVertices,
                        vector<FEdge *> &ioNewEdges,
                        vector<ViewEdge *> &ioNewViewEdges);

  /* accessors */
  /** Returns the SShape on top of which this ViewShape is built. */
  inline SShape *sshape()
  {
    return _SShape;
  }

  /** Returns the SShape on top of which this ViewShape is built. */
  inline const SShape *sshape() const
  {
    return _SShape;
  }

  /** Returns the list of ViewVertex contained in this ViewShape. */
  inline vector<ViewVertex *> &vertices()
  {
    return _Vertices;
  }

  /** Returns the list of ViewEdge contained in this ViewShape. */
  inline vector<ViewEdge *> &edges()
  {
    return _Edges;
  }

  /** Returns the ViewShape id. */
  inline Id getId() const
  {
    return _SShape->getId();
  }

  /** Returns the ViewShape name. */
  inline const string &getName() const
  {
    return _SShape->getName();
  }

  /** Returns the ViewShape library path. */
  inline const string &getLibraryPath() const
  {
    return _SShape->getLibraryPath();
  }

  /* modifiers */
  /** Sets the SShape on top of which the ViewShape is built. */
  inline void setSShape(SShape *iSShape)
  {
    _SShape = iSShape;
  }

  /** Sets the list of ViewVertex contained in this ViewShape. */
  inline void setVertices(const vector<ViewVertex *> &iVertices)
  {
    _Vertices = iVertices;
  }

  /** Sets the list of ViewEdge contained in this ViewShape. */
  inline void setEdges(const vector<ViewEdge *> &iEdges)
  {
    _Edges = iEdges;
  }

  /** Adds a ViewVertex to the list. */
  inline void AddVertex(ViewVertex *iVertex)
  {
    _Vertices.push_back(iVertex);
    //_SShape->AddNewVertex(iVertex->svertex());
  }

  /** Adds a ViewEdge to the list */
  inline void AddEdge(ViewEdge *iEdge)
  {
    _Edges.push_back(iEdge);
    iEdge->setShape(this);
    //_SShape->AddNewEdge(iEdge->fedge());
  }

  /* removes the view edge iViewEdge in the View Shape and the associated FEdge chain entry in the
   * underlying SShape
   */
  void RemoveEdge(ViewEdge *iViewEdge);

  /* removes the view vertex iViewVertex in the View Shape. */
  void RemoveVertex(ViewVertex *iViewVertex);

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewShape")
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

void ViewShape::SplitEdge(FEdge *fe,
                          const vector<TVertex *> &iViewVertices,
                          vector<FEdge *> &ioNewEdges,
                          vector<ViewEdge *> &ioNewViewEdges)
{
  ViewEdge *vEdge = fe->viewedge();

  // We first need to sort the view vertices from farther to closer to fe->vertexA
  SVertex *sv, *sv2;
  ViewVertex *vva, *vvb;
  vector<TVertex *>::const_iterator vv, vvend;
  for (vv = iViewVertices.begin(), vvend = iViewVertices.end(); vv != vvend; vv++) {
    // Add the viewvertices to the ViewShape
    AddVertex((*vv));

    // retrieve the correct SVertex from the view vertex
    //--------------------------------------------------
    sv = (*vv)->frontSVertex();
    sv2 = (*vv)->backSVertex();

    if (sv->shape() != sv2->shape()) {
      if (sv->shape() != _SShape) {
        sv = sv2;
      }
    }
    else {
      // if the shape is the same we can safely differ the two vertices using their ids:
      if (sv->getId() != fe->vertexA()->getId()) {
        sv = sv2;
      }
    }

    vva = vEdge->A();
    vvb = vEdge->B();

    // We split Fedge AB into AA' and A'B. A' and A'B are created.
    // AB becomes (address speaking) AA'. B is updated.
    //--------------------------------------------------
    SShape *shape = fe->shape();

    // a new edge, A'B is created.
    FEdge *newEdge = shape->SplitEdgeIn2(fe, sv);
    /* One of the two FEdges (fe and newEdge) may have a 2D length less than M_EPSILON.
     * (22 Feb 2011, T.K.)
     */

    ioNewEdges.push_back(newEdge);
    ViewEdge *newVEdge;

    if ((vva == 0) || (vvb == 0)) {  // that means we're dealing with a closed viewedge (loop)
      // remove the chain that was starting by the fedge A of vEdge (which is different from fe
      // !!!!)
      shape->RemoveEdgeFromChain(vEdge->fedgeA());
      // we set
      vEdge->setA(*vv);
      vEdge->setB(*vv);
      vEdge->setFEdgeA(newEdge);
      // FEdge *previousEdge = newEdge->previousEdge();
      vEdge->setFEdgeB(fe);
      newVEdge = vEdge;
      vEdge->fedgeA()->setViewEdge(newVEdge);
    }
    else {
      // while we create the view edge, it updates the "ViewEdge" pointer of every underlying
      // FEdges to this.
      newVEdge = new ViewEdge((*vv), vvb);  //, newEdge, vEdge->fedgeB());
      newVEdge->setNature((fe)->getNature());
      newVEdge->setFEdgeA(newEdge);
      // newVEdge->setFEdgeB(fe);
      // If our original viewedge is made of one FEdge, then
      if ((vEdge->fedgeA() == vEdge->fedgeB()) || (fe == vEdge->fedgeB())) {
        newVEdge->setFEdgeB(newEdge);
      }
      else {
        newVEdge->setFEdgeB(vEdge->fedgeB());  // MODIF
      }

      Id *newId = vEdge->splittingId();
      if (newId == 0) {
        newId = new Id(vEdge->getId());
        vEdge->setSplittingId(newId);
      }
      newId->setSecond(newId->getSecond() + 1);
      newVEdge->setId(*newId);
      newVEdge->setSplittingId(newId);
#if 0
      Id id(vEdge->getId().getFirst(), vEdge->getId().getSecond() + 1);
      newVEdge->setId(vEdge->getId());
      vEdge->setId(id);
#endif

      AddEdge(newVEdge);  // here this shape is set as the edge's shape

      // add new edge to the list of new edges passed as argument:
      ioNewViewEdges.push_back(newVEdge);

      if (0 != vvb) {
        vvb->Replace((vEdge), newVEdge);
      }

      // we split the view edge:
      vEdge->setB((*vv));
      vEdge->setFEdgeB(fe);  // MODIF

      // Update fedges so that they point to the new viewedge:
      newVEdge->UpdateFEdges();
    }
    // check whether this vertex is a front vertex or a back one
    if (sv == (*vv)->frontSVertex()) {
      // -- View Vertex A' --
      (*vv)->setFrontEdgeA(vEdge, true);
      (*vv)->setFrontEdgeB(newVEdge, false);
    }
    else {
      // -- View Vertex A' --
      (*vv)->setBackEdgeA(vEdge, true);
      (*vv)->setBackEdgeB(newVEdge, false);
    }
  }
}

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

#if 0
inline Vec3r ViewEdge::orientation2d(int iCombination) const
{
  return edge_orientation2d_function<ViewEdge>(*this, iCombination);
}

inline Vec3r ViewEdge::orientation3d(int iCombination) const
{
  return edge_orientation3d_function<ViewEdge>(*this, iCombination);
}

inline real ViewEdge::z_discontinuity(int iCombination) const
{
  return z_discontinuity_edge_function<ViewEdge>(*this, iCombination);
}

inline float ViewEdge::local_average_depth(int iCombination) const
{
  return local_average_depth_edge_function<ViewEdge>(*this, iCombination);
}

inline float ViewEdge::local_depth_variance(int iCombination) const
{
  return local_depth_variance_edge_function<ViewEdge>(*this, iCombination);
}

inline real ViewEdge::local_average_density(float sigma, int iCombination) const
{
  return density_edge_function<ViewEdge>(*this, iCombination);
}
#endif

inline const SShape *ViewEdge::occluded_shape() const
{
  if (0 == _aShape) {
    return 0;
  }
  return _aShape->sshape();
}

#if 0
inline Vec3r ViewEdge::curvature2d_as_vector(int iCombination) const
{
  return curvature2d_as_vector_edge_function<ViewEdge>(*this, iCombination);
}

inline real ViewEdge::curvature2d_as_angle(int iCombination) const
{
  return curvature2d_as_angle_edge_function<ViewEdge>(*this, iCombination);
}
#endif

} /* namespace Freestyle */
