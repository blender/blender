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

#ifndef LEMON_CIRCULATION_H
#define LEMON_CIRCULATION_H

#include <lemon/tolerance.h>
#include <lemon/elevator.h>
#include <limits>

///\ingroup max_flow
///\file
///\brief Push-relabel algorithm for finding a feasible circulation.
///
namespace lemon {

  /// \brief Default traits class of Circulation class.
  ///
  /// Default traits class of Circulation class.
  ///
  /// \tparam GR Type of the digraph the algorithm runs on.
  /// \tparam LM The type of the lower bound map.
  /// \tparam UM The type of the upper bound (capacity) map.
  /// \tparam SM The type of the supply map.
  template <typename GR, typename LM,
            typename UM, typename SM>
  struct CirculationDefaultTraits {

    /// \brief The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the lower bound map.
    ///
    /// The type of the map that stores the lower bounds on the arcs.
    /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
    typedef LM LowerMap;

    /// \brief The type of the upper bound (capacity) map.
    ///
    /// The type of the map that stores the upper bounds (capacities)
    /// on the arcs.
    /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
    typedef UM UpperMap;

    /// \brief The type of supply map.
    ///
    /// The type of the map that stores the signed supply values of the
    /// nodes.
    /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
    typedef SM SupplyMap;

    /// \brief The type of the flow and supply values.
    typedef typename SupplyMap::Value Value;

    /// \brief The type of the map that stores the flow values.
    ///
    /// The type of the map that stores the flow values.
    /// It must conform to the \ref concepts::ReadWriteMap "ReadWriteMap"
    /// concept.
#ifdef DOXYGEN
    typedef GR::ArcMap<Value> FlowMap;
#else
    typedef typename Digraph::template ArcMap<Value> FlowMap;
#endif

    /// \brief Instantiates a FlowMap.
    ///
    /// This function instantiates a \ref FlowMap.
    /// \param digraph The digraph for which we would like to define
    /// the flow map.
    static FlowMap* createFlowMap(const Digraph& digraph) {
      return new FlowMap(digraph);
    }

    /// \brief The elevator type used by the algorithm.
    ///
    /// The elevator type used by the algorithm.
    ///
    /// \sa Elevator, LinkedElevator
#ifdef DOXYGEN
    typedef lemon::Elevator<GR, GR::Node> Elevator;
#else
    typedef lemon::Elevator<Digraph, typename Digraph::Node> Elevator;
#endif

    /// \brief Instantiates an Elevator.
    ///
    /// This function instantiates an \ref Elevator.
    /// \param digraph The digraph for which we would like to define
    /// the elevator.
    /// \param max_level The maximum level of the elevator.
    static Elevator* createElevator(const Digraph& digraph, int max_level) {
      return new Elevator(digraph, max_level);
    }

    /// \brief The tolerance used by the algorithm
    ///
    /// The tolerance used by the algorithm to handle inexact computation.
    typedef lemon::Tolerance<Value> Tolerance;

  };

