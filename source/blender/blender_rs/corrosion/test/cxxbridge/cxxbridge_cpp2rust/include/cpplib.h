#pragma once
#include "cxxbridge-cpp/lib.h"

::RsImage read_image(::rust::Str path);
void write_image(::rust::Str path, ::RsImage const & image);
