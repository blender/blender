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

#ifndef LEMON_BUCKET_HEAP_H
#define LEMON_BUCKET_HEAP_H

///\ingroup heaps
///\file
///\brief Bucket heap implementation.

#include <vector>
#include <utility>
#include <functional>

namespace lemon {

  namespace _bucket_heap_bits {

    template <bool MIN>
    struct DirectionTraits {
      static bool less(int left, int right) {
        return left < right;
      }
      static void increase(int& value) {
        ++value;
      }
    };

    template <>
    struct DirectionTraits<false> {
      static bool less(int left, int right) {
        return left > right;
      }
      static void increase(int& value) {
        --value;
      }
    };

  }

  /// \ingroup heaps
  ///
  /// \brief Bucket heap data structure.
  ///
  /// This class implements the \e bucket \e heap data structure.
  /// It practically conforms to the \ref concepts::Heap "heap concept",
  /// but it has some limitations.
  ///
  /// The bucket heap is a very simple structure. It can store only
  /// \c int priorities and it maintains a list of items for each priority
  /// in the range <tt>[0..C)</tt>. So it should only be used when the
  /// priorities are small. It is not intended to use as a Dijkstra heap.
  ///
  /// \tparam IM A read-writable item map with \c int values, used
  /// internally to handle the cross references.
  /// \tparam MIN Indicate if the heap is a \e min-heap or a \e max-heap.
  /// The default is \e min-heap. If this parameter is set to \c false,
  /// then the comparison is reversed, so the top(), prio() and pop()
  /// functions deal with the item having maximum priority instead of the
  /// minimum.
  ///
  /// \sa SimpleBucketHeap
  template <typename IM, bool MIN = true>
  class BucketHeap {

  public:

    /// Type of the item-int map.
    typedef IM ItemIntMap;
    /// Type of the priorities.
    typedef int Prio;
    /// Type of the items stored in the heap.
    typedef typename ItemIntMap::Key Item;
    /// Type of the item-priority pairs.
    typedef std::pair<Item,Prio> Pair;

  private:

    typedef _bucket_heap_bits::DirectionTraits<MIN> Direction;

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

  public:

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    explicit BucketHeap(ItemIntMap &map) : _iim(map), _minimum(0) {}

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
    void clear() {
      _data.clear(); _first.clear(); _minimum = 0;
    }

  private:

    void relocateLast(int idx) {
      if (idx + 1 < int(_data.size())) {
        _data[idx] = _data.back();
        if (_data[idx].prev != -1) {
          _data[_data[idx].prev].next = idx;
        } else {
          _first[_data[idx].value] = idx;
        }
        if (_data[idx].next != -1) {
          _data[_data[idx].next].prev = idx;
        }
        _iim[_data[idx].item] = idx;
      }
      _data.pop_back();
    }

    void unlace(int idx) {
      if (_data[idx].prev != -1) {
        _data[_data[idx].prev].next = _data[idx].next;
      } else {
        _first[_data[idx].value] = _data[idx].next;
      }
      if (_data[idx].next != -1) {
        _data[_data[idx].next].prev = _data[idx].prev;
      }
    }

    void lace(int idx) {
      if (int(_first.size()) <= _data[idx].value) {
        _first.resize(_data[idx].value + 1, -1);
      }
      _data[idx].next = _first[_data[idx].value];
      if (_data[idx].next != -1) {
        _data[_data[idx].next].prev = idx;
      }
      _first[_data[idx].value] = idx;
      _data[idx].prev = -1;
    }

  public:

