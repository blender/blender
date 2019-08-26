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

///\ingroup paths
///\file
///\brief Classes for representing paths in digraphs.
///

#ifndef LEMON_PATH_H
#define LEMON_PATH_H

#include <vector>
#include <algorithm>

#include <lemon/error.h>
#include <lemon/core.h>
#include <lemon/concepts/path.h>

namespace lemon {

  /// \addtogroup paths
  /// @{


  /// \brief A structure for representing directed paths in a digraph.
  ///
  /// A structure for representing directed path in a digraph.
  /// \tparam GR The digraph type in which the path is.
  ///
  /// In a sense, the path can be treated as a list of arcs. The
  /// LEMON path type stores just this list. As a consequence, it
  /// cannot enumerate the nodes of the path and the source node of
  /// a zero length path is undefined.
  ///
  /// This implementation is a back and front insertable and erasable
  /// path type. It can be indexed in O(1) time. The front and back
  /// insertion and erase is done in O(1) (amortized) time. The
  /// implementation uses two vectors for storing the front and back
  /// insertions.
  template <typename GR>
  class Path {
  public:

    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;

    /// \brief Default constructor
    ///
    /// Default constructor
    Path() {}

    /// \brief Copy constructor
    ///
    Path(const Path& cpath) {
      pathCopy(cpath, *this);
    }

    /// \brief Template copy constructor
    ///
    /// This constuctor initializes the path from any other path type.
    /// It simply makes a copy of the given path.
    template <typename CPath>
    Path(const CPath& cpath) {
      pathCopy(cpath, *this);
    }

