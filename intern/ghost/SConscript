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
elif window_system in ('linux', 'openbsd3', 'sunos5', 'freebsd7', 'freebsd8', 'freebsd9', 'aix4', 'aix5'):
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'Win32.cpp')
            sources.remove('intern' + os.sep + f + 'Carbon.cpp')
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass
    ## removing because scons does not support system installation
    ## if this is used for blender.org builds it means our distrobution
    ## will find any locally installed blender and double up its script path.
    ## So until this is supported properly as with CMake,
    ## just dont use the PREFIX.
    # defs += ['PREFIX=\\"/usr/local/\\"']  # XXX, make an option
    defs += ['WITH_X11_XINPUT']  # XXX, make an option

    # freebsd doesn't seem to support XDND protocol
    if env['WITH_GHOST_XDND'] and window_system not in ('freebsd7', 'freebsd8', 'freebsd9'):
        incs += ' #/extern/xdnd'
        defs += ['WITH_XDND']
    else:
        sources.remove('intern' + os.sep + 'GHOST_DropTargetX11.cpp')

elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc', 'win64-mingw'):
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

    if env['OURPLATFORM']=='linux':
        incs += ' ' + env['BF_3DMOUSE_INC']
else:
    sources.remove('intern' + os.sep + 'GHOST_NDOFManager.cpp')
    try:
        if window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc', 'win64-mingw'):
            sources.remove('intern' + os.sep + 'GHOST_NDOFManagerWin32.cpp')
        elif window_system=='darwin':
            sources.remove('intern' + os.sep + 'GHOST_NDOFManagerCocoa.mm')
        else:
            sources.remove('intern' + os.sep + 'GHOST_NDOFManagerX11.cpp')
    except ValueError:
        pass


if window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc', 'win64-mingw'):
    incs = env['BF_WINTAB_INC'] + ' ' + incs
    incs += ' ../utfconv'

if window_system in ('win32-vc', 'win64-vc'):
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15]) #, cc_compileflags=env['CCFLAGS'].append('/WX') )

elif env['WITH_GHOST_COCOA']:	 # always use default-Apple-gcc for objC language, for gnu-compilers do not support it fully yet
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15], cc_compilerchange='/usr/bin/gcc', cxx_compilerchange='/usr/bin/g++' )
    print "GHOST COCOA WILL BE COMPILED WITH APPLE GCC"

else:
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15] )
