import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from dosbox_mcp import DosBoxDebugger, launch, ROOT

conf = ROOT / "tools" / "dosbox-mcp.conf"
log = ROOT / "tools" / "dosbox-mcp.log"

dbg = DosBoxDebugger()
proc = launch(conf, log)
try:
    dbg.accept(timeout=60)
    print("PING:", repr(dbg.ping()))
    print("--- CPU ---")
    print(dbg.exec("CPU"))
    print("--- HELP (first lines) ---")
    print("\n".join(dbg.exec("HELP").splitlines()[:20]))
finally:
    dbg.close()
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()
    print("log:", log)
