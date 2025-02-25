# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

'''
This file can be loaded into GDB to improve the debugging experience.

It has the following features:
* Pretty printers for various types like `blender::Map` and `blender::IndexMask`.
* Frame filters to reduce the complexity of backtraces.

The basic setup is simple. Add the following line to a `~/.gdbinit` file.
Everything in this file is run by GDB when it is started.
```
source ~/blender-git/blender/tools/debug/gdb/blender_gdb_extension.py
```

To validate that things are registered correctly:
1. Start `gdb`.
2. Run `info pretty-printer` and check for `blender-pretty-printers`.
3. Run `info frame-filter` and check for `blender-frame-filters`.
'''

__all__ = (
    # Not used externally but functions as a `main`.
    "register",
)

import gdb
import gdb.printing
from contextlib import contextmanager
from gdb.FrameDecorator import FrameDecorator


@contextmanager
def eval_var(variable_name, value):
    '''
    Creates a context in which the given variable name has the given value.
    '''
    gdb.set_convenience_variable(variable_name, value)
    try:
        yield
    finally:
        gdb.set_convenience_variable(variable_name, None)


@contextmanager
def ensure_unwind_on_signal():
    '''
    Creates a context in which gdb can attempt evaluating functions with
    less risk of crashing the program. If there is an error (e.g. a segfault)
    gdb will unwind the stack, raise a Python exception, but otherwise allows
    continuing the debugging session.
    '''
    was_on = "is on." in gdb.execute("show unwindonsignal", to_string=True)
    if not was_on:
        gdb.execute("set unwindonsignal on")
    try:
        yield
    finally:
        if not was_on:
            gdb.execute("set unwindonsignal off")


class VectorPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        vec_begin = self.value["begin_"]
        vec_end = self.value["end_"]
        vec_capacity_end = self.value["end_"]
        vec_size = vec_end - vec_begin
        vec_capacity = vec_capacity_end - vec_begin
        return f"Size: {vec_size}"

    def children(self):
        begin = self.value["begin_"]
        end = self.value["end_"]
        size = end - begin
        for i in range(size):
            yield str(i), begin[i]

    def display_hint(self):
        return "array"


class SetPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value
        self.key_type = value.type.template_argument(0)

    def to_string(self):
        size = int(
            self.value["occupied_and_removed_slots_"] - self.value["removed_slots_"]
        )
        return f"Size: {size}"

    def children(self):
        slots = self.value["slots_"]["data_"]
        slots_num = int(self.value["slots_"]["size_"])
        for i in range(slots_num):
            slot = slots[i]
            if self.key_type.code == gdb.TYPE_CODE_PTR:
                key = slot["key_"]
                key_int = int(key)
                # The key has two special values for an empty and removed slot.
                is_occupied = key_int < 2**64 - 2
                if is_occupied:
                    yield str(i), key
            else:
                slot_state = int(slot["state_"])
                is_occupied = slot_state == 1
                if is_occupied:
                    key = slot["key_buffer_"].cast(self.key_type)
                    yield str(i), key

    def display_hint(self):
        return "array"


class MapPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value
        self.key_type = value.type.template_argument(0)
        self.value_type = value.type.template_argument(1)

    def to_string(self):
        size = int(
            self.value["occupied_and_removed_slots_"] - self.value["removed_slots_"]
        )
        return f"Size: {size}"

    def children(self):
        slots = self.value["slots_"]["data_"]
        slots_num = int(self.value["slots_"]["size_"])
        for i in range(slots_num):
            slot = slots[i]
            if self.key_type.code == gdb.TYPE_CODE_PTR:
                key = slot["key_"]
                key_int = int(key)
                # The key has two special values for an empty and removed slot.
                is_occupied = key_int < 2**64 - 2
                if is_occupied:
                    value = slot["value_buffer_"].cast(self.value_type)
                    yield "Key", key
                    yield "Value", value
            else:
                slot_state = int(slot["state_"])
                is_occupied = slot_state == 1
                if is_occupied:
                    key = slot["key_buffer_"].cast(self.key_type)
                    value = slot["value_buffer_"].cast(self.value_type)
                    yield "Key", key
                    yield "Value", value

    def display_hint(self):
        return "map"


class MultiValueMapPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value
        self.map_value = value["map_"]

    def to_string(self):
        return MapPrinter(self.map_value).to_string()

    def children(self):
        return MapPrinter(self.map_value).children()

    def display_hint(self):
        return MapPrinter(self.map_value).display_hint()


class TypedBufferPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value
        self.type = value.type.template_argument(0)
        self.size = value.type.template_argument(1)

    def children(self):
        data = self.value.cast(self.type).address
        for i in range(self.size):
            yield str(i), data[i]

    def display_hint(self):
        return "array"


class ArrayPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        size = self.value["size_"]
        return f"Size: {size}"

    def children(self):
        data = self.value["data_"]
        size = self.value["size_"]
        for i in range(size):
            yield str(i), data[i]

    def display_hint(self):
        return "array"


class VectorSetPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def get_size(self):
        return int(
            self.value["occupied_and_removed_slots_"] - self.value["removed_slots_"]
        )

    def to_string(self):
        size = self.get_size()
        return f"Size: {size}"

    def children(self):
        data = self.value["keys_"]
        size = self.get_size()
        for i in range(size):
            yield str(i), data[i]

    def display_hint(self):
        return "array"


class VArrayPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def get_size(self):
        impl = self.value["impl_"]
        size = int(impl["size_"])
        return size

    def to_string(self):
        size = self.get_size()
        with eval_var("varray", self.value.address):
            is_single = gdb.parse_and_eval("$varray->is_single()")
        if is_single and size >= 1:
            with eval_var("varray", self.value.address):
                single_value = gdb.parse_and_eval("$varray->get_internal_single()")
            return f"Size: {size}, Single Value: {single_value}"
        return f"Size: {size}"

    def children(self):
        size = self.get_size()
        impl = self.value["impl_"]
        for i in range(size):
            with eval_var("varray_impl", impl):
                value_at_index = gdb.parse_and_eval(f"$varray_impl->get({i})")
            yield str(i), value_at_index

    def display_hint(self):
        return "array"


class MathVectorPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value
        self.base_type = value.type.template_argument(0)
        self.size = value.type.template_argument(1)

    def to_string(self):
        values = [str(self.get(i)) for i in range(self.size)]
        return "(" + ", ".join(values) + ")"

    def children(self):
        for i in range(self.size):
            yield str(i), self.get(i)

    def get(self, i):
        # Avoid taking pointer of value in case the pointer is not available.
        if 2 <= self.size <= 4:
            if i == 0:
                return self.value["x"]
            if i == 1:
                return self.value["y"]
            if i == 2:
                return self.value["z"]
            if i == 3:
                return self.value["w"]
        return self.value["values"][i]

    def display_hint(self):
        return "array"


class SpanPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        size = self.value["size_"]
        return f"Size: {size}"

    def children(self):
        data = self.value["data_"]
        size = self.value["size_"]
        for i in range(size):
            yield str(i), data[i]

    def display_hint(self):
        return "array"


class StringRefPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        data = self.value["data_"]
        size = int(self.value["size_"])
        if size == 0:
            return ""
        return data.string()

    def display_hint(self):
        return "string"


class IndexRangePrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        start = int(self.value["start_"])
        size = int(self.value["size_"])
        if size == 0:
            return "Size: 0"
        return f"Size: {size}, [{start} - {start + size - 1}]"

    def display_hint(self):
        return None


class IndexMaskPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        size = int(self.value["indices_num_"])
        if size == 0:
            return "Size: 0"

        segments_num = int(self.value["segments_num_"])

        with eval_var("mask", self.value.address):
            first = int(gdb.parse_and_eval("$mask->first()"))
            last = int(gdb.parse_and_eval("$mask->last()"))

        is_range = last - first + 1 == size
        if is_range:
            return f"Size: {size}, [{first} - {last}], Segments: {segments_num}"
        return f"Size: {size}, Segments: {segments_num}"

    def children(self):
        segments_num = int(self.value["segments_num_"])
        prev_cumulative_size = int(self.value["cumulative_segment_sizes_"][0])
        for segment_i in range(segments_num):
            cumulative_size = int(
                self.value["cumulative_segment_sizes_"][segment_i + 1]
            )
            full_segment_size = cumulative_size - prev_cumulative_size
            indices_ptr = self.value["indices_by_segment_"][segment_i]
            offset = self.value["segment_offsets_"][segment_i]
            with eval_var("mask", self.value.address):
                segment = gdb.parse_and_eval(f"$mask->segment({segment_i})")
            yield str(segment_i), segment
            prev_cumulative_size = cumulative_size

    def display_hint(self):
        return "array"


class IndexMaskSegmentPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        size = int(self.value["data_"]["size_"])
        if size == 0:
            return "Size: 0"

        offset = int(self.value["offset_"])
        first = int(self.value["data_"]["data_"][0]) + offset
        last = int(self.value["data_"]["data_"][size - 1]) + offset
        is_range = last - first + 1 == size
        if is_range:
            return f"Size: {size}, [{first} - {last}]"

        return f"Size: {size}"

    def children(self):
        size = int(self.value["data_"]["size_"])
        offset = int(self.value["offset_"])
        for i in range(size):
            element = int(self.value["data_"]["data_"][i])
            index = element + offset
            yield str(i), index

    def display_hint(self):
        return "array"


class OffsetIndicesPrinter:
    def __init__(self, value: gdb.Value):
        self.value = value

    def to_string(self):
        with eval_var("indices", self.value.address):
            size = gdb.parse_and_eval("$indices->size()")
        return f"Size: {size}"

    def children(self):
        with eval_var("indices", self.value.address):
            size = gdb.parse_and_eval("$indices->size()")
        for i in range(size):
            with eval_var("indices", self.value.address):
                element = gdb.parse_and_eval(f"(*$indices)[{i}]")
            yield str(i), element

    def display_hint(self):
        return "array"


class ErrorHandlingPrinter:
    '''
    This class can wrap another printer and adds additional error handling on top.
    '''

    def __init__(self, printer):
        self.printer = printer

    def to_string(self):
        try:
            with ensure_unwind_on_signal():
                return self.printer.to_string()
        except Exception as ex:
            return f"Error: {ex!s}"

    def children(self):
        try:
            children_generator = self.printer.children()
            while True:
                with ensure_unwind_on_signal():
                    next_child = next(children_generator)
                yield next_child
        except:
            return

    def display_hint(self):
        return self.printer.display_hint()


class BlenderPrettyPrinters(gdb.printing.PrettyPrinter):
    def __init__(self):
        super().__init__("blender-pretty-printers")

    def get_pretty_printer(self, value: gdb.Value):
        value_type = value.type
        if value_type is None:
            return None
        if value_type.code == gdb.TYPE_CODE_PTR:
            return None
        type_name = value_type.strip_typedefs().name
        if type_name is None:
            return None
        if type_name.startswith("blender::Vector<"):
            return VectorPrinter(value)
        if type_name.startswith("blender::Set<"):
            return SetPrinter(value)
        if type_name.startswith("blender::Map<"):
            return MapPrinter(value)
        if type_name.startswith("blender::MultiValueMap<"):
            return MultiValueMapPrinter(value)
        if type_name.startswith("blender::TypedBuffer<"):
            return TypedBufferPrinter(value)
        if type_name.startswith("blender::Array<"):
            return ArrayPrinter(value)
        if type_name.startswith("blender::VectorSet<"):
            return VectorSetPrinter(value)
        if type_name.startswith("blender::VArray<"):
            return VArrayPrinter(value)
        if type_name.startswith("blender::VMutableArray<"):
            return VArrayPrinter(value)
        if type_name.startswith("blender::VecBase<"):
            return MathVectorPrinter(value)
        if type_name.startswith("blender::Span<"):
            return SpanPrinter(value)
        if type_name.startswith("blender::MutableSpan<"):
            return SpanPrinter(value)
        if type_name.startswith("blender::VArraySpan<"):
            return SpanPrinter(value)
        if type_name == "blender::StringRef":
            return StringRefPrinter(value)
        if type_name == "blender::StringRefNull":
            return StringRefPrinter(value)
        if type_name == "blender::IndexRange":
            return IndexRangePrinter(value)
        if type_name == "blender::index_mask::IndexMask":
            return IndexMaskPrinter(value)
        if type_name in (
            "blender::OffsetSpan<long, short>",
            "blender::index_mask::IndexMaskSegment",
        ):
            return IndexMaskSegmentPrinter(value)
        if type_name.startswith("blender::offset_indices::OffsetIndices<"):
            return OffsetIndicesPrinter(value)
        return None

    def __call__(self, value: gdb.Value):
        if printer := self.get_pretty_printer(value):
            return ErrorHandlingPrinter(printer)
        return None


