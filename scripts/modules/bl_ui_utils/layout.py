# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import contextlib


@contextlib.contextmanager
def operator_context(layout, op_context):
    """Context manager that temporarily overrides the operator context.

    >>> with operator_context(layout, 'INVOKE_REGION_CHANNELS'):
    ...     layout.operator("anim.channels_delete")
    """

    orig_context = layout.operator_context
    layout.operator_context = op_context
    try:
        yield
    finally:
        layout.operator_context = orig_context
