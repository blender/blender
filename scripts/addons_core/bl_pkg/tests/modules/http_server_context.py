# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Starts up a web server pointed to a local directory for the purpose of simulating online access.
With basic options for PORT/path & verbosity (so tests aren't too noisy).
"""
__all__ = (
    "HTTPServerContext",
)

import socketserver
import http.server
import threading

from typing import (
    Any,
)


class HTTPServerContext:
    __slots__ = (
        "_directory",
        "_port",
        "_http_thread",
        "_http_server",
        "_wait_tries",
        "_wait_delay",
        "_verbose",
    )

    class _TestServer(socketserver.TCPServer):
        allow_reuse_address = True

    @staticmethod
    def _is_port_in_use(port: int) -> bool:
        import socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            return s.connect_ex(("localhost", port)) == 0

    @staticmethod
    def _test_handler_factory(directory: str, verbose: bool = False) -> type:
        class TestHandler(http.server.SimpleHTTPRequestHandler):
            def __init__(self, *args: Any, **kwargs: Any) -> None:
                super().__init__(*args, directory=directory, **kwargs)
            # Suppress messages by overriding the function.
            if not verbose:
                def log_message(self, *_args: Any, **_kw: Any) -> None:
                    pass
        return TestHandler

    def __init__(
            self,
            directory: str,
            port: int,
            *,
            verbose: bool = False,
            wait_delay: float = 0.0,
            wait_tries: int = 0,
    ) -> None:
        self._directory = directory
        self._port = port
        self._wait_delay = wait_delay
        self._wait_tries = wait_tries
        self._verbose = verbose

        # Members `_http_thread` & `_http_server` are set when entering the context.

    def __enter__(self) -> None:

        if self._wait_tries:
            import time
            for _ in range(self._wait_tries):
                if not HTTPServerContext._is_port_in_use(self._port):
                    break

                print("Waiting...")
                time.sleep(self._wait_delay)

        http_server = HTTPServerContext._TestServer(
            ("", self._port),
            HTTPServerContext._test_handler_factory(
                self._directory,
                verbose=self._verbose,
            ),
        )

        # Use a thread so as not to block.
        http_thread = threading.Thread(target=http_server.serve_forever)
        http_thread.daemon = True
        http_thread.start()

        self._http_thread = http_thread
        self._http_server = http_server

    def __exit__(self, _type: Any, _value: Any, traceback: Any) -> None:
        # Needed on WIN32, otherwise exit causes an `OSError`.
        self._http_server.shutdown()

        self._http_server.server_close()
        del self._http_server
        del self._http_thread
