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

/// \ingroup tools
/// \file
/// \brief Special plane graph generator.
///
/// Graph generator application for various types of plane graphs.
///
/// See
/// \code
///   lgf-gen --help
/// \endcode
/// for more information on the usage.

#include <algorithm>
#include <set>
#include <ctime>
#include <lemon/list_graph.h>
#include <lemon/random.h>
#include <lemon/dim2.h>
#include <lemon/bfs.h>
#include <lemon/counter.h>
#include <lemon/suurballe.h>
#include <lemon/graph_to_eps.h>
#include <lemon/lgf_writer.h>
#include <lemon/arg_parser.h>
#include <lemon/euler.h>
#include <lemon/math.h>
#include <lemon/kruskal.h>
#include <lemon/time_measure.h>

using namespace lemon;

typedef dim2::Point<double> Point;

GRAPH_TYPEDEFS(ListGraph);

bool progress=true;

int N;
// int girth;

ListGraph g;

std::vector<Node> nodes;
ListGraph::NodeMap<Point> coords(g);


double totalLen(){
  double tlen=0;
  for(EdgeIt e(g);e!=INVALID;++e)
    tlen+=std::sqrt((coords[g.v(e)]-coords[g.u(e)]).normSquare());
  return tlen;
}

int tsp_impr_num=0;

const double EPSILON=1e-8;
bool tsp_improve(Node u, Node v)
{
  double luv=std::sqrt((coords[v]-coords[u]).normSquare());
  Node u2=u;
  Node v2=v;
  do {
    Node n;
    for(IncEdgeIt e(g,v2);(n=g.runningNode(e))==u2;++e) { }
    u2=v2;
    v2=n;
    if(luv+std::sqrt((coords[v2]-coords[u2]).normSquare())-EPSILON>
       std::sqrt((coords[u]-coords[u2]).normSquare())+
       std::sqrt((coords[v]-coords[v2]).normSquare()))
      {
         g.erase(findEdge(g,u,v));
         g.erase(findEdge(g,u2,v2));
        g.addEdge(u2,u);
        g.addEdge(v,v2);
        tsp_impr_num++;
        return true;
      }
  } while(v2!=u);
  return false;
}

bool tsp_improve(Node u)
{
  for(IncEdgeIt e(g,u);e!=INVALID;++e)
    if(tsp_improve(u,g.runningNode(e))) return true;
  return false;
}

void tsp_improve()
{
  bool b;
  do {
    b=false;
    for(NodeIt n(g);n!=INVALID;++n)
      if(tsp_improve(n)) b=true;
  } while(b);
}

void tsp()
{
  for(int i=0;i<N;i++) g.addEdge(nodes[i],nodes[(i+1)%N]);
  tsp_improve();
}

class Line
{
public:
  Point a;
  Point b;
  Line(Point _a,Point _b) :a(_a),b(_b) {}
  Line(Node _a,Node _b) : a(coords[_a]),b(coords[_b]) {}
  Line(const Arc &e) : a(coords[g.source(e)]),b(coords[g.target(e)]) {}
  Line(const Edge &e) : a(coords[g.u(e)]),b(coords[g.v(e)]) {}
};

inline std::ostream& operator<<(std::ostream &os, const Line &l)
{
  os << l.a << "->" << l.b;
  return os;
}

bool cross(Line a, Line b)
{
  Point ao=rot90(a.b-a.a);
  Point bo=rot90(b.b-b.a);
  return (ao*(b.a-a.a))*(ao*(b.b-a.a))<0 &&
    (bo*(a.a-b.a))*(bo*(a.b-b.a))<0;
}

struct Parc
{
  Node a;
  Node b;
  double len;
};

bool pedgeLess(Parc a,Parc b)
{
  return a.len<b.len;
}

std::vector<Edge> arcs;

namespace _delaunay_bits {

  struct Part {
    int prev, curr, next;

