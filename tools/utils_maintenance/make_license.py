# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
# pylint: disable=missing-function-docstring, missing-module-docstring, missing-class-docstring

__all__ = (
    "main",
)

import datetime
import itertools
import json
import os
import re
import sys

from pathlib import Path

from typing import (
    NamedTuple,
)
from collections.abc import (
    Iterator,
)

# -----------------------------------------------------------------------------
# Path Constants

ROOT_DIR = Path(__file__).parent.parent.parent

DIRPATH_LICENSES: Path = ROOT_DIR / "release/license/"
DIRPATH_EXTERN_LIBRARIES: Path = ROOT_DIR / "extern"

FILEPATH_VERSIONS_CMAKE: Path = ROOT_DIR / "build_files/build_environment/cmake/versions.cmake"
FILEPATH_LICENSES_INDEX: Path = DIRPATH_LICENSES / "licenses.json"  # List of licenses and definitions.
FILEPATH_LICENSE_GENERATED: Path = DIRPATH_LICENSES / "license.md"  # Generated licenses file.


# -----------------------------------------------------------------------------
# Constants

INTRODUCTION = r"""<!--

This document is auto-generated with `make license`.
To update it, edit (paths relative to Blender projects root):

 * Introduction and formatting: ./tools/utils_maintenance/make_license.py
 * External libraries: ./build_files/build_environment/cmake/versions.cmake
 * Internal libraries: ./extern/*/Blender.README
 * Fonts: ./tools/utils_maintenance/make_license.py
 * New licenses: ./release/license/licenses.json

Then run `make license` and commit `license.md`.

-->
# Blender Third-Party Licenses

While Blender itself is released under [GPU-GPL 3.0 or later](https://spdx.org/licenses/GPL-3.0-or-later.html)
`© 2011-<THIS-YEAR> Blender Foundation`,
it contains dependencies which have different licenses.

<SPDX:GPL-3.0-or-later>

""".replace("<THIS-YEAR>", str(datetime.date.today().year))

INTRODUCTION += r"""
## Fonts

Blender distributes a number of font files to support many different language and uses.
They work together as a stack.

| Font     | License | Copyright |
| -------  | --------- | ------- |
| [Inter](https://rsms.me/inter/) | <SPDX:OFL-1.1|link> | `Copyright 2020 The Inter Project Authors (https://github.com/rsms/inter)` |
| [Noto Fonts](https://fonts.google.com/noto) | <SPDX:OFL-1.1|link> | `Copyright 2018 The Noto Project Authors (github.com/googlei18n/noto-fonts)`|
| [Last Resort](https://github.com/unicode-org/last-resort-font) | <SPDX:OFL-1.1|link> | `Copyright © 1998-2024 Unicode, Inc. Unicode and the Unicode Logo are registered trademarks of Unicode, Inc. in the United States and other countries.` |
| [DejaVu Sans Mono](https://github.com/dejavu-fonts/dejavu-fonts) | <Arev-Fonts|link> + <SPDX:Bitstream-Vera|link> | `2003 Bitstream, Inc. (Bitstream font glyphs). 2006 Tavmjong Bah (Arev font glyphs). DejaVu changes are in public domain` |

<Arev-Fonts>
<SPDX:Bitstream-Vera>
<SPDX:OFL-1.1>
"""


NO_LICENSE = "No License Set"

LICENSES_NOT_NEEDED = {
    "",
}

# -----------------------------------------------------------------------------
# Types


# Raw data extracted either from:
# - `README.blender` files.
# - `./build_files/build_environment/cmake/versions.cmake`.
class LibraryRaw(NamedTuple):
    name: str
    homepage: str
    version: str
    license: str
    exception: str
    copyright: str


def int_to_superscript(num: int) -> str:
    # Mapping of regular digits to superscript Unicode characters.
    superscript_map = {
        "0": "⁰",
        "1": "¹",
        "2": "²",
        "3": "³",
        "4": "⁴",
        "5": "⁵",
        "6": "⁶",
        "7": "⁷",
        "8": "⁸",
        "9": "⁹"
    }
    # Convert the integer to a string and map each digit to its superscript equivalent.
    return "".join(superscript_map[digit] for digit in str(num))


