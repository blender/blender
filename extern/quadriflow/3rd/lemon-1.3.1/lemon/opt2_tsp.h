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

#ifndef LEMON_OPT2_TSP_H
#define LEMON_OPT2_TSP_H

/// \ingroup tsp
/// \file
/// \brief 2-opt algorithm for symmetric TSP.

#include <vector>
#include <lemon/full_graph.h>

namespace lemon {

  /// \ingroup tsp
  ///
  /// \brief 2-opt algorithm for symmetric TSP.
  ///
  /// Opt2Tsp implements the 2-opt heuristic for solving
  /// symmetric \ref tsp "TSP".
  ///
  /// This algorithm starts with an initial tour and iteratively improves it.
  /// At each step, it removes two edges and the reconnects the created two
  /// paths in the other way if the resulting tour is shorter.
  /// The algorithm finishes when no such 2-opt move can be applied, and so
  /// the tour is 2-optimal.
  ///
  /// If no starting tour is given to the \ref run() function, then the
  /// algorithm uses the node sequence determined by the node IDs.
  /// Oherwise, it starts with the given tour.
  ///
  /// This is a rather slow but effective method.
  /// Its typical usage is the improvement of the result of a fast tour
  /// construction heuristic (e.g. the InsertionTsp algorithm).
  ///
  /// \tparam CM Type of the cost map.
  template <typename CM>
  class Opt2Tsp
  {
    public:

      /// Type of the cost map
      typedef CM CostMap;
      /// Type of the edge costs
      typedef typename CM::Value Cost;

    private:

      GRAPH_TYPEDEFS(FullGraph);

      const FullGraph &_gr;
      const CostMap &_cost;
      Cost _sum;
      std::vector<int> _plist;
      std::vector<Node> _path;

    public:

      /// \brief Constructor
      ///
      /// Constructor.
      /// \param gr The \ref FullGraph "full graph" the algorithm runs on.
      /// \param cost The cost map.
      Opt2Tsp(const FullGraph &gr, const CostMap &cost)
        : _gr(gr), _cost(cost) {}

      /// \name Execution Control
      /// @{

      /// \brief Runs the algorithm from scratch.
      ///
      /// This function runs the algorithm starting from the tour that is
      /// determined by the node ID sequence.
      ///
      /// \return The total cost of the found tour.
      Cost run() {
        _path.clear();

        if (_gr.nodeNum() == 0) return _sum = 0;
        else if (_gr.nodeNum() == 1) {
          _path.push_back(_gr(0));
          return _sum = 0;
        }
        else if (_gr.nodeNum() == 2) {
          _path.push_back(_gr(0));
          _path.push_back(_gr(1));
          return _sum = 2 * _cost[_gr.edge(_gr(0), _gr(1))];
        }

        _plist.resize(2*_gr.nodeNum());
        for (int i = 1; i < _gr.nodeNum()-1; ++i) {
          _plist[2*i] = i-1;
          _plist[2*i+1] = i+1;
        }
        _plist[0] = _gr.nodeNum()-1;
        _plist[1] = 1;
        _plist[2*_gr.nodeNum()-2] = _gr.nodeNum()-2;
        _plist[2*_gr.nodeNum()-1] = 0;

        return start();
      }

      /// \brief Runs the algorithm starting from the given tour.
      ///
      /// This function runs the algorithm starting from the given tour.
      ///
      /// \param tour The tour as a path structure. It must be a
      /// \ref checkPath() "valid path" containing excactly n arcs.
      ///
      /// \return The total cost of the found tour.
      template <typename Path>
      Cost run(const Path& tour) {
        _path.clear();

        if (_gr.nodeNum() == 0) return _sum = 0;
        else if (_gr.nodeNum() == 1) {
          _path.push_back(_gr(0));
          return _sum = 0;
        }
        else if (_gr.nodeNum() == 2) {
          _path.push_back(_gr(0));
          _path.push_back(_gr(1));
          return _sum = 2 * _cost[_gr.edge(_gr(0), _gr(1))];
        }

        _plist.resize(2*_gr.nodeNum());
        typename Path::ArcIt it(tour);
        int first = _gr.id(_gr.source(it)),
            prev = first,
            curr = _gr.id(_gr.target(it)),
            next = -1;
        _plist[2*first+1] = curr;
        for (++it; it != INVALID; ++it) {
          next = _gr.id(_gr.target(it));
          _plist[2*curr] = prev;
          _plist[2*curr+1] = next;
          prev = curr;
          curr = next;
        }
        _plist[2*first] = prev;

        return start();
      }