    Part(int p, int c, int n) : prev(p), curr(c), next(n) {}
  };

  inline std::ostream& operator<<(std::ostream& os, const Part& part) {
    os << '(' << part.prev << ',' << part.curr << ',' << part.next << ')';
    return os;
  }

  inline double circle_point(const Point& p, const Point& q, const Point& r) {
    double a = p.x * (q.y - r.y) + q.x * (r.y - p.y) + r.x * (p.y - q.y);
    if (a == 0) return std::numeric_limits<double>::quiet_NaN();

    double d = (p.x * p.x + p.y * p.y) * (q.y - r.y) +
      (q.x * q.x + q.y * q.y) * (r.y - p.y) +
      (r.x * r.x + r.y * r.y) * (p.y - q.y);

    double e = (p.x * p.x + p.y * p.y) * (q.x - r.x) +
      (q.x * q.x + q.y * q.y) * (r.x - p.x) +
      (r.x * r.x + r.y * r.y) * (p.x - q.x);

    double f = (p.x * p.x + p.y * p.y) * (q.x * r.y - r.x * q.y) +
      (q.x * q.x + q.y * q.y) * (r.x * p.y - p.x * r.y) +
      (r.x * r.x + r.y * r.y) * (p.x * q.y - q.x * p.y);

    return d / (2 * a) + std::sqrt((d * d + e * e) / (4 * a * a) + f / a);
  }

  inline bool circle_form(const Point& p, const Point& q, const Point& r) {
    return rot90(q - p) * (r - q) < 0.0;
  }

  inline double intersection(const Point& p, const Point& q, double sx) {
    const double epsilon = 1e-8;

    if (p.x == q.x) return (p.y + q.y) / 2.0;

    if (sx < p.x + epsilon) return p.y;
    if (sx < q.x + epsilon) return q.y;

    double a = q.x - p.x;
    double b = (q.x - sx) * p.y - (p.x - sx) * q.y;
    double d = (q.x - sx) * (p.x - sx) * (p - q).normSquare();
    return (b - std::sqrt(d)) / a;
  }

  struct YLess {


    YLess(const std::vector<Point>& points, double& sweep)
      : _points(points), _sweep(sweep) {}

    bool operator()(const Part& l, const Part& r) const {
      const double epsilon = 1e-8;

      //      std::cerr << l << " vs " << r << std::endl;
      double lbx = l.prev != -1 ?
        intersection(_points[l.prev], _points[l.curr], _sweep) :
        - std::numeric_limits<double>::infinity();
      double rbx = r.prev != -1 ?
        intersection(_points[r.prev], _points[r.curr], _sweep) :
        - std::numeric_limits<double>::infinity();
      double lex = l.next != -1 ?
        intersection(_points[l.curr], _points[l.next], _sweep) :
        std::numeric_limits<double>::infinity();
      double rex = r.next != -1 ?
        intersection(_points[r.curr], _points[r.next], _sweep) :
        std::numeric_limits<double>::infinity();

      if (lbx > lex) std::swap(lbx, lex);
      if (rbx > rex) std::swap(rbx, rex);

      if (lex < epsilon + rex && lbx + epsilon < rex) return true;
      if (rex < epsilon + lex && rbx + epsilon < lex) return false;
      return lex < rex;
    }

    const std::vector<Point>& _points;
    double& _sweep;
  };

  struct BeachIt;

  typedef std::multimap<double, BeachIt*> SpikeHeap;

  typedef std::multimap<Part, SpikeHeap::iterator, YLess> Beach;

  struct BeachIt {
    Beach::iterator it;

    BeachIt(Beach::iterator iter) : it(iter) {}
  };

}

