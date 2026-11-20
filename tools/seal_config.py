"""Usage: python3 tools/seal_config.py config.json device.key REVISION config.enc"""
import os
from pathlib import Path
import sys
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

source, keyfile, revision, output = sys.argv[1:]
data = Path(source).read_bytes()
key = Path(keyfile).read_bytes()
version = int(revision).to_bytes(4, 'big')
if len(key) != 32 or not 0 < len(data) <= 1024 or int(revision) == 0:
    sys.exit("Clé de 32 octets, configuration de 1 à 1024 octets et révision positive requises")
nonce = os.urandom(12)
Path(output).write_bytes(version + nonce + AESGCM(key).encrypt(nonce, data, version))
