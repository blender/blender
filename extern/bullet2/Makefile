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
# Contributor(s):
#
# ***** END GPL LICENSE BLOCK *****
LIBNAME = bullet2
include nan_definitions.mk
SOURCEDIR = extern/$(LIBNAME)
DIR = $(OCGDIR)/extern/$(LIBNAME)
DIRS = src
DISTDIR = src

BULLETDIRS = \
LinearMath \
BulletCollision/BroadphaseCollision \
BulletCollision/CollisionShapes \
BulletCollision/NarrowPhaseCollision \
BulletCollision/Gimpact \
BulletCollision//CollisionDispatch \
BulletDynamics/ConstraintSolver \
BulletDynamics/Vehicle \
BulletDynamics/Dynamics \
BulletSoftBody

include nan_subdirs.mk

CP = $(NANBLENDERHOME)/intern/tools/cpifdiff.sh

install: all debug
	@[ -d $(NAN_BULLET2) ] || mkdir -p $(NAN_BULLET2)
	@[ -d $(NAN_BULLET2)/include ] || mkdir -p $(NAN_BULLET2)/include
	@for i in $(BULLETDIRS); do \
	[ -d $(NAN_BULLET2)/include/$$i ] || mkdir -p $(NAN_BULLET2)/include/$$i; \
	$(CP) $(DISTDIR)/$$i/*.h $(NAN_BULLET2)/include/$$i; \
	done
	@[ -d $(NAN_BULLET2)/lib ] || mkdir -p $(NAN_BULLET2)/lib
	@$(CP) $(DISTDIR)/*.h $(NAN_BULLET2)/include
	@$(CP) $(OCGDIR)/extern/bullet2/libbullet2.a $(NAN_BULLET2)/lib
ifeq ($(OS),darwin)
	ranlib $(NAN_BULLET2)/lib/libbullet2.a
endif
