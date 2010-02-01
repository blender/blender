#!/usr/bin/python
import sys
import os

Import ('env')

window_system = env['OURPLATFORM']

sources = env.Glob('intern/*.cpp')
if window_system == 'darwin':
	sources += env.Glob('intern/*.mm')


pf = ['GHOST_DisplayManager', 'GHOST_System', 'GHOST_Window', 'GHOST_DropTarget']
defs=['_USE_MATH_DEFINES']

if window_system in ('linux2', 'openbsd3', 'sunos5', 'freebsd6', 'irix6', 'aix4', 'aix5'):
	for f in pf:
		try:
			sources.remove('intern' + os.sep + f + 'Win32.cpp')
			sources.remove('intern' + os.sep + f + 'Carbon.cpp')
		except ValueError:
			pass
elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
	for f in pf:
		try:
			sources.remove('intern' + os.sep + f + 'X11.cpp')
			sources.remove('intern' + os.sep + f + 'Carbon.cpp')
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
			except ValueError:
				pass
	else:
		for f in pf:
			try:
				sources.remove('intern' + os.sep + f + 'Win32.cpp')
				sources.remove('intern' + os.sep + f + 'X11.cpp')
				sources.remove('intern' + os.sep + f + 'Cocoa.mm')
			except ValueError:
				pass

else:
	print "Unknown window system specified."
	Exit()

if env['BF_GHOST_DEBUG']:
	defs.append('BF_GHOST_DEBUG')
	
incs = '. ../string #extern/glew/include #source/blender/imbuf #source/blender/makesdna ' + env['BF_OPENGL_INC']
if window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
	incs = env['BF_WINTAB_INC'] + ' ' + incs
env.BlenderLib ('bf_ghost', sources, Split(incs), defines=defs, libtype=['intern','player'], priority = [40,15] ) 

