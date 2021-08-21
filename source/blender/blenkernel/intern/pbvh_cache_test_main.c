#include "MEM_guardedalloc.h"

#include "BLI_compiler_compat.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_pbvh.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "pbvh_intern.h"

#include <stdio.h>
#include <stdlib.h>

#include <math.h>

void pbvh_bmesh_do_cache_test(void);

int main(int argc, char **argv)
{
  printf("argc: %d\n", argc);

  pbvh_bmesh_do_cache_test();

  return 0;
}
