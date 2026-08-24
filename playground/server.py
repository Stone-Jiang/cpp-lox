#!/usr/bin/env python3
"""Local web bridge for the Craft executable."""

from __future__ import annotations

import argparse
import json
import os
import queue
import subprocess
import tempfile
import threading
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parent
MAX_BODY_BYTES = 1024 * 1024
PROMPT = b">> "


class ReplSession:
    def __init__(self, executable: Path, timeout: float) -> None:
        self.executable = executable
        self.timeout = timeout
        self.process: subprocess.Popen[bytes] | None = None
        self.output: queue.Queue[bytes | None] = queue.Queue()
        self.lock = threading.Lock()

    def _reader(self, stream) -> None:
        try:
            while True:
                chunk = stream.read(1)
                if not chunk:
                    break
                self.output.put(chunk)
        finally:
            self.output.put(None)

    def _read_to_prompt(self) -> str:
        data = bytearray()
        while not data.endswith(PROMPT):
            try:
                chunk = self.output.get(timeout=self.timeout)
            except queue.Empty as error:
                self.stop()
                raise TimeoutError("The executable did not return to the REPL prompt.") from error

            if chunk is None:
                code = self.process.poll() if self.process else None
                self.stop()
                raise RuntimeError(f"The REPL process exited unexpectedly ({code}).")
            data.extend(chunk)

        return data[:-len(PROMPT)].decode("utf-8", errors="replace")

    def _start(self) -> None:
        self.output = queue.Queue()
        self.process = subprocess.Popen(
            [str(self.executable)],
            cwd=PROJECT_ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        assert self.process.stdout is not None
        threading.Thread(
            target=self._reader,
            args=(self.process.stdout,),
            daemon=True,
        ).start()
        self._read_to_prompt()

    def evaluate(self, source: str) -> str:
        if "\n" in source or "\r" in source:
            raise ValueError("REPL input must be a single line.")

        with self.lock:
            if self.process is None or self.process.poll() is not None:
                self._start()

            assert self.process is not None and self.process.stdin is not None
            self.process.stdin.write(source.encode("utf-8") + b"\n")
            self.process.stdin.flush()
            return self._read_to_prompt()

    def reset(self) -> None:
        with self.lock:
            self.stop()

    def stop(self) -> None:
        process, self.process = self.process, None
        if process is None or process.poll() is not None:
            return
        process.terminate()
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=1)


class PlaygroundHandler(SimpleHTTPRequestHandler):
    server_version = "CraftPlayground/1.0"

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def log_message(self, format: str, *args) -> None:
        print(f"[{self.log_date_time_string()}] {format % args}")

    def _json_body(self) -> dict:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError as error:
            raise ValueError("Invalid Content-Length.") from error
        if length <= 0 or length > MAX_BODY_BYTES:
            raise ValueError("Request body is empty or too large.")
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def _send_json(self, status: HTTPStatus, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _run_source(self, source: str) -> dict:
        path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="w",
                suffix=".lox",
                prefix="craft-playground-",
                encoding="utf-8",
                delete=False,
            ) as file:
                file.write(source)
                path = Path(file.name)

            completed = subprocess.run(
                [str(self.server.executable), str(path)],
                cwd=PROJECT_ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=self.server.execution_timeout,
                check=False,
            )
            return {
                "output": completed.stdout.decode("utf-8", errors="replace"),
                "exitCode": completed.returncode,
            }
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or b"").decode("utf-8", errors="replace")
            return {
                "output": output + "\n[playground] Execution timed out.\n",
                "exitCode": 124,
            }
        finally:
            if path is not None:
                path.unlink(missing_ok=True)

    def do_POST(self) -> None:
        route = urlparse(self.path).path
        try:
            body = self._json_body()
            if route == "/api/run":
                source = body.get("source")
                if not isinstance(source, str):
                    raise ValueError("source must be a string.")
                self._send_json(HTTPStatus.OK, self._run_source(source))
                return

            if route == "/api/repl":
                source = body.get("source")
                if not isinstance(source, str):
                    raise ValueError("source must be a string.")
                output = self.server.repl.evaluate(source)
                self._send_json(HTTPStatus.OK, {"output": output})
                return

            if route == "/api/repl/reset":
                self.server.repl.reset()
                self._send_json(HTTPStatus.OK, {"ok": True})
                return

            self._send_json(HTTPStatus.NOT_FOUND, {"error": "Not found."})
        except (ValueError, json.JSONDecodeError) as error:
            self._send_json(HTTPStatus.BAD_REQUEST, {"error": str(error)})
        except (RuntimeError, TimeoutError, BrokenPipeError) as error:
            self._send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": str(error)})


class PlaygroundServer(ThreadingHTTPServer):
    def __init__(self, address, executable: Path, timeout: float) -> None:
        super().__init__(address, PlaygroundHandler)
        self.executable = executable
        self.execution_timeout = timeout
        self.repl = ReplSession(executable, timeout)

    def server_close(self) -> None:
        self.repl.reset()
        super().server_close()


def parse_args() -> argparse.Namespace:
    default_exe = PROJECT_ROOT / ("main.exe" if os.name == "nt" else "main")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=default_exe)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--timeout", type=float, default=10.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    executable = args.exe.expanduser().resolve()
    if not executable.is_file():
        raise SystemExit(f"Executable not found: {executable}")

    server = PlaygroundServer((args.host, args.port), executable, args.timeout)
    print(f"Craft playground: http://{args.host}:{args.port}")
    print(f"Executable: {executable}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
