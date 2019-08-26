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

#ifndef LEMON_INSERTION_TSP_H
#define LEMON_INSERTION_TSP_H

/// \ingroup tsp
/// \file
/// \brief Insertion algorithm for symmetric TSP

#include <vector>
#include <functional>
#include <lemon/full_graph.h>
#include <lemon/maps.h>
#include <lemon/random.h>

namespace lemon {

  /// \ingroup tsp
  ///
  /// \brief Insertion algorithm for symmetric TSP.
  ///
  /// InsertionTsp implements the insertion heuristic for solving
  /// symmetric \ref tsp "TSP".
  ///
  /// This is a fast and effective tour construction method that has
  /// many variants.
  /// It starts with a subtour containing a few nodes of the graph and it
  /// iteratively inserts the other nodes into this subtour according to a
  /// certain node selection rule.
  ///
  /// This method is among the fastest TSP algorithms, and it typically
  /// provides quite good solutions (usually much better than
  /// \ref NearestNeighborTsp and \ref GreedyTsp).
  ///
  /// InsertionTsp implements four different node selection rules,
  /// from which the most effective one (\e farthest \e node \e selection)
  /// is used by default.
  /// With this choice, the algorithm runs in O(n<sup>2</sup>) time.
  /// For more information, see \ref SelectionRule.
  ///
  /// \tparam CM Type of the cost map.
  template <typename CM>
  class InsertionTsp
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
      std::vector<Node> _notused;
      std::vector<Node> _tour;
      Cost _sum;

    public:

      /// \brief Constants for specifying the node selection rule.
      ///
      /// Enum type containing constants for specifying the node selection
      /// rule for the \ref run() function.
      ///
      /// During the algorithm, nodes are selected for addition to the current
      /// subtour according to the applied rule.
      /// The FARTHEST method is one of the fastest selection rules, and
      /// it is typically the most effective, thus it is the default
      /// option. The RANDOM rule usually gives slightly worse results,
      /// but it is more robust.
      ///
      /// The desired selection rule can be specified as a parameter of the
      /// \ref run() function.
      enum SelectionRule {

        /// An unvisited node having minimum distance from the current
        /// subtour is selected at each step.
        /// The algorithm runs in O(n<sup>2</sup>) time using this
        /// selection rule.
        NEAREST,

        /// An unvisited node having maximum distance from the current
        /// subtour is selected at each step.
        /// The algorithm runs in O(n<sup>2</sup>) time using this
        /// selection rule.
        FARTHEST,

        /// An unvisited node whose insertion results in the least
        /// increase of the subtour's total cost is selected at each step.
        /// The algorithm runs in O(n<sup>3</sup>) time using this
        /// selection rule, but in most cases, it is almost as fast as
        /// with other rules.
        CHEAPEST,

        /// An unvisited node is selected randomly without any evaluation
        /// at each step.
        /// The global \ref rnd "random number generator instance" is used.
        /// You can seed it before executing the algorithm, if you
        /// would like to.
        /// The algorithm runs in O(n<sup>2</sup>) time using this
        /// selection rule.
        RANDOM
      };

    public:

      /// \brief Constructor
      ///
      /// Constructor.
      /// \param gr The \ref FullGraph "full graph" the algorithm runs on.
      /// \param cost The cost map.
      InsertionTsp(const FullGraph &gr, const CostMap &cost)
        : _gr(gr), _cost(cost) {}

      /// \name Execution Control
      /// @{

      /// \brief Runs the algorithm.
      ///
      /// This function runs the algorithm.
      ///
      /// \param rule The node selection rule. For more information, see
      /// \ref SelectionRule.
      ///
      /// \return The total cost of the found tour.
      Cost run(SelectionRule rule = FARTHEST) {
        _tour.clear();

        if (_gr.nodeNum() == 0) return _sum = 0;
        else if (_gr.nodeNum() == 1) {
          _tour.push_back(_gr(0));
          return _sum = 0;
        }

        switch (rule) {
          case NEAREST:
            init(true);
            start<ComparingSelection<std::less<Cost> >,
                  DefaultInsertion>();
            break;
          case FARTHEST:
            init(false);
            start<ComparingSelection<std::greater<Cost> >,
                  DefaultInsertion>();
            break;
          case CHEAPEST:
            init(true);
            start<CheapestSelection, CheapestInsertion>();
            break;
          case RANDOM:
            init(true);
            start<RandomSelection, DefaultInsertion>();
            break;
        }
        return _sum;
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
        return _tour;
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
        std::copy(_tour.begin(), _tour.end(), out);
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
        for (int i = 0; i < int(_tour.size()) - 1; ++i) {
          path.addBack(_gr.arc(_tour[i], _tour[i+1]));
        }
        if (int(_tour.size()) >= 2) {
          path.addBack(_gr.arc(_tour.back(), _tour.front()));
        }
      }

