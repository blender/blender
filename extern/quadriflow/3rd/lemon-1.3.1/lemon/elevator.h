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

#ifndef LEMON_ELEVATOR_H
#define LEMON_ELEVATOR_H

///\ingroup auxdat
///\file
///\brief Elevator class
///
///Elevator class implements an efficient data structure
///for labeling items in push-relabel type algorithms.
///

#include <lemon/core.h>
#include <lemon/bits/traits.h>

namespace lemon {

  ///Class for handling "labels" in push-relabel type algorithms.

  ///A class for handling "labels" in push-relabel type algorithms.
  ///
  ///\ingroup auxdat
  ///Using this class you can assign "labels" (nonnegative integer numbers)
  ///to the edges or nodes of a graph, manipulate and query them through
  ///operations typically arising in "push-relabel" type algorithms.
  ///
  ///Each item is either \em active or not, and you can also choose a
  ///highest level active item.
  ///
  ///\sa LinkedElevator
  ///
  ///\param GR Type of the underlying graph.
  ///\param Item Type of the items the data is assigned to (\c GR::Node,
  ///\c GR::Arc or \c GR::Edge).
  template<class GR, class Item>
  class Elevator
  {
  public:

    typedef Item Key;
    typedef int Value;

  private:

    typedef Item *Vit;
    typedef typename ItemSetTraits<GR,Item>::template Map<Vit>::Type VitMap;
    typedef typename ItemSetTraits<GR,Item>::template Map<int>::Type IntMap;

    const GR &_g;
    int _max_level;
    int _item_num;
    VitMap _where;
    IntMap _level;
    std::vector<Item> _items;
    std::vector<Vit> _first;
    std::vector<Vit> _last_active;

    int _highest_active;

    void copy(Item i, Vit p)
    {
      _where[*p=i] = p;
    }
    void copy(Vit s, Vit p)
    {
      if(s!=p)
        {
          Item i=*s;
          *p=i;
          _where[i] = p;
        }
    }
    void swap(Vit i, Vit j)
    {
      Item ti=*i;
      Vit ct = _where[ti];
      _where[ti] = _where[*i=*j];
      _where[*j] = ct;
      *j=ti;
    }

  public:

    ///Constructor with given maximum level.

    ///Constructor with given maximum level.
    ///
    ///\param graph The underlying graph.
    ///\param max_level The maximum allowed level.
    ///Set the range of the possible labels to <tt>[0..max_level]</tt>.
    Elevator(const GR &graph,int max_level) :
      _g(graph),
      _max_level(max_level),
      _item_num(_max_level),
      _where(graph),
      _level(graph,0),
      _items(_max_level),
      _first(_max_level+2),
      _last_active(_max_level+2),
      _highest_active(-1) {}
    ///Constructor.

    ///Constructor.
    ///
    ///\param graph The underlying graph.
    ///Set the range of the possible labels to <tt>[0..max_level]</tt>,
    ///where \c max_level is equal to the number of labeled items in the graph.
    Elevator(const GR &graph) :
      _g(graph),
      _max_level(countItems<GR, Item>(graph)),
      _item_num(_max_level),
      _where(graph),
      _level(graph,0),
      _items(_max_level),
      _first(_max_level+2),
      _last_active(_max_level+2),
      _highest_active(-1)
    {
    }

    ///Activate item \c i.

    ///Activate item \c i.
    ///\pre Item \c i shouldn't be active before.
    void activate(Item i)
    {
      const int l=_level[i];
      swap(_where[i],++_last_active[l]);
      if(l>_highest_active) _highest_active=l;
    }

    ///Deactivate item \c i.

    ///Deactivate item \c i.
    ///\pre Item \c i must be active before.
    void deactivate(Item i)
    {
      swap(_where[i],_last_active[_level[i]]--);
      while(_highest_active>=0 &&
            _last_active[_highest_active]<_first[_highest_active])
        _highest_active--;
    }

    ///Query whether item \c i is active
    bool active(Item i) const { return _where[i]<=_last_active[_level[i]]; }

    ///Return the level of item \c i.
    int operator[](Item i) const { return _level[i]; }

