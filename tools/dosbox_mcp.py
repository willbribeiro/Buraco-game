#!/usr/bin/env python3
"""Minimal client/server bridge for the DOSBox-X debugger MCP control port.

DOSBox-X acts as a TCP *client*: it connects to 127.0.0.1:<port>, then speaks
a line-oriented protocol:

    REQ <id> PING
    REQ <id> BREAK
    REQ <id> EXEC <debugger command>

    BEGIN <id> OK
    ...output...
    END <id>

This module starts the listener, launches DOSBox-X with -debug, and exposes
`exec_debug(cmd)` / `run()` helpers.
"""

from __future__ import annotations

import os
import socket
import subprocess
import sys
import time
from pathlib import Path

DOSBOX = r"C:\DOSBox-X\dosbox-x.exe"
ROOT = Path(r"C:\Users\ventoy\Documents\Buraco")


class DosBoxDebugger:
    def __init__(self, port: int = 58991, host: str = "127.0.0.1"):
        self.port = port
        self.host = host
        self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind((host, port))
        self.srv.listen(1)
        self.srv.settimeout(60)
        self.conn: socket.socket | None = None
        self.rfile = None
        self.next_id = 1

    def accept(self, timeout: float = 60.0) -> None:
        self.srv.settimeout(timeout)
        conn, addr = self.srv.accept()
        conn.settimeout(timeout)
        self.conn = conn
        self.rfile = conn.makefile("r", encoding="latin-1", newline="\n")
        print(f"[mcp] DOSBox-X connected from {addr}", flush=True)

    def drop(self) -> None:
        """Drop the current control socket; DOSBox-X reconnects on the next break."""
        try:
            if self.rfile:
                self.rfile.close()
        except Exception:
            pass
        try:
            if self.conn:
                self.conn.close()
        except Exception:
            pass
        self.rfile = None
        self.conn = None

    def _send(self, body: str) -> int:
        assert self.conn is not None
        rid = self.next_id
        self.next_id += 1
        line = f"REQ {rid} {body}\n"
        self.conn.sendall(line.encode("latin-1"))
        return rid

    def _read_response(self, rid: int, timeout: float) -> str:
        assert self.conn is not None
        self.conn.settimeout(timeout)
        begin = f"BEGIN {rid} "
        end = f"END {rid}"
        status = None
        out: list[str] = []
        while True:
            line = self.rfile.readline()
            if line == "":
                raise RuntimeError("DOSBox-X closed the control connection")
            line = line.rstrip("\r\n")
            if line.startswith(begin):
                status = line[len(begin):]
                continue
            if line == end:
                if status is None:
                    raise RuntimeError("END without BEGIN")
                if status != "OK":
                    raise RuntimeError("DOSBox-X error: " + "\n".join(out))
                return "\n".join(out)
            out.append(line)

    def request(self, body: str, timeout: float = 60.0) -> str:
        rid = self._send(body)
        return self._read_response(rid, timeout)

    def ping(self, timeout: float = 10.0) -> str:
        return self.request("PING", timeout)

    def exec(self, command: str, timeout: float = 60.0) -> str:
        return self.request("EXEC " + command, timeout)

    def break_(self, timeout: float = 10.0) -> str:
        return self.request("BREAK", timeout)

    def close(self) -> None:
        try:
            if self.conn:
                self.conn.close()
        finally:
            self.srv.close()


def launch(conf: Path, log: Path, debug: bool = True) -> subprocess.Popen:
    log.parent.mkdir(parents=True, exist_ok=True)
    f = open(log, "wb")
    args = [DOSBOX, "-conf", str(conf), "-fastlaunch"]
    if debug:
        args.insert(1, "-debug")
    proc = subprocess.Popen(
        args,
        cwd=str(ROOT),
        stdout=f,
        stderr=subprocess.STDOUT,
    )
    return proc
