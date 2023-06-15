/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_FUNCTION_H__
#define __UTIL_FUNCTION_H__

#include <functional>

CCL_NAMESPACE_BEGIN

#define function_bind std::bind
#define function_null nullptr
using std::function;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using std::placeholders::_5;
using std::placeholders::_6;
using std::placeholders::_7;
using std::placeholders::_8;
using std::placeholders::_9;

CCL_NAMESPACE_END

#endif /* __UTIL_FUNCTION_H__ */
