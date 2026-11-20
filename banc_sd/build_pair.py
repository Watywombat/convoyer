"""Build the candidate first, then bind the USB installer to that exact binary."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import os

root = Path(__file__).resolve().parent
idf = Path(os.environ["IDF_PATH"]) / "tools" / "idf.py"
profile = root / "main" / "build_profile.h"
out = root / "sd"
out.mkdir(exist_ok=True)

def build(label, header):
    profile.write_text(header, encoding="ascii")
    subprocess.run([sys.executable, str(idf), "-B", "build_" + label,
                    "-D", "IDF_TARGET=esp32", "build"], cwd=root, check=True)

build("B", "#define BENCH_IS_A 0\n")
candidate = root / "build_B" / "banc_sd.bin"
digest = hashlib.sha256(candidate.read_bytes()).digest()
build("A", "#define BENCH_IS_A 1\nstatic const unsigned char expected_digest[32]={" +
      ",".join(map(str, digest)) + "};\n")
shutil.copyfile(candidate, out / "firmware.bin")
print("SD firmware.bin SHA-256", digest.hex())
