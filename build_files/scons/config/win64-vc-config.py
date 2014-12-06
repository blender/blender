import subprocess

CL_OUT = subprocess.Popen(["cl.exe"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
CL_STDOUT, CL_STDERR = CL_OUT.communicate()

if "18.00." in CL_STDERR:
    VC_VERSION = '12.0'
    LCGDIR = '#../lib/win64_vc12'
else:
    import sys
    print("Visual C version not supported {}\n".format(CL_STDERR))
    sys.exit(1)
 
LIBDIR = '${LCGDIR}'

WITH_BF_FFMPEG = True
BF_FFMPEG = LIBDIR +'/ffmpeg'
BF_FFMPEG_INC = '${BF_FFMPEG}/include ${BF_FFMPEG}/include/msvc '
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'
BF_FFMPEG_LIB = 'avformat-55.lib avcodec-55.lib avdevice-55.lib avutil-52.lib swscale-2.lib'
BF_FFMPEG_DLL = '${BF_FFMPEG_LIBPATH}/avformat-55.dll ${BF_FFMPEG_LIBPATH}/avcodec-55.dll ${BF_FFMPEG_LIBPATH}/avdevice-55.dll ${BF_FFMPEG_LIBPATH}/avutil-52.dll ${BF_FFMPEG_LIBPATH}/swscale-2.dll'
    

BF_PYTHON = LIBDIR + '/python'
BF_PYTHON_VERSION = '3.4'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = 'python'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION[0]}${BF_PYTHON_VERSION[2]}'
BF_PYTHON_DLL = '${BF_PYTHON_LIB}'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib'

WITH_BF_PYTHON_INSTALL_NUMPY = True

WITH_BF_OPENAL = True
BF_OPENAL = LIBDIR + '/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include '
BF_OPENAL_LIB = 'wrap_oal'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_SNDFILE = True
BF_SNDFILE = LIBDIR + '/sndfile'
BF_SNDFILE_INC = '${BF_SNDFILE}/include'
BF_SNDFILE_LIB = 'libsndfile-1'
BF_SNDFILE_LIBPATH = '${BF_SNDFILE}/lib'

WITH_BF_ICONV = True
BF_ICONV = LIBDIR + '/iconv'
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_SDL = True
BF_SDL = LIBDIR + '/sdl'
BF_SDL_INC = '${BF_SDL}/include'
BF_SDL_LIB = 'SDL2.lib'
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_JACK = False

BF_PTHREADS = LIBDIR + '/pthreads'
BF_PTHREADS_INC = '${BF_PTHREADS}/include'
BF_PTHREADS_LIB = 'pthreadVC2'
BF_PTHREADS_LIBPATH = '${BF_PTHREADS}/lib'

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = LIBDIR + '/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/OpenEXR '
BF_OPENEXR_LIB = ' Iex-2_1 Half IlmImf-2_1 Imath-2_1 IlmThread-2_1 '
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'

WITH_BF_DDS = True

WITH_BF_JPEG = True
BF_JPEG = LIBDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'libjpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = True
BF_PNG = LIBDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'libpng'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

WITH_BF_TIFF = True
BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'
BF_TIFF_LIB = 'libtiff'
BF_TIFF_LIBPATH = '${BF_TIFF}/lib'

WITH_BF_ZLIB = True
BF_ZLIB = LIBDIR + '/zlib'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'libz_st'
BF_ZLIB_LIBPATH = '${BF_ZLIB}/lib'

WITH_BF_INTERNATIONAL = True

WITH_BF_GAMEENGINE = True
WITH_BF_PLAYER = True
WITH_BF_OCEANSIM = True

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

WITH_BF_ELTOPO = False
BF_LAPACK = LIBDIR + '/lapack'
BF_LAPACK_LIB = 'libf2c clapack_nowrap BLAS_nowrap'
BF_LAPACK_LIBPATH = '${BF_LAPACK}/lib'

BF_WINTAB = LIBDIR + '/wintab'
BF_WINTAB_INC = '${BF_WINTAB}/INCLUDE'

WITH_BF_BINRELOC = False

BF_WITH_FREETYPE = True
BF_FREETYPE = LIBDIR + '/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype2ST'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = False
BF_QUICKTIME = LIBDIR + '/QTDevWin'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/CIncludes'
BF_QUICKTIME_LIB = 'qtmlClient'
BF_QUICKTIME_LIBPATH = '${BF_QUICKTIME}/Libraries'

WITH_BF_OPENJPEG = True
BF_OPENJPEG = '#extern/libopenjpeg'
BF_OPENJPEG_LIB = ''
BF_OPENJPEG_INC = '${BF_OPENJPEG}'
BF_OPENJPEG_LIBPATH='${BF_OPENJPEG}/lib'

WITH_BF_FFTW3 = True
BF_FFTW3 = LIBDIR + '/fftw3'
BF_FFTW3_INC = '${BF_FFTW3}/include'
BF_FFTW3_LIB = 'libfftw'
BF_FFTW3_LIBPATH = '${BF_FFTW3}/lib'

WITH_BF_REDCODE = False  
BF_REDCODE_INC = '#extern'

WITH_BF_COLLADA = True
BF_COLLADA = '#source/blender/collada'
BF_COLLADA_INC = '${BF_COLLADA}'
BF_COLLADA_LIB = 'bf_collada'

BF_OPENCOLLADA = LIBDIR + '/opencollada'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}/include/opencollada'
BF_OPENCOLLADA_LIB = 'OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser MathMLSolver xml pcre buffer ftoa'
BF_OPENCOLLADA_LIBPATH = '${BF_OPENCOLLADA}/lib/opencollada'

