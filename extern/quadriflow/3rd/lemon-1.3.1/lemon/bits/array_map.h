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

#ifndef LEMON_BITS_ARRAY_MAP_H
#define LEMON_BITS_ARRAY_MAP_H

#include <memory>

#include <lemon/bits/traits.h>
#include <lemon/bits/alteration_notifier.h>
#include <lemon/concept_check.h>
#include <lemon/concepts/maps.h>

// \ingroup graphbits
// \file
// \brief Graph map based on the array storage.

namespace lemon {

  // \ingroup graphbits
  //
  // \brief Graph map based on the array storage.
  //
  // The ArrayMap template class is graph map structure that automatically
  // updates the map when a key is added to or erased from the graph.
  // This map uses the allocators to implement the container functionality.
  //
  // The template parameters are the Graph, the current Item type and
  // the Value type of the map.
  template <typename _Graph, typename _Item, typename _Value>
  class ArrayMap
    : public ItemSetTraits<_Graph, _Item>::ItemNotifier::ObserverBase {
  public:
    // The graph type.
    typedef _Graph GraphType;
    // The item type.
    typedef _Item Item;
    // The reference map tag.
    typedef True ReferenceMapTag;

    // The key type of the map.
    typedef _Item Key;
    // The value type of the map.
    typedef _Value Value;

    // The const reference type of the map.
    typedef const _Value& ConstReference;
    // The reference type of the map.
    typedef _Value& Reference;

    // The map type.
    typedef ArrayMap Map;

    // The notifier type.
    typedef typename ItemSetTraits<_Graph, _Item>::ItemNotifier Notifier;

  private:

    // The MapBase of the Map which imlements the core regisitry function.
    typedef typename Notifier::ObserverBase Parent;

    typedef std::allocator<Value> Allocator;

  public:

    // \brief Graph initialized map constructor.
    //
    // Graph initialized map constructor.
    explicit ArrayMap(const GraphType& graph) {
      Parent::attach(graph.notifier(Item()));
      allocate_memory();
      Notifier* nf = Parent::notifier();
      Item it;
      for (nf->first(it); it != INVALID; nf->next(it)) {
        int id = nf->id(it);;
        allocator.construct(&(values[id]), Value());
      }
    }

    // \brief Constructor to use default value to initialize the map.
    //
    // It constructs a map and initialize all of the the map.
    ArrayMap(const GraphType& graph, const Value& value) {
      Parent::attach(graph.notifier(Item()));
      allocate_memory();
      Notifier* nf = Parent::notifier();
      Item it;
      for (nf->first(it); it != INVALID; nf->next(it)) {
        int id = nf->id(it);;
        allocator.construct(&(values[id]), value);
      }
    }

  private:
    // \brief Constructor to copy a map of the same map type.
    //
    // Constructor to copy a map of the same map type.
    ArrayMap(const ArrayMap& copy) : Parent() {
      if (copy.attached()) {
        attach(*copy.notifier());
      }
      capacity = copy.capacity;
      if (capacity == 0) return;
      values = allocator.allocate(capacity);
      Notifier* nf = Parent::notifier();
      Item it;
      for (nf->first(it); it != INVALID; nf->next(it)) {
        int id = nf->id(it);;
        allocator.construct(&(values[id]), copy.values[id]);
      }
    }

    // \brief Assign operator.
    //
    // This operator assigns for each item in the map the
    // value mapped to the same item in the copied map.
    // The parameter map should be indiced with the same
    // itemset because this assign operator does not change
    // the container of the map.
    ArrayMap& operator=(const ArrayMap& cmap) {
      return operator=<ArrayMap>(cmap);
    }


    // \brief Template assign operator.
    //
    // The given parameter should conform to the ReadMap
    // concecpt and could be indiced by the current item set of
    // the NodeMap. In this case the value for each item
    // is assigned by the value of the given ReadMap.
    template <typename CMap>
    ArrayMap& operator=(const CMap& cmap) {
      checkConcept<concepts::ReadMap<Key, _Value>, CMap>();
      const typename Parent::Notifier* nf = Parent::notifier();
      Item it;
      for (nf->first(it); it != INVALID; nf->next(it)) {
        set(it, cmap[it]);
      }
      return *this;
    }

  public:
    // \brief The destructor of the map.
    //
    // The destructor of the map.
    virtual ~ArrayMap() {
      if (attached()) {
        clear();
        detach();
      }
    }

  protected:

    using Parent::attach;
    using Parent::detach;
    using Parent::attached;

  public:

    // \brief The subscript operator.
    //
    // The subscript operator. The map can be subscripted by the
    // actual keys of the graph.
    Value& operator[](const Key& key) {
      int id = Parent::notifier()->id(key);
      return values[id];
    }

    // \brief The const subscript operator.
    //
    // The const subscript operator. The map can be subscripted by the
    // actual keys of the graph.
    const Value& operator[](const Key& key) const {
      int id = Parent::notifier()->id(key);
      return values[id];
    }

    // \brief Setter function of the map.
    //
    // Setter function of the map. Equivalent with map[key] = val.
    // This is a compatibility feature with the not dereferable maps.
    void set(const Key& key, const Value& val) {
      (*this)[key] = val;
    }

  protected:

    // \brief Adds a new key to the map.
    //
    // It adds a new key to the map. It is called by the observer notifier
    // and it overrides the add() member function of the observer base.
    virtual void add(const Key& key) {
      Notifier* nf = Parent::notifier();
      int id = nf->id(key);
      if (id >= capacity) {
        int new_capacity = (capacity == 0 ? 1 : capacity);
        while (new_capacity <= id) {
          new_capacity <<= 1;
        }
        Value* new_values = allocator.allocate(new_capacity);
        Item it;
        for (nf->first(it); it != INVALID; nf->next(it)) {
          int jd = nf->id(it);;
          if (id != jd) {
            allocator.construct(&(new_values[jd]), values[jd]);
            allocator.destroy(&(values[jd]));
          }
        }
        if (capacity != 0) allocator.deallocate(values, capacity);
        values = new_values;
        capacity = new_capacity;
      }
      allocator.construct(&(values[id]), Value());
    }

    // \brief Adds more new keys to the map.
    //
    // It adds more new keys to the map. It is called by the observer notifier
    // and it overrides the add() member function of the observer base.
    virtual void add(const std::vector<Key>& keys) {
      Notifier* nf = Parent::notifier();
      int max_id = -1;
      for (int i = 0; i < int(keys.size()); ++i) {
        int id = nf->id(keys[i]);
        if (id > max_id) {
          max_id = id;
        }
      }
      if (max_id >= capacity) {
        int new_capacity = (capacity == 0 ? 1 : capacity);
        while (new_capacity <= max_id) {
          new_capacity <<= 1;
        }
        Value* new_values = allocator.allocate(new_capacity);
        Item it;
        for (nf->first(it); it != INVALID; nf->next(it)) {
          int id = nf->id(it);
          bool found = false;
          for (int i = 0; i < int(keys.size()); ++i) {
            int jd = nf->id(keys[i]);
            if (id == jd) {
              found = true;
              break;
            }
          }
          if (found) continue;
          allocator.construct(&(new_values[id]), values[id]);
          allocator.destroy(&(values[id]));
        }
        if (capacity != 0) allocator.deallocate(values, capacity);
        values = new_values;
        capacity = new_capacity;
      }
      for (int i = 0; i < int(keys.size()); ++i) {
        int id = nf->id(keys[i]);
        allocator.construct(&(values[id]), Value());
      }
    }

    // \brief Erase a key from the map.
    //
    // Erase a key from the map. It is called by the observer notifier
    // and it overrides the erase() member function of the observer base.
    virtual void erase(const Key& key) {
      int id = Parent::notifier()->id(key);
      allocator.destroy(&(values[id]));
    }

    // \brief Erase more keys from the map.
    //
    // Erase more keys from the map. It is called by the observer notifier
    // and it overrides the erase() member function of the observer base.
    virtual void erase(const std::vector<Key>& keys) {
      for (int i = 0; i < int(keys.size()); ++i) {
        int id = Parent::notifier()->id(keys[i]);
        allocator.destroy(&(values[id]));
      }
    }

    // \brief Builds the map.
    //
    // It builds the map. It is called by the observer notifier
    // and it overrides the build() member function of the observer base.
    virtual void build() {
      Notifier* nf = Parent::notifier();
      allocate_memory();
      Item it;
      for (nf->first(it); it != INVALID; nf->next(it)) {
        int id = nf->id(it);;
        allocator.construct(&(values[id]), Value());
      }
    }

    // \brief Clear the map.
    //
    // It erase all items from the map. It is called by the observer notifier
    // and it overrides the clear() member function of the observer base.
    virtual void clear() {
      Notifier* nf = Parent::notifier();
      if (capacity != 0) {
        Item it;
        for (nf->first(it); it != INVALID; nf->next(it)) {
          int id = nf->id(it);
          allocator.destroy(&(values[id]));
        }
        allocator.deallocate(values, capacity);
        capacity = 0;
      }
    }

  private:

    void allocate_memory() {
      int max_id = Parent::notifier()->maxId();
      if (max_id == -1) {
        capacity = 0;
        values = 0;
        return;
      }
      capacity = 1;
      while (capacity <= max_id) {
        capacity <<= 1;
      }
      values = allocator.allocate(capacity);
    }

    int capacity;
    Value* values;
    Allocator allocator;

  };

}

#endif
