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

#ifndef LEMON_UNION_FIND_H
#define LEMON_UNION_FIND_H

//!\ingroup auxdat
//!\file
//!\brief Union-Find data structures.
//!

#include <vector>
#include <list>
#include <utility>
#include <algorithm>
#include <functional>

#include <lemon/core.h>

namespace lemon {

  /// \ingroup auxdat
  ///
  /// \brief A \e Union-Find data structure implementation
  ///
  /// The class implements the \e Union-Find data structure.
  /// The union operation uses rank heuristic, while
  /// the find operation uses path compression.
  /// This is a very simple but efficient implementation, providing
  /// only four methods: join (union), find, insert and size.
  /// For more features, see the \ref UnionFindEnum class.
  ///
  /// It is primarily used in Kruskal algorithm for finding minimal
  /// cost spanning tree in a graph.
  /// \sa kruskal()
  ///
  /// \pre You need to add all the elements by the \ref insert()
  /// method.
  template <typename IM>
  class UnionFind {
  public:

    ///\e
    typedef IM ItemIntMap;
    ///\e
    typedef typename ItemIntMap::Key Item;

  private:
    // If the items vector stores negative value for an item then
    // that item is root item and it has -items[it] component size.
    // Else the items[it] contains the index of the parent.
    std::vector<int> items;
    ItemIntMap& index;

    bool rep(int idx) const {
      return items[idx] < 0;
    }

    int repIndex(int idx) const {
      int k = idx;
      while (!rep(k)) {
        k = items[k] ;
      }
      while (idx != k) {
        int next = items[idx];
        const_cast<int&>(items[idx]) = k;
        idx = next;
      }
      return k;
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor of the UnionFind class. You should give an item to
    /// integer map which will be used from the data structure. If you
    /// modify directly this map that may cause segmentation fault,
    /// invalid data structure, or infinite loop when you use again
    /// the union-find.
    UnionFind(ItemIntMap& m) : index(m) {}

    /// \brief Returns the index of the element's component.
    ///
    /// The method returns the index of the element's component.
    /// This is an integer between zero and the number of inserted elements.
    ///
    int find(const Item& a) {
      return repIndex(index[a]);
    }

    /// \brief Clears the union-find data structure
    ///
    /// Erase each item from the data structure.
    void clear() {
      items.clear();
    }

    /// \brief Inserts a new element into the structure.
    ///
    /// This method inserts a new element into the data structure.
    ///
    /// The method returns the index of the new component.
    int insert(const Item& a) {
      int n = items.size();
      items.push_back(-1);
      index.set(a,n);
      return n;
    }

    /// \brief Joining the components of element \e a and element \e b.
    ///
    /// This is the \e union operation of the Union-Find structure.
    /// Joins the component of element \e a and component of
    /// element \e b. If \e a and \e b are in the same component then
    /// it returns false otherwise it returns true.
    bool join(const Item& a, const Item& b) {
      int ka = repIndex(index[a]);
      int kb = repIndex(index[b]);

      if ( ka == kb )
        return false;

      if (items[ka] < items[kb]) {
        items[ka] += items[kb];
        items[kb] = ka;
      } else {
        items[kb] += items[ka];
        items[ka] = kb;
      }
      return true;
    }

    /// \brief Returns the size of the component of element \e a.
    ///
    /// Returns the size of the component of element \e a.
    int size(const Item& a) {
      int k = repIndex(index[a]);
      return - items[k];
    }

  };

  /// \ingroup auxdat
  ///
  /// \brief A \e Union-Find data structure implementation which
  /// is able to enumerate the components.
  ///
  /// The class implements a \e Union-Find data structure
  /// which is able to enumerate the components and the items in
  /// a component. If you don't need this feature then perhaps it's
  /// better to use the \ref UnionFind class which is more efficient.
  ///
  /// The union operation uses rank heuristic, while
  /// the find operation uses path compression.
  ///
  /// \pre You need to add all the elements by the \ref insert()
  /// method.
  ///
  template <typename IM>
  class UnionFindEnum {
  public:

    ///\e
    typedef IM ItemIntMap;
    ///\e
    typedef typename ItemIntMap::Key Item;

  private:

    ItemIntMap& index;

    // If the parent stores negative value for an item then that item
    // is root item and it has ~(items[it].parent) component id.  Else
    // the items[it].parent contains the index of the parent.
    //
    // The \c next and \c prev provides the double-linked
    // cyclic list of one component's items.
    struct ItemT {
      int parent;
      Item item;

      int next, prev;
    };

    std::vector<ItemT> items;
    int firstFreeItem;

    struct ClassT {
      int size;
      int firstItem;
      int next, prev;
    };

    std::vector<ClassT> classes;
    int firstClass, firstFreeClass;

    int newClass() {
      if (firstFreeClass == -1) {
        int cdx = classes.size();
        classes.push_back(ClassT());
        return cdx;
      } else {
        int cdx = firstFreeClass;
        firstFreeClass = classes[firstFreeClass].next;
        return cdx;
      }
    }

    int newItem() {
      if (firstFreeItem == -1) {
        int idx = items.size();
        items.push_back(ItemT());
        return idx;
      } else {
        int idx = firstFreeItem;
        firstFreeItem = items[firstFreeItem].next;
        return idx;
      }
    }


    bool rep(int idx) const {
      return items[idx].parent < 0;
    }

    int repIndex(int idx) const {
      int k = idx;
      while (!rep(k)) {
        k = items[k].parent;
      }
      while (idx != k) {
        int next = items[idx].parent;
        const_cast<int&>(items[idx].parent) = k;
        idx = next;
      }
      return k;
    }

    int classIndex(int idx) const {
      return ~(items[repIndex(idx)].parent);
    }

    void singletonItem(int idx) {
      items[idx].next = idx;
      items[idx].prev = idx;
    }

