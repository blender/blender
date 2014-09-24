BF_BUILDDIR = '../blender-build/linux-glibc211-i686'
BF_INSTALLDIR = '../blender-install/linux-glibc211-i686'
BF_NUMJOBS = 4
WITHOUT_BF_OVERWRITE_INSTALL = True

# Python configuration
BF_PYTHON_VERSION = '3.4'
BF_PYTHON_ABI_FLAGS = 'm'
BF_PYTHON = '/opt/lib/python-3.4'
WITH_BF_PYTHON_INSTALL_NUMPY = True
WITH_BF_PYTHON_INSTALL_REQUESTS = True

WITH_BF_STATICPYTHON = True

# OpenCollada configuration
WITH_BF_COLLADA = True
WITH_BF_STATICOPENCOLLADA=True
BF_OPENCOLLADA = '/opt/lib/opencollada'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}/include'
BF_OPENCOLLADA_LIB_STATIC = '${BF_OPENCOLLADA}/lib/libOpenCOLLADAStreamWriter.a ' + \
    '${BF_OPENCOLLADA}/lib/libOpenCOLLADASaxFrameworkLoader.a ' + \
    '${BF_OPENCOLLADA}/lib/libOpenCOLLADAFramework.a ' + \
    '${BF_OPENCOLLADA}/lib/libOpenCOLLADABaseUtils.a ' + \
    '${BF_OPENCOLLADA}/lib/libGeneratedSaxParser.a '  + \
    '${BF_OPENCOLLADA}/lib/libMathMLSolver.a ' + \
    '${BF_OPENCOLLADA}/lib/libbuffer.a ${BF_OPENCOLLADA}/lib/libftoa.a ' + \
    '/usr/lib/libxml2.a /usr/lib/libexpat.a /usr/lib/libpcre.a'
BF_OPENCOLLADA_LIBPATH = '${BF_OPENCOLLADA}/lib /home/sources/staticlibs/lib64'
BF_PCRE_LIB = ''
BF_EXPAT_LIB = ''

# FFMPEG configuration
WITH_BF_FFMPEG = True
WITH_BF_STATICFFMPEG = True

BF_FFMPEG = '/opt/lib/ffmpeg'
BF_FFMPEG_LIBPATH = '${BF_FFMPEG}/lib'
BF_FFMPEG_LIB_STATIC = '${BF_FFMPEG_LIBPATH}/libavformat.a ${BF_FFMPEG_LIBPATH}/libavdevice.a ' + \
    '${BF_FFMPEG_LIBPATH}/libavfilter.a ${BF_FFMPEG_LIBPATH}/libavcodec.a ${BF_FFMPEG_LIBPATH}/libavutil.a ' + \
    '${BF_FFMPEG_LIBPATH}/libswscale.a ${BF_FFMPEG_LIBPATH}/libswresample.a ' + \
    '/usr/lib/libxvidcore.a /usr/lib/libx264.a /usr/lib/libmp3lame.a /usr/lib/libvpx.a /usr/lib/libvorbis.a ' + \
    '/usr/lib/libogg.a /usr/lib/libvorbisenc.a /usr/lib/libtheora.a /usr/lib/libschroedinger-1.0.a ' + \
    '/usr/lib/liborc-0.4.a'

# Don't depend on system's libstdc++
WITH_BF_STATICCXX = True
BF_CXX_LIB_STATIC = '/usr/lib/gcc/i486-linux-gnu/4.7.1/libstdc++.a'

WITH_BF_OPENAL = True
WITH_BF_STATICOPENAL = True
BF_OPENAL = '/opt/lib/openal'
BF_OPENAL_LIB_STATIC = '/opt/lib/openal/lib/libopenal.a /opt/lib/openal/lib/libcommon.a'

WITH_BF_GETTEXT_STATIC = True

WITH_BF_FREETYPE_STATIC = False

WITH_BF_OPENEXR = True
BF_OPENEXR = '/opt/lib/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR ${BF_OPENEXR}/include'
WITH_BF_STATICOPENEXR = True

WITH_BF_TIFF = True
WITH_BF_STATICTIFF = True
BF_TIFF_LIB_STATIC = '${BF_TIFF}/lib/libtiff.a'

WITH_BF_JPEG = True
WITH_BF_STATICJPEG = True
BF_JPEG_LIB_STATIC= '${BF_JPEG}/lib/libjpeg.a'

WITH_BF_PNG = True
WITH_BF_STATICPNG = True
BF_PNG_LIB_STATIC = '${BF_PNG}/lib/libpng.a'

WITH_BF_STATICLIBSAMPLERATE = True

WITH_BF_ZLIB = True
WITH_BF_STATICZLIB = True
BF_ZLIB_LIB_STATIC = '${BF_ZLIB}/lib/libz.a'

