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
# The Original Code is Copyright (C) 2013, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Sergey Sharybin.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../guardedalloc
)

set(INC_SYS
	${OPENSUBDIV_INCLUDE_DIR}
	${GLEW_INCLUDE_PATH}
)

set(SRC
	internal/opensubdiv.cc
	internal/opensubdiv_converter_factory.cc
	internal/opensubdiv_converter_internal.cc
	internal/opensubdiv_converter_orient.cc
	internal/opensubdiv_device_context_cuda.cc
	internal/opensubdiv_device_context_opencl.cc
	internal/opensubdiv_evaluator.cc
	internal/opensubdiv_evaluator_internal.cc
	internal/opensubdiv_gl_mesh.cc
	internal/opensubdiv_gl_mesh_draw.cc
	internal/opensubdiv_gl_mesh_fvar.cc
	internal/opensubdiv_gl_mesh_internal.cc
	internal/opensubdiv_topology_refiner.cc
	internal/opensubdiv_topology_refiner_internal.cc
	internal/opensubdiv_util.cc

	internal/opensubdiv_converter_internal.h
	internal/opensubdiv_converter_orient.h
	internal/opensubdiv_converter_orient_impl.h
	internal/opensubdiv_device_context_cuda.h
	internal/opensubdiv_device_context_opencl.h
	internal/opensubdiv_evaluator_internal.h
	internal/opensubdiv_gl_mesh_fvar.h
	internal/opensubdiv_gl_mesh_internal.h
	internal/opensubdiv_internal.h
	internal/opensubdiv_topology_refiner_internal.h
	internal/opensubdiv_util.h

	opensubdiv_capi.h
	opensubdiv_capi_type.h
	opensubdiv_converter_capi.h
	opensubdiv_evaluator_capi.h
	opensubdiv_gl_mesh_capi.h
	opensubdiv_topology_refiner_capi.h
)

macro(OPENSUBDIV_DEFINE_COMPONENT component)
	if(${${component}})
		add_definitions(-D${component})
	endif()
endmacro()

OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENMP)
# TODO(sergey): OpenCL is not tested and totally unstable atm.
# OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENCL)
# TODO(sergey): CUDA stays disabled for util it's ported to drievr API.
# OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_CUDA)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_COMPUTE)

data_to_c_simple(shader/gpu_shader_opensubdiv_vertex.glsl SRC)
data_to_c_simple(shader/gpu_shader_opensubdiv_geometry.glsl SRC)
data_to_c_simple(shader/gpu_shader_opensubdiv_fragment.glsl SRC)

add_definitions(-DGLEW_STATIC)

if(WIN32)
	add_definitions(-DNOMINMAX)
	add_definitions(-D_USE_MATH_DEFINES)
endif()

# TODO(sergey): Put CUEW back when CUDA is officially supported by OSD.
#if(OPENSUBDIV_HAS_CUDA)
#	list(APPEND INC
#		../../extern/cuew/include
#	)
#	add_definitions(-DOPENSUBDIV_HAS_CUEW)
#endif()

if(OPENSUBDIV_HAS_OPENCL)
	list(APPEND INC
		../../extern/clew/include
	)
	add_definitions(-DOPENSUBDIV_HAS_CLEW)
endif()

blender_add_lib(bf_intern_opensubdiv "${SRC}" "${INC}" "${INC_SYS}")
