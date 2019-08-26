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

#ifndef LEMON_FIB_HEAP_H
#define LEMON_FIB_HEAP_H

///\file
///\ingroup heaps
///\brief Fibonacci heap implementation.

#include <vector>
#include <utility>
#include <functional>
#include <lemon/math.h>

namespace lemon {

  /// \ingroup heaps
  ///
  /// \brief Fibonacci heap data structure.
  ///
  /// This class implements the \e Fibonacci \e heap data structure.
  /// It fully conforms to the \ref concepts::Heap "heap concept".
  ///
  /// The methods \ref increase() and \ref erase() are not efficient in a
  /// Fibonacci heap. In case of many calls of these operations, it is
  /// better to use other heap structure, e.g. \ref BinHeap "binary heap".
  ///
  /// \tparam PR Type of the priorities of the items.
  /// \tparam IM A read-writable item map with \c int values, used
  /// internally to handle the cross references.
  /// \tparam CMP A functor class for comparing the priorities.
  /// The default is \c std::less<PR>.
#ifdef DOXYGEN
  template <typename PR, typename IM, typename CMP>
#else
  template <typename PR, typename IM, typename CMP = std::less<PR> >
#endif
  class FibHeap {
  public:

    /// Type of the item-int map.
    typedef IM ItemIntMap;
    /// Type of the priorities.
    typedef PR Prio;
    /// Type of the items stored in the heap.
    typedef typename ItemIntMap::Key Item;
    /// Type of the item-priority pairs.
    typedef std::pair<Item,Prio> Pair;
    /// Functor type for comparing the priorities.
    typedef CMP Compare;

  private:
    class Store;

    std::vector<Store> _data;
    int _minimum;
    ItemIntMap &_iim;
    Compare _comp;
    int _num;

  public:

    /// \brief Type to represent the states of the items.
    ///
    /// Each item has a state associated to it. It can be "in heap",
    /// "pre-heap" or "post-heap". The latter two are indifferent from the
    /// heap's point of view, but may be useful to the user.
    ///
    /// The item-int map must be initialized in such way that it assigns
    /// \c PRE_HEAP (<tt>-1</tt>) to any element to be put in the heap.
    enum State {
      IN_HEAP = 0,    ///< = 0.
      PRE_HEAP = -1,  ///< = -1.
      POST_HEAP = -2  ///< = -2.
    };

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    explicit FibHeap(ItemIntMap &map)
      : _minimum(0), _iim(map), _num() {}

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    /// \param comp The function object used for comparing the priorities.
    FibHeap(ItemIntMap &map, const Compare &comp)
      : _minimum(0), _iim(map), _comp(comp), _num() {}

    /// \brief The number of items stored in the heap.
    ///
    /// This function returns the number of items stored in the heap.
    int size() const { return _num; }

    /// \brief Check if the heap is empty.
    ///
    /// This function returns \c true if the heap is empty.
    bool empty() const { return _num==0; }

