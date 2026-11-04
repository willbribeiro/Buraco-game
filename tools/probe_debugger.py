import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
log = ROOT / "tools" / "dosbox-mcp.log"

dbg = DosBoxDebugger(port=58991)
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    print(dbg.exec("BPDEL *"), flush=True)
    print(dbg.exec("BPINT 10 00"), flush=True)
    print("RUN:", dbg.exec("RUN"), flush=True)
    time.sleep(8)
    print("BREAK:", dbg.break_(timeout=15), flush=True)
    time.sleep(1)

    for cmd in ["CPU", "SR", "STACK"]:
        print(f"--- {cmd} ---", flush=True)
        try:
            print(dbg.exec(cmd, timeout=20), flush=True)
        except Exception as e:
            print("ERR:", e, flush=True)

    help_text = dbg.exec("HELP", timeout=30)
    print("--- HELP: register/step/run lines ---", flush=True)
    for line in help_text.splitlines():
        low = line.lower()
        if any(k in low for k in ["register", "step", "run", "trace", "breakpoint", "dump"]):
            print(line, flush=True)
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
