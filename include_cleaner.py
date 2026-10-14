import os
import re
import subprocess
import sys
from pathlib import Path

# --- CONFIGURATION ---
BUILD_DIR = "./build"
TARGET_EXTENSIONS = (".cpp", ".c")

# Directories to entirely skip during the source tree scan
# Supports folder names (e.g., '3rdparty') or relative paths (e.g., './src/external')
IGNORE_DIRS = {
    "build", 
    ".git", 
    ".vscode", 
    "vcpkg", 
    "third_party", 
    "debug",
    "FreeRTOS-Kernel",
    "generated",
    "pico-sdk"
}
# ---------------------

INCLUDE_REGEX = re.compile(r"^(\s*)#\s*include\s+(<[^>]+>|\"[^\"]+\")")

def run_cmake_build(file_path: Path) -> bool:
    """Tells CMake to compile the single modified object file."""
    target_name = file_path.name + ".o"
    cmd = f"cmake --build {BUILD_DIR} --target {target_name}"
    
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode != 0:
            fallback_cmd = f"cmake --build {BUILD_DIR} -- -j"
            result = subprocess.run(fallback_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
        return result.returncode == 0
    except Exception:
        return False

def process_file(file_path: Path):
    print(f"\nAnalyzing: {file_path}")

    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    modified = False

    for i, line in enumerate(lines):
        match = INCLUDE_REGEX.match(line)
        if match:
            indent, include_target = match.groups()

            if line.strip().startswith("//"):
                continue

            print(f"  Testing removal of {include_target}...", end="", flush=True)

            original_line = lines[i]
            lines[i] = f"{indent}// {original_line.lstrip()}"

            with open(file_path, "w", encoding="utf-8") as f:
                f.writelines(lines)

            if run_cmake_build(file_path):
                print(" [UNUSED] -> Removed permanently.")
                modified = True
            else:
                print(" [REQUIRED] -> Kept.")
                lines[i] = original_line
                with open(file_path, "w", encoding="utf-8") as f:
                    f.writelines(lines)

    if not modified:
        print(f"  No changes made to {file_path.name}")

def main():
    build_path = Path(BUILD_DIR)
    if not (build_path / "CMakeCache.txt").exists():
        print(f"Error: Could not find CMakeCache.txt in '{BUILD_DIR}'.")
        print("Please configure your project first (e.g., 'cmake -B build').")
        sys.exit(1)

    print("Verifying base build system state...")
    initial_check = subprocess.run(f"cmake --build {BUILD_DIR}", shell=True, stdout=subprocess.DEVNULL)
    if initial_check.returncode != 0:
        print("Error: Initial project build failed. Fix existing errors before running.")
        sys.exit(1)

    # Convert configured ignore paths into absolute path strings for watertight filtering
    resolved_ignores = {Path(p).resolve() for p in IGNORE_DIRS}
    resolved_ignores.add(build_path.resolve())

    # Scan project source tree
    for root, dirs, files in os.walk("."):
        current_dir_path = Path(root).resolve()
        
        # 1. Prune directories in-place to prevent os.walk from entering skipped subtrees entirely
        dirs[:] = [
            d for d in dirs 
            if (current_dir_path / d).resolve() not in resolved_ignores 
            and d not in IGNORE_DIRS
        ]
        
        # 2. Process valid source files in the allowed directories
        for file in files:
            if file.endswith(TARGET_EXTENSIONS):
                process_file(Path(root) / file)

if __name__ == "__main__":
    main()
