LZMA SDK 4.65
-------------

LZMA SDK provides the documentation, samples, header files, libraries, 
and tools you need to develop applications that use LZMA compression.

LZMA is default and general compression method of 7z format
in 7-Zip compression program (www.7-zip.org). LZMA provides high 
compression ratio and very fast decompression.

LZMA is an improved version of famous LZ77 compression algorithm. 
It was improved in way of maximum increasing of compression ratio,
keeping high decompression speed and low memory requirements for 
decompressing.



LICENSE
-------

LZMA SDK is written and placed in the public domain by Igor Pavlov.


LZMA SDK Contents
-----------------

LZMA SDK includes:

  - ANSI-C/C++/C#/Java source code for LZMA compressing and decompressing
  - Compiled file->file LZMA compressing/decompressing program for Windows system


UNIX/Linux version 
------------------
To compile C++ version of file->file LZMA encoding, go to directory
C++/7zip/Compress/LZMA_Alone 
and call make to recompile it:
  make -f makefile.gcc clean all

In some UNIX/Linux versions you must compile LZMA with static libraries.
To compile with static libraries, you can use 
LIB = -lm -static


Files
---------------------
lzma.txt     - LZMA SDK description (this file)
7zFormat.txt - 7z Format description
7zC.txt      - 7z ANSI-C Decoder description
methods.txt  - Compression method IDs for .7z
lzma.exe     - Compiled file->file LZMA encoder/decoder for Windows
history.txt  - history of the LZMA SDK


Source code structure
---------------------

C/  - C files
        7zCrc*.*   - CRC code
        Alloc.*    - Memory allocation functions
        Bra*.*     - Filters for x86, IA-64, ARM, ARM-Thumb, PowerPC and SPARC code
        LzFind.*   - Match finder for LZ (LZMA) encoders 
        LzFindMt.* - Match finder for LZ (LZMA) encoders for multithreading encoding
        LzHash.h   - Additional file for LZ match finder
        LzmaDec.*  - LZMA decoding
        LzmaEnc.*  - LZMA encoding
        LzmaLib.*  - LZMA Library for DLL calling
        Types.h    - Basic types for another .c files
	Threads.*  - The code for multithreading.

    LzmaLib  - LZMA Library (.DLL for Windows)
    
    LzmaUtil - LZMA Utility (file->file LZMA encoder/decoder).

    Archive - files related to archiving
      7z     - 7z ANSI-C Decoder

CPP/ -- CPP files

  Common  - common files for C++ projects
  Windows - common files for Windows related code

  7zip    - files related to 7-Zip Project

    Common   - common files for 7-Zip

    Compress - files related to compression/decompression

      Copy         - Copy coder
      RangeCoder   - Range Coder (special code of compression/decompression)
      LZMA         - LZMA compression/decompression on C++
      LZMA_Alone   - file->file LZMA compression/decompression
      Branch       - Filters for x86, IA-64, ARM, ARM-Thumb, PowerPC and SPARC code

    Archive - files related to archiving

      Common   - common files for archive handling
      7z       - 7z C++ Encoder/Decoder

    Bundles    - Modules that are bundles of other modules
  
      Alone7z           - 7zr.exe: Standalone version of 7z.exe that supports only 7z/LZMA/BCJ/BCJ2
      Format7zR         - 7zr.dll: Reduced version of 7za.dll: extracting/compressing to 7z/LZMA/BCJ/BCJ2
      Format7zExtractR  - 7zxr.dll: Reduced version of 7zxa.dll: extracting from 7z/LZMA/BCJ/BCJ2.

    UI        - User Interface files
         
      Client7z - Test application for 7za.dll,  7zr.dll, 7zxr.dll
      Common   - Common UI files
      Console  - Code for console archiver



CS/ - C# files
  7zip
    Common   - some common files for 7-Zip
    Compress - files related to compression/decompression
      LZ     - files related to LZ (Lempel-Ziv) compression algorithm
      LZMA         - LZMA compression/decompression
      LzmaAlone    - file->file LZMA compression/decompression
      RangeCoder   - Range Coder (special code of compression/decompression)