inline void delaunay() {
  Counter cnt("Number of arcs added: ");

  using namespace _delaunay_bits;

  typedef _delaunay_bits::Part Part;
  typedef std::vector<std::pair<double, int> > SiteHeap;


  std::vector<Point> points;
  std::vector<Node> nodes;

  for (NodeIt it(g); it != INVALID; ++it) {
    nodes.push_back(it);
    points.push_back(coords[it]);
  }

  SiteHeap siteheap(points.size());

  double sweep;


  for (int i = 0; i < int(siteheap.size()); ++i) {
    siteheap[i] = std::make_pair(points[i].x, i);
  }

  std::sort(siteheap.begin(), siteheap.end());
  sweep = siteheap.front().first;

  YLess yless(points, sweep);
  Beach beach(yless);

  SpikeHeap spikeheap;

  std::set<std::pair<int, int> > arcs;

  int siteindex = 0;
  {
    SiteHeap front;

    while (siteindex < int(siteheap.size()) &&
           siteheap[0].first == siteheap[siteindex].first) {
      front.push_back(std::make_pair(points[siteheap[siteindex].second].y,
                                     siteheap[siteindex].second));
      ++siteindex;
    }

    std::sort(front.begin(), front.end());

    for (int i = 0; i < int(front.size()); ++i) {
      int prev = (i == 0 ? -1 : front[i - 1].second);
      int curr = front[i].second;
      int next = (i + 1 == int(front.size()) ? -1 : front[i + 1].second);

      beach.insert(std::make_pair(Part(prev, curr, next),
                                  spikeheap.end()));
    }
  }

  while (siteindex < int(points.size()) || !spikeheap.empty()) {

    SpikeHeap::iterator spit = spikeheap.begin();

    if (siteindex < int(points.size()) &&
        (spit == spikeheap.end() || siteheap[siteindex].first < spit->first)) {
      int site = siteheap[siteindex].second;
      sweep = siteheap[siteindex].first;

      Beach::iterator bit = beach.upper_bound(Part(site, site, site));

      if (bit->second != spikeheap.end()) {
        delete bit->second->second;
        spikeheap.erase(bit->second);
      }

      int prev = bit->first.prev;
      int curr = bit->first.curr;
      int next = bit->first.next;

      beach.erase(bit);

      SpikeHeap::iterator pit = spikeheap.end();
      if (prev != -1 &&
          circle_form(points[prev], points[curr], points[site])) {
        double x = circle_point(points[prev], points[curr], points[site]);
        pit = spikeheap.insert(std::make_pair(x, new BeachIt(beach.end())));
        pit->second->it =
          beach.insert(std::make_pair(Part(prev, curr, site), pit));
      } else {
        beach.insert(std::make_pair(Part(prev, curr, site), pit));
      }

      beach.insert(std::make_pair(Part(curr, site, curr), spikeheap.end()));

      SpikeHeap::iterator nit = spikeheap.end();
      if (next != -1 &&
          circle_form(points[site], points[curr],points[next])) {
        double x = circle_point(points[site], points[curr], points[next]);
        nit = spikeheap.insert(std::make_pair(x, new BeachIt(beach.end())));
        nit->second->it =
          beach.insert(std::make_pair(Part(site, curr, next), nit));
      } else {
        beach.insert(std::make_pair(Part(site, curr, next), nit));
      }

      ++siteindex;
    } else {
      sweep = spit->first;

      Beach::iterator bit = spit->second->it;

      int prev = bit->first.prev;
      int curr = bit->first.curr;
      int next = bit->first.next;

      {
        std::pair<int, int> arc;

        arc = prev < curr ?
          std::make_pair(prev, curr) : std::make_pair(curr, prev);

        if (arcs.find(arc) == arcs.end()) {
          arcs.insert(arc);
          g.addEdge(nodes[prev], nodes[curr]);
          ++cnt;
        }

        arc = curr < next ?
          std::make_pair(curr, next) : std::make_pair(next, curr);

        if (arcs.find(arc) == arcs.end()) {
          arcs.insert(arc);
          g.addEdge(nodes[curr], nodes[next]);
          ++cnt;
        }
      }

      Beach::iterator pbit = bit; --pbit;
      int ppv = pbit->first.prev;
      Beach::iterator nbit = bit; ++nbit;
      int nnt = nbit->first.next;

      if (bit->second != spikeheap.end())
        {
          delete bit->second->second;
          spikeheap.erase(bit->second);
        }
      if (pbit->second != spikeheap.end())
        {
          delete pbit->second->second;
          spikeheap.erase(pbit->second);
        }
      if (nbit->second != spikeheap.end())
        {
          delete nbit->second->second;
          spikeheap.erase(nbit->second);
        }
      
      beach.erase(nbit);
      beach.erase(bit);
      beach.erase(pbit);

      SpikeHeap::iterator pit = spikeheap.end();
      if (ppv != -1 && ppv != next &&
          circle_form(points[ppv], points[prev], points[next])) {
        double x = circle_point(points[ppv], points[prev], points[next]);
        if (x < sweep) x = sweep;
        pit = spikeheap.insert(std::make_pair(x, new BeachIt(beach.end())));
        pit->second->it =
          beach.insert(std::make_pair(Part(ppv, prev, next), pit));
      } else {
        beach.insert(std::make_pair(Part(ppv, prev, next), pit));
      }

      SpikeHeap::iterator nit = spikeheap.end();
      if (nnt != -1 && prev != nnt &&
          circle_form(points[prev], points[next], points[nnt])) {
        double x = circle_point(points[prev], points[next], points[nnt]);
        if (x < sweep) x = sweep;
        nit = spikeheap.insert(std::make_pair(x, new BeachIt(beach.end())));
        nit->second->it =
          beach.insert(std::make_pair(Part(prev, next, nnt), nit));
      } else {
        beach.insert(std::make_pair(Part(prev, next, nnt), nit));
      }

    }
  }

  for (Beach::iterator it = beach.begin(); it != beach.end(); ++it) {
    int curr = it->first.curr;
    int next = it->first.next;

    if (next == -1) continue;

    std::pair<int, int> arc;

    arc = curr < next ?
      std::make_pair(curr, next) : std::make_pair(next, curr);

    if (arcs.find(arc) == arcs.end()) {
      arcs.insert(arc);
      g.addEdge(nodes[curr], nodes[next]);
      ++cnt;
    }
  }
}

