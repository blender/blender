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

#include "build_config.h"

#if OS_LINUX

#include "numaapi.h"

#include <stdlib.h>

#ifndef WITH_DYNLOAD
#  include <numa.h>
#else
#  include <dlfcn.h>
#endif

#ifdef WITH_DYNLOAD

// Descriptor numa library.
static void* numa_lib;

// Types of all symbols which are read from the library.
struct bitmask;
typedef int tnuma_available(void);
typedef int tnuma_max_node(void);
typedef int tnuma_node_to_cpus(int node, struct bitmask* mask);
typedef long tnuma_node_size(int node, long* freep);
typedef int tnuma_run_on_node(int node);
typedef void* tnuma_alloc_onnode(size_t size, int node);
typedef void* tnuma_alloc_local(size_t size);
typedef void tnuma_free(void* start, size_t size);
typedef struct bitmask* tnuma_bitmask_clearall(struct bitmask *bitmask);
typedef int tnuma_bitmask_isbitset(const struct bitmask *bitmask,
                                   unsigned int n);
typedef struct bitmask* tnuma_bitmask_setbit(struct bitmask *bitmask,
                                             unsigned int n);
typedef unsigned int tnuma_bitmask_nbytes(struct bitmask *bitmask);
typedef void tnuma_bitmask_free(struct bitmask *bitmask);
typedef struct bitmask* tnuma_allocate_cpumask(void);
typedef struct bitmask* tnuma_allocate_nodemask(void);
typedef void tnuma_free_cpumask(struct bitmask* bitmask);
typedef void tnuma_free_nodemask(struct bitmask* bitmask);
typedef int tnuma_run_on_node_mask(struct bitmask *nodemask);
typedef int tnuma_run_on_node_mask_all(struct bitmask *nodemask);
typedef struct bitmask *tnuma_get_run_node_mask(void);
typedef void tnuma_set_interleave_mask(struct bitmask *nodemask);
typedef void tnuma_set_localalloc(void);

// Actual symbols.
static tnuma_available* numa_available;
static tnuma_max_node* numa_max_node;
static tnuma_node_to_cpus* numa_node_to_cpus;
static tnuma_node_size* numa_node_size;
static tnuma_run_on_node* numa_run_on_node;
static tnuma_alloc_onnode* numa_alloc_onnode;
static tnuma_alloc_local* numa_alloc_local;
static tnuma_free* numa_free;
static tnuma_bitmask_clearall* numa_bitmask_clearall;
static tnuma_bitmask_isbitset* numa_bitmask_isbitset;
static tnuma_bitmask_setbit* numa_bitmask_setbit;
static tnuma_bitmask_nbytes* numa_bitmask_nbytes;
static tnuma_bitmask_free* numa_bitmask_free;
static tnuma_allocate_cpumask* numa_allocate_cpumask;
static tnuma_allocate_nodemask* numa_allocate_nodemask;
static tnuma_free_nodemask* numa_free_nodemask;
static tnuma_free_cpumask* numa_free_cpumask;
static tnuma_run_on_node_mask* numa_run_on_node_mask;
static tnuma_run_on_node_mask_all* numa_run_on_node_mask_all;
static tnuma_get_run_node_mask* numa_get_run_node_mask;
static tnuma_set_interleave_mask* numa_set_interleave_mask;
static tnuma_set_localalloc* numa_set_localalloc;

static void* findLibrary(const char** paths) {
  int i = 0;
  while (paths[i] != NULL) {
      void* lib = dlopen(paths[i], RTLD_LAZY);
      if (lib != NULL) {
        return lib;
      }
      ++i;
  }
  return NULL;
}

static void numaExit(void) {
  if (numa_lib == NULL) {
    return;
  }
  dlclose(numa_lib);
  numa_lib = NULL;
}