Java/  - Java files
  SevenZip
    Compression    - files related to compression/decompression
      LZ           - files related to LZ (Lempel-Ziv) compression algorithm
      LZMA         - LZMA compression/decompression
      RangeCoder   - Range Coder (special code of compression/decompression)


C/C++ source code of LZMA SDK is part of 7-Zip project.
7-Zip source code can be downloaded from 7-Zip's SourceForge page:

  http://sourceforge.net/projects/sevenzip/



LZMA features
-------------
  - Variable dictionary size (up to 1 GB)
  - Estimated compressing speed: about 2 MB/s on 2 GHz CPU
  - Estimated decompressing speed: 
      - 20-30 MB/s on 2 GHz Core 2 or AMD Athlon 64
      - 1-2 MB/s on 200 MHz ARM, MIPS, PowerPC or other simple RISC
  - Small memory requirements for decompressing (16 KB + DictionarySize)
  - Small code size for decompressing: 5-8 KB

LZMA decoder uses only integer operations and can be 
implemented in any modern 32-bit CPU (or on 16-bit CPU with some conditions).

Some critical operations that affect the speed of LZMA decompression:
  1) 32*16 bit integer multiply
  2) Misspredicted branches (penalty mostly depends from pipeline length)
  3) 32-bit shift and arithmetic operations

The speed of LZMA decompressing mostly depends from CPU speed.
Memory speed has no big meaning. But if your CPU has small data cache, 
overall weight of memory speed will slightly increase.


How To Use
----------

Using LZMA encoder/decoder executable
--------------------------------------

Usage:  LZMA <e|d> inputFile outputFile [<switches>...]

  e: encode file

  d: decode file

  b: Benchmark. There are two tests: compressing and decompressing 
     with LZMA method. Benchmark shows rating in MIPS (million 
     instructions per second). Rating value is calculated from 
     measured speed and it is normalized with Intel's Core 2 results.
     Also Benchmark checks possible hardware errors (RAM 
     errors in most cases). Benchmark uses these settings:
     (-a1, -d21, -fb32, -mfbt4). You can change only -d parameter. 
     Also you can change the number of iterations. Example for 30 iterations:
       LZMA b 30
     Default number of iterations is 10.

<Switches>
  

  -a{N}:  set compression mode 0 = fast, 1 = normal
          default: 1 (normal)

  d{N}:   Sets Dictionary size - [0, 30], default: 23 (8MB)
          The maximum value for dictionary size is 1 GB = 2^30 bytes.
          Dictionary size is calculated as DictionarySize = 2^N bytes. 
          For decompressing file compressed by LZMA method with dictionary 
          size D = 2^N you need about D bytes of memory (RAM).

  -fb{N}: set number of fast bytes - [5, 273], default: 128
          Usually big number gives a little bit better compression ratio 
          and slower compression process.

  -lc{N}: set number of literal context bits - [0, 8], default: 3
          Sometimes lc=4 gives gain for big files.

  -lp{N}: set number of literal pos bits - [0, 4], default: 0
          lp switch is intended for periodical data when period is 
          equal 2^N. For example, for 32-bit (4 bytes) 
          periodical data you can use lp=2. Often it's better to set lc0, 
          if you change lp switch.

  -pb{N}: set number of pos bits - [0, 4], default: 2
          pb switch is intended for periodical data 
          when period is equal 2^N.

  -mf{MF_ID}: set Match Finder. Default: bt4. 
              Algorithms from hc* group doesn't provide good compression 
              ratio, but they often works pretty fast in combination with 
              fast mode (-a0).

              Memory requirements depend from dictionary size 
              (parameter "d" in table below). 

               MF_ID     Memory                   Description

                bt2    d *  9.5 + 4MB  Binary Tree with 2 bytes hashing.
                bt3    d * 11.5 + 4MB  Binary Tree with 3 bytes hashing.
                bt4    d * 11.5 + 4MB  Binary Tree with 4 bytes hashing.
                hc4    d *  7.5 + 4MB  Hash Chain with 4 bytes hashing.

  -eos:   write End Of Stream marker. By default LZMA doesn't write 
          eos marker, since LZMA decoder knows uncompressed size 
          stored in .lzma file header.

  -si:    Read data from stdin (it will write End Of Stream marker).
  -so:    Write data to stdout