void sparse(int d)
{
  Counter cnt("Number of arcs removed: ");
  Bfs<ListGraph> bfs(g);
  for(std::vector<Edge>::reverse_iterator ei=arcs.rbegin();
      ei!=arcs.rend();++ei)
    {
      Node a=g.u(*ei);
      Node b=g.v(*ei);
      g.erase(*ei);
      bfs.run(a,b);
      if(bfs.predArc(b)==INVALID || bfs.dist(b)>d)
        g.addEdge(a,b);
      else cnt++;
    }
}

void sparse2(int d)
{
  Counter cnt("Number of arcs removed: ");
  for(std::vector<Edge>::reverse_iterator ei=arcs.rbegin();
      ei!=arcs.rend();++ei)
    {
      Node a=g.u(*ei);
      Node b=g.v(*ei);
      g.erase(*ei);
      ConstMap<Arc,int> cegy(1);
      Suurballe<ListGraph,ConstMap<Arc,int> > sur(g,cegy);
      int k=sur.run(a,b,2);
      if(k<2 || sur.totalLength()>d)
        g.addEdge(a,b);
      else cnt++;
//       else std::cout << "Remove arc " << g.id(a) << "-" << g.id(b) << '\n';
    }
}

void sparseTriangle(int d)
{
  Counter cnt("Number of arcs added: ");
  std::vector<Parc> pedges;
  for(NodeIt n(g);n!=INVALID;++n)
    for(NodeIt m=++(NodeIt(n));m!=INVALID;++m)
      {
        Parc p;
        p.a=n;
        p.b=m;
        p.len=(coords[m]-coords[n]).normSquare();
        pedges.push_back(p);
      }
  std::sort(pedges.begin(),pedges.end(),pedgeLess);
  for(std::vector<Parc>::iterator pi=pedges.begin();pi!=pedges.end();++pi)
    {
      Line li(pi->a,pi->b);
      EdgeIt e(g);
      for(;e!=INVALID && !cross(e,li);++e) ;
      Edge ne;
      if(e==INVALID) {
        ConstMap<Arc,int> cegy(1);
        Suurballe<ListGraph,ConstMap<Arc,int> > sur(g,cegy);
        int k=sur.run(pi->a,pi->b,2);
        if(k<2 || sur.totalLength()>d)
          {
            ne=g.addEdge(pi->a,pi->b);
            arcs.push_back(ne);
            cnt++;
          }
      }
    }
}

