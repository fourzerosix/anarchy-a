"""anarchy_s -- Draw the Circle-A anarchy symbol in your terminal."""
__version__ = "1.0.0"
__all__ = ["main", "run"]
import os, sys, subprocess, shutil, tempfile
from pathlib import Path

def _pkg():  return Path(__file__).parent
def _bin():  return _pkg() / "_anarchy_s_bin"
def _src():  return _pkg() / "anarchy-s.c"

def _compile():
    binary = _bin()
    if binary.exists(): return binary
    source = _src()
    if not source.exists():
        raise FileNotFoundError(f"anarchy-s: source not found at {source}")
    cc = next((c for c in ("gcc","cc","clang") if shutil.which(c)), None)
    if not cc:
        raise RuntimeError("anarchy-s: no C compiler found (need gcc/cc/clang)")
    print(f"anarchy-s: compiling with {cc}...", file=sys.stderr)
    tmp = Path(tempfile.mktemp(suffix="_anarchy_s_bin", dir=_pkg()))
    try:
        r = subprocess.run([cc,"-O2","-std=gnu99","-o",str(tmp),str(source),"-lm"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"anarchy-s: compilation failed:\n{r.stderr}")
        tmp.chmod(0o755); tmp.rename(binary)
        print("anarchy-s: compiled.", file=sys.stderr)
    except Exception:
        if tmp.exists(): tmp.unlink()
        raise
    return binary

def run(args=None):
    if args is None: args = sys.argv[1:]
    os.execv(str(_compile()), ["anarchy-s"] + list(args))

def main():
    try: run()
    except KeyboardInterrupt: sys.exit(0)
    except Exception as e:
        print(f"anarchy-s error: {e}", file=sys.stderr); sys.exit(1)