static NUMAAPI_Result loadNumaSymbols(void) {
  // Prevent multiple initializations.
  static bool initialized = false;
  static NUMAAPI_Result result = NUMAAPI_NOT_AVAILABLE;
  if (initialized) {
    return result;
  }
  initialized = true;
  // Find appropriate .so library.
  const char* numa_paths[] = {
      "libnuma.so.1",
      "libnuma.so",
      NULL};
  // Register de-initialization.
  const int error = atexit(numaExit);
  if (error) {
    result = NUMAAPI_ERROR_ATEXIT;
    return result;
  }
  // Load library.
  numa_lib = findLibrary(numa_paths);
  if (numa_lib == NULL) {
    result = NUMAAPI_NOT_AVAILABLE;
    return result;
  }
  // Load symbols.

#define _LIBRARY_FIND(lib, name)          \
  do {                                    \
    name = (t##name *)dlsym(lib, #name);  \
  } while (0)
#define NUMA_LIBRARY_FIND(name) _LIBRARY_FIND(numa_lib, name)

  NUMA_LIBRARY_FIND(numa_available);
  NUMA_LIBRARY_FIND(numa_max_node);
  NUMA_LIBRARY_FIND(numa_node_to_cpus);
  NUMA_LIBRARY_FIND(numa_node_size);
  NUMA_LIBRARY_FIND(numa_run_on_node);
  NUMA_LIBRARY_FIND(numa_alloc_onnode);
  NUMA_LIBRARY_FIND(numa_alloc_local);
  NUMA_LIBRARY_FIND(numa_free);
  NUMA_LIBRARY_FIND(numa_bitmask_clearall);
  NUMA_LIBRARY_FIND(numa_bitmask_isbitset);
  NUMA_LIBRARY_FIND(numa_bitmask_setbit);
  NUMA_LIBRARY_FIND(numa_bitmask_nbytes);
  NUMA_LIBRARY_FIND(numa_bitmask_free);
  NUMA_LIBRARY_FIND(numa_allocate_cpumask);
  NUMA_LIBRARY_FIND(numa_allocate_nodemask);
  NUMA_LIBRARY_FIND(numa_free_cpumask);
  NUMA_LIBRARY_FIND(numa_free_nodemask);
  NUMA_LIBRARY_FIND(numa_run_on_node_mask);
  NUMA_LIBRARY_FIND(numa_run_on_node_mask_all);
  NUMA_LIBRARY_FIND(numa_get_run_node_mask);
  NUMA_LIBRARY_FIND(numa_set_interleave_mask);
  NUMA_LIBRARY_FIND(numa_set_localalloc);

#undef NUMA_LIBRARY_FIND
#undef _LIBRARY_FIND

  result = NUMAAPI_SUCCESS;
  return result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Initialization.

NUMAAPI_Result numaAPI_Initialize(void) {
#ifdef WITH_DYNLOAD
  NUMAAPI_Result result = loadNumaSymbols();
  if (result != NUMAAPI_SUCCESS) {
    return result;
  }
#endif
  if (numa_available() < 0) {
    return NUMAAPI_NOT_AVAILABLE;
  }
  return NUMAAPI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// Topology query.

int numaAPI_GetNumNodes(void) {
  return numa_max_node() + 1;
}

bool numaAPI_IsNodeAvailable(int node) {
  return numaAPI_GetNumNodeProcessors(node) > 0;
}

int numaAPI_GetNumNodeProcessors(int node) {
  struct bitmask* cpu_mask = numa_allocate_cpumask();
  numa_node_to_cpus(node, cpu_mask);
  const unsigned int num_bytes = numa_bitmask_nbytes(cpu_mask);
  const unsigned int num_bits = num_bytes * 8;
  // TODO(sergey): There might be faster way calculating number of set bits.
  int num_processors = 0;
  for (unsigned int bit = 0; bit < num_bits; ++bit) {
    if (numa_bitmask_isbitset(cpu_mask, bit)) {
      ++num_processors;
    }
  }
#ifdef WITH_DYNLOAD
  if (numa_free_cpumask != NULL) {
    numa_free_cpumask(cpu_mask);
  } else {
    numa_bitmask_free(cpu_mask);
  }
#else
  numa_free_cpumask(cpu_mask);
#endif
  return num_processors;
}

////////////////////////////////////////////////////////////////////////////////
// Topology helpers.

int numaAPI_GetNumCurrentNodesProcessors(void) {
  struct bitmask* node_mask = numa_get_run_node_mask();
  const unsigned int num_bytes = numa_bitmask_nbytes(node_mask);
  const unsigned int num_bits = num_bytes * 8;
  int num_processors = 0;
  for (unsigned int bit = 0; bit < num_bits; ++bit) {
    if (numa_bitmask_isbitset(node_mask, bit)) {
      num_processors += numaAPI_GetNumNodeProcessors(bit);
    }
  }
  numa_bitmask_free(node_mask);
  return num_processors;
}

////////////////////////////////////////////////////////////////////////////////
// Affinities.

bool numaAPI_RunProcessOnNode(int node) {
  numaAPI_RunThreadOnNode(node);
  return true;
}

bool numaAPI_RunThreadOnNode(int node) {
  // Construct bit mask from node index.
  struct bitmask* node_mask = numa_allocate_nodemask();
  numa_bitmask_clearall(node_mask);
  numa_bitmask_setbit(node_mask, node);
  numa_run_on_node_mask_all(node_mask);
  // TODO(sergey): The following commands are based on x265 code, we might want
  // to make those optional, or require to call those explicitly.
  //
  // Current assumption is that this is similar to SetThreadGroupAffinity().
  if (numa_node_size(node, NULL) > 0) {
    numa_set_interleave_mask(node_mask);
    numa_set_localalloc();
  }
#ifdef WITH_DYNLOAD
  if (numa_free_nodemask != NULL) {
    numa_free_nodemask(node_mask);
  } else {
    numa_bitmask_free(node_mask);
  }
#else
  numa_free_nodemask(node_mask);
#endif
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Memory management.

void* numaAPI_AllocateOnNode(size_t size, int node) {
  return numa_alloc_onnode(size, node);
}

void* numaAPI_AllocateLocal(size_t size) {
  return numa_alloc_local(size);
}

void numaAPI_Free(void* start, size_t size) {
  numa_free(start, size);
}

#endif  // OS_LINUX
