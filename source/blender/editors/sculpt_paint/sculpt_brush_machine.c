#if 0
#  include "MEM_guardedalloc.h"

#  include "BLI_alloca.h"
#  include "BLI_array.h"
#  include "BLI_bitmap.h"
#  include "BLI_compiler_attrs.h"
#  include "BLI_compiler_compat.h"
#  include "BLI_listbase.h"
#  include "BLI_math.h"
#  include "BLI_memarena.h"

#  include "DNA_node_types.h"

#  include "BKE_context.h"
#  include "BKE_node.h"

enum {
  OP_MATH,
  OP_COERCE,
  OP_TOOL,
  OP_CURVE_MAP,
  OP_DYNTOPO,
  OP_PEN_INPUT,
  OP_PUSH,
  OP_POP,
  OP_POP_REG,
  OP_PUSH_REG,
  OP_LOAD_REG,
  OP_STORE_REG
};

#  define MAX_BRUSH_OP_ARGS 8
#  define MAX_BRUSH_REGISTERS 128

typedef union BrushOpArg {
  float f;
  int i;
  void *p;
  char *s;
  float v[4];
} BrushOpArg;

typedef struct BrushOpCode {
  int code;

  BrushOpArg args[MAX_BRUSH_OP_ARGS];
} BrushOpCode;

typedef struct SculptBrushVM {
  BrushOpCode *codes;
  int totcode;

  BrushOpArg registers[MAX_BRUSH_REGISTERS];
  BLI_
} SculptBrushVM;

void SculptVM_AppendOp(SculptBrushVM *vm, BrushOpArg arg)
{
}

#endif
