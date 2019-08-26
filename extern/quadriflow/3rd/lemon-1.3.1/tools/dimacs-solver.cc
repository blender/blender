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

///\ingroup tools
///\file
///\brief DIMACS problem solver.
///
/// This program solves various problems given in DIMACS format.
///
/// See
/// \code
///   dimacs-solver --help
/// \endcode
/// for more info on usage.

#include <iostream>
#include <fstream>
#include <cstring>

#include <lemon/smart_graph.h>
#include <lemon/dimacs.h>
#include <lemon/lgf_writer.h>
#include <lemon/time_measure.h>

#include <lemon/arg_parser.h>
#include <lemon/error.h>

#include <lemon/dijkstra.h>
#include <lemon/preflow.h>
#include <lemon/matching.h>
#include <lemon/network_simplex.h>

using namespace lemon;
typedef SmartDigraph Digraph;
DIGRAPH_TYPEDEFS(Digraph);
typedef SmartGraph Graph;

template<class Value>
void solve_sp(ArgParser &ap, std::istream &is, std::ostream &,
              DimacsDescriptor &desc)
{
  bool report = !ap.given("q");
  Digraph g;
  Node s;
  Digraph::ArcMap<Value> len(g);
  Timer t;
  t.restart();
  readDimacsSp(is, g, len, s, desc);
  if(report) std::cerr << "Read the file: " << t << '\n';
  t.restart();
  Dijkstra<Digraph, Digraph::ArcMap<Value> > dij(g,len);
  if(report) std::cerr << "Setup Dijkstra class: " << t << '\n';
  t.restart();
  dij.run(s);
  if(report) std::cerr << "Run Dijkstra: " << t << '\n';
}

template<class Value>
void solve_max(ArgParser &ap, std::istream &is, std::ostream &,
               Value infty, DimacsDescriptor &desc)
{
  bool report = !ap.given("q");
  Digraph g;
  Node s,t;
  Digraph::ArcMap<Value> cap(g);
  Timer ti;
  ti.restart();
  readDimacsMax(is, g, cap, s, t, infty, desc);
  if(report) std::cerr << "Read the file: " << ti << '\n';
  ti.restart();
  Preflow<Digraph, Digraph::ArcMap<Value> > pre(g,cap,s,t);
  if(report) std::cerr << "Setup Preflow class: " << ti << '\n';
  ti.restart();
  pre.run();
  if(report) std::cerr << "Run Preflow: " << ti << '\n';
  if(report) std::cerr << "\nMax flow value: " << pre.flowValue() << '\n';
}

template<class Value, class LargeValue>
void solve_min(ArgParser &ap, std::istream &is, std::ostream &,
               Value infty, DimacsDescriptor &desc)
{
  bool report = !ap.given("q");
  Digraph g;
  Digraph::ArcMap<Value> lower(g), cap(g), cost(g);
  Digraph::NodeMap<Value> sup(g);
  Timer ti;

  ti.restart();
  readDimacsMin(is, g, lower, cap, cost, sup, infty, desc);
  ti.stop();
  Value sum_sup = 0;
  for (Digraph::NodeIt n(g); n != INVALID; ++n) {
    sum_sup += sup[n];
  }
  if (report) {
    std::cerr << "Sum of supply values: " << sum_sup << "\n";
    if (sum_sup <= 0)
      std::cerr << "GEQ supply contraints are used for NetworkSimplex\n\n";
    else
      std::cerr << "LEQ supply contraints are used for NetworkSimplex\n\n";
  }
  if (report) std::cerr << "Read the file: " << ti << '\n';

  typedef NetworkSimplex<Digraph, Value> MCF;
  ti.restart();
  MCF ns(g);
  ns.lowerMap(lower).upperMap(cap).costMap(cost).supplyMap(sup);
  if (sum_sup > 0) ns.supplyType(ns.LEQ);
  if (report) std::cerr << "Setup NetworkSimplex class: " << ti << '\n';
  ti.restart();
  typename MCF::ProblemType res = ns.run();
  if (report) {
    std::cerr << "Run NetworkSimplex: " << ti << "\n\n";
    std::cerr << "Feasible flow: " << (res == MCF::OPTIMAL ? "found" :
                                       "not found") << '\n';
    if (res) std::cerr << "Min flow cost: "
                       << ns.template totalCost<LargeValue>() << '\n';
  }
}

