# Blender library functions

import sys
import os
import string
import SCons

import bs_globals

def common_libs(env):
	"""
	Append to env all libraries that are common to Blender and Blenderplayer
	"""
	env.Append (LIBS=[
		'blender_readblenfile',
		'blender_img',
		'blender_blenkernel',
		'blender_blenloader',
		'blender_blenpluginapi',
		'blender_imbuf',
		'blender_avi',
		'blender_blenlib',
		'blender_makesdna',
		'blender_kernel',
		'blender_GHOST',
		'blender_STR',
		'blender_guardedalloc',
		'blender_CTR',
		'blender_MEM',
		'blender_MT',
		'blender_BMF',
		'soundsystem'])
	if bs_globals.user_options_dict['USE_QUICKTIME'] == 1:
		env.Append (LIBS=['blender_quicktime'])

def international_libs(env):
	"""
	Append international font support libraries
	"""
	if bs_globals.user_options_dict['USE_INTERNATIONAL'] == 1:
		env.Append (LIBS=bs_globals.user_options_dict['FREETYPE_LIBRARY'])
		env.Append (LIBPATH=bs_globals.user_options_dict['FREETYPE_LIBPATH'])
		env.Append (LIBS=['blender_FTF'])
		env.Append (LIBS=bs_globals.user_options_dict['FTGL_LIBRARY'])
		env.Append (LIBPATH=bs_globals.user_options_dict['FTGL_LIBPATH'])
		env.Append (LIBS=bs_globals.user_options_dict['FREETYPE_LIBRARY'])

def blender_libs(env):
	"""
	Blender only libs (not in player)
	"""
	env.Append( LIBS=['blender_creator',
		'blender_blendersrc',
		'blender_render',
		'blender_yafray',
		'blender_renderconverter',
		'blender_radiosity',
		'blender_LOD',
		'blender_BSP',
		'blender_blenkernel',
		'blender_IK',
		'blender_ONL'])

def ketsji_libs(env):
	"""
	Game Engine libs
	"""
	if bs_globals.user_options_dict['BUILD_GAMEENGINE'] == 1:
		env.Append (LIBS=['KX_blenderhook',
				'KX_converter',
				'PHY_Dummy',
				'PHY_Physics',
				'KX_ketsji',
				'SCA_GameLogic',
				'RAS_rasterizer',
				'RAS_OpenGLRasterizer',
				'blender_expressions',
				'SG_SceneGraph',
				'blender_MT',
				'KX_blenderhook',
				'KX_network',
				'blender_kernel',
				'NG_network',
				'NG_loopbacknetwork'])
		if bs_globals.user_options_dict['USE_PHYSICS'] == 'solid':
			env.Append (LIBS=['PHY_Sumo', 'PHY_Physics', 'blender_MT', 'extern_solid', 'extern_qhull'])
		else:
			env.Append (LIBS=['PHY_Ode',
					'PHY_Physics'])
			env.Append (LIBS=bs_globals.user_options_dict['ODE_LIBRARY'])
			env.Append (LIBPATH=bs_globals.user_options_dict['ODE_LIBPATH'])

def player_libs(env):
	"""
	Player libraries
	"""
	env.Append (LIBS=['GPG_ghost',
			'GPC_common'])

def player_libs2(env):
	"""
	Link order shenannigans: these libs are added after common_libs
	"""
	env.Append (LIBS=['blender_blenkernel_blc',
			'soundsystem'])

def winblenderres(env):
	"""
	build the windows icon resource file
	"""
	if sys.platform == 'win32':
		env.RES(['source/icons/winblender.rc'])

def system_libs(env):
	"""
	System libraries: Python, SDL, PNG, JPEG, Gettext, OpenAL, Carbon
	"""
	env.Append (LIBS=['blender_python'])
	env.Append (LIBS=bs_globals.user_options_dict['PYTHON_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['PYTHON_LIBPATH'])
	env.Append (LINKFLAGS=bs_globals.user_options_dict['PYTHON_LINKFLAGS'])
	env.Append (LIBS=bs_globals.user_options_dict['SDL_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['SDL_LIBPATH'])
	env.Append (LIBS=bs_globals.user_options_dict['PNG_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['PNG_LIBPATH'])
	env.Append (LIBS=bs_globals.user_options_dict['JPEG_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['JPEG_LIBPATH'])
	env.Append (LIBS=bs_globals.user_options_dict['GETTEXT_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['GETTEXT_LIBPATH'])
	env.Append (LIBS=bs_globals.user_options_dict['Z_LIBRARY'])
	env.Append (LIBPATH=bs_globals.user_options_dict['Z_LIBPATH'])
	if bs_globals.user_options_dict['USE_OPENAL'] == 1:
		env.Append (LIBS=bs_globals.user_options_dict['OPENAL_LIBRARY'])
		env.Append (LIBPATH=bs_globals.user_options_dict['OPENAL_LIBPATH'])
	env.Append (LIBS=bs_globals.user_options_dict['PLATFORM_LIBS'])
	env.Append (LIBPATH=bs_globals.user_options_dict['PLATFORM_LIBPATH'])
	if sys.platform == 'darwin':
		env.Append (LINKFLAGS='-framework')
		env.Append (LINKFLAGS='Carbon')
		env.Append (LINKFLAGS='-framework')
		env.Append (LINKFLAGS='AGL')
		env.Append (LINKFLAGS='-framework')
		env.Append (LINKFLAGS='AudioUnit')
		env.Append (LINKFLAGS='-framework')
		env.Append (LINKFLAGS='AudioToolbox')
		env.Append (LINKFLAGS='-framework')
		env.Append (LINKFLAGS='CoreAudio')
		if bs_globals.user_options_dict['USE_QUICKTIME'] == 1:
			env.Append (LINKFLAGS='-framework')
			env.Append (LINKFLAGS='QuickTime')
	else:
		env.Append (LINKFLAGS=bs_globals.user_options_dict['PLATFORM_LINKFLAGS'])
	env.BuildDir (bs_globals.root_build_dir, '.', duplicate=0)


