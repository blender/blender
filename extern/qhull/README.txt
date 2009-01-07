Name

      qhull, rbox         2002.1           August 20, 2002
  
Convex hull, Delaunay triangulation, Voronoi diagrams, Halfspace intersection
 
      Documentation:
        html/index.htm

      Available from:
        <http://www.geom.umn.edu/software/qhull>
	<http://savannah.gnu.org/projects/qhull>
	<http://www.thesa.com/software/qhull>

      Version 1 (simplicial only):
        <http://www.geom.umn.edu/software/qhull/qhull-1.0.tar.gz>
        <http://www.geom.umn.edu/software/qhull/qhull.sit.hqx>
       
      News and a paper:
        <http://www.geom.umn.edu/~bradb/qhull-news.html>
        <http://www.geom.umn.edu/software/qhull/qhull-96.ps>

Purpose

  Qhull is a general dimension convex hull program that reads a set 
  of points from stdin, and outputs the smallest convex set that contains 
  the points to stdout.  It also generates Delaunay triangulations, Voronoi 
  diagrams, furthest-site Voronoi diagrams, and halfspace intersections
  about a point.  

  Rbox is a useful tool in generating input for Qhull; it generates 
  hypercubes, diamonds, cones, circles, simplices, spirals, 
  lattices, and random points.
  
  Qhull produces graphical output for Geomview.  This helps with
  understanding the output. <http://www.geomview.org>

    
Environment requirements

  Qhull and rbox should run on all 32-bit and 64-bit computers.  Use
  an ANSI C or C++ compiler to compile the program.  The software is 
  self-contained.  
  
  Qhull is copyrighted software.  Please read COPYING.txt and REGISTER.txt
  before using or distributing Qhull.

To contribute to Qhull

  Qhull is on Savannah, http://savannah.gnu.org/projects/qhull/

