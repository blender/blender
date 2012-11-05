// Copyright 2011 Google Inc. All Rights Reserved.
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_SPLIT_H_
#define VISION_OPTIMIZATION_LEAST_SQUARES_INTERNAL_SPLIT_H_

#include <string>
#include <vector>
#include "ceres/internal/port.h"

namespace ceres {

// Split a string using one or more character delimiters, presented as a
// nul-terminated c string. Append the components to 'result'. If there are
// consecutive delimiters, this function skips over all of them.
void SplitStringUsing(const string& full, const char* delim,
                      vector<string>* res);

}  // namespace ceres

#endif  // CERES_INTERNAL_SPLIT_H_