WITH_BF_IME = True

WITH_BF_3DMOUSE = True

WITH_BF_OPENMP = True

#Cycles
WITH_BF_CYCLES = True

WITH_BF_CYCLES_OSL = True
WITH_BF_STATICOSL = True
BF_OSL = '${LIBDIR}/osl'
BF_OSL_INC = '${BF_OSL}/include'
BF_OSL_LIBPATH = '${BF_OSL}/lib'
BF_OSL_LIB_STATIC = '${BF_OSL_LIBPATH}/oslcomp.lib ${BF_OSL_LIBPATH}/oslexec.lib ${BF_OSL_LIBPATH}/oslquery.lib '
BF_OSL_COMPILER = '${BF_OSL}/bin/oslc'

WITH_BF_LLVM = True
BF_LLVM = LIBDIR + '/llvm'
BF_LLVM_LIB = 'LLVMBitReader LLVMJIT LLVMipo LLVMVectorize LLVMBitWriter LLVMX86CodeGen LLVMX86Desc LLVMX86Info LLVMX86AsmPrinter ' + \
    'LLVMX86Utils LLVMSelectionDAG LLVMCodeGen LLVMScalarOpts LLVMInstCombine LLVMTransformUtils LLVMipa LLVMAnalysis LLVMExecutionEngine ' + \
    'LLVMTarget LLVMMC LLVMCore LLVMObject LLVMRuntimeDyld LLVMSupport'
BF_LLVM_LIBPATH = '${BF_LLVM}/lib'

WITH_BF_OIIO = True
BF_OIIO = '${LIBDIR}/openimageio'
BF_OIIO_INC = '${BF_OIIO}/include'
BF_OIIO_LIBPATH = '${BF_OIIO}/lib'
BF_OIIO_LIB_STATIC = '${BF_OIIO_LIBPATH}/OpenImageIO.lib ${BF_OIIO_LIBPATH}/OpenImageIO_Util.lib'
WITH_BF_STATICOIIO = True

WITH_BF_OCIO = True
BF_OCIO = '${LIBDIR}/opencolorio'
BF_OCIO_INC = '${BF_OCIO}/include'
BF_OCIO_LIBPATH = '${BF_OCIO}/lib'
BF_OCIO_LIB_STATIC = '${BF_OCIO_LIBPATH}/OpenColorIO.lib'
WITH_BF_STATICOCIO = True

