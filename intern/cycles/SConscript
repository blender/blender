#!/usr/bin/env python
#
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
# The Original Code is Copyright (C) 2011, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Nathan Letwory.
#
# ***** END GPL LICENSE BLOCK *****

from os import path
Import('env')

cycles = env.Clone()

cycles.Depends('../../source/blender/makesrna/intern/RNA_blender_cpp.h', 'makesrna')

sources = cycles.Glob('bvh/*.cpp') + cycles.Glob('device/*.cpp') + cycles.Glob('kernel/*.cpp') + cycles.Glob('render/*.cpp') + cycles.Glob('subd/*.cpp') + cycles.Glob('util/*.cpp') + cycles.Glob('blender/*.cpp')

sources.remove(path.join('util', 'util_view.cpp'))
sources.remove(path.join('render', 'film_response.cpp'))
sources.remove(path.join('kernel', 'kernel_optimized.cpp'))

incs = [] 
defs = []
cxxflags = Split(env['CXXFLAGS'])

defs.append('CCL_NAMESPACE_BEGIN=namespace ccl {')
defs.append('CCL_NAMESPACE_END=}')

defs.append('WITH_OPENCL')
defs.append('WITH_MULTI')
defs.append('WITH_CUDA')

if env['WITH_BF_CYCLES_OSL']:
    defs.append('WITH_OSL')
    incs.append(cycles['BF_OSL_INC'])

if env['WITH_BF_CYCLES_CUDA_BINARIES']:
    defs.append('WITH_CUDA_BINARIES')

incs.extend('. bvh render device kernel kernel/osl kernel/svm util subd'.split())
incs.extend('#intern/guardedalloc #source/blender/makesrna #source/blender/makesdna'.split())
incs.extend('#source/blender/blenloader ../../source/blender/makesrna/intern'.split())
incs.extend('#extern/glew/include #intern/mikktspace'.split())
incs.append(cycles['BF_OIIO_INC'])
incs.append(cycles['BF_BOOST_INC'])
incs.append(cycles['BF_OPENEXR_INC'].split())
incs.extend(cycles['BF_PYTHON_INC'].split())

if env['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
    cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc'.split())
else:
    cxxflags.append('-ffast-math'.split())

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc', 'win64-mingw'):
    incs.append(env['BF_PTHREADS_INC'])

# optimized kernel
if env['WITH_BF_RAYOPTIMIZATION']:
    optim_cxxflags = Split(env['CXXFLAGS'])

    if env['OURPLATFORM'] == 'win32-vc':
        optim_cxxflags.append('/arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc'.split())
    elif env['OURPLATFORM'] == 'win64-vc':
        optim_cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc'.split())
    else:
        optim_cxxflags.append('-ffast-math -msse -msse2 -msse3 -mfpmath=sse'.split())
    
    defs.append('WITH_OPTIMIZED_KERNEL')
    optim_defs = defs[:]
    optim_sources = [path.join('kernel', 'kernel_optimized.cpp')]

    cycles_optim = cycles.Clone()
    cycles_optim.BlenderLib('bf_intern_cycles_optimized', optim_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=optim_cxxflags)

cycles.BlenderLib('bf_intern_cycles', sources, incs, defs, libtype=['intern'], priority=[0], cxx_compileflags=cxxflags)

if env['WITH_BF_CYCLES_OSL']:
    oso_files = SConscript(['kernel/shaders/SConscript'])
    cycles.Depends("kernel/osl/osl_shader.o", oso_files)

    SConscript(['kernel/osl/SConscript'])

# cuda kernel binaries
if env['WITH_BF_CYCLES_CUDA_BINARIES']:
    kernel_binaries = SConscript(['kernel/SConscript'])
    cycles.Depends("device/device_cuda.o", kernel_binaries)

