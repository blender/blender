# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Simple module for inspecting git commits

import os
import subprocess
import datetime
import re

_GIT_COMMIT_COAUTHORS_RE = re.compile(r"^Co-authored-by:[ \t]*([^\n]+)$", re.MULTILINE)


class GitCommit:
    __slots__ = (
        "sha1",
        # to extract more info
        "_git_dir",

        # cached values
        "_author",
        "_date",
        "_body",
        "_files",
        "_files_status",
        "_diff",
    )

    def __init__(self, sha1: bytes, git_dir: str):
        self.sha1 = sha1
        self._git_dir = git_dir

        self._author: str | None = None
        self._date: datetime.datetime | None = None
        self._body: str | None = None
        self._files: list[bytes] | None = None
        self._files_status: list[list[bytes]] | None = None
        self._diff: str | None = None

    def cache(self) -> None:
        """
        Cache all properties
        (except for diff as it's significantly larger than other members).
        """
        self.author
        self.date
        self.body
        self.files
        self.files_status

    def _log_format(
            self,
            format: str,
            *,
            args_prefix: tuple[str | bytes, ...] = (),
            args_suffix: tuple[str | bytes, ...] = (),
    ) -> bytes:
        # sha1 = self.sha1.decode('ascii')
        cmd: tuple[str | bytes, ...] = (
            "git",
            *args_prefix,
            "--git-dir",
            self._git_dir,
            "log",
            "-1",  # only this rev
            self.sha1,
            "--format=" + format,
            *args_suffix,
        )

        # print(" ".join(cmd))

        with subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
        ) as p:
            assert p is not None and p.stdout is not None
            return p.stdout.read()

    @property
    def sha1_short(self) -> str:
        cmd = (
            "git",
            "--git-dir",
            self._git_dir,
            "rev-parse",
            "--short",
            self.sha1,
        )
        with subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
        ) as p:
            assert p is not None and p.stdout is not None
            return p.stdout.read().strip().decode('ascii')

    @property
    def author(self) -> str:
        ret = self._author
        if ret is None:
            content = self._log_format("%an <%ae>")[:-1]
            ret = content.decode("utf8", errors="ignore")
            self._author = ret
        return ret

    @property
    def co_authors(self) -> list[str]:
        authors = []
        for author in _GIT_COMMIT_COAUTHORS_RE.findall(self.body):
            if not ("<" in author and ">" in author):
                # Always follow `Name <>` spec, even when no email is given.
                author = author + " <>"
            authors.append(author)
        return authors

    @property
    def date(self) -> datetime.datetime:
        ret = self._date
        if ret is None:
            import datetime
            ret = datetime.datetime.fromtimestamp(int(self._log_format("%ct")))
            self._date = ret
        return ret

    @property
    def body(self) -> str:
        ret = self._body
        if ret is None:
            content = self._log_format("%B")[:-1]
            ret = content.decode("utf8", errors="ignore")
            self._body = ret
        return ret

    @property
    def subject(self) -> str:
        return self.body.lstrip().partition("\n")[0]

    @property
    def files(self) -> list[bytes]:
        ret = self._files
        if ret is None:
            ret = [
                f for f in self._log_format(
                    "format:",
                    args_prefix=("-c", "diff.renameLimit=10000"),
                    args_suffix=("--name-only",),
                ).split(b"\n") if f
            ]
            self._files = ret
        return ret

    @property
    def files_status(self) -> list[list[bytes]]:
        ret = self._files_status
        if ret is None:
            ret = [
                f.split(None, 1) for f in self._log_format(
                    "format:",
                    args_prefix=("-c", "diff.renameLimit=10000"),
                    args_suffix=("--name-status",),
                ).split(b"\n")
                if f
            ]
            self._files_status = ret
        return ret

    @property
    def diff(self) -> str:
        ret = self._diff
        if ret is None:
            content = self._log_format(
                "",
                args_prefix=("-c", "diff.renameLimit=10000"),
                args_suffix=("-p",),
            )
            ret = content.decode("utf8", errors="ignore")
            self._diff = ret
        return ret


class GitCommitIter:
    __slots__ = (
        "_path",
        "_git_dir",
        "_sha1_range",
        "_process",
    )

    def __init__(self, path: str, sha1_range: str):
        self._path = path
        self._git_dir = os.path.join(path, ".git")
        self._sha1_range = sha1_range
        self._process: subprocess.Popen[bytes] | None = None

    def __iter__(self) -> "GitCommitIter":
        cmd = (
            "git",
            "--git-dir",
            self._git_dir,
            "log",
            self._sha1_range,
            "--format=%H",
        )
        # print(" ".join(cmd))

        self._process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
        )
        return self

    def __next__(self) -> GitCommit:
        assert self._process is not None and self._process.stdout is not None
        sha1 = self._process.stdout.readline()[:-1]
        if sha1:
            return GitCommit(sha1, self._git_dir)
        else:
            raise StopIteration


class GitRepo:
    __slots__ = (
        "_path",
        "_git_dir",
    )

    def __init__(self, path: str):
        self._path = path
        self._git_dir = os.path.join(path, ".git")

    @property
    def branch(self) -> bytes:
        cmd = (
            "git",
            "--git-dir",
            self._git_dir,
            "rev-parse",
            "--abbrev-ref",
            "HEAD",
        )
        # print(" ".join(cmd))

        p = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
        )
        assert p is not None and p.stdout is not None
        return p.stdout.read()