      /// \brief Runs the algorithm starting from the given tour.
      ///
      /// This function runs the algorithm starting from the given tour
      /// (node sequence).
      ///
      /// \param tour A vector that stores all <tt>Node</tt>s of the graph
      /// in the desired order.
      ///
      /// \return The total cost of the found tour.
      Cost run(const std::vector<Node>& tour) {
        _path.clear();

        if (_gr.nodeNum() == 0) return _sum = 0;
        else if (_gr.nodeNum() == 1) {
          _path.push_back(_gr(0));
          return _sum = 0;
        }
        else if (_gr.nodeNum() == 2) {
          _path.push_back(_gr(0));
          _path.push_back(_gr(1));
          return _sum = 2 * _cost[_gr.edge(_gr(0), _gr(1))];
        }

        _plist.resize(2*_gr.nodeNum());
        typename std::vector<Node>::const_iterator it = tour.begin();
        int first = _gr.id(*it),
            prev = first,
            curr = _gr.id(*(++it)),
            next = -1;
        _plist[2*first+1] = curr;
        for (++it; it != tour.end(); ++it) {
          next = _gr.id(*it);
          _plist[2*curr] = prev;
          _plist[2*curr+1] = next;
          prev = curr;
          curr = next;
        }
        _plist[2*first] = curr;
        _plist[2*curr] = prev;
        _plist[2*curr+1] = first;

        return start();
      }

      /// @}

      /// \name Query Functions
      /// @{

      /// \brief The total cost of the found tour.
      ///
      /// This function returns the total cost of the found tour.
      ///
      /// \pre run() must be called before using this function.
      Cost tourCost() const {
        return _sum;
      }

      /// \brief Returns a const reference to the node sequence of the
      /// found tour.
      ///
      /// This function returns a const reference to a vector
      /// that stores the node sequence of the found tour.
      ///
      /// \pre run() must be called before using this function.
      const std::vector<Node>& tourNodes() const {
        return _path;
      }

      /// \brief Gives back the node sequence of the found tour.
      ///
      /// This function copies the node sequence of the found tour into
      /// an STL container through the given output iterator. The
      /// <tt>value_type</tt> of the container must be <tt>FullGraph::Node</tt>.
      /// For example,
      /// \code
      /// std::vector<FullGraph::Node> nodes(countNodes(graph));
      /// tsp.tourNodes(nodes.begin());
      /// \endcode
      /// or
      /// \code
      /// std::list<FullGraph::Node> nodes;
      /// tsp.tourNodes(std::back_inserter(nodes));
      /// \endcode
      ///
      /// \pre run() must be called before using this function.
      template <typename Iterator>
      void tourNodes(Iterator out) const {
        std::copy(_path.begin(), _path.end(), out);
      }

      /// \brief Gives back the found tour as a path.
      ///
      /// This function copies the found tour as a list of arcs/edges into
      /// the given \ref lemon::concepts::Path "path structure".
      ///
      /// \pre run() must be called before using this function.
      template <typename Path>
      void tour(Path &path) const {
        path.clear();
        for (int i = 0; i < int(_path.size()) - 1; ++i) {
          path.addBack(_gr.arc(_path[i], _path[i+1]));
        }
        if (int(_path.size()) >= 2) {
          path.addBack(_gr.arc(_path.back(), _path.front()));
        }
      }

      /// @}

    private:

      // Iterator class for the linked list storage of the tour
      class PathListIt {
        public:
          PathListIt(const std::vector<int> &pl, int i=0)
            : plist(&pl), act(i), last(pl[2*act]) {}
          PathListIt(const std::vector<int> &pl, int i, int l)
            : plist(&pl), act(i), last(l) {}

          int nextIndex() const {
            return (*plist)[2*act] == last ? 2*act+1 : 2*act;
          }

          int prevIndex() const {
            return (*plist)[2*act] == last ? 2*act : 2*act+1;
          }

          int next() const {
            int x = (*plist)[2*act];
            return x == last ? (*plist)[2*act+1] : x;
          }

          int prev() const {
            return last;
          }

          PathListIt& operator++() {
            int tmp = act;
            act = next();
            last = tmp;
            return *this;
          }

          operator int() const {
            return act;
          }

        private:
          const std::vector<int> *plist;
          int act;
          int last;
      };

      // Checks and applies 2-opt move (if it improves the tour)
      bool checkOpt2(const PathListIt& i, const PathListIt& j) {
        Node u  = _gr.nodeFromId(i),
             un = _gr.nodeFromId(i.next()),
             v  = _gr.nodeFromId(j),
             vn = _gr.nodeFromId(j.next());

        if (_cost[_gr.edge(u, un)] + _cost[_gr.edge(v, vn)] >
            _cost[_gr.edge(u, v)] + _cost[_gr.edge(un, vn)])
        {
          _plist[PathListIt(_plist, i.next(), i).prevIndex()] = j.next();
          _plist[PathListIt(_plist, j.next(), j).prevIndex()] = i.next();

          _plist[i.nextIndex()] = j;
          _plist[j.nextIndex()] = i;

          return true;
        }

        return false;
     }

      // Executes the algorithm from the initial tour
      Cost start() {

      restart_search:
        for (PathListIt i(_plist); true; ++i) {
          PathListIt j = i;
          if (++j == 0 || ++j == 0) break;
          for (; j != 0 && j != i.prev(); ++j) {
            if (checkOpt2(i, j))
              goto restart_search;
          }
        }

        PathListIt i(_plist);
        _path.push_back(_gr.nodeFromId(i));
        for (++i; i != 0; ++i)
          _path.push_back(_gr.nodeFromId(i));

        _sum = _cost[_gr.edge(_path.back(), _path.front())];
        for (int i = 0; i < int(_path.size())-1; ++i) {
          _sum += _cost[_gr.edge(_path[i], _path[i+1])];
        }

        return _sum;
      }

  };

}; // namespace lemon

#endif