class Library:
    __slots__ = ("name", "version", "homepage", "library_copyright", "exception")

    name: str
    version: str
    homepage: str
    library_copyright: str
    exception: str

    def __init__(
        self,
        *,
        name: str,
        version: str,
        homepage: str,
        library_copyright: str,
        exception: str,
    ):
        # pylint: disable=too-many-arguments
        self.name = name
        self.version = version
        self.homepage = homepage
        self.library_copyright = library_copyright
        self.exception = exception

    def __lt__(self, other: "Library") -> bool:
        return self.name.lower() < other.name.lower()

    def check_missing_copyright(self, library_license: "License") -> None:
        """Some licenses require a copyright notice"""
        if self.library_copyright or library_license.copyright_exemption:
            return

        print(f"Warning: \"{self.name}\" missing copyright notice "
              f"(required for {library_license.identifier}).")

    def dump(self, library_license: "License") -> str:
        self.check_missing_copyright(library_license)

        library_copyright = f"`{self.library_copyright}`" if self.library_copyright else "-"
        name = f"[{self.name}]({self.homepage})" if self.homepage else self.name
        version = self.version[:11] if self.version else "-"

        # Add exception indicator in the name.
        name += library_license.get_exception_suffix(self.exception)
        raw_data = (
            f"| {name} "
            f"| {version} "
        )

        if not library_license.copyright_exemption:
            raw_data += f"| {library_copyright} "

        raw_data += "|\n"
        return raw_data


class License:
    __slots__ = ("identifier", "name", "url", "copyright_exemption", "libraries", "exceptions")

    identifier: str
    name: str
    url: str
    copyright_exemption: str
    libraries: list[Library]
    exceptions: list[str]

    def __init__(
            self,
            *,
            identifier: str,
            name: str,
            url: str,
            copyright_exemption: str = "",
    ):
        self.identifier = identifier
        self.name = name
        self.url = url
        # By default we assume that all the licenses require copyright.
        self.copyright_exemption = copyright_exemption
        self.libraries = []
        self.exceptions = []

    @property
    def filepath(self) -> str:
        if self.identifier.startswith("SPDX"):
            filepath = os.path.join(DIRPATH_LICENSES, "spdx", f"{self.identifier[5:]}.txt")
        else:
            filepath = os.path.join(DIRPATH_LICENSES, "others", f"{self.identifier}.txt")
        return filepath

    def get_exception_suffix(self, exception: str) -> str:
        """Return (optional) exception indicator e.g., ¹ """
        if not exception:
            return ""

        if exception not in self.exceptions:
            self.exceptions.append(exception)

        _id = self.exceptions.index(exception)
        return int_to_superscript(_id + 1)

    def dump(self, index: int = 0) -> str:
        """Read the complete license file from disk and return as string
        """
        # Make sure we only throw the error if we actually need the file.
        # If there are no libraries using this license, there is no need to complain.
        # The json could even have all the licenses from SPDX and only include the ones
        # Blender needs.
        if not os.path.exists(self.filepath):
            if self.copyright_exemption:
                return ""

            print(f"Error: Could not find license file for {self.identifier}: \"{self.filepath}\"")
            sys.exit(1)

        with open(self.filepath, "r", encoding="utf8") as fh:
            license_raw = fh.read()

        # Strip trailing space as this has a special meaning for mark-down,
        # avoid editing the original texts as any edits may be overwritten
        # when updating the licenses.
        #
        # This also removes page breaks "\x0C" or ^L.
        # These could be replaced with sometime similar in markdown,
        # unless this has some benefit, leave as-is.
        license_raw = "\n".join(line.rstrip() for line in license_raw.split("\n"))

        # Debug option commented out.
        # This is useful if you want a document for human inspection without the licenses.
        # license_raw = "# Debug"

        summary_prefix = f"{int_to_superscript(index)} " if index else ""
        library_license = (
            f"<details>\n<summary>{summary_prefix}{self.name}</summary>\n"
            f"\n{license_raw}\n"
            "</details>"
        )

        return library_license

    def __lt__(self, other: "License") -> bool:
        return self.name.lower() < other.name.lower()

    def __repr__(self) -> str:
        as_dict = {
            self.identifier: {
                "name": self.name,
                "url": self.url,
                "filepath": self.filepath,
                "libraries": len(self.libraries),
            }
        }
        return json.dumps(as_dict, indent=2)


