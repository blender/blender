import string
import os
import time
import sys
from distutils import sysconfig

# Build directory.
root_build_dir = '..' + os.sep + 'build' + os.sep + sys.platform + os.sep

# Create the build directory. SCons does this automatically, but since we
# don't want to put scons-generated .sconsign files in the source tree, but in
# the root_build_dir, we have to create that dir ourselves before SCons tries
# to access/create the file containing .sconsign data.
if os.path.isdir (root_build_dir) == 0:
    os.makedirs (root_build_dir)

# User configurable options file. This can be controlled by the user by running
# scons with the following argument: CONFIG=user_config_options_file
config_file = ARGUMENTS.get('CONFIG', 'config.opts')

# Blender version.
version='2.33a'

sdl_env = Environment ()
freetype_env = Environment ()
env = Environment ()

if sys.platform == 'linux2' or sys.platform == 'linux-i386':
    use_international = 'false'
    use_gameengine = 'true'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'true'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-funsigned-char']
    cxxflags = []
    defines = []
    warn_flags = ['-Wall', '-W']
    window_system = 'X11'
    platform_libs = ['m', 'util', 'stdc++']
    platform_libpath = []
    platform_linkflags = ['-pthread']
    extra_includes = []
    # z library information
    z_lib = ['z']
    z_libpath = ['/usr/lib']
    z_include = ['/usr/include']
    # png library information
    png_lib = ['png']
    png_libpath = ['/usr/lib']
    png_include = ['/usr/include']
    # jpeg library information
    jpeg_lib = ['jpeg']
    jpeg_libpath = ['/usr/lib']
    jpeg_include = ['/usr/include']
    # OpenGL library information
    opengl_lib = ['GL', 'GLU']
    opengl_static = ['/usr/lib/libGL.a', '/usr/lib/libGLU.a']
    opengl_libpath = ['/usr/lib', '/usr/X11R6/lib']
    opengl_include = ['/usr/include']
    # SDL library information
    sdl_env.ParseConfig ('sdl-config --cflags --libs')
    sdl_cflags = sdl_env.Dictionary()['CCFLAGS']
    sdl_include = sdl_env.Dictionary()['CPPPATH']
    sdl_libpath = sdl_env.Dictionary()['LIBPATH']
    sdl_lib = sdl_env.Dictionary()['LIBS']
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid']
    qhull_lib = []                                              # TODO
    qhull_libpath = []                                          # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = ['ode']
    ode_libpath = ['#../lib/linux-glibc2.2.5-i386/ode/lib']
    ode_include = ['#../lib/linux-glibc2.2.5-i386/ode/include']
    # Python library information
    python_lib = ['python%d.%d' % sys.version_info[0:2]]
    python_libpath = [sysconfig.get_python_lib (0, 1) + '/config']
    python_include = [sysconfig.get_python_inc ()]
    python_linkflags = Split (sysconfig.get_config_var('LINKFORSHARED'))
    # International support information
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/linux-glibc2.2.5-i386/ftgl/lib']
    ftgl_include = ['#../lib/linux-glibc2.2.5-i386/ftgl/include']
    freetype_env.ParseConfig ('pkg-config --cflags --libs freetype2 2>/dev/null || freetype-config --cflags --libs 2>/dev/null')
    freetype_lib = freetype_env.Dictionary()['LIBS']
    freetype_libpath = freetype_env.Dictionary()['LIBPATH']
    freetype_include = freetype_env.Dictionary()['CPPPATH']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    # OpenAL library information
    openal_lib = ['openal']
    openal_libpath = ['/usr/lib']
    openal_include = ['/usr/include']

elif sys.platform == 'darwin':
    use_international = 'true'
    use_gameengine = 'true'
    use_openal = 'true'
    use_fmod = 'false'
    use_quicktime = 'true'
    use_precomp = 'true'
    use_sumo = 'true'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    # TODO: replace darwin-6.1-powerpc with the actual directiory on the
    #       build machine
    # darwin-6.1 is the name of cvs precomp folder
    # a symbolic link named darwin-X.Y-powerpc must be manually done
    #for now. X-Y is darwin kernel rev number
    darwin_precomp = '#../lib/darwin-6.1-powerpc/'
    fink_path = '/sw/'
    # TODO : try -mpowerpc -mpowerpc-gopt -mpowerpc-gfxopt optims
    #           doing actual profiling
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char', '-ffast-math']
    cxxflags = []
    defines = ['_THREAD_SAFE', 'MT_NDEBUG']
    if use_quicktime == 'true':
        defines += ['WITH_QUICKTIME']
    warn_flags = ['-Wall', '-W']
    release_flags = ['-O3']
    debug_flags = ['-g']
    window_system = 'CARBON'
    # z library information
    z_lib = ['z']
    z_libpath = []
    z_include = []
    # TODO : add a flag to allow each lib to be build from fink or precomp
    #        without having to have to specify the path manually in config.opts.    
    # png library information
    png_lib = ['libpng']
    png_libpath = [darwin_precomp + 'png/lib']
    png_include = [darwin_precomp + 'png/include']
    # jpeg library information
    jpeg_lib = ['libjpeg']
    jpeg_libpath = [darwin_precomp + 'jpeg/lib']
    jpeg_include = [darwin_precomp + 'jpeg/include']
    # OpenGL library information
    opengl_lib = ['GL', 'GLU']
    opengl_static = []
    opengl_libpath = []
    opengl_include = []
    # SDL specific stuff.
    sdl_env.ParseConfig ('sdl-config --cflags --libs')
    sdl_cflags = sdl_env.Dictionary()['CCFLAGS']
    # Want to use precompiled libraries?
    if use_precomp == 'true':
        sdl_include = [darwin_precomp + 'sdl/include']
        sdl_libpath = [darwin_precomp + 'sdl/lib']
        sdl_lib = ['libSDL.a']
    platform_libs = ['stdc++'] 
    extra_includes = ['/sw/include']
    platform_libpath = ['/System/Library/Frameworks/OpenGL.framework/Libraries']
    platform_linkflags = []
    # SOLID library information
    solid_lib = ['libsolid']                                          
    solid_libpath = [darwin_precomp + 'solid/lib']                                          
    solid_include = [darwin_precomp + 'solid/include']
    qhull_lib = ['libqhull']                                          
    qhull_libpath = [darwin_precomp + 'qhull/lib']                                          
    qhull_include = [darwin_precomp + 'qhull/include']
    # ODE library information
    ode_lib = ['libode']                                             
    ode_libpath = [darwin_precomp + 'ode/lib']                                          
    ode_include = [darwin_precomp + 'ode/include/ode']
    # Python variables.
    # TODO : fill vars differently if we are on 10.2 or 10.3
    # python_lib = ['python%d.%d' % sys.version_info[0:2]]
    # python_libpath = [sysconfig.get_python_lib (0, 1) + '/config']
    # python_include = [sysconfig.get_python_inc ()]
    # python_linkflags = Split (sysconfig.get_config_var('LINKFORSHARED'))
    python_lib = []
    python_libpath = ['/System/Library/Frameworks/Python.framework/Versions/2.3/lib/python2.3/config']
    python_include = ['/System/Library/Frameworks/Python.framework/Versions/2.3/include/python2.3']
    python_linkflags = ['-u', '__dummy', '-u', '_PyMac_Error', 
                        '-framework', 'System',
                        '-framework', 'Python',
                        '-framework', 'CoreServices',
                        '-framework', 'Foundation']
    # International stuff
    ftgl_lib = ['ftgl']
    ftgl_libpath = [darwin_precomp + 'ftgl/lib']
    ftgl_include = [darwin_precomp + 'ftgl/include']
    freetype_lib = ['libfreetype']
    freetype_libpath = [darwin_precomp + 'freetype/lib']
    freetype_include = [darwin_precomp + 'freetype/include']
    gettext_lib = ['libintl']
    gettext_libpath = [darwin_precomp + 'gettext/lib']
    gettext_include = [darwin_precomp + 'gettext/include']
    # OpenAL library information
    openal_lib = ['libopenal']
    openal_libpath = [darwin_precomp + 'openal/lib']
    openal_include = [darwin_precomp + 'openal/include']

