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
log = work / "probe-vrt.log"

dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    print(dbg.exec("BPDEL *"), flush=True)
    print("RUN:", dbg.exec("RUN"), flush=True)
    time.sleep(6)

    print("VRT:", flush=True)
    try:
        print(dbg.exec("VRT", timeout=30), flush=True)
    except Exception as e:
        print("VRT ERR:", e, flush=True)

    for cmd in ["EV", "EV CS IP SS SP", "DOS MCBS"]:
        print(f"--- {cmd} ---", flush=True)
        try:
            print(dbg.exec(cmd, timeout=20), flush=True)
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
