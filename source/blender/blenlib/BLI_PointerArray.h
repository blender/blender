#ifndef _BLI_PARRAY_H
#define _BLI_PARRAY_H

typedef struct PointerArray {
    void **array;
    int len;
} PointerArray;

extern void PA_AddToArray(PointerArray *arr, void *ptr, int index);
extern void PA_FreeArray(PointerArray *arr);

#define LA_ARR_INC	2048

#endif /* _BLI_PARRAY_H */