    void laceItem(int idx, int rdx) {
      items[idx].prev = rdx;
      items[idx].next = items[rdx].next;
      items[items[rdx].next].prev = idx;
      items[rdx].next = idx;
    }

    void unlaceItem(int idx) {
      items[items[idx].prev].next = items[idx].next;
      items[items[idx].next].prev = items[idx].prev;

      items[idx].next = firstFreeItem;
      firstFreeItem = idx;
    }

    void spliceItems(int ak, int bk) {
      items[items[ak].prev].next = bk;
      items[items[bk].prev].next = ak;
      int tmp = items[ak].prev;
      items[ak].prev = items[bk].prev;
      items[bk].prev = tmp;

    }

    void laceClass(int cls) {
      if (firstClass != -1) {
        classes[firstClass].prev = cls;
      }
      classes[cls].next = firstClass;
      classes[cls].prev = -1;
      firstClass = cls;
    }

    void unlaceClass(int cls) {
      if (classes[cls].prev != -1) {
        classes[classes[cls].prev].next = classes[cls].next;
      } else {
        firstClass = classes[cls].next;
      }
      if (classes[cls].next != -1) {
        classes[classes[cls].next].prev = classes[cls].prev;
      }

      classes[cls].next = firstFreeClass;
      firstFreeClass = cls;
    }

  public:

    UnionFindEnum(ItemIntMap& _index)
      : index(_index), items(), firstFreeItem(-1),
        firstClass(-1), firstFreeClass(-1) {}

    /// \brief Inserts the given element into a new component.
    ///
    /// This method creates a new component consisting only of the
    /// given element.
    ///
    int insert(const Item& item) {
      int idx = newItem();

      index.set(item, idx);

      singletonItem(idx);
      items[idx].item = item;

      int cdx = newClass();

      items[idx].parent = ~cdx;

      laceClass(cdx);
      classes[cdx].size = 1;
      classes[cdx].firstItem = idx;

      firstClass = cdx;

      return cdx;
    }

    /// \brief Inserts the given element into the component of the others.
    ///
    /// This methods inserts the element \e a into the component of the
    /// element \e comp.
    void insert(const Item& item, int cls) {
      int rdx = classes[cls].firstItem;
      int idx = newItem();

      index.set(item, idx);

      laceItem(idx, rdx);

      items[idx].item = item;
      items[idx].parent = rdx;

      ++classes[~(items[rdx].parent)].size;
    }

    /// \brief Clears the union-find data structure
    ///
    /// Erase each item from the data structure.
    void clear() {
      items.clear();
      firstClass = -1;
      firstFreeItem = -1;
    }

    /// \brief Finds the component of the given element.
    ///
    /// The method returns the component id of the given element.
    int find(const Item &item) const {
      return ~(items[repIndex(index[item])].parent);
    }

    /// \brief Joining the component of element \e a and element \e b.
    ///
    /// This is the \e union operation of the Union-Find structure.
    /// Joins the component of element \e a and component of
    /// element \e b. If \e a and \e b are in the same component then
    /// returns -1 else returns the remaining class.
    int join(const Item& a, const Item& b) {

      int ak = repIndex(index[a]);
      int bk = repIndex(index[b]);

      if (ak == bk) {
        return -1;
      }

      int acx = ~(items[ak].parent);
      int bcx = ~(items[bk].parent);

      int rcx;

      if (classes[acx].size > classes[bcx].size) {
        classes[acx].size += classes[bcx].size;
        items[bk].parent = ak;
        unlaceClass(bcx);
        rcx = acx;
      } else {
        classes[bcx].size += classes[acx].size;
        items[ak].parent = bk;
        unlaceClass(acx);
        rcx = bcx;
      }
      spliceItems(ak, bk);

      return rcx;
    }

    /// \brief Returns the size of the class.
    ///
    /// Returns the size of the class.
    int size(int cls) const {
      return classes[cls].size;
    }

    /// \brief Splits up the component.
    ///
    /// Splitting the component into singleton components (component
    /// of size one).
    void split(int cls) {
      int fdx = classes[cls].firstItem;
      int idx = items[fdx].next;
      while (idx != fdx) {
        int next = items[idx].next;

        singletonItem(idx);

        int cdx = newClass();
        items[idx].parent = ~cdx;

        laceClass(cdx);
        classes[cdx].size = 1;
        classes[cdx].firstItem = idx;

        idx = next;
      }

      items[idx].prev = idx;
      items[idx].next = idx;

      classes[~(items[idx].parent)].size = 1;

    }

    /// \brief Removes the given element from the structure.
    ///
    /// Removes the element from its component and if the component becomes
    /// empty then removes that component from the component list.
    ///
    /// \warning It is an error to remove an element which is not in
    /// the structure.
    /// \warning This running time of this operation is proportional to the
    /// number of the items in this class.
    void erase(const Item& item) {
      int idx = index[item];
      int fdx = items[idx].next;

      int cdx = classIndex(idx);
      if (idx == fdx) {
        unlaceClass(cdx);
        items[idx].next = firstFreeItem;
        firstFreeItem = idx;
        return;
      } else {
        classes[cdx].firstItem = fdx;
        --classes[cdx].size;
        items[fdx].parent = ~cdx;

        unlaceItem(idx);
        idx = items[fdx].next;
        while (idx != fdx) {
          items[idx].parent = fdx;
          idx = items[idx].next;
        }

      }

    }

    /// \brief Gives back a representant item of the component.
    ///
    /// Gives back a representant item of the component.
    Item item(int cls) const {
      return items[classes[cls].firstItem].item;
    }

    /// \brief Removes the component of the given element from the structure.
    ///
    /// Removes the component of the given element from the structure.
    ///
    /// \warning It is an error to give an element which is not in the
    /// structure.
    void eraseClass(int cls) {
      int fdx = classes[cls].firstItem;
      unlaceClass(cls);
      items[items[fdx].prev].next = firstFreeItem;
      firstFreeItem = fdx;
    }

