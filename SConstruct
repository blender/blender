import string
import os
import sys
from distutils import sysconfig

# Build directory.
root_build_dir = '..' + os.sep + 'build' + os.sep

# Blender version.
config_file = 'config.opts'
version='2.32'

sdl_cenv = Environment ()
sdl_lenv = Environment ()
link_env = Environment ()
env = Environment ()

if sys.platform == 'linux2':
    use_international = 'false'
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
    platform_libs = ['m', 'util']
    platform_libpath = []
    platform_linkflags = []
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
    opengl_libpath = ['/usr/lib', '/usr/X11R6/lib']
    opengl_include = ['/usr/include']
    # SDL library information
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cflags = sdl_cenv.Dictionary()['CCFLAGS']
    sdl_include = sdl_cenv.Dictionary()['CPPPATH']
    sdl_libpath = sdl_lenv.Dictionary()['LIBPATH']
    sdl_lib = sdl_lenv.Dictionary()['LIBS']
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid/include']
    qhull_lib = []                                              # TODO
    qhull_libpath = []                                          # TODO
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = []                                                # TODO
    ode_libpath = []                                            # TODO
    ode_include = ['#extern/ode/dist/include/ode']
    # Python library information
    python_lib = ['python%d.%d' % sys.version_info[0:2]]
    python_libpath = [sysconfig.get_python_lib (0, 1) + '/config']
    python_include = [sysconfig.get_python_inc ()]
    # International support information
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/linux-glibc2.2.5-i386/ftgl/lib']
    ftgl_include = ['#../lib/linux-glibc2.2.5-i386/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = ['#../lib/linux-glibc2.2.5-i386/freetype/lib']
    freetype_include = ['#../lib/linux-glibc2.2.5-i386/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    i18n_defines = ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']

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
    opengl_lib = ['GL', 'GLU']
    opengl_libpath = []
    opengl_include = []
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

    platform_libs = ['stdc++'] 
    extra_includes = ['/sw/include']
    platform_libpath = ['/System/Library/Frameworks/OpenGL.framework/Libraries']
    platform_linkflags = []
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid/include']
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
    # International stuff
    ftgl_lib = ['ftgl']
    ftgl_libpath = [darwin_precomp + '/ftgl/lib']
    ftgl_include = [darwin_precomp + '/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = [darwin_precomp + '/freetype/lib']
    freetype_include = [darwin_precomp + '/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []
    i18n_defines = []

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
                     'mingw32']
    platform_libpath = ['/usr/lib/w32api', '/lib/w32api']
    platform_linkflags = ['-mwindows', '-mno-cygwin', '-mconsole']
    window_system = 'WIN32'
    extra_includes = ['/usr/include']
    # z library information
    z_lib = ['z']
    z_libpath = ['/usr/lib']
    z_include = ['/usr/include']
    # SDL specific stuff.
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cdict = sdl_cenv.Dictionary()
    sdl_ldict = sdl_lenv.Dictionary()
    sdl_cflags = '-DWIN32'
    # We need to force the Cygwin environment to use the g++ linker.
    link_env.Replace (CC='g++')
    # Python variables.
    python_include = sysconfig.get_python_inc ()
    python_libpath = sysconfig.get_python_lib (0, 1) + '/config'
    python_lib = 'python%d.%d' % sys.version_info[0:2]
    # International stuff
    ftgl_lib = ['ftgl']
    ftgl_libpath = ['#../lib/windows/ftgl/lib']
    ftgl_include = ['#../lib/windows/ftgl/include']
    freetype_lib = ['freetype']
    freetype_libpath = ['#../lib/windows/freetype/lib']
    freetype_include = ['#../lib/windows/freetype/include']
    gettext_lib = []
    gettext_libpath = []
    gettext_include = []

elif sys.platform == 'win32':
    use_international = 'true'
    use_gameengine = 'true'
    use_openal = 'true'
    use_fmod = 'false'
    use_quicktime = 'true'
    use_sumo = 'false'
    use_ode = 'true'
    release_flags = ['/G6', '/GF']
    debug_flags = ['/Zi']
    extra_flags = ['/EHsc', '/J', '/W3', '/Gd', '/MT']
    cxxflags = []
    defines = ['WIN32', '_CONSOLE']
    defines += ['WITH_QUICKTIME']
    defines += ['_LIB', 'USE_OPENAL']
    warn_flags = []
    platform_libs = [ 'qtmlClient', 'odelib', 'openal_static', 'soundsystem',
                     'ws2_32', 'dxguid', 'vfw32', 'winmm',
                     'iconv', 'kernel32', 'user32', 'gdi32',
                     'winspool', 'comdlg32', 'advapi32', 'shell32',
                     'ole32', 'oleaut32', 'uuid', 'odbc32', 'odbccp32',
                     'libcmt', 'libc']
    platform_libpath = ['#../lib/windows/iconv/lib',
                        '#../lib/windows/QTDevWin/Libraries',
                        '#../lib/windows/openal/lib']
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
    if use_openal == 'true':
        extra_includes += ['#../lib/windows/openal/include']
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
    opengl_libpath = []
    opengl_include = ['/usr/include']
    # SDL library information
    sdl_include = ['#../lib/windows/sdl/include']
    sdl_libpath = ['#../lib/windows/sdl/lib']
    sdl_lib = ['SDL']
    sdl_cflags = []
    link_env.RES(['source/icons/winblender.rc'])
    window_system = 'WIN32'
    # SOLID library information
    solid_lib = ['solid']
    solid_libpath = ['#../lib/windows/solid/lib']
    solid_include = ['#../lib/windows/solid/include']
    qhull_lib = ['qhull']
    qhull_libpath = ['#../lib/windows/qhull/lib']
    qhull_include = ['#extern/qhull/include']
    # ODE library information
    ode_lib = []                                                # TODO
    ode_libpath = ['#../lib/windows/ode/lib']
    ode_include = ['#../lib/windows/ode/include']
    # Python lib name
    python_include = '#../lib/windows/python/include/python2.2'
    python_libpath = '#../lib/windows/python/lib'
    python_lib = 'python22'
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
    i18n_defines = ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']

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
    platform_libs = ['m', 'util']
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
    opengl_lib = ['GL', 'GLU']
    opengl_libpath = []
    opengl_include = []
    # SDL library information
    sdl_cenv.ParseConfig ('sdl-config --cflags')
    sdl_lenv.ParseConfig ('sdl-config --libs')
    sdl_cflags = sdl_cenv.Dictionary()['CCFLAGS']
    sdl_include = sdl_cenv.Dictionary()['CPPPATH']
    sdl_libpath = sdl_lenv.Dictionary()['LIBPATH']
    sdl_lib = sdl_lenv.Dictionary()['LIBS']
    # SOLID library information
    solid_lib = []                                              # TODO
    solid_libpath = []                                          # TODO
    solid_include = ['#extern/solid/include']
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
    i18n_defines = ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']

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
    platform_libs = ['movieGL', 'Xmu', 'Xext', 'X11',
                     'c', 'm', 'dmedia', 'cl', 'audio',
                     'Cio', 'pthread']
    platform_libpath = ['/usr/lib32',
                        '/lib/freeware/lib32',
                        '/usr/lib32/mips3']
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
    i18n_defines = ['INTERNATIONAL', 'FTGL_STATIC_LIBRARY', 'WITH_FREETYPE2']

elif string.find (sys.platform, 'hp-ux') != -1:
    window_system = 'X11'
    defines = []

else:
    print "Unknown platform"

#-----------------------------------------------------------------------------
# End of platform specific section
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# User configurable options to be saved in a config file.
#-----------------------------------------------------------------------------
# Checking for an existing config file - use that one if it exists,
# otherwise create one.
if os.path.exists (config_file):
    print "Using config file: " + config_file
else:
    print "Creating new config file: " + config_file
    config=open (config_file, 'w')
    config.write ("# Configuration file containing user definable options.\n")
    config.write ("VERSION = '2.32-cvs'\n")
    config.write ("BUILD_BINARY = 'release'\n")
    config.write ("BUILD_DIR = '%s'\n"%(root_build_dir))
    config.write ("USE_INTERNATIONAL = '%s'\n"%(use_international))
    config.write ("BUILD_GAMEENGINE = '%s'\n"%(use_gameengine))
    if use_sumo == 'true':
        config.write ("USE_PHYSICS = 'solid'\n")
    else:
        config.write ("USE_PHYSICS = 'ode'\n")
    config.write ("USE_OPENAL = '%s'\n"%(use_openal))
    config.write ("USE_FMOD = '%s'\n"%(use_fmod))
    config.write ("USE_QUICKTIME = '%s'\n"%(use_quicktime))
    config.write ("\n# External library information.\n")
    config.write ("PYTHON_INCLUDE = %s\n"%(python_include))
    config.write ("PYTHON_LIBPATH = %s\n"%(python_libpath))
    config.write ("PYTHON_LIBRARY = %s\n"%(python_lib))
    config.write ("SDL_CFLAGS = %s\n"%(sdl_cflags))
    config.write ("SDL_INCLUDE = %s\n"%(sdl_include))
    config.write ("SDL_LIBPATH = %s\n"%(sdl_libpath))
    config.write ("SDL_LIBRARY = %s\n"%(sdl_lib))
    config.write ("Z_INCLUDE = %s\n"%(z_include))
    config.write ("Z_LIBPATH = %s\n"%(z_libpath))
    config.write ("Z_LIBRARY = %s\n"%(z_lib))
    config.write ("PNG_INCLUDE = %s\n"%(png_include))
    config.write ("PNG_LIBPATH = %s\n"%(png_libpath))
    config.write ("PNG_LIBRARY = %s\n"%(png_lib))
    config.write ("JPEG_INCLUDE = %s\n"%(jpeg_include))
    config.write ("JPEG_LIBPATH = %s\n"%(jpeg_libpath))
    config.write ("JPEG_LIBRARY = %s\n"%(jpeg_lib))
    config.write ("OPENGL_INCLUDE = %s\n"%(opengl_include))
    config.write ("OPENGL_LIBPATH = %s\n"%(opengl_libpath))
    config.write ("OPENGL_LIBRARY = %s\n"%(opengl_lib))
    config.write ("\n# The following information is only necessary when you've enabled support for\n")
    config.write ("# the game engine.\n")
    config.write ("SOLID_INCLUDE = %s\n"%(solid_include))
    config.write ("SOLID_LIBPATH = %s\n"%(solid_libpath))
    config.write ("SOLID_LIBRARY = %s\n"%(solid_lib))
    config.write ("QHULL_INCLUDE = %s\n"%(qhull_include))
    config.write ("QHULL_LIBPATH = %s\n"%(qhull_libpath))
    config.write ("QHULL_LIBRARY = %s\n"%(qhull_lib))
    config.write ("ODE_INCLUDE = %s\n"%(ode_include))
    config.write ("ODE_LIBPATH = %s\n"%(ode_libpath))
    config.write ("ODE_LIBRARY = %s\n"%(ode_lib))
    config.write ("\n# The following information is only necessary when building with\n")
    config.write ("# internationalization support.\n");
    config.write ("I18N_DEFINES = %s\n"%(i18n_defines))
    config.write ("FTGL_INCLUDE = %s\n"%(ftgl_include))
    config.write ("FTGL_LIBPATH = %s\n"%(ftgl_libpath))
    config.write ("FTGL_LIBRARY = %s\n"%(ftgl_lib))
    config.write ("FREETYPE_INCLUDE = %s\n"%(freetype_include))
    config.write ("FREETYPE_LIBPATH = %s\n"%(freetype_libpath))
    config.write ("FREETYPE_LIBRARY = %s\n"%(freetype_lib))
    config.write ("GETTEXT_INCLUDE = %s\n"%(gettext_include))
    config.write ("GETTEXT_LIBPATH = %s\n"%(gettext_libpath))
    config.write ("GETTEXT_LIBRARY = %s\n"%(gettext_lib))
    config.close ()

#-----------------------------------------------------------------------------
# Read the options from the config file and update the various necessary flags
#-----------------------------------------------------------------------------
list_opts = []
user_options_env = Environment ()
user_options = Options (config_file)
user_options.AddOptions (
        ('VERSION', 'Blender version', version),
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
        (EnumOption ('BUILD_BINARY', 'release',
                     'Select a release or debug binary.',
                     allowed_values = ('release', 'debug'))),
        ('PYTHON_INCLUDE', 'Include directory for Python header files.'),
        ('PYTHON_LIBPATH', 'Library path where the Python lib is located.'),
        ('PYTHON_LIBRARY', 'Python library name.'),
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
        ('SOLID_INCLUDE', 'Include directory for SOLID header files.'),
        ('SOLID_LIBPATH', 'Library path where the SOLID library is located.'),
        ('SOLID_LIBRARY', 'SOLID library name.'),
        ('QHULL_INCLUDE', 'Include directory for QHULL header files.'),
        ('QHULL_LIBPATH', 'Library path where the QHULL library is located.'),
        ('QHULL_LIBRARY', 'QHULL library name.'),
        ('ODE_INCLUDE', 'Include directory for ODE header files.'),
        ('ODE_LIBPATH', 'Library path where the ODE library is located.'),
        ('ODE_LIBRARY', 'ODE library name.'),
        ('I18N_DEFINES', 'Preprocessor defines needed for internationalization support.'),
        ('FTGL_INCLUDE', 'Include directory for ftgl header files.'),
        ('FTGL_LIBPATH', 'Library path where the ftgl library is located.'),
        ('FTGL_LIBRARY', 'ftgl library name.'),
        ('FREETYPE_INCLUDE', 'Include directory for freetype2 header files.'),
        ('FREETYPE_LIBPATH', 'Library path where the freetype2 library is located.'),
        ('FREETYPE_LIBRARY', 'Freetype2 library name.'),
        ('GETTEXT_INCLUDE', 'Include directory for gettext header files.'),
        ('GETTEXT_LIBPATH', 'Library path where the gettext library is located.'),
        ('GETTEXT_LIBRARY', 'gettext library name.')
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

if user_options_dict['USE_INTERNATIONAL']:
    defines += user_options_dict['I18N_DEFINES']

#-----------------------------------------------------------------------------
# Settings to be exported to other SConscript files
#-----------------------------------------------------------------------------

Export ('cflags')
Export ('defines')
Export ('cxxflags')
Export ('window_system')
Export ('extra_includes')
Export ('platform_libs')
Export ('platform_libpath')
Export ('platform_linkflags')
Export ('user_options_dict')

BuildDir (root_build_dir+'/intern', 'intern', duplicate=0)
SConscript (root_build_dir+'intern/SConscript')
BuildDir (root_build_dir+'/source', 'source', duplicate=0)
SConscript (root_build_dir+'source/SConscript')

libpath = (['#'+root_build_dir+'/lib'])

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

if user_options_dict['USE_INTERNATIONAL'] == 1:
    link_env.Append (LIBS=['blender_FTF'])
    link_env.Append (LIBS=user_options_dict['FTGL_LIBRARY'])
    link_env.Append (LIBPATH=user_options_dict['FTGL_LIBPATH'])
    link_env.Append (LIBS=user_options_dict['FREETYPE_LIBRARY'])
    link_env.Append (LIBPATH=user_options_dict['FREETYPE_LIBPATH'])
if user_options_dict['USE_QUICKTIME'] == 1:
    link_env.Append (LIBS=['blender_quicktime'])
if user_options_dict['BUILD_GAMEENGINE'] == 1:
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
    if user_options_dict['USE_PHYSICS'] == 'solid':
        link_env.Append (LIBS=['PHY_Sumo'])
        link_env.Append (LIBS=user_options_dict['SOLID_LIBRARY'])
        link_env.Append (LIBPATH=user_options_dict['SOLID_LIBPATH'])
        link_env.Append (LIBS=user_options_dict['QHULL_LIBRARY'])
        link_env.Append (LIBPATH=user_options_dict['QHULL_LIBPATH'])
    else:
        link_env.Append (LIBS=['PHY_Ode'])
        link_env.Append (LIBS=user_options_dict['ODE_LIBRARY'])
        link_env.Append (LIBPATH=user_options_dict['ODE_LIBPATH'])

link_env.Append (LIBS=user_options_dict['PYTHON_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['PYTHON_LIBPATH'])
link_env.Append (LIBS=user_options_dict['SDL_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['SDL_LIBPATH'])
link_env.Append (LIBS=user_options_dict['PNG_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['PNG_LIBPATH'])
link_env.Append (LIBS=user_options_dict['JPEG_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['JPEG_LIBPATH'])
link_env.Append (LIBS=user_options_dict['OPENGL_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['OPENGL_LIBPATH'])
link_env.Append (LIBS=user_options_dict['GETTEXT_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['GETTEXT_LIBPATH'])
link_env.Append (LIBS=user_options_dict['Z_LIBRARY'])
link_env.Append (LIBPATH=user_options_dict['Z_LIBPATH'])
link_env.Append (LIBS=platform_libs)
link_env.Append (LIBPATH=platform_libpath)
if sys.platform == 'darwin':
    link_env.Append (LINKFLAGS=' -framework Carbon')
    link_env.Append (LINKFLAGS=' -framework AGL')
    if user_options_dict['USE_QUICKTIME'] == 1:
        link_env.Append (LINKFLAGS=' -framework QuickTime')
else:
    link_env.Append (LINKFLAGS=platform_linkflags)

source_files = [root_build_dir+'source/creator/buildinfo.c',
                root_build_dir+'source/creator/creator.c']

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

link_env.BuildDir (root_build_dir, '.', duplicate=0)
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
