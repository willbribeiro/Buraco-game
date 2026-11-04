import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
log = ROOT / "tools" / "dosbox-mcp.log"
work = ROOT / "work"
work.mkdir(exist_ok=True)
mem = work / "mem.bin"
font = ROOT / "port" / "assets" / "font8x16.bin"

dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    print(dbg.exec("BPDEL *"), flush=True)
    print(dbg.exec("BPINT 10 00"), flush=True)
    print("RUN:", dbg.exec("RUN"), flush=True)

    # The breakpoint re-opens the debugger GUI; the control socket is dropped
    # and DOSBox-X connects again when it is stopped.
    time.sleep(2)
    dbg.drop()
    try:
        dbg.accept(timeout=45)
        print("RECONNECTED", flush=True)
    except socket.timeout:
        print("NO RECONNECT (debugger still running?)", flush=True)
        raise SystemExit(2)

    print("PING:", dbg.ping(), flush=True)
    for cmd in ["BPLIST", "CPU", "SR", "DOS MCBS"]:
        print(f"--- {cmd} ---", flush=True)
        try:
            print(dbg.exec(cmd, timeout=30), flush=True)
        except Exception as e:
            print("ERR:", e, flush=True)

    for cmd in [
        "MEMDUMPBIN 0:0 100000 " + str(mem),
        "VGA FONTDUMP " + str(font),
    ]:
        print(f"--- {cmd} ---", flush=True)
        try:
            print(dbg.exec(cmd, timeout=60), flush=True)
        except Exception as e:
            print("ERR:", e, flush=True)

    print(f"mem={mem.exists()} size={mem.stat().st_size if mem.exists() else 0}", flush=True)
    print(f"font={font.exists()} size={font.stat().st_size if font.exists() else 0}", flush=True)
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
    print("log:", log, flush=True)
