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
log = work / "capture.log"


def reconnect(dbg: DosBoxDebugger, timeout: float = 90.0) -> str:
    end = time.time() + timeout
    while time.time() < end:
        if dbg.conn is not None:
            try:
                return dbg.ping(timeout=2)
            except Exception:
                dbg.drop()
        try:
            dbg.accept(timeout=2)
            return dbg.ping(timeout=3)
        except socket.timeout:
            continue
    raise TimeoutError("no debugger connection after break")


dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    print(dbg.exec("BPDEL *"), flush=True)
    print("RUN:", dbg.exec("RUN"), flush=True)
    time.sleep(5)
    print("VRT:", dbg.exec("VRT"), flush=True)
    time.sleep(1)
    print("RECONNECT:", reconnect(dbg), flush=True)

    for cmd in [
        "EV AX BX CX DX SI DI BP SP CS DS ES SS IP FLAGS",
        "DOS MCBS",
    ]:
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
