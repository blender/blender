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

#ifndef LEMON_PAIRING_HEAP_H
#define LEMON_PAIRING_HEAP_H

///\file
///\ingroup heaps
///\brief Pairing heap implementation.

#include <vector>
#include <utility>
#include <functional>
#include <lemon/math.h>

namespace lemon {

  /// \ingroup heaps
  ///
  ///\brief Pairing Heap.
  ///
  /// This class implements the \e pairing \e heap data structure.
  /// It fully conforms to the \ref concepts::Heap "heap concept".
  ///
  /// The methods \ref increase() and \ref erase() are not efficient
  /// in a pairing heap. In case of many calls of these operations,
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
  class PairingHeap {
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
    class store;

    std::vector<store> _data;
    int _min;
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
    explicit PairingHeap(ItemIntMap &map)
      : _min(0), _iim(map), _num_items(0) {}

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    /// \param comp The function object used for comparing the priorities.
    PairingHeap(ItemIntMap &map, const Compare &comp)
      : _min(0), _iim(map), _comp(comp), _num_items(0) {}

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
      _data.clear();
      _min = 0;
      _num_items = 0;
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
      if ( i>=0 && _data[i].in ) {
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
      if( i<0 ) {
        int s=_data.size();
        _iim.set(item, s);
        store st;
        st.name=item;
        _data.push_back(st);
        i=s;
      } else {
        _data[i].parent=_data[i].child=-1;
        _data[i].left_child=false;
        _data[i].degree=0;
        _data[i].in=true;
      }

      _data[i].prio=value;

      if ( _num_items!=0 ) {
        if ( _comp( value, _data[_min].prio) ) {
          fuse(i,_min);
          _min=i;
        }
        else fuse(_min,i);
      }
      else _min=i;

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
    const Prio& prio() const { return _data[_min].prio; }

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
      std::vector<int> trees;
      int i=0, child_right = 0;
      _data[_min].in=false;

      if( -1!=_data[_min].child ) {
        i=_data[_min].child;
        trees.push_back(i);
        _data[i].parent = -1;
        _data[_min].child = -1;

        int ch=-1;
        while( _data[i].child!=-1 ) {
          ch=_data[i].child;
          if( _data[ch].left_child && i==_data[ch].parent ) {
            break;
          } else {
            if( _data[ch].left_child ) {
              child_right=_data[ch].parent;
              _data[ch].parent = i;
              --_data[i].degree;
            }
            else {
              child_right=ch;
              _data[i].child=-1;
              _data[i].degree=0;
            }
            _data[child_right].parent = -1;
            trees.push_back(child_right);
            i = child_right;
          }
        }

        int num_child = trees.size();
        int other;
        for( i=0; i<num_child-1; i+=2 ) {
          if ( !_comp(_data[trees[i]].prio, _data[trees[i+1]].prio) ) {
            other=trees[i];
            trees[i]=trees[i+1];
            trees[i+1]=other;
          }
          fuse( trees[i], trees[i+1] );
        }

        i = (0==(num_child % 2)) ? num_child-2 : num_child-1;
        while(i>=2) {
          if ( _comp(_data[trees[i]].prio, _data[trees[i-2]].prio) ) {
            other=trees[i];
            trees[i]=trees[i-2];
            trees[i-2]=other;
          }
          fuse( trees[i-2], trees[i] );
          i-=2;
        }
        _min = trees[0];
      }
      else {
        _min = _data[_min].child;
      }

      if (_min >= 0) _data[_min].left_child = false;
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
      if ( i>=0 && _data[i].in ) {
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
      _data[i].prio=value;
      int p=_data[i].parent;

      if( _data[i].left_child && i!=_data[p].child ) {
        p=_data[p].parent;
      }

      if ( p!=-1 && _comp(value,_data[p].prio) ) {
        cut(i,p);
        if ( _comp(_data[_min].prio,value) ) {
          fuse(_min,i);
        } else {
          fuse(i,_min);
          _min=i;
        }
      }
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param item The item.
    /// \param value The priority.
    /// \pre \e item must be stored in the heap with priority at most \e value.
    void increase (Item item, const Prio& value) {
      erase(item);
      push(item,value);
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
        if( _data[i].in ) i=0;
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
        if (state(i) == IN_HEAP) erase(i);
        _iim[i]=st;
        break;
      case IN_HEAP:
        break;
      }
    }

  private:

    void cut(int a, int b) {
      int child_a;
      switch (_data[a].degree) {
        case 2:
          child_a = _data[_data[a].child].parent;
          if( _data[a].left_child ) {
            _data[child_a].left_child=true;
            _data[b].child=child_a;
            _data[child_a].parent=_data[a].parent;
          }
          else {
            _data[child_a].left_child=false;
            _data[child_a].parent=b;
            if( a!=_data[b].child )
              _data[_data[b].child].parent=child_a;
            else
              _data[b].child=child_a;
          }
          --_data[a].degree;
          _data[_data[a].child].parent=a;
          break;

        case 1:
          child_a = _data[a].child;
          if( !_data[child_a].left_child ) {
            --_data[a].degree;
            if( _data[a].left_child ) {
              _data[child_a].left_child=true;
              _data[child_a].parent=_data[a].parent;
              _data[b].child=child_a;
            }
            else {
              _data[child_a].left_child=false;
              _data[child_a].parent=b;
              if( a!=_data[b].child )
                _data[_data[b].child].parent=child_a;
              else
                _data[b].child=child_a;
            }
            _data[a].child=-1;
          }
          else {
            --_data[b].degree;
            if( _data[a].left_child ) {
              _data[b].child =
                (1==_data[b].degree) ? _data[a].parent : -1;
            } else {
              if (1==_data[b].degree)
                _data[_data[b].child].parent=b;
              else
                _data[b].child=-1;
            }
          }
          break;

        case 0:
          --_data[b].degree;
          if( _data[a].left_child ) {
            _data[b].child =
              (0!=_data[b].degree) ? _data[a].parent : -1;
          } else {
            if( 0!=_data[b].degree )
              _data[_data[b].child].parent=b;
            else
              _data[b].child=-1;
          }
          break;
      }
      _data[a].parent=-1;
      _data[a].left_child=false;
    }

    void fuse(int a, int b) {
      int child_a = _data[a].child;
      int child_b = _data[b].child;
      _data[a].child=b;
      _data[b].parent=a;
      _data[b].left_child=true;

      if( -1!=child_a ) {
        _data[b].child=child_a;
        _data[child_a].parent=b;
        _data[child_a].left_child=false;
        ++_data[b].degree;

        if( -1!=child_b ) {
           _data[b].child=child_b;
           _data[child_b].parent=child_a;
        }
      }
      else { ++_data[a].degree; }
    }

    class store {
      friend class PairingHeap;

      Item name;
      int parent;
      int child;
      bool left_child;
      int degree;
      bool in;
      Prio prio;

      store() : parent(-1), child(-1), left_child(false), degree(0), in(true) {}
    };
  };

} //namespace lemon

#endif //LEMON_PAIRING_HEAP_H

