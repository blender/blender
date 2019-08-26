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

#ifndef LEMON_CONCEPTS_HEAP_H
#define LEMON_CONCEPTS_HEAP_H

///\ingroup concept
///\file
///\brief The concept of heaps.

#include <lemon/core.h>
#include <lemon/concept_check.h>

namespace lemon {

  namespace concepts {

    /// \addtogroup concept
    /// @{

    /// \brief The heap concept.
    ///
    /// This concept class describes the main interface of heaps.
    /// The various \ref heaps "heap structures" are efficient
    /// implementations of the abstract data type \e priority \e queue.
    /// They store items with specified values called \e priorities
    /// in such a way that finding and removing the item with minimum
    /// priority are efficient. The basic operations are adding and
    /// erasing items, changing the priority of an item, etc.
    ///
    /// Heaps are crucial in several algorithms, such as Dijkstra and Prim.
    /// Any class that conforms to this concept can be used easily in such
    /// algorithms.
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
    class Heap {
    public:

      /// Type of the item-int map.
      typedef IM ItemIntMap;
      /// Type of the priorities.
      typedef PR Prio;
      /// Type of the items stored in the heap.
      typedef typename ItemIntMap::Key Item;

      /// \brief Type to represent the states of the items.
      ///
      /// Each item has a state associated to it. It can be "in heap",
      /// "pre-heap" or "post-heap". The latter two are indifferent from the
      /// heap's point of view, but may be useful to the user.
      ///
      /// The item-int map must be initialized in such way that it assigns
      /// \c PRE_HEAP (<tt>-1</tt>) to any element to be put in the heap.
      enum State {
        IN_HEAP = 0,    ///< = 0. The "in heap" state constant.
        PRE_HEAP = -1,  ///< = -1. The "pre-heap" state constant.
        POST_HEAP = -2  ///< = -2. The "post-heap" state constant.
      };

      /// \brief Constructor.
      ///
      /// Constructor.
      /// \param map A map that assigns \c int values to keys of type
      /// \c Item. It is used internally by the heap implementations to
      /// handle the cross references. The assigned value must be
      /// \c PRE_HEAP (<tt>-1</tt>) for each item.
#ifdef DOXYGEN
      explicit Heap(ItemIntMap &map) {}
#else
      explicit Heap(ItemIntMap&) {}
#endif

      /// \brief Constructor.
      ///
      /// Constructor.
      /// \param map A map that assigns \c int values to keys of type
      /// \c Item. It is used internally by the heap implementations to
      /// handle the cross references. The assigned value must be
      /// \c PRE_HEAP (<tt>-1</tt>) for each item.
      /// \param comp The function object used for comparing the priorities.
#ifdef DOXYGEN
      explicit Heap(ItemIntMap &map, const CMP &comp) {}
#else
      explicit Heap(ItemIntMap&, const CMP&) {}
#endif

      /// \brief The number of items stored in the heap.
      ///
      /// This function returns the number of items stored in the heap.
      int size() const { return 0; }

      /// \brief Check if the heap is empty.
      ///
      /// This function returns \c true if the heap is empty.
      bool empty() const { return false; }

      /// \brief Make the heap empty.
      ///
      /// This functon makes the heap empty.
      /// It does not change the cross reference map. If you want to reuse
      /// a heap that is not surely empty, you should first clear it and
      /// then you should set the cross reference map to \c PRE_HEAP
      /// for each item.
      void clear() {}

      /// \brief Insert an item into the heap with the given priority.
      ///
      /// This function inserts the given item into the heap with the
      /// given priority.
      /// \param i The item to insert.
      /// \param p The priority of the item.
      /// \pre \e i must not be stored in the heap.
#ifdef DOXYGEN
      void push(const Item &i, const Prio &p) {}
#else
      void push(const Item&, const Prio&) {}
#endif

      /// \brief Return the item having minimum priority.
      ///
      /// This function returns the item having minimum priority.
      /// \pre The heap must be non-empty.
      Item top() const { return Item(); }

      /// \brief The minimum priority.
      ///
      /// This function returns the minimum priority.
      /// \pre The heap must be non-empty.
      Prio prio() const { return Prio(); }

      /// \brief Remove the item having minimum priority.
      ///
      /// This function removes the item having minimum priority.
      /// \pre The heap must be non-empty.
      void pop() {}

      /// \brief Remove the given item from the heap.
      ///
      /// This function removes the given item from the heap if it is
      /// already stored.
      /// \param i The item to delete.
      /// \pre \e i must be in the heap.
#ifdef DOXYGEN
      void erase(const Item &i) {}
#else
      void erase(const Item&) {}
#endif