    /// \brief LEMON style iterator for the representant items.
    ///
    /// ClassIt is a lemon style iterator for the components. It iterates
    /// on the ids of the classes.
    class ClassIt {
    public:
      /// \brief Constructor of the iterator
      ///
      /// Constructor of the iterator
      ClassIt(const UnionFindEnum& ufe) : unionFind(&ufe) {
        cdx = unionFind->firstClass;
      }

      /// \brief Constructor to get invalid iterator
      ///
      /// Constructor to get invalid iterator
      ClassIt(Invalid) : unionFind(0), cdx(-1) {}

      /// \brief Increment operator
      ///
      /// It steps to the next representant item.
      ClassIt& operator++() {
        cdx = unionFind->classes[cdx].next;
        return *this;
      }

      /// \brief Conversion operator
      ///
      /// It converts the iterator to the current representant item.
      operator int() const {
        return cdx;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ClassIt& i) {
        return i.cdx == cdx;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ClassIt& i) {
        return i.cdx != cdx;
      }

    private:
      const UnionFindEnum* unionFind;
      int cdx;
    };

    /// \brief LEMON style iterator for the items of a component.
    ///
    /// ClassIt is a lemon style iterator for the components. It iterates
    /// on the items of a class. By example if you want to iterate on
    /// each items of each classes then you may write the next code.
    ///\code
    /// for (ClassIt cit(ufe); cit != INVALID; ++cit) {
    ///   std::cout << "Class: ";
    ///   for (ItemIt iit(ufe, cit); iit != INVALID; ++iit) {
    ///     std::cout << toString(iit) << ' ' << std::endl;
    ///   }
    ///   std::cout << std::endl;
    /// }
    ///\endcode
    class ItemIt {
    public:
      /// \brief Constructor of the iterator
      ///
      /// Constructor of the iterator. The iterator iterates
      /// on the class of the \c item.
      ItemIt(const UnionFindEnum& ufe, int cls) : unionFind(&ufe) {
        fdx = idx = unionFind->classes[cls].firstItem;
      }

      /// \brief Constructor to get invalid iterator
      ///
      /// Constructor to get invalid iterator
      ItemIt(Invalid) : unionFind(0), idx(-1) {}

      /// \brief Increment operator
      ///
      /// It steps to the next item in the class.
      ItemIt& operator++() {
        idx = unionFind->items[idx].next;
        if (idx == fdx) idx = -1;
        return *this;
      }

      /// \brief Conversion operator
      ///
      /// It converts the iterator to the current item.
      operator const Item&() const {
        return unionFind->items[idx].item;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ItemIt& i) {
        return i.idx == idx;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ItemIt& i) {
        return i.idx != idx;
      }

    private:
      const UnionFindEnum* unionFind;
      int idx, fdx;
    };

  };

  /// \ingroup auxdat
  ///
  /// \brief A \e Extend-Find data structure implementation which
  /// is able to enumerate the components.
  ///
  /// The class implements an \e Extend-Find data structure which is
  /// able to enumerate the components and the items in a
  /// component. The data structure is a simplification of the
  /// Union-Find structure, and it does not allow to merge two components.
  ///
  /// \pre You need to add all the elements by the \ref insert()
  /// method.
  template <typename IM>
  class ExtendFindEnum {
  public:

    ///\e
    typedef IM ItemIntMap;
    ///\e
    typedef typename ItemIntMap::Key Item;

  private:

    ItemIntMap& index;

    struct ItemT {
      int cls;
      Item item;
      int next, prev;
    };

    std::vector<ItemT> items;
    int firstFreeItem;

    struct ClassT {
      int firstItem;
      int next, prev;
    };

    std::vector<ClassT> classes;

    int firstClass, firstFreeClass;

    int newClass() {
      if (firstFreeClass != -1) {
        int cdx = firstFreeClass;
        firstFreeClass = classes[cdx].next;
        return cdx;
      } else {
        classes.push_back(ClassT());
        return classes.size() - 1;
      }
    }

    int newItem() {
      if (firstFreeItem != -1) {
        int idx = firstFreeItem;
        firstFreeItem = items[idx].next;
        return idx;
      } else {
        items.push_back(ItemT());
        return items.size() - 1;
      }
    }

  public:

    /// \brief Constructor
    ExtendFindEnum(ItemIntMap& _index)
      : index(_index), items(), firstFreeItem(-1),
        classes(), firstClass(-1), firstFreeClass(-1) {}

    /// \brief Inserts the given element into a new component.
    ///
    /// This method creates a new component consisting only of the
    /// given element.
    int insert(const Item& item) {
      int cdx = newClass();
      classes[cdx].prev = -1;
      classes[cdx].next = firstClass;
      if (firstClass != -1) {
        classes[firstClass].prev = cdx;
      }
      firstClass = cdx;

      int idx = newItem();
      items[idx].item = item;
      items[idx].cls = cdx;
      items[idx].prev = idx;
      items[idx].next = idx;

      classes[cdx].firstItem = idx;

      index.set(item, idx);

      return cdx;
    }

    /// \brief Inserts the given element into the given component.
    ///
    /// This methods inserts the element \e item a into the \e cls class.
    void insert(const Item& item, int cls) {
      int idx = newItem();
      int rdx = classes[cls].firstItem;
      items[idx].item = item;
      items[idx].cls = cls;

      items[idx].prev = rdx;
      items[idx].next = items[rdx].next;
      items[items[rdx].next].prev = idx;
      items[rdx].next = idx;

      index.set(item, idx);
    }

    /// \brief Clears the union-find data structure
    ///
    /// Erase each item from the data structure.
    void clear() {
      items.clear();
      classes.clear();
      firstClass = firstFreeClass = firstFreeItem = -1;
    }

    /// \brief Gives back the class of the \e item.
    ///
    /// Gives back the class of the \e item.
    int find(const Item &item) const {
      return items[index[item]].cls;
    }

