import string
import os
import sys
from distutils import sysconfig

# Setting up default environment variables for all platforms

sdl_cenv = Environment ()
sdl_lenv = Environment ()
link_env = Environment ()
env = Environment ()

if sys.platform == 'linux2':
    use_international = 'true'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char']
    cxxflags = []
    defines = []
    warn_flags = ['-Wall', '-W']
    window_system = 'X11'
    platform_libs = ['m', 'z', 'GL', 'GLU', 'png', 'jpeg', 'util']
    platform_libpath = []
    platform_linkflags = []
    extra_includes = []
    # SDL specific stuff.
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cdict = sdl_cenv.Dictionary()
    sdl_ldict = sdl_lenv.Dictionary()
    sdl_cflags = string.join(sdl_cdict['CCFLAGS'])
    sdl_include = sdl_cdict['CPPPATH'][0]
    link_env.Append (LIBS=sdl_ldict['LIBS'])
    link_env.Append (LIBPATH=sdl_ldict['LIBPATH'])
    solid_include = '#extern/solid/include'
    ode_include = '#extern/ode/dist/include/ode'
    # Python variables.
    python_include = sysconfig.get_python_inc ()
    python_libpath = sysconfig.get_python_lib (0, 1) + '/config'
    python_lib = 'python%d.%d' % sys.version_info[0:2]
    # International stuff
    if (use_international == 'true'):
        defines += ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']
        platform_libpath += ['#../lib/linux-glibc2.2.5-i386/ftgl',
                             '#../lib/linux-glibc2.2.5-i386/freetype/lib']
        platform_libs += ['ftgl', 'freetype']
        extra_includes += ['#../lib/linux-glibc2.2.5-i386/ftgl/include',
                           '#../lib/linux-glibc2.2.5-i386/freetype/include']

elif sys.platform == 'darwin':
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'true'
    use_precomp = 'true'
    use_sumo = 'false'
    use_ode = 'false'
    # TODO: replace darwin-6.8-powerpc with the actual directiory on the
    #       build machine
    darwin_precomp = '#../lib/darwin-6.8-powerpc'
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char']
    cxxflags = []
    defines = ['_THREAD_SAFE']
    if use_quicktime == 'true':
        defines += ['WITH_QUICKTIME']
    warn_flags = ['-Wall', '-W']
    release_flags = []
    debug_flags = ['-g']
    window_system = 'CARBON'
    # SDL specific stuff.
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cdict = sdl_cenv.Dictionary()
    sdl_ldict = sdl_lenv.Dictionary()
    sdl_cflags = string.join(sdl_cdict['CCFLAGS'])
    # Want to use precompiled libraries?
    if use_precomp == 'true':
        sdl_ldict['LIBS'] = ['libSDL.a']
        sdl_ldict['LIBPATH'] = [darwin_precomp + '/sdl/lib']
        sdl_cdict['CPPPATH'] = [darwin_precomp + '/sdl/include']

    sdl_include = sdl_cdict['CPPPATH'][0]
    link_env.Append (LIBS=sdl_ldict['LIBS'])
    link_env.Append (LIBPATH=sdl_ldict['LIBPATH'])
    platform_libs = ['z', 'GL', 'GLU', 'png', 'jpeg', 'stdc++'] 
    extra_includes = ['/sw/include']
    platform_libpath = ['/System/Library/Frameworks/OpenGL.framework/Libraries']
    platform_linkflags = []
    # Python variables.
    python_lib = 'python%d.%d' % sys.version_info[0:2]
    python_libpath = sysconfig.get_python_lib (0, 1) + '/config'
    python_include = sysconfig.get_python_inc ()

elif sys.platform == 'cygwin':
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-mno-cygwin', '-mwindows', '-funsigned-char']
    defines = ['FREE_WINDOWS', 'NDEBUG']
    cxxflags = []
    warn_flags = ['-Wall', '-Wno-char-subscripts']
    platform_libs = ['png', 'jpeg', 'netapi32',
                     'opengl32', 'glu32', 'winmm',
                     'mingw32', 'z']
    platform_libpath = ['/usr/lib/w32api', '/lib/w32api']
    platform_linkflags = ['-mwindows', '-mno-cygwin', '-mconsole']
    window_system = 'WIN32'
    extra_includes = ['/usr/include']
    # SDL specific stuff.
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cdict = sdl_cenv.Dictionary()
    sdl_ldict = sdl_lenv.Dictionary()
    sdl_cflags = '-DWIN32'
    sdl_include = sdl_cdict['CPPPATH'][0]
    link_env.Append (LIBS=sdl_ldict['LIBS'])
    link_env.Append (LIBPATH=sdl_ldict['LIBPATH'])
    # We need to force the Cygwin environment to use the g++ linker.
    link_env.Replace (CC='g++')
    # Python variables.
    python_include = sysconfig.get_python_inc ()
    python_libpath = sysconfig.get_python_lib (0, 1) + '/config'
    python_lib = 'python%d.%d' % sys.version_info[0:2]

