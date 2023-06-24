/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types_make.hh"

#include "BKE_cpp_types.h"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

#include "DNA_meshdata_types.h"

#include "FN_init.h"

struct Tex;
struct Image;
struct Material;

BLI_CPP_TYPE_MAKE(blender::bke::GeometrySet, CPPTypeFlags::Printable);
BLI_CPP_TYPE_MAKE(blender::bke::InstanceReference, CPPTypeFlags::None)

BLI_VECTOR_CPP_TYPE_MAKE(blender::bke::GeometrySet);

BLI_CPP_TYPE_MAKE(Object *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Collection *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Tex *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Image *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Material *, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(MStringProperty, CPPTypeFlags::None);

BLI_CPP_TYPE_MAKE(blender::bke::AnonymousAttributeSet, CPPTypeFlags::None);

void BKE_cpp_types_init()
{
  blender::register_cpp_types();
  FN_register_cpp_types();

  BLI_CPP_TYPE_REGISTER(blender::bke::GeometrySet);
  BLI_CPP_TYPE_REGISTER(blender::bke::InstanceReference);

  BLI_VECTOR_CPP_TYPE_REGISTER(blender::bke::GeometrySet);

  BLI_CPP_TYPE_REGISTER(Object *);
  BLI_CPP_TYPE_REGISTER(Collection *);
  BLI_CPP_TYPE_REGISTER(Tex *);
  BLI_CPP_TYPE_REGISTER(Image *);
  BLI_CPP_TYPE_REGISTER(Material *);

  BLI_CPP_TYPE_REGISTER(MStringProperty);

  BLI_CPP_TYPE_REGISTER(blender::bke::AnonymousAttributeSet);
}