elif sys.platform == 'cygwin':
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-mno-cygwin', '-mwindows', '-funsigned-char']
    cxxflags = []
    defines = ['FREE_WINDOWS', 'NDEBUG']
    warn_flags = ['-Wall', '-Wno-char-subscripts']
    platform_libs = ['png', 'jpeg', 'netapi32',
                     'opengl32', 'glu32', 'winmm',
                     'mingw32']
    platform_libpath = ['/usr/lib/w32api', '/lib/w32api']
    platform_linkflags = ['-mwindows', '-mno-cygwin', '-mconsole']
    window_system = 'WIN32'
    extra_includes = []
    # z library information
    z_lib = ['z']
    z_libpath = ['#../lib/windows/zlib/lib']
    z_include = ['#../lib/windows/zlib/include']
    # png library information
    png_lib = ['png']
    png_libpath = ['#../lib/windows/png/lib']
    png_include = ['#../lib/windows/png/include']
    # jpeg library information
    jpeg_lib = ['jpeg']
    jpeg_libpath = ['#../lib/windows/jpeg/lib']
    jpeg_include = ['#../lib/windows/jpeg/include']
    # OpenGL library information
    opengl_lib = ['opengl32', 'glu32']
    opengl_static = []
    opengl_libpath = []
    opengl_include = []
    # SDL specific stuff.
    sdl_include = ['#../lib/windows/sdl/include']
    sdl_libpath = ['#../lib/windows/sdl/lib']
    sdl_lib = ['SDL']
    sdl_cflags = []
    #sdl_cflags = '-DWIN32'
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid']
    qhull_lib = []                                              # TODO
    qhull_libpath = []                                          # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = ['ode']
    ode_libpath = ['#../lib/windows/gcc/ode/lib']
    ode_include = ['#../lib/windows/gcc/ode/include']
    # Python library information
    python_include = ['#../lib/windows/python/include/python2.2']
    python_libpath = ['#../lib/windows/python/lib']
    python_lib = ['python22']
    python_linkflags = []
    # International stuff
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/windows/gcc/ftgl/lib']
    ftgl_include = ['#../lib/windows/gcc/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = ['#../lib/windows/gcc/freetype/lib']
    freetype_include = ['#../lib/windows/gcc/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    # OpenAL library information
    openal_lib = []
    openal_libpath = []
    openal_include = []

elif sys.platform == 'win32':
    use_international = 'true'
    use_gameengine = 'true'
    use_openal = 'true'
    use_fmod = 'false'
    use_quicktime = 'true'
    use_sumo = 'true'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    release_flags = ['/Og', '/Ot', '/Ob1', '/Op', '/G6']
    debug_flags = ['/Zi', '/Fr${TARGET.base}.sbr']
    extra_flags = ['/EHsc', '/J', '/W3', '/Gd', '/MT']
    cxxflags = []
    defines = ['WIN32', '_CONSOLE']
    defines += ['WITH_QUICKTIME']
    defines += ['_LIB', 'USE_OPENAL']
    defines += ['FTGL_LIBRARY_STATIC']
    warn_flags = []
    platform_libs = [ 'qtmlClient', 'soundsystem',
                     'ws2_32', 'dxguid', 'vfw32', 'winmm',
                     'iconv', 'kernel32', 'user32', 'gdi32',
                     'winspool', 'comdlg32', 'advapi32', 'shell32',
                     'ole32', 'oleaut32', 'uuid', 'odbc32', 'odbccp32',
                     'libcmt', 'libc']
    platform_libpath = ['#../lib/windows/iconv/lib',
                        '#../lib/windows/QTDevWin/Libraries']
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
    extra_includes = []
    if use_quicktime == 'true':
        extra_includes += ['#../lib/windows/QTDevWin/CIncludes']
    # z library information
    z_lib = ['libz_st']
    z_libpath = ['#../lib/windows/zlib/lib']
    z_include = ['#../lib/windows/zlib/include']
    # png library information
    png_lib = ['libpng_st']
    png_libpath = ['#../lib/windows/png/lib']
    png_include = ['#../lib/windows/png/include']
    # jpeg library information
    jpeg_lib = ['libjpeg']
    jpeg_libpath = ['#../lib/windows/jpeg/lib']
    jpeg_include = ['#../lib/windows/jpeg/include']
    # OpenGL library information
    opengl_lib = ['opengl32', 'glu32']
    opengl_static = []
    opengl_libpath = []
    opengl_include = ['/usr/include']
    # SDL library information
    sdl_include = ['#../lib/windows/sdl/include']
    sdl_libpath = ['#../lib/windows/sdl/lib']
    sdl_lib = ['SDL']
    sdl_cflags = []
    window_system = 'WIN32'
    # SOLID library information
    solid_lib = ['extern/solid']
    solid_libpath = ['#../lib/windows/solid/lib']
    solid_include = ['#extern/solid']
    qhull_lib = ['qhull']
    qhull_libpath = ['#../lib/windows/qhull/lib']
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = []                                                # TODO
    ode_libpath = ['#../lib/windows/ode/lib']
    ode_include = ['#../lib/windows/ode/include']
    # Python lib name
    python_include = ['#../lib/windows/python/include/python2.3']
    python_libpath = ['#../lib/windows/python/lib']
    python_lib = ['python23']
    python_linkflags = []
    # International stuff
    ftgl_lib = ['ftgl_static_ST']
    ftgl_libpath = ['#../lib/windows/ftgl/lib']
    ftgl_include = ['#../lib/windows/ftgl/include']
    freetype_lib = ['freetype2ST']
    freetype_libpath = ['#../lib/windows/freetype/lib']
    freetype_include = ['#../lib/windows/freetype/include']
    gettext_lib = ['gnu_gettext']
    gettext_libpath = ['#../lib/windows/gettext/lib']
    gettext_include = ['#../lib/windows/gettext/include']
    # OpenAL library information
    openal_lib = ['openal_static']
    openal_libpath = ['#../lib/windows/openal/lib']
    openal_include = ['#../lib/windows/openal/include']

elif string.find (sys.platform, 'sunos') != -1:
    use_international = 'true'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char', '-DSUN_OGL_NO_VERTEX_MACROS']
    cxxflags = []
    defines = []
    warn_flags = ['-Wall', '-W']
    window_system = 'X11'
    platform_libs = ['stdc++', 'dl', 'm']
    platform_libpath = []
    platform_linkflags = []
    extra_includes = []
    # z library information
    z_lib = ['z']
    z_libpath = []
    z_include = []
    # png library information
    png_lib = ['png']
    png_libpath = []
    png_include = []
    # jpeg library information
    jpeg_lib = ['jpeg']
    jpeg_libpath = []
    jpeg_include = []
    # OpenGL library information
    opengl_lib = ['GL', 'GLU', 'X11']
    opengl_static = []
    opengl_libpath = ['/usr/openwin/include']
    opengl_include = ['/usr/openwin/lib']
    # SDL library information
    sdl_env.ParseConfig ('sdl-config --cflags --libs')
    sdl_cflags = sdl_env.Dictionary()['CCFLAGS']
    sdl_include = sdl_env.Dictionary()['CPPPATH']
    sdl_libpath = sdl_env.Dictionary()['LIBPATH']
    sdl_lib = sdl_env.Dictionary()['LIBS']
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid']
    qhull_lib = []                                              # TODO
    qhull_libpath = []                                          # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = []                                                # TODO
    ode_libpath = []                                            # TODO
    ode_include = ['#extern/ode/dist/include/ode']
    # Python variables.
    python_lib = ['python%d.%d' % sys.version_info[0:2]]
    python_libpath = [sysconfig.get_python_lib (0, 1) + '/config']
    python_include = [sysconfig.get_python_inc ()]
    python_linkflags = []
    # International support information
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/solaris-2.8-sparc/ftgl/lib']
    ftgl_include = ['#../lib/solaris-2.8-sparc/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = ['#../lib/solaris-2.8-sparc/freetype/lib']
    freetype_include = ['#../lib/solaris-2.8-sparc/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    # OpenAL library information
    openal_lib = []
    openal_libpath = []
    openal_include = []

elif string.find (sys.platform, 'irix') != -1:
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    use_buildinfo = 'false'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    irix_precomp = '#../lib/irix-6.5-mips'
    extra_flags = ['-n32', '-mips3', '-Xcpluscomm']
    cxxflags = ['-n32', '-mips3', '-Xcpluscomm', '-LANG:std']
    cxxflags += ['-LANG:libc_in_namespace_std=off']
    
    window_system = 'X11'
    release_flags = ['-O2', '-OPT:Olimit=0']
    debug_flags = ['-O2', '-g']
    defines = []
    warn_flags = ['-fullwarn', '-woff', '1001,1110,1201,1209,1355,1424,1681,3201']
    platform_libs = ['movieGL', 'Xmu', 'Xext', 'X11',
                     'c', 'm', 'dmedia', 'cl', 'audio',
                     'Cio', 'pthread']
    platform_libpath = ['/usr/lib32/mips3',
                        '/lib/freeware/lib32',
                        '/usr/lib32']
    platform_linkflags = ['-mips3', '-n32']
    extra_includes = ['/usr/freeware/include',
                      '/usr/include']
    # z library information
    z_lib = ['z']
    z_libpath = []
    z_include = []
    # png library information
    png_lib = ['png']
    png_libpath = [irix_precomp + '/png/lib']
    png_include = [irix_precomp + '/png/include']
    # jpeg library information
    jpeg_lib = ['jpeg']
    jpeg_libpath = [irix_precomp + '/jpeg/lib']
    jpeg_include = [irix_precomp + '/jpeg/include']
    # OpenGL library information
    opengl_lib = ['GL', 'GLU']
    opengl_static = []
    opengl_libpath = []
    opengl_include = []
    # SDL library information
    sdl_cflags = []
    sdl_include = [irix_precomp + '/sdl/include/SDL']
    sdl_libpath = [irix_precomp + '/sdl/lib']
    sdl_lib = ['SDL', 'libSDL.a']
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = [irix_precomp + '/solid/include']
    qhull_lib = []                                              # TODO
    qhull_libpath = []                                          # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = []                                                # TODO
    ode_libpath = []                                            # TODO
    ode_include = [irix_precomp + '/ode/include']
    # Python library information
    python_libpath = [irix_precomp + '/python/lib/python2.2/config']
    python_include = [irix_precomp + '/python/include/python2.2']
    python_lib = ['python2.2']
    python_linkflags = []
    # International support information
    ftgl_lib = ['ftgl']
    ftgl_libpath = [irix_precomp + '/ftgl/lib']
    ftgl_include = [irix_precomp + '/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = [irix_precomp + '/freetype/lib']
    freetype_include = [irix_precomp + '/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    # OpenAL library information
    openal_lib = []
    openal_libpath = []
    openal_include = []

elif string.find (sys.platform, 'hp-ux') != -1:
    window_system = 'X11'
    defines = []

elif sys.platform=='openbsd3':
    print "Building for OpenBSD 3.x"
    use_international = 'false'
    use_gameengine = 'false'
    use_openal = 'false'
    use_fmod = 'false'
    use_quicktime = 'false'
    use_sumo = 'false'
    use_ode = 'false'
    use_buildinfo = 'true'
    build_blender_dynamic = 'true'
    build_blender_static = 'false'
    build_blender_player = 'false'
    build_blender_plugin = 'false'
    release_flags = ['-O2']
    debug_flags = ['-O2', '-g']
    extra_flags = ['-pipe', '-fPIC', '-funsigned-char']
    cxxflags = []
    defines = []
    warn_flags = ['-Wall','-W']
    window_system = 'X11'
    platform_libs = ['m', 'stdc++', 'pthread', 'util']
    platform_libpath = []
    platform_linkflags = []
    extra_includes = []
    z_lib = ['z']
    z_libpath = ['/usr/lib']
    z_include = ['/usr/include']
    # png library information
    png_lib = ['png']
    png_libpath = ['/usr/local/lib']
    png_include = ['/usr/local/include']
    # jpeg library information
    jpeg_lib = ['jpeg']
    jpeg_libpath = ['/usr/local/lib']
    jpeg_include = ['/usr/local/include']
    # OpenGL library information
    opengl_lib = ['GL', 'GLU']
    opengl_static = ['/usr/lib/libGL.a', '/usr/lib/libGLU.a']
    opengl_libpath = ['/usr/lib', '/usr/X11R6/lib']
    opengl_include = ['/usr/X11R6/include/']
    # SDL library information
    sdl_env.ParseConfig ('sdl-config --cflags --libs')
    sdl_cflags = sdl_env.Dictionary()['CCFLAGS']
    sdl_include = sdl_env.Dictionary()['CPPPATH']
    sdl_libpath = sdl_env.Dictionary()['LIBPATH']
    sdl_lib = sdl_env.Dictionary()['LIBS']
    # SOLID library information
    solid_lib = []                     # TODO
    solid_libpath = []        # TODO
    solid_include = ['#extern/solid']
    qhull_lib = []       # TODO
    qhull_libpath = []  # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = ['ode']
    ode_libpath = ['#../lib/linux-glibc2.2.5-i386/ode/lib']
    ode_include = ['#../lib/linux-glibc2.2.5-i386/ode/include']
    # Python library information
    python_lib = ['python%d.%d' % sys.version_info[0:2]]
    python_libpath = [sysconfig.get_python_lib (0, 1) + '/config']
    python_include = [sysconfig.get_python_inc ()]
    python_linkflags = []
    # International support information
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/linux-glibc2.2.5-i386/ftgl/lib']
    ftgl_include = ['#../lib/linux-glibc2.2.5-i386/ftgl/include']
    freetype_env.ParseConfig('pkg-config --cflags --libs freetype2')
    freetype_lib = freetype_env.Dictionary()['LIBS']
    freetype_libpath = freetype_env.Dictionary()['LIBPATH']
    freetype_include = freetype_env.Dictionary()['CPPPATH']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    # OpenAL library information
    openal_lib = ['openal']
    openal_libpath = ['/usr/lib']
    openal_include = ['/usr/include']

else:
    print "Unknown platform %s"%sys.platform
    exit

#-----------------------------------------------------------------------------
# End of platform specific section
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# User configurable options to be saved in a config file.
#-----------------------------------------------------------------------------
# Checking for an existing config file - use that one if it exists,
# otherwise create one.
my_defines = []
my_ccflags = []
my_cxxflags = []
my_ldflags = []
if os.path.exists (config_file):
    print "Using config file: " + config_file
else:
    print "Creating new config file: " + config_file
    env_dict = env.Dictionary()
    config=open (config_file, 'w')
    config.write ("# Configuration file containing user definable options.\n")
    config.write ("VERSION = '2.33-cvs'\n")
    config.write ("BUILD_BINARY = 'release'\n")
    config.write ("USE_BUILDINFO = %r\n"%(use_buildinfo))
    config.write ("BUILD_BLENDER_DYNAMIC = %r\n"%(build_blender_dynamic))
    config.write ("BUILD_BLENDER_STATIC = %r\n"%(build_blender_static))
    config.write ("BUILD_BLENDER_PLAYER = %r\n"%(build_blender_player))
    config.write ("BUILD_BLENDER_PLUGIN = %r\n"%(build_blender_plugin))
    config.write ("BUILD_DIR = %r\n"%(root_build_dir))
    
    config.write ("\n# Extra compiler flags can be defined here.\n")
    config.write ("DEFINES = %s\n"%(my_defines))
    config.write ("CCFLAGS = %s\n"%(my_ccflags))
    config.write ("CXXFLAGS = %s\n"%(my_cxxflags))
    config.write ("LDFLAGS = %s\n"%(my_ldflags))

    config.write ("USE_INTERNATIONAL = %r\n"%(use_international))
    config.write ("BUILD_GAMEENGINE = %r\n"%(use_gameengine))
    if use_ode == 'true':
        config.write ("USE_PHYSICS = 'ode'\n")
    else:
        config.write ("USE_PHYSICS = 'solid'\n")
    config.write ("USE_OPENAL = %r\n"%(use_openal))
    config.write ("USE_FMOD = %r\n"%(use_fmod))
    config.write ("USE_QUICKTIME = %r\n"%(use_quicktime))
    config.write ("\n# Compiler information.\n")
    config.write ("HOST_CC = %r\n"%(env_dict['CC']))
    config.write ("HOST_CXX = %r\n"%(env_dict['CXX']))
    config.write ("TARGET_CC = %r\n"%(env_dict['CC']))
    config.write ("TARGET_CXX = %r\n"%(env_dict['CXX']))
    config.write ("TARGET_AR = %r\n"%(env_dict['AR']))
    config.write ("PATH = %r\n"%(os.environ['PATH']))
    config.write ("\n# External library information.\n")
    config.write ("PLATFORM_LIBS = %r\n"%(platform_libs))
    config.write ("PLATFORM_LIBPATH = %r\n"%(platform_libpath))
    config.write ("PLATFORM_LINKFLAGS = %r\n"%(platform_linkflags))
    config.write ("PYTHON_INCLUDE = %r\n"%(python_include))
    config.write ("PYTHON_LIBPATH = %r\n"%(python_libpath))
    config.write ("PYTHON_LIBRARY = %r\n"%(python_lib))
    config.write ("PYTHON_LINKFLAGS = %r\n"%(python_linkflags))
    config.write ("SDL_CFLAGS = %r\n"%(sdl_cflags))
    config.write ("SDL_INCLUDE = %r\n"%(sdl_include))
    config.write ("SDL_LIBPATH = %r\n"%(sdl_libpath))
    config.write ("SDL_LIBRARY = %r\n"%(sdl_lib))
    config.write ("Z_INCLUDE = %r\n"%(z_include))
    config.write ("Z_LIBPATH = %r\n"%(z_libpath))
    config.write ("Z_LIBRARY = %r\n"%(z_lib))
    config.write ("PNG_INCLUDE = %r\n"%(png_include))
    config.write ("PNG_LIBPATH = %r\n"%(png_libpath))
    config.write ("PNG_LIBRARY = %r\n"%(png_lib))
    config.write ("JPEG_INCLUDE = %r\n"%(jpeg_include))
    config.write ("JPEG_LIBPATH = %r\n"%(jpeg_libpath))
    config.write ("JPEG_LIBRARY = %r\n"%(jpeg_lib))
    config.write ("OPENGL_INCLUDE = %r\n"%(opengl_include))
    config.write ("OPENGL_LIBPATH = %r\n"%(opengl_libpath))
    config.write ("OPENGL_LIBRARY = %r\n"%(opengl_lib))
    config.write ("OPENGL_STATIC = %r\n"%(opengl_static))
    config.write ("\n# The following information is only necessary when you've enabled support for\n")
    config.write ("# the game engine.\n")
    config.write ("SOLID_INCLUDE = %r\n"%(solid_include))
    config.write ("SOLID_LIBPATH = %r\n"%(solid_libpath))
    config.write ("SOLID_LIBRARY = %r\n"%(solid_lib))
    config.write ("QHULL_INCLUDE = %r\n"%(qhull_include))
    config.write ("QHULL_LIBPATH = %r\n"%(qhull_libpath))
    config.write ("QHULL_LIBRARY = %r\n"%(qhull_lib))
    config.write ("ODE_INCLUDE = %r\n"%(ode_include))
    config.write ("ODE_LIBPATH = %r\n"%(ode_libpath))
    config.write ("ODE_LIBRARY = %r\n"%(ode_lib))
    config.write ("OPENAL_INCLUDE = %r\n"%(openal_include))
    config.write ("OPENAL_LIBPATH = %r\n"%(openal_libpath))
    config.write ("OPENAL_LIBRARY = %r\n"%(openal_lib))
    config.write ("\n# The following information is only necessary when building with\n")
    config.write ("# internationalization support.\n");
    config.write ("FTGL_INCLUDE = %r\n"%(ftgl_include))
    config.write ("FTGL_LIBPATH = %r\n"%(ftgl_libpath))
    config.write ("FTGL_LIBRARY = %r\n"%(ftgl_lib))
    config.write ("FREETYPE_INCLUDE = %r\n"%(freetype_include))
    config.write ("FREETYPE_LIBPATH = %r\n"%(freetype_libpath))
    config.write ("FREETYPE_LIBRARY = %r\n"%(freetype_lib))
    config.write ("GETTEXT_INCLUDE = %r\n"%(gettext_include))
    config.write ("GETTEXT_LIBPATH = %r\n"%(gettext_libpath))
    config.write ("GETTEXT_LIBRARY = %r\n"%(gettext_lib))
    config.close ()

#-----------------------------------------------------------------------------
# Read the options from the config file and update the various necessary flags
#-----------------------------------------------------------------------------
list_opts = []
user_options_env = Environment ()
user_options = Options (config_file)
user_options.AddOptions (
        ('VERSION', 'Blender version', version),
        (EnumOption ('BUILD_BINARY', 'release',
                     'Select a release or debug binary.',
                     allowed_values = ('release', 'debug'))),
        (BoolOption ('USE_BUILDINFO',
                     'Set to 1 if you want to add build information.',
                     'false')),
        (BoolOption ('BUILD_BLENDER_DYNAMIC',
                     'Set to 1 if you want to build blender with hardware accellerated OpenGL support.',
                     'true')),
        (BoolOption ('BUILD_BLENDER_STATIC',
                     'Set to 1 if you want to build blender with software OpenGL support.',
                     'false')),
        (BoolOption ('BUILD_BLENDER_PLAYER',
                     'Set to 1 if you want to build the blender player.',
                     'false')),
        (BoolOption ('BUILD_BLENDER_PLUGIN',
                     'Set to 1 if you want to build the blender plugin.',
                     'false')),
        ('BUILD_DIR', 'Target directory for intermediate files.',
                     root_build_dir),
        (BoolOption ('USE_INTERNATIONAL',
                     'Set to 1 to have international support.',
                     'false')),
        (EnumOption ('USE_PHYSICS', 'solid',
                     'Select which physics engine to use.',
                     allowed_values = ('ode', 'solid'))),
        (BoolOption ('BUILD_GAMEENGINE',
                     'Set to 1 to build blender with game engine support.',
                     'false')),
        (BoolOption ('USE_OPENAL',
                     'Set to 1 to build the game engine with OpenAL support.',
                     'false')),
        (BoolOption ('USE_FMOD',
                     'Set to 1 to build the game engine with FMod support.',
                     'false')),
        (BoolOption ('USE_QUICKTIME',
                     'Set to 1 to add support for QuickTime.',
                     'false')),
        ('HOST_CC', 'C compiler for the host platfor. This is the same as target platform when not cross compiling.'),
        ('HOST_CXX', 'C++ compiler for the host platform. This is the same as target platform when not cross compiling.'),
        ('TARGET_CC', 'C compiler for the target platform.'),
        ('TARGET_CXX', 'C++ compiler for the target platform.'),
        ('TARGET_AR', 'Linker command for linking libraries.'),
        ('PATH', 'Standard search path'),
        ('PLATFORM_LIBS', 'Platform specific libraries.'),
        ('PLATFORM_LIBPATH', 'Platform specific library link path.'),
        ('PLATFORM_LINKFLAGS', 'Platform specific linkflags'),
        ('PYTHON_INCLUDE', 'Include directory for Python header files.'),
        ('PYTHON_LIBPATH', 'Library path where the Python lib is located.'),
        ('PYTHON_LIBRARY', 'Python library name.'),
        ('PYTHON_LINKFLAGS', 'Python specific linkflags.'),
        ('SDL_CFLAGS', 'Necessary CFLAGS when using sdl functionality.'),
        ('SDL_INCLUDE', 'Include directory for SDL header files.'),
        ('SDL_LIBPATH', 'Library path where the SDL library is located.'),
        ('SDL_LIBRARY', 'SDL library name.'),
        ('Z_INCLUDE', 'Include directory for zlib header files.'),
        ('Z_LIBPATH', 'Library path where the zlib library is located.'),
        ('Z_LIBRARY', 'Z library name.'),
        ('PNG_INCLUDE', 'Include directory for png header files.'),
        ('PNG_LIBPATH', 'Library path where the png library is located.'),
        ('PNG_LIBRARY', 'png library name.'),
        ('JPEG_INCLUDE', 'Include directory for jpeg header files.'),
        ('JPEG_LIBPATH', 'Library path where the jpeg library is located.'),
        ('JPEG_LIBRARY', 'jpeg library name.'),
        ('OPENGL_INCLUDE', 'Include directory for OpenGL header files.'),
        ('OPENGL_LIBPATH', 'Library path where the OpenGL libraries are located.'),
        ('OPENGL_LIBRARY', 'OpenGL library names.'),
        ('OPENGL_STATIC', 'Linker flags for static linking of Open GL.'),
        ('SOLID_INCLUDE', 'Include directory for SOLID header files.'),
        ('SOLID_LIBPATH', 'Library path where the SOLID library is located.'),
        ('SOLID_LIBRARY', 'SOLID library name.'),
        ('QHULL_INCLUDE', 'Include directory for QHULL header files.'),
        ('QHULL_LIBPATH', 'Library path where the QHULL library is located.'),
        ('QHULL_LIBRARY', 'QHULL library name.'),
        ('ODE_INCLUDE', 'Include directory for ODE header files.'),
        ('ODE_LIBPATH', 'Library path where the ODE library is located.'),
        ('ODE_LIBRARY', 'ODE library name.'),
        ('OPENAL_INCLUDE', 'Include directory for OpenAL header files.'),
        ('OPENAL_LIBPATH', 'Library path where the OpenAL library is located.'),
        ('OPENAL_LIBRARY', 'OpenAL library name.'),
        ('FTGL_INCLUDE', 'Include directory for ftgl header files.'),
        ('FTGL_LIBPATH', 'Library path where the ftgl library is located.'),
        ('FTGL_LIBRARY', 'ftgl library name.'),
        ('FREETYPE_INCLUDE', 'Include directory for freetype2 header files.'),
        ('FREETYPE_LIBPATH', 'Library path where the freetype2 library is located.'),
        ('FREETYPE_LIBRARY', 'Freetype2 library name.'),
        ('GETTEXT_INCLUDE', 'Include directory for gettext header files.'),
        ('GETTEXT_LIBPATH', 'Library path where the gettext library is located.'),
        ('GETTEXT_LIBRARY', 'gettext library name.'),

        ('DEFINES', 'Extra Preprocessor defines.'),
        ('CCFLAGS', 'Extra C Compiler flags.'),
        ('CXXFLAGS','Extra C++ Compiler flags.'),
        ('LDFLAGS', 'Extra Linker flags.')
    )
user_options.Update (user_options_env)
user_options_dict = user_options_env.Dictionary()

root_build_dir = user_options_dict['BUILD_DIR']
    
if user_options_dict['BUILD_GAMEENGINE'] == 1:
    defines += ['GAMEBLENDER=1']
    if user_options_dict['USE_PHYSICS'] == 'ode':
        defines += ['USE_ODE']
    else:
        defines += ['USE_SUMO_SOLID']
else:
    defines += ['GAMEBLENDER=0']

if user_options_dict['BUILD_BINARY'] == 'release':
    cflags = extra_flags + release_flags + warn_flags
    if sys.platform == 'win32':
        defines += ['NDEBUG']
else:
    cflags = extra_flags + debug_flags + warn_flags
    if sys.platform == 'win32':
        #defines += ['_DEBUG'] specifying this makes msvc want to link to python22_d.lib??
        platform_linkflags += ['/DEBUG','/PDB:blender.pdb']

defines += user_options_dict['DEFINES']
cflags += user_options_dict['CCFLAGS']
cxxflags += user_options_dict['CXXFLAGS']
platform_linkflags += user_options_dict['LDFLAGS']

#-----------------------------------------------------------------------------
# Generic library generation environment. This one is the basis for each
# library.
#-----------------------------------------------------------------------------
library_env = env.Copy ()
library_env.Replace (CC = user_options_dict['TARGET_CC'])
library_env.Replace (CXX = user_options_dict['TARGET_CXX'])
library_env.Replace (PATH = user_options_dict['PATH'])
library_env.Replace (AR = user_options_dict['TARGET_AR'])
library_env.Append (CCFLAGS = cflags)
library_env.Append (CXXFLAGS = cxxflags)
library_env.Append (CPPDEFINES = defines)
library_env.SConsignFile (root_build_dir+'scons-signatures')

#-----------------------------------------------------------------------------
# Settings to be exported to other SConscript files
#-----------------------------------------------------------------------------

Export ('cflags')
Export ('defines')
Export ('window_system')
Export ('extra_includes')
Export ('user_options_dict')
Export ('library_env')

BuildDir (root_build_dir+'/extern', 'extern', duplicate=0)
SConscript (root_build_dir+'extern/SConscript')
BuildDir (root_build_dir+'/intern', 'intern', duplicate=0)
SConscript (root_build_dir+'intern/SConscript')
BuildDir (root_build_dir+'/source', 'source', duplicate=0)
SConscript (root_build_dir+'source/SConscript')

libpath = (['#'+root_build_dir+'/lib'])

link_env = library_env.Copy ()
link_env.Append (LIBPATH=libpath)

def common_libs(env):
	"""
	Append to env all libraries that are common to Blender and Blenderplayer
	"""
	env.Append (LIBS=[
		'blender_blenloader',
		'blender_readblenfile',
		'blender_img',
		'blender_blenkernel',
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
	if user_options_dict['USE_QUICKTIME'] == 1:
		env.Append (LIBS=['blender_quicktime'])


def international_libs(env):
	"""
	Append international font support libraries
	"""
	if user_options_dict['USE_INTERNATIONAL'] == 1:
		env.Append (LIBS=user_options_dict['FREETYPE_LIBRARY'])
		env.Append (LIBPATH=user_options_dict['FREETYPE_LIBPATH'])
		env.Append (LIBS=['blender_FTF'])
		env.Append (LIBS=user_options_dict['FTGL_LIBRARY'])
		env.Append (LIBPATH=user_options_dict['FTGL_LIBPATH'])
		env.Append (LIBS=user_options_dict['FREETYPE_LIBRARY'])

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
		'blender_IK'])

def ketsji_libs(env):
	"""
	Game Engine libs
	"""
	if user_options_dict['BUILD_GAMEENGINE'] == 1:
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
		if user_options_dict['USE_PHYSICS'] == 'solid':
			env.Append (LIBS=['PHY_Sumo', 'PHY_Physics', 'blender_MT', 'extern_solid', 'extern_qhull'])
		else:
			env.Append (LIBS=['PHY_Ode',
					'PHY_Physics'])
			env.Append (LIBS=user_options_dict['ODE_LIBRARY'])
			env.Append (LIBPATH=user_options_dict['ODE_LIBPATH'])

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
			
def system_libs(env):
	"""
	System libraries: Python, SDL, PNG, JPEG, Gettext, OpenAL, Carbon
	"""
	env.Append (LIBS=['blender_python'])
	env.Append (LIBS=user_options_dict['PYTHON_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['PYTHON_LIBPATH'])
	env.Append (LINKFLAGS=user_options_dict['PYTHON_LINKFLAGS'])
	env.Append (LIBS=user_options_dict['SDL_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['SDL_LIBPATH'])
	env.Append (LIBS=user_options_dict['PNG_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['PNG_LIBPATH'])
	env.Append (LIBS=user_options_dict['JPEG_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['JPEG_LIBPATH'])
	env.Append (LIBS=user_options_dict['GETTEXT_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['GETTEXT_LIBPATH'])
	env.Append (LIBS=user_options_dict['Z_LIBRARY'])
	env.Append (LIBPATH=user_options_dict['Z_LIBPATH'])
	if user_options_dict['USE_OPENAL'] == 1:
		env.Append (LIBS=user_options_dict['OPENAL_LIBRARY'])
		env.Append (LIBPATH=user_options_dict['OPENAL_LIBPATH'])
	env.Append (LIBS=user_options_dict['PLATFORM_LIBS'])
	env.Append (LIBPATH=user_options_dict['PLATFORM_LIBPATH'])
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
		if user_options_dict['USE_QUICKTIME'] == 1:
			env.Append (LINKFLAGS='-framework')
			env.Append (LINKFLAGS='QuickTime')
	else:
		env.Append (LINKFLAGS=user_options_dict['PLATFORM_LINKFLAGS'])
	if sys.platform == 'win32':
		env.Append (RES = ['source/icons/winblender.rc'])
	env.BuildDir (root_build_dir, '.', duplicate=0)

def buildinfo(env, build_type):
	"""
	Generate a buildinfo object
	"""
	build_date = time.strftime ("%Y-%m-%d")
	build_time = time.strftime ("%H:%M:%S")
	obj = []
	if user_options_dict['USE_BUILDINFO'] == 1:
		if sys.platform=='win32':
			build_info_file = open("source/creator/winbuildinfo.h", 'w')
			build_info_file.write("char *build_date=\"%s\";\n"%build_date)
			build_info_file.write("char *build_time=\"%s\";\n"%build_time)
			build_info_file.write("char *build_platform=\"win32\";\n")
			build_info_file.write("char *build_type=\"%s\";\n"%build_type)
			build_info_file.close()
			env.Append (CPPDEFINES = ['NAN_BUILDINFO', 'BUILD_DATE'])
		else:
			env.Append (CPPDEFINES = ['BUILD_TIME=\'"%s"\''%(build_time),
							'BUILD_DATE=\'"%s"\''%(build_date),
							'BUILD_TYPE=\'"dynamic"\'',
							'NAN_BUILDINFO',
							'BUILD_PLATFORM=\'"%s"\''%(sys.platform)])
		obj = [env.Object (root_build_dir+'source/creator/%s_buildinfo'%build_type,
						[root_build_dir+'source/creator/buildinfo.c'])]
	return obj

def BlenderBundle(target):
	"""
	Make a MacOSX bundle.
	
	target = Name of bundle to make (string)
	eg: BlenderBundle('blender')
	"""
	if sys.platform == 'darwin':
		bundle = Environment ()
		blender_app = target
		bundle.Depends ('#/%s.app/Contents/Info.plist'%target, blender_app)
		bundle.Command ('#/%s.app/Contents/Info.plist'%target,
				'#/source/darwin/%s.app/Contents/Info.plist'%target,
				"rm -rf %s.app && "%target + \
				"cp -R source/darwin/%s.app . && "%target +
				"cat $SOURCE | sed s/VERSION/`cat release/VERSION`/ | \
						sed s/DATE/`date +'%Y-%b-%d'`/ \
						> $TARGET && " + \
				'cp -p %s %s.app/Contents/MacOS/%s && '%(target, target, target) + \
				'strip -u -r %s.app/Contents/MacOS/%s && '%(target, target) + \
				'cp bin/.blender/.bfont.ttf %s.app/Contents/Resources/ && '%target + \
				'cp bin/.blender/.Blanguages %s.app/Contents/Resources/ && '%target + \
				'cp -R bin/.blender/locale %s.app/Contents/Resources/ && '%target + \
				'cp -R release/bpydata %s.app/Contents/Resources/ && '%target + \
				'cp -R release/scripts %s.app/Contents/Resources/ && '%target + \
				'cp -R release/plugins %s.app/Contents/Resources/ && '%target + \
				'chmod +x $TARGET && ' + \
				'find %s.app -name CVS -prune -exec rm -rf {} \; && '%target +
				'find %s.app -name .DS_Store -exec rm -rf {} \;'%target)


if user_options_dict['BUILD_BLENDER_DYNAMIC'] == 1:
	dy_blender = link_env.Copy ()
	blender_libs(dy_blender)
	common_libs(dy_blender)
	international_libs(dy_blender)
	ketsji_libs(dy_blender)
	system_libs(dy_blender)
	dy_blender.Append (LIBS=user_options_dict['OPENGL_LIBRARY'])
	dy_blender.Append (LIBPATH=user_options_dict['OPENGL_LIBPATH'])
	dy_blender.Append (CPPPATH=user_options_dict['OPENGL_INCLUDE'])
	d_obj = buildinfo(dy_blender, "dynamic")
	if sys.platform == 'win32':
		dy_blender.Program (target='blender',
					source=d_obj + ['source/icons/winblender.res'])
	else:
		if sys.platform == 'cygwin':
			dy_blender.Replace (CC='g++')
		dy_blender.Program (target='blender', source=d_obj)

if user_options_dict['BUILD_BLENDER_STATIC'] == 1:
	st_blender = link_env.Copy ()
	blender_libs(st_blender)
	common_libs(st_blender)
	international_libs(st_blender)
	ketsji_libs(dst_blender)
	system_libs(st_blender)
	# The next line is to make sure that the LINKFLAGS are appended at the end
	# of the link command. This 'trick' is needed because the GL and GLU static
	# libraries need to be at the end of the command.
	st_blender.Replace(LINKCOM="$LINK -o $TARGET $SOURCES $_LIBDIRFLAGS $_LIBFLAGS $LINKFLAGS")
	s_obj = buildinfo(st_blender, "static")
	st_blender.Append (LINKFLAGS=user_options_dict['OPENGL_STATIC'])
	st_blender.Append (CPPPATH=user_options_dict['OPENGL_INCLUDE'])
	st_blender.Prepend (LIBPATH=['/usr/lib/opengl/xfree/lib'])
	st_blender.Program (target='blenderstatic', source=s_obj)

BlenderBundle('blender')

if sys.platform=='win32':
    if user_options_dict['BUILD_BINARY']=='debug':
        browser = Environment()
        browser_tmp = root_build_dir+'bscmake.tmp'
        browser.Command ('blender.bsc', 'blender$PROGSUFFIX',
            ['dir /b/s '+root_build_dir+'*.sbr >'+browser_tmp,
             'bscmake /nologo /n /oblender.bsc @'+browser_tmp,
             'del '+browser_tmp])

if user_options_dict['BUILD_BLENDER_PLAYER'] == 1 and user_options_dict['BUILD_GAMEENGINE'] == 1:
	player_blender = link_env.Copy()
	player_libs(player_blender)
	common_libs(player_blender)
	international_libs(player_blender)
	ketsji_libs(player_blender)
	player_libs2(player_blender)
	system_libs(player_blender)
	player_blender.Append (LIBS=user_options_dict['OPENGL_LIBRARY'])
	player_blender.Append (LIBPATH=user_options_dict['OPENGL_LIBPATH'])
	player_blender.Append (CPPPATH=user_options_dict['OPENGL_INCLUDE'])
	d_obj = buildinfo(player_blender, "player")
	if sys.platform == 'win32':
		player_blender.Program (target='blenderplayer',
					source=d_obj + ['source/icons/winblender.res'])
	else:
		if sys.platform == 'cygwin':
			player_blender.Replace (CC='g++')
		player_blender.Program (target='blenderplayer', source=d_obj)
	BlenderBundle('blenderplayer')
	if sys.platform=='win32':
		if user_options_dict['BUILD_BINARY']=='debug':
			browser = Environment()
			browser_tmp = root_build_dir+'bscmake.tmp'
			browser.Command ('blenderplayer.bsc', 'blenderplayer$PROGSUFFIX',
			['dir /b/s '+root_build_dir+'*.sbr >'+browser_tmp,
			'bscmake /nologo /n /oblenderplayer.bsc @'+browser_tmp,
			'del '+browser_tmp])
