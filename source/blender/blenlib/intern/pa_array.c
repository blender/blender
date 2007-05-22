#include "BLI_PointerArray.h"
#include "MEM_guardedalloc.h"

#include <stdio.h>

void PA_AddToArray(struct PointerArray *arr, void *ptr, int index)
{
    void **tmp;
    int i;
    
    if (index>=arr->len) {
        //if (arr->len != 0) printf("Reallocating Array: %d.\n", arr->len);
        tmp = MEM_callocN(sizeof(void*)*(arr->len+LA_ARR_INC), "new pointer arrray");
		if (arr->array) {
			for (i=0; i<arr->len; i++) tmp[i] = arr->array[i];
			MEM_freeN(arr->array);
		}			
		arr->len += LA_ARR_INC;
		arr->array = tmp;
    }
    arr->array[index] = ptr;
}

void PA_FreeArray(struct PointerArray *arr)
{
    if (arr->array) MEM_freeN(arr->array);
    arr->array = NULL;
    arr->len = 0;
}    
 
