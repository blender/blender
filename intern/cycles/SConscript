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
sources.remove(path.join('kernel', 'kernel_sse2.cpp'))
sources.remove(path.join('kernel', 'kernel_sse3.cpp'))
sources.remove(path.join('kernel', 'kernel_sse41.cpp'))
sources.remove(path.join('kernel', 'kernel_avx.cpp'))

incs = [] 
defs = []
cxxflags = Split(env['CXXFLAGS'])

defs.append('GLEW_STATIC')

defs.append('CCL_NAMESPACE_BEGIN=namespace ccl {')
defs.append('CCL_NAMESPACE_END=}')

defs.append('WITH_OPENCL')
defs.append('WITH_MULTI')
defs.append('WITH_CUDA')

if env['WITH_BF_CYCLES_OSL']:
    defs.append('WITH_OSL')
    defs.append('OSL_STATIC_LIBRARY')
    incs.append(cycles['BF_OSL_INC'])

incs.extend('. bvh render device kernel kernel/osl kernel/svm util subd'.split())
incs.extend('#intern/guardedalloc #source/blender/makesrna #source/blender/makesdna #source/blender/blenlib'.split())
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
sse2_cxxflags = Split(env['CXXFLAGS'])
sse3_cxxflags = Split(env['CXXFLAGS'])
sse41_cxxflags = Split(env['CXXFLAGS'])
avx_cxxflags = Split(env['CXXFLAGS'])

if env['OURPLATFORM'] == 'win32-vc':
    # there is no /arch:SSE3, but intrinsics are available anyway
    sse2_cxxflags.append('/arch:SSE /arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    sse3_cxxflags.append('/arch:SSE /arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    sse41_cxxflags.append('/arch:SSE /arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    avx_cxxflags.append('/arch:SSE /arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split()) #/arch:AVX for VC2012 and above
elif env['OURPLATFORM'] == 'win64-vc':
    sse2_cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    sse3_cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    sse41_cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split())
    avx_cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /Ox /Gs-'.split()) #/arch:AVX for VC2012 and above
else:
    sse2_cxxflags.append('-ffast-math -msse -msse2 -mfpmath=sse'.split())
    sse3_cxxflags.append('-ffast-math -msse -msse2 -msse3 -mssse3 -mfpmath=sse'.split())
    sse41_cxxflags.append('-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mfpmath=sse'.split())
    avx_cxxflags.append('-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mfpmath=sse'.split())

optim_defs = defs[:]

cycles_avx = cycles.Clone()
avx_sources = [path.join('kernel', 'kernel_avx.cpp')]
if env['OURPLATFORM'] == 'darwin' and env['C_COMPILER_ID'] == 'gcc' and  env['CCVERSION'] >= '4.6': # use Apple assembler for avx , gnu-compilers do not support it ( gnu gcc-4.6 or higher case )
    cycles_avx.BlenderLib('bf_intern_cycles_avx', avx_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=avx_cxxflags, cc_compilerchange='/usr/bin/clang', cxx_compilerchange='/usr/bin/clang++')
else:
    cycles_avx.BlenderLib('bf_intern_cycles_avx', avx_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=avx_cxxflags)

cycles_sse41 = cycles.Clone()
sse41_sources = [path.join('kernel', 'kernel_sse41.cpp')]
cycles_sse41.BlenderLib('bf_intern_cycles_sse41', sse41_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=sse41_cxxflags)

cycles_sse3 = cycles.Clone()
sse3_sources = [path.join('kernel', 'kernel_sse3.cpp')]
cycles_sse3.BlenderLib('bf_intern_cycles_sse3', sse3_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=sse3_cxxflags)

cycles_sse2 = cycles.Clone()
sse2_sources = [path.join('kernel', 'kernel_sse2.cpp')]
cycles_sse2.BlenderLib('bf_intern_cycles_sse2', sse2_sources, incs, optim_defs, libtype=['intern'], priority=[10], cxx_compileflags=sse2_cxxflags)

cycles.BlenderLib('bf_intern_cycles', sources, incs, defs, libtype=['intern'], priority=[0], cxx_compileflags=cxxflags)

if env['WITH_BF_CYCLES_OSL']:
    oso_files = SConscript(['kernel/shaders/SConscript'])
    cycles.Depends("kernel/osl/osl_shader.o", oso_files)

    SConscript(['kernel/osl/SConscript'])

# cuda kernel binaries
if env['WITH_BF_CYCLES_CUDA_BINARIES']:
    kernel_binaries = SConscript(['kernel/SConscript'])
    cycles.Depends("device/device_cuda.o", kernel_binaries)

