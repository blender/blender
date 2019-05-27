# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

remove_strict_flags()
FIND_FILE(OPENCOLLADA_ANIMATION_CLIP
      NAMES
        COLLADAFWAnimationClip.h
      PATHS
        ${OPENCOLLADA_INCLUDE_DIRS}
      NO_DEFAULT_PATH
    )

IF(OPENCOLLADA_ANIMATION_CLIP)
   message(STATUS "Found opencollada: ${OPENCOLLADA_ANIMATION_CLIP} ")
   add_definitions(-DWITH_OPENCOLLADA_ANIMATION_CLIP)
ELSE()
   message(STATUS "opencollada: Build without animation clip support")
ENDIF()

set(INC
  .
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../depsgraph
  ../editors/include
  ../imbuf
  ../makesdna
  ../makesrna
  ../windowmanager
  ../../../intern/guardedalloc
  ../ikplugin
  ../../../intern/iksolver/extern
  ../bmesh
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
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  # COLLADAFWArray.h gives error with gcc 4.5
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

blender_add_lib(bf_collada "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