    /// \brief Gives back a representant item of the component.
    ///
    /// Gives back a representant item of the component.
    Item item(int cls) const {
      return items[classes[cls].firstItem].item;
    }

    /// \brief Removes the given element from the structure.
    ///
    /// Removes the element from its component and if the component becomes
    /// empty then removes that component from the component list.
    ///
    /// \warning It is an error to remove an element which is not in
    /// the structure.
    void erase(const Item &item) {
      int idx = index[item];
      int cdx = items[idx].cls;

      if (idx == items[idx].next) {
        if (classes[cdx].prev != -1) {
          classes[classes[cdx].prev].next = classes[cdx].next;
        } else {
          firstClass = classes[cdx].next;
        }
        if (classes[cdx].next != -1) {
          classes[classes[cdx].next].prev = classes[cdx].prev;
        }
        classes[cdx].next = firstFreeClass;
        firstFreeClass = cdx;
      } else {
        classes[cdx].firstItem = items[idx].next;
        items[items[idx].next].prev = items[idx].prev;
        items[items[idx].prev].next = items[idx].next;
      }
      items[idx].next = firstFreeItem;
      firstFreeItem = idx;

    }


    /// \brief Removes the component of the given element from the structure.
    ///
    /// Removes the component of the given element from the structure.
    ///
    /// \warning It is an error to give an element which is not in the
    /// structure.
    void eraseClass(int cdx) {
      int idx = classes[cdx].firstItem;
      items[items[idx].prev].next = firstFreeItem;
      firstFreeItem = idx;

      if (classes[cdx].prev != -1) {
        classes[classes[cdx].prev].next = classes[cdx].next;
      } else {
        firstClass = classes[cdx].next;
      }
      if (classes[cdx].next != -1) {
        classes[classes[cdx].next].prev = classes[cdx].prev;
      }
      classes[cdx].next = firstFreeClass;
      firstFreeClass = cdx;
    }

    /// \brief LEMON style iterator for the classes.
    ///
    /// ClassIt is a lemon style iterator for the components. It iterates
    /// on the ids of classes.
    class ClassIt {
    public:
      /// \brief Constructor of the iterator
      ///
      /// Constructor of the iterator
      ClassIt(const ExtendFindEnum& ufe) : extendFind(&ufe) {
        cdx = extendFind->firstClass;
      }

      /// \brief Constructor to get invalid iterator
      ///
      /// Constructor to get invalid iterator
      ClassIt(Invalid) : extendFind(0), cdx(-1) {}

      /// \brief Increment operator
      ///
      /// It steps to the next representant item.
      ClassIt& operator++() {
        cdx = extendFind->classes[cdx].next;
        return *this;
      }

      /// \brief Conversion operator
      ///
      /// It converts the iterator to the current class id.
      operator int() const {
        return cdx;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ClassIt& i) {
        return i.cdx == cdx;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ClassIt& i) {
        return i.cdx != cdx;
      }

    private:
      const ExtendFindEnum* extendFind;
      int cdx;
    };

    /// \brief LEMON style iterator for the items of a component.
    ///
    /// ClassIt is a lemon style iterator for the components. It iterates
    /// on the items of a class. By example if you want to iterate on
    /// each items of each classes then you may write the next code.
    ///\code
    /// for (ClassIt cit(ufe); cit != INVALID; ++cit) {
    ///   std::cout << "Class: ";
    ///   for (ItemIt iit(ufe, cit); iit != INVALID; ++iit) {
    ///     std::cout << toString(iit) << ' ' << std::endl;
    ///   }
    ///   std::cout << std::endl;
    /// }
    ///\endcode
    class ItemIt {
    public:
      /// \brief Constructor of the iterator
      ///
      /// Constructor of the iterator. The iterator iterates
      /// on the class of the \c item.
      ItemIt(const ExtendFindEnum& ufe, int cls) : extendFind(&ufe) {
        fdx = idx = extendFind->classes[cls].firstItem;
      }

      /// \brief Constructor to get invalid iterator
      ///
      /// Constructor to get invalid iterator
      ItemIt(Invalid) : extendFind(0), idx(-1) {}

      /// \brief Increment operator
      ///
      /// It steps to the next item in the class.
      ItemIt& operator++() {
        idx = extendFind->items[idx].next;
        if (fdx == idx) idx = -1;
        return *this;
      }

      /// \brief Conversion operator
      ///
      /// It converts the iterator to the current item.
      operator const Item&() const {
        return extendFind->items[idx].item;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ItemIt& i) {
        return i.idx == idx;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ItemIt& i) {
        return i.idx != idx;
      }

    private:
      const ExtendFindEnum* extendFind;
      int idx, fdx;
    };

  };

  /// \ingroup auxdat
  ///
  /// \brief A \e Union-Find data structure implementation which
  /// is able to store a priority for each item and retrieve the minimum of
  /// each class.
  ///
  /// A \e Union-Find data structure implementation which is able to
  /// store a priority for each item and retrieve the minimum of each
  /// class. In addition, it supports the joining and splitting the
  /// components. If you don't need this feature then you makes
  /// better to use the \ref UnionFind class which is more efficient.
  ///
  /// The union-find data strcuture based on a (2, 16)-tree with a
  /// tournament minimum selection on the internal nodes. The insert
  /// operation takes O(1), the find, set, decrease and increase takes
  /// O(log(n)), where n is the number of nodes in the current
  /// component.  The complexity of join and split is O(log(n)*k),
  /// where n is the sum of the number of the nodes and k is the
  /// number of joined components or the number of the components
  /// after the split.
  ///
  /// \pre You need to add all the elements by the \ref insert()
  /// method.
  template <typename V, typename IM, typename Comp = std::less<V> >
  class HeapUnionFind {
  public:

    ///\e
    typedef V Value;
    ///\e
    typedef typename IM::Key Item;
    ///\e
    typedef IM ItemIntMap;
    ///\e
    typedef Comp Compare;

