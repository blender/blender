#!/usr/bin/python
import sys
import os

Import ('env')

window_system = env['OURPLATFORM']

sources = env.Glob('intern/*.cpp')
if window_system == 'darwin':
    sources += env.Glob('intern/*.mm')


pf = ['GHOST_DisplayManager', 'GHOST_System', 'GHOST_SystemPaths', 'GHOST_Window', 'GHOST_DropTarget', 'GHOST_NDOFManager']
defs=['_USE_MATH_DEFINES']

incs = '. ../string #extern/glew/include #source/blender/imbuf #source/blender/makesdna ' + env['BF_OPENGL_INC']

if env['WITH_GHOST_SDL']:
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'Carbon.cpp')
            sources.remove('intern' + os.sep + f + 'Win32.cpp')
            sources.remove('intern' + os.sep + f + 'X11.cpp')
        except ValueError:
            pass
    incs += ' ' + env['BF_SDL_INC']
    defs += ['WITH_GHOST_SDL']
elif window_system in ('linux2', 'openbsd3', 'sunos5', 'freebsd7', 'freebsd8', 'freebsd9', 'irix6', 'aix4', 'aix5'):
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'Win32.cpp')
            sources.remove('intern' + os.sep + f + 'Carbon.cpp')
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass
    defs += ['PREFIX=\\"/usr/local/\\"']  # XXX, make an option
    defs += ['WITH_X11_XINPUT']  # XXX, make an option

elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'X11.cpp')
            sources.remove('intern' + os.sep + f + 'Carbon.cpp')
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass
elif window_system == 'darwin':
    if env['WITH_GHOST_COCOA']:
        if env['WITH_BF_QUICKTIME']:
            defs.append('WITH_QUICKTIME')
        if env['USE_QTKIT']:
            defs.append('USE_QTKIT')
        for f in pf:
            try:
                sources.remove('intern' + os.sep + f + 'Win32.cpp')
                sources.remove('intern' + os.sep + f + 'X11.cpp')
                sources.remove('intern' + os.sep + f + 'Carbon.cpp')
                sources.remove('intern' + os.sep + f + 'SDL.cpp')
            except ValueError:
                pass
    else:
        for f in pf:
            try:
                sources.remove('intern' + os.sep + f + 'Win32.cpp')
                sources.remove('intern' + os.sep + f + 'X11.cpp')
                sources.remove('intern' + os.sep + f + 'Cocoa.mm')
                sources.remove('intern' + os.sep + f + 'SDL.cpp')
            except ValueError:
                pass

else:
    print "Unknown window system specified."
    Exit()

if env['BF_GHOST_DEBUG']:
    defs.append('WITH_GHOST_DEBUG')
else:
	sources.remove('intern' + os.sep + 'GHOST_EventPrinter.cpp')

if env['WITH_BF_3DMOUSE']:
	defs.append('WITH_INPUT_NDOF')

if window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
    incs = env['BF_WINTAB_INC'] + ' ' + incs

if window_system in ('win32-vc', 'win64-vc'):
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15]) #, cc_compileflags=env['CCFLAGS'].append('/WX') ) 
else:
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15] ) 
