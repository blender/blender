LCGDIR = '#../lib/mingw64'
LIBDIR = "${LCGDIR}"

BF_PYTHON = LIBDIR + '/python'
BF_PYTHON_VERSION = '3.4'
WITH_BF_STATICPYTHON = False
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = 'python'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION[0]}${BF_PYTHON_VERSION[2]}mw'
BF_PYTHON_DLL = 'python${BF_PYTHON_VERSION[0]}${BF_PYTHON_VERSION[2]}'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib'

WITH_BF_OPENAL = True
BF_OPENAL = LIBDIR + '/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'wrap_oal'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_FFMPEG = True
BF_FFMPEG_LIB = 'avformat.dll avcodec.dll avdevice.dll avutil.dll swscale.dll swresample.dll'
BF_FFMPEG_LIBPATH = LIBDIR + '/ffmpeg/lib'
BF_FFMPEG_INC =  LIBDIR + '/ffmpeg/include'
BF_FFMPEG_DLL = '${BF_FFMPEG_LIBPATH}/avformat-53.dll ${BF_FFMPEG_LIBPATH}/avcodec-53.dll ${BF_FFMPEG_LIBPATH}/avdevice-53.dll ${BF_FFMPEG_LIBPATH}/avutil-51.dll ${BF_FFMPEG_LIBPATH}/swscale-2.dll ${BF_FFMPEG_LIBPATH}/swresample-0.dll ${BF_FFMPEG_LIBPATH}/xvidcore.dll'

WITH_BF_JACK = False
BF_JACK = LIBDIR + '/jack'
BF_JACK_INC = '${BF_JACK}/include'
BF_JACK_LIB = 'libjack'
BF_JACK_LIBPATH = '${BF_JACK}/lib'

WITH_BF_SNDFILE = False
BF_SNDFILE = LIBDIR + '/sndfile'
BF_SNDFILE_INC = '${BF_SNDFILE}/include'
BF_SNDFILE_LIB = 'libsndfile-1'
BF_SNDFILE_LIBPATH = '${BF_SNDFILE}/lib'

WITH_BF_SDL = True
BF_SDL = LIBDIR + '/sdl'
BF_SDL_INC = '${BF_SDL}/include'
BF_SDL_LIB = 'SDL'
BF_SDL_LIBPATH = '${BF_SDL}/lib'

BF_PTHREADS = '' # Part of MinGW-w64
BF_PTHREADS_INC = ''
BF_PTHREADS_LIB = ''
BF_PTHREADS_LIBPATH = ''

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = LIBDIR + '/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Imath IlmThread Iex'
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'

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

WITH_BF_TIFF = True
BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'
BF_TIFF_LIB = 'tiff'
BF_TIFF_LIBPATH = '${BF_TIFF}/lib'

WITH_BF_ZLIB = True
BF_ZLIB = LIBDIR + '/zlib'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'
BF_ZLIB_LIBPATH = '${BF_ZLIB}/lib'

WITH_BF_INTERNATIONAL = True

WITH_BF_OPENJPEG = True
BF_OPENJPEG = '#extern/libopenjpeg'
BF_OPENJPEG_LIB = ''
BF_OPENJPEG_INC = '${BF_OPENJPEG}'
BF_OPENJPEG_LIBPATH='${BF_OPENJPEG}/lib'

WITH_BF_FFTW3 = True
BF_FFTW3 = LIBDIR + '/fftw3'
BF_FFTW3_INC = '${BF_FFTW3}/include'
BF_FFTW3_LIB = 'fftw3'
BF_FFTW3_LIBPATH = '${BF_FFTW3}/lib'

WITH_BF_GAMEENGINE = True
WITH_BF_OCEANSIM = True
WITH_BF_PLAYER = True
WITH_BF_LIBMV = True

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

BF_WINTAB = LIBDIR + '/wintab'
BF_WINTAB_INC = '${BF_WINTAB}/INCLUDE'

# enable freetype2 support for text objects
BF_FREETYPE = LIBDIR + '/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2/'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_ICONV = False
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_REDCODE = False
BF_REDCODE_INC = '#extern'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = False
BF_OPENGL = 'C:\\MingW'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIBINC = '${BF_OPENGL}/lib'
BF_OPENGL_LIB = 'opengl32 glu32'
BF_OPENGL_LIB_STATIC = [ '${BF_OPENGL}/lib/libGL.a', '${BF_OPENGL}/lib/libGLU.a',
             '${BF_OPENGL}/lib/libXmu.a', '${BF_OPENGL}/lib/libXext.a',
             '${BF_OPENGL}/lib/libX11.a', '${BF_OPENGL}/lib/libXi.a' ]

