#pragma once

/*

This is a minimal page-based CustomData backend for bmesh.
It's purpose is to test whether a page-based system would
be faster then the current block-based one.

The idea is to plug into the existing API in as minimal
a way as possible.

*/

#include "BKE_attribute.h"
#include "bmesh.h"

#ifdef USE_BMESH_PAGE_CUSTOMDATA

#  define BM_PAGE_SHIFT 10
#  define BM_PAGE_SIZE (1 << BM_PAGE_SHIFT)
#  define BM_PAGE_MASK (BM_PAGE_SIZE - 1)

#  ifdef __cplusplus

#    include "BLI_alloca.h"
#    include "BLI_array.hh"
#    include <vector>

namespace blender {
namespace bmesh {

using PageElemRef = int;

template<int PageSizeShift = BM_PAGE_SHIFT> struct PageArray {
  std::vector<void *> pages;
  size_t elemSize;
  eCustomDataType type;

  PageArray(const PageArray &b)
  {
    elemSize = b.elemSize;
    pages = b.pages;
  }

  PageArray(const PageArray &&b)
  {
    elemSize = b.elemSize;
    pages = std::move(b.pages);
  }

  PageArray(eCustomDataType t, size_t size = 0) : type(t)
  {
    elemSize = CustomData_getTypeSize(type);

    reserve(size);
  }

  ~PageArray()
  {
  }

  void *getElemPtr(PageElemRef elem)
  {
    size_t page = elem >> PageSizeShift;
    size_t elem_i = elem & ((1 << PageSizeShift) - 1);

    char *ptr = static_cast<char *>(pages[page]);
    ptr += elem_i * elemSize;

    return static_cast<void *>(ptr);
  }

  void interp(PageElemRef elem, int count, PageElemRef *srcs, float *ws, float *sub_ws)
  {
    void **blocks = static_cast<void **>(BLI_array_alloca(blocks, count));

    for (int i : IndexRange(count)) {
      blocks[i] = getElemPtr(srcs[i]);
    }

    CustomData_interpData(type, getElemPtr(elem), count, (const void **)blocks, ws, sub_ws);
  }

  void free(PageElemRef elem)
  {
    CustomData_freeData(type, getElemPtr(elem), 1);
  }

  void setDefault(PageElemRef elem)
  {
    CustomData_setDefaultData(type, getElemPtr(elem), 1);
  }

  void reserve(size_t size)
  {
    size_t totpage = size >> PageSizeShift;
    // const size_t pagesize = 1ULL << PageSizeShift;

    int curpage = pages.size();
    pages.resize(totpage);

    for (int i = curpage; i < totpage; i++) {
      newPage();
    }
  }

  void newPage()
  {
    const int pagesize = 1 << PageSizeShift;

    pages.resize(pages.size() + 1);
    pages[pages.size() - 1] = MEM_malloc_arrayN(pagesize, elemSize, "bmesh attribute page");
  }
};

struct BMAttrDomain {
  std::vector<PageElemRef> freelist;
  std::vector<PageArray<BM_PAGE_SHIFT> *> arrays;

  eAttrDomain domain;
  int totpage;
  int totelem;
  int totalloc;

  BMAttrDomain(eAttrDomain d) : domain(d), totpage(0), totelem(0)
  {
  }

  ~BMAttrDomain()
  {
    for (PageArray<BM_PAGE_SHIFT> *array : arrays) {
      delete array;
    }
  }

  void interp(PageElemRef elem, int count, PageElemRef *srcs, float *ws, float *sub_ws)
  {
    for (PageArray<BM_PAGE_SHIFT> *array : arrays) {
      array->interp(elem, count, srcs, ws, sub_ws);
    }
  }

  PageArray<BM_PAGE_SHIFT> *addLayer(eCustomDataType type)
  {
    PageArray<BM_PAGE_SHIFT> *array = new PageArray<BM_PAGE_SHIFT>(type);
    array->reserve(totelem);

    /* keep layers ordered by type */

    bool state = false;
    for (auto iter = arrays.begin(); iter != arrays.end(); ++iter) {
      if ((*iter)->type == type) {
        state = true;
        // now find next layer with wrong type, we
        // will insert before it.
      }
      else if (state) {
        arrays.insert(iter, array);
        return array;
      }
    }

    arrays.push_back(array);
    return array;
  }

  PageElemRef alloc(bool &haveNewPage)
  {
    if (freelist.size() == 0) {
      haveNewPage = true;
      newPage();
    }
    else {
      haveNewPage = false;
    }

    totelem++;
    PageElemRef r = freelist[freelist.size() - 1];
    freelist.pop_back();

    for (auto *array : arrays) {
      array->setDefault(r);
    }

    return r;
  }

  void setDefault(PageElemRef ref)
  {
    for (auto *array : arrays) {
      array->setDefault(ref);
    }
  }

  void free(PageElemRef ref, bool &removedPage)
  {
    removedPage = false;

    totelem--;
    freelist.push_back(ref);
  }

 private:
  void newPage()
  {
    totpage++;
    totalloc += BM_PAGE_SIZE;

    for (PageArray<> *array : arrays) {
      array->newPage();
    }

    size_t count = 1 << BM_PAGE_SHIFT;
    for (size_t i = 0; i < count; i++) {
      freelist.push_back(i);
    }
  }
};

extern "C" {
#  else
struct BMAttrDomain;
#  endif  //_cplusplus

#  include "BLI_compiler_compat.h"
#  include "bmesh_class.h"

struct BMesh;

typedef struct BMeshPageArray {
  int esize, psize;
  void **pages;
  void *cppClass;
} BmeshPageArray;

typedef struct BMeshAttrList {
  BMeshPageArray **arrays;
  int totarray;
  struct BMAttrDomain *domains[ATTR_DOMAIN_NUM];
} BMeshAttrList;

typedef struct BMeshPageRef {
  // point to arrays, attribute for all domains go into one
  // list of arrays
  BMeshAttrList *attrs;
  int idx;  // index into attribute list, NOT element index
  int domain;
} BMeshPageRef;

BLI_INLINE void *BM_ELEM_CD_GET_VOID_P_2(BMElem *elem, int offset)
{
  BMeshPageRef *ref = (BMeshPageRef *)elem->head.data;
  BmeshPageArray *array = ref->attrs->arrays[offset];
  size_t page = ref->idx >> BM_PAGE_SHIFT;
  size_t off = ref->idx & BM_PAGE_MASK;

  char *ptr = (char *)array->pages[page];
  ptr += off * array->esize;

  return (void *)ptr;
}

BMeshAttrList *BMAttr_new();
void BMAttr_reset(BMeshAttrList *list);
void BMAttr_free(BMeshAttrList *list);
void BMAttr_fromCData(BMeshAttrList *list, CustomData *domains[ATTR_DOMAIN_NUM]);
void BMAttr_init(struct BMesh *bm);

#  ifdef __cplusplus
}

}  // namespace bmesh
}  // namespace blender
#  endif
#endif
