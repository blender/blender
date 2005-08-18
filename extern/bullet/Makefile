#
# $Id$
#
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
#

include nan_definitions.mk


SOURCEDIR = extern/bullet
LIBNAMES = bullet bulletdyn
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = Bullet BulletDynamics

include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_BULLET) ] || mkdir $(NAN_BULLET)
	@[ -d $(NAN_BULLET)/include ] || mkdir $(NAN_BULLET)/include
	@[ -d $(NAN_BULLET)/include/CollisionShapes ] || mkdir $(NAN_BULLET)/include/CollisionShapes
	@[ -d $(NAN_BULLET)/include/BroadphaseCollision ] || mkdir $(NAN_BULLET)/include/BroadphaseCollision
	@[ -d $(NAN_BULLET)/include/NarrowPhaseCollision ] || mkdir $(NAN_BULLET)/include/NarrowPhaseCollision
	@[ -d $(NAN_BULLET)/include/Dynamics ] || mkdir $(NAN_BULLET)/include/Dynamics
	@[ -d $(NAN_BULLET)/include/ConstraintSolver ] || mkdir $(NAN_BULLET)/include/ConstraintSolver
	@[ -d $(NAN_BULLET)/include/CollisionDispatch ] || mkdir $(NAN_BULLET)/include/CollisionDispatch
	@[ -d $(NAN_BULLET)/lib ] || mkdir $(NAN_BULLET)/lib
	@[ -d $(NAN_BULLET)/lib/debug ] || mkdir $(NAN_BULLET)/lib/debug
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/Bullet/libbullet.a $(NAN_BULLET)/lib/
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/Bullet/debug/libbullet.a $(NAN_BULLET)/lib/debug/
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/BulletDynamics/libbulletdyn.a $(NAN_BULLET)/lib/
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/BulletDynamics/debug/libbulletdyn.a $(NAN_BULLET)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_BULLET)/lib/libbullet.a
	ranlib $(NAN_BULLET)/lib/debug/libbullet.a
	ranlib $(NAN_BULLET)/lib/libbulletdyn.a
	ranlib $(NAN_BULLET)/lib/debug/libbulletdyn.a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh Bullet/CollisionShapes/*.h $(NAN_BULLET)/include/CollisionShapes
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh Bullet/BroadphaseCollision/*.h $(NAN_BULLET)/include/BroadphaseCollision
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh Bullet/NarrowPhaseCollision/*.h $(NAN_BULLET)/include/NarrowPhaseCollision
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh BulletDynamics/Dynamics/*.h $(NAN_BULLET)/include/Dynamics
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh BulletDynamics/ConstraintSolver/*.h $(NAN_BULLET)/include/ConstraintSolver
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh BulletDynamics/CollisionDispatch/*.h $(NAN_BULLET)/include/CollisionDispatch
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh LinearMath/*.h $(NAN_BULLET)/include
