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

///\ingroup concept
///\file
///\brief The concept of paths
///

#ifndef LEMON_CONCEPTS_PATH_H
#define LEMON_CONCEPTS_PATH_H

#include <lemon/core.h>
#include <lemon/concept_check.h>

namespace lemon {
  namespace concepts {

    /// \addtogroup concept
    /// @{

    /// \brief A skeleton structure for representing directed paths in
    /// a digraph.
    ///
    /// A skeleton structure for representing directed paths in a
    /// digraph.
    /// In a sense, a path can be treated as a list of arcs.
    /// LEMON path types just store this list. As a consequence, they cannot
    /// enumerate the nodes on the path directly and a zero length path
    /// cannot store its source node.
    ///
    /// The arcs of a path should be stored in the order of their directions,
    /// i.e. the target node of each arc should be the same as the source
    /// node of the next arc. This consistency could be checked using
    /// \ref checkPath().
    /// The source and target nodes of a (consistent) path can be obtained
    /// using \ref pathSource() and \ref pathTarget().
    ///
    /// A path can be constructed from another path of any type using the
    /// copy constructor or the assignment operator.
    ///
    /// \tparam GR The digraph type in which the path is.
    template <typename GR>
    class Path {
    public:

      /// Type of the underlying digraph.
      typedef GR Digraph;
      /// Arc type of the underlying digraph.
      typedef typename Digraph::Arc Arc;

      class ArcIt;

      /// \brief Default constructor
      Path() {}

      /// \brief Template copy constructor
      template <typename CPath>
      Path(const CPath& cpath) {}

      /// \brief Template assigment operator
      template <typename CPath>
      Path& operator=(const CPath& cpath) {
        ::lemon::ignore_unused_variable_warning(cpath);
        return *this;
      }

      /// Length of the path, i.e. the number of arcs on the path.
      int length() const { return 0;}

      /// Returns whether the path is empty.
      bool empty() const { return true;}

      /// Resets the path to an empty path.
      void clear() {}

      /// \brief LEMON style iterator for enumerating the arcs of a path.
      ///
      /// LEMON style iterator class for enumerating the arcs of a path.
      class ArcIt {
      public:
        /// Default constructor
        ArcIt() {}
        /// Invalid constructor
        ArcIt(Invalid) {}
        /// Sets the iterator to the first arc of the given path
        ArcIt(const Path &) {}

        /// Conversion to \c Arc
        operator Arc() const { return INVALID; }

        /// Next arc
        ArcIt& operator++() {return *this;}

        /// Comparison operator
        bool operator==(const ArcIt&) const {return true;}
        /// Comparison operator
        bool operator!=(const ArcIt&) const {return true;}
        /// Comparison operator
        bool operator<(const ArcIt&) const {return false;}

      };

      template <typename _Path>
      struct Constraints {
        void constraints() {
          Path<Digraph> pc;
          _Path p, pp(pc);
          int l = p.length();
          int e = p.empty();
          p.clear();

          p = pc;

          typename _Path::ArcIt id, ii(INVALID), i(p);

          ++i;
          typename Digraph::Arc ed = i;

          e = (i == ii);
          e = (i != ii);
          e = (i < ii);

          ::lemon::ignore_unused_variable_warning(l);
          ::lemon::ignore_unused_variable_warning(pp);
          ::lemon::ignore_unused_variable_warning(e);
          ::lemon::ignore_unused_variable_warning(id);
          ::lemon::ignore_unused_variable_warning(ii);
          ::lemon::ignore_unused_variable_warning(ed);
        }
      };

    };

    namespace _path_bits {

      template <typename _Digraph, typename _Path, typename RevPathTag = void>
      struct PathDumperConstraints {
        void constraints() {
          int l = p.length();
          int e = p.empty();

          typename _Path::ArcIt id, i(p);

          ++i;
          typename _Digraph::Arc ed = i;

          e = (i == INVALID);
          e = (i != INVALID);

          ::lemon::ignore_unused_variable_warning(l);
          ::lemon::ignore_unused_variable_warning(e);
          ::lemon::ignore_unused_variable_warning(id);
          ::lemon::ignore_unused_variable_warning(ed);
        }
        _Path& p;
        PathDumperConstraints() {}
      };

