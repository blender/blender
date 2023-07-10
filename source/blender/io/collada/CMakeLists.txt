# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

remove_strict_flags()
find_file(OPENCOLLADA_ANIMATION_CLIP
  NAMES
    COLLADAFWAnimationClip.h
  PATHS
    ${OPENCOLLADA_INCLUDE_DIRS}
  NO_DEFAULT_PATH
)

if(OPENCOLLADA_ANIMATION_CLIP)
  add_definitions(-DWITH_OPENCOLLADA_ANIMATION_CLIP)
endif()

# In CMAKE version 3.21 and up, we can instead use the NO_CACHE option for
# find_file so we don't need to clear it from the cache here.
unset(OPENCOLLADA_ANIMATION_CLIP CACHE)

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../editors/include
  ../../imbuf
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../ikplugin
  ../../../../intern/iksolver/extern
  ../../bmesh
)

set(INC_SYS
  ${OPENCOLLADA_INCLUDE_DIRS}
)

set(SRC
  AnimationClipExporter.cpp
  AnimationExporter.cpp
  AnimationImporter.cpp
  ArmatureExporter.cpp
  ArmatureImporter.cpp
  BCAnimationCurve.cpp
  BCAnimationSampler.cpp
  BCMath.cpp
  BCSampleData.cpp
  BlenderContext.cpp
  CameraExporter.cpp
  ControllerExporter.cpp
  DocumentExporter.cpp
  DocumentImporter.cpp
  EffectExporter.cpp
  ErrorHandler.cpp
  ExportSettings.cpp
  ExtraHandler.cpp
  ExtraTags.cpp
  GeometryExporter.cpp
  ImageExporter.cpp
  ImportSettings.cpp
  InstanceWriter.cpp
  LightExporter.cpp
  MaterialExporter.cpp
  Materials.cpp
  MeshImporter.cpp
  SceneExporter.cpp
  SkinInfo.cpp
  TransformReader.cpp
  TransformWriter.cpp
  collada.cpp
  collada_internal.cpp
  collada_utils.cpp

  AnimationClipExporter.h
  AnimationExporter.h
  AnimationImporter.h
  ArmatureExporter.h
  ArmatureImporter.h
  BCAnimationCurve.h
  BCAnimationSampler.h
  BCMath.h
  BCSampleData.h
  BlenderContext.h
  BlenderTypes.h
  CameraExporter.h
  ControllerExporter.h
  DocumentExporter.h
  DocumentImporter.h
  EffectExporter.h
  ErrorHandler.h
  ExportSettings.h
  ExtraHandler.h
  ExtraTags.h
  GeometryExporter.h
  ImageExporter.h
  ImportSettings.h
  InstanceWriter.h
  LightExporter.h
  MaterialExporter.h
  Materials.h
  MeshImporter.h
  SceneExporter.h
  SkinInfo.h
  TransformReader.h
  TransformWriter.h
  collada.h
  collada_internal.h
  collada_utils.h
)

set(LIB
  ${OPENCOLLADA_LIBRARIES}
  ${PCRE_LIBRARIES}
  ${XML2_LIBRARIES}
  PRIVATE bf::dna
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

blender_add_lib(bf_collada "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