template <typename Graph, typename CoordMap>
class LengthSquareMap {
public:
  typedef typename Graph::Edge Key;
  typedef typename CoordMap::Value::Value Value;

  LengthSquareMap(const Graph& graph, const CoordMap& coords)
    : _graph(graph), _coords(coords) {}

  Value operator[](const Key& key) const {
    return (_coords[_graph.v(key)] -
            _coords[_graph.u(key)]).normSquare();
  }

private:

  const Graph& _graph;
  const CoordMap& _coords;
};

void minTree() {
  std::vector<Parc> pedges;
  Timer T;
  std::cout << T.realTime() << "s: Creating delaunay triangulation...\n";
  delaunay();
  std::cout << T.realTime() << "s: Calculating spanning tree...\n";
  LengthSquareMap<ListGraph, ListGraph::NodeMap<Point> > ls(g, coords);
  ListGraph::EdgeMap<bool> tree(g);
  kruskal(g, ls, tree);
  std::cout << T.realTime() << "s: Removing non tree arcs...\n";
  std::vector<Edge> remove;
  for (EdgeIt e(g); e != INVALID; ++e) {
    if (!tree[e]) remove.push_back(e);
  }
  for(int i = 0; i < int(remove.size()); ++i) {
    g.erase(remove[i]);
  }
  std::cout << T.realTime() << "s: Done\n";
}

void tsp2()
{
  std::cout << "Find a tree..." << std::endl;

  minTree();

  std::cout << "Total arc length (tree) : " << totalLen() << std::endl;

  std::cout << "Make it Euler..." << std::endl;

  {
    std::vector<Node> leafs;
    for(NodeIt n(g);n!=INVALID;++n)
      if(countIncEdges(g,n)%2==1) leafs.push_back(n);

//    for(unsigned int i=0;i<leafs.size();i+=2)
//       g.addArc(leafs[i],leafs[i+1]);

    std::vector<Parc> pedges;
    for(unsigned int i=0;i<leafs.size()-1;i++)
      for(unsigned int j=i+1;j<leafs.size();j++)
        {
          Node n=leafs[i];
          Node m=leafs[j];
          Parc p;
          p.a=n;
          p.b=m;
          p.len=(coords[m]-coords[n]).normSquare();
          pedges.push_back(p);
        }
    std::sort(pedges.begin(),pedges.end(),pedgeLess);
    for(unsigned int i=0;i<pedges.size();i++)
      if(countIncEdges(g,pedges[i].a)%2 &&
         countIncEdges(g,pedges[i].b)%2)
        g.addEdge(pedges[i].a,pedges[i].b);
  }

  for(NodeIt n(g);n!=INVALID;++n)
    if(countIncEdges(g,n)%2 || countIncEdges(g,n)==0 )
      std::cout << "GEBASZ!!!" << std::endl;

  for(EdgeIt e(g);e!=INVALID;++e)
    if(g.u(e)==g.v(e))
      std::cout << "LOOP GEBASZ!!!" << std::endl;

  std::cout << "Number of arcs : " << countEdges(g) << std::endl;

  std::cout << "Total arc length (euler) : " << totalLen() << std::endl;

  ListGraph::EdgeMap<Arc> enext(g);
  {
    EulerIt<ListGraph> e(g);
    Arc eo=e;
    Arc ef=e;
//     std::cout << "Tour arc: " << g.id(Edge(e)) << std::endl;
    for(++e;e!=INVALID;++e)
      {
//         std::cout << "Tour arc: " << g.id(Edge(e)) << std::endl;
        enext[eo]=e;
        eo=e;
      }
    enext[eo]=ef;
  }

  std::cout << "Creating a tour from that..." << std::endl;

  int nnum = countNodes(g);
  int ednum = countEdges(g);

  for(Arc p=enext[EdgeIt(g)];ednum>nnum;p=enext[p])
    {
//       std::cout << "Checking arc " << g.id(p) << std::endl;
      Arc e=enext[p];
      Arc f=enext[e];
      Node n2=g.source(f);
      Node n1=g.oppositeNode(n2,e);
      Node n3=g.oppositeNode(n2,f);
      if(countIncEdges(g,n2)>2)
        {
//           std::cout << "Remove an Arc" << std::endl;
          Arc ff=enext[f];
          g.erase(e);
          g.erase(f);
          if(n1!=n3)
            {
              Arc ne=g.direct(g.addEdge(n1,n3),n1);
              enext[p]=ne;
              enext[ne]=ff;
              ednum--;
            }
          else {
            enext[p]=ff;
            ednum-=2;
          }
        }
    }

  std::cout << "Total arc length (tour) : " << totalLen() << std::endl;

  std::cout << "2-opt the tour..." << std::endl;

  tsp_improve();

  std::cout << "Total arc length (2-opt tour) : " << totalLen() << std::endl;
}