# -----------------------------------------------------------------------------
# Internal Logic

def initialize_licenses() -> dict[str, License]:
    with open(FILEPATH_LICENSES_INDEX, "r", encoding="utf8") as fh:
        licenses_json = json.load(fh)

    licenses = {key: License(identifier=key, **values) for key, values in licenses_json.items()}
    return licenses


def get_license_exception(library_license: str) -> tuple[str, str]:
    """Split license into main license and exception

        Example of acceptable license: "SPDX:Apache-2.0 WITH LLVM-exception"
        This would output: ("SPDX:Apache-2.0", "LLVM-exception")
    """
    # Use `re.IGNORECASE` to match "with" in any case (e.g., "with" or "WITH").
    re_match = re.match(r"^(.*)\swith\s(.+)$", library_license, re.IGNORECASE)
    if re_match:
        return re_match.group(1).strip(), re_match.group(2).strip()
    return library_license, ""


def flatten_cmake_file(content: str) -> str:
    """Resolve all the ${VARIABLES} in CMake"""
    # Find all variable definitions of the form `set(VAR_NAME VALUE)`.
    variables = dict(re.findall(r"set\((\w+)\s+([^\)]+)\)", content))

    # Replace all occurrences of ${VAR_NAME} with the corresponding value.
    for var, value in variables.items():
        content = re.sub(rf"\$\{{{var}\}}", value, content)
    return content


def process_versions_cmake() -> Iterator[LibraryRaw]:
    """
    Parse versions.cmake

        Return a dictionary grouped by license.
    """
    # pylint: disable=too-many-locals

    # NOTE(@ideasman42): basic & imperfect variable extractions.
    # It can be fairly easily tripped up by expressions such as:
    # - `set(VAR "VALUE ) # ")`
    # - Uppercase or additional spaces e.g. `SET (...)`.
    # - Or `set()` expressions inside a multi-line string.
    #
    # Any effort to rewrite this logic would be better spent running the file through CMake it's self,
    # appending logic to dump all variables using `string(JSON ...)` which Python can then read reliably.

    libraries_raw: dict[str, dict[str, str]] = {}
    with open(FILEPATH_VERSIONS_CMAKE, "r", encoding="utf8") as fh:
        data = fh.read()

    data = flatten_cmake_file(data)
    for re_match in re.finditer(r"^set\((\w+)\s+", data, re.MULTILINE):
        # Use regex to capture the key from each set() statement.
        # Extract the value from the remainder.
        key = re_match.group(1)
        value_start = re_match.end()
        value_eol = data.find("\n", value_start)
        assert value_eol != -1
        value_line = data[value_start: value_eol].rstrip()

        # Strip any comments at the line end.
        # `set(FOO BAR) # BAZ`.
        if (re_match_comment := next(re.finditer(r"\)\s*#", value_line), None)):
            value_line = value_line[:re_match_comment.start() + 1]

        # Extract the value by checking this line and detecting single or multi-line text.
        if value_line.endswith(")"):
            # Single line variable.
            value = value_line[:-1].strip()
        elif value_line.endswith("[=["):
            # Calculate the bounds of the multi-line string.
            value_ml_start = value_start + len(value_line)
            value_ml_end = data.find("]=]", value_ml_start)
            assert value_ml_end != -1
            value = data[value_ml_start:value_ml_end].strip().replace("\n", " ")
        else:
            # Could not detect a single line value OR a multi-line value.
            print(f"Error: Unable to parse {key!r}, line {value_line!r}, "
                  "expected an \")\" ending or beginning of a multi-line string \"[=[\"")
            sys.exit(1)

        # Determine the library name from the prefix (minus the suffix).
        library_name, end_word = key.rpartition("_")[0::2]
        if not library_name:
            # No suffix to check, it can be skipped.
            continue

        # Initialize the library entry if it doesn't exist.
        if (library_vars := libraries_raw.get(library_name)) is None:
            library_vars = libraries_raw[library_name] = {
                "name": library_name.replace("_", " ").title(),
                "homepage": "",
                "version": "",
                "license": "",
                "exception": "",
                "copyright": "",
                # Exclude from `LibraryRaw`.
                "_hash": "",
                "_build_time_only": "",
            }

        # Populate the relevant fields based on the key.
        match end_word:
            case "NAME":
                library_vars["name"] = value.strip('"')
            case "HOMEPAGE":
                library_vars["homepage"] = value.strip('"')
            case "VERSION":
                library_vars["version"] = value.strip('"')
            case "LICENSE":
                library_license, exception = get_license_exception(value)
                library_vars["license"] = library_license
                library_vars["exception"] = exception
            case "COPYRIGHT":
                library_vars["copyright"] = value.strip('"')
            case "HASH":
                library_vars["_hash"] = value
            case "DEPSBUILDTIMEONLY":
                # Use only strings to simplify the type-checking.
                library_vars["_build_time_only"] = "True"

    # If there is no hash we assume it is not a real library but some other information on the file.
    # Also remove any library which is only used during build time and have no
    # artifact included in the final Blender binary.
    for key, lib_info_args in libraries_raw.items():
        if not (lib_info_args["_hash"] and not lib_info_args["_build_time_only"]):
            continue
        yield LibraryRaw(**{k: v for k, v in lib_info_args.items() if not k.startswith("_")})


