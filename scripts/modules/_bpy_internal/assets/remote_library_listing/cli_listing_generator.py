# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

"""Blender Online Asset Repository Listing Generator."""

import argparse
import dataclasses
import logging
import sys
from pathlib import Path
from typing import Any

import fastjsonschema
import cattrs.preconf.json

from . import asset_catalogs, asset_finder, listing_common, pagination, json_parsing
from . import blender_asset_library_openapi as api_models

SCHEMA_VERSION = "1.0.0"


_api_versions = {
    "v{:d}".format(listing_common.API_VERSION): listing_common.API_VERSIONED_ASSET_INDEX_JSON_PATH,
}
DEFAULT_METADATA = api_models.AssetLibraryMeta(
    api_versions=_api_versions.copy(),
    name="Your Asset Library",
    contact=api_models.Contact(
        name="Your Name",
        url="https://example.org/",
        email="example@example.org",
    ),
)

logger = logging.getLogger(__name__)

_converter = cattrs.preconf.json.JsonConverter(omit_if_default=True)


@dataclasses.dataclass
class CLIArguments:
    """Parsed commandline arguments."""

    repository: Path
    limit: int
    page_size: int


def cli_main(arguments_raw: argparse.Namespace) -> None:
    """Generate the index for the passed-on-the-CLI asset library path."""

    # Parse CLI arguments.
    arguments = _parse_cli_args(arguments_raw)

    # Write the top-level meta file first. If this already exists, an attempt
    # at parsing & upgrading it is performed. Better to do this (and stop on
    # errors) before diving into the assets themselves.
    _write_toplevel_meta(arguments)

    # Find all .blend files.
    filepaths: list[Path] = []
    logger.info("Traversing %s", arguments.repository)
    for filepath in arguments.repository.rglob("*.blend"):
        filepaths.append(filepath)

    files_total = len(filepaths)
    logger.info(f"* {files_total} .blend files found.")

    limit = _total_files_to_process(arguments, files_total)

    # Find the assets in the blend files.
    logger.info("Parsing the files...")
    assets: list[api_models.AssetV1] = []
    files: list[api_models.FileV1] = []

    for i, filepath in enumerate(filepaths[:limit]):
        logger.info(f"* {i + 1}/{limit}: {filepath.relative_to(arguments.repository)}")

        bfile_info, assets_in_file = asset_finder.list_assets(filepath, arguments.repository)
        if not assets_in_file:
            continue

        assets.extend(assets_in_file)
        files.append(bfile_info)

    # Write the output.
    asset_index_pages = pagination.paginate_asset_list(assets, files, arguments.page_size)
    _write_json_files(arguments, asset_index_pages)


def _write_toplevel_meta(arguments: CLIArguments) -> None:
    outdir_root = arguments.repository

    # Metadata file /_asset-library-meta.json. This gets loaded if it exists.
    meta_json_path = outdir_root / listing_common.ASSET_TOP_METADATA_FILENAME
    try:
        metadata = _toplevel_metadata(meta_json_path)
    except fastjsonschema.JsonSchemaException as ex:
        msg = "Metadata file {} could not be parsed as JSON: {}"
        logger.error(msg.format(meta_json_path, ex))
        raise SystemExit(1) from None

    _save_json(metadata, meta_json_path)


