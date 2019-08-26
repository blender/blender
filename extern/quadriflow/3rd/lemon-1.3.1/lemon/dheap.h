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

#ifndef LEMON_DHEAP_H
#define LEMON_DHEAP_H

///\ingroup heaps
///\file
///\brief D-ary heap implementation.

#include <vector>
#include <utility>
#include <functional>

namespace lemon {

  /// \ingroup heaps
  ///
  ///\brief D-ary heap data structure.
  ///
  /// This class implements the \e D-ary \e heap data structure.
  /// It fully conforms to the \ref concepts::Heap "heap concept".
  ///
  /// The \ref DHeap "D-ary heap" is a generalization of the
  /// \ref BinHeap "binary heap" structure, its nodes have at most
  /// \c D children, instead of two.
  /// \ref BinHeap and \ref QuadHeap are specialized implementations
  /// of this structure for <tt>D=2</tt> and <tt>D=4</tt>, respectively.
  ///
  /// \tparam PR Type of the priorities of the items.
  /// \tparam IM A read-writable item map with \c int values, used
  /// internally to handle the cross references.
  /// \tparam D The degree of the heap, each node have at most \e D
  /// children. The default is 16. Powers of two are suggested to use
  /// so that the multiplications and divisions needed to traverse the
  /// nodes of the heap could be performed faster.
  /// \tparam CMP A functor class for comparing the priorities.
  /// The default is \c std::less<PR>.
  ///
  ///\sa BinHeap
  ///\sa FouraryHeap
#ifdef DOXYGEN
  template <typename PR, typename IM, int D, typename CMP>
#else
  template <typename PR, typename IM, int D = 16,
            typename CMP = std::less<PR> >
#endif
  class DHeap {
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
    std::vector<Pair> _data;
    Compare _comp;
    ItemIntMap &_iim;

  public:
    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    explicit DHeap(ItemIntMap &map) : _iim(map) {}

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    /// \param comp The function object used for comparing the priorities.
    DHeap(ItemIntMap &map, const Compare &comp)
      : _iim(map), _comp(comp) {}

    /// \brief The number of items stored in the heap.
    ///
    /// This function returns the number of items stored in the heap.
    int size() const { return _data.size(); }

    /// \brief Check if the heap is empty.
    ///
    /// This function returns \c true if the heap is empty.
    bool empty() const { return _data.empty(); }

    /// \brief Make the heap empty.
    ///
    /// This functon makes the heap empty.
    /// It does not change the cross reference map. If you want to reuse
    /// a heap that is not surely empty, you should first clear it and
    /// then you should set the cross reference map to \c PRE_HEAP
    /// for each item.
    void clear() { _data.clear(); }

  private:
    int parent(int i) { return (i-1)/D; }
    int firstChild(int i) { return D*i+1; }

    bool less(const Pair &p1, const Pair &p2) const {
      return _comp(p1.second, p2.second);
    }

    void bubbleUp(int hole, Pair p) {
      int par = parent(hole);
      while( hole>0 && less(p,_data[par]) ) {
        move(_data[par],hole);
        hole = par;
        par = parent(hole);
      }
      move(p, hole);
    }

    void bubbleDown(int hole, Pair p, int length) {
      if( length>1 ) {
        int child = firstChild(hole);
        while( child+D<=length ) {
          int min=child;
          for (int i=1; i<D; ++i) {
            if( less(_data[child+i], _data[min]) )
              min=child+i;
          }
          if( !less(_data[min], p) )
            goto ok;
          move(_data[min], hole);
          hole = min;
          child = firstChild(hole);
        }
        if ( child<length ) {
          int min = child;
          while (++child < length) {
            if( less(_data[child], _data[min]) )
              min=child;
          }
          if( less(_data[min], p) ) {
            move(_data[min], hole);
            hole = min;
          }
        }
      }
    ok:
      move(p, hole);
    }

    void move(const Pair &p, int i) {
      _data[i] = p;
      _iim.set(p.first, i);
    }

  public:
    /// \brief Insert a pair of item and priority into the heap.
    ///
    /// This function inserts \c p.first to the heap with priority
    /// \c p.second.
    /// \param p The pair to insert.
    /// \pre \c p.first must not be stored in the heap.
    void push(const Pair &p) {
      int n = _data.size();
      _data.resize(n+1);
      bubbleUp(n, p);
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param i The item to insert.
    /// \param p The priority of the item.
    /// \pre \e i must not be stored in the heap.
    void push(const Item &i, const Prio &p) { push(Pair(i,p)); }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const { return _data[0].first; }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const { return _data[0].second; }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
      int n = _data.size()-1;
      _iim.set(_data[0].first, POST_HEAP);
      if (n>0) bubbleDown(0, _data[n], n);
      _data.pop_back();
    }

    /// \brief Remove the given item from the heap.
    ///
    /// This function removes the given item from the heap if it is
    /// already stored.
    /// \param i The item to delete.
    /// \pre \e i must be in the heap.
    void erase(const Item &i) {
      int h = _iim[i];
      int n = _data.size()-1;
      _iim.set(_data[h].first, POST_HEAP);
      if( h<n ) {
        if( less(_data[parent(h)], _data[n]) )
          bubbleDown(h, _data[n], n);
        else
          bubbleUp(h, _data[n]);
      }
      _data.pop_back();
    }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param i The item.
    /// \pre \e i must be in the heap.
    Prio operator[](const Item &i) const {
      int idx = _iim[i];
      return _data[idx].second;
    }

    /// \brief Set the priority of an item or insert it, if it is
    /// not stored in the heap.
    ///
    /// This method sets the priority of the given item if it is
    /// already stored in the heap. Otherwise it inserts the given
    /// item into the heap with the given priority.
    /// \param i The item.
    /// \param p The priority.
    void set(const Item &i, const Prio &p) {
      int idx = _iim[i];
      if( idx<0 )
        push(i,p);
      else if( _comp(p, _data[idx].second) )
        bubbleUp(idx, Pair(i,p));
      else
        bubbleDown(idx, Pair(i,p), _data.size());
    }

    /// \brief Decrease the priority of an item to the given value.
    ///
    /// This function decreases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at least \e p.
    void decrease(const Item &i, const Prio &p) {
      int idx = _iim[i];
      bubbleUp(idx, Pair(i,p));
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at most \e p.
    void increase(const Item &i, const Prio &p) {
      int idx = _iim[i];
      bubbleDown(idx, Pair(i,p), _data.size());
    }

    /// \brief Return the state of an item.
    ///
    /// This method returns \c PRE_HEAP if the given item has never
    /// been in the heap, \c IN_HEAP if it is in the heap at the moment,
    /// and \c POST_HEAP otherwise.
    /// In the latter case it is possible that the item will get back
    /// to the heap again.
    /// \param i The item.
    State state(const Item &i) const {
      int s = _iim[i];
      if (s>=0) s=0;
      return State(s);
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
          _iim[i] = st;
          break;
        case IN_HEAP:
          break;
      }
    }

    /// \brief Replace an item in the heap.
    ///
    /// This function replaces item \c i with item \c j.
    /// Item \c i must be in the heap, while \c j must be out of the heap.
    /// After calling this method, item \c i will be out of the
    /// heap and \c j will be in the heap with the same prioriority
    /// as item \c i had before.
    void replace(const Item& i, const Item& j) {
      int idx=_iim[i];
      _iim.set(i, _iim[j]);
      _iim.set(j, idx);
      _data[idx].first=j;
    }

  }; // class DHeap

} // namespace lemon

#endif // LEMON_DHEAP_H
