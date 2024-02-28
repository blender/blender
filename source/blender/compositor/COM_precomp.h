/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Pre-compiled headers, see: D13797. */

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <ostream>
#include <set>
#include <string>

#include "COM_ConstantOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_Debug.h"
#include "COM_Enums.h"
#include "COM_ExecutionSystem.h"
#include "COM_MultiThreadedOperation.h"
#include "COM_Node.h"
#include "COM_NodeOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SetSamplerOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_defines.h"
