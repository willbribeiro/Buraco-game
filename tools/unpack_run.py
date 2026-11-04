import sys
import socket
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
log = ROOT / "tools" / "dosbox-mcp.log"
mem = ROOT / "work" / "mem.bin"
font = ROOT / "port" / "assets" / "font8x16.bin"
mem.parent.mkdir(parents=True, exist_ok=True)
font.parent.mkdir(parents=True, exist_ok=True)

dbg = DosBoxDebugger()
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", repr(dbg.ping()), flush=True)

    for cmd in ["BPDEL *", "BPINT 10 00"]:
        print(f"$ {cmd}\n{dbg.exec(cmd)}", flush=True)

    print("$ RUN (waiting for first INT 10h AH=00h)...", flush=True)
    try:
        print(dbg.exec("RUN", timeout=90), flush=True)
    except (socket.timeout, TimeoutError):
        print("RUN timed out; issuing BREAK", flush=True)
        try:
            print(dbg.break_(timeout=10), flush=True)
        except Exception as e:
            print("BREAK failed:", e, flush=True)

    for cmd in ["CPU", "R", "DOS MCBS"]:
        print(f"$ {cmd}\n{dbg.exec(cmd)}", flush=True)

    print("$ STEP (execute the INT 10h call)", flush=True)
    try:
        print(dbg.exec("STEP", timeout=20), flush=True)
    except Exception as e:
        print("STEP failed:", e, flush=True)

    for cmd, path in [
        ("MEMDUMPBIN 0:0 100000 " + str(mem), mem),
        ("VGA FONTDUMP " + str(font), font),
    ]:
        print(f"$ {cmd}", flush=True)
        try:
            print(dbg.exec(cmd, timeout=60), flush=True)
        except Exception as e:
            print("failed:", e, flush=True)

    print("$ VGA DAC", flush=True)
    try:
        print(dbg.exec("VGA DAC", timeout=30)[:4000], flush=True)
    except Exception as e:
        print("failed:", e, flush=True)

    print(f"mem exists={mem.exists()} size={mem.stat().st_size if mem.exists() else 0}", flush=True)
    print(f"font exists={font.exists()} size={font.stat().st_size if font.exists() else 0}", flush=True)
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
    print("log:", log, flush=True)
