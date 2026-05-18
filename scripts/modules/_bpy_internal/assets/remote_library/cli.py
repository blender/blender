# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import argparse
import datetime
import logging
import time


def main(cli_args: list[str]) -> None:
    """CLI entry point for the 'asset_listing' CLI commands."""

    parser = argparse.ArgumentParser(
        prog="blender -c asset_listing",
        description="Manage asset library index files.",
    )

    # func is set by subparsers to indicate which function to run.
    parser.set_defaults(func=None, loglevel=logging.INFO)

    loggroup = parser.add_mutually_exclusive_group()
    loggroup.add_argument(
        "-v",
        "--verbose",
        dest="loglevel",
        action="store_const",
        const=logging.DEBUG,
        help="Log DEBUG level and higher",
    )
    loggroup.add_argument(
        "-q",
        "--quiet",
        dest="loglevel",
        action="store_const",
        const=logging.WARNING,
        help="Log at WARNING level and higher",
    )

    subparsers = parser.add_subparsers(
        help="Choose a subcommand to actually make Blender do something. "
        "Global options go before the subcommand, "
        "whereas subcommand-specific options go after it. "
        "Use --help after the subcommand to get more info."
    )

    from . import cli_listing_generator, cli_listing_downloader

    cli_listing_generator.add_cli_parser(subparsers)
    cli_listing_downloader.add_cli_parser(subparsers)

    args = parser.parse_args(cli_args)

    config_logging(args)
    log = logging.getLogger(__name__)

    if not args.func:
        parser.error("No subcommand was given")

    start_time = time.monotonic()
    args.func(args)

    duration = datetime.timedelta(seconds=time.monotonic() - start_time)
    log.info("Command took %s to complete", duration)


def config_logging(args) -> None:  # type: ignore
    """Configures the logging system based on CLI arguments."""

    logging.basicConfig(
        level=args.loglevel,
        format="%(asctime)-15s %(levelname)8s %(threadName)10s %(name)16s %(message)s",
    )