    /// \brief Insert a pair of item and priority into the heap.
    ///
    /// This function inserts \c p.first to the heap with priority
    /// \c p.second.
    /// \param p The pair to insert.
    /// \pre \c p.first must not be stored in the heap.
    void push(const Pair& p) {
      push(p.first, p.second);
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param i The item to insert.
    /// \param p The priority of the item.
    /// \pre \e i must not be stored in the heap.
    void push(const Item &i, const Prio &p) {
      int idx = _data.size();
      _iim[i] = idx;
      _data.push_back(BucketItem(i, p));
      lace(idx);
      if (Direction::less(p, _minimum)) {
        _minimum = p;
      }
    }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      return _data[_first[_minimum]].item;
    }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      return _minimum;
    }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      int idx = _first[_minimum];
      _iim[_data[idx].item] = -2;
      unlace(idx);
      relocateLast(idx);
    }

    /// \brief Remove the given item from the heap.
    ///
    /// This function removes the given item from the heap if it is
    /// already stored.
    /// \param i The item to delete.
    /// \pre \e i must be in the heap.
    void erase(const Item &i) {
      int idx = _iim[i];
      _iim[_data[idx].item] = -2;
      unlace(idx);
      relocateLast(idx);
    }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param i The item.
    /// \pre \e i must be in the heap.
    Prio operator[](const Item &i) const {
      int idx = _iim[i];
      return _data[idx].value;
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
      if (idx < 0) {
        push(i, p);
      } else if (Direction::less(p, _data[idx].value)) {
        decrease(i, p);
      } else {
        increase(i, p);
      }
    }

    /// \brief Decrease the priority of an item to the given value.
    ///
    /// This function decreases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at least \e p.
    void decrease(const Item &i, const Prio &p) {
      int idx = _iim[i];
      unlace(idx);
      _data[idx].value = p;
      if (Direction::less(p, _minimum)) {
        _minimum = p;
      }
      lace(idx);
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at most \e p.
    void increase(const Item &i, const Prio &p) {
      int idx = _iim[i];
      unlace(idx);
      _data[idx].value = p;
      lace(idx);
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
      int idx = _iim[i];
      if (idx >= 0) idx = 0;
      return State(idx);
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

    struct BucketItem {
      BucketItem(const Item& _item, int _value)
        : item(_item), value(_value) {}

      Item item;
      int value;

      int prev, next;
    };

    ItemIntMap& _iim;
    std::vector<int> _first;
    std::vector<BucketItem> _data;
    mutable int _minimum;

  }; // class BucketHeap

  /// \ingroup heaps
  ///
  /// \brief Simplified bucket heap data structure.
  ///
  /// This class implements a simplified \e bucket \e heap data
  /// structure. It does not provide some functionality, but it is
  /// faster and simpler than BucketHeap. The main difference is
  /// that BucketHeap stores a doubly-linked list for each key while
  /// this class stores only simply-linked lists. It supports erasing
  /// only for the item having minimum priority and it does not support
  /// key increasing and decreasing.
  ///
  /// Note that this implementation does not conform to the
  /// \ref concepts::Heap "heap concept" due to the lack of some
  /// functionality.
  ///
  /// \tparam IM A read-writable item map with \c int values, used
  /// internally to handle the cross references.
  /// \tparam MIN Indicate if the heap is a \e min-heap or a \e max-heap.
  /// The default is \e min-heap. If this parameter is set to \c false,
  /// then the comparison is reversed, so the top(), prio() and pop()
  /// functions deal with the item having maximum priority instead of the
  /// minimum.
  ///
  /// \sa BucketHeap
  template <typename IM, bool MIN = true >
  class SimpleBucketHeap {

  public:

    /// Type of the item-int map.
    typedef IM ItemIntMap;
    /// Type of the priorities.
    typedef int Prio;
    /// Type of the items stored in the heap.
    typedef typename ItemIntMap::Key Item;
    /// Type of the item-priority pairs.
    typedef std::pair<Item,Prio> Pair;

  private:

    typedef _bucket_heap_bits::DirectionTraits<MIN> Direction;

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

  public:

    /// \brief Constructor.
    ///
    /// Constructor.
    /// \param map A map that assigns \c int values to the items.
    /// It is used internally to handle the cross references.
    /// The assigned value must be \c PRE_HEAP (<tt>-1</tt>) for each item.
    explicit SimpleBucketHeap(ItemIntMap &map)
      : _iim(map), _free(-1), _num(0), _minimum(0) {}

    /// \brief The number of items stored in the heap.
    ///
    /// This function returns the number of items stored in the heap.
    int size() const { return _num; }

    /// \brief Check if the heap is empty.
    ///
    /// This function returns \c true if the heap is empty.
    bool empty() const { return _num == 0; }

    /// \brief Make the heap empty.
    ///
    /// This functon makes the heap empty.
    /// It does not change the cross reference map. If you want to reuse
    /// a heap that is not surely empty, you should first clear it and
    /// then you should set the cross reference map to \c PRE_HEAP
    /// for each item.
    void clear() {
      _data.clear(); _first.clear(); _free = -1; _num = 0; _minimum = 0;
    }

    /// \brief Insert a pair of item and priority into the heap.
    ///
    /// This function inserts \c p.first to the heap with priority
    /// \c p.second.
    /// \param p The pair to insert.
    /// \pre \c p.first must not be stored in the heap.
    void push(const Pair& p) {
      push(p.first, p.second);
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param i The item to insert.
    /// \param p The priority of the item.
    /// \pre \e i must not be stored in the heap.
    void push(const Item &i, const Prio &p) {
      int idx;
      if (_free == -1) {
        idx = _data.size();
        _data.push_back(BucketItem(i));
      } else {
        idx = _free;
        _free = _data[idx].next;
        _data[idx].item = i;
      }
      _iim[i] = idx;
      if (p >= int(_first.size())) _first.resize(p + 1, -1);
      _data[idx].next = _first[p];
      _first[p] = idx;
      if (Direction::less(p, _minimum)) {
        _minimum = p;
      }
      ++_num;
    }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      return _data[_first[_minimum]].item;
    }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      return _minimum;
    }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
      while (_first[_minimum] == -1) {
        Direction::increase(_minimum);
      }
      int idx = _first[_minimum];
      _iim[_data[idx].item] = -2;
      _first[_minimum] = _data[idx].next;
      _data[idx].next = _free;
      _free = idx;
      --_num;
    }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param i The item.
    /// \pre \e i must be in the heap.
    /// \warning This operator is not a constant time function because
    /// it scans the whole data structure to find the proper value.
    Prio operator[](const Item &i) const {
      for (int k = 0; k < int(_first.size()); ++k) {
        int idx = _first[k];
        while (idx != -1) {
          if (_data[idx].item == i) {
            return k;
          }
          idx = _data[idx].next;
        }
      }
      return -1;
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
      int idx = _iim[i];
      if (idx >= 0) idx = 0;
      return State(idx);
    }

  private:

    struct BucketItem {
      BucketItem(const Item& _item)
        : item(_item) {}

      Item item;
      int next;
    };

    ItemIntMap& _iim;
    std::vector<int> _first;
    std::vector<BucketItem> _data;
    int _free, _num;
    mutable int _minimum;

  }; // class SimpleBucketHeap

}

#endif