Examples:

1) LZMA e file.bin file.lzma -d16 -lc0 

compresses file.bin to file.lzma with 64 KB dictionary (2^16=64K)  
and 0 literal context bits. -lc0 allows to reduce memory requirements 
for decompression.


2) LZMA e file.bin file.lzma -lc0 -lp2

compresses file.bin to file.lzma with settings suitable 
for 32-bit periodical data (for example, ARM or MIPS code).

3) LZMA d file.lzma file.bin

decompresses file.lzma to file.bin.


Compression ratio hints
-----------------------

Recommendations
---------------

To increase the compression ratio for LZMA compressing it's desirable 
to have aligned data (if it's possible) and also it's desirable to locate
data in such order, where code is grouped in one place and data is 
grouped in other place (it's better than such mixing: code, data, code,
data, ...).


Filters
-------
You can increase the compression ratio for some data types, using
special filters before compressing. For example, it's possible to 
increase the compression ratio on 5-10% for code for those CPU ISAs: 
x86, IA-64, ARM, ARM-Thumb, PowerPC, SPARC.

You can find C source code of such filters in C/Bra*.* files

You can check the compression ratio gain of these filters with such 
7-Zip commands (example for ARM code):
No filter:
  7z a a1.7z a.bin -m0=lzma

With filter for little-endian ARM code:
  7z a a2.7z a.bin -m0=arm -m1=lzma        

It works in such manner:
Compressing    = Filter_encoding + LZMA_encoding
Decompressing  = LZMA_decoding + Filter_decoding

Compressing and decompressing speed of such filters is very high,
so it will not increase decompressing time too much.
Moreover, it reduces decompression time for LZMA_decoding, 
since compression ratio with filtering is higher.

These filters convert CALL (calling procedure) instructions 
from relative offsets to absolute addresses, so such data becomes more 
compressible.

For some ISAs (for example, for MIPS) it's impossible to get gain from such filter.


LZMA compressed file format
---------------------------
Offset Size Description
  0     1   Special LZMA properties (lc,lp, pb in encoded form)
  1     4   Dictionary size (little endian)
  5     8   Uncompressed size (little endian). -1 means unknown size
 13         Compressed data


ANSI-C LZMA Decoder
~~~~~~~~~~~~~~~~~~~

Please note that interfaces for ANSI-C code were changed in LZMA SDK 4.58.
If you want to use old interfaces you can download previous version of LZMA SDK
from sourceforge.net site.

To use ANSI-C LZMA Decoder you need the following files:
1) LzmaDec.h + LzmaDec.c + Types.h
LzmaUtil/LzmaUtil.c is example application that uses these files.


Memory requirements for LZMA decoding
-------------------------------------

Stack usage of LZMA decoding function for local variables is not 
larger than 200-400 bytes.

LZMA Decoder uses dictionary buffer and internal state structure.
Internal state structure consumes
  state_size = (4 + (1.5 << (lc + lp))) KB
by default (lc=3, lp=0), state_size = 16 KB.


How To decompress data
----------------------

LZMA Decoder (ANSI-C version) now supports 2 interfaces:
1) Single-call Decompressing
2) Multi-call State Decompressing (zlib-like interface)

You must use external allocator:
Example:
void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
void SzFree(void *p, void *address) { p = p; free(address); }
ISzAlloc alloc = { SzAlloc, SzFree };

You can use p = p; operator to disable compiler warnings.


Single-call Decompressing
-------------------------
When to use: RAM->RAM decompressing
Compile files: LzmaDec.h + LzmaDec.c + Types.h
Compile defines: no defines
Memory Requirements:
  - Input buffer: compressed size
  - Output buffer: uncompressed size
  - LZMA Internal Structures: state_size (16 KB for default settings) 