    ///Return the number of items on level \c l.
    int onLevel(int l) const
    {
      return _first[l+1]-_first[l];
    }
    ///Return true if level \c l is empty.
    bool emptyLevel(int l) const
    {
      return _first[l+1]-_first[l]==0;
    }
    ///Return the number of items above level \c l.
    int aboveLevel(int l) const
    {
      return _first[_max_level+1]-_first[l+1];
    }
    ///Return the number of active items on level \c l.
    int activesOnLevel(int l) const
    {
      return _last_active[l]-_first[l]+1;
    }
    ///Return true if there is no active item on level \c l.
    bool activeFree(int l) const
    {
      return _last_active[l]<_first[l];
    }
    ///Return the maximum allowed level.
    int maxLevel() const
    {
      return _max_level;
    }

    ///\name Highest Active Item
    ///Functions for working with the highest level
    ///active item.

    ///@{

    ///Return a highest level active item.

    ///Return a highest level active item or INVALID if there is no active
    ///item.
    Item highestActive() const
    {
      return _highest_active>=0?*_last_active[_highest_active]:INVALID;
    }

    ///Return the highest active level.

    ///Return the level of the highest active item or -1 if there is no active
    ///item.
    int highestActiveLevel() const
    {
      return _highest_active;
    }

    ///Lift the highest active item by one.

    ///Lift the item returned by highestActive() by one.
    ///
    void liftHighestActive()
    {
      Item it = *_last_active[_highest_active];
      ++_level[it];
      swap(_last_active[_highest_active]--,_last_active[_highest_active+1]);
      --_first[++_highest_active];
    }

    ///Lift the highest active item to the given level.

    ///Lift the item returned by highestActive() to level \c new_level.
    ///
    ///\warning \c new_level must be strictly higher
    ///than the current level.
    ///
    void liftHighestActive(int new_level)
    {
      const Item li = *_last_active[_highest_active];

      copy(--_first[_highest_active+1],_last_active[_highest_active]--);
      for(int l=_highest_active+1;l<new_level;l++)
        {
          copy(--_first[l+1],_first[l]);
          --_last_active[l];
        }
      copy(li,_first[new_level]);
      _level[li] = new_level;
      _highest_active=new_level;
    }

    ///Lift the highest active item to the top level.

    ///Lift the item returned by highestActive() to the top level and
    ///deactivate it.
    void liftHighestActiveToTop()
    {
      const Item li = *_last_active[_highest_active];

      copy(--_first[_highest_active+1],_last_active[_highest_active]--);
      for(int l=_highest_active+1;l<_max_level;l++)
        {
          copy(--_first[l+1],_first[l]);
          --_last_active[l];
        }
      copy(li,_first[_max_level]);
      --_last_active[_max_level];
      _level[li] = _max_level;

      while(_highest_active>=0 &&
            _last_active[_highest_active]<_first[_highest_active])
        _highest_active--;
    }

    ///@}

    ///\name Active Item on Certain Level
    ///Functions for working with the active items.

    ///@{

    ///Return an active item on level \c l.

    ///Return an active item on level \c l or \ref INVALID if there is no such
    ///an item. (\c l must be from the range [0...\c max_level].
    Item activeOn(int l) const
    {
      return _last_active[l]>=_first[l]?*_last_active[l]:INVALID;
    }

    ///Lift the active item returned by \c activeOn(level) by one.

    ///Lift the active item returned by \ref activeOn() "activeOn(level)"
    ///by one.
    Item liftActiveOn(int level)
    {
      Item it =*_last_active[level];
      ++_level[it];
      swap(_last_active[level]--, --_first[level+1]);
      if (level+1>_highest_active) ++_highest_active;
    }

    ///Lift the active item returned by \c activeOn(level) to the given level.

    ///Lift the active item returned by \ref activeOn() "activeOn(level)"
    ///to the given level.
    void liftActiveOn(int level, int new_level)
    {
      const Item ai = *_last_active[level];

      copy(--_first[level+1], _last_active[level]--);
      for(int l=level+1;l<new_level;l++)
        {
          copy(_last_active[l],_first[l]);
          copy(--_first[l+1], _last_active[l]--);
        }
      copy(ai,_first[new_level]);
      _level[ai] = new_level;
      if (new_level>_highest_active) _highest_active=new_level;
    }

    ///Lift the active item returned by \c activeOn(level) to the top level.

