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
import bpy, math
from ....utils       import MetarigError, connected_children_names
from ....utils       import create_widget, copy_bone, create_circle_widget
from ....utils       import strip_org, flip_bone, put_bone
from rna_prop_ui     import rna_idprop_ui_prop_get
from ..super_widgets import create_foot_widget, create_ballsocket_widget
from .limb_utils     import *

def create_leg( cls, bones ):
    org_bones = list(
        [cls.org_bones[0]] + connected_children_names(cls.obj, cls.org_bones[0])
    )

    bones['ik']['ctrl']['terminal'] = []

    bpy.ops.object.mode_set(mode='EDIT')
    eb = cls.obj.data.edit_bones

    # Create toes def bone
    toes_def = get_bone_name( org_bones[-1], 'def' )
    toes_def = copy_bone( cls.obj, org_bones[-1], toes_def )

    eb[ toes_def ].use_connect = False
    eb[ toes_def ].parent      = eb[ bones['def'][-1] ]
    eb[ toes_def ].use_connect = True

    bones['def'] += [ toes_def ]

    # Create IK leg control
    ctrl = get_bone_name( org_bones[2], 'ctrl', 'ik' )
    ctrl = copy_bone( cls.obj, org_bones[2], ctrl )

    # clear parent (so that rigify will parent to root)
    eb[ ctrl ].parent      = None
    eb[ ctrl ].use_connect = False

    # Create heel ctrl bone
    heel = get_bone_name( org_bones[2], 'ctrl', 'heel_ik' )
    heel = copy_bone( cls.obj, org_bones[2], heel )
    orient_bone( cls, eb[ heel ], 'y', 0.5 )
    eb[ heel ].length = eb[ org_bones[2] ].length / 2

    # Reset control position and orientation
    l = eb[ ctrl ].length
    orient_bone( cls, eb[ ctrl ], 'y', reverse = True )
    eb[ ctrl ].length = l

    # Parent
    eb[ heel ].use_connect = False
    eb[ heel ].parent      = eb[ ctrl ]

    eb[ bones['ik']['mch_target'] ].parent      = eb[ heel ]
    eb[ bones['ik']['mch_target'] ].use_connect = False

    # Create foot mch rock and roll bones

    # Get the tmp heel (floating unconnected without children)
    tmp_heel = ""
    for b in cls.obj.data.bones[ org_bones[2] ].children:
        if not b.use_connect and not b.children:
            tmp_heel = b.name

    # roll1 MCH bone
    roll1_mch = get_bone_name( tmp_heel, 'mch', 'roll' )
    roll1_mch = copy_bone( cls.obj, org_bones[2], roll1_mch )

    # clear parent
    eb[ roll1_mch ].use_connect = False
    eb[ roll1_mch ].parent      = None

    flip_bone( cls.obj, roll1_mch )

    # Create 2nd roll mch, and two rock mch bones
    roll2_mch = get_bone_name( tmp_heel, 'mch', 'roll' )
    roll2_mch = copy_bone( cls.obj, org_bones[3], roll2_mch )

    eb[ roll2_mch ].use_connect = False
    eb[ roll2_mch ].parent      = None

    put_bone(
        cls.obj,
        roll2_mch,
        ( eb[ tmp_heel ].head + eb[ tmp_heel ].tail ) / 2
    )

    eb[ roll2_mch ].length /= 4

    # Rock MCH bones
    rock1_mch = get_bone_name( tmp_heel, 'mch', 'rock' )
    rock1_mch = copy_bone( cls.obj, tmp_heel, rock1_mch )

    eb[ rock1_mch ].use_connect = False
    eb[ rock1_mch ].parent      = None

    orient_bone( cls, eb[ rock1_mch ], 'y', 1.0, reverse = True )
    eb[ rock1_mch ].length = eb[ tmp_heel ].length / 2

    rock2_mch = get_bone_name( tmp_heel, 'mch', 'rock' )
    rock2_mch = copy_bone( cls.obj, tmp_heel, rock2_mch )

    eb[ rock2_mch ].use_connect = False
    eb[ rock2_mch ].parent      = None

    orient_bone( cls, eb[ rock2_mch ], 'y', 1.0 )
    eb[ rock2_mch ].length = eb[ tmp_heel ].length / 2

    # Parent rock and roll MCH bones
    eb[ roll1_mch ].parent = eb[ roll2_mch ]
    eb[ roll2_mch ].parent = eb[ rock1_mch ]
    eb[ rock1_mch ].parent = eb[ rock2_mch ]
    eb[ rock2_mch ].parent = eb[ ctrl ]

    # Constrain rock and roll MCH bones
    make_constraint( cls, roll1_mch, {
        'constraint'   : 'COPY_ROTATION',
        'subtarget'    : heel,
        'owner_space'  : 'LOCAL',
        'target_space' : 'LOCAL'
    })
    make_constraint( cls, roll1_mch, {
        'constraint'  : 'LIMIT_ROTATION',
        'use_limit_x' : True,
        'max_x'       : math.radians(360),
        'owner_space' : 'LOCAL'
    })
    make_constraint( cls, roll2_mch, {
        'constraint'   : 'COPY_ROTATION',
        'subtarget'    : heel,
        'use_y'        : False,
        'use_z'        : False,
        'invert_x'     : True,
        'owner_space'  : 'LOCAL',
        'target_space' : 'LOCAL'
    })
    make_constraint( cls, roll2_mch, {
        'constraint'  : 'LIMIT_ROTATION',
        'use_limit_x' : True,
        'max_x'       : math.radians(360),
        'owner_space' : 'LOCAL'
    })

    pb = cls.obj.pose.bones
    for i,b in enumerate([ rock1_mch, rock2_mch ]):
        head_tail = pb[b].head - pb[tmp_heel].head
        if '.L' in b:
            if not i:
                min_y = 0
                max_y = math.radians(360)
            else:
                min_y = math.radians(-360)
                max_y = 0
        else:
            if not i:
                min_y = math.radians(-360)
                max_y = 0
            else:
                min_y = 0
                max_y = math.radians(360)


        make_constraint( cls, b, {
            'constraint'   : 'COPY_ROTATION',
            'subtarget'    : heel,
            'use_x'        : False,
            'use_z'        : False,
            'owner_space'  : 'LOCAL',
            'target_space' : 'LOCAL'
        })
        make_constraint( cls, b, {
            'constraint'  : 'LIMIT_ROTATION',
            'use_limit_y' : True,
            'min_y'       : min_y,
            'max_y'       : max_y,
            'owner_space' : 'LOCAL'
        })

    # Constrain 4th ORG to roll2 MCH bone
    make_constraint( cls, org_bones[3], {
        'constraint'  : 'COPY_TRANSFORMS',
        'subtarget'   : roll2_mch
    })

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
        'subtarget'   : roll1_mch,
        'head_tail'   : 1.0
    })
    make_constraint( cls, bones['ik']['mch_str'], {
        'constraint'  : 'STRETCH_TO',
        'subtarget'   : roll1_mch,
        'head_tail'   : 1.0
    })
    make_constraint( cls, bones['ik']['mch_str'], {
        'constraint'  : 'LIMIT_SCALE',
        'use_min_y'   : True,
        'use_max_y'   : True,
        'max_y'       : 1.05,
        'owner_space' : 'LOCAL'
    })

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

    # Create leg widget
    create_foot_widget(cls.obj, ctrl, bone_transform_name=None)

    # Create heel ctrl locks
    pb[ heel ].lock_location = True, True, True
    pb[ heel ].lock_rotation = False, False, True
    pb[ heel ].lock_scale    = True, True, True

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

        # Constrain toes def bones
        make_constraint( cls, bones['def'][-2], {
            'constraint'  : 'DAMPED_TRACK',
            'subtarget'   : toes
        })
        make_constraint( cls, bones['def'][-2], {
            'constraint'  : 'STRETCH_TO',
            'subtarget'   : toes
        })

        make_constraint( cls, bones['def'][-1], {
            'constraint'  : 'COPY_TRANSFORMS',
            'subtarget'   : toes
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