Interface:
  int LzmaDecode(Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen,
      const Byte *propData, unsigned propSize, ELzmaFinishMode finishMode, 
      ELzmaStatus *status, ISzAlloc *alloc);
  In: 
    dest     - output data
    destLen  - output data size
    src      - input data
    srcLen   - input data size
    propData - LZMA properties  (5 bytes)
    propSize - size of propData buffer (5 bytes)
    finishMode - It has meaning only if the decoding reaches output limit (*destLen).
	 LZMA_FINISH_ANY - Decode just destLen bytes.
	 LZMA_FINISH_END - Stream must be finished after (*destLen).
                           You can use LZMA_FINISH_END, when you know that 
                           current output buffer covers last bytes of stream. 
    alloc    - Memory allocator.

  Out: 
    destLen  - processed output size 
    srcLen   - processed input size 

  Output:
    SZ_OK
      status:
        LZMA_STATUS_FINISHED_WITH_MARK
        LZMA_STATUS_NOT_FINISHED 
        LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK
    SZ_ERROR_DATA - Data error
    SZ_ERROR_MEM  - Memory allocation error
    SZ_ERROR_UNSUPPORTED - Unsupported properties
    SZ_ERROR_INPUT_EOF - It needs more bytes in input buffer (src).

  If LZMA decoder sees end_marker before reaching output limit, it returns OK result,
  and output value of destLen will be less than output buffer size limit.

  You can use multiple checks to test data integrity after full decompression:
    1) Check Result and "status" variable.
    2) Check that output(destLen) = uncompressedSize, if you know real uncompressedSize.
    3) Check that output(srcLen) = compressedSize, if you know real compressedSize. 
       You must use correct finish mode in that case. */ 


Multi-call State Decompressing (zlib-like interface)
----------------------------------------------------

When to use: file->file decompressing 
Compile files: LzmaDec.h + LzmaDec.c + Types.h

Memory Requirements:
 - Buffer for input stream: any size (for example, 16 KB)
 - Buffer for output stream: any size (for example, 16 KB)
 - LZMA Internal Structures: state_size (16 KB for default settings) 
 - LZMA dictionary (dictionary size is encoded in LZMA properties header)

1) read LZMA properties (5 bytes) and uncompressed size (8 bytes, little-endian) to header:
   unsigned char header[LZMA_PROPS_SIZE + 8];
   ReadFile(inFile, header, sizeof(header)

2) Allocate CLzmaDec structures (state + dictionary) using LZMA properties

  CLzmaDec state;
  LzmaDec_Constr(&state);
  res = LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc);
  if (res != SZ_OK)
    return res;

3) Init LzmaDec structure before any new LZMA stream. And call LzmaDec_DecodeToBuf in loop

  LzmaDec_Init(&state);
  for (;;)
  {
    ... 
    int res = LzmaDec_DecodeToBuf(CLzmaDec *p, Byte *dest, SizeT *destLen, 
    	const Byte *src, SizeT *srcLen, ELzmaFinishMode finishMode);
    ...
  }


4) Free all allocated structures
  LzmaDec_Free(&state, &g_Alloc);

For full code example, look at C/LzmaUtil/LzmaUtil.c code.


How To compress data
--------------------

Compile files: LzmaEnc.h + LzmaEnc.c + Types.h +
LzFind.c + LzFind.h + LzFindMt.c + LzFindMt.h + LzHash.h

Memory Requirements:
  - (dictSize * 11.5 + 6 MB) + state_size

Lzma Encoder can use two memory allocators:
1) alloc - for small arrays.
2) allocBig - for big arrays.

For example, you can use Large RAM Pages (2 MB) in allocBig allocator for 
better compression speed. Note that Windows has bad implementation for 
Large RAM Pages. 
It's OK to use same allocator for alloc and allocBig.


Single-call Compression with callbacks
--------------------------------------

Check C/LzmaUtil/LzmaUtil.c as example, 

When to use: file->file decompressing 

1) you must implement callback structures for interfaces:
ISeqInStream
ISeqOutStream
ICompressProgress
ISzAlloc

