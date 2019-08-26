/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_GRAPH_TO_EPS_H
#define LEMON_GRAPH_TO_EPS_H

#include<iostream>
#include<fstream>
#include<sstream>
#include<algorithm>
#include<vector>

#ifndef WIN32
#include<sys/time.h>
#include<ctime>
#else
#include<lemon/bits/windows.h>
#endif

#include<lemon/math.h>
#include<lemon/core.h>
#include<lemon/dim2.h>
#include<lemon/maps.h>
#include<lemon/color.h>
#include<lemon/bits/bezier.h>
#include<lemon/error.h>


///\ingroup eps_io
///\file
///\brief A well configurable tool for visualizing graphs

namespace lemon {

  namespace _graph_to_eps_bits {
    template<class MT>
    class _NegY {
    public:
      typedef typename MT::Key Key;
      typedef typename MT::Value Value;
      const MT &map;
      int yscale;
      _NegY(const MT &m,bool b) : map(m), yscale(1-b*2) {}
      Value operator[](Key n) { return Value(map[n].x,map[n].y*yscale);}
    };
  }

///Default traits class of GraphToEps

///Default traits class of \ref GraphToEps.
///
///\param GR is the type of the underlying graph.
template<class GR>
struct DefaultGraphToEpsTraits
{
  typedef GR Graph;
  typedef GR Digraph;
  typedef typename Graph::Node Node;
  typedef typename Graph::NodeIt NodeIt;
  typedef typename Graph::Arc Arc;
  typedef typename Graph::ArcIt ArcIt;
  typedef typename Graph::InArcIt InArcIt;
  typedef typename Graph::OutArcIt OutArcIt;


  const Graph &g;

  std::ostream& os;

  typedef ConstMap<typename Graph::Node,dim2::Point<double> > CoordsMapType;
  CoordsMapType _coords;
  ConstMap<typename Graph::Node,double > _nodeSizes;
  ConstMap<typename Graph::Node,int > _nodeShapes;

  ConstMap<typename Graph::Node,Color > _nodeColors;
  ConstMap<typename Graph::Arc,Color > _arcColors;

  ConstMap<typename Graph::Arc,double > _arcWidths;

  double _arcWidthScale;

  double _nodeScale;
  double _xBorder, _yBorder;
  double _scale;
  double _nodeBorderQuotient;

  bool _drawArrows;
  double _arrowLength, _arrowWidth;

  bool _showNodes, _showArcs;

  bool _enableParallel;
  double _parArcDist;

  bool _showNodeText;
  ConstMap<typename Graph::Node,bool > _nodeTexts;
  double _nodeTextSize;

  bool _showNodePsText;
  ConstMap<typename Graph::Node,bool > _nodePsTexts;
  char *_nodePsTextsPreamble;

  bool _undirected;

  bool _pleaseRemoveOsStream;

  bool _scaleToA4;

  std::string _title;
  std::string _copyright;

  enum NodeTextColorType
    { DIST_COL=0, DIST_BW=1, CUST_COL=2, SAME_COL=3 } _nodeTextColorType;
  ConstMap<typename Graph::Node,Color > _nodeTextColors;

  bool _autoNodeScale;
  bool _autoArcWidthScale;

  bool _absoluteNodeSizes;
  bool _absoluteArcWidths;

  bool _negY;

  bool _preScale;
  ///Constructor

