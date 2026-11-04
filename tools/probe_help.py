import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

dbg = DosBoxDebugger(port=58991)
proc = launch(ROOT / "tools" / "dosbox-mcp.conf", ROOT / "tools" / "probe-help.log")
try:
    dbg.accept(timeout=60)
    print("PING:", dbg.ping(), flush=True)
    h = dbg.exec("HELP", timeout=30)
    print(h, flush=True)
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