int main(int argc,const char **argv)
{
  ArgParser ap(argc,argv);

//   bool eps;
  bool disc_d, square_d, gauss_d;
//   bool tsp_a,two_a,tree_a;
  int num_of_cities=1;
  double area=1;
  N=100;
//   girth=10;
  std::string ndist("disc");
  ap.refOption("n", "Number of nodes (default is 100)", N)
    .intOption("g", "Girth parameter (default is 10)", 10)
    .refOption("cities", "Number of cities (default is 1)", num_of_cities)
    .refOption("area", "Full relative area of the cities (default is 1)", area)
    .refOption("disc", "Nodes are evenly distributed on a unit disc (default)",
               disc_d)
    .optionGroup("dist", "disc")
    .refOption("square", "Nodes are evenly distributed on a unit square",
               square_d)
    .optionGroup("dist", "square")
    .refOption("gauss", "Nodes are located according to a two-dim Gauss "
               "distribution", gauss_d)
    .optionGroup("dist", "gauss")
    .onlyOneGroup("dist")
    .boolOption("eps", "Also generate .eps output (<prefix>.eps)")
    .boolOption("nonodes", "Draw only the edges in the generated .eps output")
    .boolOption("dir", "Directed graph is generated (each edge is replaced by "
                "two directed arcs)")
    .boolOption("2con", "Create a two connected planar graph")
    .optionGroup("alg","2con")
    .boolOption("tree", "Create a min. cost spanning tree")
    .optionGroup("alg","tree")
    .boolOption("tsp", "Create a TSP tour")
    .optionGroup("alg","tsp")
    .boolOption("tsp2", "Create a TSP tour (tree based)")
    .optionGroup("alg","tsp2")
    .boolOption("dela", "Delaunay triangulation graph")
    .optionGroup("alg","dela")
    .onlyOneGroup("alg")
    .boolOption("rand", "Use time seed for random number generator")
    .optionGroup("rand", "rand")
    .intOption("seed", "Random seed", -1)
    .optionGroup("rand", "seed")
    .onlyOneGroup("rand")
    .other("[prefix]","Prefix of the output files. Default is 'lgf-gen-out'")
    .run();

  if (ap["rand"]) {
    int seed = int(time(0));
    std::cout << "Random number seed: " << seed << std::endl;
    rnd = Random(seed);
  }
  if (ap.given("seed")) {
    int seed = ap["seed"];
    std::cout << "Random number seed: " << seed << std::endl;
    rnd = Random(seed);
  }

  std::string prefix;
  switch(ap.files().size())
    {
    case 0:
      prefix="lgf-gen-out";
      break;
    case 1:
      prefix=ap.files()[0];
      break;
    default:
      std::cerr << "\nAt most one prefix can be given\n\n";
      exit(1);
    }

  double sum_sizes=0;
  std::vector<double> sizes;
  std::vector<double> cum_sizes;
  for(int s=0;s<num_of_cities;s++)
    {
      //         sum_sizes+=rnd.exponential();
      double d=rnd();
      sum_sizes+=d;
      sizes.push_back(d);
      cum_sizes.push_back(sum_sizes);
    }
  int i=0;
  for(int s=0;s<num_of_cities;s++)
    {
      Point center=(num_of_cities==1?Point(0,0):rnd.disc());
      if(gauss_d)
        for(;i<N*(cum_sizes[s]/sum_sizes);i++) {
          Node n=g.addNode();
          nodes.push_back(n);
          coords[n]=center+rnd.gauss2()*area*
            std::sqrt(sizes[s]/sum_sizes);
        }
      else if(square_d)
        for(;i<N*(cum_sizes[s]/sum_sizes);i++) {
          Node n=g.addNode();
          nodes.push_back(n);
          coords[n]=center+Point(rnd()*2-1,rnd()*2-1)*area*
            std::sqrt(sizes[s]/sum_sizes);
        }
      else if(disc_d || true)
        for(;i<N*(cum_sizes[s]/sum_sizes);i++) {
          Node n=g.addNode();
          nodes.push_back(n);
          coords[n]=center+rnd.disc()*area*
            std::sqrt(sizes[s]/sum_sizes);
        }
    }

//   for (ListGraph::NodeIt n(g); n != INVALID; ++n) {
//     std::cerr << coords[n] << std::endl;
//   }

  if(ap["tsp"]) {
    tsp();
    std::cout << "#2-opt improvements: " << tsp_impr_num << std::endl;
  }
  if(ap["tsp2"]) {
    tsp2();
    std::cout << "#2-opt improvements: " << tsp_impr_num << std::endl;
  }
  else if(ap["2con"]) {
    std::cout << "Make triangles\n";
    //   triangle();
    sparseTriangle(ap["g"]);
    std::cout << "Make it sparser\n";
    sparse2(ap["g"]);
  }
  else if(ap["tree"]) {
    minTree();
  }
  else if(ap["dela"]) {
    delaunay();
  }


  std::cout << "Number of nodes    : " << countNodes(g) << std::endl;
  std::cout << "Number of arcs    : " << countEdges(g) << std::endl;
  double tlen=0;
  for(EdgeIt e(g);e!=INVALID;++e)
    tlen+=std::sqrt((coords[g.v(e)]-coords[g.u(e)]).normSquare());
  std::cout << "Total arc length  : " << tlen << std::endl;

  if(ap["eps"])
    graphToEps(g,prefix+".eps").scaleToA4().
      scale(600).nodeScale(.005).arcWidthScale(.001).preScale(false).
      coords(coords).hideNodes(ap.given("nonodes")).run();

  if(ap["dir"])
    DigraphWriter<ListGraph>(g,prefix+".lgf").
      nodeMap("coordinates_x",scaleMap(xMap(coords),600)).
      nodeMap("coordinates_y",scaleMap(yMap(coords),600)).
      run();
  else GraphWriter<ListGraph>(g,prefix+".lgf").
         nodeMap("coordinates_x",scaleMap(xMap(coords),600)).
         nodeMap("coordinates_y",scaleMap(yMap(coords),600)).
         run();
}

