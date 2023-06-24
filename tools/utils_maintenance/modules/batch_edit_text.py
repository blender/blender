# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from typing import (
    Callable,
    Generator,
    Optional,
    Sequence,
)

TextOpFn = Callable[
    # file_name, data_src
    [str, str],
    # data_dst or None when no change is made.
    Optional[str]
]


def operation_wrap(fn: str, text_operation: TextOpFn) -> None:
    with open(fn, "r", encoding="utf-8") as f:
        data_src = f.read()
        data_dst = text_operation(fn, data_src)

    if data_dst is None or (data_src == data_dst):
        return

    with open(fn, "w", encoding="utf-8") as f:
        f.write(data_dst)


def run(
        *,
        directories: Sequence[str],
        is_text: Callable[[str], bool],
        text_operation: TextOpFn,
        use_multiprocess: bool,
) -> None:
    print(directories)

    import os

    def source_files(path: str) -> Generator[str, None, None]:
        for dirpath, dirnames, filenames in os.walk(path):
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for filename in filenames:
                if filename.startswith("."):
                    continue
                filepath = os.path.join(dirpath, filename)
                if is_text(filepath):
                    yield filepath

    if use_multiprocess:
        args = [
            (fn, text_operation) for directory in directories
            for fn in source_files(directory)
        ]
        import multiprocessing
        job_total = multiprocessing.cpu_count()
        pool = multiprocessing.Pool(processes=job_total * 2)
        pool.starmap(operation_wrap, args)
    else:
        for directory in directories:
            for fn in source_files(directory):
                operation_wrap(fn, text_operation)