class ForeachIndexFilter:
    filename_pattern = r".*index_mask.*"

    @staticmethod
    def frame_to_name(frame):
        function_name = frame.function()
        if function_name.startswith("blender::index_mask::IndexMask::foreach_index"):
            return "Foreach Index"


class LazyFunctionEvalFilter:
    filename_pattern = r".*functions.*lazy_function.*"

    @staticmethod
    def frame_to_name(frame):
        function_name = frame.function()
        if function_name.startswith((
                "blender::fn::lazy_function::LazyFunction::execute",
                "blender::fn::lazy_function::Executor::push_to_task_pool",
        )):
            return "Execute Lazy Function"


class ThreadingFilter:
    filename_pattern = (
        r".*(include/tbb|libtbb|task_pool.cc|BLI_task.hh|task_range.cc).*"
    )

    @staticmethod
    def frame_to_name(frame):
        function_name = frame.function()
        if function_name.startswith("BLI_task_pool_work_and_wait"):
            return "Task Pool work and wait"
        if function_name.startswith(
            "tbb::internal::rml::private_worker::thread_routine"
        ):
            return "TBB Worker Thread"
        if function_name.startswith("blender::threading::parallel_for"):
            return "Parallel For"
        if function_name.startswith("blender::threading::isolate_task"):
            return "Isolate Task"


class StdFilter:
    filename_pattern = r".*/include/c\+\+/13.*"

    @staticmethod
    def frame_to_name(frame):
        function_name = frame.function()
        if function_name.startswith("std::function") and "operator()" in function_name:
            return "Call std::function"


class FunctionRefFilter:
    filename_pattern = r".*BLI_function_ref.hh"

    @staticmethod
    def frame_to_name(frame):
        function_name = frame.function()
        if "operator()" in function_name:
            return "Call FunctionRef"


frame_filters = [
    ForeachIndexFilter(),
    LazyFunctionEvalFilter(),
    ThreadingFilter(),
    StdFilter(),
    FunctionRefFilter(),
]


class FrameFilter:
    def __init__(self):
        self.name = "blender-frame-filters"
        self.priority = 100
        self.enabled = True

    def filter(self, frame_iter):
        import re

        current_filter = None
        current_frames = []

        def handle_gathered_frames():
            nonlocal current_frames
            nonlocal current_filter

            if current_frames:
                top_frame = current_frames[-1]
                bottom_frame = current_frames[0]
                name = current_filter.frame_to_name(top_frame)
                if name is None:
                    yield from current_frames
                else:
                    yield SimpleFrameDecorator(name, bottom_frame, current_frames[1:])

            current_frames = []
            current_filter = None

        for frame in frame_iter:
            file_name = frame.filename()
            if file_name is None:
                yield frame
                continue
            if current_filter and re.match(
                current_filter.filename_pattern, file_name
            ):
                current_frames.append(frame)
                continue

            yield from handle_gathered_frames()

            for f in frame_filters:
                if re.match(f.filename_pattern, file_name):
                    current_filter = f
                    current_frames = [frame]
                    break
            else:
                yield frame

        yield from handle_gathered_frames()


class SimpleFrameDecorator(FrameDecorator):
    def __init__(self, name, frame, elided_frames):
        super().__init__(frame)
        self.name = name
        self.frame = frame
        self.elided_frames = elided_frames

    def elided(self):
        return iter(self.elided_frames)

    def function(self):
        return self.name

    def frame_args(self):
        return None

    def address(self):
        return None

    def line(self):
        return None

    def filename(self):
        return None

    def frame_locals(self):
        return None


def register():
    gdb.printing.register_pretty_printer(None, BlenderPrettyPrinters(), replace=True)

    frame_filter = FrameFilter()
    gdb.frame_filters[frame_filter.name] = frame_filter


if __name__ == "__main__":
    register()