void solve_mat(ArgParser &ap, std::istream &is, std::ostream &,
              DimacsDescriptor &desc)
{
  bool report = !ap.given("q");
  Graph g;
  Timer ti;
  ti.restart();
  readDimacsMat(is, g, desc);
  if(report) std::cerr << "Read the file: " << ti << '\n';
  ti.restart();
  MaxMatching<Graph> mat(g);
  if(report) std::cerr << "Setup MaxMatching class: " << ti << '\n';
  ti.restart();
  mat.run();
  if(report) std::cerr << "Run MaxMatching: " << ti << '\n';
  if(report) std::cerr << "\nCardinality of max matching: "
                       << mat.matchingSize() << '\n';
}


template<class Value, class LargeValue>
void solve(ArgParser &ap, std::istream &is, std::ostream &os,
           DimacsDescriptor &desc)
{
  std::stringstream iss(static_cast<std::string>(ap["infcap"]));
  Value infty;
  iss >> infty;
  if(iss.fail())
    {
      std::cerr << "Cannot interpret '"
                << static_cast<std::string>(ap["infcap"]) << "' as infinite"
                << std::endl;
      exit(1);
    }

  switch(desc.type)
    {
    case DimacsDescriptor::MIN:
      solve_min<Value, LargeValue>(ap,is,os,infty,desc);
      break;
    case DimacsDescriptor::MAX:
      solve_max<Value>(ap,is,os,infty,desc);
      break;
    case DimacsDescriptor::SP:
      solve_sp<Value>(ap,is,os,desc);
      break;
    case DimacsDescriptor::MAT:
      solve_mat(ap,is,os,desc);
      break;
    default:
      break;
    }
}

int main(int argc, const char *argv[]) {

  std::string inputName;
  std::string outputName;

  ArgParser ap(argc, argv);
  ap.other("[INFILE [OUTFILE]]",
           "If either the INFILE or OUTFILE file is missing the standard\n"
           "     input/output will be used instead.")
    .boolOption("q", "Do not print any report")
    .boolOption("int","Use 'int' for capacities, costs etc. (default)")
    .optionGroup("datatype","int")
#ifdef LEMON_HAVE_LONG_LONG
    .boolOption("long","Use 'long long' for capacities, costs etc.")
    .optionGroup("datatype","long")
#endif
    .boolOption("double","Use 'double' for capacities, costs etc.")
    .optionGroup("datatype","double")
    .boolOption("ldouble","Use 'long double' for capacities, costs etc.")
    .optionGroup("datatype","ldouble")
    .onlyOneGroup("datatype")
    .stringOption("infcap","Value used for 'very high' capacities","0")
    .run();

  std::ifstream input;
  std::ofstream output;

  switch(ap.files().size())
    {
    case 2:
      output.open(ap.files()[1].c_str());
      if (!output) {
        throw IoError("Cannot open the file for writing", ap.files()[1]);
      }
    case 1:
      input.open(ap.files()[0].c_str());
      if (!input) {
        throw IoError("File cannot be found", ap.files()[0]);
      }
    case 0:
      break;
    default:
      std::cerr << ap.commandName() << ": too many arguments\n";
      return 1;
    }
  std::istream& is = (ap.files().size()<1 ? std::cin : input);
  std::ostream& os = (ap.files().size()<2 ? std::cout : output);

  DimacsDescriptor desc = dimacsType(is);

  if(!ap.given("q"))
    {
      std::cout << "Problem type: ";
      switch(desc.type)
        {
        case DimacsDescriptor::MIN:
          std::cout << "min";
          break;
        case DimacsDescriptor::MAX:
          std::cout << "max";
          break;
        case DimacsDescriptor::SP:
          std::cout << "sp";
        case DimacsDescriptor::MAT:
          std::cout << "mat";
          break;
        default:
          exit(1);
          break;
        }
      std::cout << "\nNum of nodes: " << desc.nodeNum;
      std::cout << "\nNum of arcs:  " << desc.edgeNum;
      std::cout << "\n\n";
    }

  if(ap.given("double"))
    solve<double, double>(ap,is,os,desc);
  else if(ap.given("ldouble"))
    solve<long double, long double>(ap,is,os,desc);
#ifdef LEMON_HAVE_LONG_LONG
  else if(ap.given("long"))
    solve<long long, long long>(ap,is,os,desc);
  else solve<int, long long>(ap,is,os,desc);
#else
  else solve<int, long>(ap,is,os,desc);
#endif

  return 0;
}
