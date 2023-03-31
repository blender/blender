/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Utility header for auto_ptr/unique_ptr selection
 */

#include <memory>

namespace Freestyle {

template<typename T> class AutoPtr : public std::unique_ptr<T> {
 public:
  using std::unique_ptr<T>::unique_ptr;

  AutoPtr() : std::unique_ptr<T>() {}
  AutoPtr(T *ptr) : std::unique_ptr<T>(ptr) {}

  /* Mimic behavior of legacy auto_ptr.
   * Keep implementation as small as possible, hens delete assignment operator. */

  template<typename X> AutoPtr(AutoPtr<X> &other) : std::unique_ptr<T>(other.get())
  {
    other.release();
  }

  using std::unique_ptr<T>::operator=;

  template<typename X> AutoPtr &operator=(AutoPtr<X> &other) = delete;
};

} /* namespace Freestyle */
