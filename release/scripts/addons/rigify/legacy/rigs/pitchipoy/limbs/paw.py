#====================== BEGIN GPL LICENSE BLOCK ======================
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
#======================= END GPL LICENSE BLOCK ========================

# <pep8 compliant>
import bpy
from ....utils       import MetarigError, connected_children_names
from ....utils       import create_widget, copy_bone, create_circle_widget
from ....utils       import strip_org, flip_bone
from rna_prop_ui     import rna_idprop_ui_prop_get
from ..super_widgets import create_foot_widget, create_ballsocket_widget
from .limb_utils     import *

def create_paw( cls, bones ):
    org_bones = list(
        [cls.org_bones[0]] + connected_children_names(cls.obj, cls.org_bones[0])
    )


    bones['ik']['ctrl']['terminal'] = []

    bpy.ops.object.mode_set(mode='EDIT')
    eb = cls.obj.data.edit_bones

    # Create IK paw control
    ctrl = get_bone_name( org_bones[2], 'ctrl', 'ik' )
    ctrl = copy_bone( cls.obj, org_bones[2], ctrl )

    # clear parent (so that rigify will parent to root)
    eb[ ctrl ].parent      = None
    eb[ ctrl ].use_connect = False

    # Create heel control bone
    heel = get_bone_name( org_bones[2], 'ctrl', 'heel_ik' )
    heel = copy_bone( cls.obj, org_bones[2], heel )

    # clear parent
    eb[ heel ].parent      = None
    eb[ heel ].use_connect = False

    # Parent
    eb[ heel ].parent      = eb[ ctrl ]
    eb[ heel ].use_connect = False

    flip_bone( cls.obj, heel )

    eb[ bones['ik']['mch_target'] ].parent      = eb[ heel ]
    eb[ bones['ik']['mch_target'] ].use_connect = False

    # Reset control position and orientation
    l = eb[ ctrl ].length
    orient_bone( cls, eb[ ctrl ], 'y', reverse = True )
    eb[ ctrl ].length = l

    # Set up constraints
    # Constrain mch target bone to the ik control and mch stretch

    make_constraint( cls, bones['ik']['mch_target'], {
        'constraint'  : 'COPY_LOCATION',
        'subtarget'   : bones['ik']['mch_str'],
        'head_tail'   : 1.0
    })

    # Constrain mch ik stretch bone to the ik control
    make_constraint( cls, bones['ik']['mch_str'], {
        'constraint'  : 'DAMPED_TRACK',
        'subtarget'   : heel,
        'head_tail'   : 1.0
    })
    make_constraint( cls, bones['ik']['mch_str'], {
        'constraint'  : 'STRETCH_TO',
        'subtarget'   : heel,
        'head_tail'   : 1.0
    })
    make_constraint( cls, bones['ik']['mch_str'], {
        'constraint'  : 'LIMIT_SCALE',
        'use_min_y'   : True,
        'use_max_y'   : True,
        'max_y'       : 1.05,
        'owner_space' : 'LOCAL'
    })

    pb = cls.obj.pose.bones

    # Modify rotation mode for ik and tweak controls
    pb[bones['ik']['ctrl']['limb']].rotation_mode = 'ZXY'

    for b in bones['tweak']['ctrl']:
        pb[b].rotation_mode = 'ZXY'

    # Create ik/fk switch property
    pb_parent = pb[ bones['parent'] ]

    pb_parent['IK_Strertch'] = 1.0
    prop = rna_idprop_ui_prop_get( pb_parent, 'IK_Strertch', create=True )
    prop["min"]         = 0.0
    prop["max"]         = 1.0
    prop["soft_min"]    = 0.0
    prop["soft_max"]    = 1.0
    prop["description"] = 'IK Stretch'

    # Add driver to limit scale constraint influence
    b        = bones['ik']['mch_str']
    drv      = pb[b].constraints[-1].driver_add("influence").driver
    drv.type = 'AVERAGE'

    var = drv.variables.new()
    var.name = prop.name
    var.type = "SINGLE_PROP"
    var.targets[0].id = cls.obj
    var.targets[0].data_path = \
        pb_parent.path_from_id() + '['+ '"' + prop.name + '"' + ']'

    drv_modifier = cls.obj.animation_data.drivers[-1].modifiers[0]

    drv_modifier.mode            = 'POLYNOMIAL'
    drv_modifier.poly_order      = 1
    drv_modifier.coefficients[0] = 1.0
    drv_modifier.coefficients[1] = -1.0

    # Create paw widget
    create_foot_widget(cls.obj, ctrl, bone_transform_name=None)

    # Create heel ctrl locks
    pb[ heel ].lock_location = True, True, True

    # Add ballsocket widget to heel
    create_ballsocket_widget(cls.obj, heel, bone_transform_name=None)

    bpy.ops.object.mode_set(mode='EDIT')
    eb = cls.obj.data.edit_bones

    if len( org_bones ) >= 4:
        # Create toes control bone
        toes = get_bone_name( org_bones[3], 'ctrl' )
        toes = copy_bone( cls.obj, org_bones[3], toes )

        eb[ toes ].use_connect = False
        eb[ toes ].parent      = eb[ org_bones[3] ]

        # Create toes mch bone
        toes_mch = get_bone_name( org_bones[3], 'mch' )
        toes_mch = copy_bone( cls.obj, org_bones[3], toes_mch )

        eb[ toes_mch ].use_connect = False
        eb[ toes_mch ].parent      = eb[ ctrl ]

        eb[ toes_mch ].length /= 4

        # Constrain 4th ORG to toes MCH bone
        make_constraint( cls, org_bones[3], {
            'constraint'  : 'COPY_TRANSFORMS',
            'subtarget'   : toes_mch
        })

        # Constrain toes def bones

        make_constraint( cls, bones['def'][-1], {
            'constraint'  : 'DAMPED_TRACK',
            'subtarget'   : toes,
            'head_tail'   : 1
        })
        make_constraint( cls, bones['def'][-1], {
            'constraint'  : 'STRETCH_TO',
            'subtarget'   : toes,
            'head_tail'   : 1
        })


        # Find IK/FK switch property
        pb   = cls.obj.pose.bones
        prop = rna_idprop_ui_prop_get( pb[ bones['parent'] ], 'IK/FK' )

        # Add driver to limit scale constraint influence
        b        = org_bones[3]
        drv      = pb[b].constraints[-1].driver_add("influence").driver
        drv.type = 'AVERAGE'

        var = drv.variables.new()
        var.name = prop.name
        var.type = "SINGLE_PROP"
        var.targets[0].id = cls.obj
        var.targets[0].data_path = \
            pb_parent.path_from_id() + '['+ '"' + prop.name + '"' + ']'

        drv_modifier = cls.obj.animation_data.drivers[-1].modifiers[0]

        drv_modifier.mode            = 'POLYNOMIAL'
        drv_modifier.poly_order      = 1
        drv_modifier.coefficients[0] = 1.0
        drv_modifier.coefficients[1] = -1.0

        # Create toe circle widget
        create_circle_widget(cls.obj, toes, radius=0.4, head_tail=0.5)

        bones['ik']['ctrl']['terminal'] += [ toes ]

    bones['ik']['ctrl']['terminal'] += [ heel, ctrl ]

    return bones
