# Craft web playground

This local interface invokes the existing Craft executable for both file runs
and a persistent REPL session.

```sh
python playground/server.py
```

Open <http://127.0.0.1:8765>.

Use another executable or port with:

```sh
python playground/server.py --exe debug.exe --port 9000
```

The server binds to `127.0.0.1` by default. Source submitted with **Run** is
written to a temporary `.lox` file and passed to the executable. The REPL tab
keeps one executable process alive until it is reset or the server stops.