  private:

    static const int cmax = 16;

    ItemIntMap& index;

    struct ClassNode {
      int parent;
      int depth;

      int left, right;
      int next, prev;
    };

    int first_class;
    int first_free_class;
    std::vector<ClassNode> classes;

    int newClass() {
      if (first_free_class < 0) {
        int id = classes.size();
        classes.push_back(ClassNode());
        return id;
      } else {
        int id = first_free_class;
        first_free_class = classes[id].next;
        return id;
      }
    }

    void deleteClass(int id) {
      classes[id].next = first_free_class;
      first_free_class = id;
    }

    struct ItemNode {
      int parent;
      Item item;
      Value prio;
      int next, prev;
      int left, right;
      int size;
    };

    int first_free_node;
    std::vector<ItemNode> nodes;

    int newNode() {
      if (first_free_node < 0) {
        int id = nodes.size();
        nodes.push_back(ItemNode());
        return id;
      } else {
        int id = first_free_node;
        first_free_node = nodes[id].next;
        return id;
      }
    }

    void deleteNode(int id) {
      nodes[id].next = first_free_node;
      first_free_node = id;
    }

    Comp comp;

    int findClass(int id) const {
      int kd = id;
      while (kd >= 0) {
        kd = nodes[kd].parent;
      }
      return ~kd;
    }

    int leftNode(int id) const {
      int kd = ~(classes[id].parent);
      for (int i = 0; i < classes[id].depth; ++i) {
        kd = nodes[kd].left;
      }
      return kd;
    }

    int nextNode(int id) const {
      int depth = 0;
      while (id >= 0 && nodes[id].next == -1) {
        id = nodes[id].parent;
        ++depth;
      }
      if (id < 0) {
        return -1;
      }
      id = nodes[id].next;
      while (depth--) {
        id = nodes[id].left;
      }
      return id;
    }


    void setPrio(int id) {
      int jd = nodes[id].left;
      nodes[id].prio = nodes[jd].prio;
      nodes[id].item = nodes[jd].item;
      jd = nodes[jd].next;
      while (jd != -1) {
        if (comp(nodes[jd].prio, nodes[id].prio)) {
          nodes[id].prio = nodes[jd].prio;
          nodes[id].item = nodes[jd].item;
        }
        jd = nodes[jd].next;
      }
    }

    void push(int id, int jd) {
      nodes[id].size = 1;
      nodes[id].left = nodes[id].right = jd;
      nodes[jd].next = nodes[jd].prev = -1;
      nodes[jd].parent = id;
    }

    void pushAfter(int id, int jd) {
      int kd = nodes[id].parent;
      if (nodes[id].next != -1) {
        nodes[nodes[id].next].prev = jd;
        if (kd >= 0) {
          nodes[kd].size += 1;
        }
      } else {
        if (kd >= 0) {
          nodes[kd].right = jd;
          nodes[kd].size += 1;
        }
      }
      nodes[jd].next = nodes[id].next;
      nodes[jd].prev = id;
      nodes[id].next = jd;
      nodes[jd].parent = kd;
    }

    void pushRight(int id, int jd) {
      nodes[id].size += 1;
      nodes[jd].prev = nodes[id].right;
      nodes[jd].next = -1;
      nodes[nodes[id].right].next = jd;
      nodes[id].right = jd;
      nodes[jd].parent = id;
    }

    void popRight(int id) {
      nodes[id].size -= 1;
      int jd = nodes[id].right;
      nodes[nodes[jd].prev].next = -1;
      nodes[id].right = nodes[jd].prev;
    }

    void splice(int id, int jd) {
      nodes[id].size += nodes[jd].size;
      nodes[nodes[id].right].next = nodes[jd].left;
      nodes[nodes[jd].left].prev = nodes[id].right;
      int kd = nodes[jd].left;
      while (kd != -1) {
        nodes[kd].parent = id;
        kd = nodes[kd].next;
      }
      nodes[id].right = nodes[jd].right;
    }

    void split(int id, int jd) {
      int kd = nodes[id].parent;
      nodes[kd].right = nodes[id].prev;
      nodes[nodes[id].prev].next = -1;

      nodes[jd].left = id;
      nodes[id].prev = -1;
      int num = 0;
      while (id != -1) {
        nodes[id].parent = jd;
        nodes[jd].right = id;
        id = nodes[id].next;
        ++num;
      }
      nodes[kd].size -= num;
      nodes[jd].size = num;
    }

    void pushLeft(int id, int jd) {
      nodes[id].size += 1;
      nodes[jd].next = nodes[id].left;
      nodes[jd].prev = -1;
      nodes[nodes[id].left].prev = jd;
      nodes[id].left = jd;
      nodes[jd].parent = id;
    }

    void popLeft(int id) {
      nodes[id].size -= 1;
      int jd = nodes[id].left;
      nodes[nodes[jd].next].prev = -1;
      nodes[id].left = nodes[jd].next;
    }

    void repairLeft(int id) {
      int jd = ~(classes[id].parent);
      while (nodes[jd].left != -1) {
        int kd = nodes[jd].left;
        if (nodes[jd].size == 1) {
          if (nodes[jd].parent < 0) {
            classes[id].parent = ~kd;
            classes[id].depth -= 1;
            nodes[kd].parent = ~id;
            deleteNode(jd);
            jd = kd;
          } else {
            int pd = nodes[jd].parent;
            if (nodes[nodes[jd].next].size < cmax) {
              pushLeft(nodes[jd].next, nodes[jd].left);
              if (less(jd, nodes[jd].next) ||
                  nodes[jd].item == nodes[pd].item) {
                nodes[nodes[jd].next].prio = nodes[jd].prio;
                nodes[nodes[jd].next].item = nodes[jd].item;
              }
              popLeft(pd);
              deleteNode(jd);
              jd = pd;
            } else {
              int ld = nodes[nodes[jd].next].left;
              popLeft(nodes[jd].next);
              pushRight(jd, ld);
              if (less(ld, nodes[jd].left) ||
                  nodes[ld].item == nodes[pd].item) {
                nodes[jd].item = nodes[ld].item;
                nodes[jd].prio = nodes[ld].prio;
              }
              if (nodes[nodes[jd].next].item == nodes[ld].item) {
                setPrio(nodes[jd].next);
              }
              jd = nodes[jd].left;
            }
          }
        } else {
          jd = nodes[jd].left;
        }
      }
    }

