/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
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

#include <iostream>

#include <lemon/error.h>
#include "test_tools.h"

using namespace lemon;

#ifdef LEMON_ENABLE_ASSERTS
#undef LEMON_ENABLE_ASSERTS
#endif

#ifdef LEMON_DISABLE_ASSERTS
#undef LEMON_DISABLE_ASSERTS
#endif

#ifdef NDEBUG
#undef NDEBUG
#endif

//checking disabled asserts
#define LEMON_DISABLE_ASSERTS
#include <lemon/assert.h>

void no_assertion_text_disable() {
  LEMON_ASSERT(true, "This is a fault message");
}

void assertion_text_disable() {
  LEMON_ASSERT(false, "This is a fault message");
}

void check_assertion_disable() {
  no_assertion_text_disable();
  assertion_text_disable();
}
#undef LEMON_DISABLE_ASSERTS

//checking custom assert handler
#define LEMON_ASSERT_CUSTOM

static int cnt = 0;
void my_assert_handler(const char*, int, const char*,
                       const char*, const char*) {
  ++cnt;
}

#define LEMON_CUSTOM_ASSERT_HANDLER my_assert_handler
#include <lemon/assert.h>

void no_assertion_text_custom() {
  LEMON_ASSERT(true, "This is a fault message");
}

void assertion_text_custom() {
  LEMON_ASSERT(false, "This is a fault message");
}

void check_assertion_custom() {
  no_assertion_text_custom();
  assertion_text_custom();
  check(cnt == 1, "The custom assert handler does not work");
}

#undef LEMON_ASSERT_CUSTOM


int main() {
  check_assertion_disable();
  check_assertion_custom();

  return 0;
}
