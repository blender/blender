# Unix Makefile for qhull and rbox
#
#       see README.txt
#
#       make           to produce qhull qconvex qdelaunay qhalf qvoronoi rbox
#       make qvoronoi  to produce qvoronoi (etc.)
#       make qhullx    to produce qhull qconvex etc.  w/o using libqhull.a
#       make doc       to print documentation
#       make install   to copy qhull, rbox, qhull.1, rbox.1 to BINDIR, MANDIR
#       make new       to rebuild qhull and rbox from source
#
#       make printall  to print all files
#       make user_eg   to produce user_eg
#       make user_eg2  to produce user_eg2
#       make clean     to remove object files and core
#       make cleanall  to remove all generated files
#
#       PRINTMAN --  command for printing manual pages
#       PRINTC --  command for printing C files
#       BINDIR -- directory where to copy executables
#       MANDIR -- directory where to copy manual pages
#       CC --     ANSI C or C++ compiler
#       CCOPTS1 - options used to compile .c files
#       CCOPTS2 -- options used to link .o files
#
#       CFILES -- .c files for printing
#       HFILES -- .h files for printing
#       DFILES -- documentation files
#       MFILES -- man pages and html files
#       TFILES -- .txt versions of html html files
#       FILES -- all other files
#       OBJS -- specifies the object files of libqhull.a
#
BINDIR  = /usr/local/bin
MANDIR  = /usr/local/man/man1

# if you do not have enscript, try a2ps or just use lpr.  The files are text.
PRINTMAN = enscript -2rl
PRINTC = enscript -2r
# PRINTMAN = lpr
# PRINTC = lpr

#for Gnu's gcc compiler -O2 for optimization, -g for debugging, -Wall for check
#
CC     = gcc
CCOPTS1 = -O2 -ansi 

# for Sun's cc compiler, -fast or O2 for optimization, -g for debugging, -Xc for ANSI
#CC = cc
#CCOPTS1 = -Xc -v -fast

# for Silicon Graphics cc compiler, -O2 for optimization, -g for debugging
#CC = cc
#CCOPTS1 = -ansi -O2

# for Next cc compiler with fat executable
#CC = cc
#CCOPTS1 = -ansi -O2 -arch m68k -arch i386 -arch hppa

# for loader, ld
CCOPTS2 = $(CCOPTS1)

# OBJS in execution frequency order.  CFILES after qhull.c are alphabetical
OBJS = user.o global.o stat.o io.o geom2.o poly2.o \
       merge.o qhull.o geom.o poly.o qset.o mem.o

CFILES= unix.c qhull.c geom.c geom2.c global.c io.c mem.c merge.c poly.c \
        poly2.c qset.c stat.c user.c qconvex.c qdelaun.c qhalf.c qvoronoi.c
HFILES= user.h qhull.h qhull_a.h geom.h io.h mem.h merge.h poly.h qset.h stat.h
TXTFILES= ../Announce.txt ../REGISTER.txt ../COPYING.txt ../README.txt Changes.txt
DOCFILES= ../html/rbox.txt ../html/qhull.txt
FILES=  Makefile rbox.c user_eg.c ../eg/q_test ../eg/q_egtest ../eg/q_eg
HTMFILES= qhull.man rbox.man qh-in.htm qh-optg.htm qh-optt.htm qh-optp.htm \
        index.htm qh-quick.htm qh-impre.htm qh-eg.htm \
        qh-optc.htm qh-opto.htm qh-optf.htm qh-optq.htm \
	    qh-c.htm qh-faq.htm qhull.htm qconvex.htm qdelaun.htm \
		qh-geom.htm qh-globa.htm qh-io.htm qh-mem.htm qh-merge.htm \
		qh-poly.htm qh-qhull.htm qh-set.htm qh-stat.htm qh-user.htm \
		qdelau_f.htm qhalf.htm qvoronoi.htm qvoron_f.htm rbox.htm 

all: rbox qconvex qdelaunay qhalf qvoronoi qhull

unix.o:   qhull.h user.h mem.h
qconvex.o:   qhull.h user.h mem.h
qdelaun.o:   qhull.h user.h mem.h
qhalf.o:   qhull.h user.h mem.h
qvoronoi.o:   qhull.h user.h mem.h
qhull.o:  $(HFILES)
geom.o:   $(HFILES)
geom2.o:  $(HFILES)
global.o: $(HFILES)
io.o:     $(HFILES)
mem.o:    mem.h 
merge.o:  $(HFILES)
poly.o:   $(HFILES)
poly2.o:  $(HFILES)
qset.o:   qset.h mem.h 
stat.o:   $(HFILES)
user.o:   $(HFILES)

