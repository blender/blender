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

#ifndef __MEMORYALLOCATOR_H__
#define __MEMORYALLOCATOR_H__

#include <stdio.h>
#include <stdlib.h>

#define HEAP_BASE 16
#define UCHAR unsigned char

/**
 * Customized memory allocators that allocates/deallocates memory in chunks
 *
 * @author Tao Ju
 */

/**
 * Base class of memory allocators
 */
class VirtualMemoryAllocator {
 public:
  virtual ~VirtualMemoryAllocator()
  {
  }

  virtual void *allocate() = 0;
  virtual void deallocate(void *obj) = 0;
  virtual void destroy() = 0;
  virtual void printInfo() = 0;

  virtual int getAllocated() = 0;
  virtual int getAll() = 0;
  virtual int getBytes() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:VirtualMemoryAllocator")
#endif
};

/**
 * Dynamic memory allocator - allows allocation/deallocation
 *
 * Note: there are 4 bytes overhead for each allocated yet unused object.
 */
template<int N> class MemoryAllocator : public VirtualMemoryAllocator {
 private:
  /// Constants
  int HEAP_UNIT, HEAP_MASK;

  /// Data array
  UCHAR **data;

  /// Allocation stack
  UCHAR ***stack;

  /// Number of data blocks
  int datablocknum;

  /// Number of stack blocks
  int stackblocknum;

  /// Size of stack
  int stacksize;

  /// Number of available objects on stack
  int available;

  /**
   * Allocate a memory block
   */
  void allocateDataBlock()
  {
    // Allocate a data block
    datablocknum += 1;
    data = (UCHAR **)realloc(data, sizeof(UCHAR *) * datablocknum);
    data[datablocknum - 1] = (UCHAR *)malloc(HEAP_UNIT * N);

    // Update allocation stack
    for (int i = 0; i < HEAP_UNIT; i++) {
      stack[0][i] = (data[datablocknum - 1] + i * N);
    }
    available = HEAP_UNIT;
  }

  /**
   * Allocate a stack block, to store more deallocated objects
   */
  void allocateStackBlock()
  {
    // Allocate a stack block
    stackblocknum += 1;
    stacksize += HEAP_UNIT;
    stack = (UCHAR ***)realloc(stack, sizeof(UCHAR **) * stackblocknum);
    stack[stackblocknum - 1] = (UCHAR **)malloc(HEAP_UNIT * sizeof(UCHAR *));
  }

 public:
  /**
   * Constructor
   */
  MemoryAllocator()
  {
    HEAP_UNIT = 1 << HEAP_BASE;
    HEAP_MASK = (1 << HEAP_BASE) - 1;

    data = (UCHAR **)malloc(sizeof(UCHAR *));
    data[0] = (UCHAR *)malloc(HEAP_UNIT * N);
    datablocknum = 1;

    stack = (UCHAR ***)malloc(sizeof(UCHAR **));
    stack[0] = (UCHAR **)malloc(HEAP_UNIT * sizeof(UCHAR *));
    stackblocknum = 1;
    stacksize = HEAP_UNIT;
    available = HEAP_UNIT;

    for (int i = 0; i < HEAP_UNIT; i++) {
      stack[0][i] = (data[0] + i * N);
    }
  }

  /**
   * Destructor
   */
  void destroy()
  {
    int i;
    for (i = 0; i < datablocknum; i++) {
      free(data[i]);
    }
    for (i = 0; i < stackblocknum; i++) {
      free(stack[i]);
    }
    free(data);
    free(stack);
  }

  /**
   * Allocation method
   */
  void *allocate()
  {
    if (available == 0) {
      allocateDataBlock();
    }

    // printf("Allocating %d\n", header[ allocated ]) ;
    available--;
    return (void *)stack[available >> HEAP_BASE][available & HEAP_MASK];
  }

  /**
   * De-allocation method
   */
  void deallocate(void *obj)
  {
    if (available == stacksize) {
      allocateStackBlock();
    }

    // printf("De-allocating %d\n", ( obj - data ) / N ) ;
    stack[available >> HEAP_BASE][available & HEAP_MASK] = (UCHAR *)obj;
    available++;
    // printf("%d %d\n", allocated, header[ allocated ]) ;
  }

  /**
   * Print information
   */
  void printInfo()
  {
    printf("Bytes: %d Used: %d Allocated: %d Maxfree: %d\n",
           getBytes(),
           getAllocated(),
           getAll(),
           stacksize);
  }

  /**
   * Query methods
   */
  int getAllocated()
  {
    return HEAP_UNIT * datablocknum - available;
  };

  int getAll()
  {
    return HEAP_UNIT * datablocknum;
  };

  int getBytes()
  {
    return N;
  };

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:MemoryAllocator")
#endif
};

#endif /* __MEMORYALLOCATOR_H__ */
