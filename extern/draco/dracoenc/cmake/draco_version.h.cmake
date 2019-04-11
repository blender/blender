// If this file is named draco_version.h.cmake:
// This file is used as input at cmake generation time.

// If this file is named draco_version.h:
// GENERATED FILE, DO NOT EDIT. SEE ABOVE.
#ifndef DRACO_DRACO_VERSION_H_
#define DRACO_DRACO_VERSION_H_

#include "draco/core/draco_version.h"

// Returns git hash of Draco git repository.
const char *draco_git_hash();

// Returns the output of the git describe command when run from the Draco git
// repository.
const char *draco_git_version();

// Returns the version string from core/draco_version.h.
const char* draco_version();

#endif  // DRACO_DRACO_VERSION_H_
