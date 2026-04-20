#!/usr/bin/env python3
# Pre-build script: injects short git commit hash as GIT_COMMIT C define,
# and current build date as FIRMWARE_BUILD_DATE C define.

Import("env")
import subprocess
from datetime import datetime

try:
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
    env.Append(CPPDEFINES=[("GIT_COMMIT", '\\"' + commit + '\\"')])
    print(f"[git_version] GIT_COMMIT = {commit}")
except Exception as e:
    print(f"[git_version] Could not get git commit: {e}")

date_str = datetime.now().strftime("%d %b %Y")
env.Append(CPPDEFINES=[("FIRMWARE_BUILD_DATE", '\\"' + date_str + '\\"')])
print(f"[git_version] FIRMWARE_BUILD_DATE = {date_str}")