    /// \brief Make the heap empty.
    ///
    /// This functon makes the heap empty.
    /// It does not change the cross reference map. If you want to reuse
    /// a heap that is not surely empty, you should first clear it and
    /// then you should set the cross reference map to \c PRE_HEAP
    /// for each item.
    void clear() {
      _data.clear(); _minimum = 0; _num = 0;
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param item The item to insert.
    /// \param prio The priority of the item.
    /// \pre \e item must not be stored in the heap.
    void push (const Item& item, const Prio& prio) {
      int i=_iim[item];
      if ( i < 0 ) {
        int s=_data.size();
        _iim.set( item, s );
        Store st;
        st.name=item;
        _data.push_back(st);
        i=s;
      } else {
        _data[i].parent=_data[i].child=-1;
        _data[i].degree=0;
        _data[i].in=true;
        _data[i].marked=false;
      }

      if ( _num ) {
        _data[_data[_minimum].right_neighbor].left_neighbor=i;
        _data[i].right_neighbor=_data[_minimum].right_neighbor;
        _data[_minimum].right_neighbor=i;
        _data[i].left_neighbor=_minimum;
        if ( _comp( prio, _data[_minimum].prio) ) _minimum=i;
      } else {
        _data[i].right_neighbor=_data[i].left_neighbor=i;
        _minimum=i;
      }
      _data[i].prio=prio;
      ++_num;
    }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const { return _data[_minimum].name; }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const { return _data[_minimum].prio; }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
      /*The first case is that there are only one root.*/
      if ( _data[_minimum].left_neighbor==_minimum ) {
        _data[_minimum].in=false;
        if ( _data[_minimum].degree!=0 ) {
          makeRoot(_data[_minimum].child);
          _minimum=_data[_minimum].child;
          balance();
        }
      } else {
        int right=_data[_minimum].right_neighbor;
        unlace(_minimum);
        _data[_minimum].in=false;
        if ( _data[_minimum].degree > 0 ) {
          int left=_data[_minimum].left_neighbor;
          int child=_data[_minimum].child;
          int last_child=_data[child].left_neighbor;

          makeRoot(child);

          _data[left].right_neighbor=child;
          _data[child].left_neighbor=left;
          _data[right].left_neighbor=last_child;
          _data[last_child].right_neighbor=right;
        }
        _minimum=right;
        balance();
      } // the case where there are more roots
      --_num;
    }

    /// \brief Remove the given item from the heap.
    ///
    /// This function removes the given item from the heap if it is
    /// already stored.
    /// \param item The item to delete.
    /// \pre \e item must be in the heap.
    void erase (const Item& item) {
      int i=_iim[item];

      if ( i >= 0 && _data[i].in ) {
        if ( _data[i].parent!=-1 ) {
          int p=_data[i].parent;
          cut(i,p);
          cascade(p);
        }
        _minimum=i;     //As if its prio would be -infinity
        pop();
      }
    }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param item The item.
    /// \pre \e item must be in the heap.
    Prio operator[](const Item& item) const {
      return _data[_iim[item]].prio;
    }

    /// \brief Set the priority of an item or insert it, if it is
    /// not stored in the heap.
    ///
    /// This method sets the priority of the given item if it is
    /// already stored in the heap. Otherwise it inserts the given
    /// item into the heap with the given priority.
    /// \param item The item.
    /// \param prio The priority.
    void set (const Item& item, const Prio& prio) {
      int i=_iim[item];
      if ( i >= 0 && _data[i].in ) {
        if ( _comp(prio, _data[i].prio) ) decrease(item, prio);
        if ( _comp(_data[i].prio, prio) ) increase(item, prio);
      } else push(item, prio);
    }