elif sys.platform == 'win32':
    use_international = 'true'
    use_gameengine = 'true'
    use_openal = 'true'
    use_fmod = 'false'
    use_quicktime = 'true'
    use_sumo = 'false'
    use_ode = 'true'
    release_flags = ['/G6', '/GF']
    debug_flags = []
    extra_flags = ['/EHsc', '/J', '/W3', '/Gd', '/MT']
    cxxflags = []
    defines = ['WIN32', 'NDEBUG', '_CONSOLE', 'FTGL_STATIC_LIBRARY']
    defines += ['INTERNATIONAL', 'WITH_QUICKTIME']
    defines += ['_LIB', 'WITH_FREETYPE2', 'USE_OPENAL']
    warn_flags = []
    platform_libs = ['SDL', 'freetype2ST', 'ftgl_static_ST', 'gnu_gettext',
                     'qtmlClient', 'odelib', 'openal_static', 'soundsystem',
                     'ws2_32', 'dxguid', 'opengl32', 'libjpeg', 'glu32',
                     'vfw32', 'winmm', 'libpng_st', 'libz_st', 'solid',
                     'qhull', 'iconv', 'kernel32', 'user32', 'gdi32',
                     'winspool', 'comdlg32', 'advapi32', 'shell32',
                     'ole32', 'oleaut32', 'uuid', 'odbc32', 'odbccp32',
                     'libcmt', 'libc']
    platform_libpath = ['#../lib/windows/ftgl/lib',
                        '#../lib/windows/freetype/lib',
                        '#../lib/windows/gettext/lib',
                        '#../lib/windows/iconv/lib',
                        '#../lib/windows/jpeg/lib',
                        '#../lib/windows/QTDevWin/Libraries',
                        '#../lib/windows/ode/lib',
                        '#../lib/windows/openal/lib',
                        '#../lib/windows/png/lib',
                        '#../lib/windows/zlib/lib',
                        '#../lib/windows/solid/lib',
                        '#../lib/windows/qhull/lib',
                        '#../lib/windows/sdl/lib']
    platform_linkflags = [
                        '/SUBSYSTEM:CONSOLE',
                        '/MACHINE:IX86',
                        '/ENTRY:mainCRTStartup',
                        '/INCREMENTAL:NO',
                        '/NODEFAULTLIB:"msvcprt.lib"',
                        '/NODEFAULTLIB:"glut32.lib"',
                        '/NODEFAULTLIB:"libcd.lib"',
                        #'/NODEFAULTLIB:"libc.lib"',
                        '/NODEFAULTLIB:"libcpd.lib"',
                        '/NODEFAULTLIB:"libcp.lib"',
                        '/NODEFAULTLIB:"libcmtd.lib"',
                        ]
    window_system = 'WIN32'
    extra_includes = ['#../lib/windows/zlib/include',
                      '#../lib/windows/jpeg/include',
                      '#../lib/windows/png/include']
    if use_international == 'true':
        extra_includes += ['#../lib/windows/ftgl/include',
                           '#../lib/windows/freetype/include',
                           '#../lib/windows/gettext/include']
    if use_quicktime == 'true':
        extra_includes += ['#../lib/windows/QTDevWin/CIncludes']
    if use_openal == 'true':
        extra_includes += ['#../lib/windows/openal/include']
    sdl_include = '#../lib/windows/sdl/include'
    sdl_cflags = ''
    link_env.RES(['source/icons/winblender.rc'])
    window_system = 'WIN32'
    solid_include = '#../lib/windows/solid/include'
    ode_include = '#../lib/windows/ode/include'
    # Python lib name
    python_include = '#../lib/windows/python/include/python2.2'
    python_libpath = '#../lib/windows/python/lib'
    python_lib = 'python22'

