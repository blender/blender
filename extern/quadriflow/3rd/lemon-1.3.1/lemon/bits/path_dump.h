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

#ifndef LEMON_BITS_PATH_DUMP_H
#define LEMON_BITS_PATH_DUMP_H

#include <lemon/core.h>
#include <lemon/concept_check.h>

namespace lemon {

  template <typename _Digraph, typename _PredMap>
  class PredMapPath {
  public:
    typedef True RevPathTag;

    typedef _Digraph Digraph;
    typedef typename Digraph::Arc Arc;
    typedef _PredMap PredMap;

    PredMapPath(const Digraph& _digraph, const PredMap& _predMap,
                typename Digraph::Node _target)
      : digraph(_digraph), predMap(_predMap), target(_target) {}

    int length() const {
      int len = 0;
      typename Digraph::Node node = target;
      typename Digraph::Arc arc;
      while ((arc = predMap[node]) != INVALID) {
        node = digraph.source(arc);
        ++len;
      }
      return len;
    }

    bool empty() const {
      return predMap[target] == INVALID;
    }

    class RevArcIt {
    public:
      RevArcIt() {}
      RevArcIt(Invalid) : path(0), current(INVALID) {}
      RevArcIt(const PredMapPath& _path)
        : path(&_path), current(_path.target) {
        if (path->predMap[current] == INVALID) current = INVALID;
      }

      operator const typename Digraph::Arc() const {
        return path->predMap[current];
      }

      RevArcIt& operator++() {
        current = path->digraph.source(path->predMap[current]);
        if (path->predMap[current] == INVALID) current = INVALID;
        return *this;
      }

      bool operator==(const RevArcIt& e) const {
        return current == e.current;
      }

      bool operator!=(const RevArcIt& e) const {
        return current != e.current;
      }

      bool operator<(const RevArcIt& e) const {
        return current < e.current;
      }

    private:
      const PredMapPath* path;
      typename Digraph::Node current;
    };

  private:
    const Digraph& digraph;
    const PredMap& predMap;
    typename Digraph::Node target;
  };


  template <typename _Digraph, typename _PredMatrixMap>
  class PredMatrixMapPath {
  public:
    typedef True RevPathTag;

    typedef _Digraph Digraph;
    typedef typename Digraph::Arc Arc;
    typedef _PredMatrixMap PredMatrixMap;

    PredMatrixMapPath(const Digraph& _digraph,
                      const PredMatrixMap& _predMatrixMap,
                      typename Digraph::Node _source,
                      typename Digraph::Node _target)
      : digraph(_digraph), predMatrixMap(_predMatrixMap),
        source(_source), target(_target) {}

    int length() const {
      int len = 0;
      typename Digraph::Node node = target;
      typename Digraph::Arc arc;
      while ((arc = predMatrixMap(source, node)) != INVALID) {
        node = digraph.source(arc);
        ++len;
      }
      return len;
    }

    bool empty() const {
      return predMatrixMap(source, target) == INVALID;
    }

    class RevArcIt {
    public:
      RevArcIt() {}
      RevArcIt(Invalid) : path(0), current(INVALID) {}
      RevArcIt(const PredMatrixMapPath& _path)
        : path(&_path), current(_path.target) {
        if (path->predMatrixMap(path->source, current) == INVALID)
          current = INVALID;
      }

      operator const typename Digraph::Arc() const {
        return path->predMatrixMap(path->source, current);
      }

      RevArcIt& operator++() {
        current =
          path->digraph.source(path->predMatrixMap(path->source, current));
        if (path->predMatrixMap(path->source, current) == INVALID)
          current = INVALID;
        return *this;
      }

      bool operator==(const RevArcIt& e) const {
        return current == e.current;
      }

      bool operator!=(const RevArcIt& e) const {
        return current != e.current;
      }

      bool operator<(const RevArcIt& e) const {
        return current < e.current;
      }

    private:
      const PredMatrixMapPath* path;
      typename Digraph::Node current;
    };

  private:
    const Digraph& digraph;
    const PredMatrixMap& predMatrixMap;
    typename Digraph::Node source;
    typename Digraph::Node target;
  };

}

#endif
