/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2010
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

#ifndef LEMON_BINOMIAL_HEAP_H
#define LEMON_BINOMIAL_HEAP_H

///\file
///\ingroup heaps
///\brief Binomial Heap implementation.

#include <vector>
#include <utility>
#include <functional>
#include <lemon/math.h>
#include <lemon/counter.h>

namespace lemon {

  /// \ingroup heaps
  ///
  ///\brief Binomial heap data structure.
  ///
  /// This class implements the \e binomial \e heap data structure.
  /// It fully conforms to the \ref concepts::Heap "heap concept".
  ///
  /// The methods \ref increase() and \ref erase() are not efficient
  /// in a binomial heap. In case of many calls of these operations,
  /// it is better to use other heap structure, e.g. \ref BinHeap
  /// "binary heap".
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
  class BinomialHeap {
  public:
    /// Type of the item-int map.
    typedef IM ItemIntMap;
    /// Type of the priorities.
    typedef PR Prio;
    /// Type of the items stored in the heap.
    typedef typename ItemIntMap::Key Item;
    /// Functor type for comparing the priorities.
    typedef CMP Compare;

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

  private:
    class Store;

    std::vector<Store> _data;
    int _min, _head;
    ItemIntMap &_iim;
    Compare _comp;
    int _num_items;

  public:
    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    explicit BinomialHeap(ItemIntMap &map)
      : _min(0), _head(-1), _iim(map), _num_items(0) {}

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    /// \param comp The function object used for comparing the priorities.
    BinomialHeap(ItemIntMap &map, const Compare &comp)
      : _min(0), _head(-1), _iim(map), _comp(comp), _num_items(0) {}

    /// \brief The number of items stored in the heap.
    ///
    /// This function returns the number of items stored in the heap.
    int size() const { return _num_items; }

    /// \brief Check if the heap is empty.
    ///
    /// This function returns \c true if the heap is empty.
    bool empty() const { return _num_items==0; }

    /// \brief Make the heap empty.
    ///
    /// This functon makes the heap empty.
    /// It does not change the cross reference map. If you want to reuse
    /// a heap that is not surely empty, you should first clear it and
    /// then you should set the cross reference map to \c PRE_HEAP
    /// for each item.
    void clear() {
      _data.clear(); _min=0; _num_items=0; _head=-1;
    }

    /// \brief Set the priority of an item or insert it, if it is
    /// not stored in the heap.
    ///
    /// This method sets the priority of the given item if it is
    /// already stored in the heap. Otherwise it inserts the given
    /// item into the heap with the given priority.
    /// \param item The item.
    /// \param value The priority.
    void set (const Item& item, const Prio& value) {
      int i=_iim[item];
      if ( i >= 0 && _data[i].in ) {
        if ( _comp(value, _data[i].prio) ) decrease(item, value);
        if ( _comp(_data[i].prio, value) ) increase(item, value);
      } else push(item, value);
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param item The item to insert.
    /// \param value The priority of the item.
    /// \pre \e item must not be stored in the heap.
    void push (const Item& item, const Prio& value) {
      int i=_iim[item];
      if ( i<0 ) {
        int s=_data.size();
        _iim.set( item,s );
        Store st;
        st.name=item;
        st.prio=value;
        _data.push_back(st);
        i=s;
      }
      else {
        _data[i].parent=_data[i].right_neighbor=_data[i].child=-1;
        _data[i].degree=0;
        _data[i].in=true;
        _data[i].prio=value;
      }

      if( 0==_num_items ) {
        _head=i;
        _min=i;
      } else {
        merge(i);
        if( _comp(_data[i].prio, _data[_min].prio) ) _min=i;
      }
      ++_num_items;
    }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const { return _data[_min].name; }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const { return _data[_min].prio; }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param item The item.
    /// \pre \e item must be in the heap.
    const Prio& operator[](const Item& item) const {
      return _data[_iim[item]].prio;
    }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
      _data[_min].in=false;

      int head_child=-1;
      if ( _data[_min].child!=-1 ) {
        int child=_data[_min].child;
        int neighb;
        while( child!=-1 ) {
          neighb=_data[child].right_neighbor;
          _data[child].parent=-1;
          _data[child].right_neighbor=head_child;
          head_child=child;
          child=neighb;
        }
      }

      if ( _data[_head].right_neighbor==-1 ) {
        // there was only one root
        _head=head_child;
      }
      else {
        // there were more roots
        if( _head!=_min )  { unlace(_min); }
        else { _head=_data[_head].right_neighbor; }
        merge(head_child);
      }
      _min=findMin();
      --_num_items;
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
        decrease( item, _data[_min].prio-1 );
        pop();
      }
    }

