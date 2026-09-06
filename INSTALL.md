# Install & Run

This guide is for people who want to **download and run** the project without modifying or pushing code.

The environment setup is the one described in [`CONTRIBUTING.md`](CONTRIBUTING.md). The table below tells you which parts of that guide you need and which you can skip. If you later decide to contribute, just follow `CONTRIBUTING.md` in full.

| Step in [`CONTRIBUTING.md`](CONTRIBUTING.md) | Needed to just run it? |
|------|------|
| [1. Create the Docker volume and container](CONTRIBUTING.md#1-create-the-docker-volume-and-container) | ✅ |
| [2. Attach VS Code to the container](CONTRIBUTING.md#2-attach-vs-code-to-the-container) | ✅ |
| [3. Verify the environment](CONTRIBUTING.md#3-verify-the-environment) | ✅ |
| [4. Install Git](CONTRIBUTING.md#4-install-git) | ✅ (git is used to overlay the code, not to push) |
| [5. Install Python and pexpect](CONTRIBUTING.md#5-install-python-and-pexpect) | ✅ optional — only for the automated tests |
| [6. Configure your Git identity](CONTRIBUTING.md#6-configure-your-git-identity) | ❌ skip |
| [7. Set up SSH authentication](CONTRIBUTING.md#7-set-up-ssh-authentication-for-github) | ❌ skip |
| [8. Initialize the local repository](CONTRIBUTING.md#8-initialize-the-local-repository) | ✅ but use the HTTPS URL below |

## The one change: HTTPS instead of SSH

Since you skip the SSH setup (step 7), add the remote with the **HTTPS** URL instead of the SSH one shown in step 8:

```bash
git remote add origin https://github.com/devgfe/polito-os161-shell.git
```

The rest of step 8 (`git fetch origin`, `git checkout -f main`) is unchanged.

## Build and run

From the VS Code menu, use **Terminal → Run Task…** and choose:

1. **Full Kernel Build** — configures, compiles, and installs the kernel. When prompted for the **kernel version name**, type `SHELL`.
2. **Build Tests** — builds and installs the userland test programs.
3. **Run OS161 (no debug)** — boots the installed kernel with `sys161 kernel`.

Alternatively, if you prefer to run the automated test suites, use the **Run Shell Tests** or **Run Extra Tests** tasks (they need Python and pexpect, see step 5 in the table above).

To run it again later: `docker start polito-os161-shell`, re-attach VS Code (step 2 in the table above), then re-run the tasks — **Full Kernel Build** only if the code changed, followed by **Run OS161 (no debug)** (or **Run Shell Tests** / **Run Extra Tests** if you prefer the automated suites).