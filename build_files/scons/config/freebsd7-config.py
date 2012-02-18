CC = 'gcc44'
CXX = 'g++44'

LCGDIR = '../lib/freebsd7'
LIBDIR = "${LCGDIR}"

BF_PYTHON_ABI_FLAGS = ''
BF_PYTHON = '/usr/local'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib'
BF_PYTHON_VERSION = '3.2'
WITH_BF_STATICPYTHON = False
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}${BF_PYTHON_ABI_FLAGS}'
BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}${BF_PYTHON_ABI_FLAGS}'
BF_PYTHON_LIB_STATIC = '${BF_PYTHON}/lib/libpython${BF_PYTHON_VERSION}${BF_PYTHON_ABI_FLAGS}.a'

WITH_BF_OPENAL = True
WITH_BF_STATICOPENAL = False
BF_OPENAL = '/usr/local'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'openal'
BF_OPENAL_LIB_STATIC = '${BF_OPENAL}/lib/libopenal.a'

BF_CXX = '/usr/local'
WITH_BF_STATICCXX = False
BF_CXX_LIB_STATIC = '${BF_CXX}/lib/libstdc++.a'

WITH_BF_JACK = True
BF_JACK = '/usr/local'
BF_JACK_INC = '${BF_JACK}/include/jack'
BF_JACK_LIB = 'jack'
BF_JACK_LIBPATH = '${BF_JACK}/lib'

WITH_BF_SNDFILE = True
BF_SNDFILE = '/usr/local'
BF_SNDFILE_INC = '${BF_SNDFILE}/include/sndfile'
BF_SNDFILE_LIB = 'sndfile'
BF_SNDFILE_LIBPATH = '${BF_SNDFILE}/lib'

WITH_BF_SDL = True
BF_SDL = '/usr/local' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include/SDL' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = '/usr/local'
# when compiling with your own openexr lib you might need to set...
# BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR ${BF_OPENEXR}/include'

BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Iex Imath '
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'
# BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'

WITH_BF_DDS = True

WITH_BF_JPEG = True
BF_JPEG = '/usr/local'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'

WITH_BF_PNG = True
BF_PNG = '/usr/local'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'

WITH_BF_TIFF = True
BF_TIFF = '/usr/local'
BF_TIFF_INC = '${BF_TIFF}/include'
BF_TIFF_LIB = 'tiff'

WITH_BF_ZLIB = True
BF_ZLIB = '/usr'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = True

BF_GETTEXT = '/usr/local'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'gettextlib'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_GAMEENGINE = False
WITH_BF_PLAYER = True
WITH_BF_OCEANSIM = True

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

# enable freetype2 support for text objects
BF_FREETYPE = '/usr/local'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'

### XXX Find what this actually wants; it doesn't want libquicktime.
WITH_BF_QUICKTIME = False # -DWITH_QUICKTIME
BF_QUICKTIME = '/usr/local'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/include'

WITH_BF_ICONV = True
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_BINRELOC = True

# enable ffmpeg  support
WITH_BF_FFMPEG = True  # -DWITH_FFMPEG
BF_FFMPEG = '/usr/local'
BF_FFMPEG_LIB = 'avformat avcodec swscale avutil avdevice'
BF_FFMPEG_INC = '${BF_FFMPEG}/include'
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'

# enable ogg, vorbis and theora in ffmpeg
WITH_BF_OGG = True
BF_OGG = '/usr/local'
BF_OGG_INC = '${BF_OGG}/include'
BF_OGG_LIB = 'ogg vorbis vorbisenc theoraenc theoradec'

WITH_BF_OPENJPEG = True
BF_OPENJPEG = '#extern/libopenjpeg'
BF_OPENJPEG_LIB = ''
BF_OPENJPEG_INC = '${BF_OPENJPEG}'
BF_OPENJPEG_LIBPATH='${BF_OPENJPEG}/lib'

WITH_BF_FFTW3 = True
BF_FFTW3 = LIBDIR + '/usr/local'
BF_FFTW3_INC = '${BF_FFTW3}/include'
BF_FFTW3_LIB = 'fftw3'
BF_FFTW3_LIBPATH = '${BF_FFTW3}/lib'

WITH_BF_REDCODE = False
BF_REDCODE = '#extern/libredcode'
BF_REDCODE_LIB = ''
# BF_REDCODE_INC = '${BF_REDCODE}/include'
BF_REDCODE_INC = '${BF_REDCODE}/../' #C files request "libredcode/format.h" which is in "#extern/libredcode/format.h", stupid but compiles for now.
BF_REDCODE_LIBPATH='${BF_REDCODE}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = False
BF_OPENGL = '/usr/local'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIB = 'GL GLU X11 Xi'
BF_OPENGL_LIBPATH = '/usr/X11R6/lib'
BF_OPENGL_LIB_STATIC = '${BF_OPENGL_LIBPATH}/libGL.a ${BF_OPENGL_LIBPATH}/libGLU.a ${BF_OPENGL_LIBPATH}/libXxf86vm.a ${BF_OPENGL_LIBPATH}/libX11.a ${BF_OPENGL_LIBPATH}/libXi.a ${BF_OPENGL_LIBPATH}/libXext.a ${BF_OPENGL_LIBPATH}/libXxf86vm.a'

WITH_BF_COLLADA = False
BF_COLLADA = '#source/blender/collada'
BF_COLLADA_INC = '${BF_COLLADA}'
BF_COLLADA_LIB = 'bf_collada'
BF_OPENCOLLADA = '/usr'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}'
BF_OPENCOLLADA_LIB = 'OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver pcre buffer ftoa'
BF_OPENCOLLADA_LIBPATH = '${BF_OPENCOLLADA}/lib'
BF_PCRE = '/usr/local'
BF_PCRE_LIB = 'pcre'
BF_PCRE_LIBPATH = '${BF_PCRE}/lib'
BF_EXPAT = '/usr/local'
BF_EXPAT_LIB = 'expat'
BF_EXPAT_LIBPATH = '${BF_EXPAT}/lib'

WITH_GHOST_XDND = False

WITH_BF_OPENMP = True

#Ray trace optimization
WITH_BF_RAYOPTIMIZATION = True
BF_RAYOPTIMIZATION_SSE_FLAGS = ['-msse','-pthread']

CCFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing','-D_LARGEFILE_SOURCE','-D_FILE_OFFSET_BITS=64','-D_LARGEFILE64_SOURCE']

CPPFLAGS = []
CXXFLAGS = []
if WITH_BF_FFMPEG:
    # libavutil needs UINT64_C()
    CXXFLAGS += ['-D__STDC_CONSTANT_MACROS', ]
REL_CFLAGS = []
REL_CXXFLAGS = []
REL_CCFLAGS = ['-DNDEBUG', '-O2']
##BF_DEPEND = True
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
C_WARN = ['-Wno-char-subscripts', '-Wdeclaration-after-statement', '-Wstrict-prototypes']
CC_WARN = ['-Wall']
CXX_WARN = ['-Wno-invalid-offsetof', '-Wno-sign-compare']


##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = ['util', 'c', 'm', 'pthread', 'stdc++']
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE = False
BF_PROFILE_CCFLAGS = ['-pg','-g']
BF_PROFILE_LINKFLAGS = ['-pg']

BF_DEBUG = False
BF_DEBUG_CCFLAGS = ['-g', '-D_DEBUG']

BF_BUILDDIR = '../build/freebsd7'
BF_INSTALLDIR='../install/freebsd7'

#Link against pthread
PLATFORM_LINKFLAGS = ['-pthread']
