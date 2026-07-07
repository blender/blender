/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief These structs are the foundation for all linked lists in the library system.
 *
 * Doubly-linked lists start from a ListBase and contain elements beginning
 * with Link.
 */

#pragma once

namespace blender {

/** Generic - all structs which are put into linked lists begin with this. */
struct Link {
  struct Link *next, *prev;
};

/** Simple subclass of Link. Use this when it is not worth defining a custom one. */
struct LinkData {
  struct LinkData *next, *prev;
  void *data;
};

/**
 * The basic double linked-list structure.
 *
 * \warning Never change the size/definition of this struct! The #init_structDNA
 * function (from dna_genfile.cc) uses it to compute the #pointer_size.
 */
struct ListBase {
  void *first, *last;
};

#ifdef __cplusplus

template<typename T> struct ListBaseTIterator;
template<typename T> struct ListBaseEnumerateWrapper;
template<typename T> struct ListBaseMutableWrapper;
template<typename T> struct ListBaseBackwardWrapper;
template<typename T> struct ListBaseMutableBackwardWrapper;

/**
 * This is a thin wrapper around #ListBase to make it type-safe. It's designed to be used in DNA
 * structs. It is written as untyped #ListBase in .blend files for compatibility.
 */
template<typename T> struct ListBaseT : public ListBase {
  /* TODO: Add const and non-const iterators. However this will require some refactoring
   * as some places rely on being able to get a mutable list element from a const list. */
  ListBaseTIterator<T> begin() const
  {
    return ListBaseTIterator<T>{static_cast<T *>(this->first)};
  }

  ListBaseTIterator<T> end() const
  {
    /* Don't use `this->last` because this iterator has to point to one-past-the-end. */
    return ListBaseTIterator<T>{nullptr};
  }

  /** Iterator that returns also gives an index for every item. This helps prevent mistakes
   * where continue mistakenly skips the incrementation.
   *
   * Usage: `for(auto [index, item] : list.enumerate())` */
  ListBaseEnumerateWrapper<T> enumerate()
  {
    return {this->first};
  }

  ListBaseEnumerateWrapper<const T> enumerate() const
  {
    return {this->first};
  }

  /** Iterator that supports removing the item we're looping over. */
  ListBaseMutableWrapper<T> items_mutable()
  {
    return {this->first};
  }

  /** Iterator that runs in reverse order. */
  ListBaseBackwardWrapper<T> items_reversed()
  {
    return {this->last};
  }

  ListBaseBackwardWrapper<const T> items_reversed() const
  {
    return {this->last};
  }

  /** Iterator that runs in reverse order and supports removing the item we're looping over. */
  ListBaseMutableBackwardWrapper<T> items_reversed_mutable()
  {
    return {this->last};
  }

  /* Cast for opaque types and C style subclasses. */
  template<typename OtherT> const ListBaseT<OtherT> &cast() const
  {
    return *reinterpret_cast<const ListBaseT<OtherT> *>(this);
  }

  template<typename OtherT> ListBaseT<OtherT> &cast()
  {
    return *reinterpret_cast<ListBaseT<OtherT> *>(this);
  }
};

#endif

/* 8 byte alignment! */

}  // namespace blender
