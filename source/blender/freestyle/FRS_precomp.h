/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Pre-compiled headers, see: D2606. */

#include <Python.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <math.h>
#include <memory>
#include <pthread.h>
#include <set>
#include <sstream>
#include <stack>
#include <stdarg.h>
#include <stdbool.h>
#include <string>
#include <time.h>
#include <vector>

#include "intern/python/BPy_BBox.h"
#include "intern/python/BPy_BinaryPredicate0D.h"
#include "intern/python/BPy_BinaryPredicate1D.h"
#include "intern/python/BPy_ContextFunctions.h"
#include "intern/python/BPy_Convert.h"
#include "intern/python/BPy_Freestyle.h"
#include "intern/python/BPy_FrsMaterial.h"
#include "intern/python/BPy_FrsNoise.h"
#include "intern/python/BPy_Id.h"
#include "intern/python/BPy_IntegrationType.h"
#include "intern/python/BPy_Interface0D.h"
#include "intern/python/BPy_Interface1D.h"
#include "intern/python/BPy_Iterator.h"
#include "intern/python/BPy_MediumType.h"
#include "intern/python/BPy_Nature.h"
#include "intern/python/BPy_Operators.h"
#include "intern/python/BPy_SShape.h"
#include "intern/python/BPy_StrokeAttribute.h"
#include "intern/python/BPy_StrokeShader.h"
#include "intern/python/BPy_UnaryFunction0D.h"
#include "intern/python/BPy_UnaryFunction1D.h"
#include "intern/python/BPy_UnaryPredicate0D.h"
#include "intern/python/BPy_UnaryPredicate1D.h"
#include "intern/python/BPy_ViewMap.h"
#include "intern/python/BPy_ViewShape.h"
#include "intern/python/Director.h"
