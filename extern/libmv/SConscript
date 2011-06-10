#!/usr/bin/python
import sys
import os

Import('env')

defs = ' -DUSE_FORTRAN_BLAS -DNOGUI'
cflags = []

src = env.Glob("*.cpp")
src += env.Glob('libmv/numeric/*.cc')
src += env.Glob('libmv/image/*.cc')
src += env.Glob('libmv/tracking/*.cc')

incs = '. ../Eigen3 '

env.BlenderLib ( libname = 'extern_libmv', sources=src, includes=Split(incs), defines=Split(defs), libtype=['extern', 'player'], priority=[20,137], compileflags=cflags )
