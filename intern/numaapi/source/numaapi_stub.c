// Copyright (c) 2016, libnumaapi authors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Author: Sergey Sharybin (sergey.vfx@gmail.com)

#include "numaapi.h"

#include "build_config.h"

// Stub implementation for platforms which doesn't have NUMA support.

#if !OS_LINUX && !OS_WIN

////////////////////////////////////////////////////////////////////////////////
// Initialization.

NUMAAPI_Result numaAPI_Initialize(void) {
  return NUMAAPI_NOT_AVAILABLE;
}

////////////////////////////////////////////////////////////////////////////////
// Topology query.

int numaAPI_GetNumNodes(void) {
  return 0;
}

bool numaAPI_IsNodeAvailable(int node) {
  (void) node;  // Ignored.
  return false;
}

int numaAPI_GetNumNodeProcessors(int node) {
  (void) node;  // Ignored.
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Affinities.

bool numaAPI_RunProcessOnNode(int node) {
  (void) node;  // Ignored.
  return false;
}

bool numaAPI_RunThreadOnNode(int node) {
  (void) node;  // Ignored.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Memory management.

void* numaAPI_AllocateOnNode(size_t size, int node) {
  (void) size;  // Ignored.
  (void) node;  // Ignored.
  return 0;
}

void* numaAPI_AllocateLocal(size_t size) {
  (void) size;  // Ignored.
  return NULL;
}

void numaAPI_Free(void* start, size_t size) {
  (void) start;  // Ignored.
  (void) size;  // Ignored.
}

#endif  // !OS_LINUX && !OS_WIN