    /// \brief Copy assignment
    ///
    Path& operator=(const Path& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Template copy assignment
    ///
    /// This operator makes a copy of a path of any other type.
    template <typename CPath>
    Path& operator=(const CPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief LEMON style iterator for path arcs
    ///
    /// This class is used to iterate on the arcs of the paths.
    class ArcIt {
      friend class Path;
    public:
      /// \brief Default constructor
      ArcIt() {}
      /// \brief Invalid constructor
      ArcIt(Invalid) : path(0), idx(-1) {}
      /// \brief Initializate the iterator to the first arc of path
      ArcIt(const Path &_path)
        : path(&_path), idx(_path.empty() ? -1 : 0) {}

    private:

      ArcIt(const Path &_path, int _idx)
        : path(&_path), idx(_idx) {}

    public:

      /// \brief Conversion to Arc
      operator const Arc&() const {
        return path->nth(idx);
      }

      /// \brief Next arc
      ArcIt& operator++() {
        ++idx;
        if (idx >= path->length()) idx = -1;
        return *this;
      }

      /// \brief Comparison operator
      bool operator==(const ArcIt& e) const { return idx==e.idx; }
      /// \brief Comparison operator
      bool operator!=(const ArcIt& e) const { return idx!=e.idx; }
      /// \brief Comparison operator
      bool operator<(const ArcIt& e) const { return idx<e.idx; }

    private:
      const Path *path;
      int idx;
    };

    /// \brief Length of the path.
    int length() const { return head.size() + tail.size(); }
    /// \brief Return whether the path is empty.
    bool empty() const { return head.empty() && tail.empty(); }

    /// \brief Reset the path to an empty one.
    void clear() { head.clear(); tail.clear(); }

    /// \brief The n-th arc.
    ///
    /// \pre \c n is in the <tt>[0..length() - 1]</tt> range.
    const Arc& nth(int n) const {
      return n < int(head.size()) ? *(head.rbegin() + n) :
        *(tail.begin() + (n - head.size()));
    }

    /// \brief Initialize arc iterator to point to the n-th arc
    ///
    /// \pre \c n is in the <tt>[0..length() - 1]</tt> range.
    ArcIt nthIt(int n) const {
      return ArcIt(*this, n);
    }

    /// \brief The first arc of the path
    const Arc& front() const {
      return head.empty() ? tail.front() : head.back();
    }

    /// \brief Add a new arc before the current path
    void addFront(const Arc& arc) {
      head.push_back(arc);
    }

    /// \brief Erase the first arc of the path
    void eraseFront() {
      if (!head.empty()) {
        head.pop_back();
      } else {
        head.clear();
        int halfsize = tail.size() / 2;
        head.resize(halfsize);
        std::copy(tail.begin() + 1, tail.begin() + halfsize + 1,
                  head.rbegin());
        std::copy(tail.begin() + halfsize + 1, tail.end(), tail.begin());
        tail.resize(tail.size() - halfsize - 1);
      }
    }

    /// \brief The last arc of the path
    const Arc& back() const {
      return tail.empty() ? head.front() : tail.back();
    }

    /// \brief Add a new arc behind the current path
    void addBack(const Arc& arc) {
      tail.push_back(arc);
    }

    /// \brief Erase the last arc of the path
    void eraseBack() {
      if (!tail.empty()) {
        tail.pop_back();
      } else {
        int halfsize = head.size() / 2;
        tail.resize(halfsize);
        std::copy(head.begin() + 1, head.begin() + halfsize + 1,
                  tail.rbegin());
        std::copy(head.begin() + halfsize + 1, head.end(), head.begin());
        head.resize(head.size() - halfsize - 1);
      }
    }

    typedef True BuildTag;

    template <typename CPath>
    void build(const CPath& path) {
      int len = path.length();
      tail.reserve(len);
      for (typename CPath::ArcIt it(path); it != INVALID; ++it) {
        tail.push_back(it);
      }
    }

    template <typename CPath>
    void buildRev(const CPath& path) {
      int len = path.length();
      head.reserve(len);
      for (typename CPath::RevArcIt it(path); it != INVALID; ++it) {
        head.push_back(it);
      }
    }

  protected:
    typedef std::vector<Arc> Container;
    Container head, tail;

  };

  /// \brief A structure for representing directed paths in a digraph.
  ///
  /// A structure for representing directed path in a digraph.
  /// \tparam GR The digraph type in which the path is.
  ///
  /// In a sense, the path can be treated as a list of arcs. The
  /// LEMON path type stores just this list. As a consequence it
  /// cannot enumerate the nodes in the path and the zero length paths
  /// cannot store the source.
  ///
  /// This implementation is a just back insertable and erasable path
  /// type. It can be indexed in O(1) time. The back insertion and
  /// erasure is amortized O(1) time. This implementation is faster
  /// then the \c Path type because it use just one vector for the
  /// arcs.
  template <typename GR>
  class SimplePath {
  public:

    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;

    /// \brief Default constructor
    ///
    /// Default constructor
    SimplePath() {}

    /// \brief Copy constructor
    ///
    SimplePath(const SimplePath& cpath) {
      pathCopy(cpath, *this);
    }

    /// \brief Template copy constructor
    ///
    /// This path can be initialized with any other path type. It just
    /// makes a copy of the given path.
    template <typename CPath>
    SimplePath(const CPath& cpath) {
      pathCopy(cpath, *this);
    }

    /// \brief Copy assignment
    ///
    SimplePath& operator=(const SimplePath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Template copy assignment
    ///
    /// This path can be initialized with any other path type. It just
    /// makes a copy of the given path.
    template <typename CPath>
    SimplePath& operator=(const CPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Iterator class to iterate on the arcs of the paths
    ///
    /// This class is used to iterate on the arcs of the paths
    ///
    /// Of course it converts to Digraph::Arc
    class ArcIt {
      friend class SimplePath;
    public:
      /// Default constructor
      ArcIt() {}
      /// Invalid constructor
      ArcIt(Invalid) : path(0), idx(-1) {}
      /// \brief Initializate the constructor to the first arc of path
      ArcIt(const SimplePath &_path)
        : path(&_path), idx(_path.empty() ? -1 : 0) {}

    private:

      /// Constructor with starting point
      ArcIt(const SimplePath &_path, int _idx)
        : path(&_path), idx(_idx) {}

    public:

      ///Conversion to Digraph::Arc
      operator const Arc&() const {
        return path->nth(idx);
      }

      /// Next arc
      ArcIt& operator++() {
        ++idx;
        if (idx >= path->length()) idx = -1;
        return *this;
      }

      /// Comparison operator
      bool operator==(const ArcIt& e) const { return idx==e.idx; }
      /// Comparison operator
      bool operator!=(const ArcIt& e) const { return idx!=e.idx; }
      /// Comparison operator
      bool operator<(const ArcIt& e) const { return idx<e.idx; }

    private:
      const SimplePath *path;
      int idx;
    };

    /// \brief Length of the path.
    int length() const { return data.size(); }
    /// \brief Return true if the path is empty.
    bool empty() const { return data.empty(); }

    /// \brief Reset the path to an empty one.
    void clear() { data.clear(); }

    /// \brief The n-th arc.
    ///
    /// \pre \c n is in the <tt>[0..length() - 1]</tt> range.
    const Arc& nth(int n) const {
      return data[n];
    }

    /// \brief  Initializes arc iterator to point to the n-th arc.
    ArcIt nthIt(int n) const {
      return ArcIt(*this, n);
    }

    /// \brief The first arc of the path.
    const Arc& front() const {
      return data.front();
    }

    /// \brief The last arc of the path.
    const Arc& back() const {
      return data.back();
    }

    /// \brief Add a new arc behind the current path.
    void addBack(const Arc& arc) {
      data.push_back(arc);
    }

    /// \brief Erase the last arc of the path
    void eraseBack() {
      data.pop_back();
    }

    typedef True BuildTag;

    template <typename CPath>
    void build(const CPath& path) {
      int len = path.length();
      data.resize(len);
      int index = 0;
      for (typename CPath::ArcIt it(path); it != INVALID; ++it) {
        data[index] = it;;
        ++index;
      }
    }

    template <typename CPath>
    void buildRev(const CPath& path) {
      int len = path.length();
      data.resize(len);
      int index = len;
      for (typename CPath::RevArcIt it(path); it != INVALID; ++it) {
        --index;
        data[index] = it;;
      }
    }

  protected:
    typedef std::vector<Arc> Container;
    Container data;

  };

  /// \brief A structure for representing directed paths in a digraph.
  ///
  /// A structure for representing directed path in a digraph.
  /// \tparam GR The digraph type in which the path is.
  ///
  /// In a sense, the path can be treated as a list of arcs. The
  /// LEMON path type stores just this list. As a consequence it
  /// cannot enumerate the nodes in the path and the zero length paths
  /// cannot store the source.
  ///
  /// This implementation is a back and front insertable and erasable
  /// path type. It can be indexed in O(k) time, where k is the rank
  /// of the arc in the path. The length can be computed in O(n)
  /// time. The front and back insertion and erasure is O(1) time
  /// and it can be splited and spliced in O(1) time.
  template <typename GR>
  class ListPath {
  public:

    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;

  protected:

    // the std::list<> is incompatible
    // hard to create invalid iterator
    struct Node {
      Arc arc;
      Node *next, *prev;
    };

    Node *first, *last;

    std::allocator<Node> alloc;

  public:

    /// \brief Default constructor
    ///
    /// Default constructor
    ListPath() : first(0), last(0) {}

    /// \brief Copy constructor
    ///
    ListPath(const ListPath& cpath) : first(0), last(0) {
      pathCopy(cpath, *this);
    }

    /// \brief Template copy constructor
    ///
    /// This path can be initialized with any other path type. It just
    /// makes a copy of the given path.
    template <typename CPath>
    ListPath(const CPath& cpath) : first(0), last(0) {
      pathCopy(cpath, *this);
    }

    /// \brief Destructor of the path
    ///
    /// Destructor of the path
    ~ListPath() {
      clear();
    }

    /// \brief Copy assignment
    ///
    ListPath& operator=(const ListPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Template copy assignment
    ///
    /// This path can be initialized with any other path type. It just
    /// makes a copy of the given path.
    template <typename CPath>
    ListPath& operator=(const CPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Iterator class to iterate on the arcs of the paths
    ///
    /// This class is used to iterate on the arcs of the paths
    ///
    /// Of course it converts to Digraph::Arc
    class ArcIt {
      friend class ListPath;
    public:
      /// Default constructor
      ArcIt() {}
      /// Invalid constructor
      ArcIt(Invalid) : path(0), node(0) {}
      /// \brief Initializate the constructor to the first arc of path
      ArcIt(const ListPath &_path)
        : path(&_path), node(_path.first) {}

    protected:

      ArcIt(const ListPath &_path, Node *_node)
        : path(&_path), node(_node) {}


    public:

      ///Conversion to Digraph::Arc
      operator const Arc&() const {
        return node->arc;
      }

      /// Next arc
      ArcIt& operator++() {
        node = node->next;
        return *this;
      }

      /// Comparison operator
      bool operator==(const ArcIt& e) const { return node==e.node; }
      /// Comparison operator
      bool operator!=(const ArcIt& e) const { return node!=e.node; }
      /// Comparison operator
      bool operator<(const ArcIt& e) const { return node<e.node; }

    private:
      const ListPath *path;
      Node *node;
    };

    /// \brief The n-th arc.
    ///
    /// This function looks for the n-th arc in O(n) time.
    /// \pre \c n is in the <tt>[0..length() - 1]</tt> range.
    const Arc& nth(int n) const {
      Node *node = first;
      for (int i = 0; i < n; ++i) {
        node = node->next;
      }
      return node->arc;
    }

    /// \brief Initializes arc iterator to point to the n-th arc.
    ArcIt nthIt(int n) const {
      Node *node = first;
      for (int i = 0; i < n; ++i) {
        node = node->next;
      }
      return ArcIt(*this, node);
    }

    /// \brief Length of the path.
    int length() const {
      int len = 0;
      Node *node = first;
      while (node != 0) {
        node = node->next;
        ++len;
      }
      return len;
    }

    /// \brief Return true if the path is empty.
    bool empty() const { return first == 0; }

    /// \brief Reset the path to an empty one.
    void clear() {
      while (first != 0) {
        last = first->next;
        alloc.destroy(first);
        alloc.deallocate(first, 1);
        first = last;
      }
    }

    /// \brief The first arc of the path
    const Arc& front() const {
      return first->arc;
    }

    /// \brief Add a new arc before the current path
    void addFront(const Arc& arc) {
      Node *node = alloc.allocate(1);
      alloc.construct(node, Node());
      node->prev = 0;
      node->next = first;
      node->arc = arc;
      if (first) {
        first->prev = node;
        first = node;
      } else {
        first = last = node;
      }
    }

    /// \brief Erase the first arc of the path
    void eraseFront() {
      Node *node = first;
      first = first->next;
      if (first) {
        first->prev = 0;
      } else {
        last = 0;
      }
      alloc.destroy(node);
      alloc.deallocate(node, 1);
    }

    /// \brief The last arc of the path.
    const Arc& back() const {
      return last->arc;
    }

    /// \brief Add a new arc behind the current path.
    void addBack(const Arc& arc) {
      Node *node = alloc.allocate(1);
      alloc.construct(node, Node());
      node->next = 0;
      node->prev = last;
      node->arc = arc;
      if (last) {
        last->next = node;
        last = node;
      } else {
        last = first = node;
      }
    }

    /// \brief Erase the last arc of the path
    void eraseBack() {
      Node *node = last;
      last = last->prev;
      if (last) {
        last->next = 0;
      } else {
        first = 0;
      }
      alloc.destroy(node);
      alloc.deallocate(node, 1);
    }

    /// \brief Splice a path to the back of the current path.
    ///
    /// It splices \c tpath to the back of the current path and \c
    /// tpath becomes empty. The time complexity of this function is
    /// O(1).
    void spliceBack(ListPath& tpath) {
      if (first) {
        if (tpath.first) {
          last->next = tpath.first;
          tpath.first->prev = last;
          last = tpath.last;
        }
      } else {
        first = tpath.first;
        last = tpath.last;
      }
      tpath.first = tpath.last = 0;
    }

    /// \brief Splice a path to the front of the current path.
    ///
    /// It splices \c tpath before the current path and \c tpath
    /// becomes empty. The time complexity of this function
    /// is O(1).
    void spliceFront(ListPath& tpath) {
      if (first) {
        if (tpath.first) {
          first->prev = tpath.last;
          tpath.last->next = first;
          first = tpath.first;
        }
      } else {
        first = tpath.first;
        last = tpath.last;
      }
      tpath.first = tpath.last = 0;
    }

    /// \brief Splice a path into the current path.
    ///
    /// It splices the \c tpath into the current path before the
    /// position of \c it iterator and \c tpath becomes empty. The
    /// time complexity of this function is O(1). If the \c it is
    /// \c INVALID then it will splice behind the current path.
    void splice(ArcIt it, ListPath& tpath) {
      if (it.node) {
        if (tpath.first) {
          tpath.first->prev = it.node->prev;
          if (it.node->prev) {
            it.node->prev->next = tpath.first;
          } else {
            first = tpath.first;
          }
          it.node->prev = tpath.last;
          tpath.last->next = it.node;
        }
      } else {
        if (first) {
          if (tpath.first) {
            last->next = tpath.first;
            tpath.first->prev = last;
            last = tpath.last;
          }
        } else {
          first = tpath.first;
          last = tpath.last;
        }
      }
      tpath.first = tpath.last = 0;
    }

    /// \brief Split the current path.
    ///
    /// It splits the current path into two parts. The part before
    /// the iterator \c it will remain in the current path and the part
    /// starting with
    /// \c it will put into \c tpath. If \c tpath have arcs
    /// before the operation they are removed first.  The time
    /// complexity of this function is O(1) plus the the time of emtying
    /// \c tpath. If \c it is \c INVALID then it just clears \c tpath
    void split(ArcIt it, ListPath& tpath) {
      tpath.clear();
      if (it.node) {
        tpath.first = it.node;
        tpath.last = last;
        if (it.node->prev) {
          last = it.node->prev;
          last->next = 0;
        } else {
          first = last = 0;
        }
        it.node->prev = 0;
      }
    }


    typedef True BuildTag;

    template <typename CPath>
    void build(const CPath& path) {
      for (typename CPath::ArcIt it(path); it != INVALID; ++it) {
        addBack(it);
      }
    }

    template <typename CPath>
    void buildRev(const CPath& path) {
      for (typename CPath::RevArcIt it(path); it != INVALID; ++it) {
        addFront(it);
      }
    }

  };

  /// \brief A structure for representing directed paths in a digraph.
  ///
  /// A structure for representing directed path in a digraph.
  /// \tparam GR The digraph type in which the path is.
  ///
  /// In a sense, the path can be treated as a list of arcs. The
  /// LEMON path type stores just this list. As a consequence it
  /// cannot enumerate the nodes in the path and the source node of
  /// a zero length path is undefined.
  ///
  /// This implementation is completly static, i.e. it can be copy constucted
  /// or copy assigned from another path, but otherwise it cannot be
  /// modified.
  ///
  /// Being the the most memory efficient path type in LEMON,
  /// it is intented to be
  /// used when you want to store a large number of paths.
  template <typename GR>
  class StaticPath {
  public:

    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;

    /// \brief Default constructor
    ///
    /// Default constructor
    StaticPath() : len(0), arcs(0) {}

    /// \brief Copy constructor
    ///
    StaticPath(const StaticPath& cpath) : arcs(0) {
      pathCopy(cpath, *this);
    }

    /// \brief Template copy constructor
    ///
    /// This path can be initialized from any other path type.
    template <typename CPath>
    StaticPath(const CPath& cpath) : arcs(0) {
      pathCopy(cpath, *this);
    }

    /// \brief Destructor of the path
    ///
    /// Destructor of the path
    ~StaticPath() {
      if (arcs) delete[] arcs;
    }

    /// \brief Copy assignment
    ///
    StaticPath& operator=(const StaticPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Template copy assignment
    ///
    /// This path can be made equal to any other path type. It simply
    /// makes a copy of the given path.
    template <typename CPath>
    StaticPath& operator=(const CPath& cpath) {
      pathCopy(cpath, *this);
      return *this;
    }

    /// \brief Iterator class to iterate on the arcs of the paths
    ///
    /// This class is used to iterate on the arcs of the paths
    ///
    /// Of course it converts to Digraph::Arc
    class ArcIt {
      friend class StaticPath;
    public:
      /// Default constructor
      ArcIt() {}
      /// Invalid constructor
      ArcIt(Invalid) : path(0), idx(-1) {}
      /// Initializate the constructor to the first arc of path
      ArcIt(const StaticPath &_path)
        : path(&_path), idx(_path.empty() ? -1 : 0) {}

    private:

      /// Constructor with starting point
      ArcIt(const StaticPath &_path, int _idx)
        : idx(_idx), path(&_path) {}

    public:

      ///Conversion to Digraph::Arc
      operator const Arc&() const {
        return path->nth(idx);
      }

      /// Next arc
      ArcIt& operator++() {
        ++idx;
        if (idx >= path->length()) idx = -1;
        return *this;
      }

      /// Comparison operator
      bool operator==(const ArcIt& e) const { return idx==e.idx; }
      /// Comparison operator
      bool operator!=(const ArcIt& e) const { return idx!=e.idx; }
      /// Comparison operator
      bool operator<(const ArcIt& e) const { return idx<e.idx; }

    private:
      const StaticPath *path;
      int idx;
    };

    /// \brief The n-th arc.
    ///
    /// \pre \c n is in the <tt>[0..length() - 1]</tt> range.
    const Arc& nth(int n) const {
      return arcs[n];
    }

    /// \brief The arc iterator pointing to the n-th arc.
    ArcIt nthIt(int n) const {
      return ArcIt(*this, n);
    }

    /// \brief The length of the path.
    int length() const { return len; }

    /// \brief Return true when the path is empty.
    int empty() const { return len == 0; }

    /// \brief Erase all arcs in the digraph.
    void clear() {
      len = 0;
      if (arcs) delete[] arcs;
      arcs = 0;
    }

    /// \brief The first arc of the path.
    const Arc& front() const {
      return arcs[0];
    }

    /// \brief The last arc of the path.
    const Arc& back() const {
      return arcs[len - 1];
    }


    typedef True BuildTag;

    template <typename CPath>
    void build(const CPath& path) {
      len = path.length();
      arcs = new Arc[len];
      int index = 0;
      for (typename CPath::ArcIt it(path); it != INVALID; ++it) {
        arcs[index] = it;
        ++index;
      }
    }

    template <typename CPath>
    void buildRev(const CPath& path) {
      len = path.length();
      arcs = new Arc[len];
      int index = len;
      for (typename CPath::RevArcIt it(path); it != INVALID; ++it) {
        --index;
        arcs[index] = it;
      }
    }

  private:
    int len;
    Arc* arcs;
  };

  ///////////////////////////////////////////////////////////////////////
  // Additional utilities
  ///////////////////////////////////////////////////////////////////////

  namespace _path_bits {

    template <typename Path, typename Enable = void>
    struct RevPathTagIndicator {
      static const bool value = false;
    };

    template <typename Path>
    struct RevPathTagIndicator<
      Path,
      typename enable_if<typename Path::RevPathTag, void>::type
      > {
      static const bool value = true;
    };

    template <typename Path, typename Enable = void>
    struct BuildTagIndicator {
      static const bool value = false;
    };

    template <typename Path>
    struct BuildTagIndicator<
      Path,
      typename enable_if<typename Path::BuildTag, void>::type
    > {
      static const bool value = true;
    };

    template <typename From, typename To,
              bool buildEnable = BuildTagIndicator<To>::value>
    struct PathCopySelectorForward {
      static void copy(const From& from, To& to) {
        to.clear();
        for (typename From::ArcIt it(from); it != INVALID; ++it) {
          to.addBack(it);
        }
      }
    };

    template <typename From, typename To>
    struct PathCopySelectorForward<From, To, true> {
      static void copy(const From& from, To& to) {
        to.clear();
        to.build(from);
      }
    };

    template <typename From, typename To,
              bool buildEnable = BuildTagIndicator<To>::value>
    struct PathCopySelectorBackward {
      static void copy(const From& from, To& to) {
        to.clear();
        for (typename From::RevArcIt it(from); it != INVALID; ++it) {
          to.addFront(it);
        }
      }
    };

    template <typename From, typename To>
    struct PathCopySelectorBackward<From, To, true> {
      static void copy(const From& from, To& to) {
        to.clear();
        to.buildRev(from);
      }
    };


    template <typename From, typename To,
              bool revEnable = RevPathTagIndicator<From>::value>
    struct PathCopySelector {
      static void copy(const From& from, To& to) {
        PathCopySelectorForward<From, To>::copy(from, to);
      }
    };

    template <typename From, typename To>
    struct PathCopySelector<From, To, true> {
      static void copy(const From& from, To& to) {
        PathCopySelectorBackward<From, To>::copy(from, to);
      }
    };

  }


  /// \brief Make a copy of a path.
  ///
  /// This function makes a copy of a path.
  template <typename From, typename To>
  void pathCopy(const From& from, To& to) {
    checkConcept<concepts::PathDumper<typename From::Digraph>, From>();
    _path_bits::PathCopySelector<From, To>::copy(from, to);
  }

  /// \brief Deprecated version of \ref pathCopy().
  ///
  /// Deprecated version of \ref pathCopy() (only for reverse compatibility).
  template <typename To, typename From>
  void copyPath(To& to, const From& from) {
    pathCopy(from, to);
  }

  /// \brief Check the consistency of a path.
  ///
  /// This function checks that the target of each arc is the same
  /// as the source of the next one.
  ///
  template <typename Digraph, typename Path>
  bool checkPath(const Digraph& digraph, const Path& path) {
    typename Path::ArcIt it(path);
    if (it == INVALID) return true;
    typename Digraph::Node node = digraph.target(it);
    ++it;
    while (it != INVALID) {
      if (digraph.source(it) != node) return false;
      node = digraph.target(it);
      ++it;
    }
    return true;
  }

  /// \brief The source of a path
  ///
  /// This function returns the source node of the given path.
  /// If the path is empty, then it returns \c INVALID.
  template <typename Digraph, typename Path>
  typename Digraph::Node pathSource(const Digraph& digraph, const Path& path) {
    return path.empty() ? INVALID : digraph.source(path.front());
  }

  /// \brief The target of a path
  ///
  /// This function returns the target node of the given path.
  /// If the path is empty, then it returns \c INVALID.
  template <typename Digraph, typename Path>
  typename Digraph::Node pathTarget(const Digraph& digraph, const Path& path) {
    return path.empty() ? INVALID : digraph.target(path.back());
  }

  /// \brief Class which helps to iterate through the nodes of a path
  ///
  /// In a sense, the path can be treated as a list of arcs. The
  /// LEMON path type stores only this list. As a consequence, it
  /// cannot enumerate the nodes in the path and the zero length paths
  /// cannot have a source node.
  ///
  /// This class implements the node iterator of a path structure. To
  /// provide this feature, the underlying digraph should be passed to
  /// the constructor of the iterator.
  template <typename Path>
  class PathNodeIt {
  private:
    const typename Path::Digraph *_digraph;
    typename Path::ArcIt _it;
    typename Path::Digraph::Node _nd;

  public:

    typedef typename Path::Digraph Digraph;
    typedef typename Digraph::Node Node;

    /// Default constructor
    PathNodeIt() {}
    /// Invalid constructor
    PathNodeIt(Invalid)
      : _digraph(0), _it(INVALID), _nd(INVALID) {}
    /// Constructor
    PathNodeIt(const Digraph& digraph, const Path& path)
      : _digraph(&digraph), _it(path) {
      _nd = (_it != INVALID ? _digraph->source(_it) : INVALID);
    }
    /// Constructor
    PathNodeIt(const Digraph& digraph, const Path& path, const Node& src)
      : _digraph(&digraph), _it(path), _nd(src) {}

    ///Conversion to Digraph::Node
    operator Node() const {
      return _nd;
    }

    /// Next node
    PathNodeIt& operator++() {
      if (_it == INVALID) _nd = INVALID;
      else {
        _nd = _digraph->target(_it);
        ++_it;
      }
      return *this;
    }

    /// Comparison operator
    bool operator==(const PathNodeIt& n) const {
      return _it == n._it && _nd == n._nd;
    }
    /// Comparison operator
    bool operator!=(const PathNodeIt& n) const {
      return _it != n._it || _nd != n._nd;
    }
    /// Comparison operator
    bool operator<(const PathNodeIt& n) const {
      return (_it < n._it && _nd != INVALID);
    }

  };

  ///@}

} // namespace lemon

#endif // LEMON_PATH_H