  ///Constructor
  ///\param gr  Reference to the graph to be printed.
  ///\param ost Reference to the output stream.
  ///By default, it is <tt>std::cout</tt>.
  ///\param pros If it is \c true, then the \c ostream referenced by \c os
  ///will be explicitly deallocated by the destructor.
  DefaultGraphToEpsTraits(const GR &gr, std::ostream& ost = std::cout,
                          bool pros = false) :
    g(gr), os(ost),
    _coords(dim2::Point<double>(1,1)), _nodeSizes(1), _nodeShapes(0),
    _nodeColors(WHITE), _arcColors(BLACK),
    _arcWidths(1.0), _arcWidthScale(0.003),
    _nodeScale(.01), _xBorder(10), _yBorder(10), _scale(1.0),
    _nodeBorderQuotient(.1),
    _drawArrows(false), _arrowLength(1), _arrowWidth(0.3),
    _showNodes(true), _showArcs(true),
    _enableParallel(false), _parArcDist(1),
    _showNodeText(false), _nodeTexts(false), _nodeTextSize(1),
    _showNodePsText(false), _nodePsTexts(false), _nodePsTextsPreamble(0),
    _undirected(lemon::UndirectedTagIndicator<GR>::value),
    _pleaseRemoveOsStream(pros), _scaleToA4(false),
    _nodeTextColorType(SAME_COL), _nodeTextColors(BLACK),
    _autoNodeScale(false),
    _autoArcWidthScale(false),
    _absoluteNodeSizes(false),
    _absoluteArcWidths(false),
    _negY(false),
    _preScale(true)
  {}
};

///Auxiliary class to implement the named parameters of \ref graphToEps()

///Auxiliary class to implement the named parameters of \ref graphToEps().
///
///For detailed examples see the \ref graph_to_eps_demo.cc demo file.
template<class T> class GraphToEps : public T
{
  // Can't believe it is required by the C++ standard
  using T::g;
  using T::os;

  using T::_coords;
  using T::_nodeSizes;
  using T::_nodeShapes;
  using T::_nodeColors;
  using T::_arcColors;
  using T::_arcWidths;

  using T::_arcWidthScale;
  using T::_nodeScale;
  using T::_xBorder;
  using T::_yBorder;
  using T::_scale;
  using T::_nodeBorderQuotient;

  using T::_drawArrows;
  using T::_arrowLength;
  using T::_arrowWidth;

  using T::_showNodes;
  using T::_showArcs;

  using T::_enableParallel;
  using T::_parArcDist;

  using T::_showNodeText;
  using T::_nodeTexts;
  using T::_nodeTextSize;

  using T::_showNodePsText;
  using T::_nodePsTexts;
  using T::_nodePsTextsPreamble;

  using T::_undirected;

  using T::_pleaseRemoveOsStream;

  using T::_scaleToA4;

  using T::_title;
  using T::_copyright;

  using T::CUST_COL;
  using T::DIST_COL;
  using T::DIST_BW;
  using T::_nodeTextColorType;
  using T::_nodeTextColors;

  using T::_autoNodeScale;
  using T::_autoArcWidthScale;

  using T::_absoluteNodeSizes;
  using T::_absoluteArcWidths;


  using T::_negY;
  using T::_preScale;

  // dradnats ++C eht yb deriuqer si ti eveileb t'naC

  typedef typename T::Graph Graph;
  typedef typename T::Digraph Digraph;
  typedef typename Graph::Node Node;
  typedef typename Graph::NodeIt NodeIt;
  typedef typename Graph::Arc Arc;
  typedef typename Graph::ArcIt ArcIt;
  typedef typename Graph::InArcIt InArcIt;
  typedef typename Graph::OutArcIt OutArcIt;

  static const int INTERPOL_PREC;
  static const double A4HEIGHT;
  static const double A4WIDTH;
  static const double A4BORDER;

  bool dontPrint;

public:
  ///Node shapes

  ///Node shapes.
  ///
  enum NodeShapes {
    /// = 0
    ///\image html nodeshape_0.png
    ///\image latex nodeshape_0.eps "CIRCLE shape (0)" width=2cm
    CIRCLE=0,
    /// = 1
    ///\image html nodeshape_1.png
    ///\image latex nodeshape_1.eps "SQUARE shape (1)" width=2cm
    SQUARE=1,
    /// = 2
    ///\image html nodeshape_2.png
    ///\image latex nodeshape_2.eps "DIAMOND shape (2)" width=2cm
    DIAMOND=2,
    /// = 3
    ///\image html nodeshape_3.png
    ///\image latex nodeshape_3.eps "MALE shape (3)" width=2cm
    MALE=3,
    /// = 4
    ///\image html nodeshape_4.png
    ///\image latex nodeshape_4.eps "FEMALE shape (4)" width=2cm
    FEMALE=4
  };

private:
  class arcLess {
    const Graph &g;
  public:
    arcLess(const Graph &_g) : g(_g) {}
    bool operator()(Arc a,Arc b) const
    {
      Node ai=std::min(g.source(a),g.target(a));
      Node aa=std::max(g.source(a),g.target(a));
      Node bi=std::min(g.source(b),g.target(b));
      Node ba=std::max(g.source(b),g.target(b));
      return ai<bi ||
        (ai==bi && (aa < ba ||
                    (aa==ba && ai==g.source(a) && bi==g.target(b))));
    }
  };
  bool isParallel(Arc e,Arc f) const
  {
    return (g.source(e)==g.source(f)&&
            g.target(e)==g.target(f)) ||
      (g.source(e)==g.target(f)&&
       g.target(e)==g.source(f));
  }
  template<class TT>
  static std::string psOut(const dim2::Point<TT> &p)
    {
      std::ostringstream os;
      os << p.x << ' ' << p.y;
      return os.str();
    }
  static std::string psOut(const Color &c)
    {
      std::ostringstream os;
      os << c.red() << ' ' << c.green() << ' ' << c.blue();
      return os.str();
    }

public:
  GraphToEps(const T &t) : T(t), dontPrint(false) {};

  template<class X> struct CoordsTraits : public T {
  typedef X CoordsMapType;
    const X &_coords;
    CoordsTraits(const T &t,const X &x) : T(t), _coords(x) {}
  };
  ///Sets the map of the node coordinates

  ///Sets the map of the node coordinates.
  ///\param x must be a node map with \ref dim2::Point "dim2::Point<double>" or
  ///\ref dim2::Point "dim2::Point<int>" values.
  template<class X> GraphToEps<CoordsTraits<X> > coords(const X &x) {
    dontPrint=true;
    return GraphToEps<CoordsTraits<X> >(CoordsTraits<X>(*this,x));
  }
  template<class X> struct NodeSizesTraits : public T {
    const X &_nodeSizes;
    NodeSizesTraits(const T &t,const X &x) : T(t), _nodeSizes(x) {}
  };
  ///Sets the map of the node sizes

  ///Sets the map of the node sizes.
  ///\param x must be a node map with \c double (or convertible) values.
  template<class X> GraphToEps<NodeSizesTraits<X> > nodeSizes(const X &x)
  {
    dontPrint=true;
    return GraphToEps<NodeSizesTraits<X> >(NodeSizesTraits<X>(*this,x));
  }
  template<class X> struct NodeShapesTraits : public T {
    const X &_nodeShapes;
    NodeShapesTraits(const T &t,const X &x) : T(t), _nodeShapes(x) {}
  };
  ///Sets the map of the node shapes

  ///Sets the map of the node shapes.
  ///The available shape values
  ///can be found in \ref NodeShapes "enum NodeShapes".
  ///\param x must be a node map with \c int (or convertible) values.
  ///\sa NodeShapes
  template<class X> GraphToEps<NodeShapesTraits<X> > nodeShapes(const X &x)
  {
    dontPrint=true;
    return GraphToEps<NodeShapesTraits<X> >(NodeShapesTraits<X>(*this,x));
  }
  template<class X> struct NodeTextsTraits : public T {
    const X &_nodeTexts;
    NodeTextsTraits(const T &t,const X &x) : T(t), _nodeTexts(x) {}
  };
  ///Sets the text printed on the nodes

  ///Sets the text printed on the nodes.
  ///\param x must be a node map with type that can be pushed to a standard
  ///\c ostream.
  template<class X> GraphToEps<NodeTextsTraits<X> > nodeTexts(const X &x)
  {
    dontPrint=true;
    _showNodeText=true;
    return GraphToEps<NodeTextsTraits<X> >(NodeTextsTraits<X>(*this,x));
  }
  template<class X> struct NodePsTextsTraits : public T {
    const X &_nodePsTexts;
    NodePsTextsTraits(const T &t,const X &x) : T(t), _nodePsTexts(x) {}
  };
  ///Inserts a PostScript block to the nodes

  ///With this command it is possible to insert a verbatim PostScript
  ///block to the nodes.
  ///The PS current point will be moved to the center of the node before
  ///the PostScript block inserted.
  ///
  ///Before and after the block a newline character is inserted so you
  ///don't have to bother with the separators.
  ///
  ///\param x must be a node map with type that can be pushed to a standard
  ///\c ostream.
  ///
  ///\sa nodePsTextsPreamble()
  template<class X> GraphToEps<NodePsTextsTraits<X> > nodePsTexts(const X &x)
  {
    dontPrint=true;
    _showNodePsText=true;
    return GraphToEps<NodePsTextsTraits<X> >(NodePsTextsTraits<X>(*this,x));
  }
  template<class X> struct ArcWidthsTraits : public T {
    const X &_arcWidths;
    ArcWidthsTraits(const T &t,const X &x) : T(t), _arcWidths(x) {}
  };
  ///Sets the map of the arc widths

  ///Sets the map of the arc widths.
  ///\param x must be an arc map with \c double (or convertible) values.
  template<class X> GraphToEps<ArcWidthsTraits<X> > arcWidths(const X &x)
  {
    dontPrint=true;
    return GraphToEps<ArcWidthsTraits<X> >(ArcWidthsTraits<X>(*this,x));
  }

  template<class X> struct NodeColorsTraits : public T {
    const X &_nodeColors;
    NodeColorsTraits(const T &t,const X &x) : T(t), _nodeColors(x) {}
  };
  ///Sets the map of the node colors

  ///Sets the map of the node colors.
  ///\param x must be a node map with \ref Color values.
  ///
  ///\sa Palette
  template<class X> GraphToEps<NodeColorsTraits<X> >
  nodeColors(const X &x)
  {
    dontPrint=true;
    return GraphToEps<NodeColorsTraits<X> >(NodeColorsTraits<X>(*this,x));
  }
  template<class X> struct NodeTextColorsTraits : public T {
    const X &_nodeTextColors;
    NodeTextColorsTraits(const T &t,const X &x) : T(t), _nodeTextColors(x) {}
  };
  ///Sets the map of the node text colors

  ///Sets the map of the node text colors.
  ///\param x must be a node map with \ref Color values.
  ///
  ///\sa Palette
  template<class X> GraphToEps<NodeTextColorsTraits<X> >
  nodeTextColors(const X &x)
  {
    dontPrint=true;
    _nodeTextColorType=CUST_COL;
    return GraphToEps<NodeTextColorsTraits<X> >
      (NodeTextColorsTraits<X>(*this,x));
  }
  template<class X> struct ArcColorsTraits : public T {
    const X &_arcColors;
    ArcColorsTraits(const T &t,const X &x) : T(t), _arcColors(x) {}
  };
  ///Sets the map of the arc colors

  ///Sets the map of the arc colors.
  ///\param x must be an arc map with \ref Color values.
  ///
  ///\sa Palette
  template<class X> GraphToEps<ArcColorsTraits<X> >
  arcColors(const X &x)
  {
    dontPrint=true;
    return GraphToEps<ArcColorsTraits<X> >(ArcColorsTraits<X>(*this,x));
  }
  ///Sets a global scale factor for node sizes

  ///Sets a global scale factor for node sizes.
  ///
  /// If nodeSizes() is not given, this function simply sets the node
  /// sizes to \c d.  If nodeSizes() is given, but
  /// autoNodeScale() is not, then the node size given by
  /// nodeSizes() will be multiplied by the value \c d.
  /// If both nodeSizes() and autoNodeScale() are used, then the
  /// node sizes will be scaled in such a way that the greatest size will be
  /// equal to \c d.
  /// \sa nodeSizes()
  /// \sa autoNodeScale()
  GraphToEps<T> &nodeScale(double d=.01) {_nodeScale=d;return *this;}
  ///Turns on/off the automatic node size scaling.

  ///Turns on/off the automatic node size scaling.
  ///
  ///\sa nodeScale()
  ///
  GraphToEps<T> &autoNodeScale(bool b=true) {
    _autoNodeScale=b;return *this;
  }

  ///Turns on/off the absolutematic node size scaling.

  ///Turns on/off the absolutematic node size scaling.
  ///
  ///\sa nodeScale()
  ///
  GraphToEps<T> &absoluteNodeSizes(bool b=true) {
    _absoluteNodeSizes=b;return *this;
  }

  ///Negates the Y coordinates.
  GraphToEps<T> &negateY(bool b=true) {
    _negY=b;return *this;
  }

  ///Turn on/off pre-scaling

  ///By default, graphToEps() rescales the whole image in order to avoid
  ///very big or very small bounding boxes.
  ///
  ///This (p)rescaling can be turned off with this function.
  ///
  GraphToEps<T> &preScale(bool b=true) {
    _preScale=b;return *this;
  }

  ///Sets a global scale factor for arc widths

  /// Sets a global scale factor for arc widths.
  ///
  /// If arcWidths() is not given, this function simply sets the arc
  /// widths to \c d.  If arcWidths() is given, but
  /// autoArcWidthScale() is not, then the arc withs given by
  /// arcWidths() will be multiplied by the value \c d.
  /// If both arcWidths() and autoArcWidthScale() are used, then the
  /// arc withs will be scaled in such a way that the greatest width will be
  /// equal to \c d.
  GraphToEps<T> &arcWidthScale(double d=.003) {_arcWidthScale=d;return *this;}
  ///Turns on/off the automatic arc width scaling.

  ///Turns on/off the automatic arc width scaling.
  ///
  ///\sa arcWidthScale()
  ///
  GraphToEps<T> &autoArcWidthScale(bool b=true) {
    _autoArcWidthScale=b;return *this;
  }
  ///Turns on/off the absolutematic arc width scaling.

  ///Turns on/off the absolutematic arc width scaling.
  ///
  ///\sa arcWidthScale()
  ///
  GraphToEps<T> &absoluteArcWidths(bool b=true) {
    _absoluteArcWidths=b;return *this;
  }
  ///Sets a global scale factor for the whole picture
  GraphToEps<T> &scale(double d) {_scale=d;return *this;}
  ///Sets the width of the border around the picture
  GraphToEps<T> &border(double b=10) {_xBorder=_yBorder=b;return *this;}
  ///Sets the width of the border around the picture
  GraphToEps<T> &border(double x, double y) {
    _xBorder=x;_yBorder=y;return *this;
  }
  ///Sets whether to draw arrows
  GraphToEps<T> &drawArrows(bool b=true) {_drawArrows=b;return *this;}
  ///Sets the length of the arrowheads
  GraphToEps<T> &arrowLength(double d=1.0) {_arrowLength*=d;return *this;}
  ///Sets the width of the arrowheads
  GraphToEps<T> &arrowWidth(double d=.3) {_arrowWidth*=d;return *this;}

  ///Scales the drawing to fit to A4 page
  GraphToEps<T> &scaleToA4() {_scaleToA4=true;return *this;}

  ///Enables parallel arcs
  GraphToEps<T> &enableParallel(bool b=true) {_enableParallel=b;return *this;}

  ///Sets the distance between parallel arcs
  GraphToEps<T> &parArcDist(double d) {_parArcDist*=d;return *this;}

  ///Hides the arcs
  GraphToEps<T> &hideArcs(bool b=true) {_showArcs=!b;return *this;}
  ///Hides the nodes
  GraphToEps<T> &hideNodes(bool b=true) {_showNodes=!b;return *this;}

  ///Sets the size of the node texts
  GraphToEps<T> &nodeTextSize(double d) {_nodeTextSize=d;return *this;}

  ///Sets the color of the node texts to be different from the node color

  ///Sets the color of the node texts to be as different from the node color
  ///as it is possible.
  GraphToEps<T> &distantColorNodeTexts()
  {_nodeTextColorType=DIST_COL;return *this;}
  ///Sets the color of the node texts to be black or white and always visible.

  ///Sets the color of the node texts to be black or white according to
  ///which is more different from the node color.
  GraphToEps<T> &distantBWNodeTexts()
  {_nodeTextColorType=DIST_BW;return *this;}

  ///Gives a preamble block for node Postscript block.

  ///Gives a preamble block for node Postscript block.
  ///
  ///\sa nodePsTexts()
  GraphToEps<T> & nodePsTextsPreamble(const char *str) {
    _nodePsTextsPreamble=str ;return *this;
  }
  ///Sets whether the graph is undirected

  ///Sets whether the graph is undirected.
  ///
  ///This setting is the default for undirected graphs.
  ///
  ///\sa directed()
   GraphToEps<T> &undirected(bool b=true) {_undirected=b;return *this;}

  ///Sets whether the graph is directed

  ///Sets whether the graph is directed.
  ///Use it to show the edges as a pair of directed ones.
  ///
  ///This setting is the default for digraphs.
  ///
  ///\sa undirected()
  GraphToEps<T> &directed(bool b=true) {_undirected=!b;return *this;}

  ///Sets the title.

  ///Sets the title of the generated image,
  ///namely it inserts a <tt>%%Title:</tt> DSC field to the header of
  ///the EPS file.
  GraphToEps<T> &title(const std::string &t) {_title=t;return *this;}
  ///Sets the copyright statement.

  ///Sets the copyright statement of the generated image,
  ///namely it inserts a <tt>%%Copyright:</tt> DSC field to the header of
  ///the EPS file.
  GraphToEps<T> &copyright(const std::string &t) {_copyright=t;return *this;}

protected:
  bool isInsideNode(dim2::Point<double> p, double r,int t)
  {
    switch(t) {
    case CIRCLE:
    case MALE:
    case FEMALE:
      return p.normSquare()<=r*r;
    case SQUARE:
      return p.x<=r&&p.x>=-r&&p.y<=r&&p.y>=-r;
    case DIAMOND:
      return p.x+p.y<=r && p.x-p.y<=r && -p.x+p.y<=r && -p.x-p.y<=r;
    }
    return false;
  }

public:
  ~GraphToEps() { }

  ///Draws the graph.

  ///Like other functions using
  ///\ref named-templ-func-param "named template parameters",
  ///this function calls the algorithm itself, i.e. in this case
  ///it draws the graph.
  void run() {
    const double EPSILON=1e-9;
    if(dontPrint) return;

    _graph_to_eps_bits::_NegY<typename T::CoordsMapType>
      mycoords(_coords,_negY);

    os << "%!PS-Adobe-2.0 EPSF-2.0\n";
    if(_title.size()>0) os << "%%Title: " << _title << '\n';
     if(_copyright.size()>0) os << "%%Copyright: " << _copyright << '\n';
    os << "%%Creator: LEMON, graphToEps()\n";

    {
      os << "%%CreationDate: ";
#ifndef WIN32
      timeval tv;
      gettimeofday(&tv, 0);

      char cbuf[26];
      ctime_r(&tv.tv_sec,cbuf);
      os << cbuf;
#else
      os << bits::getWinFormattedDate();
      os << std::endl;
#endif
    }

    if (_autoArcWidthScale) {
      double max_w=0;
      for(ArcIt e(g);e!=INVALID;++e)
        max_w=std::max(double(_arcWidths[e]),max_w);
      if(max_w>EPSILON) {
        _arcWidthScale/=max_w;
      }
    }

    if (_autoNodeScale) {
      double max_s=0;
      for(NodeIt n(g);n!=INVALID;++n)
        max_s=std::max(double(_nodeSizes[n]),max_s);
      if(max_s>EPSILON) {
        _nodeScale/=max_s;
      }
    }

    double diag_len = 1;
    if(!(_absoluteNodeSizes&&_absoluteArcWidths)) {
      dim2::Box<double> bb;
      for(NodeIt n(g);n!=INVALID;++n) bb.add(mycoords[n]);
      if (bb.empty()) {
        bb = dim2::Box<double>(dim2::Point<double>(0,0));
      }
      diag_len = std::sqrt((bb.bottomLeft()-bb.topRight()).normSquare());
      if(diag_len<EPSILON) diag_len = 1;
      if(!_absoluteNodeSizes) _nodeScale*=diag_len;
      if(!_absoluteArcWidths) _arcWidthScale*=diag_len;
    }

    dim2::Box<double> bb;
    for(NodeIt n(g);n!=INVALID;++n) {
      double ns=_nodeSizes[n]*_nodeScale;
      dim2::Point<double> p(ns,ns);
      switch(_nodeShapes[n]) {
      case CIRCLE:
      case SQUARE:
      case DIAMOND:
        bb.add(p+mycoords[n]);
        bb.add(-p+mycoords[n]);
        break;
      case MALE:
        bb.add(-p+mycoords[n]);
        bb.add(dim2::Point<double>(1.5*ns,1.5*std::sqrt(3.0)*ns)+mycoords[n]);
        break;
      case FEMALE:
        bb.add(p+mycoords[n]);
        bb.add(dim2::Point<double>(-ns,-3.01*ns)+mycoords[n]);
        break;
      }
    }
    if (bb.empty()) {
      bb = dim2::Box<double>(dim2::Point<double>(0,0));
    }

    if(_scaleToA4)
      os <<"%%BoundingBox: 0 0 596 842\n%%DocumentPaperSizes: a4\n";
    else {
      if(_preScale) {
        //Rescale so that BoundingBox won't be neither to big nor too small.
        while(bb.height()*_scale>1000||bb.width()*_scale>1000) _scale/=10;
        while(bb.height()*_scale<100||bb.width()*_scale<100) _scale*=10;
      }

      os << "%%BoundingBox: "
         << int(floor(bb.left()   * _scale - _xBorder)) << ' '
         << int(floor(bb.bottom() * _scale - _yBorder)) << ' '
         << int(ceil(bb.right()  * _scale + _xBorder)) << ' '
         << int(ceil(bb.top()    * _scale + _yBorder)) << '\n';
    }

    os << "%%EndComments\n";

    //x1 y1 x2 y2 x3 y3 cr cg cb w
    os << "/lb { setlinewidth setrgbcolor newpath moveto\n"
       << "      4 2 roll 1 index 1 index curveto stroke } bind def\n";
    os << "/l { setlinewidth setrgbcolor newpath moveto lineto stroke }"
       << " bind def\n";
    //x y r
    os << "/c { newpath dup 3 index add 2 index moveto 0 360 arc closepath }"
       << " bind def\n";
    //x y r
    os << "/sq { newpath 2 index 1 index add 2 index 2 index add moveto\n"
       << "      2 index 1 index sub 2 index 2 index add lineto\n"
       << "      2 index 1 index sub 2 index 2 index sub lineto\n"
       << "      2 index 1 index add 2 index 2 index sub lineto\n"
       << "      closepath pop pop pop} bind def\n";
    //x y r
    os << "/di { newpath 2 index 1 index add 2 index moveto\n"
       << "      2 index             2 index 2 index add lineto\n"
       << "      2 index 1 index sub 2 index             lineto\n"
       << "      2 index             2 index 2 index sub lineto\n"
       << "      closepath pop pop pop} bind def\n";
    // x y r cr cg cb
    os << "/nc { 0 0 0 setrgbcolor 5 index 5 index 5 index c fill\n"
       << "     setrgbcolor " << 1+_nodeBorderQuotient << " div c fill\n"
       << "   } bind def\n";
    os << "/nsq { 0 0 0 setrgbcolor 5 index 5 index 5 index sq fill\n"
       << "     setrgbcolor " << 1+_nodeBorderQuotient << " div sq fill\n"
       << "   } bind def\n";
    os << "/ndi { 0 0 0 setrgbcolor 5 index 5 index 5 index di fill\n"
       << "     setrgbcolor " << 1+_nodeBorderQuotient << " div di fill\n"
       << "   } bind def\n";
    os << "/nfemale { 0 0 0 setrgbcolor 3 index "
       << _nodeBorderQuotient/(1+_nodeBorderQuotient)
       << " 1.5 mul mul setlinewidth\n"
       << "  newpath 5 index 5 index moveto "
       << "5 index 5 index 5 index 3.01 mul sub\n"
       << "  lineto 5 index 4 index .7 mul sub 5 index 5 index 2.2 mul sub"
       << " moveto\n"
       << "  5 index 4 index .7 mul add 5 index 5 index 2.2 mul sub lineto "
       << "stroke\n"
       << "  5 index 5 index 5 index c fill\n"
       << "  setrgbcolor " << 1+_nodeBorderQuotient << " div c fill\n"
       << "  } bind def\n";
    os << "/nmale {\n"
       << "  0 0 0 setrgbcolor 3 index "
       << _nodeBorderQuotient/(1+_nodeBorderQuotient)
       <<" 1.5 mul mul setlinewidth\n"
       << "  newpath 5 index 5 index moveto\n"
       << "  5 index 4 index 1 mul 1.5 mul add\n"
       << "  5 index 5 index 3 sqrt 1.5 mul mul add\n"
       << "  1 index 1 index lineto\n"
       << "  1 index 1 index 7 index sub moveto\n"
       << "  1 index 1 index lineto\n"
       << "  exch 5 index 3 sqrt .5 mul mul sub exch 5 index .5 mul sub"
       << " lineto\n"
       << "  stroke\n"
       << "  5 index 5 index 5 index c fill\n"
       << "  setrgbcolor " << 1+_nodeBorderQuotient << " div c fill\n"
       << "  } bind def\n";


    os << "/arrl " << _arrowLength << " def\n";
    os << "/arrw " << _arrowWidth << " def\n";
    // l dx_norm dy_norm
    os << "/lrl { 2 index mul exch 2 index mul exch rlineto pop} bind def\n";
    //len w dx_norm dy_norm x1 y1 cr cg cb
    os << "/arr { setrgbcolor /y1 exch def /x1 exch def /dy exch def /dx "
       << "exch def\n"
       << "       /w exch def /len exch def\n"
      //<< "0.1 setlinewidth x1 y1 moveto dx len mul dy len mul rlineto stroke"
       << "       newpath x1 dy w 2 div mul add y1 dx w 2 div mul sub moveto\n"
       << "       len w sub arrl sub dx dy lrl\n"
       << "       arrw dy dx neg lrl\n"
       << "       dx arrl w add mul dy w 2 div arrw add mul sub\n"
       << "       dy arrl w add mul dx w 2 div arrw add mul add rlineto\n"
       << "       dx arrl w add mul neg dy w 2 div arrw add mul sub\n"
       << "       dy arrl w add mul neg dx w 2 div arrw add mul add rlineto\n"
       << "       arrw dy dx neg lrl\n"
       << "       len w sub arrl sub neg dx dy lrl\n"
       << "       closepath fill } bind def\n";
    os << "/cshow { 2 index 2 index moveto dup stringwidth pop\n"
       << "         neg 2 div fosi .35 mul neg rmoveto show pop pop} def\n";

    os << "\ngsave\n";
    if(_scaleToA4)
      if(bb.height()>bb.width()) {
        double sc= std::min((A4HEIGHT-2*A4BORDER)/bb.height(),
                  (A4WIDTH-2*A4BORDER)/bb.width());
        os << ((A4WIDTH -2*A4BORDER)-sc*bb.width())/2 + A4BORDER << ' '
           << ((A4HEIGHT-2*A4BORDER)-sc*bb.height())/2 + A4BORDER
           << " translate\n"
           << sc << " dup scale\n"
           << -bb.left() << ' ' << -bb.bottom() << " translate\n";
      }
      else {
        double sc= std::min((A4HEIGHT-2*A4BORDER)/bb.width(),
                  (A4WIDTH-2*A4BORDER)/bb.height());
        os << ((A4WIDTH -2*A4BORDER)-sc*bb.height())/2 + A4BORDER << ' '
           << ((A4HEIGHT-2*A4BORDER)-sc*bb.width())/2 + A4BORDER
           << " translate\n"
           << sc << " dup scale\n90 rotate\n"
           << -bb.left() << ' ' << -bb.top() << " translate\n";
        }
    else if(_scale!=1.0) os << _scale << " dup scale\n";

    if(_showArcs) {
      os << "%Arcs:\ngsave\n";
      if(_enableParallel) {
        std::vector<Arc> el;
        for(ArcIt e(g);e!=INVALID;++e)
          if((!_undirected||g.source(e)<g.target(e))&&_arcWidths[e]>0
             &&g.source(e)!=g.target(e))
            el.push_back(e);
        std::sort(el.begin(),el.end(),arcLess(g));

        typename std::vector<Arc>::iterator j;
        for(typename std::vector<Arc>::iterator i=el.begin();i!=el.end();i=j) {
          for(j=i+1;j!=el.end()&&isParallel(*i,*j);++j) ;

          double sw=0;
          for(typename std::vector<Arc>::iterator e=i;e!=j;++e)
            sw+=_arcWidths[*e]*_arcWidthScale+_parArcDist;
          sw-=_parArcDist;
          sw/=-2.0;
          dim2::Point<double>
            dvec(mycoords[g.target(*i)]-mycoords[g.source(*i)]);
          double l=std::sqrt(dvec.normSquare());
          dim2::Point<double> d(dvec/std::max(l,EPSILON));
          dim2::Point<double> m;
//           m=dim2::Point<double>(mycoords[g.target(*i)]+
//                                 mycoords[g.source(*i)])/2.0;

//            m=dim2::Point<double>(mycoords[g.source(*i)])+
//             dvec*(double(_nodeSizes[g.source(*i)])/
//                (_nodeSizes[g.source(*i)]+_nodeSizes[g.target(*i)]));

          m=dim2::Point<double>(mycoords[g.source(*i)])+
            d*(l+_nodeSizes[g.source(*i)]-_nodeSizes[g.target(*i)])/2.0;

          for(typename std::vector<Arc>::iterator e=i;e!=j;++e) {
            sw+=_arcWidths[*e]*_arcWidthScale/2.0;
            dim2::Point<double> mm=m+rot90(d)*sw/.75;
            if(_drawArrows) {
              int node_shape;
              dim2::Point<double> s=mycoords[g.source(*e)];
              dim2::Point<double> t=mycoords[g.target(*e)];
              double rn=_nodeSizes[g.target(*e)]*_nodeScale;
              node_shape=_nodeShapes[g.target(*e)];
              dim2::Bezier3 bez(s,mm,mm,t);
              double t1=0,t2=1;
              for(int ii=0;ii<INTERPOL_PREC;++ii)
                if(isInsideNode(bez((t1+t2)/2)-t,rn,node_shape)) t2=(t1+t2)/2;
                else t1=(t1+t2)/2;
              dim2::Point<double> apoint=bez((t1+t2)/2);
              rn = _arrowLength+_arcWidths[*e]*_arcWidthScale;
              rn*=rn;
              t2=(t1+t2)/2;t1=0;
              for(int ii=0;ii<INTERPOL_PREC;++ii)
                if((bez((t1+t2)/2)-apoint).normSquare()>rn) t1=(t1+t2)/2;
                else t2=(t1+t2)/2;
              dim2::Point<double> linend=bez((t1+t2)/2);
              bez=bez.before((t1+t2)/2);
//               rn=_nodeSizes[g.source(*e)]*_nodeScale;
//               node_shape=_nodeShapes[g.source(*e)];
//               t1=0;t2=1;
//               for(int i=0;i<INTERPOL_PREC;++i)
//                 if(isInsideNode(bez((t1+t2)/2)-t,rn,node_shape))
//                   t1=(t1+t2)/2;
//                 else t2=(t1+t2)/2;
//               bez=bez.after((t1+t2)/2);
              os << _arcWidths[*e]*_arcWidthScale << " setlinewidth "
                 << _arcColors[*e].red() << ' '
                 << _arcColors[*e].green() << ' '
                 << _arcColors[*e].blue() << " setrgbcolor newpath\n"
                 << bez.p1.x << ' ' <<  bez.p1.y << " moveto\n"
                 << bez.p2.x << ' ' << bez.p2.y << ' '
                 << bez.p3.x << ' ' << bez.p3.y << ' '
                 << bez.p4.x << ' ' << bez.p4.y << " curveto stroke\n";
              dim2::Point<double> dd(rot90(linend-apoint));
              dd*=(.5*_arcWidths[*e]*_arcWidthScale+_arrowWidth)/
                std::sqrt(dd.normSquare());
              os << "newpath " << psOut(apoint) << " moveto "
                 << psOut(linend+dd) << " lineto "
                 << psOut(linend-dd) << " lineto closepath fill\n";
            }
            else {
              os << mycoords[g.source(*e)].x << ' '
                 << mycoords[g.source(*e)].y << ' '
                 << mm.x << ' ' << mm.y << ' '
                 << mycoords[g.target(*e)].x << ' '
                 << mycoords[g.target(*e)].y << ' '
                 << _arcColors[*e].red() << ' '
                 << _arcColors[*e].green() << ' '
                 << _arcColors[*e].blue() << ' '
                 << _arcWidths[*e]*_arcWidthScale << " lb\n";
            }
            sw+=_arcWidths[*e]*_arcWidthScale/2.0+_parArcDist;
          }
        }
      }
      else for(ArcIt e(g);e!=INVALID;++e)
        if((!_undirected||g.source(e)<g.target(e))&&_arcWidths[e]>0
           &&g.source(e)!=g.target(e)) {
          if(_drawArrows) {
            dim2::Point<double> d(mycoords[g.target(e)]-mycoords[g.source(e)]);
            double rn=_nodeSizes[g.target(e)]*_nodeScale;
            int node_shape=_nodeShapes[g.target(e)];
            double t1=0,t2=1;
            for(int i=0;i<INTERPOL_PREC;++i)
              if(isInsideNode((-(t1+t2)/2)*d,rn,node_shape)) t1=(t1+t2)/2;
              else t2=(t1+t2)/2;
            double l=std::sqrt(d.normSquare());
            d/=l;

            os << l*(1-(t1+t2)/2) << ' '
               << _arcWidths[e]*_arcWidthScale << ' '
               << d.x << ' ' << d.y << ' '
               << mycoords[g.source(e)].x << ' '
               << mycoords[g.source(e)].y << ' '
               << _arcColors[e].red() << ' '
               << _arcColors[e].green() << ' '
               << _arcColors[e].blue() << " arr\n";
          }
          else os << mycoords[g.source(e)].x << ' '
                  << mycoords[g.source(e)].y << ' '
                  << mycoords[g.target(e)].x << ' '
                  << mycoords[g.target(e)].y << ' '
                  << _arcColors[e].red() << ' '
                  << _arcColors[e].green() << ' '
                  << _arcColors[e].blue() << ' '
                  << _arcWidths[e]*_arcWidthScale << " l\n";
        }
      os << "grestore\n";
    }
    if(_showNodes) {
      os << "%Nodes:\ngsave\n";
      for(NodeIt n(g);n!=INVALID;++n) {
        os << mycoords[n].x << ' ' << mycoords[n].y << ' '
           << _nodeSizes[n]*_nodeScale << ' '
           << _nodeColors[n].red() << ' '
           << _nodeColors[n].green() << ' '
           << _nodeColors[n].blue() << ' ';
        switch(_nodeShapes[n]) {
        case CIRCLE:
          os<< "nc";break;
        case SQUARE:
          os<< "nsq";break;
        case DIAMOND:
          os<< "ndi";break;
        case MALE:
          os<< "nmale";break;
        case FEMALE:
          os<< "nfemale";break;
        }
        os<<'\n';
      }
      os << "grestore\n";
    }
    if(_showNodeText) {
      os << "%Node texts:\ngsave\n";
      os << "/fosi " << _nodeTextSize << " def\n";
      os << "(Helvetica) findfont fosi scalefont setfont\n";
      for(NodeIt n(g);n!=INVALID;++n) {
        switch(_nodeTextColorType) {
        case DIST_COL:
          os << psOut(distantColor(_nodeColors[n])) << " setrgbcolor\n";
          break;
        case DIST_BW:
          os << psOut(distantBW(_nodeColors[n])) << " setrgbcolor\n";
          break;
        case CUST_COL:
          os << psOut(distantColor(_nodeTextColors[n])) << " setrgbcolor\n";
          break;
        default:
          os << "0 0 0 setrgbcolor\n";
        }
        os << mycoords[n].x << ' ' << mycoords[n].y
           << " (" << _nodeTexts[n] << ") cshow\n";
      }
      os << "grestore\n";
    }
    if(_showNodePsText) {
      os << "%Node PS blocks:\ngsave\n";
      for(NodeIt n(g);n!=INVALID;++n)
        os << mycoords[n].x << ' ' << mycoords[n].y
           << " moveto\n" << _nodePsTexts[n] << "\n";
      os << "grestore\n";
    }

    os << "grestore\nshowpage\n";

    //CleanUp:
    if(_pleaseRemoveOsStream) {delete &os;}
  }

  ///\name Aliases
  ///These are just some aliases to other parameter setting functions.

  ///@{

  ///An alias for arcWidths()
  template<class X> GraphToEps<ArcWidthsTraits<X> > edgeWidths(const X &x)
  {
    return arcWidths(x);
  }

  ///An alias for arcColors()
  template<class X> GraphToEps<ArcColorsTraits<X> >
  edgeColors(const X &x)
  {
    return arcColors(x);
  }

  ///An alias for arcWidthScale()
  GraphToEps<T> &edgeWidthScale(double d) {return arcWidthScale(d);}

  ///An alias for autoArcWidthScale()
  GraphToEps<T> &autoEdgeWidthScale(bool b=true)
  {
    return autoArcWidthScale(b);
  }

  ///An alias for absoluteArcWidths()
  GraphToEps<T> &absoluteEdgeWidths(bool b=true)
  {
    return absoluteArcWidths(b);
  }

  ///An alias for parArcDist()
  GraphToEps<T> &parEdgeDist(double d) {return parArcDist(d);}

  ///An alias for hideArcs()
  GraphToEps<T> &hideEdges(bool b=true) {return hideArcs(b);}

  ///@}
};

template<class T>
const int GraphToEps<T>::INTERPOL_PREC = 20;
template<class T>
const double GraphToEps<T>::A4HEIGHT = 841.8897637795276;
template<class T>
const double GraphToEps<T>::A4WIDTH  = 595.275590551181;
template<class T>
const double GraphToEps<T>::A4BORDER = 15;


///Generates an EPS file from a graph

///\ingroup eps_io
///Generates an EPS file from a graph.
///\param g Reference to the graph to be printed.
///\param os Reference to the output stream.
///By default, it is <tt>std::cout</tt>.
///
///This function also has a lot of
///\ref named-templ-func-param "named parameters",
///they are declared as the members of class \ref GraphToEps. The following
///example shows how to use these parameters.
///\code
/// graphToEps(g,os).scale(10).coords(coords)
///              .nodeScale(2).nodeSizes(sizes)
///              .arcWidthScale(.4).run();
///\endcode
///
///For more detailed examples, see the \ref graph_to_eps_demo.cc demo file.
///
///\warning Don't forget to put the \ref GraphToEps::run() "run()"
///to the end of the parameter list.
///\sa GraphToEps
///\sa graphToEps(GR &g, const char *file_name)
template<class GR>
GraphToEps<DefaultGraphToEpsTraits<GR> >
graphToEps(GR &g, std::ostream& os=std::cout)
{
  return
    GraphToEps<DefaultGraphToEpsTraits<GR> >(DefaultGraphToEpsTraits<GR>(g,os));
}

///Generates an EPS file from a graph

///\ingroup eps_io
///This function does the same as
///\ref graphToEps(GR &g,std::ostream& os)
///but it writes its output into the file \c file_name
///instead of a stream.
///\sa graphToEps(GR &g, std::ostream& os)
template<class GR>
GraphToEps<DefaultGraphToEpsTraits<GR> >
graphToEps(GR &g,const char *file_name)
{
  std::ostream* os = new std::ofstream(file_name);
  if (!(*os)) {
    delete os;
    throw IoError("Cannot write file", file_name);
  }
  return GraphToEps<DefaultGraphToEpsTraits<GR> >
    (DefaultGraphToEpsTraits<GR>(g,*os,true));
}

///Generates an EPS file from a graph

///\ingroup eps_io
///This function does the same as
///\ref graphToEps(GR &g,std::ostream& os)
///but it writes its output into the file \c file_name
///instead of a stream.
///\sa graphToEps(GR &g, std::ostream& os)
template<class GR>
GraphToEps<DefaultGraphToEpsTraits<GR> >
graphToEps(GR &g,const std::string& file_name)
{
  std::ostream* os = new std::ofstream(file_name.c_str());
  if (!(*os)) {
    delete os;
    throw IoError("Cannot write file", file_name);
  }
  return GraphToEps<DefaultGraphToEpsTraits<GR> >
    (DefaultGraphToEpsTraits<GR>(g,*os,true));
}

} //END OF NAMESPACE LEMON

#endif // LEMON_GRAPH_TO_EPS_H
