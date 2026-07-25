import subprocess
import datetime
import os

Import("env")

def get_firmware_specifier_build_flag():
    ret = subprocess.run(
        [
            "git",
            "describe",
            "--tags",
            "--dirty",
            "--exclude",
            "nightly",
            "--exclude",
            "db",
            "--exclude",
            "hardware-scales-latest",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )  # Uses release tags, excluding rolling build tags.
    build_version = ret.stdout.strip()
    if not build_version:
        sha_result = subprocess.run(
            ["git", "rev-parse", "--short=8", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        short_sha = sha_result.stdout.strip() or "unknown"
        run_number = os.environ.get("GITHUB_RUN_NUMBER", "0")
        build_version = f"v0.0.0-hwscales.{run_number}+g{short_sha}"
    build_flag = "#define BUILD_GIT_VERSION \"" + build_version + "\""
    print ("Build version: " + build_version)
    return build_flag

def get_time_specifier_build_flag():
    build_timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    build_flag = "#define BUILD_TIMESTAMP \"" + build_timestamp + "\""
    print ("Build date: " + build_timestamp)
    return build_flag

with open('src/version.h', 'w') as f:
    f.write(
        '#pragma once\n' +
        '#ifndef GIT_VERSION_H\n' +
        '#define GIT_VERSION_H\n' +
        get_firmware_specifier_build_flag() + '\n' +
        get_time_specifier_build_flag() + '\n'
        '#endif\n'
    )