    void repairRight(int id) {
      int jd = ~(classes[id].parent);
      while (nodes[jd].right != -1) {
        int kd = nodes[jd].right;
        if (nodes[jd].size == 1) {
          if (nodes[jd].parent < 0) {
            classes[id].parent = ~kd;
            classes[id].depth -= 1;
            nodes[kd].parent = ~id;
            deleteNode(jd);
            jd = kd;
          } else {
            int pd = nodes[jd].parent;
            if (nodes[nodes[jd].prev].size < cmax) {
              pushRight(nodes[jd].prev, nodes[jd].right);
              if (less(jd, nodes[jd].prev) ||
                  nodes[jd].item == nodes[pd].item) {
                nodes[nodes[jd].prev].prio = nodes[jd].prio;
                nodes[nodes[jd].prev].item = nodes[jd].item;
              }
              popRight(pd);
              deleteNode(jd);
              jd = pd;
            } else {
              int ld = nodes[nodes[jd].prev].right;
              popRight(nodes[jd].prev);
              pushLeft(jd, ld);
              if (less(ld, nodes[jd].right) ||
                  nodes[ld].item == nodes[pd].item) {
                nodes[jd].item = nodes[ld].item;
                nodes[jd].prio = nodes[ld].prio;
              }
              if (nodes[nodes[jd].prev].item == nodes[ld].item) {
                setPrio(nodes[jd].prev);
              }
              jd = nodes[jd].right;
            }
          }
        } else {
          jd = nodes[jd].right;
        }
      }
    }


    bool less(int id, int jd) const {
      return comp(nodes[id].prio, nodes[jd].prio);
    }

  public:

    /// \brief Returns true when the given class is alive.
    ///
    /// Returns true when the given class is alive, ie. the class is
    /// not nested into other class.
    bool alive(int cls) const {
      return classes[cls].parent < 0;
    }

    /// \brief Returns true when the given class is trivial.
    ///
    /// Returns true when the given class is trivial, ie. the class
    /// contains just one item directly.
    bool trivial(int cls) const {
      return classes[cls].left == -1;
    }

    /// \brief Constructs the union-find.
    ///
    /// Constructs the union-find.
    /// \brief _index The index map of the union-find. The data
    /// structure uses internally for store references.
    HeapUnionFind(ItemIntMap& _index)
      : index(_index), first_class(-1),
        first_free_class(-1), first_free_node(-1) {}

    /// \brief Clears the union-find data structure
    ///
    /// Erase each item from the data structure.
    void clear() {
      nodes.clear();
      classes.clear();
      first_free_node = first_free_class = first_class = -1;
    }

    /// \brief Insert a new node into a new component.
    ///
    /// Insert a new node into a new component.
    /// \param item The item of the new node.
    /// \param prio The priority of the new node.
    /// \return The class id of the one-item-heap.
    int insert(const Item& item, const Value& prio) {
      int id = newNode();
      nodes[id].item = item;
      nodes[id].prio = prio;
      nodes[id].size = 0;

      nodes[id].prev = -1;
      nodes[id].next = -1;

      nodes[id].left = -1;
      nodes[id].right = -1;

      nodes[id].item = item;
      index[item] = id;

      int class_id = newClass();
      classes[class_id].parent = ~id;
      classes[class_id].depth = 0;

      classes[class_id].left = -1;
      classes[class_id].right = -1;

      if (first_class != -1) {
        classes[first_class].prev = class_id;
      }
      classes[class_id].next = first_class;
      classes[class_id].prev = -1;
      first_class = class_id;

      nodes[id].parent = ~class_id;

      return class_id;
    }

    /// \brief The class of the item.
    ///
    /// \return The alive class id of the item, which is not nested into
    /// other classes.
    ///
    /// The time complexity is O(log(n)).
    int find(const Item& item) const {
      return findClass(index[item]);
    }