static void *SzAlloc(void *p, size_t size) { p = p; return MyAlloc(size); }
static void SzFree(void *p, void *address) {  p = p; MyFree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

  CFileSeqInStream inStream;
  CFileSeqOutStream outStream;

  inStream.funcTable.Read = MyRead;
  inStream.file = inFile;
  outStream.funcTable.Write = MyWrite;
  outStream.file = outFile;


2) Create CLzmaEncHandle object;

  CLzmaEncHandle enc;

  enc = LzmaEnc_Create(&g_Alloc);
  if (enc == 0)
    return SZ_ERROR_MEM;


3) initialize CLzmaEncProps properties;

  LzmaEncProps_Init(&props);

  Then you can change some properties in that structure.

4) Send LZMA properties to LZMA Encoder

  res = LzmaEnc_SetProps(enc, &props);

5) Write encoded properties to header

    Byte header[LZMA_PROPS_SIZE + 8];
    size_t headerSize = LZMA_PROPS_SIZE;
    UInt64 fileSize;
    int i;

    res = LzmaEnc_WriteProperties(enc, header, &headerSize);
    fileSize = MyGetFileLength(inFile);
    for (i = 0; i < 8; i++)
      header[headerSize++] = (Byte)(fileSize >> (8 * i));
    MyWriteFileAndCheck(outFile, header, headerSize)

6) Call encoding function:
      res = LzmaEnc_Encode(enc, &outStream.funcTable, &inStream.funcTable, 
        NULL, &g_Alloc, &g_Alloc);

7) Destroy LZMA Encoder Object
  LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);


If callback function return some error code, LzmaEnc_Encode also returns that code.


Single-call RAM->RAM Compression
--------------------------------

Single-call RAM->RAM Compression is similar to Compression with callbacks,
but you provide pointers to buffers instead of pointers to stream callbacks:

HRes LzmaEncode(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
    CLzmaEncProps *props, Byte *propsEncoded, SizeT *propsSize, int writeEndMark, 
    ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig);

Return code:
  SZ_OK               - OK
  SZ_ERROR_MEM        - Memory allocation error 
  SZ_ERROR_PARAM      - Incorrect paramater
  SZ_ERROR_OUTPUT_EOF - output buffer overflow
  SZ_ERROR_THREAD     - errors in multithreading functions (only for Mt version)



LZMA Defines
------------

_LZMA_SIZE_OPT - Enable some optimizations in LZMA Decoder to get smaller executable code.

_LZMA_PROB32   - It can increase the speed on some 32-bit CPUs, but memory usage for 
                 some structures will be doubled in that case.

_LZMA_UINT32_IS_ULONG  - Define it if int is 16-bit on your compiler and long is 32-bit.

_LZMA_NO_SYSTEM_SIZE_T  - Define it if you don't want to use size_t type.


C++ LZMA Encoder/Decoder 
~~~~~~~~~~~~~~~~~~~~~~~~
C++ LZMA code use COM-like interfaces. So if you want to use it, 
you can study basics of COM/OLE.
C++ LZMA code is just wrapper over ANSI-C code.


C++ Notes
~~~~~~~~~~~~~~~~~~~~~~~~
If you use some C++ code folders in 7-Zip (for example, C++ code for .7z handling),
you must check that you correctly work with "new" operator.
7-Zip can be compiled with MSVC 6.0 that doesn't throw "exception" from "new" operator.
So 7-Zip uses "CPP\Common\NewHandler.cpp" that redefines "new" operator:
operator new(size_t size)
{
  void *p = ::malloc(size);
  if (p == 0)
    throw CNewException();
  return p;
}
If you use MSCV that throws exception for "new" operator, you can compile without 
"NewHandler.cpp". So standard exception will be used. Actually some code of 
7-Zip catches any exception in internal code and converts it to HRESULT code.
So you don't need to catch CNewException, if you call COM interfaces of 7-Zip.

---

http://www.7-zip.org
http://www.7-zip.org/sdk.html
http://www.7-zip.org/support.html
