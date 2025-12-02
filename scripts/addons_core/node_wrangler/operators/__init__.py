# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from .add_attribute_node import *
from .add_image_sequence import *
from .add_principled_setup import *
from .add_reroutes import *
from .add_texture_setup import *
from .align_selected import *
from .batch_change import *
from .center_selected import *
from .change_factor import *
from .copy_label import *
from .copy_settings import *
from .delete_unused import *
from .detach_outputs import *
from .labels_clear import *
from .labels_modify import *
from .lazy_connect import *
from .lazy_mix import *
from .link_active_to_selected import *
from .link_to_output import *
from .merge_selected import *
from .reload_images import *
from .reset_backdrop import *
from .reset_selected import *
from .save_viewer_image import *
from .select_hierarchy import *
from .swap_links import *


#### ------------------------------ REGISTRATION ------------------------------ ####

classes = (
    NODE_OT_add_attribute_node,
    NODE_OT_add_image_sequence,
    NODE_OT_add_principled_setup,
    NODE_OT_add_reroutes,
    NODE_OT_add_texture_setup,
    NODE_OT_align_selected,
    NODE_OT_batch_change,
    NODE_OT_center_selected,
    NODE_OT_change_factor,
    NODE_OT_copy_label,
    NODE_OT_copy_settings,
    NODE_OT_delete_unused,
    NODE_OT_detach_outputs,
    NODE_OT_labels_clear,
    NODE_OT_labels_modify,
    NODE_OT_lazy_connect,
    NODE_OT_lazy_mix,
    NODE_OT_link_active_to_selected,
    NODE_OT_link_to_output,
    NODE_OT_merge_selected,
    NODE_OT_reload_images,
    NODE_OT_reset_backdrop,
    NODE_OT_reset_selected,
    NODE_OT_save_viewer_image,
    NODE_OT_select_hierarchy,
    NODE_OT_swap_links,

    NODE_OT_lazy_connect_call_inputs_menu,
    NODE_OT_lazy_connect_make_link,

    NODE_MT_lazy_connect_outputs,
    NODE_MT_lazy_connect_inputs,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