    /// \brief Joins the classes.
    ///
    /// The current function joins the given classes. The parameter is
    /// an STL range which should be contains valid class ids. The
    /// time complexity is O(log(n)*k) where n is the overall number
    /// of the joined nodes and k is the number of classes.
    /// \return The class of the joined classes.
    /// \pre The range should contain at least two class ids.
    template <typename Iterator>
    int join(Iterator begin, Iterator end) {
      std::vector<int> cs;
      for (Iterator it = begin; it != end; ++it) {
        cs.push_back(*it);
      }

      int class_id = newClass();
      { // creation union-find

        if (first_class != -1) {
          classes[first_class].prev = class_id;
        }
        classes[class_id].next = first_class;
        classes[class_id].prev = -1;
        first_class = class_id;

        classes[class_id].depth = classes[cs[0]].depth;
        classes[class_id].parent = classes[cs[0]].parent;
        nodes[~(classes[class_id].parent)].parent = ~class_id;

        int l = cs[0];

        classes[class_id].left = l;
        classes[class_id].right = l;

        if (classes[l].next != -1) {
          classes[classes[l].next].prev = classes[l].prev;
        }
        classes[classes[l].prev].next = classes[l].next;

        classes[l].prev = -1;
        classes[l].next = -1;

        classes[l].depth = leftNode(l);
        classes[l].parent = class_id;

      }

      { // merging of heap
        int l = class_id;
        for (int ci = 1; ci < int(cs.size()); ++ci) {
          int r = cs[ci];
          int rln = leftNode(r);
          if (classes[l].depth > classes[r].depth) {
            int id = ~(classes[l].parent);
            for (int i = classes[r].depth + 1; i < classes[l].depth; ++i) {
              id = nodes[id].right;
            }
            while (id >= 0 && nodes[id].size == cmax) {
              int new_id = newNode();
              int right_id = nodes[id].right;

              popRight(id);
              if (nodes[id].item == nodes[right_id].item) {
                setPrio(id);
              }
              push(new_id, right_id);
              pushRight(new_id, ~(classes[r].parent));

              if (less(~classes[r].parent, right_id)) {
                nodes[new_id].item = nodes[~classes[r].parent].item;
                nodes[new_id].prio = nodes[~classes[r].parent].prio;
              } else {
                nodes[new_id].item = nodes[right_id].item;
                nodes[new_id].prio = nodes[right_id].prio;
              }

              id = nodes[id].parent;
              classes[r].parent = ~new_id;
            }
            if (id < 0) {
              int new_parent = newNode();
              nodes[new_parent].next = -1;
              nodes[new_parent].prev = -1;
              nodes[new_parent].parent = ~l;

              push(new_parent, ~(classes[l].parent));
              pushRight(new_parent, ~(classes[r].parent));
              setPrio(new_parent);

              classes[l].parent = ~new_parent;
              classes[l].depth += 1;
            } else {
              pushRight(id, ~(classes[r].parent));
              while (id >= 0 && less(~(classes[r].parent), id)) {
                nodes[id].prio = nodes[~(classes[r].parent)].prio;
                nodes[id].item = nodes[~(classes[r].parent)].item;
                id = nodes[id].parent;
              }
            }
          } else if (classes[r].depth > classes[l].depth) {
            int id = ~(classes[r].parent);
            for (int i = classes[l].depth + 1; i < classes[r].depth; ++i) {
              id = nodes[id].left;
            }
            while (id >= 0 && nodes[id].size == cmax) {
              int new_id = newNode();
              int left_id = nodes[id].left;

              popLeft(id);
              if (nodes[id].prio == nodes[left_id].prio) {
                setPrio(id);
              }
              push(new_id, left_id);
              pushLeft(new_id, ~(classes[l].parent));

              if (less(~classes[l].parent, left_id)) {
                nodes[new_id].item = nodes[~classes[l].parent].item;
                nodes[new_id].prio = nodes[~classes[l].parent].prio;
              } else {
                nodes[new_id].item = nodes[left_id].item;
                nodes[new_id].prio = nodes[left_id].prio;
              }

              id = nodes[id].parent;
              classes[l].parent = ~new_id;

            }
            if (id < 0) {
              int new_parent = newNode();
              nodes[new_parent].next = -1;
              nodes[new_parent].prev = -1;
              nodes[new_parent].parent = ~l;

              push(new_parent, ~(classes[r].parent));
              pushLeft(new_parent, ~(classes[l].parent));
              setPrio(new_parent);

              classes[r].parent = ~new_parent;
              classes[r].depth += 1;
            } else {
              pushLeft(id, ~(classes[l].parent));
              while (id >= 0 && less(~(classes[l].parent), id)) {
                nodes[id].prio = nodes[~(classes[l].parent)].prio;
                nodes[id].item = nodes[~(classes[l].parent)].item;
                id = nodes[id].parent;
              }
            }
            nodes[~(classes[r].parent)].parent = ~l;
            classes[l].parent = classes[r].parent;
            classes[l].depth = classes[r].depth;
          } else {
            if (classes[l].depth != 0 &&
                nodes[~(classes[l].parent)].size +
                nodes[~(classes[r].parent)].size <= cmax) {
              splice(~(classes[l].parent), ~(classes[r].parent));
              deleteNode(~(classes[r].parent));
              if (less(~(classes[r].parent), ~(classes[l].parent))) {
                nodes[~(classes[l].parent)].prio =
                  nodes[~(classes[r].parent)].prio;
                nodes[~(classes[l].parent)].item =
                  nodes[~(classes[r].parent)].item;
              }
            } else {
              int new_parent = newNode();
              nodes[new_parent].next = nodes[new_parent].prev = -1;
              push(new_parent, ~(classes[l].parent));
              pushRight(new_parent, ~(classes[r].parent));
              setPrio(new_parent);

              classes[l].parent = ~new_parent;
              classes[l].depth += 1;
              nodes[new_parent].parent = ~l;
            }
          }
          if (classes[r].next != -1) {
            classes[classes[r].next].prev = classes[r].prev;
          }
          classes[classes[r].prev].next = classes[r].next;

          classes[r].prev = classes[l].right;
          classes[classes[l].right].next = r;
          classes[l].right = r;
          classes[r].parent = l;

          classes[r].next = -1;
          classes[r].depth = rln;
        }
      }
      return class_id;
    }

    /// \brief Split the class to subclasses.
    ///
    /// The current function splits the given class. The join, which
    /// made the current class, stored a reference to the
    /// subclasses. The \c splitClass() member restores the classes
    /// and creates the heaps. The parameter is an STL output iterator
    /// which will be filled with the subclass ids. The time
    /// complexity is O(log(n)*k) where n is the overall number of
    /// nodes in the splitted classes and k is the number of the
    /// classes.
    template <typename Iterator>
    void split(int cls, Iterator out) {
      std::vector<int> cs;
      { // splitting union-find
        int id = cls;
        int l = classes[id].left;

        classes[l].parent = classes[id].parent;
        classes[l].depth = classes[id].depth;

        nodes[~(classes[l].parent)].parent = ~l;

        *out++ = l;

        while (l != -1) {
          cs.push_back(l);
          l = classes[l].next;
        }

        classes[classes[id].right].next = first_class;
        classes[first_class].prev = classes[id].right;
        first_class = classes[id].left;

        if (classes[id].next != -1) {
          classes[classes[id].next].prev = classes[id].prev;
        }
        classes[classes[id].prev].next = classes[id].next;

        deleteClass(id);
      }

      {
        for (int i = 1; i < int(cs.size()); ++i) {
          int l = classes[cs[i]].depth;
          while (nodes[nodes[l].parent].left == l) {
            l = nodes[l].parent;
          }
          int r = l;
          while (nodes[l].parent >= 0) {
            l = nodes[l].parent;
            int new_node = newNode();

            nodes[new_node].prev = -1;
            nodes[new_node].next = -1;

            split(r, new_node);
            pushAfter(l, new_node);
            setPrio(l);
            setPrio(new_node);
            r = new_node;
          }
          classes[cs[i]].parent = ~r;
          classes[cs[i]].depth = classes[~(nodes[l].parent)].depth;
          nodes[r].parent = ~cs[i];

          nodes[l].next = -1;
          nodes[r].prev = -1;

          repairRight(~(nodes[l].parent));
          repairLeft(cs[i]);

          *out++ = cs[i];
        }
      }
    }