Qhull on Windows 95, 98, ME, NT, 2000, XP

  The zip file contains rbox.exe, qhull.exe, qconvex.exe, qdelaunay.exe, 
  qhalf.exe, qvoronoi.exe, documentation files, and source files.
  
  To install Qhull:
  - Unzip the files into a directory.  You may use WinZip32 <www.hotfiles.com>
  - Open a DOS window for the directory.  
  - In Windows 95, the DOS window needs improvement.
      - Double-click on qhull\eg\qhull-go.bat to call doskey (arrow keys).
      - Increase the size of the screen font to 8x12.
      - If the text is too dim, fix the screen colors with shareware (e.g., crt.exe)
  - If you use qhull a lot, consider using the Cygwin Unix shell,
        Cygwin tools (http://sources.redhat.com/cygwin/)
  - Execute 'qconvex' for a synopsis and examples.
  - Execute 'rbox 10 | qconvex' to compute the convex hull of 10 random points.
  - Execute 'rbox 10 | qconvex i TO file' to write results to 'file'. 
  - If an error occurs, Windows 95 sends the error to stdout instead of stderr 
      - use 'TO xxx' to send normal output to xxx and error output to stdout
  - Browse the documentation: qhull\html\index.htm

Compiling for Unix

  The gzip file, qhull.tgz, contains documentation and source files for
  qhull and rbox.  
  
  To unpack the gzip file
  - tar zxf qhull.tgz
  - cd qhull
  
  Compiling with the Debian Make:[R. Laboissiere]
  - cd src
  - ./Make-config.sh
  - cd ..
  - configure
  - make

  Compiling with Makefile (i.e., Makefile.txt)   
  - cd src
  - in Makefile, check the CC, CCOPTS1, PRINTMAN, and PRINTC defines
      - the defaults are gcc and enscript
      - CCOPTS1 should include the ANSI flag.  It defines __STDC__
  - in user.h, check the definitions of qh_SECticks and qh_CPUclock.
      - use '#define qh_CLOCKtype 2' for timing runs longer than 1 hour
  - type: make 
      - this builds: qhull qconvex qdelaunay qhalf qvoronoi rbox libqhull.a
  - type: make doc
      - this prints the man page
      - See also qhull/html/index.htm
  - if your compiler reports many errors, it is probably not a ANSI C compiler
      - you will need to set the -ansi switch or find another compiler
  - if your compiler warns about missing prototypes for fprintf() etc.
      - this is ok, your compiler should have these in stdio.h
  - if your compiler warns about missing prototypes for memset() etc.
      - include memory.h in qhull_a.h
  - if your compiler is gcc-2.95.1, you need to set flag -fno-strict-aliasing.  
      - This flag is set by default for other versions [Karas, Krishnaswami]
  - if your compiler reports "global.c: storage size of 'qh_qh' isn't known"
      - delete the initializer "={0}" in global.c, stat.c and mem.c
  - if your compiler warns about "stat.c: improper initializer"
      - this is ok, the initializer is not used
  - if you have trouble building libqhull.a with 'ar'
      - try 'make -f Makefile.txt qhullx' 
  - if the code compiles, the qhull test case will automatically execute
  - if an error occurs, there's an incompatibility between machines
      - For gcc-2.95.1, you need to set flag -fno-strict-aliasing.
	    It is set by default for other versions of gcc [Karas, Krishnaswami]
      - If you can, try a different compiler 
      - You can turn off the Qhull memory manager with qh_NOmem in mem.h
      - You can turn off compiler optimization (-O2 in Makefile)
      - If you find the source of the problem, please let us know
  - if you have Geomview (www.geomview.org)
       - try  'rbox 100 | qconvex G >a' and load 'a' into Geomview
       - run 'q_eg' for Geomview examples of Qhull output (see qh-eg.htm)
  - to install the programs and their man pages:
      - define MANDIR and BINDIR
      - type 'make install'

Compiling for Windows NT, 2000, XP with cygwin (www.cygwin.com)

    - install cygwin with gcc, make, ar, and ln
    - cd qhull/src
    - make -f Makefile.txt

Compiling for Windows 95, 98, NT, 2000, XP

  Qhull compiles as a console application in Visual C++ 5.0 at warning 
  level 3.

  Visual C++ quickstart for qhull.exe:  
    - create a "Win32 console application" called "qhull"
	- add the following files:
	    geom.c geom2.c global.c io.c mem.c merge.c poly.c poly2.c qhull.c
		qset.c stat.c unix.c user.c
    - create a "Win32 console application" called "rbox" 
	- add rbox.c

  Visual C++ quickstart for qhull library, qconvex.exe, etc.
    - To simplify setting up lots of projects, 
	- create a temporary "Win32 console application" called "source"
	- add all .c files from .../src/...
	- In Tools::Options::Tab
	  Set tab size to 8 and indent size to 2

    - create a "Win32 console application" called "rbox"
	- move rbox.c from "qhull source"
	- for Project:Settings..., Link
	  you only need the default libraries
	- build the project

    - create a "Win32 static library" called "library"
	- move these files from "qhull source"
	    geom.c geom2.c global.c io.c mem.c merge.c poly.c poly2.c qhull.c
		qset.c stat.c user.c
	- set the library file (use the same for debug and release)
	- build the project

    - create a "Win32 console application" called "qhull"
	- move unix.c from "qhull source"
	- Set the library file in Project:Settings..., Link
	- Qhull does not use other libraries

    - create a "Win32 console application" called "qconvex"
	- move qconvex.c from "qhull source"
	- Set the library file in Project:Settings..., Link

    - do the same for qdelaun.c, qhalf, qvoronoi.c, user_eg.c, user_eg2.c
	- delete "qhull sources" since it is no longer needed
	- Set the library file in Project:Settings..., Link
	- use Project:Settings to make any changes
	- use batch build to rebuild everything
  
  Qhull compiles with Borland C++ 5.0 bcc32.  A Makefile is included.
  Execute 'make -f MBorland'.  If you use the Borland IDE, set the ANSI
  option in Options:Project:Compiler:Source:Language-compliance.
  
  Qhull compiles with Borland C++ 4.02 for Win32 and DOS Power Pack.  
  Use 'make -f MBorland -D_DPMI'.  Qhull 1.0 compiles with Borland 
  C++ 4.02.  For rbox 1.0, use "bcc32 -WX -w- -O2-e -erbox -lc rbox.c".  
  Use the same options for Qhull 1.0. [D. Zwick]
  
  Qhull compiles with Metrowerks C++ 1.7 with the ANSI option.

  If you turn on full warnings, the compiler will report a number of 
  unused variables, variables set but not used, and dead code.  These are
  intentional.  For example, variables may be initialized (unnecessarily)
  to prevent warnings about possible use of uninitialized variables.  

Compiling for the Power Macintosh

  Qhull compiles for the Power Macintosh with Metrowerk's C compiler.
  It uses the SIOUX interface to read point coordinates and return output.
  There is no graphical output.  For project files, see 'Compiling for
  Windows 95'.  Instead of using SIOUX, Qhull may be embedded within an 
  application.  

  Version 1 is available for Macintosh computers by download of qhull.sit.hqx
  It reads point coordinates from a standard file and returns output
  to a standard file.  There is no graphical output.


Compiling for other machines
 
  Some users have reported problems with compiling Qhull under Irix 5.1.  It
  compiles under other versions of Irix. 
  
  If you have troubles with the memory manager, you can turn it off by
  defining qh_NOmem in mem.h.

  You may compile Qhull with a C++ compiler.  


Distributed files

  README.txt           // instructions for installing Qhull 
  REGISTER.txt         // Qhull registration 
  COPYING.txt          // copyright notice 
  Announce.txt         // announcement 
  Changes.txt          // change history for Qhull and rbox 
  qh-faq.htm           // Frequently asked questions
  qh-home.htm          // Home page 
  qh-get.htm	       // Download page
  html/index.htm       // Manual
  Makefile.txt         // Makefile for Unix or cygwin 'make' 
  MBorland             // Makefile for Borland C++/Win32
  Make-config.sh       // Create Debian configure and automake
 
src/      
  rbox consists of:
     rbox.exe          // Win32 executable (.zip only) 
     rbox.htm          // html manual 
     rbox.man          // Unix man page 
     rbox.txt
     rbox.c            // source program 
     
  qhull consists of:
     qhull.exe         // Win32 executables (.zip only) 
     qconvex.exe
     qdelaunay.exe
     qhalf.exe
     qvoronoi.exe
     qhull-go.bat      // DOS window
     qconvex.htm       // html manuals
     qdelaun.htm
     qdelau_f.htm        
     qhalf.htm
     qvoronoi.htm
     qvoron_f.htm
     qh-eg.htm
     qh-impre.htm
     qh-in.htm
     index.htm
     qh-opt*.htm
     qh-quick.htm
     qh--4d.gif,etc.   // images for manual 
     qhull.man         // Unix man page 
     qhull.txt
     q_eg              // shell script for Geomview examples
     q_egtest          // shell script for Geomview test examples
     q_test            // shell script to test qhull
  
  top-level source files:
     src/index.htm     // index to source files 
	 qh-...htm         //   specific files
     user.h            // header file of user definable constants 
     qhull.h           // header file for qhull 
     unix.c            // Unix front end to qhull 
     qhull.c           // Quickhull algorithm with partitioning 
     user.c            // user re-definable functions 
     user_eg.c         // example of incorporating qhull into a user program 
     user_eg2.c        // more complex example 
     qhull_interface.cpp // call Qhull from C++

  other source files:
     qhull_a.h         // include file for *.c 
     geom.c            // geometric routines 
     geom2.c
     geom.h	
     global.c          // global variables 
     io.c              // input-output routines 
     io.h
     mem.c             // memory routines, this is stand-alone code 
     mem.h
     merge.c           // merging of non-convex facets 
     merge.h
     poly.c            // polyhedron routines 
     poly2.c
     poly.h 
     qset.c            // set routines, this only depends on mem.c 
     qset.h
     stat.c            // statistics 
     stat.h

Authors:

  C. Bradford Barber                    Hannu Huhdanpaa
  bradb@geom.umn.edu                    hannu@geom.umn.edu
  
                    c/o The Geometry Center
                    University of Minnesota
                    400 Lind Hall
                    207 Church Street S.E.
                    Minneapolis, MN 55455
  
  This software was developed under NSF grants NSF/DMS-8920161 and
  NSF-CCR-91-15793 750-7504 at the Geometry Center and Harvard 
  University.  If you find Qhull useful, please let us know.
