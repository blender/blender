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
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****
#

LIBNAME = bullet2
DIR = $(OCGDIR)/extern/$(LIBNAME)

BULLETDIRS = \
LinearMath \
BulletCollision/BroadphaseCollision \
BulletCollision/CollisionShapes \
BulletCollision/NarrowPhaseCollision \
BulletCollision/CollisionDispatch \
BulletCollision/Gimpact \
BulletDynamics/ConstraintSolver \
BulletDynamics/Vehicle \
BulletDynamics/Dynamics \
BulletSoftBody

CCSRCS = $(wildcard \
LinearMath/*.cpp \
BulletCollision/BroadphaseCollision/*.cpp \
BulletCollision/CollisionShapes/*.cpp \
BulletCollision/NarrowPhaseCollision/*.cpp \
BulletCollision/CollisionDispatch/*.cpp \
BulletCollision/Gimpact/*.cpp \
BulletDynamics/ConstraintSolver/*.cpp \
BulletDynamics/Vehicle/*.cpp \
BulletDynamics/Dynamics/*.cpp \
BulletSoftBody/*.cpp)

CPPFLAGS += -D_LIB -I. -IBulletCollision -IBulletDynamics -ILinearMath

all debug:: objdirs

include nan_compile.mk

.PHONY: objdirs clean
objdirs:
	@for i in $(BULLETDIRS); do \
	[ -d $(DIR)/$(DEBUG_DIR)$$i ] || mkdir -p $(DIR)/$(DEBUG_DIR)$$i; \
	done

clean::
	rm -rf $(DIR)
	rm -rf $(NAN_BULLET2)/lib/libbullet2.a
	rm -rf $(NAN_BULLET2)/include