    /// \brief Gives back the priority of the current item.
    ///
    /// Gives back the priority of the current item.
    const Value& operator[](const Item& item) const {
      return nodes[index[item]].prio;
    }

    /// \brief Sets the priority of the current item.
    ///
    /// Sets the priority of the current item.
    void set(const Item& item, const Value& prio) {
      if (comp(prio, nodes[index[item]].prio)) {
        decrease(item, prio);
      } else if (!comp(prio, nodes[index[item]].prio)) {
        increase(item, prio);
      }
    }

    /// \brief Increase the priority of the current item.
    ///
    /// Increase the priority of the current item.
    void increase(const Item& item, const Value& prio) {
      int id = index[item];
      int kd = nodes[id].parent;
      nodes[id].prio = prio;
      while (kd >= 0 && nodes[kd].item == item) {
        setPrio(kd);
        kd = nodes[kd].parent;
      }
    }

    /// \brief Increase the priority of the current item.
    ///
    /// Increase the priority of the current item.
    void decrease(const Item& item, const Value& prio) {
      int id = index[item];
      int kd = nodes[id].parent;
      nodes[id].prio = prio;
      while (kd >= 0 && less(id, kd)) {
        nodes[kd].prio = prio;
        nodes[kd].item = item;
        kd = nodes[kd].parent;
      }
    }

    /// \brief Gives back the minimum priority of the class.
    ///
    /// Gives back the minimum priority of the class.
    const Value& classPrio(int cls) const {
      return nodes[~(classes[cls].parent)].prio;
    }

    /// \brief Gives back the minimum priority item of the class.
    ///
    /// \return Gives back the minimum priority item of the class.
    const Item& classTop(int cls) const {
      return nodes[~(classes[cls].parent)].item;
    }

    /// \brief Gives back a representant item of the class.
    ///
    /// Gives back a representant item of the class.
    /// The representant is indpendent from the priorities of the
    /// items.
    const Item& classRep(int id) const {
      int parent = classes[id].parent;
      return nodes[parent >= 0 ? classes[id].depth : leftNode(id)].item;
    }

    /// \brief LEMON style iterator for the items of a class.
    ///
    /// ClassIt is a lemon style iterator for the components. It iterates
    /// on the items of a class. By example if you want to iterate on
    /// each items of each classes then you may write the next code.
    ///\code
    /// for (ClassIt cit(huf); cit != INVALID; ++cit) {
    ///   std::cout << "Class: ";
    ///   for (ItemIt iit(huf, cit); iit != INVALID; ++iit) {
    ///     std::cout << toString(iit) << ' ' << std::endl;
    ///   }
    ///   std::cout << std::endl;
    /// }
    ///\endcode
    class ItemIt {
    private:

      const HeapUnionFind* _huf;
      int _id, _lid;

    public:

      /// \brief Default constructor
      ///
      /// Default constructor
      ItemIt() {}

      ItemIt(const HeapUnionFind& huf, int cls) : _huf(&huf) {
        int id = cls;
        int parent = _huf->classes[id].parent;
        if (parent >= 0) {
          _id = _huf->classes[id].depth;
          if (_huf->classes[id].next != -1) {
            _lid = _huf->classes[_huf->classes[id].next].depth;
          } else {
            _lid = -1;
          }
        } else {
          _id = _huf->leftNode(id);
          _lid = -1;
        }
      }

      /// \brief Increment operator
      ///
      /// It steps to the next item in the class.
      ItemIt& operator++() {
        _id = _huf->nextNode(_id);
        return *this;
      }

      /// \brief Conversion operator
      ///
      /// It converts the iterator to the current item.
      operator const Item&() const {
        return _huf->nodes[_id].item;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ItemIt& i) {
        return i._id == _id;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ItemIt& i) {
        return i._id != _id;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(Invalid) {
        return _id == _lid;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(Invalid) {
        return _id != _lid;
      }

    };

    /// \brief Class iterator
    ///
    /// The iterator stores
    class ClassIt {
    private:

      const HeapUnionFind* _huf;
      int _id;

    public:

      ClassIt(const HeapUnionFind& huf)
        : _huf(&huf), _id(huf.first_class) {}

      ClassIt(const HeapUnionFind& huf, int cls)
        : _huf(&huf), _id(huf.classes[cls].left) {}

      ClassIt(Invalid) : _huf(0), _id(-1) {}

      const ClassIt& operator++() {
        _id = _huf->classes[_id].next;
        return *this;
      }

      /// \brief Equality operator
      ///
      /// Equality operator
      bool operator==(const ClassIt& i) {
        return i._id == _id;
      }

      /// \brief Inequality operator
      ///
      /// Inequality operator
      bool operator!=(const ClassIt& i) {
        return i._id != _id;
      }

      operator int() const {
        return _id;
      }

    };

  };

  //! @}

} //namespace lemon

#endif //LEMON_UNION_FIND_H
