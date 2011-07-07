#!/usr/bin/python
import sys
import os

Import('env')

defs = 'V3DLIB_ENABLE_SUITESPARSE GOOGLE_GLOG_DLL_DECL='
cflags = []

src = env.Glob("*.cpp")
src += env.Glob('libmv/image/*.cc')
src += env.Glob('libmv/multiview/*.cc')
src += env.Glob('libmv/numeric/*.cc')
src += env.Glob('libmv/simple_pipeline/*.cc')
src += env.Glob('libmv/tracking/*.cc')
src += env.Glob('third_party/gflags/*.cc')
src += env.Glob('third_party/ldl/Source/*.c')
src += env.Glob('third_party/ssba/Geometry/*.cpp')
src += env.Glob('third_party/ssba/Math/*.cpp')

incs = '. ../Eigen3'

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc'):
    incs += ' ./third_party/glog/src/windows ./third_party/glog/src/windows/glog ./third_party/msinttypes'

    src += ['./third_party/glog/src/logging.cc', './third_party/glog/src/raw_logging.cc', './third_party/glog/src/utilities.cc', './third_party/glog/src/vlog_is_on.cc']
    src += ['./third_party/glog/src/windows/port.cc']
else:
    src += env.Glob("third_party/glog/src/*.cc")
    incs += ' ./third_party/glog/src'

incs += ' ./third_party/ssba ./third_party/ldl/Include ../colamd/Include'

env.BlenderLib ( libname = 'extern_libmv', sources=src, includes=Split(incs), defines=Split(defs), libtype=['extern', 'player'], priority=[20,137], compileflags=cflags )
