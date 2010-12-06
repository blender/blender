# $Id$
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SET(INC 
	.
	../avi
	../blenlib
	../blenloader
	../freestyle
	../gpu
	../ikplugin
	../imbuf
	../makesdna
	../makesrna
	../modifiers
	../nodes
	../editors/include
	../render/extern/include
	../../../extern/glew/include
	../../../intern/audaspace/intern
	../../../intern/bsp/extern ../blenfont
	../../../intern/decimation/extern
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/iksolver/extern
	../../../intern/memutil
	../../../intern/opennl/extern
	../../../intern/smoke/extern
    ../../../source/blender/windowmanager # XXX - BAD LEVEL CALL WM_api.h
	${ZLIB_INC}
)

SET(SRC
	intern/BME_Customdata.c
	intern/BME_conversions.c
	intern/BME_eulers.c
	intern/BME_mesh.c
	intern/BME_structure.c
	intern/BME_tools.c
	intern/CCGSubSurf.c
	intern/DerivedMesh.c
	intern/action.c
	intern/anim.c
	intern/anim_sys.c
	intern/armature.c
	intern/blender.c
	intern/bmfont.c
	intern/boids.c
	intern/booleanops_mesh.c
	intern/brush.c
	intern/bullet.c
	intern/bvhutils.c
	intern/cdderivedmesh.c
	intern/cloth.c
	intern/collision.c
	intern/colortools.c
	intern/constraint.c
	intern/context.c
	intern/curve.c
	intern/customdata.c
	intern/customdata_file.c
	intern/deform.c
	intern/depsgraph.c
	intern/displist.c
	intern/effect.c
	intern/exotic.c
	intern/fcurve.c
	intern/fluidsim.c
	intern/fmodifier.c
	intern/font.c
	intern/gpencil.c
	intern/group.c
	intern/icons.c
	intern/idcode.c
	intern/idprop.c
	intern/image.c
	intern/image_gen.c
	intern/implicit.c
	intern/ipo.c
	intern/key.c
	intern/lattice.c
	intern/library.c
	intern/linestyle.c
	intern/material.c
	intern/mball.c
	intern/mesh.c
	intern/modifier.c
	intern/multires.c
	intern/nla.c
	intern/node.c
	intern/object.c
	intern/packedFile.c
	intern/paint.c
	intern/particle.c
	intern/particle_system.c
	intern/pointcache.c
	intern/property.c
	intern/report.c
	intern/sca.c
	intern/scene.c
	intern/screen.c
	intern/script.c
	intern/seqcache.c
	intern/seqeffects.c
	intern/sequencer.c
	intern/shrinkwrap.c
	intern/sketch.c
	intern/smoke.c
	intern/softbody.c
	intern/sound.c
	intern/subsurf_ccg.c
	intern/suggestions.c
	intern/text.c
	intern/texture.c
	intern/unit.c
	intern/world.c
	intern/writeavi.c
	intern/writeffmpeg.c
	intern/writeframeserver.c

	BKE_DerivedMesh.h
	BKE_action.h
	BKE_anim.h
	BKE_animsys.h
	BKE_armature.h
	BKE_blender.h
	BKE_bmesh.h
	BKE_bmeshCustomData.h
	BKE_bmfont.h
	BKE_bmfont_types.h
	BKE_boids.h
	BKE_booleanops_mesh.h
	BKE_brush.h
	BKE_bullet.h
	BKE_bvhutils.h
	BKE_cdderivedmesh.h
	BKE_cloth.h
	BKE_collision.h
	BKE_colortools.h
	BKE_constraint.h
	BKE_context.h
	BKE_curve.h
	BKE_customdata.h
	BKE_customdata_file.h
	BKE_deform.h
	BKE_depsgraph.h
	BKE_displist.h
	BKE_effect.h
	BKE_endian.h
	BKE_exotic.h
	BKE_fcurve.h
	BKE_fluidsim.h
	BKE_font.h
	BKE_global.h
	BKE_gpencil.h
	BKE_group.h
	BKE_icons.h
	BKE_idcode.h
	BKE_idprop.h
	BKE_image.h
	BKE_ipo.h
	BKE_key.h
	BKE_lattice.h
	BKE_library.h
	BKE_main.h
	BKE_material.h
	BKE_mball.h
	BKE_mesh.h
	BKE_modifier.h
	BKE_multires.h
	BKE_nla.h
	BKE_node.h
	BKE_object.h
	BKE_packedFile.h
	BKE_paint.h
	BKE_particle.h
	BKE_plugin_types.h
	BKE_pointcache.h
	BKE_property.h
	BKE_report.h
	BKE_sca.h
	BKE_scene.h
	BKE_screen.h
	BKE_script.h
	BKE_sequencer.h
	BKE_shrinkwrap.h
	BKE_sketch.h
	BKE_smoke.h
	BKE_softbody.h
	BKE_sound.h
	BKE_subsurf.h
	BKE_suggestions.h
	BKE_text.h
	BKE_texture.h
	BKE_unit.h
	BKE_utildefines.h
	BKE_world.h
	BKE_writeavi.h
	BKE_writeffmpeg.h
	BKE_writeframeserver.h
	depsgraph_private.h
	intern/CCGSubSurf.h
	intern/bmesh_private.h
	nla_private.h
)

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_BULLET)
	LIST(APPEND INC ../../../extern/bullet2/src)
	ADD_DEFINITIONS(-DUSE_BULLET)
ENDIF(WITH_BULLET)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_IMAGE_TIFF)
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_IMAGE_TIFF)

IF(WITH_IMAGE_OPENJPEG)
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_IMAGE_OPENJPEG)

IF(WITH_IMAGE_DDS)
	ADD_DEFINITIONS(-DWITH_DDS)
ENDIF(WITH_IMAGE_DDS)

IF(WITH_IMAGE_CINEON)
	ADD_DEFINITIONS(-DWITH_CINEON)
ENDIF(WITH_IMAGE_CINEON)

IF(WITH_IMAGE_HDR)
	ADD_DEFINITIONS(-DWITH_HDR)
ENDIF(WITH_IMAGE_HDR)

IF(WITH_CODEC_QUICKTIME)
	LIST(APPEND INC ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_CODEC_QUICKTIME)

IF(WITH_CODEC_FFMPEG)
	LIST(APPEND INC ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_CODEC_FFMPEG)

IF(WITH_LCMS)
	LIST(APPEND INC ${LCMS_INCLUDE_DIR})
	ADD_DEFINITIONS(-DWITH_LCMS)
ENDIF(WITH_LCMS)

IF(WITH_PYTHON)
	LIST(APPEND INC ../python ${PYTHON_INC})
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(NOT WITH_MOD_FLUID)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_MOD_FLUID)

IF(WITH_JACK)
	ADD_DEFINITIONS(-DWITH_JACK)
ENDIF(WITH_JACK)

IF(WITH_LZO)
	LIST(APPEND INC ../../../extern/lzo/minilzo)
	ADD_DEFINITIONS(-DWITH_LZO)
ENDIF(WITH_LZO)

IF(WITH_LZMA)
	LIST(APPEND INC ../../../extern/lzma)
	ADD_DEFINITIONS(-DWITH_LZMA)
ENDIF(WITH_LZMA)

IF(MSVC)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
ENDIF(MSVC)

BLENDERLIB(bf_blenkernel "${SRC}" "${INC}")
