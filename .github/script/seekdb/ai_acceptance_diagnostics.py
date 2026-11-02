#!/usr/bin/env python3
"""Attach a bounded failure-log tail to the GitHub check's public annotations."""
import argparse
from pathlib import Path

LOG_NAMES = ("prerequisites.log", "rust.log", "init.log", "configure.log", "build.log", "core-tests.log", "sql-tests.log")


def escape_annotation(value):
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def last_failure_annotation(directory):
    files = [directory / name for name in LOG_NAMES]
    files = [path for path in files if path.is_file() and not path.is_symlink()]
    if not files:
        return "No build or test log was created; inspect the workflow setup step."
    path = max(files, key=lambda item: item.stat().st_mtime_ns)
    with path.open("rb") as stream:
        stream.seek(0, 2)
        size = stream.tell()
        stream.seek(max(0, size - 12000))
        tail = stream.read(12000).decode("utf-8", "replace")
    tail = "\n".join(tail.splitlines()[-60:])[-6000:]
    return path.name + "\n" + tail


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    message = last_failure_annotation(args.directory)
    print("::error title=AI acceptance failure::" + escape_annotation(message))


if __name__ == "__main__":
    main()