    /// \brief Decrease the priority of an item to the given value.
    ///
    /// This function decreases the priority of an item to the given value.
    /// \param item The item.
    /// \param value The priority.
    /// \pre \e item must be stored in the heap with priority at least \e value.
    void decrease (Item item, const Prio& value) {
      int i=_iim[item];
      int p=_data[i].parent;
      _data[i].prio=value;

      while( p!=-1 && _comp(value, _data[p].prio) ) {
        _data[i].name=_data[p].name;
        _data[i].prio=_data[p].prio;
        _data[p].name=item;
        _data[p].prio=value;
        _iim[_data[i].name]=i;
        i=p;
        p=_data[p].parent;
      }
      _iim[item]=i;
      if ( _comp(value, _data[_min].prio) ) _min=i;
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param item The item.
    /// \param value The priority.
    /// \pre \e item must be stored in the heap with priority at most \e value.
    void increase (Item item, const Prio& value) {
      erase(item);
      push(item, value);
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

    // Find the minimum of the roots
    int findMin() {
      if( _head!=-1 ) {
        int min_loc=_head, min_val=_data[_head].prio;
        for( int x=_data[_head].right_neighbor; x!=-1;
             x=_data[x].right_neighbor ) {
          if( _comp( _data[x].prio,min_val ) ) {
            min_val=_data[x].prio;
            min_loc=x;
          }
        }
        return min_loc;
      }
      else return -1;
    }

    // Merge the heap with another heap starting at the given position
    void merge(int a) {
      if( _head==-1 || a==-1 ) return;
      if( _data[a].right_neighbor==-1 &&
          _data[a].degree<=_data[_head].degree ) {
        _data[a].right_neighbor=_head;
        _head=a;
      } else {
        interleave(a);
      }
      if( _data[_head].right_neighbor==-1 ) return;

      int x=_head;
      int x_prev=-1, x_next=_data[x].right_neighbor;
      while( x_next!=-1 ) {
        if( _data[x].degree!=_data[x_next].degree ||
            ( _data[x_next].right_neighbor!=-1 &&
              _data[_data[x_next].right_neighbor].degree==_data[x].degree ) ) {
          x_prev=x;
          x=x_next;
        }
        else {
          if( _comp(_data[x_next].prio,_data[x].prio) ) {
            if( x_prev==-1 ) {
              _head=x_next;
            } else {
              _data[x_prev].right_neighbor=x_next;
            }
            fuse(x,x_next);
            x=x_next;
          }
          else {
            _data[x].right_neighbor=_data[x_next].right_neighbor;
            fuse(x_next,x);
          }
        }
        x_next=_data[x].right_neighbor;
      }
    }

    // Interleave the elements of the given list into the list of the roots
    void interleave(int a) {
      int p=_head, q=a;
      int curr=_data.size();
      _data.push_back(Store());

      while( p!=-1 || q!=-1 ) {
        if( q==-1 || ( p!=-1 && _data[p].degree<_data[q].degree ) ) {
          _data[curr].right_neighbor=p;
          curr=p;
          p=_data[p].right_neighbor;
        }
        else {
          _data[curr].right_neighbor=q;
          curr=q;
          q=_data[q].right_neighbor;
        }
      }

      _head=_data.back().right_neighbor;
      _data.pop_back();
    }

    // Lace node a under node b
    void fuse(int a, int b) {
      _data[a].parent=b;
      _data[a].right_neighbor=_data[b].child;
      _data[b].child=a;

      ++_data[b].degree;
    }

    // Unlace node a (if it has siblings)
    void unlace(int a) {
      int neighb=_data[a].right_neighbor;
      int other=_head;

      while( _data[other].right_neighbor!=a )
        other=_data[other].right_neighbor;
      _data[other].right_neighbor=neighb;
    }

  private:

    class Store {
      friend class BinomialHeap;

      Item name;
      int parent;
      int right_neighbor;
      int child;
      int degree;
      bool in;
      Prio prio;

      Store() : parent(-1), right_neighbor(-1), child(-1), degree(0),
        in(true) {}
    };
  };

} //namespace lemon

#endif //LEMON_BINOMIAL_HEAP_H

