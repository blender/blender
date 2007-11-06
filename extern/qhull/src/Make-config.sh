#!/bin/sh -e
#
# Make-config.sh
#
#     Setup for Debian build
#
#     Writes configure.in and Makefile.am files
#     and runs automake and autoconfig
#
#     Use 'make dist' to build Unix distribution.
#     Use 'configure; make' to build Qhull
#
#note:
#     'configure; make' does not work under cygwin.
#	src/unix.c:354: variable 'qh_qh' can't be auto-imported.
#	Please read the documentation for ld's --enable-auto-import for details.

###################################################
###########  ../configure.in ######################
###################################################

echo Create ../configure.in
cat >../configure.in <<\HERE-CONFIGURE
dnl configure.in for the qhull package
dnl Author: Rafael Laboissiere <rafael@debian.org>
dnl Created: Mon Dec  3 21:36:21 CET 2001

AC_INIT(src/qhull.c)
AM_INIT_AUTOMAKE(qhull, 2002.1)

AC_PROG_CC
AC_PROG_LIBTOOL

AC_OUTPUT([Makefile src/Makefile html/Makefile eg/Makefile])

HERE-CONFIGURE

###################################################
###########  ../Makefile.am #######################
###################################################

echo Create ../Makefile.am
cat >../Makefile.am <<\HERE-TOP
### Makefile.am for the qhull package (main)
### Author: Rafael Laboissiere <rafael@debian.org>
### Created: Mon Dec  3 21:36:21 CET 2001

### Documentation files

# to:
docdir = $(prefix)/share/doc/$(PACKAGE)

# which:
doc_DATA = \
  Announce.txt \
  COPYING.txt \
  README.txt \
  REGISTER.txt

### Extra files to be included in the tarball

EXTRA_DIST = \
  $(doc_DATA) \
  File_id.diz \
  QHULL-GO.pif

### Subdirectories for Automaking

SUBDIRS = src html eg

HERE-TOP

###################################################
###########  ../eg/Makefile.am ####################
###################################################

echo Create ../eg/Makefile.am
cat >../eg/Makefile.am <<\HERE-AM
### Makefile.am for the qhull package (eg)
### Author: Rafael Laboissiere <rafael@debian.org>
### Created: Mon Dec  3 21:36:21 CET 2001

### Documentation files

# to:
docdir = $(prefix)/share/doc/$(PACKAGE)
examplesdir = $(docdir)/examples

# which:
examples_DATA = \
  q_eg \
  q_egtest \
  q_test \
  Qhull-go.bat \
  q_test.bat

### Extra files to be included in the tarball

EXTRA_DIST = $(examples_DATA)

HERE-AM

###################################################
###########  ../html/Makefile.am ##################
###################################################

echo Create ../html/Makefile.am
cat >../html/Makefile.am <<\HERE-HTML
### Makefile.am for the qhull package (html)
### Author: Rafael Laboissiere <rafael@debian.org>
### Created: Mon Dec  3 21:36:21 CET 2001

### Man pages (trick to get around .man extension)

%.1: %.man
	cp $< $@
CLEANFILES = *.1
man_MANS = rbox.1 qhull.1

### Documentation files

# to:
docdir = $(prefix)/share/doc/$(PACKAGE)
htmldir = $(docdir)/html

# which:
html_DATA = \
  index.htm \
  qconvex.htm \
  qdelau_f.htm \
  qdelaun.htm \
  qh--4d.gif \
  qh--cone.gif \
  qh--dt.gif \
  qh--geom.gif \
  qh--half.gif \
  qh--rand.gif \
  qh-eg.htm \
  qh-faq.htm \
  qh-get.htm \
  qh-home.htm \
  qh-impre.htm \
  qh-in.htm \
  qh-optc.htm \
  qh-optf.htm \
  qh-optg.htm \
  qh-opto.htm \
  qh-optp.htm \
  qh-optq.htm \
  qh-optt.htm \
  qh-quick.htm \
  qhalf.htm \
  qhull.htm \
  qvoron_f.htm \
  qvoronoi.htm \
  rbox.htm

### Extra files to be included in the tarball

EXTRA_DIST = \
  $(html_DATA) \
  qhull.man \
  qhull.txt \
  rbox.man \
  rbox.txt

HERE-HTML

###################################################
###########  ../src/Makefile.am ###################
###################################################

echo Create ../src/Makefile.am
cat >../src/Makefile.am <<\HERE-SRC
### Makefile.am for the qhull package (src)
### Author: Rafael Laboissiere <rafael@debian.org>
### Created: Mon Dec  3 21:36:21 CET 2001

### Shared Library

# to:
lib_LTLIBRARIES = libqhull.la

# from:
libqhull_la_SOURCES = \
  user.c \
  global.c \
  stat.c \
  io.c \
  geom2.c \
  poly2.c \
  merge.c \
  qhull.c \
  geom.c \
  poly.c \
  qset.c \
  mem.c

# how:
libqhull_la_LDFLAGS = -version-info 0:0:0 -lm

### Utility programs

# to:
bin_PROGRAMS = qhull rbox qconvex qdelaunay qvoronoi qhalf

# from:
qhull_SOURCES = unix.c
rbox_SOURCES = rbox.c
qconvex_SOURCES = qconvex.c
qdelaunay_SOURCES = qdelaun.c
qvoronoi_SOURCES = qvoronoi.c
qhalf_SOURCES = qhalf.c

# how:
qhull_LDADD = libqhull.la
rbox_LDADD = libqhull.la
qconvex_LDADD = libqhull.la
qdelaunay_LDADD = libqhull.la
qvoronoi_LDADD = libqhull.la
qhalf_LDADD = libqhull.la

### Include files

pkginclude_HEADERS = \
  geom.h \
  mem.h \
  poly.h \
  qhull_a.h \
  stat.h \
  io.h \
  merge.h \
  qhull.h  \
  qset.h \
  user.h


### Example programs

# to:
docdir = $(prefix)/share/doc/$(PACKAGE)
examplesdir = $(docdir)/examples

# which:
examples_DATA = \
  user_eg.c \
  user_eg2.c \
  qhull_interface.cpp \
  Makefile.txt \
  Make-config.sh \
  MBorland

doc_DATA = Changes.txt \
    index.htm \
    qh-geom.htm \
    qh-globa.htm \
    qh-io.htm \
    qh-mem.htm \
    qh-merge.htm \
    qh-poly.htm \
    qh-qhull.htm \
    qh-set.htm \
    qh-stat.htm \
    qh-user.htm


### Extra files to be included in the tarball

EXTRA_DIST = \
  $(doc_DATA) \
  $(examples_DATA)

HERE-SRC

###################################################
###########  run automake autoconf ################
###################################################


echo Run automake, libtoolize, and autoconf
cd ..; aclocal &&\
  automake --foreign --add-missing --force-missing && \
  libtoolize --force && \
  autoconf