    /// \brief Decrease the priority of an item to the given value.
    ///
    /// This function decreases the priority of an item to the given value.
    /// \param item The item.
    /// \param prio The priority.
    /// \pre \e item must be stored in the heap with priority at least \e prio.
    void decrease (const Item& item, const Prio& prio) {
      int i=_iim[item];
      _data[i].prio=prio;
      int p=_data[i].parent;

      if ( p!=-1 && _comp(prio, _data[p].prio) ) {
        cut(i,p);
        cascade(p);
      }
      if ( _comp(prio, _data[_minimum].prio) ) _minimum=i;
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param item The item.
    /// \param prio The priority.
    /// \pre \e item must be stored in the heap with priority at most \e prio.
    void increase (const Item& item, const Prio& prio) {
      erase(item);
      push(item, prio);
    }

    /// \brief Return the state of an item.
    ///
    /// This method returns \c PRE_HEAP if the given item has never
    /// been in the heap, \c IN_HEAP if it is in the heap at the moment,
    /// and \c POST_HEAP otherwise.
    /// In the latter case it is possible that the item will get back
    /// to the heap again.
    /// \param item The item.
    State state(const Item &item) const {
      int i=_iim[item];
      if( i>=0 ) {
        if ( _data[i].in ) i=0;
        else i=-2;
      }
      return State(i);
    }

    /// \brief Set the state of an item in the heap.
    ///
    /// This function sets the state of the given item in the heap.
    /// It can be used to manually clear the heap when it is important
    /// to achive better time complexity.
    /// \param i The item.
    /// \param st The state. It should not be \c IN_HEAP.
    void state(const Item& i, State st) {
      switch (st) {
      case POST_HEAP:
      case PRE_HEAP:
        if (state(i) == IN_HEAP) {
          erase(i);
        }
        _iim[i] = st;
        break;
      case IN_HEAP:
        break;
      }
    }

  private:

    void balance() {

      int maxdeg=int( std::floor( 2.08*log(double(_data.size()))))+1;

      std::vector<int> A(maxdeg,-1);

      /*
       *Recall that now minimum does not point to the minimum prio element.
       *We set minimum to this during balance().
       */
      int anchor=_data[_minimum].left_neighbor;
      int next=_minimum;
      bool end=false;

      do {
        int active=next;
        if ( anchor==active ) end=true;
        int d=_data[active].degree;
        next=_data[active].right_neighbor;

        while (A[d]!=-1) {
          if( _comp(_data[active].prio, _data[A[d]].prio) ) {
            fuse(active,A[d]);
          } else {
            fuse(A[d],active);
            active=A[d];
          }
          A[d]=-1;
          ++d;
        }
        A[d]=active;
      } while ( !end );


      while ( _data[_minimum].parent >=0 )
        _minimum=_data[_minimum].parent;
      int s=_minimum;
      int m=_minimum;
      do {
        if ( _comp(_data[s].prio, _data[_minimum].prio) ) _minimum=s;
        s=_data[s].right_neighbor;
      } while ( s != m );
    }

    void makeRoot(int c) {
      int s=c;
      do {
        _data[s].parent=-1;
        s=_data[s].right_neighbor;
      } while ( s != c );
    }

    void cut(int a, int b) {
      /*
       *Replacing a from the children of b.
       */
      --_data[b].degree;

      if ( _data[b].degree !=0 ) {
        int child=_data[b].child;
        if ( child==a )
          _data[b].child=_data[child].right_neighbor;
        unlace(a);
      }


      /*Lacing a to the roots.*/
      int right=_data[_minimum].right_neighbor;
      _data[_minimum].right_neighbor=a;
      _data[a].left_neighbor=_minimum;
      _data[a].right_neighbor=right;
      _data[right].left_neighbor=a;

      _data[a].parent=-1;
      _data[a].marked=false;
    }

    void cascade(int a) {
      if ( _data[a].parent!=-1 ) {
        int p=_data[a].parent;

        if ( _data[a].marked==false ) _data[a].marked=true;
        else {
          cut(a,p);
          cascade(p);
        }
      }
    }

    void fuse(int a, int b) {
      unlace(b);

      /*Lacing b under a.*/
      _data[b].parent=a;

      if (_data[a].degree==0) {
        _data[b].left_neighbor=b;
        _data[b].right_neighbor=b;
        _data[a].child=b;
      } else {
        int child=_data[a].child;
        int last_child=_data[child].left_neighbor;
        _data[child].left_neighbor=b;
        _data[b].right_neighbor=child;
        _data[last_child].right_neighbor=b;
        _data[b].left_neighbor=last_child;
      }

      ++_data[a].degree;

      _data[b].marked=false;
    }

    /*
     *It is invoked only if a has siblings.
     */
    void unlace(int a) {
      int leftn=_data[a].left_neighbor;
      int rightn=_data[a].right_neighbor;
      _data[leftn].right_neighbor=rightn;
      _data[rightn].left_neighbor=leftn;
    }


    class Store {
      friend class FibHeap;

      Item name;
      int parent;
      int left_neighbor;
      int right_neighbor;
      int child;
      int degree;
      bool marked;
      bool in;
      Prio prio;

      Store() : parent(-1), child(-1), degree(), marked(false), in(true) {}
    };
  };

} //namespace lemon

#endif //LEMON_FIB_HEAP_H