      /// \brief The priority of the given item.
      ///
      /// This function returns the priority of the given item.
      /// \param i The item.
      /// \pre \e i must be in the heap.
#ifdef DOXYGEN
      Prio operator[](const Item &i) const {}
#else
      Prio operator[](const Item&) const { return Prio(); }
#endif

      /// \brief Set the priority of an item or insert it, if it is
      /// not stored in the heap.
      ///
      /// This method sets the priority of the given item if it is
      /// already stored in the heap. Otherwise it inserts the given
      /// item into the heap with the given priority.
      ///
      /// \param i The item.
      /// \param p The priority.
#ifdef DOXYGEN
      void set(const Item &i, const Prio &p) {}
#else
      void set(const Item&, const Prio&) {}
#endif

      /// \brief Decrease the priority of an item to the given value.
      ///
      /// This function decreases the priority of an item to the given value.
      /// \param i The item.
      /// \param p The priority.
      /// \pre \e i must be stored in the heap with priority at least \e p.
#ifdef DOXYGEN
      void decrease(const Item &i, const Prio &p) {}
#else
      void decrease(const Item&, const Prio&) {}
#endif

      /// \brief Increase the priority of an item to the given value.
      ///
      /// This function increases the priority of an item to the given value.
      /// \param i The item.
      /// \param p The priority.
      /// \pre \e i must be stored in the heap with priority at most \e p.
#ifdef DOXYGEN
      void increase(const Item &i, const Prio &p) {}
#else
      void increase(const Item&, const Prio&) {}
#endif

      /// \brief Return the state of an item.
      ///
      /// This method returns \c PRE_HEAP if the given item has never
      /// been in the heap, \c IN_HEAP if it is in the heap at the moment,
      /// and \c POST_HEAP otherwise.
      /// In the latter case it is possible that the item will get back
      /// to the heap again.
      /// \param i The item.
#ifdef DOXYGEN
      State state(const Item &i) const {}
#else
      State state(const Item&) const { return PRE_HEAP; }
#endif

      /// \brief Set the state of an item in the heap.
      ///
      /// This function sets the state of the given item in the heap.
      /// It can be used to manually clear the heap when it is important
      /// to achive better time complexity.
      /// \param i The item.
      /// \param st The state. It should not be \c IN_HEAP.
#ifdef DOXYGEN
      void state(const Item& i, State st) {}
#else
      void state(const Item&, State) {}
#endif


      template <typename _Heap>
      struct Constraints {
      public:
        void constraints() {
          typedef typename _Heap::Item OwnItem;
          typedef typename _Heap::Prio OwnPrio;
          typedef typename _Heap::State OwnState;

          Item item;
          Prio prio;
          item=Item();
          prio=Prio();
          ::lemon::ignore_unused_variable_warning(item);
          ::lemon::ignore_unused_variable_warning(prio);

          OwnItem own_item;
          OwnPrio own_prio;
          OwnState own_state;
          own_item=Item();
          own_prio=Prio();
          ::lemon::ignore_unused_variable_warning(own_item);
          ::lemon::ignore_unused_variable_warning(own_prio);
          ::lemon::ignore_unused_variable_warning(own_state);

          _Heap heap1(map);
          _Heap heap2 = heap1;
          ::lemon::ignore_unused_variable_warning(heap1);
          ::lemon::ignore_unused_variable_warning(heap2);

          int s = heap.size();
          ::lemon::ignore_unused_variable_warning(s);
          bool e = heap.empty();
          ::lemon::ignore_unused_variable_warning(e);

          prio = heap.prio();
          item = heap.top();
          prio = heap[item];
          own_prio = heap.prio();
          own_item = heap.top();
          own_prio = heap[own_item];

          heap.push(item, prio);
          heap.push(own_item, own_prio);
          heap.pop();

          heap.set(item, prio);
          heap.decrease(item, prio);
          heap.increase(item, prio);
          heap.set(own_item, own_prio);
          heap.decrease(own_item, own_prio);
          heap.increase(own_item, own_prio);

          heap.erase(item);
          heap.erase(own_item);
          heap.clear();

          own_state = heap.state(own_item);
          heap.state(own_item, own_state);

          own_state = _Heap::PRE_HEAP;
          own_state = _Heap::IN_HEAP;
          own_state = _Heap::POST_HEAP;
        }

        _Heap& heap;
        ItemIntMap& map;
        Constraints() {}
      };
    };

    /// @}
  } // namespace lemon
}
#endif
