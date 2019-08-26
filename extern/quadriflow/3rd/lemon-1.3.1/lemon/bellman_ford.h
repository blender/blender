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

#ifndef LEMON_BELLMAN_FORD_H
#define LEMON_BELLMAN_FORD_H

/// \ingroup shortest_path
/// \file
/// \brief Bellman-Ford algorithm.

#include <lemon/list_graph.h>
#include <lemon/bits/path_dump.h>
#include <lemon/core.h>
#include <lemon/error.h>
#include <lemon/maps.h>
#include <lemon/path.h>

#include <limits>

namespace lemon {

  /// \brief Default OperationTraits for the BellmanFord algorithm class.
  ///
  /// This operation traits class defines all computational operations
  /// and constants that are used in the Bellman-Ford algorithm.
  /// The default implementation is based on the \c numeric_limits class.
  /// If the numeric type does not have infinity value, then the maximum
  /// value is used as extremal infinity value.
  template <
    typename V,
    bool has_inf = std::numeric_limits<V>::has_infinity>
  struct BellmanFordDefaultOperationTraits {
    /// \e
    typedef V Value;
    /// \brief Gives back the zero value of the type.
    static Value zero() {
      return static_cast<Value>(0);
    }
    /// \brief Gives back the positive infinity value of the type.
    static Value infinity() {
      return std::numeric_limits<Value>::infinity();
    }
    /// \brief Gives back the sum of the given two elements.
    static Value plus(const Value& left, const Value& right) {
      return left + right;
    }
    /// \brief Gives back \c true only if the first value is less than
    /// the second.
    static bool less(const Value& left, const Value& right) {
      return left < right;
    }
  };

  template <typename V>
  struct BellmanFordDefaultOperationTraits<V, false> {
    typedef V Value;
    static Value zero() {
      return static_cast<Value>(0);
    }
    static Value infinity() {
      return std::numeric_limits<Value>::max();
    }
    static Value plus(const Value& left, const Value& right) {
      if (left == infinity() || right == infinity()) return infinity();
      return left + right;
    }
    static bool less(const Value& left, const Value& right) {
      return left < right;
    }
  };

  /// \brief Default traits class of BellmanFord class.
  ///
  /// Default traits class of BellmanFord class.
  /// \param GR The type of the digraph.
  /// \param LEN The type of the length map.
  template<typename GR, typename LEN>
  struct BellmanFordDefaultTraits {
    /// The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the map that stores the arc lengths.
    ///
    /// The type of the map that stores the arc lengths.
    /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
    typedef LEN LengthMap;

    /// The type of the arc lengths.
    typedef typename LEN::Value Value;

    /// \brief Operation traits for Bellman-Ford algorithm.
    ///
    /// It defines the used operations and the infinity value for the
    /// given \c Value type.
    /// \see BellmanFordDefaultOperationTraits
    typedef BellmanFordDefaultOperationTraits<Value> OperationTraits;

    /// \brief The type of the map that stores the last arcs of the
    /// shortest paths.
    ///
    /// The type of the map that stores the last
    /// arcs of the shortest paths.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename GR::template NodeMap<typename GR::Arc> PredMap;

    /// \brief Instantiates a \c PredMap.
    ///
    /// This function instantiates a \ref PredMap.
    /// \param g is the digraph to which we would like to define the
    /// \ref PredMap.
    static PredMap *createPredMap(const GR& g) {
      return new PredMap(g);
    }

    /// \brief The type of the map that stores the distances of the nodes.
    ///
    /// The type of the map that stores the distances of the nodes.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename GR::template NodeMap<typename LEN::Value> DistMap;

    /// \brief Instantiates a \c DistMap.
    ///
    /// This function instantiates a \ref DistMap.
    /// \param g is the digraph to which we would like to define the
    /// \ref DistMap.
    static DistMap *createDistMap(const GR& g) {
      return new DistMap(g);
    }

  };