def iterate_readme_files(base_dir: Path) -> Iterator[str]:
    base_path = Path(base_dir)

    # Iterate over all subdirectories.
    for subdir in base_path.iterdir():
        if not subdir.is_dir():
            continue

        readme_path = subdir / "README.blender"
        if not readme_path.exists():
            print(f"Warning: Missing file \"{readme_path}\"")
            continue

        with readme_path.open("r", encoding="utf8") as fh:
            contents = fh.read()
        yield contents


def process_readme_blender() -> Iterator[LibraryRaw]:
    """"Handle the README.blender files"""
    keys = {
        "Project": "name",
        "URL": "homepage",
        "License": "license",
        "Upstream version": "version",
        "Copyright": "copyright"
    }

    for readme in iterate_readme_files(DIRPATH_EXTERN_LIBRARIES):
        lines = readme.strip().split("\n")

        # Temporary storage for project fields.
        project_fields = {}

        for line in lines:
            line_split = line.split(":", 1)

            # Ignore comments and empty lines.
            if len(line_split) != 2:
                continue

            key, value = line_split
            key = key.strip()
            value = value.strip().strip('"')

            # Check if the current line matches one of the provided keys.
            if key in keys:
                project_fields[keys[key]] = value

        # Assign the fields to the project name.
        project_name = project_fields.get("name", "Unknown Project")

        # Split the license into license and its (optional) extension.
        library_license, exception = get_license_exception(project_fields.get("license", ""))

        yield LibraryRaw(
            name=project_name,
            version=project_fields.get("version", ""),
            homepage=project_fields.get("homepage", ""),
            license=library_license,
            exception=exception,
            copyright=project_fields.get("copyright", ""),
        )