    ///Lift the active item returned by \ref activeOn() "activeOn(level)"
    ///to the top level and deactivate it.
    void liftActiveToTop(int level)
    {
      const Item ai = *_last_active[level];

      copy(--_first[level+1],_last_active[level]--);
      for(int l=level+1;l<_max_level;l++)
        {
          copy(_last_active[l],_first[l]);
          copy(--_first[l+1], _last_active[l]--);
        }
      copy(ai,_first[_max_level]);
      --_last_active[_max_level];
      _level[ai] = _max_level;

      if (_highest_active==level) {
        while(_highest_active>=0 &&
              _last_active[_highest_active]<_first[_highest_active])
          _highest_active--;
      }
    }

    ///@}

    ///Lift an active item to a higher level.

    ///Lift an active item to a higher level.
    ///\param i The item to be lifted. It must be active.
    ///\param new_level The new level of \c i. It must be strictly higher
    ///than the current level.
    ///
    void lift(Item i, int new_level)
    {
      const int lo = _level[i];
      const Vit w = _where[i];

      copy(_last_active[lo],w);
      copy(--_first[lo+1],_last_active[lo]--);
      for(int l=lo+1;l<new_level;l++)
        {
          copy(_last_active[l],_first[l]);
          copy(--_first[l+1],_last_active[l]--);
        }
      copy(i,_first[new_level]);
      _level[i] = new_level;
      if(new_level>_highest_active) _highest_active=new_level;
    }

    ///Move an inactive item to the top but one level (in a dirty way).

    ///This function moves an inactive item from the top level to the top
    ///but one level (in a dirty way).
    ///\warning It makes the underlying datastructure corrupt, so use it
    ///only if you really know what it is for.
    ///\pre The item is on the top level.
    void dirtyTopButOne(Item i) {
      _level[i] = _max_level - 1;
    }

    ///Lift all items on and above the given level to the top level.

    ///This function lifts all items on and above level \c l to the top
    ///level and deactivates them.
    void liftToTop(int l)
    {
      const Vit f=_first[l];
      const Vit tl=_first[_max_level];
      for(Vit i=f;i!=tl;++i)
        _level[*i] = _max_level;
      for(int i=l;i<=_max_level;i++)
        {
          _first[i]=f;
          _last_active[i]=f-1;
        }
      for(_highest_active=l-1;
          _highest_active>=0 &&
            _last_active[_highest_active]<_first[_highest_active];
          _highest_active--) ;
    }

  private:
    int _init_lev;
    Vit _init_num;

  public:

    ///\name Initialization
    ///Using these functions you can initialize the levels of the items.
    ///\n
    ///The initialization must be started with calling \c initStart().
    ///Then the items should be listed level by level starting with the
    ///lowest one (level 0) using \c initAddItem() and \c initNewLevel().
    ///Finally \c initFinish() must be called.
    ///The items not listed are put on the highest level.
    ///@{

    ///Start the initialization process.
    void initStart()
    {
      _init_lev=0;
      _init_num=&_items[0];
      _first[0]=&_items[0];
      _last_active[0]=&_items[0]-1;
      Vit n=&_items[0];
      for(typename ItemSetTraits<GR,Item>::ItemIt i(_g);i!=INVALID;++i)
        {
          *n=i;
          _where[i] = n;
          _level[i] = _max_level;
          ++n;
        }
    }

    ///Add an item to the current level.
    void initAddItem(Item i)
    {
      swap(_where[i],_init_num);
      _level[i] = _init_lev;
      ++_init_num;
    }

    ///Start a new level.

    ///Start a new level.
    ///It shouldn't be used before the items on level 0 are listed.
    void initNewLevel()
    {
      _init_lev++;
      _first[_init_lev]=_init_num;
      _last_active[_init_lev]=_init_num-1;
    }

    ///Finalize the initialization process.
    void initFinish()
    {
      for(_init_lev++;_init_lev<=_max_level;_init_lev++)
        {
          _first[_init_lev]=_init_num;
          _last_active[_init_lev]=_init_num-1;
        }
      _first[_max_level+1]=&_items[0]+_item_num;
      _last_active[_max_level+1]=&_items[0]+_item_num-1;
      _highest_active = -1;
    }

    ///@}

  };

  ///Class for handling "labels" in push-relabel type algorithms.

