"""Regenerate and locally commit both derived branches from a clean main.

Preserves branch history, never force-pushes, and returns to main. Run tests
before publishing the three branches. Uncommitted changes are rejected.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

from export_platform import ROOT, export, git


def mutate(*args):
    subprocess.run(["git", "-c", f"safe.directory={ROOT.as_posix()}", *args],
                   cwd=ROOT, check=True)


def sync():
    if git("branch", "--show-current") != "main" or git("status", "--porcelain"):
        raise SystemExit("Sync requires a clean, committed main checkout")
    source_commit = git("rev-parse", "HEAD")
    branches = (("mobilgene", "prj_mobilgene_ccp"), ("pc", "prj_test_ccp"))
    for _, branch in branches:
        git("rev-parse", "--verify", f"refs/heads/{branch}")
    with tempfile.TemporaryDirectory(prefix="fsm_platforms_") as temporary:
        snapshots = {}
        # All parsing and file generation completes before switching branches.
        for platform, _ in branches:
            snapshots[platform] = Path(temporary) / platform
            export(platform, snapshots[platform])
        try:
            for platform, branch in branches:
                mutate("switch", branch)
                mutate("restore", "--source", source_commit, "--staged", "--worktree", "--", ".")
                snapshot = snapshots[platform]
                expected = {p.relative_to(snapshot).as_posix()
                            for p in snapshot.rglob("*") if p.is_file()}
                for name in git("ls-files", "-z").split("\0"):
                    if name and name not in expected:
                        target = (ROOT / name).resolve()
                        if ROOT not in target.parents:
                            raise ValueError(f"Path outside repository: {name}")
                        target.unlink()
                for name in sorted(expected):
                    target = ROOT / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(snapshot / name, target)
                mutate("add", "-A")
                staged = git("diff", "--cached", "--name-only")
                if staged:
                    mutate("commit", "-m", f"Sync {platform} snapshot from main {source_commit[:12]}")
        finally:
            mutate("switch", "main")


if __name__ == "__main__":
    sync()