def fetch_libraries_licenses() -> dict[str, License]:
    """Populate the licenses dict with its corresponding libraries and copyrights"""

    licenses = initialize_licenses()

    # Intermediate storage.

    # Map the license name to all libraries that use it.
    # Keys may be: `SPDX:GPL-2.0-or-later`, `SPDX:MIT`, ... `ICS` etc.
    licenses_data: dict[str, list[LibraryRaw]] = {}

    for lib_info in itertools.chain(
        # Get data from `./build_files/build_environment/cmake/versions.cmake`.
        process_versions_cmake(),
        # Get data from `README.blender` files.
        process_readme_blender(),
    ):
        license_name = lib_info.license or NO_LICENSE
        if (libraries_data := licenses_data.get(license_name)) is None:
            libraries_data = licenses_data[license_name] = []
        libraries_data.append(lib_info)

    # Populate licenses with the corresponding libraries.
    for license_key, libraries_data in licenses_data.items():
        if license_key == NO_LICENSE:
            print("Warning: The following libraries have no license:")
            for lib_info in libraries_data:
                print(f" * {lib_info.name}")
            continue
        if license_key in LICENSES_NOT_NEEDED:
            # Do nothing about these licenses.
            continue
        if (license_obj := licenses.get(license_key)) is None:
            # Do nothing about these licenses.
            print(f"Error: {license_key} license not found in: \"{FILEPATH_LICENSES_INDEX}\"")
            continue

        for lib_info in libraries_data:
            library = Library(
                name=lib_info.name,
                version=lib_info.version,
                homepage=lib_info.homepage,
                library_copyright=lib_info.copyright,
                exception=lib_info.exception,
            )
            license_obj.libraries.append(library)

    return licenses


def extract_licenses(text: str) -> set[str]:
    """Extract all the licenses from the text

    Licenses are defined under <>, and |link is ignored.

    For example, for the input:
     * <SPDX:GPL-3.0-or-later|link>
     * <Example-Fonts>

   The output would be:
     {"SPDX:GPL-3.0-or-later", "Example-Fonts"}
    """
    # Remove multi-line comments (<!-- ... -->).
    text = re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)

    # Find all licenses in < >.
    license_pattern = r"<([^<|>]+?)>"

    # Find all matches.
    matches = re.findall(license_pattern, text)

    # Extract unique licenses while ignoring emails.
    licenses = {match.strip() for match in matches if "@" not in match}

    return licenses


def get_introduction(licenses: dict[str, License]) -> str:
    introduction = INTRODUCTION
    license_lookups = extract_licenses(INTRODUCTION)

    for license_lookup in license_lookups:
        if license_lookup not in licenses:
            print(f"Error: {license_lookup} license not found in: \"{FILEPATH_LICENSES_INDEX}\"")
            continue

        license_item = licenses[license_lookup]
        introduction = introduction.replace(
            f"<{license_lookup}>",
            license_item.dump()
        )

        introduction = introduction.replace(
            f"<{license_lookup}|link>",
            f"[{license_item.name}]({license_item.url})"
        )

    return introduction


def generate_license_file(licenses: dict[str, License]) -> None:
    filepath = FILEPATH_LICENSE_GENERATED

    with open(filepath, "w", encoding="utf8") as fh:
        fh.write(get_introduction(licenses))

        for license_item in sorted(licenses.values()):
            if len(license_item.libraries) == 0:
                continue

            if license_item.url:
                fh.write(f"\n\n## [{license_item.name}]({license_item.url})\n\n")
            else:
                fh.write(f"\n\n## {license_item.name}\n\n")

            if license_item.copyright_exemption:
                fh.write(f"{license_item.copyright_exemption}\n\n")
                fh.write("| Library | Version |\n")
                fh.write("| ------- | ------- |\n")
            else:
                fh.write("| Library | Version | Copyright |\n")
                fh.write("| ------- | ------- | --------- |\n")

            for library in sorted(license_item.libraries):
                fh.write(library.dump(license_item))

            fh.write(license_item.dump())
            for i, exception in enumerate(license_item.exceptions):
                exception_license = licenses.get(exception)

                if exception_license is None:
                    print(f"Error: {exception} extension license not found in: \"{FILEPATH_LICENSES_INDEX}\"")
                    continue

                fh.write(exception_license.dump(i + 1))
        fh.write("\n")

    print(f"\nLicense file successfully generated: \"{filepath}\"")
    print("Remember to commit the file to the Blender repository.\n")


# -----------------------------------------------------------------------------
# Main Function


def main() -> None:
    licenses = fetch_libraries_licenses()

    generate_license_file(licenses)


if __name__ == "__main__":
    main()