  ///A class for handling "labels" in push-relabel type algorithms.
  ///
  ///\ingroup auxdat
  ///Using this class you can assign "labels" (nonnegative integer numbers)
  ///to the edges or nodes of a graph, manipulate and query them through
  ///operations typically arising in "push-relabel" type algorithms.
  ///
  ///Each item is either \em active or not, and you can also choose a
  ///highest level active item.
  ///
  ///\sa Elevator
  ///
  ///\param GR Type of the underlying graph.
  ///\param Item Type of the items the data is assigned to (\c GR::Node,
  ///\c GR::Arc or \c GR::Edge).
  template <class GR, class Item>
  class LinkedElevator {
  public:

    typedef Item Key;
    typedef int Value;

  private:

    typedef typename ItemSetTraits<GR,Item>::
    template Map<Item>::Type ItemMap;
    typedef typename ItemSetTraits<GR,Item>::
    template Map<int>::Type IntMap;
    typedef typename ItemSetTraits<GR,Item>::
    template Map<bool>::Type BoolMap;

    const GR &_graph;
    int _max_level;
    int _item_num;
    std::vector<Item> _first, _last;
    ItemMap _prev, _next;
    int _highest_active;
    IntMap _level;
    BoolMap _active;

  public:
    ///Constructor with given maximum level.

    ///Constructor with given maximum level.
    ///
    ///\param graph The underlying graph.
    ///\param max_level The maximum allowed level.
    ///Set the range of the possible labels to <tt>[0..max_level]</tt>.
    LinkedElevator(const GR& graph, int max_level)
      : _graph(graph), _max_level(max_level), _item_num(_max_level),
        _first(_max_level + 1), _last(_max_level + 1),
        _prev(graph), _next(graph),
        _highest_active(-1), _level(graph), _active(graph) {}

    ///Constructor.

    ///Constructor.
    ///
    ///\param graph The underlying graph.
    ///Set the range of the possible labels to <tt>[0..max_level]</tt>,
    ///where \c max_level is equal to the number of labeled items in the graph.
    LinkedElevator(const GR& graph)
      : _graph(graph), _max_level(countItems<GR, Item>(graph)),
        _item_num(_max_level),
        _first(_max_level + 1), _last(_max_level + 1),
        _prev(graph, INVALID), _next(graph, INVALID),
        _highest_active(-1), _level(graph), _active(graph) {}


    ///Activate item \c i.

    ///Activate item \c i.
    ///\pre Item \c i shouldn't be active before.
    void activate(Item i) {
      _active[i] = true;

      int level = _level[i];
      if (level > _highest_active) {
        _highest_active = level;
      }

      if (_prev[i] == INVALID || _active[_prev[i]]) return;
      //unlace
      _next[_prev[i]] = _next[i];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = _prev[i];
      } else {
        _last[level] = _prev[i];
      }
      //lace
      _next[i] = _first[level];
      _prev[_first[level]] = i;
      _prev[i] = INVALID;
      _first[level] = i;

    }

    ///Deactivate item \c i.

    ///Deactivate item \c i.
    ///\pre Item \c i must be active before.
    void deactivate(Item i) {
      _active[i] = false;
      int level = _level[i];

      if (_next[i] == INVALID || !_active[_next[i]])
        goto find_highest_level;

      //unlace
      _prev[_next[i]] = _prev[i];
      if (_prev[i] != INVALID) {
        _next[_prev[i]] = _next[i];
      } else {
        _first[_level[i]] = _next[i];
      }
      //lace
      _prev[i] = _last[level];
      _next[_last[level]] = i;
      _next[i] = INVALID;
      _last[level] = i;

    find_highest_level:
      if (level == _highest_active) {
        while (_highest_active >= 0 && activeFree(_highest_active))
          --_highest_active;
      }
    }

    ///Query whether item \c i is active
    bool active(Item i) const { return _active[i]; }

    ///Return the level of item \c i.
    int operator[](Item i) const { return _level[i]; }

    ///Return the number of items on level \c l.
    int onLevel(int l) const {
      int num = 0;
      Item n = _first[l];
      while (n != INVALID) {
        ++num;
        n = _next[n];
      }
      return num;
    }

    ///Return true if the level is empty.
    bool emptyLevel(int l) const {
      return _first[l] == INVALID;
    }

