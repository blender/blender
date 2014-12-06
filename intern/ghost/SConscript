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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Nathan Letwory.
#
# ***** END GPL LICENSE BLOCK *****

import sys
import os

Import ('env')

window_system = env['OURPLATFORM']

sources = env.Glob('intern/*.cpp')
sources2 = env.Glob('intern/GHOST_NDOFManager3Dconnexion.c')
if window_system == 'darwin':
    sources += env.Glob('intern/*.mm')
    #remove, will be readded below if needed.
    sources.remove('intern' + os.sep + 'GHOST_ContextCGL.mm')

if not env['WITH_BF_GL_EGL']:
    sources.remove('intern' + os.sep + 'GHOST_ContextEGL.cpp')

# seems cleaner to remove these now then add back the one that is needed
sources.remove('intern' + os.sep + 'GHOST_ContextGLX.cpp')
sources.remove('intern' + os.sep + 'GHOST_ContextWGL.cpp')

pf = ['GHOST_DisplayManager', 'GHOST_System', 'GHOST_SystemPaths', 'GHOST_Window', 'GHOST_DropTarget', 'GHOST_NDOFManager', 'GHOST_Context']

defs = env['BF_GL_DEFINITIONS']

if env['WITH_BF_GL_EGL']:
    defs.append('WITH_EGL')

incs = [
    '.',
    env['BF_GLEW_INC'],
    '../glew-mx',
    '#source/blender/imbuf',
    '#source/blender/makesdna',
    '../string',
    ]
incs = ' '.join(incs)

if env['WITH_GHOST_SDL']:
    for f in pf:
        try:
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
        except ValueError:
            pass

        try:
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass

    defs += ['WITH_X11']

    ## removing because scons does not support system installation
    ## if this is used for blender.org builds it means our distrobution
    ## will find any locally installed blender and double up its script path.
    ## So until this is supported properly as with CMake,
    ## just dont use the PREFIX.
    # defs += ['PREFIX=\\"/usr/local/\\"']  # XXX, make an option
    if env['WITH_X11_XINPUT']:
        defs += ['WITH_X11_XINPUT']

    if env['WITH_X11_XF86VMODE']:
        #incs += env['X11_xf86vmode_INCLUDE_PATH']
        defs += ['WITH_X11_XF86VMODE']

    # freebsd doesn't seem to support XDND protocol
    if env['WITH_GHOST_XDND'] and window_system not in ('freebsd7', 'freebsd8', 'freebsd9'):
        incs += ' #/extern/xdnd'
        defs += ['WITH_XDND']
    else:
        sources.remove('intern' + os.sep + 'GHOST_DropTargetX11.cpp')

    if not env['WITH_BF_GL_EGL']:
        sources.append('intern' + os.sep + 'GHOST_ContextGLX.cpp')

elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc', 'win64-mingw'):
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'X11.cpp')
        except ValueError:
            pass

        try:
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass

    if not env['WITH_BF_GL_EGL']:
        sources.append('intern' + os.sep + 'GHOST_ContextWGL.cpp')

elif window_system == 'darwin':
    if env['WITH_BF_QUICKTIME']:
        defs.append('WITH_QUICKTIME')
    for f in pf:
        try:
            sources.remove('intern' + os.sep + f + 'Win32.cpp')
        except ValueError:
            pass

        try:
            sources.remove('intern' + os.sep + f + 'X11.cpp')
        except ValueError:
            pass
        try:
            sources.remove('intern' + os.sep + f + 'SDL.cpp')
        except ValueError:
            pass

    if not env['WITH_BF_GL_EGL']:
        sources.append('intern' + os.sep + 'GHOST_ContextCGL.mm')

else:
    print "Unknown window system specified."
    Exit()

if env['BF_GHOST_DEBUG']:
    defs.append('WITH_GHOST_DEBUG')
else:
    sources.remove('intern' + os.sep + 'GHOST_EventPrinter.cpp')

if env['WITH_BF_IME']:
    if window_system in ('win32-vc', 'win32-mingw', 'win64-vc', 'win64-mingw'):
        defs.append('WITH_INPUT_IME')
    else:
        sources.remove('intern' + os.sep + 'GHOST_ImeWin32.h')
        sources.remove('intern' + os.sep + 'GHOST_ImeWin32.cpp')

if env['WITH_BF_3DMOUSE']:
    defs.append('WITH_INPUT_NDOF')

    if env['OURPLATFORM'] in ('linux','darwin'):
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

elif window_system == 'darwin' and env['C_COMPILER_ID'] == 'gcc' and  env['CCVERSION'] >= '4.6': # always use default-Apple-gcc for objC language, for gnu-compilers do not support it fully yet
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15], cc_compilerchange='/usr/bin/gcc', cxx_compilerchange='/usr/bin/g++' )
    print "GHOST COCOA WILL BE COMPILED WITH APPLE GCC"

else:
    env.BlenderLib ('bf_intern_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15] )
    
if window_system == 'darwin' and env['WITH_BF_3DMOUSE']: # build seperate to circumvent extern "C" linkage issues
    env.BlenderLib ('bf_intern_ghostndof3dconnexion', sources2, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15] )