  /**
     \brief Push-relabel algorithm for the network circulation problem.

     \ingroup max_flow
     This class implements a push-relabel algorithm for the \e network
     \e circulation problem.
     It is to find a feasible circulation when lower and upper bounds
     are given for the flow values on the arcs and lower bounds are
     given for the difference between the outgoing and incoming flow
     at the nodes.

     The exact formulation of this problem is the following.
     Let \f$G=(V,A)\f$ be a digraph, \f$lower: A\rightarrow\mathbf{R}\f$
     \f$upper: A\rightarrow\mathbf{R}\cup\{\infty\}\f$ denote the lower and
     upper bounds on the arcs, for which \f$lower(uv) \leq upper(uv)\f$
     holds for all \f$uv\in A\f$, and \f$sup: V\rightarrow\mathbf{R}\f$
     denotes the signed supply values of the nodes.
     If \f$sup(u)>0\f$, then \f$u\f$ is a supply node with \f$sup(u)\f$
     supply, if \f$sup(u)<0\f$, then \f$u\f$ is a demand node with
     \f$-sup(u)\f$ demand.
     A feasible circulation is an \f$f: A\rightarrow\mathbf{R}\f$
     solution of the following problem.

     \f[ \sum_{uv\in A} f(uv) - \sum_{vu\in A} f(vu)
     \geq sup(u) \quad \forall u\in V, \f]
     \f[ lower(uv) \leq f(uv) \leq upper(uv) \quad \forall uv\in A. \f]

     The sum of the supply values, i.e. \f$\sum_{u\in V} sup(u)\f$ must be
     zero or negative in order to have a feasible solution (since the sum
     of the expressions on the left-hand side of the inequalities is zero).
     It means that the total demand must be greater or equal to the total
     supply and all the supplies have to be carried out from the supply nodes,
     but there could be demands that are not satisfied.
     If \f$\sum_{u\in V} sup(u)\f$ is zero, then all the supply/demand
     constraints have to be satisfied with equality, i.e. all demands
     have to be satisfied and all supplies have to be used.

     If you need the opposite inequalities in the supply/demand constraints
     (i.e. the total demand is less than the total supply and all the demands
     have to be satisfied while there could be supplies that are not used),
     then you could easily transform the problem to the above form by reversing
     the direction of the arcs and taking the negative of the supply values
     (e.g. using \ref ReverseDigraph and \ref NegMap adaptors).

     This algorithm either calculates a feasible circulation, or provides
     a \ref barrier() "barrier", which prooves that a feasible soultion
     cannot exist.

     Note that this algorithm also provides a feasible solution for the
     \ref min_cost_flow "minimum cost flow problem".

     \tparam GR The type of the digraph the algorithm runs on.
     \tparam LM The type of the lower bound map. The default
     map type is \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
     \tparam UM The type of the upper bound (capacity) map.
     The default map type is \c LM.
     \tparam SM The type of the supply map. The default map type is
     \ref concepts::Digraph::NodeMap "GR::NodeMap<UM::Value>".
     \tparam TR The traits class that defines various types used by the
     algorithm. By default, it is \ref CirculationDefaultTraits
     "CirculationDefaultTraits<GR, LM, UM, SM>".
     In most cases, this parameter should not be set directly,
     consider to use the named template parameters instead.
  */
#ifdef DOXYGEN
template< typename GR,
          typename LM,
          typename UM,
          typename SM,
          typename TR >
#else
template< typename GR,
          typename LM = typename GR::template ArcMap<int>,
          typename UM = LM,
          typename SM = typename GR::template NodeMap<typename UM::Value>,
          typename TR = CirculationDefaultTraits<GR, LM, UM, SM> >
#endif
  class Circulation {
  public:

    /// \brief The \ref lemon::CirculationDefaultTraits "traits class"
    /// of the algorithm.
    typedef TR Traits;
    ///The type of the digraph the algorithm runs on.
    typedef typename Traits::Digraph Digraph;
    ///The type of the flow and supply values.
    typedef typename Traits::Value Value;

    ///The type of the lower bound map.
    typedef typename Traits::LowerMap LowerMap;
    ///The type of the upper bound (capacity) map.
    typedef typename Traits::UpperMap UpperMap;
    ///The type of the supply map.
    typedef typename Traits::SupplyMap SupplyMap;
    ///The type of the flow map.
    typedef typename Traits::FlowMap FlowMap;

    ///The type of the elevator.
    typedef typename Traits::Elevator Elevator;
    ///The type of the tolerance.
    typedef typename Traits::Tolerance Tolerance;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

    const Digraph &_g;
    int _node_num;

    const LowerMap *_lo;
    const UpperMap *_up;
    const SupplyMap *_supply;

    FlowMap *_flow;
    bool _local_flow;

    Elevator* _level;
    bool _local_level;

