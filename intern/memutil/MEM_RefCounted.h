/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 *
 * Declaration of MEM_RefCounted class.
 */

#ifndef __MEM_REFCOUNTED_H__
#define __MEM_REFCOUNTED_H__

/**
 * An object with reference counting.
 * Base class for objects with reference counting.
 * When a shared object is ceated, it has reference count == 1.
 * If the reference count of a shared object reaches zero, the object self-destructs.
 * The default destructor of this object has been made protected on purpose.
 * This disables the creation of shared objects on the stack.
 *
 * \author  Maarten Gribnau
 * \date    March 31, 2001
 */
class MEM_RefCounted {
 public:
  /**
   * Constructs a shared object.
   */
  MEM_RefCounted() : m_refCount(1) {}

  /**
   * Returns the reference count of this object.
   * \return the reference count.
   */
  inline virtual int getRef() const;

  /**
   * Increases the reference count of this object.
   * \return the new reference count.
   */
  inline virtual int incRef();

  /**
   * Decreases the reference count of this object.
   * If the reference count reaches zero, the object self-destructs.
   * \return the new reference count.
   */
  inline virtual int decRef();

 protected:
  /**
   * Destructs a shared object.
   * The destructor is protected to force the use of incRef and decRef.
   */
  virtual ~MEM_RefCounted() {}

 protected:
  /** The reference count. */
  int m_refCount;
};

inline int MEM_RefCounted::getRef() const
{
  return m_refCount;
}

inline int MEM_RefCounted::incRef()
{
  return ++m_refCount;
}

inline int MEM_RefCounted::decRef()
{
  m_refCount--;
  if (m_refCount == 0) {
    delete this;
    return 0;
  }
  return m_refCount;
}

#endif  // __MEM_REFCOUNTED_H__
