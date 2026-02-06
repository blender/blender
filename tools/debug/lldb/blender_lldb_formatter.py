# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""
LLDB pretty printer for Blender types

Add to ~/.lldbinit:

command script import {path-to-blender-source}/tools/debug/lldb/blender_lldb_formatter.py

If using CodeLLDB extension for VSCode this needs to be added to the user or workspace settings.json:

"lldb.launch.initCommands": [ "command source ${env:HOME}/.lldbinit" ]
"""

import lldb


def __lldb_init_module(debugger, dict):
    debugger.HandleCommand(
        'type summary add -x "^blender::VecBase<" -F blender_lldb_formatter.math_vector_SummaryProvider'
    )
    debugger.HandleCommand(
        'type synthetic add -x "^blender::(float|double)(2|3|4)x(2|3|4)$" -l blender_lldb_formatter.math_matrix_SyntheticProvider'
    )
    debugger.HandleCommand(
        'type synthetic add -x "^blender::Vector<" -l blender_lldb_formatter.bli_vector_SyntheticProvider'
    )


def math_vector_SummaryProvider(valobj, internal_dict):
    debug_values_elems = valobj.GetChildAtIndex(0).GetChildAtIndex(0).GetChildAtIndex(0).GetChildAtIndex(0)
    x = debug_values_elems.GetChildAtIndex(0).GetValue()
    y = debug_values_elems.GetChildAtIndex(1).GetValue()
    z = debug_values_elems.GetChildAtIndex(2).GetValue()
    w = debug_values_elems.GetChildAtIndex(3).GetValue()
    if w is not None:
        return f'{{{x}, {y}, {z}, {w}}}'
    if z is not None:
        return f'{{{x}, {y}, {z}}}'
    if y is not None:
        return f'{{{x}, {y}}}'
    return None


class math_matrix_SyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

        if valobj.GetChildMemberWithName('w') is not None:
            self.len = 4
        elif valobj.GetChildMemberWithName('z') is not None:
            self.len = 3
        elif valobj.GetChildMemberWithName('y') is not None:
            self.len = 2

    def num_children(self):
        return self.len

    def get_child_at_index(self, index):
        matrix_type_name = self.valobj.GetTypeName()
        if "float4" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::float4")
        elif "float3" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::float3")
        elif "float2" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::float2")
        elif "double4" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::double4")
        elif "double3" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::double3")
        elif "double2" in matrix_type_name:
            vec_type = self.valobj.GetTarget().FindFirstType("blender::double2")

        if index == 0:
            return self.valobj.GetChildMemberWithName('x').Cast(vec_type)
        if index == 1:
            return self.valobj.GetChildMemberWithName('y').Cast(vec_type)
        if index == 2:
            return self.valobj.GetChildMemberWithName('z').Cast(vec_type)
        if index == 3:
            return self.valobj.GetChildMemberWithName('w').Cast(vec_type)
        return None


class bli_vector_SyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.begin = self.valobj.GetChildMemberWithName("begin_")
        self.end = self.valobj.GetChildMemberWithName("end_")
        self.capacity_end = self.valobj.GetChildMemberWithName("capacity_end_")
        self.data_type = self.begin.GetType().GetPointeeType()
        self.data_size = self.data_type.GetByteSize()

    def num_children(self):
        try:
            start_val = self.begin.GetValueAsUnsigned(0)
            end_val = self.end.GetValueAsUnsigned(0)
            end_capacity_val = self.capacity_end.GetValueAsUnsigned(0)
            # Make sure nothing is NULL.
            if start_val == 0 or end_val == 0 or end_capacity_val == 0:
                return 0
            # Make sure start is less than finish.
            if start_val >= end_val:
                return 0
            # Make sure finish is less than or equal to end of storage.
            if end_val > end_capacity_val:
                return 0

            size_in_bytes = (end_val - start_val)
            if (size_in_bytes % self.data_size) != 0:
                return 0
            return (size_in_bytes // self.data_size)
        except:
            return 0

    def get_child_at_index(self, index):
        begin = self.valobj.GetChildMemberWithName("begin_")
        data_type = begin.GetType().GetPointeeType()
        data_size = data_type.GetByteSize()
        offset = index * data_size
        return begin.CreateChildAtOffset(f"[{index}]", offset, data_type)
