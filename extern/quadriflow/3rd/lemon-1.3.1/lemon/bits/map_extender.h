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

#ifndef LEMON_BITS_MAP_EXTENDER_H
#define LEMON_BITS_MAP_EXTENDER_H

#include <iterator>

#include <lemon/bits/traits.h>

#include <lemon/concept_check.h>
#include <lemon/concepts/maps.h>

//\file
//\brief Extenders for iterable maps.

namespace lemon {

  // \ingroup graphbits
  //
  // \brief Extender for maps
  template <typename _Map>
  class MapExtender : public _Map {
    typedef _Map Parent;
    typedef typename Parent::GraphType GraphType;

  public:

    typedef MapExtender Map;
    typedef typename Parent::Key Item;

    typedef typename Parent::Key Key;
    typedef typename Parent::Value Value;
    typedef typename Parent::Reference Reference;
    typedef typename Parent::ConstReference ConstReference;

    typedef typename Parent::ReferenceMapTag ReferenceMapTag;

    class MapIt;
    class ConstMapIt;

    friend class MapIt;
    friend class ConstMapIt;

  public:

    MapExtender(const GraphType& graph)
      : Parent(graph) {}

    MapExtender(const GraphType& graph, const Value& value)
      : Parent(graph, value) {}

  private:
    MapExtender& operator=(const MapExtender& cmap) {
      return operator=<MapExtender>(cmap);
    }

    template <typename CMap>
    MapExtender& operator=(const CMap& cmap) {
      Parent::operator=(cmap);
      return *this;
    }

  public:
    class MapIt : public Item {
      typedef Item Parent;

    public:

      typedef typename Map::Value Value;

      MapIt() : map(NULL) {}

      MapIt(Invalid i) : Parent(i), map(NULL) {}

      explicit MapIt(Map& _map) : map(&_map) {
        map->notifier()->first(*this);
      }

      MapIt(const Map& _map, const Item& item)
        : Parent(item), map(&_map) {}

      MapIt& operator++() {
        map->notifier()->next(*this);
        return *this;
      }

      typename MapTraits<Map>::ConstReturnValue operator*() const {
        return (*map)[*this];
      }

      typename MapTraits<Map>::ReturnValue operator*() {
        return (*map)[*this];
      }

      void set(const Value& value) {
        map->set(*this, value);
      }

    protected:
      Map* map;

    };

    class ConstMapIt : public Item {
      typedef Item Parent;

    public:

      typedef typename Map::Value Value;

      ConstMapIt() : map(NULL) {}

      ConstMapIt(Invalid i) : Parent(i), map(NULL) {}

      explicit ConstMapIt(Map& _map) : map(&_map) {
        map->notifier()->first(*this);
      }

      ConstMapIt(const Map& _map, const Item& item)
        : Parent(item), map(_map) {}

      ConstMapIt& operator++() {
        map->notifier()->next(*this);
        return *this;
      }

      typename MapTraits<Map>::ConstReturnValue operator*() const {
        return map[*this];
      }

    protected:
      const Map* map;
    };

    class ItemIt : public Item {
      typedef Item Parent;

    public:
      ItemIt() : map(NULL) {}


      ItemIt(Invalid i) : Parent(i), map(NULL) {}

      explicit ItemIt(Map& _map) : map(&_map) {
        map->notifier()->first(*this);
      }

      ItemIt(const Map& _map, const Item& item)
        : Parent(item), map(&_map) {}

      ItemIt& operator++() {
        map->notifier()->next(*this);
        return *this;
      }

    protected:
      const Map* map;

    };
  };

  // \ingroup graphbits
  //
  // \brief Extender for maps which use a subset of the items.
  template <typename _Graph, typename _Map>
  class SubMapExtender : public _Map {
    typedef _Map Parent;
    typedef _Graph GraphType;

  public:

    typedef SubMapExtender Map;
    typedef typename Parent::Key Item;

    typedef typename Parent::Key Key;
    typedef typename Parent::Value Value;
    typedef typename Parent::Reference Reference;
    typedef typename Parent::ConstReference ConstReference;

    typedef typename Parent::ReferenceMapTag ReferenceMapTag;

    class MapIt;
    class ConstMapIt;

    friend class MapIt;
    friend class ConstMapIt;

  public:

    SubMapExtender(const GraphType& _graph)
      : Parent(_graph), graph(_graph) {}

    SubMapExtender(const GraphType& _graph, const Value& _value)
      : Parent(_graph, _value), graph(_graph) {}

  private:
    SubMapExtender& operator=(const SubMapExtender& cmap) {
      return operator=<MapExtender>(cmap);
    }

    template <typename CMap>
    SubMapExtender& operator=(const CMap& cmap) {
      checkConcept<concepts::ReadMap<Key, Value>, CMap>();
      Item it;
      for (graph.first(it); it != INVALID; graph.next(it)) {
        Parent::set(it, cmap[it]);
      }
      return *this;
    }

  public:
    class MapIt : public Item {
      typedef Item Parent;

    public:
      typedef typename Map::Value Value;

      MapIt() : map(NULL) {}

      MapIt(Invalid i) : Parent(i), map(NULL) { }

      explicit MapIt(Map& _map) : map(&_map) {
        map->graph.first(*this);
      }

      MapIt(const Map& _map, const Item& item)
        : Parent(item), map(&_map) {}

      MapIt& operator++() {
        map->graph.next(*this);
        return *this;
      }

      typename MapTraits<Map>::ConstReturnValue operator*() const {
        return (*map)[*this];
      }

      typename MapTraits<Map>::ReturnValue operator*() {
        return (*map)[*this];
      }

      void set(const Value& value) {
        map->set(*this, value);
      }

    protected:
      Map* map;

    };

    class ConstMapIt : public Item {
      typedef Item Parent;

    public:

      typedef typename Map::Value Value;

      ConstMapIt() : map(NULL) {}

      ConstMapIt(Invalid i) : Parent(i), map(NULL) { }

      explicit ConstMapIt(Map& _map) : map(&_map) {
        map->graph.first(*this);
      }

      ConstMapIt(const Map& _map, const Item& item)
        : Parent(item), map(&_map) {}

      ConstMapIt& operator++() {
        map->graph.next(*this);
        return *this;
      }

      typename MapTraits<Map>::ConstReturnValue operator*() const {
        return (*map)[*this];
      }

    protected:
      const Map* map;
    };

    class ItemIt : public Item {
      typedef Item Parent;

    public:
      ItemIt() : map(NULL) {}


      ItemIt(Invalid i) : Parent(i), map(NULL) { }

      explicit ItemIt(Map& _map) : map(&_map) {
        map->graph.first(*this);
      }

      ItemIt(const Map& _map, const Item& item)
        : Parent(item), map(&_map) {}

      ItemIt& operator++() {
        map->graph.next(*this);
        return *this;
      }

    protected:
      const Map* map;

    };

  private:

    const GraphType& graph;

  };

}

#endif
