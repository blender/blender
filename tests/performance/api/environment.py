# Apache License, Version 2.0

import base64
import glob
import inspect
import multiprocessing
import os
import pathlib
import platform
import pickle
import subprocess
import sys
from typing import Callable, Dict, List

from .config import TestConfig
from .device import TestMachine


class TestEnvironment:
    def __init__(self, blender_git_dir: pathlib.Path, base_dir: pathlib.Path):
        self.blender_git_dir = blender_git_dir
        self.base_dir = base_dir
        self.blender_dir = base_dir / 'blender'
        self.build_dir = base_dir / 'build'
        self.lib_dir = base_dir / 'lib'
        self.benchmarks_dir = self.blender_git_dir.parent / 'lib' / 'benchmarks'
        self.git_executable = 'git'
        self.cmake_executable = 'cmake'
        self.cmake_options = ['-DWITH_INTERNATIONAL=OFF', '-DWITH_BUILDINFO=OFF']
        self.unset_blender_executable()
        self.log_file = None
        self.machine = None

    def get_machine(self, need_gpus: bool=True) -> None:
        if not self.machine or (need_gpus and not self.machine.has_gpus):
            self.machine = TestMachine(self, need_gpus)

        return self.machine

    def init(self, build) -> None:
        if not self.benchmarks_dir.exists():
            sys.stderr.write(f'Error: benchmark files directory not found at {self.benchmarks_dir}')
            sys.exit(1)

        # Create benchmarks folder contents.
        print(f'Init {self.base_dir}')
        self.base_dir.mkdir(parents=True, exist_ok=True)

        if len(self.get_configs(names_only=True)) == 0:
            config_dir = self.base_dir / 'default'
            print(f'Creating default configuration in {config_dir}')
            TestConfig.write_default_config(self, config_dir)

        if build:
            if not self.lib_dir.exists():
                print(f'Creating symlink at {self.lib_dir}')
                self.lib_dir.symlink_to(self.blender_git_dir.parent / 'lib')
            else:
                print(f'Exists {self.lib_dir}')

            if not self.blender_dir.exists():
                print(f'Init git worktree in {self.blender_dir}')
                self.call([self.git_executable, 'worktree', 'add', '--detach', self.blender_dir, 'HEAD'], self.blender_git_dir)
            else:
                print(f'Exists {self.blender_dir}')

            if not self.build_dir.exists():
                print(f'Init build in {self.build_dir}')
                self.build_dir.mkdir()
                # No translation to avoid dealing with submodules
                self.call([self.cmake_executable, self.blender_dir, '.'] + self.cmake_options, self.build_dir)
            else:
                print(f'Exists {self.build_dir}')

            print("Building")
            self.build()

        print('Done')

    def checkout(self) -> None:
        # Checkout Blender revision
        if not self.blender_dir.exists():
            sys.stderr.write('\n\nError: no build set up, run `./benchmark init --build` first\n')
            sys.exit(1)

        self.call([self.git_executable, 'clean', '-f', '-d'], self.blender_dir)
        self.call([self.git_executable, 'reset', '--hard', 'HEAD'], self.blender_dir)
        self.call([self.git_executable, 'checkout', '--detach', git_hash], self.blender_dir)

        self.build()

    def build(self) -> None:
        # Build Blender revision
        if not self.build_dir.exists():
            sys.stderr.write('\n\nError: no build set up, run `./benchmark init --build` first\n')
            sys.exit(1)

        jobs = str(multiprocessing.cpu_count())
        self.call([self.cmake_executable, '.'] + self.cmake_options, self.build_dir)
        self.call([self.cmake_executable, '--build', '.', '-j', jobs, '--target', 'install'], self.build_dir)

    def set_blender_executable(self, executable_path: pathlib.Path) -> None:
        # Run all Blender commands with this executable.
        self.blender_executable = executable_path

    def unset_blender_executable(self) -> None:
        if platform.system() == "Windows":
            self.blender_executable = self.build_dir / 'bin' / 'blender.exe'
        elif platform.system() == "Darwin":
            self.blender_executable = self.build_dir / 'bin' / 'Blender.app' / 'Contents' / 'MacOS' / 'Blender'
        else:
            self.blender_executable = self.build_dir / 'bin' / 'blender'

        if not self.blender_executable.exists():
            self.blender_executable = 'blender'

    def set_log_file(self, filepath: pathlib.Path, clear=True) -> None:
        # Log all commands and output to this file.
        self.log_file = filepath

        if clear:
            self.log_file.unlink(missing_ok=True)

    def unset_log_file(self) -> None:
        self.log_file = None

    def call(self, args: List[str], cwd: pathlib.Path, silent=False) -> List[str]:
        # Execute command with arguments in specified directory,
        # and return combined stdout and stderr output.

        # Open log file for writing
        f = None
        if self.log_file:
            if not self.log_file.exists():
                self.log_file.parent.mkdir(parents=True, exist_ok=True)
            f = open(self.log_file, 'a')
            f.write('\n' + ' '.join([str(arg) for arg in args]) + '\n\n')

        proc = subprocess.Popen(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        # Read line by line
        lines = []
        try:
            while proc.poll() is None:
                line = proc.stdout.readline()
                if line:
                    line_str = line.decode('utf-8', 'ignore')
                    lines.append(line_str)
                    if f:
                        f.write(line_str)
        except KeyboardInterrupt:
            # Avoid processes that keep running when interrupting.
            proc.terminate()

        if f:
            f.close()

        # Print command output on error
        if proc.returncode != 0 and not silent:
            for line in lines:
                print(line.rstrip())
            raise Exception("Error executing command")

        return lines

    def call_blender(self, args: List[str], foreground=False) -> List[str]:
        # Execute Blender command with arguments.
        common_args = ['--factory-startup', '--enable-autoexec', '--python-exit-code', '1']
        if foreground:
            common_args += ['--no-window-focus', '--window-geometry', '0', '0', '1024', '768']
        else:
            common_args += ['--background']

        return self.call([self.blender_executable] + common_args + args, cwd=self.base_dir)

    def run_in_blender(self,
                       function: Callable[[Dict], Dict],
                       args: Dict,
                       blender_args: List=[],
                       foreground=False) -> Dict:
        # Run function in a Blender instance. Arguments and return values are
        # passed as a Python object that must be serializable with pickle.

        # Get information to call this function from Blender.
        package_path = pathlib.Path(__file__).parent.parent
        functionname = function.__name__
        modulename = inspect.getmodule(function).__name__

        # Serialize arguments in base64, to avoid having to escape it.
        args = base64.b64encode(pickle.dumps(args))
        output_prefix = 'TEST_OUTPUT: '

        expression = (f'import sys, pickle, base64\n'
                      f'sys.path.append("{package_path}")\n'
                      f'import {modulename}\n'
                      f'args = pickle.loads(base64.b64decode({args}))\n'
                      f'result = {modulename}.{functionname}(args)\n'
                      f'result = base64.b64encode(pickle.dumps(result))\n'
                      f'print("{output_prefix}" + result.decode())\n')

        expr_args = blender_args + ['--python-expr', expression]
        lines = self.call_blender(expr_args, foreground=foreground)

        # Parse output.
        for line in lines:
            if line.startswith(output_prefix):
                output = line[len(output_prefix):].strip()
                result = pickle.loads(base64.b64decode(output))
                return result, lines

        return {}, lines

    def find_blend_files(self, dirpath: pathlib.Path) -> List:
        # Find .blend files in subdirectories of the given directory in the
        # lib/benchmarks directory.
        dirpath = self.benchmarks_dir / dirpath
        filepaths = []
        for filename in glob.iglob(str(dirpath / '*.blend'), recursive=True):
            filepaths.append(pathlib.Path(filename))
        return filepaths

    def get_configs(self, name: str=None, names_only: bool=False) -> List:
        # Get list of configurations in the benchmarks directory.
        configs = []

        if self.base_dir.exists():
            for dirname in os.listdir(self.base_dir):
                if not name or dirname == name:
                    dirpath = self.base_dir / dirname / 'config.py'
                    if dirpath.exists():
                        if names_only:
                            configs.append(dirname)
                        else:
                            configs.append(TestConfig(self, dirname))

        return configs

    def resolve_git_hash(self, revision):
        # Get git hash for a tag or branch.
        return self.call([self.git_executable, 'rev-parse', revision], self.blender_git_dir)[0].strip()

    def git_hash_date(self, git_hash):
        # Get commit data for a git hash.
        return int(self.call([self.git_executable, 'log', '-n1', git_hash, '--format=%at'], self.blender_git_dir)[0].strip())