    typedef typename Digraph::template NodeMap<Value> ExcessMap;
    ExcessMap* _excess;

    Tolerance _tol;
    int _el;

  public:

    typedef Circulation Create;

    ///\name Named Template Parameters

    ///@{

    template <typename T>
    struct SetFlowMapTraits : public Traits {
      typedef T FlowMap;
      static FlowMap *createFlowMap(const Digraph&) {
        LEMON_ASSERT(false, "FlowMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// FlowMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting FlowMap
    /// type.
    template <typename T>
    struct SetFlowMap
      : public Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                           SetFlowMapTraits<T> > {
      typedef Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                          SetFlowMapTraits<T> > Create;
    };

    template <typename T>
    struct SetElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Digraph&, int) {
        LEMON_ASSERT(false, "Elevator is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// Elevator type
    ///
    /// \ref named-templ-param "Named parameter" for setting Elevator
    /// type. If this named parameter is used, then an external
    /// elevator object must be passed to the algorithm using the
    /// \ref elevator(Elevator&) "elevator()" function before calling
    /// \ref run() or \ref init().
    /// \sa SetStandardElevator
    template <typename T>
    struct SetElevator
      : public Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                           SetElevatorTraits<T> > {
      typedef Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                          SetElevatorTraits<T> > Create;
    };

    template <typename T>
    struct SetStandardElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Digraph& digraph, int max_level) {
        return new Elevator(digraph, max_level);
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// Elevator type with automatic allocation
    ///
    /// \ref named-templ-param "Named parameter" for setting Elevator
    /// type with automatic allocation.
    /// The Elevator should have standard constructor interface to be
    /// able to automatically created by the algorithm (i.e. the
    /// digraph and the maximum level should be passed to it).
    /// However, an external elevator object could also be passed to the
    /// algorithm with the \ref elevator(Elevator&) "elevator()" function
    /// before calling \ref run() or \ref init().
    /// \sa SetElevator
    template <typename T>
    struct SetStandardElevator
      : public Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                       SetStandardElevatorTraits<T> > {
      typedef Circulation<Digraph, LowerMap, UpperMap, SupplyMap,
                      SetStandardElevatorTraits<T> > Create;
    };

    /// @}

  protected:

    Circulation() {}

  public:

    /// Constructor.

    /// The constructor of the class.
    ///
    /// \param graph The digraph the algorithm runs on.
    /// \param lower The lower bounds for the flow values on the arcs.
    /// \param upper The upper bounds (capacities) for the flow values
    /// on the arcs.
    /// \param supply The signed supply values of the nodes.
    Circulation(const Digraph &graph, const LowerMap &lower,
                const UpperMap &upper, const SupplyMap &supply)
      : _g(graph), _lo(&lower), _up(&upper), _supply(&supply),
        _flow(NULL), _local_flow(false), _level(NULL), _local_level(false),
        _excess(NULL) {}

    /// Destructor.
    ~Circulation() {
      destroyStructures();
    }


  private:

    bool checkBoundMaps() {
      for (ArcIt e(_g);e!=INVALID;++e) {
        if (_tol.less((*_up)[e], (*_lo)[e])) return false;
      }
      return true;
    }

    void createStructures() {
      _node_num = _el = countNodes(_g);

      if (!_flow) {
        _flow = Traits::createFlowMap(_g);
        _local_flow = true;
      }
      if (!_level) {
        _level = Traits::createElevator(_g, _node_num);
        _local_level = true;
      }
      if (!_excess) {
        _excess = new ExcessMap(_g);
      }
    }

    void destroyStructures() {
      if (_local_flow) {
        delete _flow;
      }
      if (_local_level) {
        delete _level;
      }
      if (_excess) {
        delete _excess;
      }
    }

  public:

    /// Sets the lower bound map.

    /// Sets the lower bound map.
    /// \return <tt>(*this)</tt>
    Circulation& lowerMap(const LowerMap& map) {
      _lo = &map;
      return *this;
    }

    /// Sets the upper bound (capacity) map.

    /// Sets the upper bound (capacity) map.
    /// \return <tt>(*this)</tt>
    Circulation& upperMap(const UpperMap& map) {
      _up = &map;
      return *this;
    }

    /// Sets the supply map.

    /// Sets the supply map.
    /// \return <tt>(*this)</tt>
    Circulation& supplyMap(const SupplyMap& map) {
      _supply = &map;
      return *this;
    }

    /// \brief Sets the flow map.
    ///
    /// Sets the flow map.
    /// If you don't use this function before calling \ref run() or
    /// \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt>(*this)</tt>
    Circulation& flowMap(FlowMap& map) {
      if (_local_flow) {
        delete _flow;
        _local_flow = false;
      }
      _flow = &map;
      return *this;
    }

    /// \brief Sets the elevator used by algorithm.
    ///
    /// Sets the elevator used by algorithm.
    /// If you don't use this function before calling \ref run() or
    /// \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated elevator,
    /// of course.
    /// \return <tt>(*this)</tt>
    Circulation& elevator(Elevator& elevator) {
      if (_local_level) {
        delete _level;
        _local_level = false;
      }
      _level = &elevator;
      return *this;
    }

    /// \brief Returns a const reference to the elevator.
    ///
    /// Returns a const reference to the elevator.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const Elevator& elevator() const {
      return *_level;
    }

    /// \brief Sets the tolerance used by the algorithm.
    ///
    /// Sets the tolerance object used by the algorithm.
    /// \return <tt>(*this)</tt>
    Circulation& tolerance(const Tolerance& tolerance) {
      _tol = tolerance;
      return *this;
    }

    /// \brief Returns a const reference to the tolerance.
    ///
    /// Returns a const reference to the tolerance object used by
    /// the algorithm.
    const Tolerance& tolerance() const {
      return _tol;
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to call \ref run().\n
    /// If you need better control on the initial solution or the execution,
    /// you have to call one of the \ref init() functions first, then
    /// the \ref start() function.

    ///@{

    /// Initializes the internal data structures.

    /// Initializes the internal data structures and sets all flow values
    /// to the lower bound.
    void init()
    {
      LEMON_DEBUG(checkBoundMaps(),
        "Upper bounds must be greater or equal to the lower bounds");

      createStructures();

      for(NodeIt n(_g);n!=INVALID;++n) {
        (*_excess)[n] = (*_supply)[n];
      }

      for (ArcIt e(_g);e!=INVALID;++e) {
        _flow->set(e, (*_lo)[e]);
        (*_excess)[_g.target(e)] += (*_flow)[e];
        (*_excess)[_g.source(e)] -= (*_flow)[e];
      }

      // global relabeling tested, but in general case it provides
      // worse performance for random digraphs
      _level->initStart();
      for(NodeIt n(_g);n!=INVALID;++n)
        _level->initAddItem(n);
      _level->initFinish();
      for(NodeIt n(_g);n!=INVALID;++n)
        if(_tol.positive((*_excess)[n]))
          _level->activate(n);
    }

    /// Initializes the internal data structures using a greedy approach.

    /// Initializes the internal data structures using a greedy approach
    /// to construct the initial solution.
    void greedyInit()
    {
      LEMON_DEBUG(checkBoundMaps(),
        "Upper bounds must be greater or equal to the lower bounds");

      createStructures();

      for(NodeIt n(_g);n!=INVALID;++n) {
        (*_excess)[n] = (*_supply)[n];
      }

      for (ArcIt e(_g);e!=INVALID;++e) {
        if (!_tol.less(-(*_excess)[_g.target(e)], (*_up)[e])) {
          _flow->set(e, (*_up)[e]);
          (*_excess)[_g.target(e)] += (*_up)[e];
          (*_excess)[_g.source(e)] -= (*_up)[e];
        } else if (_tol.less(-(*_excess)[_g.target(e)], (*_lo)[e])) {
          _flow->set(e, (*_lo)[e]);
          (*_excess)[_g.target(e)] += (*_lo)[e];
          (*_excess)[_g.source(e)] -= (*_lo)[e];
        } else {
          Value fc = -(*_excess)[_g.target(e)];
          _flow->set(e, fc);
          (*_excess)[_g.target(e)] = 0;
          (*_excess)[_g.source(e)] -= fc;
        }
      }

      _level->initStart();
      for(NodeIt n(_g);n!=INVALID;++n)
        _level->initAddItem(n);
      _level->initFinish();
      for(NodeIt n(_g);n!=INVALID;++n)
        if(_tol.positive((*_excess)[n]))
          _level->activate(n);
    }

    ///Executes the algorithm

    ///This function executes the algorithm.
    ///
    ///\return \c true if a feasible circulation is found.
    ///
    ///\sa barrier()
    ///\sa barrierMap()
    bool start()
    {

      Node act;
      while((act=_level->highestActive())!=INVALID) {
        int actlevel=(*_level)[act];
        int mlevel=_node_num;
        Value exc=(*_excess)[act];

        for(OutArcIt e(_g,act);e!=INVALID; ++e) {
          Node v = _g.target(e);
          Value fc=(*_up)[e]-(*_flow)[e];
          if(!_tol.positive(fc)) continue;
          if((*_level)[v]<actlevel) {
            if(!_tol.less(fc, exc)) {
              _flow->set(e, (*_flow)[e] + exc);
              (*_excess)[v] += exc;
              if(!_level->active(v) && _tol.positive((*_excess)[v]))
                _level->activate(v);
              (*_excess)[act] = 0;
              _level->deactivate(act);
              goto next_l;
            }
            else {
              _flow->set(e, (*_up)[e]);
              (*_excess)[v] += fc;
              if(!_level->active(v) && _tol.positive((*_excess)[v]))
                _level->activate(v);
              exc-=fc;
            }
          }
          else if((*_level)[v]<mlevel) mlevel=(*_level)[v];
        }
        for(InArcIt e(_g,act);e!=INVALID; ++e) {
          Node v = _g.source(e);
          Value fc=(*_flow)[e]-(*_lo)[e];
          if(!_tol.positive(fc)) continue;
          if((*_level)[v]<actlevel) {
            if(!_tol.less(fc, exc)) {
              _flow->set(e, (*_flow)[e] - exc);
              (*_excess)[v] += exc;
              if(!_level->active(v) && _tol.positive((*_excess)[v]))
                _level->activate(v);
              (*_excess)[act] = 0;
              _level->deactivate(act);
              goto next_l;
            }
            else {
              _flow->set(e, (*_lo)[e]);
              (*_excess)[v] += fc;
              if(!_level->active(v) && _tol.positive((*_excess)[v]))
                _level->activate(v);
              exc-=fc;
            }
          }
          else if((*_level)[v]<mlevel) mlevel=(*_level)[v];
        }

        (*_excess)[act] = exc;
        if(!_tol.positive(exc)) _level->deactivate(act);
        else if(mlevel==_node_num) {
          _level->liftHighestActiveToTop();
          _el = _node_num;
          return false;
        }
        else {
          _level->liftHighestActive(mlevel+1);
          if(_level->onLevel(actlevel)==0) {
            _el = actlevel;
            return false;
          }
        }
      next_l:
        ;
      }
      return true;
    }

    /// Runs the algorithm.

    /// This function runs the algorithm.
    ///
    /// \return \c true if a feasible circulation is found.
    ///
    /// \note Apart from the return value, c.run() is just a shortcut of
    /// the following code.
    /// \code
    ///   c.greedyInit();
    ///   c.start();
    /// \endcode
    bool run() {
      greedyInit();
      return start();
    }

    /// @}

    /// \name Query Functions
    /// The results of the circulation algorithm can be obtained using
    /// these functions.\n
    /// Either \ref run() or \ref start() should be called before
    /// using them.

    ///@{

    /// \brief Returns the flow value on the given arc.
    ///
    /// Returns the flow value on the given arc.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value flow(const Arc& arc) const {
      return (*_flow)[arc];
    }

    /// \brief Returns a const reference to the flow map.
    ///
    /// Returns a const reference to the arc map storing the found flow.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const FlowMap& flowMap() const {
      return *_flow;
    }

    /**
       \brief Returns \c true if the given node is in a barrier.

       Barrier is a set \e B of nodes for which

       \f[ \sum_{uv\in A: u\in B} upper(uv) -
           \sum_{uv\in A: v\in B} lower(uv) < \sum_{v\in B} sup(v) \f]

       holds. The existence of a set with this property prooves that a
       feasible circualtion cannot exist.

       This function returns \c true if the given node is in the found
       barrier. If a feasible circulation is found, the function
       gives back \c false for every node.

       \pre Either \ref run() or \ref init() must be called before
       using this function.

       \sa barrierMap()
       \sa checkBarrier()
    */
    bool barrier(const Node& node) const
    {
      return (*_level)[node] >= _el;
    }

    /// \brief Gives back a barrier.
    ///
    /// This function sets \c bar to the characteristic vector of the
    /// found barrier. \c bar should be a \ref concepts::WriteMap "writable"
    /// node map with \c bool (or convertible) value type.
    ///
    /// If a feasible circulation is found, the function gives back an
    /// empty set, so \c bar[v] will be \c false for all nodes \c v.
    ///
    /// \note This function calls \ref barrier() for each node,
    /// so it runs in O(n) time.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    ///
    /// \sa barrier()
    /// \sa checkBarrier()
    template<class BarrierMap>
    void barrierMap(BarrierMap &bar) const
    {
      for(NodeIt n(_g);n!=INVALID;++n)
        bar.set(n, (*_level)[n] >= _el);
    }

    /// @}

    /// \name Checker Functions
    /// The feasibility of the results can be checked using
    /// these functions.\n
    /// Either \ref run() or \ref start() should be called before
    /// using them.

    ///@{

    ///Check if the found flow is a feasible circulation

    ///Check if the found flow is a feasible circulation,
    ///
    bool checkFlow() const {
      for(ArcIt e(_g);e!=INVALID;++e)
        if((*_flow)[e]<(*_lo)[e]||(*_flow)[e]>(*_up)[e]) return false;
      for(NodeIt n(_g);n!=INVALID;++n)
        {
          Value dif=-(*_supply)[n];
          for(InArcIt e(_g,n);e!=INVALID;++e) dif-=(*_flow)[e];
          for(OutArcIt e(_g,n);e!=INVALID;++e) dif+=(*_flow)[e];
          if(_tol.negative(dif)) return false;
        }
      return true;
    }

    ///Check whether or not the last execution provides a barrier

    ///Check whether or not the last execution provides a barrier.
    ///\sa barrier()
    ///\sa barrierMap()
    bool checkBarrier() const
    {
      Value delta=0;
      Value inf_cap = std::numeric_limits<Value>::has_infinity ?
        std::numeric_limits<Value>::infinity() :
        std::numeric_limits<Value>::max();
      for(NodeIt n(_g);n!=INVALID;++n)
        if(barrier(n))
          delta-=(*_supply)[n];
      for(ArcIt e(_g);e!=INVALID;++e)
        {
          Node s=_g.source(e);
          Node t=_g.target(e);
          if(barrier(s)&&!barrier(t)) {
            if (_tol.less(inf_cap - (*_up)[e], delta)) return false;
            delta+=(*_up)[e];
          }
          else if(barrier(t)&&!barrier(s)) delta-=(*_lo)[e];
        }
      return _tol.negative(delta);
    }

    /// @}

  };

}

#endif