WITH_BF_BOOST = True
BF_BOOST = '${LIBDIR}/boost'
BF_BOOST_INC = '${BF_BOOST}/include'
BF_BOOST_LIB = 'libboost_date_time-vc120-mt-s-1_55 libboost_filesystem-vc120-mt-s-1_55 libboost_regex-vc120-mt-s-1_55 libboost_system-vc120-mt-s-1_55 libboost_thread-vc120-mt-s-1_55 libboost_wave-vc120-mt-s-1_55'
BF_BOOST_LIB_INTERNATIONAL = ' libboost_locale-vc120-mt-s-1_55'
BF_BOOST_LIBPATH = '${BF_BOOST}/lib'

#CUDA
WITH_BF_CYCLES_CUDA_BINARIES = False
#BF_CYCLES_CUDA_NVCC = "" # Path to the nvidia compiler
BF_CYCLES_CUDA_BINARIES_ARCH = ['sm_20', 'sm_21', 'sm_30', 'sm_35', 'sm_50', 'sm_52']

#Ray trace optimization
WITH_BF_RAYOPTIMIZATION = True
# No need to manually specify SSE/SSE2 on x64 systems.
BF_RAYOPTIMIZATION_SSE_FLAGS = ['']

#Freestyle
WITH_BF_FREESTYLE = True

WITH_BF_STATICOPENGL = False
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIBINC = '${BF_OPENGL}/lib'
BF_OPENGL_LIB = 'opengl32 glu32'
BF_OPENGL_LIB_STATIC = [ '${BF_OPENGL}/lib/libGL.a', '${BF_OPENGL}/lib/libGLU.a',
                         '${BF_OPENGL}/lib/libXmu.a', '${BF_OPENGL}/lib/libXext.a',
                         '${BF_OPENGL}/lib/libX11.a', '${BF_OPENGL}/lib/libXi.a' ]
CC = 'cl.exe'
CXX = 'cl.exe'

CFLAGS = []
CCFLAGS = ['/nologo', '/J', '/W3', '/Gd', '/w34062', '/wd4018', '/wd4065', '/wd4127', '/wd4181', '/wd4200', '/wd4244', '/wd4267', '/wd4305', '/wd4800', '/we4013', '/we4431']

# We want to support Vista level ABI for x64
if VC_VERSION == '12.0':
    CCFLAGS.append('/D_WIN32_WINNT=0x600')
    CCFLAGS.append('/DOIIO_STATIC_BUILD')  # OIIO api changed with 1.4 making this needed 

CXXFLAGS = ['/EHsc']
BGE_CXXFLAGS = ['/O2', '/Ob2', '/EHsc', '/GR', '/fp:fast']

BF_DEBUG_CCFLAGS = ['/Zi', '/FR${TARGET}.sbr', '/Od', '/Ob0']

CPPFLAGS = ['-DWIN32', '-D_CONSOLE', '-D_LIB', '-D_CRT_SECURE_NO_DEPRECATE', '-DOPJ_STATIC']
REL_CFLAGS = []
REL_CXXFLAGS = []
REL_CCFLAGS = ['-O2', '/Ob2']

C_WARN = []
CC_WARN = []
CXX_WARN = []

LLIBS = ['ws2_32', 'vfw32', 'winmm', 'kernel32', 'user32', 'gdi32', 'comdlg32', 'advapi32', 'shfolder', 'shell32', 'ole32', 'oleaut32', 'uuid', 'psapi']

if WITH_BF_IME:
    LLIBS.append('imm32')

PLATFORM_LINKFLAGS = ['/SUBSYSTEM:CONSOLE','/MACHINE:X64','/STACK:2097152','/OPT:NOREF','/INCREMENTAL:NO', '/NODEFAULTLIB:msvcrt.lib', '/NODEFAULTLIB:msvcmrt.lib', '/NODEFAULTLIB:msvcurt.lib', '/NODEFAULTLIB:msvcrtd.lib']

BF_CYCLES_CUDA_ENV="C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd"
BF_BUILDDIR = '..\\build\\win64-vc'
BF_INSTALLDIR='..\\install\\win64-vc'
