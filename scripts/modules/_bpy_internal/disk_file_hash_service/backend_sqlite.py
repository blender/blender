# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

__all__ = (
    'SQLiteBackend',
)

import contextlib
import datetime
import sqlite3
from pathlib import Path
from typing import Iterator, Callable

from . import types


DB_TIMEOUT_MSEC = 5000  # SQLite busy timeout in milliseconds.
DB_SCHEMA_VERSION = 1
CREATE_SCHEMA_V1 = """
BEGIN EXCLUSIVE;
CREATE TABLE IF NOT EXISTS files (
    file_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    path TEXT UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS hashes (
    file_id INTEGER NOT NULL,
    hash_algo VARCHAR(10) NOT NULL,
    hexdigest TEXT NOT NULL,
    size_in_bytes BIGINT NOT NULL,
    file_stat_mtime FLOAT NOT NULL,
    last_checked DATETIME NOT NULL,
    PRIMARY KEY(file_id, hash_algo)
    FOREIGN KEY(file_id) REFERENCES files(file_id) ON DELETE CASCADE ON UPDATE CASCADE
);
COMMIT;
"""

# Set to True to print all SQL queries.
_DEBUG_QUERIES = False


class SQLiteBackend:
    """DiskFileHashBackend implementation using SQLite as storage engine."""

    dbfile_path: Path  # Path of the .sqlite file to use.
    _storage_path: Path  # The original storage path, only for the '__repr__' function.

    db_conn_rw: sqlite3.Connection | None = None
    db_conn_ro: sqlite3.Connection | None = None

    def __init__(self, storage_path: Path) -> None:
        assert not storage_path.is_dir(), "SQLite back-end expects a directory + file prefix as storage path"

        self._storage_path = storage_path
        self.dbfile_path = storage_path.with_name("{}_v{}.sqlite".format(storage_path.stem, DB_SCHEMA_VERSION))
        self.db_conn_rw = None
        self.db_conn_ro = None

    def __repr__(self) -> str:
        return "{!s}({!r})".format(self.__class__.__qualname__, self._storage_path)

    def open(self) -> None:
        """Prepare the back-end for use.

        Create the directory structure & database file, and ensure the schema is as expected.
        """
        import sqlite3

        self.dbfile_path.parent.mkdir(parents=True, exist_ok=True)

        # Open a read-write connection.
        # Once we upgrade to Python 3.12+, pass autocommit=False instead of isolation_level=None.
        self.db_conn_rw = sqlite3.connect(self.dbfile_path, timeout=DB_TIMEOUT_MSEC / 1000, isolation_level=None)
        if _DEBUG_QUERIES:
            def callback_rw(query: str) -> None:
                query = query.replace("\n", "\n    ")
                print(f"SQL/RW: {query}")
            self.db_conn_rw.set_trace_callback(callback_rw)
        self._execute_pragmas_on_connect(self.db_conn_rw)

        # Open a read-only connection.
        uri = self.dbfile_path.as_uri() + "?mode=ro"
        # Once we upgrade to Python 3.12+, pass autocommit=False instead of isolation_level=None.
        self.db_conn_ro = sqlite3.connect(uri, uri=True, timeout=DB_TIMEOUT_MSEC / 1000, isolation_level=None)
        if _DEBUG_QUERIES:
            def callback_ro(query: str) -> None:
                query = query.replace("\n", "\n    ")
                print(f"SQL/RO: {query}")
            self.db_conn_ro.set_trace_callback(callback_ro)
        self._execute_pragmas_on_connect(self.db_conn_ro)

        # Assumption: if the table exists, it should be in the right shape. If
        # that's not the case, the DB_SCHEMA_VERSION class variable should have
        # been incremented, and we'd be accessing another database file.
        #
        # This does not use our _transaction_rw() function, as the executescript()
        # function expects the transaction management to be included in the script
        # itself. It will auto-commit any already-opened transaction, before
        # running the script.
        self.db_conn_rw.executescript(CREATE_SCHEMA_V1)

    def close(self) -> None:
        """Close the database connection."""
        if self.db_conn_ro:
            self.db_conn_ro.close()
            self.db_conn_ro = None

        # Close the read-write connection last, otherwise the WAL journal files
        # will not be checkpointed and removed.
        if self.db_conn_rw:
            self.db_conn_rw.close()
            self.db_conn_rw = None

    def fetch_hash(self, filepath: Path, hash_algorithm: str) -> types.FileHashInfo | None:
        """Return the cached hash info of a given file.

        Returns a tuple (hexdigest, file size in bytes, last file mtime).
        """

        with self._transaction_ro() as db:
            cursor = db.execute(
                "SELECT h.size_in_bytes, h.hexdigest, h.file_stat_mtime " +
                "FROM files f INNER JOIN hashes h USING (file_id) " +
                "WHERE f.path=? AND h.hash_algo=?",
                (str(filepath), hash_algorithm))
            # The uniqueness constraints ensure there is at most one row.
            row = cursor.fetchone()

        if row is None:
            return None

        size, hex, mtime = row
        return types.FileHashInfo(
            hexhash=hex,
            file_size_bytes=size,
            file_stat_mtime=mtime,
        )

    def store_hash(
            self,
            filepath: Path,
            hash_algorithm: str,
            hash_info: types.FileHashInfo,
            pre_write_callback: Callable[[], None] | None = None,
    ) -> None:
        """Store a pre-computed hash for the given file path. The path has to exist."""
        now = self._now_string()

        with self._transaction_rw() as db:
            if pre_write_callback is not None:
                pre_write_callback()

            # The 'RETURNING file_id' ensures that we know which file ID was
            # referenced. We can't rely on last_insert_rowid() or
            # cursor.lastrowid, as that only works on actual INSERT and not on
            # the 'ON CONFLICT' part. The 'DO UPDATE SET file_id=file_id' is
            # senseless, but an update is necessary to get the `RETURNING
            # file_id` to work (it won't return with `ON CONFLICT DO NOTHING`).
            cursor = db.execute(
                "INSERT INTO files (path) values (?) ON CONFLICT DO UPDATE SET file_id=file_id RETURNING file_id",
                (str(filepath),),
            )
            file_id = cursor.fetchone()[0]
            assert file_id, "file_id={!r}".format(file_id)

            db.execute(
                "INSERT INTO hashes " +
                "(file_id, hash_algo, hexdigest, size_in_bytes, file_stat_mtime, last_checked) " +
                "VALUES (:file_id, :hash_algo, :hex, :size, :mtime, :now) ON CONFLICT DO UPDATE " +
                "SET hexdigest=:hex, size_in_bytes=:size, file_stat_mtime=:mtime, last_checked=:now", {
                    "file_id": file_id,
                    "hash_algo": hash_algorithm,
                    "hex": hash_info.hexhash,
                    "size": hash_info.file_size_bytes,
                    "mtime": hash_info.file_stat_mtime,
                    "now": now,
                },
            )

    def mark_hash_as_fresh(self, filepath: Path, hash_algorithm: str) -> None:
        """Store that the hash is still considered 'fresh'.

        See `remove_older_than()`.
        """

        now = self._now_string()
        with self._transaction_rw() as db:
            db.execute(
                "UPDATE hashes SET last_checked=? " +
                "WHERE file_id = (SELECT file_id FROM files WHERE path=?) AND hash_algo=?",
                (now, str(filepath), hash_algorithm))

    def remove_older_than(self, *, days: int) -> None:
        """Remove all hash entries that are older than this many days.

        When this removes all known hashes for a file, the file entry itself is
        also removed.
        """
        older_than = self._now() - datetime.timedelta(days=days)

        with self._transaction_rw() as db:
            # Delete all old hashes.
            db.execute("DELETE FROM hashes WHERE last_checked<?",
                       (older_than.isoformat(),))

            # Delete file entries for which there are no hashes known.
            db.execute(
                "DELETE FROM files WHERE file_id IN (" +
                "SELECT f.file_id FROM files f " +
                "LEFT JOIN hashes h USING (file_id) " +
                "GROUP BY f.file_id "
                "HAVING count(h.file_id) == 0" +
                ")")

    def _now(self) -> datetime.datetime:
        """Current time, as UTC, in a timezone-aware object."""
        return datetime.datetime.now(tz=datetime.timezone.utc)

    def _now_string(self) -> str:
        """Current time, as UTC, in ISO 6801 notation."""
        return self._now().isoformat()

    @contextlib.contextmanager
    def _transaction_rw(self) -> Iterator[sqlite3.Connection]:
        """Start a read-write transaction.

        The transaction is rolled back when an exception is raised, and
        committed otherwise.
        """
        assert self.db_conn_rw is not None, "Open the back-end before trying to use it"

        self.db_conn_rw.execute("BEGIN EXCLUSIVE")
        try:
            yield self.db_conn_rw
        except BaseException:
            self.db_conn_rw.rollback()
            raise
        else:
            self.db_conn_rw.commit()

    @contextlib.contextmanager
    def _transaction_ro(self) -> Iterator[sqlite3.Connection]:
        """Start a read-write transaction.

        The transaction is always rolled back, because it shouldn't write
        anything anyway.
        """
        assert self.db_conn_ro is not None, "Open the back-end before trying to use it"

        self.db_conn_ro.execute("BEGIN IMMEDIATE")
        try:
            yield self.db_conn_ro
        finally:
            self.db_conn_ro.rollback()

    def _execute_pragmas_on_connect(self, db_conn: sqlite3.Connection) -> None:
        db_conn.execute("PRAGMA busy_timeout = {:d}".format(DB_TIMEOUT_MSEC))
        db_conn.execute("PRAGMA foreign_keys = 1")
        db_conn.execute("PRAGMA journal_mode = WAL")
        db_conn.execute("PRAGMA synchronous = normal")
