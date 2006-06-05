#!/usr/bin/python
import sys
import os

Import ('env')

window_system = env['OURPLATFORM']

sources = env.Glob('intern/*.cpp')

pf = ['GHOST_DisplayManager', 'GHOST_System', 'GHOST_Window']

if window_system == 'linux2':
    for f in pf:
        sources.remove('intern' + os.sep + f + 'Win32.cpp')
        sources.remove('intern' + os.sep + f + 'Carbon.cpp')
elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross'):
    for f in pf:
        sources.remove('intern' + os.sep + f + 'X11.cpp')
        sources.remove('intern' + os.sep + f + 'Carbon.cpp')
elif window_system == 'darwin':
    for f in pf:
        sources.remove('intern' + os.sep + f + 'Win32.cpp')
        sources.remove('intern' + os.sep + f + 'X11.cpp')
elif window_system == 'openbsd3':
    for f in pf:
        sources.remove('intern' + os.sep + f + 'Win32.cpp')
        sources.remove('intern' + os.sep + f + 'Carbon.cpp')
else:
    print "Unknown window system specified."
    Exit()

incs = '. ../string ' + env['BF_OPENGL_INC']
env.BlenderLib ('bf_ghost', sources, Split(incs), [], libtype=['core','player'], priority = [25,15] ) 
