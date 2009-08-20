#
# a very simple Makefile for miniLZO
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

PROGRAM = testmini
SOURCES = testmini.c minilzo.c

default:
	@echo "Please choose one of the following targets:"
	@echo "    gcc:   gcc"
	@echo "    unix:  hpux hpux9"
	@echo "    win32: win32-bc win32-cygwin win32-dm win32-lccwin32"
	@echo "           win32-intelc win32-mingw win32-vc win32-watcomc"
	@echo "    dos16: dos16-bc dos16-mc dos16-wc"
	@echo "    dos32: dos32-djgpp2 dos32-wc"


# Make sure that minilzo.h, lzoconf.h and lzodefs.h are in the
# current dircectory. Otherwise you may want to adjust CPPFLAGS.
##CPPFLAGS = -I../include/lzo -I.

GCC_CFLAGS = -s -Wall -O2 -fomit-frame-pointer


#
# gcc (generic)
#

gcc:
	gcc $(CPPFLAGS) $(GCC_CFLAGS) -o $(PROGRAM) $(SOURCES)

cc:
	cc $(CPPFLAGS) -o $(PROGRAM) $(SOURCES)


#
# UNIX
#

hpux:
	cc -Ae $(CPPFLAGS) -o $(PROGRAM) $(SOURCES)

hpux9:
	cc -Aa -D_HPUX_SOURCE $(CPPFLAGS) -o $(PROGRAM) $(SOURCES)


#
# Windows (32-bit)
#

win32-borlandc win32-bc:
	bcc32 -O2 -d -w -w-aus $(CPPFLAGS) $(SOURCES)

win32-cygwin32 win32-cygwin:
	gcc -mcygwin $(CPPFLAGS) $(GCC_CFLAGS) -o $(PROGRAM).exe $(SOURCES)

win32-digitalmars win32-dm:
	dmc -mn -o -w- $(CPPFLAGS) $(SOURCES)

win32-intelc win32-ic:
	icl -nologo -MD -W3 -O2 -GF $(CPPFLAGS) $(SOURCES)

win32-lccwin32:
	@echo "NOTE: need lcc 2002-07-25 or newer, older versions have bugs"
	lc -A -unused -O $(CPPFLAGS) $(SOURCES)

win32-mingw32 win32-mingw:
	gcc -mno-cygwin $(CPPFLAGS) $(GCC_CFLAGS) -o $(PROGRAM).exe $(SOURCES)

win32-visualc win32-vc:
	cl -nologo -MD -W3 -O2 -GF $(CPPFLAGS) $(SOURCES)

win32-watcomc win32-wc:
	wcl386 -bt=nt -zq -mf -5r -zc -w5 -oneatx $(CPPFLAGS) $(SOURCES)


#
# DOS (16-bit)
#

dos16-borlandc dos16-bc:
	bcc -ml -w -d -O -4 $(CPPFLAGS) $(SOURCES)

dos16-microsoftc dos16-msc dos16-mc:
	cl -nologo -f- -AL -O -G2 -W3 $(CPPFLAGS) $(SOURCES)

dos16-watcomc dos16-wc:
	wcl -zq -ml -bt=dos -l=dos -ox -w5 $(CPPFLAGS) $(SOURCES)


#
# DOS (32-bit)
#

dos32-djgpp2 dos32-dj2:
	gcc $(CPPFLAGS) $(GCC_CFLAGS) -o $(PROGRAM).exe $(SOURCES)

dos32-watcomc dos32-wc:
	wcl386 -zq -mf -bt=dos -l=dos4g -5r -ox -zc $(CPPFLAGS) $(SOURCES)


#
# other targets
#

clean:
	rm -f $(PROGRAM) $(PROGRAM).exe $(PROGRAM).map $(PROGRAM).tds
	rm -f *.err *.o *.obj

.PHONY: default clean

