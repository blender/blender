# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import datetime
from collections.abc import Callable

from .environment import TestEnvironment
from .test import Test


def date_str(ts: int) -> str:
    """Format a Unix timestamp as 'YYYY-MM-DD HH:MM:SS' in UTC."""
    return datetime.datetime.fromtimestamp(ts, tz=datetime.timezone.utc).strftime('%Y-%m-%d %H:%M:%S')


def passes_threshold(value: float, success: str, threshold: float) -> bool:
    """Check if a performance value passes the given threshold.

    Returns True when the value is on the good side of the threshold
    based on the success direction ('greater_than' or 'less_than').
    """
    if success == 'greater_than':
        return value > threshold
    elif success == 'less_than':
        return value < threshold
    return False


class BisectProgress:
    """Tracks the current search window during bisecting."""

    def __init__(self) -> None:
        self.min_index = 0
        self.max_index = 0

    @property
    def remaining(self) -> int:
        return self.max_index - self.min_index


class Bisect:
    """
    Bisect over a commit range.

    """

    def __init__(
        self,
        env: TestEnvironment,
        test_commit_cb: Callable[..., tuple[float | None, str]],
        start_ts: int,
        end_ts: int,
    ) -> None:
        self.env = env
        self.test_commit_cb = test_commit_cb
        self.start_ts = start_ts
        self.end_ts = end_ts
        self.commit_status: dict[str, str] = {}
        self.last_good: str | None = None
        self.first_bad: str | None = None

    def run(
        self,
        progress: BisectProgress | None,
    ) -> None:
        self._run_per_day(progress)
        if self.first_bad is not None:
            self._run_single_day(progress)

    def _run_per_day(
        self,
        progress: BisectProgress | None,
    ) -> None:
        """
        Walks day-by-day trying up to three commits per day to quickly locate the good/bad commit.
        """
        SECONDS_PER_DAY = 86400

        day_windows: list[list[tuple[str, int]]] = []
        day_ts = self.start_ts
        while day_ts < self.end_ts:
            next_day_ts = day_ts + SECONDS_PER_DAY
            day_windows.append(self.env.commits_in_window(day_ts, next_day_ts))
            day_ts = next_day_ts

        total_commits = sum(len(w) for w in day_windows)
        self._update_progress(progress, 0, total_commits)

        day_index = 0
        consumed = 0
        last_tested = None
        while day_index < len(day_windows):
            day_commits = day_windows[day_index]
            self._update_progress(progress, consumed, total_commits)
            consumed += len(day_commits)

            attempts = 0
            for commit_hash, commit_ts in day_commits:
                if commit_hash == last_tested:
                    continue
                if attempts >= 3:
                    break
                if commit_hash in self.commit_status:
                    break

                attempts += 1
                _, status = self.test_commit_cb(commit_hash, commit_ts)
                if status in {'build_error', 'no_output', 'run_error', 'skip'}:
                    continue

                if status == 'pass':
                    self.last_good = commit_hash
                    self.commit_status[commit_hash] = 'pass'
                else:
                    self.first_bad = commit_hash
                    self.commit_status[commit_hash] = 'fail'
                last_tested = commit_hash
                break

            if self.first_bad:
                break
            day_index += 1

    def _run_single_day(
        self,
        progress: BisectProgress | None,
    ) -> None:
        """
        Narrows down to the exact commit with a binary search.
        """
        all_commits = self.env.commits_in_window(self.start_ts, self.end_ts)
        commit_index = {commit_hash: index for index, (commit_hash, _) in enumerate(all_commits)}
        max_index = commit_index[self.first_bad]
        min_index = commit_index[self.last_good] + 1 if self.last_good else 0
        self._update_progress(progress, min_index, max_index)

        while min_index < max_index:
            mid_index = (min_index + max_index) // 2
            commit_hash, commit_ts = all_commits[mid_index]

            if commit_hash in self.commit_status:
                if self.commit_status[commit_hash] == 'pass':
                    min_index = mid_index + 1
                else:
                    max_index = mid_index
                self._update_progress(progress, min_index, max_index)
                continue

            _, status = self.test_commit_cb(commit_hash, commit_ts)

            if status == 'pass':
                self.commit_status[commit_hash] = 'pass'
                self.last_good = commit_hash
                min_index = mid_index + 1
            elif status == 'fail':
                self.commit_status[commit_hash] = 'fail'
                self.first_bad = commit_hash
                max_index = mid_index
            else:
                _, new_max = self._forward_scan(mid_index + 1, max_index, all_commits, progress)
                if new_max is None:
                    break
                max_index = new_max
                self._update_progress(progress, min_index, max_index)
                continue

            self._update_progress(progress, min_index, max_index)

    def _forward_scan(
        self,
        start_index: int,
        max_index: int,
        commits: list[tuple[str, int]],
        progress: BisectProgress | None,
    ) -> tuple[int | None, int | None]:
        for scan_index in range(start_index, max_index):
            scan_hash, scan_ts = commits[scan_index]
            if scan_hash in self.commit_status:
                if self.commit_status[scan_hash] == 'fail':
                    self.first_bad = scan_hash
                    self._update_progress(progress, start_index, scan_index)
                    return start_index, scan_index
                continue
            _, status = self.test_commit_cb(scan_hash, scan_ts)
            if status == 'pass':
                self.commit_status[scan_hash] = 'pass'
                self.last_good = scan_hash
                self._update_progress(progress, scan_index + 1, max_index)
                return scan_index + 1, max_index
            elif status == 'fail':
                self.commit_status[scan_hash] = 'fail'
                self.first_bad = scan_hash
                self._update_progress(progress, start_index, scan_index)
                return start_index, scan_index
        return None, None

    @staticmethod
    def run_commit(
        env: TestEnvironment,
        test: Test,
        device_id: str,
        gpu_backend: str,
        count: int,
        attribute: str,
        success: str,
        threshold: float,
        tested: set[str],
        on_progress: Callable[..., None],
        commit_hash: str,
        commit_ts: int,
    ) -> tuple[float | None, str]:
        """Build, benchmark, and evaluate a single commit.

        Builds the given git hash, runs the test ``count`` times, averages the
        results, and checks whether the value passes the threshold.

        Args:
            env: TestEnvironment for git/build operations.
            test: Test object to run.
            device_id: Device identifier string.
            gpu_backend: GPU backend string.
            count: Number of benchmark runs per commit.
            attribute: Name of the performance attribute to extract from output.
            success: Comparison direction ('greater_than' or 'less_than').
            threshold: Pass/fail threshold value.
            tested: Mutable set tracking already-tested commit hashes.
            on_progress: Callable ``(row_values, end)`` for printing table rows.
            commit_hash: Commit hash to test.
            commit_ts: Unix timestamp of the commit.

        Returns:
            Tuple ``(value, status)`` where status is one of
            ``'skip'``, ``'build_error'``, ``'no_output'``, ``'run_error``, ``'pass'`` or ``'fail'``.
            value can be None when status is ``'skip'``, ``'build_error'``, ``'no_output'``, ``'run_error'``.
        """
        # During the weekends it can happen that a day doesn't have any commit. In that case a commit
        # can be selected that has already been performed.
        if commit_hash in tested:
            return None, 'skip'
        tested.add(commit_hash)

        title = env.commit_title(commit_hash)[:70]
        on_progress([commit_hash, date_str(commit_ts), title, '', 'building'], end='\r')

        install_dir = env.install_dir
        ok = env.build(commit_hash, install_dir)
        if not ok:
            on_progress([commit_hash, date_str(commit_ts), title, 'error', 'FAIL (build)'])
            return None, 'build_error'

        env.set_blender_executable(install_dir, {})

        values: list[float] = []
        try:
            for run_idx in range(count):
                run_status = 'running' if count == 1 else f'run [{run_idx + 1}/{count}]'
                on_progress([commit_hash, date_str(commit_ts), title, '', run_status], end='\r')
                output = test.run(env, device_id, gpu_backend)
                if not output or attribute not in output:
                    env.set_default_blender_executable()
                    on_progress([commit_hash, date_str(commit_ts), title, 'error', 'run'])
                    return None, 'no_output'
                values.append(output[attribute])
        except Exception as e:
            env.set_default_blender_executable()
            on_progress([commit_hash, date_str(commit_ts), title, 'error', str(e)[:30]])
            return None, 'run_error'

        env.set_default_blender_executable()
        avg = sum(values) / len(values)

        good = passes_threshold(avg, success, threshold)
        status = 'PASS' if good else 'FAIL'
        on_progress([commit_hash, date_str(commit_ts), title, f'{avg:.4f}', status])
        return avg, 'pass' if good else 'fail'

    @staticmethod
    def _update_progress(
        progress: BisectProgress | None,
        min_index: int,
        max_index: int,
    ) -> None:
        if progress:
            progress.min_index = min_index
            progress.max_index = max_index
