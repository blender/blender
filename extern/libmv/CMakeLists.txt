# $Id$
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2011 Blender Foundation.
# All rights reserved.
#
# Contributor(s): Blender Foundation,
#                 Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../Eigen3
)

set(INC_SYS

)

set(SRC
	libmv-capi.cpp
	libmv/numeric/numeric.cc
	libmv/numeric/poly.cc
	libmv/numeric/tinyvector.cc
	libmv/image/convolve.cc
	libmv/image/array_nd.cc
	libmv/tracking/pyramid_region_tracker.cc
	libmv/tracking/trklt_region_tracker.cc
	libmv/tracking/klt_region_tracker.cc
	libmv/tracking/retrack_region_tracker.cc

	libmv-capi.h
	libmv/logging/logging.h
	libmv/numeric/dogleg.h
	libmv/numeric/levenberg_marquardt.h
	libmv/numeric/poly.h
	libmv/numeric/function_derivative.h
	libmv/numeric/numeric.h
	libmv/image/convolve.h
	libmv/image/tuple.h
	libmv/image/array_nd.h
	libmv/image/sample.h
	libmv/image/image.h
	libmv/tracking/region_tracker.h
	libmv/tracking/retrack_region_tracker.h
	libmv/tracking/pyramid_region_tracker.h
	libmv/tracking/trklt_region_tracker.h
	libmv/tracking/klt_region_tracker.h
	libmv/base/id_generator.h
	libmv/base/vector.h
	libmv/base/scoped_ptr.h
	libmv/base/vector_utils.h
)

blender_add_lib(extern_libmv "${SRC}" "${INC}" "${INC_SYS}")