.c.o:
	$(CC) -c $(CCOPTS1) $<

clean:
	rm -f *.o ../core qconvex qdelaunay qhalf qvoronoi qhull libqhull.a \
	    *.exe

cleanall: clean
	rm -f *~ ../rbox ../qhull ../qhalf ../qconvex ../qdelaunay ../qhalf\
	   ../qvoronoi ../user_eg ../user_eg2 ../*.exe >/dev/null

doc: 
	$(PRINTMAN) $(TXTFILES) $(DOCFILES)

install: all 
	cp ../qconvex $(BINDIR)/qconvex
	cp ../qdelaunay $(BINDIR)/qdelaunay
	cp ../qhalf $(BINDIR)/qhalf
	cp ../qhull $(BINDIR)/qhull
	cp ../qvoronoi $(BINDIR)/qvoronoi
	cp ../rbox $(BINDIR)/rbox
	cp ../html/qhull.man $(MANDIR)/qhull.1
	cp ../html/rbox.man $(MANDIR)/rbox.1

new:    cleanall all

printall: doc printh printc printf

printh:
	$(PRINTC) $(HFILES)

printc:
	$(PRINTC) $(CFILES)

printf:
	$(PRINTC) $(FILES) 

libqhull.a: $(OBJS)
	@echo if 'ar' or 'ranlib' fails, try 'make qhullx'
	ar r libqhull.a $(OBJS)
	@echo the next line may need to be removed.
	-test -x /bin/ranlib -o -x /usr/bin/ranlib && ranlib libqhull.a

# don't use ../qconvex.  Does not work on Red Hat Linux
qconvex: qconvex.o libqhull.a
	$(CC) -o qconvex $(CCOPTS2) qconvex.o -L. -lqhull -lm 
	cp qconvex ..

qdelaunay: qdelaun.o libqhull.a
	$(CC) -o qdelaunay $(CCOPTS2) qdelaun.o -L. -lqhull -lm 
	cp qdelaunay ..

qhalf: qhalf.o libqhull.a
	$(CC) -o qhalf $(CCOPTS2) qhalf.o -L. -lqhull -lm 
	cp qhalf ..

qvoronoi: qvoronoi.o libqhull.a
	$(CC) -o qvoronoi $(CCOPTS2) qvoronoi.o -L. -lqhull -lm 
	cp qvoronoi ..

qhull: unix.o libqhull.a
	$(CC) -o qhull $(CCOPTS2) unix.o -L. -lqhull -lm 
	cp qhull ..
	-chmod +x ../eg/q_test ../eg/q_eg ../eg/q_egtest
	-cd ..; ./rbox D4 | ./qhull

# compile qhull without using libqhull.a
qhullx: qconvex.o qdelaun.o qhalf.o qvoronoi.o unix.o $(OBJS)
	$(CC) -o qconvex $(CCOPTS2) qconvex.o $(OBJS) -lm 
	$(CC) -o qdelaunay $(CCOPTS2) qdelaun.o $(OBJS) -lm 
	$(CC) -o qhalf $(CCOPTS2) qhalf.o $(OBJS) -lm 
	$(CC) -o qvoronoi $(CCOPTS2) qvoronoi.o $(OBJS) -lm 
	$(CC) -o qhull $(CCOPTS2) unix.o $(OBJS) -lm 
	cp qconvex qdelaunay qhalf qvoronoi qhull ..
	-chmod +x ../eg/q_test ../eg/q_eg ../eg/q_egtest
	-cd ..; ./rbox D4 | ./qhull

rbox: rbox.o
	$(CC) -o rbox rbox.o $(CCOPTS2) -lm
	cp rbox ..

user_eg: user_eg.o libqhull.a 
	$(CC)  -o user_eg $(CCOPTS2) user_eg.o  -L. -lqhull -lm 
	cp user_eg ..

user_eg2: user_eg2.o libqhull.a 
	$(CC)  -o user_eg2 $(CCOPTS2) user_eg2.o  -L. -lqhull -lm 
	cp user_eg2 ..

# end of Makefile