  /// \brief %BellmanFord algorithm class.
  ///
  /// \ingroup shortest_path
  /// This class provides an efficient implementation of the Bellman-Ford
  /// algorithm. The maximum time complexity of the algorithm is
  /// <tt>O(nm)</tt>.
  ///
  /// The Bellman-Ford algorithm solves the single-source shortest path
  /// problem when the arcs can have negative lengths, but the digraph
  /// should not contain directed cycles with negative total length.
  /// If all arc costs are non-negative, consider to use the Dijkstra
  /// algorithm instead, since it is more efficient.
  ///
  /// The arc lengths are passed to the algorithm using a
  /// \ref concepts::ReadMap "ReadMap", so it is easy to change it to any
  /// kind of length. The type of the length values is determined by the
  /// \ref concepts::ReadMap::Value "Value" type of the length map.
  ///
  /// There is also a \ref bellmanFord() "function-type interface" for the
  /// Bellman-Ford algorithm, which is convenient in the simplier cases and
  /// it can be used easier.
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// The default type is \ref ListDigraph.
  /// \tparam LEN A \ref concepts::ReadMap "readable" arc map that specifies
  /// the lengths of the arcs. The default map type is
  /// \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref BellmanFordDefaultTraits
  /// "BellmanFordDefaultTraits<GR, LEN>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR, typename LEN, typename TR>
#else
  template <typename GR=ListDigraph,
            typename LEN=typename GR::template ArcMap<int>,
            typename TR=BellmanFordDefaultTraits<GR,LEN> >
#endif
  class BellmanFord {
  public:

    ///The type of the underlying digraph.
    typedef typename TR::Digraph Digraph;

    /// \brief The type of the arc lengths.
    typedef typename TR::LengthMap::Value Value;
    /// \brief The type of the map that stores the arc lengths.
    typedef typename TR::LengthMap LengthMap;
    /// \brief The type of the map that stores the last
    /// arcs of the shortest paths.
    typedef typename TR::PredMap PredMap;
    /// \brief The type of the map that stores the distances of the nodes.
    typedef typename TR::DistMap DistMap;
    /// The type of the paths.
    typedef PredMapPath<Digraph, PredMap> Path;
    ///\brief The \ref lemon::BellmanFordDefaultOperationTraits
    /// "operation traits class" of the algorithm.
    typedef typename TR::OperationTraits OperationTraits;

    ///\brief The \ref lemon::BellmanFordDefaultTraits "traits class"
    ///of the algorithm.
    typedef TR Traits;

  private:

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::OutArcIt OutArcIt;

    // Pointer to the underlying digraph.
    const Digraph *_gr;
    // Pointer to the length map
    const LengthMap *_length;
    // Pointer to the map of predecessors arcs.
    PredMap *_pred;
    // Indicates if _pred is locally allocated (true) or not.
    bool _local_pred;
    // Pointer to the map of distances.
    DistMap *_dist;
    // Indicates if _dist is locally allocated (true) or not.
    bool _local_dist;

    typedef typename Digraph::template NodeMap<bool> MaskMap;
    MaskMap *_mask;

    std::vector<Node> _process;

    // Creates the maps if necessary.
    void create_maps() {
      if(!_pred) {
        _local_pred = true;
        _pred = Traits::createPredMap(*_gr);
      }
      if(!_dist) {
        _local_dist = true;
        _dist = Traits::createDistMap(*_gr);
      }
      if(!_mask) {
        _mask = new MaskMap(*_gr);
      }
    }

  public :

    typedef BellmanFord Create;

    /// \name Named Template Parameters

    ///@{

