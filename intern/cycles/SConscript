#!/usr/bin/python
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
cxxflags = []

defs.append('CCL_NAMESPACE_BEGIN=namespace ccl {')
defs.append('CCL_NAMESPACE_END=}')

defs.append('WITH_OPENCL')
defs.append('WITH_MULTI')
defs.append('WITH_CUDA')

if env['WITH_BF_CYCLES_CUDA_BINARIES']:
    defs.append('WITH_CUDA_BINARIES')

incs.extend('. bvh render device kernel kernel/osl kernel/svm util subd'.split())
incs.extend('#intern/guardedalloc #source/blender/makesrna #source/blender/makesdna'.split())
incs.extend('#source/blender/blenloader ../../source/blender/makesrna/intern'.split())
incs.extend('#extern/glew/include'.split())
incs.append(cycles['BF_OIIO_INC'])
incs.append(cycles['BF_BOOST_INC'])
incs.append(cycles['BF_PYTHON_INC'])

if env['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
    cxxflags.append('-D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc'.split())
else:
    cxxflags.append('-ffast-math'.split())

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc'):
    incs.append(env['BF_PTHREADS_INC'])

# optimized kernel
if env['WITH_BF_RAYOPTIMIZATION']:
    optim_cxxflags = []

    if env['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
        optim_cxxflags.append('/arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc'.split())
    else:
        optim_cxxflags.append('-ffast-math -msse -msse2 -msse3 -mfpmath=sse'.split())
    
    defs.append('WITH_OPTIMIZED_KERNEL')
    optim_defs = defs[:]
    optim_sources = [path.join('kernel', 'kernel_optimized.cpp')]

    cycles_optim = cycles.Clone()
    cycles_optim.BlenderLib('bf_intern_cycles_optimized', optim_sources, incs, optim_defs, libtype=['intern'], priority=[10], compileflags=[None], cxx_compileflags=optim_cxxflags)

cycles.BlenderLib('bf_intern_cycles', sources, incs, defs, libtype=['intern'], priority=[0], compileflags=[None], cxx_compileflags=cxxflags)

# cuda kernel binaries
if env['WITH_BF_CYCLES_CUDA_BINARIES']:
    kernel_binaries = SConscript(['kernel/SConscript'])
    cycles.Depends("device/device_cuda.o", kernel_binaries)