elif string.find (sys.platform, 'sunos') != -1:
    use_international = 'true'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char']
    cxxflags = []
    defines = []
    env['ENV']['CC']='gcc'
    env['ENV']['CXX']='g++'
    warn_flags = ['-Wall', '-W']
    window_system = 'X11'
    platform_libs = ['m', 'z', 'GL', 'GLU', 'png', 'jpeg', 'util']
    platform_libpath = []
    platform_linkflags = []
    extra_includes = []
    # SDL specific stuff.
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cdict = sdl_cenv.Dictionary()
    sdl_ldict = sdl_lenv.Dictionary()
    sdl_cflags = string.join(sdl_cdict['CCFLAGS'])
    sdl_include = sdl_cdict['CPPPATH'][0]
    link_env.Append (LIBS=sdl_ldict['LIBS'])
    link_env.Append (LIBPATH=sdl_ldict['LIBPATH'])
    solid_include = '#extern/solid/include'
    ode_include = '#extern/ode/dist/include/ode'
    # Python variables.
    python_include = sysconfig.get_python_inc ()
    python_libpath = sysconfig.get_python_lib (0, 1) + '/config'
    python_lib = 'python%d.%d' % sys.version_info[0:2]
    # International stuff
    if (use_international == 'true'):
        defines += ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']
        platform_libpath += ['#../lib/solaris-2.8-sparc/ftgl',
                             '#../lib/solaris-2.8-sparc/freetype/lib']
        platform_libs += ['ftgl', 'freetype']
        extra_includes += ['#../lib/solaris-2.8-sparc/ftgl/include',
                           '#../lib/solaris-2.8-sparc/freetype/include']


elif string.find (sys.platform, 'irix') != -1:
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    irix_precomp = '#../lib/irix-6.5-mips'
    extra_flags = ['-n32', '-mips3', '-Xcpluscomm']
    cxxflags = ['-n32', '-mips3', '-Xcpluscomm', '-LANG:std']
    cxxflags += ['-LANG:libc_in_namespace_std=off']
    
    window_system = 'X11'
    release_flags = ['-O2', '-OPT:Olimit=0']
    debug_flags = ['-O2', '-g']
    defines = []
    warn_flags = ['-fullwarn', '-woff', '1001,1110,1201,1209,1355,1424,1681,3201']
    sdl_cflags = ''
    sdl_include = irix_precomp + '/sdl/include/SDL'
    link_env.Append (LIBS=['libSDL.a'])
    link_env.Append (LIBPATH=['/usr/lib32/mips3', irix_precomp + '/sdl/lib'])
    python_libpath = irix_precomp + '/python/lib/python2.2/config'
    python_include = irix_precomp + '/python/include/python2.2'
    python_lib = 'python2.2'

    platform_libs = ['SDL', 'movieGL', 'GLU', 'GL', 'Xmu', 'Xext', 'X11',
                     'c', 'm', 'dmedia', 'cl', 'audio',
                     'Cio', 'png', 'jpeg', 'z', 'pthread']
    platform_libpath = [irix_precomp + '/png/lib',
                        irix_precomp + '/jpeg/lib',
                        '/usr/lib32', '/lib/freeware/lib32']
    platform_linkflags = ['-mips3', '-n32']
    extra_includes = [irix_precomp + '/jpeg/include',
                      irix_precomp + '/png/include',
                      '/usr/freeware/include',
                      '/usr/include']
    solid_include = irix_precomp + '/solid/include'
    ode_include = irix_precomp + '/ode/include'


elif string.find (sys.platform, 'hp-ux') != -1:
    window_system = 'X11'
    defines = []

else:
    print "Unknown platform"

#-----------------------------------------------------------------------------
# End of platform specific section
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Game Engine settings
#-----------------------------------------------------------------------------
if use_gameengine == 'true':
    defines += ['GAMEBLENDER=1']
    if use_sumo == 'true':
        defines += ['USE_SUMO_SOLID']
    if use_ode == 'true':
        defines += ['USE_ODE']
else:
    defines += ['GAMEBLENDER=0']

#-----------------------------------------------------------------------------
# Settings to be exported to other SConscript files
#-----------------------------------------------------------------------------
cflags = extra_flags + release_flags + warn_flags

Export ('use_international')
Export ('use_gameengine')
Export ('use_openal')
Export ('use_fmod')
Export ('use_quicktime')
Export ('use_ode')
Export ('use_sumo')
Export ('python_include')
Export ('cflags')
Export ('defines')
Export ('cxxflags')
Export ('window_system')
Export ('sdl_cflags')
Export ('sdl_include')
Export ('solid_include')
Export ('ode_include')
Export ('extra_includes')
Export ('platform_libs')
Export ('platform_libpath')
Export ('platform_linkflags')

