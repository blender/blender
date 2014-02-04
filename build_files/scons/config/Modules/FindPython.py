import os
import platform

def FindPython():
    all_abi_flags = ['m', 'mu', '']

    python = "/usr"
    abi_flags = "m"  # Most common for linux distros
    version = "3.3"

    _arch = platform.uname()[4] + "-linux-gnu"

    # Determine ABI flags used on this system
    include = os.path.join(python, "include")
    for cur_flags in all_abi_flags:
        inc = os.path.join(include, "python" + version + cur_flags, "Python.h")
        if os.path.exists(inc):
            abi_flags = cur_flags
            break

    # Find config.h. In some distros, such as ubuntu 12.10 they are not in standard include dir.
    incconf = os.path.join(include, _arch, "python" + version + cur_flags)
    if not os.path.exists(os.path.join(incconf, "pyconfig.h")):
        incconf = ''

    # Determine whether python is in /usr/lib or /usr/lib64
    lib32 = os.path.join(python, "lib", "python" + version, "sysconfig.py")
    lib64 = os.path.join(python, "lib64", "python" + version, "sysconfig.py")
    if os.path.exists(lib32):
        libpath = "${BF_PYTHON}/lib"
    elif os.path.exists(lib64):
        libpath = "${BF_PYTHON}/lib64"
    else:
        # roll back to default value
        libpath = "${BF_PYTHON}/lib"

    libpath_arch = libpath
    _libpath_arch = os.path.join(python, "lib", _arch)  # No lib64 stuff with recent deb-like distro afaik...
    _libs = ["libpython" + version + abi_flags + ext for ext in (".so", ".a")]
    for l in _libs:
        if not os.path.exists(os.path.join(libpath, l)) and os.path.exists(os.path.join(_libpath_arch, l)):
            libpath_arch = os.path.join(libpath, _arch)
            break

    return {"PYTHON": python,
            "VERSION": version,
            "LIBPATH": libpath,
            "LIBPATH_ARCH": libpath_arch,
            "ABI_FLAGS": abi_flags,
            "CONFIG": incconf}
