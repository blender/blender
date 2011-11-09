#!/usr/bin/python
from os import path
Import('env')

cycles = env.Clone()

cycles.Depends('../../source/blender/makesrna/intern/RNA_blender_cpp.h', 'makesrna')

sources = cycles.Glob('bvh/*.cpp') + cycles.Glob('device/*.cpp') + cycles.Glob('kernel/*.cpp') + cycles.Glob('render/*.cpp') + cycles.Glob('subd/*.cpp') + cycles.Glob('util/*.cpp') + cycles.Glob('util/*.c') + cycles.Glob('blender/*.cpp')

sources.remove(path.join('util', 'util_view.cpp'))
sources.remove(path.join('render', 'film_response.cpp'))

incs = [] 
defs = []
ccflags = []
cxxflags = []

defs.append('CCL_NAMESPACE_BEGIN=namespace ccl {')
defs.append('CCL_NAMESPACE_END=}')

defs.append('WITH_OPENCL')
defs.append('WITH_MULTI')
defs.append('WITH_CUDA')

if env['OURPLATFORM'] in ('win32-mingw'):
	cxxflags.append('-fno-rtti -ffast-math -msse -msse2 -msse3 -mtune=native'.split())
	ccflags.append('-ffast-math -msse -msse2 -msse3 -mtune=native'.split())
	defs.append('BOOST_NO_RTTI BOOST_NO_TYPEID'.split())
	
incs.extend('. bvh render device kernel kernel/osl kernel/svm util subd'.split())
incs.extend('#intern/guardedalloc #source/blender/makesrna #source/blender/makesdna'.split())
incs.extend('#source/blender/blenloader ../../source/blender/makesrna/intern'.split())
incs.extend('#extern/glew/include'.split())
incs.append(cycles['BF_OIIO_INC'])
incs.append(cycles['BF_BOOST_INC'])
incs.append(cycles['BF_PYTHON_INC'])

cycles.BlenderLib('bf_intern_cycles', sources, incs, defs, libtype=['intern'], priority=[0], compileflags=[None], cc_compileflags=ccflags, cxx_compileflags=cxxflags)

