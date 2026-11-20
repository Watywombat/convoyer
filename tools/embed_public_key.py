"""Embed the team's public EC P-256 key in the firmware build."""
from pathlib import Path
import sys
pem = Path(sys.argv[1]).read_text(encoding="ascii")
if not pem.startswith("-----BEGIN PUBLIC KEY-----") or ')PEM"' in pem:
    sys.exit("A PEM public key is required")
Path("include/config_public_key.hpp").write_text(
    '#pragma once\nconstexpr char configPublicKey[]=R"PEM(' + pem + ')PEM";\n')
