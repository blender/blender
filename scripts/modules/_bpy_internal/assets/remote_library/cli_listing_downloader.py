# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import argparse
import dataclasses
import logging
import time
import urllib.parse
from pathlib import Path

from . import listing_downloader

logger = logging.getLogger(__name__)


@dataclasses.dataclass
class CLIArguments:
    """Parsed command-line arguments."""

    url: str


def cli_main(arguments_raw: argparse.Namespace) -> None:
    """Generate the index for the passed-on-the-CLI asset library path."""

    # Parse CLI arguments.
    arguments = _parse_cli_args(arguments_raw)

    base_path = Path(".").resolve() / "_asset_download_location"  # TODO: be sensible.

    is_done = False

    def on_done_callback(_: listing_downloader.RemoteAssetListingDownloader) -> None:
        nonlocal is_done

        is_done = True

    downloader = listing_downloader.RemoteAssetListingDownloader(
        arguments.url,
        base_path,
        lambda *args: None,
        on_done_callback)
    downloader.download_and_process()

    while not is_done:
        # Ordinarily Blender's timer system will call the right method. But
        # because this is intended to run headless, and we're blocking the main
        # thread here, that doesn't happen.
        downloader.on_timer_event()
        time.sleep(downloader._DOWNLOAD_POLL_INTERVAL)

    print("Done!")


# Ignore the type of the `subparsers` argument, because there doesn't seem
# to be a way to make both static mypy and the runtime Python happy at the
# same time.


def add_cli_parser(subparsers: argparse._SubParsersAction) -> None:  # type: ignore[type-arg]
    """Add argparser for this subcommand."""

    parser = subparsers.add_parser("download", help="Download and parse a remote asset library index")
    parser.set_defaults(func=cli_main)

    parser.add_argument(
        "url",
        type=str,
        help="""URL of the remote asset library""",
    )


def _parse_cli_args(arguments_raw: argparse.Namespace) -> CLIArguments:
    """Make sure the passed arguments are valid."""

    try:
        urllib.parse.urlparse(arguments_raw.url)
    except ValueError as ex:
        logger.error("invalid URL specified: {}".format(ex))

    arguments = CLIArguments(
        url=arguments_raw.url,
    )

    return arguments


class APIVersionError(Exception):
    """Raised when none of the API versions declared by a remote asset library are supported by Blender."""