      template <typename _Digraph, typename _Path>
      struct PathDumperConstraints<
        _Digraph, _Path,
        typename enable_if<typename _Path::RevPathTag, void>::type
      > {
        void constraints() {
          int l = p.length();
          int e = p.empty();

          typename _Path::RevArcIt id, i(p);

          ++i;
          typename _Digraph::Arc ed = i;

          e = (i == INVALID);
          e = (i != INVALID);

          ::lemon::ignore_unused_variable_warning(l);
          ::lemon::ignore_unused_variable_warning(e);
          ::lemon::ignore_unused_variable_warning(id);
          ::lemon::ignore_unused_variable_warning(ed);
        }
        _Path& p;
        PathDumperConstraints() {}
      };

    }


    /// \brief A skeleton structure for path dumpers.
    ///
    /// A skeleton structure for path dumpers. The path dumpers are
    /// the generalization of the paths, they can enumerate the arcs
    /// of the path either in forward or in backward order.
    /// These classes are typically not used directly, they are rather
    /// used to be assigned to a real path type.
    ///
    /// The main purpose of this concept is that the shortest path
    /// algorithms can enumerate the arcs easily in reverse order.
    /// In LEMON, such algorithms give back a (reverse) path dumper that
    /// can be assigned to a real path. The dumpers can be implemented as
    /// an adaptor class to the predecessor map.
    ///
    /// \tparam GR The digraph type in which the path is.
    template <typename GR>
    class PathDumper {
    public:

      /// Type of the underlying digraph.
      typedef GR Digraph;
      /// Arc type of the underlying digraph.
      typedef typename Digraph::Arc Arc;

      /// Length of the path, i.e. the number of arcs on the path.
      int length() const { return 0;}

      /// Returns whether the path is empty.
      bool empty() const { return true;}

      /// \brief Forward or reverse dumping
      ///
      /// If this tag is defined to be \c True, then reverse dumping
      /// is provided in the path dumper. In this case, \c RevArcIt
      /// iterator should be implemented instead of \c ArcIt iterator.
      typedef False RevPathTag;

      /// \brief LEMON style iterator for enumerating the arcs of a path.
      ///
      /// LEMON style iterator class for enumerating the arcs of a path.
      class ArcIt {
      public:
        /// Default constructor
        ArcIt() {}
        /// Invalid constructor
        ArcIt(Invalid) {}
        /// Sets the iterator to the first arc of the given path
        ArcIt(const PathDumper&) {}

        /// Conversion to \c Arc
        operator Arc() const { return INVALID; }

        /// Next arc
        ArcIt& operator++() {return *this;}

        /// Comparison operator
        bool operator==(const ArcIt&) const {return true;}
        /// Comparison operator
        bool operator!=(const ArcIt&) const {return true;}
        /// Comparison operator
        bool operator<(const ArcIt&) const {return false;}

      };

      /// \brief LEMON style iterator for enumerating the arcs of a path
      /// in reverse direction.
      ///
      /// LEMON style iterator class for enumerating the arcs of a path
      /// in reverse direction.
      class RevArcIt {
      public:
        /// Default constructor
        RevArcIt() {}
        /// Invalid constructor
        RevArcIt(Invalid) {}
        /// Sets the iterator to the last arc of the given path
        RevArcIt(const PathDumper &) {}

        /// Conversion to \c Arc
        operator Arc() const { return INVALID; }

        /// Next arc
        RevArcIt& operator++() {return *this;}

        /// Comparison operator
        bool operator==(const RevArcIt&) const {return true;}
        /// Comparison operator
        bool operator!=(const RevArcIt&) const {return true;}
        /// Comparison operator
        bool operator<(const RevArcIt&) const {return false;}

      };

      template <typename _Path>
      struct Constraints {
        void constraints() {
          function_requires<_path_bits::
            PathDumperConstraints<Digraph, _Path> >();
        }
      };

    };


    ///@}
  }

} // namespace lemon

#endif
