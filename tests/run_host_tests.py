import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests" / "build"


def run(cmd):
    print(" ".join(str(part) for part in cmd))
    subprocess.run(cmd, cwd=ROOT, check=True)


def build_and_run(cc, name, sources):
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / (name + (".exe" if os.name == "nt" else ""))
    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        "src",
        *sources,
        "-o",
        str(exe),
    ]
    run(cmd)
    run([str(exe)])


def main():
    cc = os.environ.get("CC") or shutil.which("gcc")
    if not cc:
        raise SystemExit("No C compiler found. Set CC or install gcc.")

    build_and_run(cc, "test_crypto", [
        "tests/test_crypto.c",
        "src/tuya_crc32.c",
    ])
    build_and_run(cc, "test_protocol", [
        "tests/test_protocol.c",
        "src/tuya_protocol.c",
        "src/tuya_crc32.c",
        "tests/host_crypto_stubs.c",
    ])


if __name__ == "__main__":
    main()
