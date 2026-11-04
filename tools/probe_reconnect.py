import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
log = ROOT / "tools" / "probe-reconnect.log"

dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    print(dbg.exec("BPDEL *"), flush=True)
    print(dbg.exec("BP F000:FFF0"), flush=True)
    print("RUN:", dbg.exec("RUN"), flush=True)

    deadline = time.time() + 120
    got = False
    while time.time() < deadline:
        # 1) try the existing socket
        if dbg.conn is not None:
            try:
                p = dbg.ping(timeout=2)
                print("OLD SOCKET PING:", p, flush=True)
                got = True
                break
            except Exception as e:
                print("old socket dead:", type(e).__name__, flush=True)
                dbg.drop()
        # 2) try a new connection
        try:
            dbg.accept(timeout=3)
            print("NEW CONNECTION", flush=True)
            print("PING:", dbg.ping(timeout=3), flush=True)
            got = True
            break
        except socket.timeout:
            pass
    print("GOT:", got, flush=True)
    if got:
        for cmd in ["BPLIST", "CPU", "SR"]:
            print(f"--- {cmd} ---", flush=True)
            try:
                print(dbg.exec(cmd, timeout=20), flush=True)
            except Exception as e:
                print("ERR:", e, flush=True)
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