    template <class T>
    struct SetPredMapTraits : public Traits {
      typedef T PredMap;
      static PredMap *createPredMap(const Digraph&) {
        LEMON_ASSERT(false, "PredMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c PredMap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c PredMap type.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    template <class T>
    struct SetPredMap
      : public BellmanFord< Digraph, LengthMap, SetPredMapTraits<T> > {
      typedef BellmanFord< Digraph, LengthMap, SetPredMapTraits<T> > Create;
    };

    template <class T>
    struct SetDistMapTraits : public Traits {
      typedef T DistMap;
      static DistMap *createDistMap(const Digraph&) {
        LEMON_ASSERT(false, "DistMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c DistMap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c DistMap type.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    template <class T>
    struct SetDistMap
      : public BellmanFord< Digraph, LengthMap, SetDistMapTraits<T> > {
      typedef BellmanFord< Digraph, LengthMap, SetDistMapTraits<T> > Create;
    };

    template <class T>
    struct SetOperationTraitsTraits : public Traits {
      typedef T OperationTraits;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c OperationTraits type.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c OperationTraits type.
    /// For more information, see \ref BellmanFordDefaultOperationTraits.
    template <class T>
    struct SetOperationTraits
      : public BellmanFord< Digraph, LengthMap, SetOperationTraitsTraits<T> > {
      typedef BellmanFord< Digraph, LengthMap, SetOperationTraitsTraits<T> >
      Create;
    };

    ///@}

  protected:

    BellmanFord() {}

  public:

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param g The digraph the algorithm runs on.
    /// \param length The length map used by the algorithm.
    BellmanFord(const Digraph& g, const LengthMap& length) :
      _gr(&g), _length(&length),
      _pred(0), _local_pred(false),
      _dist(0), _local_dist(false), _mask(0) {}

    ///Destructor.
    ~BellmanFord() {
      if(_local_pred) delete _pred;
      if(_local_dist) delete _dist;
      if(_mask) delete _mask;
    }

    /// \brief Sets the length map.
    ///
    /// Sets the length map.
    /// \return <tt>(*this)</tt>
    BellmanFord &lengthMap(const LengthMap &map) {
      _length = &map;
      return *this;
    }

    /// \brief Sets the map that stores the predecessor arcs.
    ///
    /// Sets the map that stores the predecessor arcs.
    /// If you don't use this function before calling \ref run()
    /// or \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt>(*this)</tt>
    BellmanFord &predMap(PredMap &map) {
      if(_local_pred) {
        delete _pred;
        _local_pred=false;
      }
      _pred = &map;
      return *this;
    }

    /// \brief Sets the map that stores the distances of the nodes.
    ///
    /// Sets the map that stores the distances of the nodes calculated
    /// by the algorithm.
    /// If you don't use this function before calling \ref run()
    /// or \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt>(*this)</tt>
    BellmanFord &distMap(DistMap &map) {
      if(_local_dist) {
        delete _dist;
        _local_dist=false;
      }
      _dist = &map;
      return *this;
    }

    /// \name Execution Control
    /// The simplest way to execute the Bellman-Ford algorithm is to use
    /// one of the member functions called \ref run().\n
    /// If you need better control on the execution, you have to call
    /// \ref init() first, then you can add several source nodes
    /// with \ref addSource(). Finally the actual path computation can be
    /// performed with \ref start(), \ref checkedStart() or
    /// \ref limitedStart().

    ///@{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures. The optional parameter
    /// is the initial distance of each node.
    void init(const Value value = OperationTraits::infinity()) {
      create_maps();
      for (NodeIt it(*_gr); it != INVALID; ++it) {
        _pred->set(it, INVALID);
        _dist->set(it, value);
      }
      _process.clear();
      if (OperationTraits::less(value, OperationTraits::infinity())) {
        for (NodeIt it(*_gr); it != INVALID; ++it) {
          _process.push_back(it);
          _mask->set(it, true);
        }
      } else {
        for (NodeIt it(*_gr); it != INVALID; ++it) {
          _mask->set(it, false);
        }
      }
    }

    /// \brief Adds a new source node.
    ///
    /// This function adds a new source node. The optional second parameter
    /// is the initial distance of the node.
    void addSource(Node source, Value dst = OperationTraits::zero()) {
      _dist->set(source, dst);
      if (!(*_mask)[source]) {
        _process.push_back(source);
        _mask->set(source, true);
      }
    }

    /// \brief Executes one round from the Bellman-Ford algorithm.
    ///
    /// If the algoritm calculated the distances in the previous round
    /// exactly for the paths of at most \c k arcs, then this function
    /// will calculate the distances exactly for the paths of at most
    /// <tt>k+1</tt> arcs. Performing \c k iterations using this function
    /// calculates the shortest path distances exactly for the paths
    /// consisting of at most \c k arcs.
    ///
    /// \warning The paths with limited arc number cannot be retrieved
    /// easily with \ref path() or \ref predArc() functions. If you also
    /// need the shortest paths and not only the distances, you should
    /// store the \ref predMap() "predecessor map" after each iteration
    /// and build the path manually.
    ///
    /// \return \c true when the algorithm have not found more shorter
    /// paths.
    ///
    /// \see ActiveIt
    bool processNextRound() {
      for (int i = 0; i < int(_process.size()); ++i) {
        _mask->set(_process[i], false);
      }
      std::vector<Node> nextProcess;
      std::vector<Value> values(_process.size());
      for (int i = 0; i < int(_process.size()); ++i) {
        values[i] = (*_dist)[_process[i]];
      }
      for (int i = 0; i < int(_process.size()); ++i) {
        for (OutArcIt it(*_gr, _process[i]); it != INVALID; ++it) {
          Node target = _gr->target(it);
          Value relaxed = OperationTraits::plus(values[i], (*_length)[it]);
          if (OperationTraits::less(relaxed, (*_dist)[target])) {
            _pred->set(target, it);
            _dist->set(target, relaxed);
            if (!(*_mask)[target]) {
              _mask->set(target, true);
              nextProcess.push_back(target);
            }
          }
        }
      }
      _process.swap(nextProcess);
      return _process.empty();
    }

    /// \brief Executes one weak round from the Bellman-Ford algorithm.
    ///
    /// If the algorithm calculated the distances in the previous round
    /// at least for the paths of at most \c k arcs, then this function
    /// will calculate the distances at least for the paths of at most
    /// <tt>k+1</tt> arcs.
    /// This function does not make it possible to calculate the shortest
    /// path distances exactly for paths consisting of at most \c k arcs,
    /// this is why it is called weak round.
    ///
    /// \return \c true when the algorithm have not found more shorter
    /// paths.
    ///
    /// \see ActiveIt
    bool processNextWeakRound() {
      for (int i = 0; i < int(_process.size()); ++i) {
        _mask->set(_process[i], false);
      }
      std::vector<Node> nextProcess;
      for (int i = 0; i < int(_process.size()); ++i) {
        for (OutArcIt it(*_gr, _process[i]); it != INVALID; ++it) {
          Node target = _gr->target(it);
          Value relaxed =
            OperationTraits::plus((*_dist)[_process[i]], (*_length)[it]);
          if (OperationTraits::less(relaxed, (*_dist)[target])) {
            _pred->set(target, it);
            _dist->set(target, relaxed);
            if (!(*_mask)[target]) {
              _mask->set(target, true);
              nextProcess.push_back(target);
            }
          }
        }
      }
      _process.swap(nextProcess);
      return _process.empty();
    }

    /// \brief Executes the algorithm.
    ///
    /// Executes the algorithm.
    ///
    /// This method runs the Bellman-Ford algorithm from the root node(s)
    /// in order to compute the shortest path to each node.
    ///
    /// The algorithm computes
    /// - the shortest path tree (forest),
    /// - the distance of each node from the root(s).
    ///
    /// \pre init() must be called and at least one root node should be
    /// added with addSource() before using this function.
    void start() {
      int num = countNodes(*_gr) - 1;
      for (int i = 0; i < num; ++i) {
        if (processNextWeakRound()) break;
      }
    }

    /// \brief Executes the algorithm and checks the negative cycles.
    ///
    /// Executes the algorithm and checks the negative cycles.
    ///
    /// This method runs the Bellman-Ford algorithm from the root node(s)
    /// in order to compute the shortest path to each node and also checks
    /// if the digraph contains cycles with negative total length.
    ///
    /// The algorithm computes
    /// - the shortest path tree (forest),
    /// - the distance of each node from the root(s).
    ///
    /// \return \c false if there is a negative cycle in the digraph.
    ///
    /// \pre init() must be called and at least one root node should be
    /// added with addSource() before using this function.
    bool checkedStart() {
      int num = countNodes(*_gr);
      for (int i = 0; i < num; ++i) {
        if (processNextWeakRound()) return true;
      }
      return _process.empty();
    }

    /// \brief Executes the algorithm with arc number limit.
    ///
    /// Executes the algorithm with arc number limit.
    ///
    /// This method runs the Bellman-Ford algorithm from the root node(s)
    /// in order to compute the shortest path distance for each node
    /// using only the paths consisting of at most \c num arcs.
    ///
    /// The algorithm computes
    /// - the limited distance of each node from the root(s),
    /// - the predecessor arc for each node.
    ///
    /// \warning The paths with limited arc number cannot be retrieved
    /// easily with \ref path() or \ref predArc() functions. If you also
    /// need the shortest paths and not only the distances, you should
    /// store the \ref predMap() "predecessor map" after each iteration
    /// and build the path manually.
    ///
    /// \pre init() must be called and at least one root node should be
    /// added with addSource() before using this function.
    void limitedStart(int num) {
      for (int i = 0; i < num; ++i) {
        if (processNextRound()) break;
      }
    }

    /// \brief Runs the algorithm from the given root node.
    ///
    /// This method runs the Bellman-Ford algorithm from the given root
    /// node \c s in order to compute the shortest path to each node.
    ///
    /// The algorithm computes
    /// - the shortest path tree (forest),
    /// - the distance of each node from the root(s).
    ///
    /// \note bf.run(s) is just a shortcut of the following code.
    /// \code
    ///   bf.init();
    ///   bf.addSource(s);
    ///   bf.start();
    /// \endcode
    void run(Node s) {
      init();
      addSource(s);
      start();
    }

    /// \brief Runs the algorithm from the given root node with arc
    /// number limit.
    ///
    /// This method runs the Bellman-Ford algorithm from the given root
    /// node \c s in order to compute the shortest path distance for each
    /// node using only the paths consisting of at most \c num arcs.
    ///
    /// The algorithm computes
    /// - the limited distance of each node from the root(s),
    /// - the predecessor arc for each node.
    ///
    /// \warning The paths with limited arc number cannot be retrieved
    /// easily with \ref path() or \ref predArc() functions. If you also
    /// need the shortest paths and not only the distances, you should
    /// store the \ref predMap() "predecessor map" after each iteration
    /// and build the path manually.
    ///
    /// \note bf.run(s, num) is just a shortcut of the following code.
    /// \code
    ///   bf.init();
    ///   bf.addSource(s);
    ///   bf.limitedStart(num);
    /// \endcode
    void run(Node s, int num) {
      init();
      addSource(s);
      limitedStart(num);
    }

    ///@}

    /// \brief LEMON iterator for getting the active nodes.
    ///
    /// This class provides a common style LEMON iterator that traverses
    /// the active nodes of the Bellman-Ford algorithm after the last
    /// phase. These nodes should be checked in the next phase to
    /// find augmenting arcs outgoing from them.
    class ActiveIt {
    public:

      /// \brief Constructor.
      ///
      /// Constructor for getting the active nodes of the given BellmanFord
      /// instance.
      ActiveIt(const BellmanFord& algorithm) : _algorithm(&algorithm)
      {
        _index = _algorithm->_process.size() - 1;
      }

      /// \brief Invalid constructor.
      ///
      /// Invalid constructor.
      ActiveIt(Invalid) : _algorithm(0), _index(-1) {}

      /// \brief Conversion to \c Node.
      ///
      /// Conversion to \c Node.
      operator Node() const {
        return _index >= 0 ? _algorithm->_process[_index] : INVALID;
      }

      /// \brief Increment operator.
      ///
      /// Increment operator.
      ActiveIt& operator++() {
        --_index;
        return *this;
      }

      bool operator==(const ActiveIt& it) const {
        return static_cast<Node>(*this) == static_cast<Node>(it);
      }
      bool operator!=(const ActiveIt& it) const {
        return static_cast<Node>(*this) != static_cast<Node>(it);
      }
      bool operator<(const ActiveIt& it) const {
        return static_cast<Node>(*this) < static_cast<Node>(it);
      }

    private:
      const BellmanFord* _algorithm;
      int _index;
    };

    /// \name Query Functions
    /// The result of the Bellman-Ford algorithm can be obtained using these
    /// functions.\n
    /// Either \ref run() or \ref init() should be called before using them.

    ///@{

    /// \brief The shortest path to the given node.
    ///
    /// Gives back the shortest path to the given node from the root(s).
    ///
    /// \warning \c t should be reached from the root(s).
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Path path(Node t) const
    {
      return Path(*_gr, *_pred, t);
    }

    /// \brief The distance of the given node from the root(s).
    ///
    /// Returns the distance of the given node from the root(s).
    ///
    /// \warning If node \c v is not reached from the root(s), then
    /// the return value of this function is undefined.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value dist(Node v) const { return (*_dist)[v]; }

    /// \brief Returns the 'previous arc' of the shortest path tree for
    /// the given node.
    ///
    /// This function returns the 'previous arc' of the shortest path
    /// tree for node \c v, i.e. it returns the last arc of a
    /// shortest path from a root to \c v. It is \c INVALID if \c v
    /// is not reached from the root(s) or if \c v is a root.
    ///
    /// The shortest path tree used here is equal to the shortest path
    /// tree used in \ref predNode() and \ref predMap().
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Arc predArc(Node v) const { return (*_pred)[v]; }

    /// \brief Returns the 'previous node' of the shortest path tree for
    /// the given node.
    ///
    /// This function returns the 'previous node' of the shortest path
    /// tree for node \c v, i.e. it returns the last but one node of
    /// a shortest path from a root to \c v. It is \c INVALID if \c v
    /// is not reached from the root(s) or if \c v is a root.
    ///
    /// The shortest path tree used here is equal to the shortest path
    /// tree used in \ref predArc() and \ref predMap().
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Node predNode(Node v) const {
      return (*_pred)[v] == INVALID ? INVALID : _gr->source((*_pred)[v]);
    }

    /// \brief Returns a const reference to the node map that stores the
    /// distances of the nodes.
    ///
    /// Returns a const reference to the node map that stores the distances
    /// of the nodes calculated by the algorithm.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const DistMap &distMap() const { return *_dist;}

    /// \brief Returns a const reference to the node map that stores the
    /// predecessor arcs.
    ///
    /// Returns a const reference to the node map that stores the predecessor
    /// arcs, which form the shortest path tree (forest).
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const PredMap &predMap() const { return *_pred; }

    /// \brief Checks if a node is reached from the root(s).
    ///
    /// Returns \c true if \c v is reached from the root(s).
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    bool reached(Node v) const {
      return (*_dist)[v] != OperationTraits::infinity();
    }

    /// \brief Gives back a negative cycle.
    ///
    /// This function gives back a directed cycle with negative total
    /// length if the algorithm has already found one.
    /// Otherwise it gives back an empty path.
    lemon::Path<Digraph> negativeCycle() const {
      typename Digraph::template NodeMap<int> state(*_gr, -1);
      lemon::Path<Digraph> cycle;
      for (int i = 0; i < int(_process.size()); ++i) {
        if (state[_process[i]] != -1) continue;
        for (Node v = _process[i]; (*_pred)[v] != INVALID;
             v = _gr->source((*_pred)[v])) {
          if (state[v] == i) {
            cycle.addFront((*_pred)[v]);
            for (Node u = _gr->source((*_pred)[v]); u != v;
                 u = _gr->source((*_pred)[u])) {
              cycle.addFront((*_pred)[u]);
            }
            return cycle;
          }
          else if (state[v] >= 0) {
            break;
          }
          state[v] = i;
        }
      }
      return cycle;
    }

    ///@}
  };

  /// \brief Default traits class of bellmanFord() function.
  ///
  /// Default traits class of bellmanFord() function.
  /// \tparam GR The type of the digraph.
  /// \tparam LEN The type of the length map.
  template <typename GR, typename LEN>
  struct BellmanFordWizardDefaultTraits {
    /// The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the map that stores the arc lengths.
    ///
    /// The type of the map that stores the arc lengths.
    /// It must meet the \ref concepts::ReadMap "ReadMap" concept.
    typedef LEN LengthMap;

    /// The type of the arc lengths.
    typedef typename LEN::Value Value;

    /// \brief Operation traits for Bellman-Ford algorithm.
    ///
    /// It defines the used operations and the infinity value for the
    /// given \c Value type.
    /// \see BellmanFordDefaultOperationTraits
    typedef BellmanFordDefaultOperationTraits<Value> OperationTraits;

    /// \brief The type of the map that stores the last
    /// arcs of the shortest paths.
    ///
    /// The type of the map that stores the last arcs of the shortest paths.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename GR::template NodeMap<typename GR::Arc> PredMap;

    /// \brief Instantiates a \c PredMap.
    ///
    /// This function instantiates a \ref PredMap.
    /// \param g is the digraph to which we would like to define the
    /// \ref PredMap.
    static PredMap *createPredMap(const GR &g) {
      return new PredMap(g);
    }

    /// \brief The type of the map that stores the distances of the nodes.
    ///
    /// The type of the map that stores the distances of the nodes.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename GR::template NodeMap<Value> DistMap;

    /// \brief Instantiates a \c DistMap.
    ///
    /// This function instantiates a \ref DistMap.
    /// \param g is the digraph to which we would like to define the
    /// \ref DistMap.
    static DistMap *createDistMap(const GR &g) {
      return new DistMap(g);
    }

    ///The type of the shortest paths.

    ///The type of the shortest paths.
    ///It must meet the \ref concepts::Path "Path" concept.
    typedef lemon::Path<Digraph> Path;
  };

  /// \brief Default traits class used by BellmanFordWizard.
  ///
  /// Default traits class used by BellmanFordWizard.
  /// \tparam GR The type of the digraph.
  /// \tparam LEN The type of the length map.
  template <typename GR, typename LEN>
  class BellmanFordWizardBase
    : public BellmanFordWizardDefaultTraits<GR, LEN> {

    typedef BellmanFordWizardDefaultTraits<GR, LEN> Base;
  protected:
    // Type of the nodes in the digraph.
    typedef typename Base::Digraph::Node Node;

    // Pointer to the underlying digraph.
    void *_graph;
    // Pointer to the length map
    void *_length;
    // Pointer to the map of predecessors arcs.
    void *_pred;
    // Pointer to the map of distances.
    void *_dist;
    //Pointer to the shortest path to the target node.
    void *_path;
    //Pointer to the distance of the target node.
    void *_di;

    public:
    /// Constructor.

    /// This constructor does not require parameters, it initiates
    /// all of the attributes to default values \c 0.
    BellmanFordWizardBase() :
      _graph(0), _length(0), _pred(0), _dist(0), _path(0), _di(0) {}

    /// Constructor.

    /// This constructor requires two parameters,
    /// others are initiated to \c 0.
    /// \param gr The digraph the algorithm runs on.
    /// \param len The length map.
    BellmanFordWizardBase(const GR& gr,
                          const LEN& len) :
      _graph(reinterpret_cast<void*>(const_cast<GR*>(&gr))),
      _length(reinterpret_cast<void*>(const_cast<LEN*>(&len))),
      _pred(0), _dist(0), _path(0), _di(0) {}

  };

  /// \brief Auxiliary class for the function-type interface of the
  /// \ref BellmanFord "Bellman-Ford" algorithm.
  ///
  /// This auxiliary class is created to implement the
  /// \ref bellmanFord() "function-type interface" of the
  /// \ref BellmanFord "Bellman-Ford" algorithm.
  /// It does not have own \ref run() method, it uses the
  /// functions and features of the plain \ref BellmanFord.
  ///
  /// This class should only be used through the \ref bellmanFord()
  /// function, which makes it easier to use the algorithm.
  ///
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm.
  template<class TR>
  class BellmanFordWizard : public TR {
    typedef TR Base;

    typedef typename TR::Digraph Digraph;

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::OutArcIt ArcIt;

    typedef typename TR::LengthMap LengthMap;
    typedef typename LengthMap::Value Value;
    typedef typename TR::PredMap PredMap;
    typedef typename TR::DistMap DistMap;
    typedef typename TR::Path Path;

  public:
    /// Constructor.
    BellmanFordWizard() : TR() {}

    /// \brief Constructor that requires parameters.
    ///
    /// Constructor that requires parameters.
    /// These parameters will be the default values for the traits class.
    /// \param gr The digraph the algorithm runs on.
    /// \param len The length map.
    BellmanFordWizard(const Digraph& gr, const LengthMap& len)
      : TR(gr, len) {}

    /// \brief Copy constructor
    BellmanFordWizard(const TR &b) : TR(b) {}

    ~BellmanFordWizard() {}

    /// \brief Runs the Bellman-Ford algorithm from the given source node.
    ///
    /// This method runs the Bellman-Ford algorithm from the given source
    /// node in order to compute the shortest path to each node.
    void run(Node s) {
      BellmanFord<Digraph,LengthMap,TR>
        bf(*reinterpret_cast<const Digraph*>(Base::_graph),
           *reinterpret_cast<const LengthMap*>(Base::_length));
      if (Base::_pred) bf.predMap(*reinterpret_cast<PredMap*>(Base::_pred));
      if (Base::_dist) bf.distMap(*reinterpret_cast<DistMap*>(Base::_dist));
      bf.run(s);
    }

    /// \brief Runs the Bellman-Ford algorithm to find the shortest path
    /// between \c s and \c t.
    ///
    /// This method runs the Bellman-Ford algorithm from node \c s
    /// in order to compute the shortest path to node \c t.
    /// Actually, it computes the shortest path to each node, but using
    /// this function you can retrieve the distance and the shortest path
    /// for a single target node easier.
    ///
    /// \return \c true if \c t is reachable form \c s.
    bool run(Node s, Node t) {
      BellmanFord<Digraph,LengthMap,TR>
        bf(*reinterpret_cast<const Digraph*>(Base::_graph),
           *reinterpret_cast<const LengthMap*>(Base::_length));
      if (Base::_pred) bf.predMap(*reinterpret_cast<PredMap*>(Base::_pred));
      if (Base::_dist) bf.distMap(*reinterpret_cast<DistMap*>(Base::_dist));
      bf.run(s);
      if (Base::_path) *reinterpret_cast<Path*>(Base::_path) = bf.path(t);
      if (Base::_di) *reinterpret_cast<Value*>(Base::_di) = bf.dist(t);
      return bf.reached(t);
    }

    template<class T>
    struct SetPredMapBase : public Base {
      typedef T PredMap;
      static PredMap *createPredMap(const Digraph &) { return 0; };
      SetPredMapBase(const TR &b) : TR(b) {}
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// the predecessor map.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// the map that stores the predecessor arcs of the nodes.
    template<class T>
    BellmanFordWizard<SetPredMapBase<T> > predMap(const T &t) {
      Base::_pred=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BellmanFordWizard<SetPredMapBase<T> >(*this);
    }

    template<class T>
    struct SetDistMapBase : public Base {
      typedef T DistMap;
      static DistMap *createDistMap(const Digraph &) { return 0; };
      SetDistMapBase(const TR &b) : TR(b) {}
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// the distance map.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// the map that stores the distances of the nodes calculated
    /// by the algorithm.
    template<class T>
    BellmanFordWizard<SetDistMapBase<T> > distMap(const T &t) {
      Base::_dist=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BellmanFordWizard<SetDistMapBase<T> >(*this);
    }

    template<class T>
    struct SetPathBase : public Base {
      typedef T Path;
      SetPathBase(const TR &b) : TR(b) {}
    };

    /// \brief \ref named-func-param "Named parameter" for getting
    /// the shortest path to the target node.
    ///
    /// \ref named-func-param "Named parameter" for getting
    /// the shortest path to the target node.
    template<class T>
    BellmanFordWizard<SetPathBase<T> > path(const T &t)
    {
      Base::_path=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BellmanFordWizard<SetPathBase<T> >(*this);
    }

    /// \brief \ref named-func-param "Named parameter" for getting
    /// the distance of the target node.
    ///
    /// \ref named-func-param "Named parameter" for getting
    /// the distance of the target node.
    BellmanFordWizard dist(const Value &d)
    {
      Base::_di=reinterpret_cast<void*>(const_cast<Value*>(&d));
      return *this;
    }

  };

  /// \brief Function type interface for the \ref BellmanFord "Bellman-Ford"
  /// algorithm.
  ///
  /// \ingroup shortest_path
  /// Function type interface for the \ref BellmanFord "Bellman-Ford"
  /// algorithm.
  ///
  /// This function also has several \ref named-templ-func-param
  /// "named parameters", they are declared as the members of class
  /// \ref BellmanFordWizard.
  /// The following examples show how to use these parameters.
  /// \code
  ///   // Compute shortest path from node s to each node
  ///   bellmanFord(g,length).predMap(preds).distMap(dists).run(s);
  ///
  ///   // Compute shortest path from s to t
  ///   bool reached = bellmanFord(g,length).path(p).dist(d).run(s,t);
  /// \endcode
  /// \warning Don't forget to put the \ref BellmanFordWizard::run() "run()"
  /// to the end of the parameter list.
  /// \sa BellmanFordWizard
  /// \sa BellmanFord
  template<typename GR, typename LEN>
  BellmanFordWizard<BellmanFordWizardBase<GR,LEN> >
  bellmanFord(const GR& digraph,
              const LEN& length)
  {
    return BellmanFordWizard<BellmanFordWizardBase<GR,LEN> >(digraph, length);
  }

} //END OF NAMESPACE LEMON

#endif

