#!/usr/bin/python
import sys
import os

Import ('env')

window_system = env['OURPLATFORM']

sources = env.Glob('intern/*.cpp')
if window_system == 'darwin':
	sources += env.Glob('intern/*.mm')

if env['WITH_GHOST_COCOA'] == True:
	env.Append(CFLAGS=['-DGHOST_COCOA']) 
	env.Append(CXXFLAGS=['-DGHOST_COCOA'])
	env.Append(CPPFLAGS=['-DGHOST_COCOA'])

#defs = ''
#if env['WITH_GHOST_COCOA']:
#	defs += 'GHOST_COCOA'
# maybe we need it later

pf = ['GHOST_DisplayManager', 'GHOST_System', 'GHOST_Window']

if window_system in ('linux2', 'openbsd3', 'sunos5', 'freebsd6', 'irix6'):
	for f in pf:
		sources.remove('intern' + os.sep + f + 'Win32.cpp')
		sources.remove('intern' + os.sep + f + 'Carbon.cpp')
elif window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
	for f in pf:
		sources.remove('intern' + os.sep + f + 'X11.cpp')
		sources.remove('intern' + os.sep + f + 'Carbon.cpp')
elif window_system == 'darwin':
	if env['WITH_GHOST_COCOA']:
	for f in pf:
		sources.remove('intern' + os.sep + f + 'Win32.cpp')
		sources.remove('intern' + os.sep + f + 'X11.cpp')
		sources.remove('intern' + os.sep + f + 'Carbon.cpp')
	else:
		for f in pf:
			sources.remove('intern' + os.sep + f + 'Win32.cpp')
			sources.remove('intern' + os.sep + f + 'X11.cpp')
			sources.remove('intern' + os.sep + f + 'Cocoa.mm')

else:
	print "Unknown window system specified."
	Exit()

incs = '. ../string ' + env['BF_OPENGL_INC']
if window_system in ('win32-vc', 'win32-mingw', 'cygwin', 'linuxcross', 'win64-vc'):
	incs = env['BF_WINTAB_INC'] + ' ' + incs
env.BlenderLib ('bf_ghost', sources, Split(incs), defines=['_USE_MATH_DEFINES'], libtype=['intern','player'], priority = [40,15] ) 

