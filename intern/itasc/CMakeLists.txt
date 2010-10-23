# $Id: CMakeLists.txt 19905 2009-04-23 13:29:54Z ben2610 $
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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SET(INC
	../../extern/Eigen2
)

SET(SRC
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
)

BLENDERLIB(bf_intern_itasc "${SRC}" "${INC}")
