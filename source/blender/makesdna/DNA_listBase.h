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

#include "BLI_listbase_iterator.hh"

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

/**
 * This is a thin wrapper around #ListBase to make it type-safe. It's designed to be used in DNA
 * structs. It is written as untyped #ListBase in .blend files for compatibility.
 */
template<typename T> struct ListBaseT : public ListBase {
  ListBaseTIterator<const T> begin() const
  {
    return ListBaseTIterator<const T>{static_cast<const T *>(this->first)};
  }

  ListBaseTIterator<const T> end() const
  {
    /* Don't use `this->last` because this iterator has to point to one-past-the-end. */
    return ListBaseTIterator<const T>{nullptr};
  }

  ListBaseTIterator<T> begin()
  {
    return ListBaseTIterator<T>{static_cast<T *>(this->first)};
  }

  ListBaseTIterator<T> end()
  {
    /* Don't use `this->last` because this iterator has to point to one-past-the-end. */
    return ListBaseTIterator<T>{nullptr};
  }
};

#endif

/* 8 byte alignment! */
