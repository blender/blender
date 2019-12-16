/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Include pwrapper headers
 *
 ******************************************************************************/

#ifndef _MANTA_H
#define _MANTA_H

// Remove preprocessor keywords, so there won't infere with autocompletion etc.
#define KERNEL(...) extern int i, j, k, idx, X, Y, Z;
#define PYTHON(...)
#define returns(X) extern X;
#define alias typedef

#include "general.h"
#include "vectorbase.h"
#include "vector4d.h"
#include "registry.h"
#include "pclass.h"
#include "pconvert.h"
#include "fluidsolver.h"

#endif
