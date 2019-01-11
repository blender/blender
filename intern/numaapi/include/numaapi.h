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

#ifndef __LIBNUMAAPI_H__
#define __LIBNUMAAPI_H__

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUMAAPI_VERSION_MAJOR 1
#define NUMAAPI_VERSION_MINOR 0

typedef enum NUMAAPI_Result {
  NUMAAPI_SUCCESS       = 0,
  // NUMA is not available on this platform.
  NUMAAPI_NOT_AVAILABLE = 1,
  // Generic error, no real details are available,
  NUMAAPI_ERROR         = 2,
  // Error installing atexit() handlers.
  NUMAAPI_ERROR_ATEXIT  = 3,
} NUMAAPI_Result;

////////////////////////////////////////////////////////////////////////////////
// Initialization.

// Initialize NUMA API.
//
// This is first call which should be called before any other NUMA functions
// can be used.
NUMAAPI_Result numaAPI_Initialize(void);

// Get string representation of NUMAPIResult.
const char* numaAPI_ResultAsString(NUMAAPI_Result result);

////////////////////////////////////////////////////////////////////////////////
// Topology query.

// Get number of available nodes.
//
// This is in fact an index of last node plus one and it's not guaranteed
// that all nodes up to this one are available.
int numaAPI_GetNumNodes(void);

// Returns truth if the given node is available for compute.
bool numaAPI_IsNodeAvailable(int node);

// Get number of available processors on a given node.
int numaAPI_GetNumNodeProcessors(int node);

////////////////////////////////////////////////////////////////////////////////
// Topology helpers.
//
// Those are a bit higher level queries, but is still rather platform-specific
// and generally useful.

// Get number of processors within the NUMA nodes on which current thread is
// set affinity on.
int numaAPI_GetNumCurrentNodesProcessors(void);

////////////////////////////////////////////////////////////////////////////////
// Affinities.

// Runs the current process and its children on a specific node.
//
// Returns truth if affinity has successfully changed.
//
// NOTE: This function can not change active CPU group. Mainly designed to deal
// with Threadripper 2 topology, to make it possible to gain maximum performance
// for the main application thread.
bool numaAPI_RunProcessOnNode(int node);

// Runs the current thread and its children on a specific node.
//
// Returns truth if affinity has successfully changed.
bool numaAPI_RunThreadOnNode(int node);

////////////////////////////////////////////////////////////////////////////////
// Memory management.

// Allocate memory on a given node,
void* numaAPI_AllocateOnNode(size_t size, int node);

// Allocate memory in the local memory, closest to the current node.
void* numaAPI_AllocateLocal(size_t size);

// Frees size bytes of memory starting at start.
//
// TODO(sergey): Consider making it regular free() semantic.
void numaAPI_Free(void* start, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // __LIBNUMAAPI_H__
