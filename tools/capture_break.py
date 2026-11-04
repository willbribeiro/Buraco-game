import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
work = ROOT / "work"
work.mkdir(exist_ok=True)
mem = work / "mem.bin"
font = ROOT / "port" / "assets" / "font8x16.bin"
log = work / "capture-break.log"

dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log, debug=False)
try:
    try:
        dbg.accept(timeout=20)
        print("CONNECTED (no -debug)", flush=True)
        print("PING:", dbg.ping(), flush=True)
    except socket.timeout:
        print("NO CONNECTION without -debug", flush=True)
        raise SystemExit(2)

    print("waiting for the game to boot...", flush=True)
    time.sleep(8)

    print("BREAK:", flush=True)
    try:
        print(dbg.break_(timeout=20), flush=True)
    except Exception as e:
        print("BREAK ERR:", e, flush=True)

    for cmd in [
        "EV AX BX CX DX SI DI BP SP CS DS ES SS IP FLAGS",
        "DOS MCBS",
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