WITH_BF_COLLADA = True
BF_COLLADA = '#source/blender/collada'
BF_COLLADA_INC = '${BF_COLLADA}'
BF_COLLADA_LIB = 'bf_collada'

BF_OPENCOLLADA = LIBDIR + '/opencollada'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}/include/opencollada'
BF_OPENCOLLADA_LIB = 'OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver pcre buffer ftoa xml'
BF_OPENCOLLADA_LIBPATH = '${BF_OPENCOLLADA}/lib/opencollada'

#Cycles
WITH_BF_CYCLES = True
WITH_BF_CYCLES_CUDA_BINARIES = False
BF_CYCLES_CUDA_NVCC = "" # Path to the NVIDIA CUDA compiler
BF_CYCLES_CUDA_BINARIES_ARCH = ['sm_20', 'sm_21', 'sm_30', 'sm_35', 'sm_50']

WITH_BF_OIIO = True
BF_OIIO = LIBDIR + '/openimageio'
BF_OIIO_INC = '${BF_OIIO}/include'
BF_OIIO_LIB = 'OpenImageIO'
BF_OIIO_LIBPATH = '${BF_OIIO}/lib'

WITH_BF_OCIO = True
BF_OCIO = LIBDIR + '/opencolorio'
BF_OCIO_INC = '${BF_OCIO}/include'
BF_OCIO_LIB = 'OpenColorIO'
BF_OCIO_LIBPATH = '${BF_OCIO}/lib'

WITH_BF_BOOST = True
BF_BOOST = LIBDIR + '/boost'
BF_BOOST_INC = '${BF_BOOST}/include'
BF_BOOST_LIB = 'boost_date_time-mgw47-mt-s-1_49 boost_date_time-mgw47-mt-sd-1_49 boost_filesystem-mgw47-mt-s-1_49 boost_filesystem-mgw47-mt-sd-1_49 boost_regex-mgw47-mt-s-1_49 boost_regex-mgw47-mt-sd-1_49 boost_system-mgw47-mt-s-1_49 boost_system-mgw47-mt-sd-1_49 boost_thread-mgw47-mt-s-1_49 boost_thread-mgw47-mt-sd-1_49'
BF_BOOST_LIB_INTERNATIONAL = ' boost_locale-mgw47-mt-s-1_49 boost_locale-mgw47-mt-sd-1_49'
BF_BOOST_LIBPATH = '${BF_BOOST}/lib'

#Ray trace optimization
WITH_BF_RAYOPTIMIZATION = True
BF_RAYOPTIMIZATION_SSE_FLAGS = ['-mmmx', '-msse', '-msse2']

WITH_BF_OPENMP = True

#Freestyle
WITH_BF_FREESTYLE = True

##
CC = 'gcc'
CXX = 'g++'

CCFLAGS = [ '-pipe', '-funsigned-char', '-fno-strict-aliasing' ]
CXXFLAGS = [ '-fpermissive' ]

CPPFLAGS = ['-DWIN32', '-DMS_WIN64', '-DFREE_WINDOWS', '-DFREE_WINDOWS64', '-D_LARGEFILE_SOURCE', '-D_FILE_OFFSET_BITS=64', '-D_LARGEFILE64_SOURCE', '-DBOOST_ALL_NO_LIB', '-DBOOST_THREAD_USE_LIB', '-DGLEW_STATIC', '-DOPJ_STATIC']
REL_CFLAGS = []
REL_CXXFLAGS = []
REL_CCFLAGS = ['-O2', '-ftree-vectorize']

C_WARN = ['-Wno-char-subscripts', '-Wdeclaration-after-statement', '-Wstrict-prototypes']

CC_WARN = [ '-Wall' ]

LLIBS = ['-lshell32', '-lshfolder', '-lgdi32', '-lmsvcrt', '-lwinmm', '-lmingw32', '-lm', '-lws2_32', '-lz', '-lstdc++','-lole32','-luuid', '-lwsock32', '-lpsapi', '-lpthread']

PLATFORM_LINKFLAGS = ['-Xlinker', '--stack=2097152']

## DISABLED, causes linking errors!
## for re-distribution, so users dont need mingw installed
# PLATFORM_LINKFLAGS += ["-static-libgcc", "-static-libstdc++"]

BF_DEBUG = False
BF_DEBUG_CCFLAGS= ['-g']

BF_PROFILE_CCFLAGS = ['-pg', '-g']
BF_PROFILE_LINKFLAGS = ['-pg']
BF_PROFILE_FLAGS = BF_PROFILE_CCFLAGS
BF_PROFILE = False

BF_BUILDDIR = '..\\build\\win64-mingw'
BF_INSTALLDIR='..\\install\\win64-mingw'
