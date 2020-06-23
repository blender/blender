/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FN_MULTI_FUNCTION_CONTEXT_HH__
#define __FN_MULTI_FUNCTION_CONTEXT_HH__

/** \file
 * \ingroup fn
 *
 * An MFContext is passed along with every call to a multi-function. Right now it does nothing, but
 * it can be used for the following purposes:
 * - Pass debug information up and down the function call stack.
 * - Pass reusable memory buffers to subfunctions to increase performance.
 * - Pass cached data to called functions.
 */

#include "BLI_utildefines.h"

namespace blender {
namespace fn {

class MFContextBuilder {
};

class MFContext {
 public:
  MFContext(MFContextBuilder &UNUSED(builder))
  {
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_CONTEXT_HH__ */
