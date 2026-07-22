# Contributing

This guide walks you through setting up the development environment from scratch and explains the daily workflow for contributing to the project.

The project runs inside a Docker container based on [`marcopalena/polito-os161`](https://github.com/marcopalena/polito-os161-docker), a compact image pre-configured to compile, run, and debug OS/161.

> **Platform note:** all Docker commands below are identical on Windows, macOS, and Linux. On **Windows**, run them in PowerShell or Windows Terminal; on **macOS/Linux**, use your system terminal.

---

## First-time Setup

### 1. Create the Docker volume and container

```sh
docker volume create polito-os161-shell-vol
```

```sh
docker pull marcopalena/polito-os161:latest
```

```sh
docker run --volume polito-os161-shell-vol:/home/os161user --name polito-os161-shell -itd marcopalena/polito-os161 /bin/bash
```

Verify the container is running:

```sh
docker ps
```

You should see `polito-os161-shell` in the list with status `Up`.

---

### 2. Attach VS Code to the container

1. Open VS Code.
2. `Ctrl+Shift+P` → search and select **Dev Containers: Attach to Running Container...**
3. Select `/polito-os161-shell` from the list.
4. Wait for VS Code to install its server inside the container.
5. Once ready: **File → Open Folder** → type `/home/os161user/os161` → **OK**.
6. If prompted, confirm **Trust the authors**.

From this point on, everything (terminal, file editing) runs inside the Linux container.

---

### 3. Verify the environment

Open the integrated terminal (**Terminal → New Terminal** or `` Ctrl+` ``):

```bash
whoami   # expected: os161user
pwd      # expected: /home/os161user/os161
ls       # expected: src  tools  root
```

---

### 4. Install Git

```bash
sudo apt update && sudo apt install -y git
```

The sudo password for the container user is `os161`.

---

### 5. Configure your Git identity

```bash
git config --global user.name "First Last"
git config --global user.email "your@email.com"
```

Use the email associated with your GitHub account.

---

### 6. Set up SSH authentication for GitHub

Generate a key (press Enter at all prompts to accept the defaults):

```bash
ssh-keygen -t ed25519 -C "your@email.com"
```

Print the public key and copy the entire output:

```bash
cat ~/.ssh/id_ed25519.pub
```

Add it to GitHub (in your browser):
1. **github.com** → profile icon → **Settings**
2. **SSH and GPG keys** → **New SSH key**
3. Title: `os161-docker-container` (or any label you prefer)
4. Key: paste the copied output → **Add SSH key**

Test the connection:

```bash
ssh -T git@github.com
```

Type `yes` when asked to confirm the host fingerprint. A successful response looks like:

```
Hi <username>! You've successfully authenticated, but GitHub does not provide shell access.
```

> **Note:** to be able to `push`, the repository owner (`devgfe`) must first add you as a collaborator (**Repository Settings → Collaborators → Add people**). SSH authentication will work before that, but push attempts will be rejected.

---

### 7. Initialize the local repository

The container already pre-populates `src/`, `tools/`, and `root/`. The following commands overlay the repository contents on top, replacing `src/` with the project version while leaving `tools/` and `root/` untouched.

```bash
git init
git remote add origin git@github.com:devgfe/polito-os161-shell.git
git fetch origin
git checkout -f main
```

Verify the result:

```bash
git status        # expected: nothing to commit, working tree clean
git log --oneline -5
```

---

## Daily Workflow

If the container already exists, you do not need to repeat the setup — just:

1. Start the container if it is not running:
   ```sh
   docker start polito-os161-shell
   ```
2. In VS Code: **Dev Containers: Attach to Running Container** → `/polito-os161-shell`
3. In the integrated terminal:
   ```bash
   cd ~/os161
   git pull
   ```
4. Make your changes, then commit and push:
   ```bash
   git add .
   git commit -m "type: short description"
   git push
   ```