#!/usr/bin/env python3
# Pre-build script: injects short git commit hash as GIT_COMMIT C define.
# Usage in code: #define FIRMWARE_VERSION "v1.2.3-" GIT_COMMIT
# C adjacent-string concatenation merges them → "v1.2.3-abc1234"

Import("env")
import subprocess

try:
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
    env.Append(CPPDEFINES=[("GIT_COMMIT", '\\"' + commit + '\\"')])
    print(f"[git_version] GIT_COMMIT = {commit}")
except Exception as e:
    print(f"[git_version] Could not get git commit: {e}")