      /// @}

    private:

      // Initializes the algorithm
      void init(bool min) {
        Edge min_edge = min ? mapMin(_gr, _cost) : mapMax(_gr, _cost);

        _tour.clear();
        _tour.push_back(_gr.u(min_edge));
        _tour.push_back(_gr.v(min_edge));

        _notused.clear();
        for (NodeIt n(_gr); n!=INVALID; ++n) {
          if (n != _gr.u(min_edge) && n != _gr.v(min_edge)) {
            _notused.push_back(n);
          }
        }

        _sum = _cost[min_edge] * 2;
      }

      // Executes the algorithm
      template <class SelectionFunctor, class InsertionFunctor>
      void start() {
        SelectionFunctor selectNode(_gr, _cost, _tour, _notused);
        InsertionFunctor insertNode(_gr, _cost, _tour, _sum);

        for (int i=0; i<_gr.nodeNum()-2; ++i) {
          insertNode.insert(selectNode.select());
        }

        _sum = _cost[_gr.edge(_tour.back(), _tour.front())];
        for (int i = 0; i < int(_tour.size())-1; ++i) {
          _sum += _cost[_gr.edge(_tour[i], _tour[i+1])];
        }
      }


      // Implementation of the nearest and farthest selection rule
      template <typename Comparator>
      class ComparingSelection {
        public:
          ComparingSelection(const FullGraph &gr, const CostMap &cost,
                  std::vector<Node> &tour, std::vector<Node> &notused)
            : _gr(gr), _cost(cost), _tour(tour), _notused(notused),
              _dist(gr, 0), _compare()
          {
            // Compute initial distances for the unused nodes
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Node u = _notused[i];
              Cost min_dist = _cost[_gr.edge(u, _tour[0])];
              for (unsigned int j=1; j<_tour.size(); ++j) {
                Cost curr = _cost[_gr.edge(u, _tour[j])];
                if (curr < min_dist) {
                  min_dist = curr;
                }
              }
              _dist[u] = min_dist;
            }
          }

          Node select() {

            // Select an used node with minimum distance
            Cost ins_dist = 0;
            int ins_node = -1;
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Cost curr = _dist[_notused[i]];
              if (_compare(curr, ins_dist) || ins_node == -1) {
                ins_dist = curr;
                ins_node = i;
              }
            }

            // Remove the selected node from the unused vector
            Node sn = _notused[ins_node];
            _notused[ins_node] = _notused.back();
            _notused.pop_back();