WITH_BF_SDL = True
WITH_BF_OGG = True

WITH_BF_OPENMP = True
WITH_BF_STATICOPENMP = True
BF_OPENMP_LIB_STATIC = '/usr/lib/gcc/i486-linux-gnu/4.7/libgomp.a'

WITH_BF_GAMEENGINE = True
WITH_BF_BULLET = True

# Blender player (would be enabled in it's own config)
WITH_BF_PLAYER = False

# Use jemalloc memory manager
WITH_BF_JEMALLOC = True
WITH_BF_STATICJEMALLOC = True
BF_JEMALLOC = '/opt/lib/jemalloc'
BF_JEMALLOC_LIBPATH = '${BF_JEMALLOC}/lib'

# Use 3d mouse library
WITH_BF_3DMOUSE = True
WITH_BF_STATIC3DMOUSE = True
BF_3DMOUSE = '/opt/lib/libspnav'
BF_3DMOUSE_LIBPATH = '${BF_3DMOUSE}/lib'

# FFT
WITH_BF_FFTW3 = True
WITH_BF_STATICFFTW3 = True

# JACK
WITH_BF_JACK = True
WITH_BF_JACK_DYNLOAD = True

# Cycles
WITH_BF_CYCLES = True
WITH_BF_CYCLES_CUDA_BINARIES = False

WITH_BF_OIIO = True
WITH_BF_STATICOIIO = True
BF_OIIO = '/opt/lib/oiio'
BF_OIIO_INC = '${BF_OIIO}/include'
BF_OIIO_LIB_STATIC = '${BF_OIIO_LIBPATH}/libOpenImageIO.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_JPEG}/lib/libjpeg.a'
BF_OIIO_LIBPATH = '${BF_OIIO}/lib'

WITH_BF_CYCLES_OSL = True
WITH_BF_STATICOSL = False
BF_OSL = '/opt/lib/osl'
BF_OSL_INC = '${BF_OSL}/include'
# note oslexec would passed via program linkflags, which is needed to
# make llvm happy with osl_allocate_closure_component
BF_OSL_LIB = 'oslcomp oslexec oslquery'
BF_OSL_LIBPATH = '${BF_OSL}/lib'
BF_OSL_COMPILER = '${BF_OSL}/bin/oslc'

WITH_BF_LLVM = True
WITH_BF_STATICLLVM = False
BF_LLVM = '/opt/lib/llvm-3.4.2'
BF_LLVM_LIB = 'LLVMBitReader LLVMJIT LLVMipo LLVMVectorize LLVMBitWriter LLVMX86CodeGen LLVMX86Desc LLVMObject LLVMX86Info LLVMX86AsmPrinter ' + \
    'LLVMX86Utils LLVMSelectionDAG LLVMCodeGen LLVMScalarOpts LLVMInstCombine LLVMTransformUtils LLVMipa LLVMAnalysis LLVMExecutionEngine ' + \
    'LLVMTarget LLVMMC LLVMCore LLVMSupport'
BF_LLVM_LIBPATH = '${BF_LLVM}/lib'

# Color management
WITH_BF_OCIO = True
WITH_BF_STATICOCIO = True
BF_OCIO = '/opt/lib/ocio'
BF_OCIO_INC = '${BF_OCIO}/include'
BF_OCIO_LIB_STATIC = '${BF_OCIO_LIBPATH}/libOpenColorIO.a ${BF_OCIO_LIBPATH}/libtinyxml.a ${BF_OCIO_LIBPATH}/libyaml-cpp.a'
BF_OCIO_LIBPATH = '${BF_OCIO}/lib'

WITH_BF_BOOST = True
WITH_BF_STATICBOOST = True
BF_BOOST = '/opt/lib/boost'
BF_BOOST_INC = '${BF_BOOST}/include'
BF_BOOST_LIB_STATIC = '${BF_BOOST_LIBPATH}/libboost_filesystem.a ${BF_BOOST_LIBPATH}/libboost_date_time.a ' + \
    '${BF_BOOST_LIBPATH}/libboost_regex.a ${BF_BOOST_LIBPATH}/libboost_locale.a ${BF_BOOST_LIBPATH}/libboost_system.a \
    ${BF_BOOST_LIBPATH}/libboost_thread.a'
BF_BOOST_LIBPATH = '${BF_BOOST}/lib'

# Ocean Simulation
WITH_BF_OCEANSIM = True

# Compilation and optimization
BF_DEBUG = False
REL_CCFLAGS = ['-DNDEBUG', '-O2', '-msse', '-msse2']  # C & C++
PLATFORM_LINKFLAGS = ['-lrt']
BF_PROGRAM_LINKFLAGS = ['-Wl,--whole-archive', '-loslexec', '-Wl,--no-whole-archive', '-Wl,--version-script=source/creator/blender.map']
