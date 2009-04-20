#
# $Id$
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2002 by Hans Lambermont
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****

include nan_definitions.mk

DISTDIR = dist
CP = ../../intern/tools/cpifdiff.sh
USERSETTINGS = ./dist/config/user-settings
TEMPSETTINGS = ./user-settings

all:
	[ -d $(DISTDIR)/lib ] || mkdir $(DISTDIR)/lib
	# prepare settings for patching, clean in case of interruption
    ifeq ($(OS),$(findstring $(OS), "darwin windows"))
	[ ! -f $(TEMPSETTINGS) ] || mv $(TEMPSETTINGS) $(USERSETTINGS) 
	cp $(USERSETTINGS) $(TEMPSETTINGS)
    endif
    ifeq ($(OS),freebsd)
	(grep FreeBSD $(DISTDIR)/Makefile >/dev/null ; \
	    [ $$? -eq 0 ] || patch < patchfile.FreeBSD )
    endif
    ifeq ($(OS),darwin)
	cat $(TEMPSETTINGS) | sed s/unix-gcc/osx/ > $(USERSETTINGS)
    endif
    ifeq ($(OS),windows)
	# compile with MSVC
	cat $(TEMPSETTINGS) | sed s/unix-gcc/msvc/ > $(USERSETTINGS)
	env PATH=".:$(PATH)" $(MAKE) -C $(DISTDIR)
    endif
    ifeq ($(OS),$(findstring $(OS), "freebsd linux darwin"))
	$(MAKE) -C $(DISTDIR)
    endif
	# restore settings
    ifeq ($(OS),$(findstring $(OS), "darwin windows"))
	mv $(TEMPSETTINGS) $(USERSETTINGS)
    endif

# fake debug target
debug:

install: all
    ifeq ($(OS),$(findstring $(OS), "freebsd linux darwin"))
	[ -d $(LCGDIR) ] || mkdir $(LCGDIR)
	[ -d $(NAN_ODE) ] || mkdir $(NAN_ODE)
	[ -d $(NAN_ODE)/include ] || mkdir $(NAN_ODE)/include
	[ -d $(NAN_ODE)/include/ode ] || mkdir $(NAN_ODE)/include/ode
	[ -d $(NAN_ODE)/lib ] || mkdir $(NAN_ODE)/lib
	[ -d $(NAN_ODE)/ode ] || mkdir $(NAN_ODE)/ode
	[ -d $(NAN_ODE)/ode/src ] || mkdir $(NAN_ODE)/ode/src
	@$(CP) $(DISTDIR)/lib/libode.a $(NAN_ODE)/lib/
	@$(CP) $(DISTDIR)/include/ode/*.h $(NAN_ODE)/include/ode/
	@$(CP) $(DISTDIR)/ode/src/array.h $(NAN_ODE)/ode/src/
	@$(CP) $(DISTDIR)/ode/src/joint.h $(NAN_ODE)/ode/src/
	@$(CP) $(DISTDIR)/ode/src/objects.h $(NAN_ODE)/ode/src/
	@$(CP) $(DISTDIR)/ode/src/obstack.h $(NAN_ODE)/ode/src/
    ifeq ($(OS),darwin)
	ranlib $(NAN_ODE)/lib/libode.a
    endif
    endif
    ifeq ($(OS),windows)
	@echo "====> $(MAKE) $@ in $(SOURCEDIR)"
	[ -d $(LCGDIR) ] || mkdir $(LCGDIR)
	[ -d $(NAN_ODE) ] || mkdir $(NAN_ODE)
	[ -d $(NAN_ODE)/include ] || mkdir $(NAN_ODE)/include
	[ -d $(NAN_ODE)/include/ode ] || mkdir $(NAN_ODE)/include/ode
	[ -d $(NAN_ODE)/lib ] || mkdir $(NAN_ODE)/lib
	[ -d $(NAN_ODE)/ode ] || mkdir $(NAN_ODE)/ode
	[ -d $(NAN_ODE)/ode/src ] || mkdir $(NAN_ODE)/ode/src
	cp $(DISTDIR)/lib/ode.lib $(NAN_ODE)/lib/libode.a
	cp $(DISTDIR)/include/ode/*.h $(NAN_ODE)/include/ode/
	cp $(DISTDIR)/ode/src/array.h $(NAN_ODE)/ode/src/
	cp $(DISTDIR)/ode/src/joint.h $(NAN_ODE)/ode/src/
	cp $(DISTDIR)/ode/src/objects.h $(NAN_ODE)/ode/src/
	cp $(DISTDIR)/ode/src/obstack.h $(NAN_ODE)/ode/src/
    endif

clean:
    ifeq ($(OS),$(findstring $(OS), "freebsd linux darwin"))
	[ ! -f dist/Makefile ] || $(MAKE) -C dist clean
    endif
    ifeq ($(OS),freebsd)
	(grep FreeBSD $(DISTDIR)/Makefile >/dev/null ; \
	    [ $$? -ne 0 ] || patch -R < patchfile.FreeBSD )
    endif