SConscript(['intern/SConscript',
            'source/SConscript'])

libpath = (['lib'])

libraries = (['blender_render',
              'blender_yafray',
              'blender_blendersrc',
              'blender_renderconverter',
              'blender_blenloader',
              'blender_writestreamglue',
              'blender_deflate',
              'blender_writeblenfile',
              'blender_readblenfile',
              'blender_readstreamglue',
              'blender_inflate',
              'blender_img',
              'blender_radiosity',
              'blender_blenkernel',
              'blender_blenpluginapi',
              'blender_imbuf',
              'blender_avi',
              'blender_blenlib',
              'blender_python',
              'blender_makesdna',
              'blender_kernel',
              'blender_BSP',
              'blender_LOD',
              'blender_GHOST',
              'blender_STR',
              'blender_guardedalloc',
              'blender_BMF',
              'blender_CTR',
              'blender_MEM',
              'blender_IK',
              'blender_MT',
              'soundsystem'])

link_env.Append (LIBS=libraries)
link_env.Append (LIBPATH=libpath)
link_env.Append (CPPDEFINES=defines)

if use_international == 'true':
    link_env.Append (LIBS=['blender_FTF'])
if use_quicktime == 'true':
    link_env.Append (LIBS=['blender_quicktime'])
if use_gameengine == 'true':
    link_env.Append (LIBS=['blender_expressions',
                           'KX_blenderhook',
                           'KX_converter',
                           'KX_ketsji',
                           'KX_network',
                           'NG_loopbacknetwork',
                           'NG_network',
                           'PHY_Physics',
                           'PHY_Dummy',
                           'SCA_GameLogic',
                           'RAS_rasterizer',
                           'RAS_OpenGLRasterizer',
                           'SG_SceneGraph'])
    if use_sumo == 'true':
        link_env.Append (LIBS=['PHY_Sumo'])
    if use_ode == 'true':
        link_env.Append (LIBS=['PHY_Ode'])

link_env.Append (LIBS=python_lib)
link_env.Append (LIBPATH=python_libpath)
link_env.Append (LIBS=platform_libs)
link_env.Append (LIBPATH=platform_libpath)
if sys.platform == 'darwin':
    link_env.Append (LINKFLAGS=' -framework Carbon')
    link_env.Append (LINKFLAGS=' -framework AGL')
    if use_quicktime == 'true':
        link_env.Append (LINKFLAGS=' -framework QuickTime')
else:
    link_env.Append (LINKFLAGS=platform_linkflags)

source_files = ['source/creator/buildinfo.c',
                'source/creator/creator.c']

if sys.platform == 'win32':
	source_files += ['source/icons/winblender.res']

include_paths = ['#/intern/guardedalloc',
                 '#/source/blender/makesdna',
                 '#/source/blender/blenkernel',
                 '#/source/blender/blenloader',
                 '#/source/blender/python',
                 '#/source/blender/blenlib',
                 '#/source/blender/renderconverter',
                 '#/source/blender/render/extern/include',
                 '#/source/kernel/gen_messaging',
                 '#/source/kernel/gen_system',
                 '#/source/blender/include',
                 '#/source/blender/imbuf']

link_env.Append (CPPPATH=include_paths)
link_env.Program (target='blender', source=source_files, CCFLAGS=cflags)

if sys.platform == 'darwin':
    bundle = Environment ()
    blender_app = 'blender'
    bundle.Depends ('#/blender.app/Contents/MacOS/' + blender_app, blender_app)
    bundle.Command ('#/blender.app/Contents/Info.plist',
                    '#/source/darwin/blender.app/Contents/Info.plist',
                    "rm -rf blender.app && " + \
                    "cp -R source/darwin/blender.app . && " +
                    "cat $SOURCE | sed s/VERSION/`cat release/VERSION`/ | \
                                   sed s/DATE/`date +'%Y-%b-%d'`/ \
                                   > $TARGET")
    bundle.Command ('blender.app/Contents/MacOS/' + blender_app, blender_app,
                    'cp $SOURCE $TARGET && ' + \
                    'chmod +x $TARGET && ' + \
                    'find $SOURCE -name CVS -prune -exec rm -rf {} \; && ' +
                    'find $SOURCE -name .DS_Store -exec rm -rf {} \;')