    ///Return the number of items above level \c l.
    int aboveLevel(int l) const {
      int num = 0;
      for (int level = l + 1; level < _max_level; ++level)
        num += onLevel(level);
      return num;
    }

    ///Return the number of active items on level \c l.
    int activesOnLevel(int l) const {
      int num = 0;
      Item n = _first[l];
      while (n != INVALID && _active[n]) {
        ++num;
        n = _next[n];
      }
      return num;
    }

    ///Return true if there is no active item on level \c l.
    bool activeFree(int l) const {
      return _first[l] == INVALID || !_active[_first[l]];
    }

    ///Return the maximum allowed level.
    int maxLevel() const {
      return _max_level;
    }

    ///\name Highest Active Item
    ///Functions for working with the highest level
    ///active item.

    ///@{

    ///Return a highest level active item.

    ///Return a highest level active item or INVALID if there is no active
    ///item.
    Item highestActive() const {
      return _highest_active >= 0 ? _first[_highest_active] : INVALID;
    }

    ///Return the highest active level.

    ///Return the level of the highest active item or -1 if there is no active
    ///item.
    int highestActiveLevel() const {
      return _highest_active;
    }

    ///Lift the highest active item by one.

    ///Lift the item returned by highestActive() by one.
    ///
    void liftHighestActive() {
      Item i = _first[_highest_active];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[_highest_active] = _next[i];
      } else {
        _first[_highest_active] = INVALID;
        _last[_highest_active] = INVALID;
      }
      _level[i] = ++_highest_active;
      if (_first[_highest_active] == INVALID) {
        _first[_highest_active] = i;
        _last[_highest_active] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[_first[_highest_active]] = i;
        _next[i] = _first[_highest_active];
        _first[_highest_active] = i;
      }
    }

    ///Lift the highest active item to the given level.

    ///Lift the item returned by highestActive() to level \c new_level.
    ///
    ///\warning \c new_level must be strictly higher
    ///than the current level.
    ///
    void liftHighestActive(int new_level) {
      Item i = _first[_highest_active];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[_highest_active] = _next[i];
      } else {
        _first[_highest_active] = INVALID;
        _last[_highest_active] = INVALID;
      }
      _level[i] = _highest_active = new_level;
      if (_first[_highest_active] == INVALID) {
        _first[_highest_active] = _last[_highest_active] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[_first[_highest_active]] = i;
        _next[i] = _first[_highest_active];
        _first[_highest_active] = i;
      }
    }

    ///Lift the highest active item to the top level.

    ///Lift the item returned by highestActive() to the top level and
    ///deactivate it.
    void liftHighestActiveToTop() {
      Item i = _first[_highest_active];
      _level[i] = _max_level;
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[_highest_active] = _next[i];
      } else {
        _first[_highest_active] = INVALID;
        _last[_highest_active] = INVALID;
      }
      while (_highest_active >= 0 && activeFree(_highest_active))
        --_highest_active;
    }

    ///@}

    ///\name Active Item on Certain Level
    ///Functions for working with the active items.

    ///@{

    ///Return an active item on level \c l.

    ///Return an active item on level \c l or \ref INVALID if there is no such
    ///an item. (\c l must be from the range [0...\c max_level].
    Item activeOn(int l) const
    {
      return _active[_first[l]] ? _first[l] : INVALID;
    }

    ///Lift the active item returned by \c activeOn(l) by one.

    ///Lift the active item returned by \ref activeOn() "activeOn(l)"
    ///by one.
    Item liftActiveOn(int l)
    {
      Item i = _first[l];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[l] = _next[i];
      } else {
        _first[l] = INVALID;
        _last[l] = INVALID;
      }
      _level[i] = ++l;
      if (_first[l] == INVALID) {
        _first[l] = _last[l] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[_first[l]] = i;
        _next[i] = _first[l];
        _first[l] = i;
      }
      if (_highest_active < l) {
        _highest_active = l;
      }
    }

    ///Lift the active item returned by \c activeOn(l) to the given level.

    ///Lift the active item returned by \ref activeOn() "activeOn(l)"
    ///to the given level.
    void liftActiveOn(int l, int new_level)
    {
      Item i = _first[l];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[l] = _next[i];
      } else {
        _first[l] = INVALID;
        _last[l] = INVALID;
      }
      _level[i] = l = new_level;
      if (_first[l] == INVALID) {
        _first[l] = _last[l] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[_first[l]] = i;
        _next[i] = _first[l];
        _first[l] = i;
      }
      if (_highest_active < l) {
        _highest_active = l;
      }
    }

    ///Lift the active item returned by \c activeOn(l) to the top level.

    ///Lift the active item returned by \ref activeOn() "activeOn(l)"
    ///to the top level and deactivate it.
    void liftActiveToTop(int l)
    {
      Item i = _first[l];
      if (_next[i] != INVALID) {
        _prev[_next[i]] = INVALID;
        _first[l] = _next[i];
      } else {
        _first[l] = INVALID;
        _last[l] = INVALID;
      }
      _level[i] = _max_level;
      if (l == _highest_active) {
        while (_highest_active >= 0 && activeFree(_highest_active))
          --_highest_active;
      }
    }

    ///@}

    /// \brief Lift an active item to a higher level.
    ///
    /// Lift an active item to a higher level.
    /// \param i The item to be lifted. It must be active.
    /// \param new_level The new level of \c i. It must be strictly higher
    /// than the current level.
    ///
    void lift(Item i, int new_level) {
      if (_next[i] != INVALID) {
        _prev[_next[i]] = _prev[i];
      } else {
        _last[new_level] = _prev[i];
      }
      if (_prev[i] != INVALID) {
        _next[_prev[i]] = _next[i];
      } else {
        _first[new_level] = _next[i];
      }
      _level[i] = new_level;
      if (_first[new_level] == INVALID) {
        _first[new_level] = _last[new_level] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[_first[new_level]] = i;
        _next[i] = _first[new_level];
        _first[new_level] = i;
      }
      if (_highest_active < new_level) {
        _highest_active = new_level;
      }
    }

    ///Move an inactive item to the top but one level (in a dirty way).

    ///This function moves an inactive item from the top level to the top
    ///but one level (in a dirty way).
    ///\warning It makes the underlying datastructure corrupt, so use it
    ///only if you really know what it is for.
    ///\pre The item is on the top level.
    void dirtyTopButOne(Item i) {
      _level[i] = _max_level - 1;
    }

    ///Lift all items on and above the given level to the top level.

    ///This function lifts all items on and above level \c l to the top
    ///level and deactivates them.
    void liftToTop(int l)  {
      for (int i = l + 1; _first[i] != INVALID; ++i) {
        Item n = _first[i];
        while (n != INVALID) {
          _level[n] = _max_level;
          n = _next[n];
        }
        _first[i] = INVALID;
        _last[i] = INVALID;
      }
      if (_highest_active > l - 1) {
        _highest_active = l - 1;
        while (_highest_active >= 0 && activeFree(_highest_active))
          --_highest_active;
      }
    }

  private:

    int _init_level;

  public:

    ///\name Initialization
    ///Using these functions you can initialize the levels of the items.
    ///\n
    ///The initialization must be started with calling \c initStart().
    ///Then the items should be listed level by level starting with the
    ///lowest one (level 0) using \c initAddItem() and \c initNewLevel().
    ///Finally \c initFinish() must be called.
    ///The items not listed are put on the highest level.
    ///@{

    ///Start the initialization process.
    void initStart() {

      for (int i = 0; i <= _max_level; ++i) {
        _first[i] = _last[i] = INVALID;
      }
      _init_level = 0;
      for(typename ItemSetTraits<GR,Item>::ItemIt i(_graph);
          i != INVALID; ++i) {
        _level[i] = _max_level;
        _active[i] = false;
      }
    }

    ///Add an item to the current level.
    void initAddItem(Item i) {
      _level[i] = _init_level;
      if (_last[_init_level] == INVALID) {
        _first[_init_level] = i;
        _last[_init_level] = i;
        _prev[i] = INVALID;
        _next[i] = INVALID;
      } else {
        _prev[i] = _last[_init_level];
        _next[i] = INVALID;
        _next[_last[_init_level]] = i;
        _last[_init_level] = i;
      }
    }

    ///Start a new level.

    ///Start a new level.
    ///It shouldn't be used before the items on level 0 are listed.
    void initNewLevel() {
      ++_init_level;
    }

    ///Finalize the initialization process.
    void initFinish() {
      _highest_active = -1;
    }

    ///@}

  };


} //END OF NAMESPACE LEMON

#endif

