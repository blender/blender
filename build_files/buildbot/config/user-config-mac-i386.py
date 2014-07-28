
MACOSX_ARCHITECTURE = 'i386' # valid archs: ppc, i386, ppc64, x86_64

WITH_BF_CYCLES_CUDA_BINARIES = True

WITH_BF_CYCLES_OSL = False # OSL never worked on OSX 32bit !
WITH_BF_LLVM = False # no OSL -> no LLVM -> OpenCollada must have libUTF.a then !

BF_OPENCOLLADA_LIB = 'OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils OpenCOLLADAStreamWriter MathMLSolver GeneratedSaxParser xml2 buffer ftoa UTF'

