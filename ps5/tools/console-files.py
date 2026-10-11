#!/usr/bin/env python3
# Wind Waker HD for the PlayStation 5 - files in the title's folder on the console, over FTP.
# SPDX-License-Identifier: MIT
"""Files of /data/homebrew/<TITLE_ID>/ on the console.

  console-files.py game [FOLDER]      upload the game's files (default ./game) to game/: what is
                                      already there with the same size is skipped
  console-files.py put LOCAL REMOTE   upload one file (REMOTE relative to the title's folder)
  console-files.py get REMOTE LOCAL   download one file
  console-files.py rm REMOTE          delete one file
  console-files.py ls [REMOTE]        list a folder

The game's files come from your own dump (tools/prepare-game.sh). Connection
settings are PS5_Vulkan's tools/ps5_console.py's (its ignored .env).
"""
import json
import os
import sys
from ftplib import FTP, error_perm, error_reply
from pathlib import Path
from posixpath import join

root = Path(__file__).resolve().parents[2]
vulkan = Path(os.environ.get("PS5_VULKAN_DIR", root.parent / "PS5_Vulkan"))
sys.path.insert(0, str(vulkan / "tools"))
import ps5_console  # noqa: E402

title = json.loads((root / "ps5/sce_sys/param.json").read_text())["titleId"]
base = f"/data/homebrew/{title}"


def connect():
    settings = ps5_console.load_settings()
    ftp = FTP()
    ftp.connect(settings["host"], settings["ftp_port"], timeout=60)
    ftp.login(settings["ftp_user"], settings["ftp_password"] or "codex")
    ftp.voidcmd("TYPE I")
    return ftp


def size(ftp, path):
    try:
        return ftp.size(path)
    except error_perm:
        return None


def mkdirs(ftp, path, made):
    current = ""
    for part in [p for p in path.split("/") if p]:
        current += "/" + part
        if current in made:
            continue
        try:
            ftp.mkd(current)
        except error_perm as error:
            if not str(error).startswith("550"):
                raise
        made.add(current)


def delete(ftp, path):
    # the console's ftpsrv completes a deletion with 226, which ftplib calls an error
    try:
        ftp.sendcmd(f"DELE {path}")
    except error_perm:
        pass
    except error_reply as reply:
        if not str(reply).startswith("2"):
            raise


def put(ftp, local, remote, made):
    folder, name = remote.rsplit("/", 1)
    mkdirs(ftp, folder, made)
    temporary = join(folder, "." + name + ".upload")
    with open(local, "rb") as source:
        ftp.storbinary(f"STOR {temporary}", source, blocksize=1 << 20)
    delete(ftp, remote)
    ftp.rename(temporary, remote)


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    command, args = args[0], args[1:]
    made = set()
    with connect() as ftp:
        if command == "game":
            folder = Path(args[0] if args else root / "game")
            if not (folder / "code/cking.rpx").is_file():
                raise SystemExit(f"{folder} is not an extracted game (no code/cking.rpx)")
            files = sorted(p for p in folder.rglob("*") if p.is_file())
            sent = skipped = 0
            total = sum(p.stat().st_size for p in files)
            done = 0
            for number, local in enumerate(files, 1):
                remote = join(base, "game", local.relative_to(folder).as_posix())
                if size(ftp, remote) == local.stat().st_size:
                    skipped += 1
                else:
                    put(ftp, local, remote, made)
                    sent += 1
                done += local.stat().st_size
                if number % 50 == 0 or number == len(files):
                    print(f"  {number}/{len(files)} files, {done / total:.0%}", flush=True)
            print(f"==> {title}/game: {sent} files sent, {skipped} already there")
        elif command == "put":
            put(ftp, args[0], join(base, args[1]), made)
        elif command == "get":
            with open(args[1], "wb") as out:
                ftp.retrbinary(f"RETR {join(base, args[0])}", out.write)
        elif command == "rm":
            delete(ftp, join(base, args[0]))
        elif command == "ls":
            ftp.retrlines(f"LIST {join(base, args[0]) if args else base}")
        else:
            raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
