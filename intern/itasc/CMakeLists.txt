# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

remove_strict_flags()
set(INC

)

set(INC_SYS
)

set(SRC
  Armature.cpp
  Cache.cpp
  ConstraintSet.cpp
  ControlledObject.cpp
  CopyPose.cpp
  Distance.cpp
  FixedObject.cpp
  MovingFrame.cpp
  Scene.cpp
  UncontrolledObject.cpp
  WDLSSolver.cpp
  WSDLSSolver.cpp
  WorldObject.cpp
  eigen_types.cpp

  kdl/chain.cpp
  kdl/chainfksolverpos_recursive.cpp
  kdl/chainjnttojacsolver.cpp
  kdl/frameacc.cpp
  kdl/frames.cpp
  kdl/frames_io.cpp
  kdl/framevel.cpp
  kdl/inertia.cpp
  kdl/jacobian.cpp
  kdl/jntarray.cpp
  kdl/jntarrayacc.cpp
  kdl/jntarrayvel.cpp
  kdl/joint.cpp
  kdl/kinfam_io.cpp
  kdl/segment.cpp
  kdl/tree.cpp
  kdl/treefksolverpos_recursive.cpp
  kdl/treejnttojacsolver.cpp
  kdl/utilities/error_stack.cpp
  kdl/utilities/utility.cpp
  kdl/utilities/utility_io.cpp

  Armature.hpp
  Cache.hpp
  ConstraintSet.hpp
  ControlledObject.hpp
  CopyPose.hpp
  Distance.hpp
  FixedObject.hpp
  MovingFrame.hpp
  Object.hpp
  Scene.hpp
  Solver.hpp
  UncontrolledObject.hpp
  WDLSSolver.hpp
  WSDLSSolver.hpp
  WorldObject.hpp
  eigen_types.hpp
  kdl/chain.hpp
  kdl/chainfksolver.hpp
  kdl/chainfksolverpos_recursive.hpp
  kdl/chainjnttojacsolver.hpp
  kdl/frameacc.hpp
  kdl/frames.hpp
  kdl/frames_io.hpp
  kdl/framevel.hpp
  kdl/inertia.hpp
  kdl/jacobian.hpp
  kdl/jntarray.hpp
  kdl/jntarrayacc.hpp
  kdl/jntarrayvel.hpp
  kdl/joint.hpp
  kdl/kinfam_io.hpp
  kdl/segment.hpp
  kdl/tree.hpp
  kdl/treefksolver.hpp
  kdl/treefksolverpos_recursive.hpp
  kdl/treejnttojacsolver.hpp
  kdl/utilities/error.h
  kdl/utilities/error_stack.h
  kdl/utilities/kdl-config.h
  kdl/utilities/rall1d.h
  kdl/utilities/rall2d.h
  kdl/utilities/svd_eigen_HH.hpp
  kdl/utilities/traits.h
  kdl/utilities/utility.h
  kdl/utilities/utility_io.h

  kdl/frameacc.inl
  kdl/frames.inl
  kdl/framevel.inl
)

set(LIB
  PUBLIC bf::dependencies::eigen
)

blender_add_lib(bf_intern_itasc "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
