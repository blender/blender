/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_lazy_threading.hh"
#include "BLI_vector.hh"

namespace blender::lazy_threading {

/**
 * This is a #RawVector so that it can be destructed after Blender checks for memory leaks.
 */
thread_local RawVector<FunctionRef<void()>, 0> hint_receivers;

void send_hint()
{
  for (const FunctionRef<void()> &fn : hint_receivers) {
    fn();
  }
}

HintReceiver::HintReceiver(const FunctionRef<void()> fn)
{
  hint_receivers.append(fn);
}

HintReceiver::~HintReceiver()
{
  hint_receivers.pop_last();
}

}  // namespace blender::lazy_threading
