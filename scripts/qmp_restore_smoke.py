#!/usr/bin/env python3
"""Save and restore an SF2000 QEMU machine with QMP migration."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path


PRE_SAVE_MARKER = "sf2000: loaded ASD"
POST_RESTORE_MARKERS = (
    "sf2000: entry-bytes storage_probe_entry pc=0x047c0050",
)


def parse_timeout(value: str) -> float:
    value = value.strip()
    if value.endswith("s"):
        value = value[:-1]
    return float(value)


def wait_for_path_contains(path: Path, needle: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    seen_size = -1
    while time.monotonic() < deadline:
        try:
            data = path.read_text(errors="replace")
        except FileNotFoundError:
            data = ""
        if needle in data:
            return
        current_size = len(data)
        if current_size != seen_size:
            seen_size = current_size
        time.sleep(0.1)
    raise RuntimeError(f"timed out waiting for {needle!r} in {path}")


def drain_response(stream) -> dict:
    while True:
        line = stream.readline()
        if not line:
            raise RuntimeError("unexpected EOF on QMP socket")
        message = json.loads(line.decode("utf-8"))
        if "event" in message:
            continue
        return message


class QMPClient:
    def __init__(self, socket_path: Path, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        while True:
            try:
                sock.connect(str(socket_path))
                break
            except (FileNotFoundError, ConnectionRefusedError, OSError):
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"timed out connecting to {socket_path}")
                time.sleep(0.1)
        self.sock = sock
        self.stream = sock.makefile("rwb", buffering=0)
        greeting = drain_response(self.stream)
        if "QMP" not in greeting:
            raise RuntimeError(f"unexpected QMP greeting: {greeting}")

    def command(self, name: str, arguments: dict | None = None) -> dict:
        payload = {"execute": name}
        if arguments:
            payload["arguments"] = arguments
        self.stream.write((json.dumps(payload) + "\n").encode("utf-8"))
        response = drain_response(self.stream)
        if "error" in response:
            raise RuntimeError(f"QMP {name} failed: {response['error']}")
        return response.get("return", {})

    def close(self) -> None:
        try:
            self.stream.close()
        finally:
            self.sock.close()


def spawn_qemu(argv: list[str], console_path: Path, log_path: Path) -> subprocess.Popen:
    console_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    console_fd = console_path.open("wb")
    log_fd = log_path.open("wb")
    try:
        proc = subprocess.Popen(argv, stdout=console_fd, stderr=console_fd)
    finally:
        console_fd.close()
        log_fd.close()
    return proc


def wait_for_process(proc: subprocess.Popen, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc = proc.poll()
        if rc is not None:
            raise RuntimeError(f"qemu exited early with status {rc}")
        time.sleep(0.1)
    raise RuntimeError("timed out waiting for qemu process")


def terminate_process(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--cpu", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--append", required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--source-console", required=True)
    parser.add_argument("--source-log", required=True)
    parser.add_argument("--restore-console", required=True)
    parser.add_argument("--restore-log", required=True)
    parser.add_argument("--socket", required=True)
    parser.add_argument("--restore-socket", required=True)
    parser.add_argument("--timeout", default="120s")
    args = parser.parse_args()

    timeout = parse_timeout(args.timeout)
    state = Path(args.state)
    source_console = Path(args.source_console)
    source_log = Path(args.source_log)
    restore_console = Path(args.restore_console)
    restore_log = Path(args.restore_log)
    socket_path = Path(args.socket)
    restore_socket_path = Path(args.restore_socket)

    for path in (
        state,
        source_console,
        source_log,
        restore_console,
        restore_log,
        socket_path,
        restore_socket_path,
    ):
        try:
            path.unlink()
        except FileNotFoundError:
            pass

    source_cmd = [
        args.qemu,
        "-M",
        "sf2000",
        "-cpu",
        args.cpu,
        "-kernel",
        args.kernel,
        "-append",
        args.append,
        "-display",
        "none",
        "-serial",
        "none",
        "-monitor",
        "none",
        "-qmp",
        f"unix:{socket_path},server,nowait",
        "-d",
        "guest_errors,unimp",
        "-D",
        str(source_log),
    ]
    source_proc = subprocess.Popen(
        source_cmd,
        stdout=source_console.open("wb"),
        stderr=subprocess.STDOUT,
    )
    try:
        wait_for_path_contains(source_console, PRE_SAVE_MARKER, timeout)
        qmp = QMPClient(socket_path, timeout)
        try:
            qmp.command("qmp_capabilities")
            qmp.command("stop")
            qmp.command(
                "migrate",
                {"uri": f"exec:sh -c {shlex.quote(f'cat > {state}')}"},
            )
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                status = qmp.command("query-migrate").get("status")
                if status == "completed":
                    break
                if status in {"failed", "cancelled"}:
                    raise RuntimeError(f"migration failed with status {status}")
                time.sleep(0.1)
            else:
                raise RuntimeError("timed out waiting for migration completion")
        finally:
            qmp.close()
        terminate_process(source_proc)

        restore_cmd = [
            args.qemu,
            "-M",
            "sf2000",
            "-cpu",
            args.cpu,
            "-kernel",
            args.kernel,
            "-append",
            args.append,
            "-incoming",
            f"file:{state}",
            "-display",
            "none",
            "-serial",
            "none",
            "-monitor",
            "none",
            "-qmp",
            f"unix:{restore_socket_path},server,nowait",
            "-d",
            "guest_errors,unimp",
            "-D",
            str(restore_log),
        ]
        restore_proc = subprocess.Popen(
            restore_cmd,
            stdout=restore_console.open("wb"),
            stderr=subprocess.STDOUT,
        )
        try:
            restore_qmp = QMPClient(restore_socket_path, timeout)
            try:
                restore_qmp.command("qmp_capabilities")
                status = restore_qmp.command("query-status").get("status")
                if status == "paused":
                    restore_qmp.command("cont")
            finally:
                restore_qmp.close()
            for marker in POST_RESTORE_MARKERS:
                wait_for_path_contains(restore_log, marker, timeout)
        finally:
            terminate_process(restore_proc)
    finally:
        terminate_process(source_proc)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
