LCGDIR = '#../lib/windows'
LIBDIR = "${LCGDIR}"

WITH_BF_VERSE = False
BF_VERSE_INCLUDE = "#extern/verse/dist"

BF_PYTHON = LIBDIR + '/python'
BF_PYTHON_VERSION = '2.5'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = 'python'
BF_PYTHON_LIB = 'python25'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib'

WITH_BF_OPENAL = True
WITH_BF_STATICOPENAL = False
BF_OPENAL = LIBDIR + '/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'dxguid openal_static'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'
# Warning, this static lib configuration is untested! users of this OS please confirm.
BF_OPENAL_LIB_STATIC = '${BF_OPENAL}/lib/libopenal.a'

WITH_BF_FFMPEG = False
BF_FFMPEG_LIB = 'avformat swscale avcodec avutil xvidcore x264'
BF_FFMPEG_LIBPATH = LIBDIR + '/gcc/ffmpeg/lib'
BF_FFMPEG_INC =  LIBDIR + '/gcc/ffmpeg/include'

WITH_BF_SDL = True
BF_SDL = LIBDIR + '/sdl'
BF_SDL_INC = '${BF_SDL}/include'
BF_SDL_LIB = 'SDL'
BF_SDL_LIBPATH = '${BF_SDL}/lib'

BF_PTHREADS = LIBDIR + '/pthreads'
BF_PTHREADS_INC = '${BF_PTHREADS}/include'
BF_PTHREADS_LIB = 'pthreadGC2'
BF_PTHREADS_LIBPATH = '${BF_PTHREADS}/lib'

WITH_BF_FMOD = False
BF_FMOD = LIBDIR + '/fmod'

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = LIBDIR + '/gcc/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = ' Half IlmImf Iex '
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'
# Warning, this static lib configuration is untested! users of this OS please confirm.
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'

WITH_BF_DDS = True

WITH_BF_JPEG = True
BF_JPEG = LIBDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = True
BF_PNG = LIBDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = True
BF_ZLIB = LIBDIR + '/zlib'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIBPATH = '${BF_ZLIB}/lib'

WITH_BF_INTERNATIONAL = True

BF_GETTEXT = LIBDIR + '/gettext'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'gnu_gettext'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = True
BF_FTGL = LIBDIR + '/ftgl'
BF_FTGL_INC = '${BF_FTGL}/include'
BF_FTGL_LIB = 'extern_ftgl'

WITH_BF_GAMEENGINE = False

WITH_BF_ODE = True
BF_ODE = LIBDIR + '/ode'
BF_ODE_INC = BF_ODE + '/include'
BF_ODE_LIB = BF_ODE + '/lib/libode.a'

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

BF_SOLID = '#extern/solid'
BF_SOLID_INC = '${BF_SOLID}'
BF_SOLID_LIB = 'extern_solid'

BF_WINTAB = LIBDIR + '/wintab'
BF_WINTAB_INC = '${BF_WINTAB}/INCLUDE'

WITH_BF_YAFRAY = True

#WITH_BF_NSPR = True
#BF_NSPR = $(LIBDIR)/nspr
#BF_NSPR_INC = -I$(BF_NSPR)/include -I$(BF_NSPR)/include/nspr
#BF_NSPR_LIB = 

# Uncomment the following line to use Mozilla inplace of netscape
#CPPFLAGS += -DMOZ_NOT_NET
# Location of MOZILLA/Netscape header files...
#BF_MOZILLA = $(LIBDIR)/mozilla
#BF_MOZILLA_INC = -I$(BF_MOZILLA)/include/mozilla/nspr -I$(BF_MOZILLA)/include/mozilla -I$(BF_MOZILLA)/include/mozilla/xpcom -I$(BF_MOZILLA)/include/mozilla/idl
#BF_MOZILLA_LIB =
# Will fall back to look in BF_MOZILLA_INC/nspr and BF_MOZILLA_LIB
# if this is not set.

# enable freetype2 support for text objects
BF_FREETYPE = LIBDIR + '/gcc/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = False # -DWITH_QUICKTIME
BF_QUICKTIME = '/usr/local'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/include'

WITH_BF_ICONV = False
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = False
BF_OPENGL = 'C:\\MingW'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIBINC = '${BF_OPENGL}/lib'
BF_OPENGL_LIB = 'opengl32 glu32'
BF_OPENGL_LIB_STATIC = [ '${BF_OPENGL}/lib/libGL.a', '${BF_OPENGL}/lib/libGLU.a',
             '${BF_OPENGL}/lib/libXmu.a', '${BF_OPENGL}/lib/libXext.a',
             '${BF_OPENGL}/lib/libX11.a', '${BF_OPENGL}/lib/libXi.a' ]
##
CC = 'gcc'
CXX = 'g++'

CCFLAGS = [ '-pipe', '-funsigned-char', '-fno-strict-aliasing' ]

CPPFLAGS = [ '-DXP_UNIX', '-DWIN32', '-DFREE_WINDOWS' ]
CXXFLAGS = ['-pipe', '-mwindows', '-funsigned-char', '-fno-strict-aliasing' ]
REL_CFLAGS = [ '-O2' ]
REL_CCFLAGS = [ '-O2' ]

C_WARN = [ '-Wno-char-subscripts', '-Wdeclaration-after-statement' ]

CC_WARN = [ '-Wall' ]

LLIBS = ['-lshell32', '-lshfolder', '-lgdi32', '-lmsvcrt', '-lwinmm', '-lmingw32', '-lm', '-lws2_32', '-lz', '-lstdc++']

BF_DEBUG = False
BF_DEBUG_FLAGS= ['-g']

BF_PROFILE_FLAGS = ['-pg','-g']
BF_PROFILE = False

BF_BUILDDIR = '..\\build\\win32-mingw'
BF_INSTALLDIR='..\\install\\win32-mingw'
BF_DOCDIR = '..\\install\\doc'