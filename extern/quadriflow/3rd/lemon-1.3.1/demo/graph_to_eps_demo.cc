/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
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

/// \ingroup demos
/// \file
/// \brief Demo of the graph drawing function \ref graphToEps()
///
/// This demo program shows examples how to use the function \ref
/// graphToEps(). It takes no input but simply creates seven
/// <tt>.eps</tt> files demonstrating the capability of \ref
/// graphToEps(), and showing how to draw directed graphs,
/// how to handle parallel egdes, how to change the properties (like
/// color, shape, size, title etc.) of nodes and arcs individually
/// using appropriate graph maps.
///
/// \include graph_to_eps_demo.cc

#include<lemon/list_graph.h>
#include<lemon/graph_to_eps.h>
#include<lemon/math.h>

using namespace std;
using namespace lemon;

int main()
{
  Palette palette;
  Palette paletteW(true);

  // Create a small digraph
  ListDigraph g;
  typedef ListDigraph::Node Node;
  typedef ListDigraph::NodeIt NodeIt;
  typedef ListDigraph::Arc Arc;
  typedef dim2::Point<int> Point;

  Node n1=g.addNode();
  Node n2=g.addNode();
  Node n3=g.addNode();
  Node n4=g.addNode();
  Node n5=g.addNode();

  ListDigraph::NodeMap<Point> coords(g);
  ListDigraph::NodeMap<double> sizes(g);
  ListDigraph::NodeMap<int> colors(g);
  ListDigraph::NodeMap<int> shapes(g);
  ListDigraph::ArcMap<int> acolors(g);
  ListDigraph::ArcMap<int> widths(g);

  coords[n1]=Point(50,50);  sizes[n1]=1; colors[n1]=1; shapes[n1]=0;
  coords[n2]=Point(50,70);  sizes[n2]=2; colors[n2]=2; shapes[n2]=2;
  coords[n3]=Point(70,70);  sizes[n3]=1; colors[n3]=3; shapes[n3]=0;
  coords[n4]=Point(70,50);  sizes[n4]=2; colors[n4]=4; shapes[n4]=1;
  coords[n5]=Point(85,60);  sizes[n5]=3; colors[n5]=5; shapes[n5]=2;

  Arc a;

  a=g.addArc(n1,n2); acolors[a]=0; widths[a]=1;
  a=g.addArc(n2,n3); acolors[a]=0; widths[a]=1;
  a=g.addArc(n3,n5); acolors[a]=0; widths[a]=3;
  a=g.addArc(n5,n4); acolors[a]=0; widths[a]=1;
  a=g.addArc(n4,n1); acolors[a]=0; widths[a]=1;
  a=g.addArc(n2,n4); acolors[a]=1; widths[a]=2;
  a=g.addArc(n3,n4); acolors[a]=2; widths[a]=1;

  IdMap<ListDigraph,Node> id(g);

  // Create .eps files showing the digraph with different options
  cout << "Create 'graph_to_eps_demo_out_1_pure.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_1_pure.eps").
    coords(coords).
    title("Sample .eps figure").
    copyright("(C) 2003-2009 LEMON Project").
    run();

  cout << "Create 'graph_to_eps_demo_out_2.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_2.eps").
    coords(coords).
    title("Sample .eps figure").
    copyright("(C) 2003-2009 LEMON Project").
    absoluteNodeSizes().absoluteArcWidths().
    nodeScale(2).nodeSizes(sizes).
    nodeShapes(shapes).
    nodeColors(composeMap(palette,colors)).
    arcColors(composeMap(palette,acolors)).
    arcWidthScale(.4).arcWidths(widths).
    nodeTexts(id).nodeTextSize(3).
    run();

  cout << "Create 'graph_to_eps_demo_out_3_arr.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_3_arr.eps").
    title("Sample .eps figure (with arrowheads)").
    copyright("(C) 2003-2009 LEMON Project").
    absoluteNodeSizes().absoluteArcWidths().
    nodeColors(composeMap(palette,colors)).
    coords(coords).
    nodeScale(2).nodeSizes(sizes).
    nodeShapes(shapes).
    arcColors(composeMap(palette,acolors)).
    arcWidthScale(.4).arcWidths(widths).
    nodeTexts(id).nodeTextSize(3).
    drawArrows().arrowWidth(2).arrowLength(2).
    run();

  // Add more arcs to the digraph
  a=g.addArc(n1,n4); acolors[a]=2; widths[a]=1;
  a=g.addArc(n4,n1); acolors[a]=1; widths[a]=2;

  a=g.addArc(n1,n2); acolors[a]=1; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=2; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=3; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=4; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=5; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=6; widths[a]=1;
  a=g.addArc(n1,n2); acolors[a]=7; widths[a]=1;

  cout << "Create 'graph_to_eps_demo_out_4_par.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_4_par.eps").
    title("Sample .eps figure (parallel arcs)").
    copyright("(C) 2003-2009 LEMON Project").
    absoluteNodeSizes().absoluteArcWidths().
    nodeShapes(shapes).
    coords(coords).
    nodeScale(2).nodeSizes(sizes).
    nodeColors(composeMap(palette,colors)).
    arcColors(composeMap(palette,acolors)).
    arcWidthScale(.4).arcWidths(widths).
    nodeTexts(id).nodeTextSize(3).
    enableParallel().parArcDist(1.5).
    run();

  cout << "Create 'graph_to_eps_demo_out_5_par_arr.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_5_par_arr.eps").
    title("Sample .eps figure (parallel arcs and arrowheads)").
    copyright("(C) 2003-2009 LEMON Project").
    absoluteNodeSizes().absoluteArcWidths().
    nodeScale(2).nodeSizes(sizes).
    coords(coords).
    nodeShapes(shapes).
    nodeColors(composeMap(palette,colors)).
    arcColors(composeMap(palette,acolors)).
    arcWidthScale(.3).arcWidths(widths).
    nodeTexts(id).nodeTextSize(3).
    enableParallel().parArcDist(1).
    drawArrows().arrowWidth(1).arrowLength(1).
    run();

  cout << "Create 'graph_to_eps_demo_out_6_par_arr_a4.eps'" << endl;
  graphToEps(g,"graph_to_eps_demo_out_6_par_arr_a4.eps").
    title("Sample .eps figure (fits to A4)").
    copyright("(C) 2003-2009 LEMON Project").
    scaleToA4().
    absoluteNodeSizes().absoluteArcWidths().
    nodeScale(2).nodeSizes(sizes).
    coords(coords).
    nodeShapes(shapes).
    nodeColors(composeMap(palette,colors)).
    arcColors(composeMap(palette,acolors)).
    arcWidthScale(.3).arcWidths(widths).
    nodeTexts(id).nodeTextSize(3).
    enableParallel().parArcDist(1).
    drawArrows().arrowWidth(1).arrowLength(1).
    run();

  // Create an .eps file showing the colors of a default Palette
  ListDigraph h;
  ListDigraph::NodeMap<int> hcolors(h);
  ListDigraph::NodeMap<Point> hcoords(h);

  int cols=int(std::sqrt(double(palette.size())));
  for(int i=0;i<int(paletteW.size());i++) {
    Node n=h.addNode();
    hcoords[n]=Point(1+i%cols,1+i/cols);
    hcolors[n]=i;
  }

  cout << "Create 'graph_to_eps_demo_out_7_colors.eps'" << endl;
  graphToEps(h,"graph_to_eps_demo_out_7_colors.eps").
    scale(60).
    title("Sample .eps figure (Palette demo)").
    copyright("(C) 2003-2009 LEMON Project").
    coords(hcoords).
    absoluteNodeSizes().absoluteArcWidths().
    nodeScale(.45).
    distantColorNodeTexts().
    nodeTexts(hcolors).nodeTextSize(.6).
    nodeColors(composeMap(paletteW,hcolors)).
    run();

  return 0;
}