def _write_json_files(
    arguments: CLIArguments,
    asset_index_pages: list[api_models.AssetLibraryIndexPageV1],
) -> None:
    outdir_root = arguments.repository
    outdir_versioned = outdir_root / listing_common.API_VERSIONED_SUBDIR

    # Remove old pages, in case the number of assets per page was increased and
    # so less page files are needed.
    existing_pages = outdir_versioned.glob("assets-*.json")
    for filepath in existing_pages:
        filepath.unlink()

    # Library Index Page /_v1/assets-{page}.json
    #
    # Note that these paths are determined by the generator, and their URLs are
    # listed explicitly in the index file, so there is no need to have those in
    # the listing_common.py file.
    page_urls = []
    for page_index, page in enumerate(asset_index_pages):
        page_relpath = listing_common.api_versioned(f"assets-{page_index:05}.json")
        page_urls.append(page_relpath.as_posix())

        _save_json(page, outdir_root / page_relpath)

    # Library Index file /_v1/asset-index.json:
    total_asset_count = sum(page.asset_count for page in asset_index_pages)
    total_file_count = sum(page.file_count for page in asset_index_pages)
    asset_size_bytes = sum(file.size_in_bytes
                           for page in asset_index_pages
                           for file in page.files)
    asset_cats = asset_catalogs.parse_catalogs(arguments.repository)
    index = api_models.AssetLibraryIndexV1(
        schema_version=SCHEMA_VERSION,
        asset_size_bytes=asset_size_bytes,
        asset_count=total_asset_count,
        file_count=total_file_count,
        page_urls=page_urls,
        catalogs=asset_cats,
    )
    _save_json(index, outdir_versioned / listing_common.ASSET_INDEX_JSON_FILENAME)


def _save_json(model: Any, json_path: Path) -> None:
    as_json = _converter.dumps(model, indent=2)

    json_path.parent.mkdir(exist_ok=True, parents=True)

    logger.info("Writing %s", json_path)
    with json_path.open("wt") as json_file:
        json_file.write(as_json)


def _toplevel_metadata(json_path: Path) -> api_models.AssetLibraryMeta:
    """Construct the top-level metadata.

    Returns the metadata, or raises an exception (see json_parsing.ValidatingParser)
    if it is not valid JSON.

    Writing is considered safe, except when the file exists but does not contain
    valid JSON. In that case, it's better to warn about this and keep the file
    as-is, so that the user can either delete or fix it.
    """
    try:
        json_data = json_path.read_bytes()
    except IOError:
        # Ignore any read errors, as this likely means the file simply doesn't exist.
        return DEFAULT_METADATA

    parser = json_parsing.ValidatingParser(api_models.OPENAPI_SPEC)
    metadata = parser.parse_and_validate(api_models.AssetLibraryMeta, json_data)

    # Update the metadata to declare the API version for which we're going to
    # write the data.
    metadata.api_versions = DEFAULT_METADATA.api_versions.copy()

    return metadata


# Ignore the type of the `subparsers` argument, because there doesn't seem
# to be a way to make both static mypy and the runtime Python happy at the
# same time.
def add_cli_parser(subparsers: argparse._SubParsersAction) -> None:  # type: ignore[type-arg]
    """Add argparser for this subcommand."""

    parser = subparsers.add_parser("generate", help="Generate files necessary to serve an asset library")
    parser.set_defaults(func=cli_main)

    parser.add_argument(
        "repository",
        type=Path,
        help="""Asset repository folder""",
    )

    parser.add_argument(
        "--limit",
        "-l",
        metavar="NUM_BLEND_FILES",
        type=int,
        default=None,
        help="Limit the number of files to process",
    )

    parser.add_argument(
        "--page",
        "-p",
        metavar="ASSETS_PER_PAGE",
        type=int,
        default=1000,
        help="Number of assets per JSON file, set to 0 to disable pagination",
    )


def _parse_cli_args(arguments_raw: argparse.Namespace) -> CLIArguments:
    """Make sure the passed arguments are valid."""

    repository = arguments_raw.repository.absolute()
    if not repository.is_dir():
        print(f"Error: Repository specified is not a folder: {repository}")
        sys.exit(1)

    arguments = CLIArguments(
        repository=repository,
        limit=arguments_raw.limit or 0,
        page_size=arguments_raw.page or 0,
    )

    return arguments


def _total_files_to_process(arguments: CLIArguments, files_total: int) -> int:
    if not arguments.limit:
        return files_total

    return min(arguments.limit, files_total)
