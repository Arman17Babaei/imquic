#!/usr/bin/env python3
"""Read-only preflight inspection for the IMQUIC L4S Prague skill."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

TOOLS = (
    "git",
    "cmake",
    "make",
    "autoconf",
    "automake",
    "pkg-config",
    "ip",
    "tc",
    "tcpdump",
    "tshark",
)


def _run(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [*args],
        cwd=repo,
        text=True,
        capture_output=True,
        check=False,
    )


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def is_imquic_repository(repo: Path) -> bool:
    readme = _read(repo / "README.md").lower()
    configure = _read(repo / "configure.ac").lower()
    quic_source = _read(repo / "src" / "quic.c").lower()
    build_marker = (repo / "Makefile.am").exists() or "ac_init([imquic]" in configure
    return (
        repo.is_dir()
        and "imquic" in readme
        and "picoquic" in (readme + quic_source)
        and build_marker
    )


def git_snapshot(repo: Path) -> dict[str, Any]:
    if not (repo / ".git").exists() and _run(repo, "git", "rev-parse", "--git-dir").returncode != 0:
        return {"available": False, "clean": False, "branch": None, "head": None, "status": []}
    branch = _run(repo, "git", "branch", "--show-current").stdout.strip() or None
    head = _run(repo, "git", "rev-parse", "HEAD").stdout.strip() or None
    status_lines = [line for line in _run(repo, "git", "status", "--porcelain").stdout.splitlines() if line]
    return {
        "available": True,
        "clean": not status_lines,
        "branch": branch,
        "head": head,
        "status": status_lines,
    }


def inspect(repo: Path) -> dict[str, Any]:
    linux = platform.system() == "Linux"
    tools = {tool: shutil.which(tool) for tool in TOOLS}
    return {
        "repository": str(repo.resolve()),
        "is_imquic": is_imquic_repository(repo),
        "git": git_snapshot(repo),
        "tools": tools,
        "capabilities": {
            "linux": linux,
            "root": hasattr(os, "geteuid") and os.geteuid() == 0,
            "network_namespaces": linux and bool(tools["ip"]),
            "traffic_control": linux and bool(tools["tc"]),
            "packet_capture": bool(tools["tcpdump"] or tools["tshark"]),
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", nargs="?", default=".", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    result = inspect(args.repository)
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"repository: {result['repository']}")
        print(f"imquic: {result['is_imquic']}")
        print(f"git clean: {result['git']['clean']}")
        print(f"linux root validation: {result['capabilities']['linux'] and result['capabilities']['root']}")
    return 0 if result["is_imquic"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
