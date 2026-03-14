Import("env")
import os
import shutil
import subprocess
from pathlib import Path


def _find_host_compiler():
    for name in ("gcc", "clang", "cc"):
        path = shutil.which(name)
        if path:
            return path
    return None


def run_generator(source, target, env):
    project_dir = Path(env.get("PROJECT_DIR"))
    src = project_dir / "tools" / "gen_athread_offsets.c"
    if not src.exists():
        raise RuntimeError("gen_athread_offsets.c not found")

    compiler = _find_host_compiler()
    if not compiler:
        raise RuntimeError("Host C compiler not found (need gcc/clang)")

    exe_name = "gen_athread_offsets.exe" if os.name == "nt" else "gen_athread_offsets"
    exe = project_dir / "tools" / exe_name

    compile_cmd = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-O2",
        "-I",
        str(project_dir / "include"),
        str(src),
        "-o",
        str(exe),
    ]

    subprocess.check_call(compile_cmd)
    subprocess.check_call([str(exe)], cwd=str(project_dir))


env.AddPreAction("buildprog", run_generator)