            // Update the distances of the remaining nodes
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Node u = _notused[i];
              Cost nc = _cost[_gr.edge(sn, u)];
              if (nc < _dist[u]) {
                _dist[u] = nc;
              }
            }

            return sn;
          }

        private:
          const FullGraph &_gr;
          const CostMap &_cost;
          std::vector<Node> &_tour;
          std::vector<Node> &_notused;
          FullGraph::NodeMap<Cost> _dist;
          Comparator _compare;
      };

      // Implementation of the cheapest selection rule
      class CheapestSelection {
        private:
          Cost costDiff(Node u, Node v, Node w) const {
            return
              _cost[_gr.edge(u, w)] +
              _cost[_gr.edge(v, w)] -
              _cost[_gr.edge(u, v)];
          }

        public:
          CheapestSelection(const FullGraph &gr, const CostMap &cost,
                            std::vector<Node> &tour, std::vector<Node> &notused)
            : _gr(gr), _cost(cost), _tour(tour), _notused(notused),
              _ins_cost(gr, 0), _ins_pos(gr, -1)
          {
            // Compute insertion cost and position for the unused nodes
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Node u = _notused[i];
              Cost min_cost = costDiff(_tour.back(), _tour.front(), u);
              int min_pos = 0;
              for (unsigned int j=1; j<_tour.size(); ++j) {
                Cost curr_cost = costDiff(_tour[j-1], _tour[j], u);
                if (curr_cost < min_cost) {
                  min_cost = curr_cost;
                  min_pos = j;
                }
              }
              _ins_cost[u] = min_cost;
              _ins_pos[u] = min_pos;
            }
          }

          Cost select() {

            // Select an used node with minimum insertion cost
            Cost min_cost = 0;
            int min_node = -1;
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Cost curr_cost = _ins_cost[_notused[i]];
              if (curr_cost < min_cost || min_node == -1) {
                min_cost = curr_cost;
                min_node = i;
              }
            }

            // Remove the selected node from the unused vector
            Node sn = _notused[min_node];
            _notused[min_node] = _notused.back();
            _notused.pop_back();

            // Insert the selected node into the tour
            const int ipos = _ins_pos[sn];
            _tour.insert(_tour.begin() + ipos, sn);

            // Update the insertion cost and position of the remaining nodes
            for (unsigned int i=0; i<_notused.size(); ++i) {
              Node u = _notused[i];
              Cost curr_cost = _ins_cost[u];
              int curr_pos = _ins_pos[u];

              int ipos_prev = ipos == 0 ? _tour.size()-1 : ipos-1;
              int ipos_next = ipos == int(_tour.size())-1 ? 0 : ipos+1;
              Cost nc1 = costDiff(_tour[ipos_prev], _tour[ipos], u);
              Cost nc2 = costDiff(_tour[ipos], _tour[ipos_next], u);

              if (nc1 <= curr_cost || nc2 <= curr_cost) {
                // A new position is better than the old one
                if (nc1 <= nc2) {
                  curr_cost = nc1;
                  curr_pos = ipos;
                } else {
                  curr_cost = nc2;
                  curr_pos = ipos_next;
                }
              }
              else {
                if (curr_pos == ipos) {
                  // The minimum should be found again
                  curr_cost = costDiff(_tour.back(), _tour.front(), u);
                  curr_pos = 0;
                  for (unsigned int j=1; j<_tour.size(); ++j) {
                    Cost tmp_cost = costDiff(_tour[j-1], _tour[j], u);
                    if (tmp_cost < curr_cost) {
                      curr_cost = tmp_cost;
                      curr_pos = j;
                    }
                  }
                }
                else if (curr_pos > ipos) {
                  ++curr_pos;
                }
              }

              _ins_cost[u] = curr_cost;
              _ins_pos[u] = curr_pos;
            }

            return min_cost;
          }

        private:
          const FullGraph &_gr;
          const CostMap &_cost;
          std::vector<Node> &_tour;
          std::vector<Node> &_notused;
          FullGraph::NodeMap<Cost> _ins_cost;
          FullGraph::NodeMap<int> _ins_pos;
      };

      // Implementation of the random selection rule
      class RandomSelection {
        public:
          RandomSelection(const FullGraph &, const CostMap &,
                          std::vector<Node> &, std::vector<Node> &notused)
            : _notused(notused) {}

          Node select() const {
            const int index = rnd[_notused.size()];
            Node n = _notused[index];
            _notused[index] = _notused.back();
            _notused.pop_back();
            return n;
          }

        private:
          std::vector<Node> &_notused;
      };


      // Implementation of the default insertion method
      class DefaultInsertion {
        private:
          Cost costDiff(Node u, Node v, Node w) const {
            return
              _cost[_gr.edge(u, w)] +
              _cost[_gr.edge(v, w)] -
              _cost[_gr.edge(u, v)];
          }

        public:
          DefaultInsertion(const FullGraph &gr, const CostMap &cost,
                           std::vector<Node> &tour, Cost &total_cost) :
            _gr(gr), _cost(cost), _tour(tour), _total(total_cost) {}

          void insert(Node n) const {
            int min = 0;
            Cost min_val =
              costDiff(_tour.front(), _tour.back(), n);

            for (unsigned int i=1; i<_tour.size(); ++i) {
              Cost tmp = costDiff(_tour[i-1], _tour[i], n);
              if (tmp < min_val) {
                min = i;
                min_val = tmp;
              }
            }

            _tour.insert(_tour.begin()+min, n);
            _total += min_val;
          }

        private:
          const FullGraph &_gr;
          const CostMap &_cost;
          std::vector<Node> &_tour;
          Cost &_total;
      };

      // Implementation of a special insertion method for the cheapest
      // selection rule
      class CheapestInsertion {
        TEMPLATE_GRAPH_TYPEDEFS(FullGraph);
        public:
          CheapestInsertion(const FullGraph &, const CostMap &,
                            std::vector<Node> &, Cost &total_cost) :
            _total(total_cost) {}

          void insert(Cost diff) const {
            _total += diff;
          }

        private:
          Cost &_total;
      };

  };

}; // namespace lemon

#endif
