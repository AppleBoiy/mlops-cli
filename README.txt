mops - The Multipurpose Operations CLI
======================================

mops is a lightweight, dependency-free, high-performance C utility for MLOps engineers and DevOps professionals running on Linux. It provides instant, zero-latency feedback on system resources, container environments, and background task lifecycles, bridging the gap between low-level system metrics and high-level operational context.

Core Features
-------------

### System & Hardware (`mops sys`)
*   **Container/Cgroup Usage:** Instantly check memory usage and limits for the current Docker container (`mops sys cgroup`).
*   **OOM Killer Tracking:** Parse the kernel ring buffer for "Out of Memory" events (`mops sys oom`).
*   **GPU Process Mapping:** See exactly which PIDs are using GPU VRAM (`mops sys gpu --pids`).
*   **CPU & Disk Stats:** Get fast, human-readable CPU utilization and disk I/O metrics.

### Network (`mops net`)
*   **Port Investigator:** Find the exact PID and command bound to a local TCP port (`mops net port 8080`).

### Google Cloud Platform (`mops gcp`)
*   **Instant Metadata:** Get the current VM's Project ID, Zone, and Service Account in milliseconds (`mops gcp whoami`).
*   **Spot Instance Watcher:** Run a background daemon that executes a checkpoint script 30 seconds before a spot VM is preempted (`mops gcp spot-watch --exec "./save.sh"`).
*   **IAP Tunneling Shortcut:** Abstract away complex `gcloud` commands to forward local ports to remote instances (`mops gcp tunnel <instance> <remote:local>`).
*   **Secure Secret Injection:** Fetch secrets from GCP Secret Manager and inject them directly into a process's environment variables in memory (`mops gcp run-with-secrets --secret="API_KEY=my-secret" "..."`).

### Task Management (`mops task`, *dev mode only*)
*   **Daemonization:** Run any command as a background daemon, tracking its PID and status in a local SQLite database (`mops task bg "<cmd>"`).
*   **Webhook Notifications:** Trigger an HTTP POST request to a URL when a background task completes (`mops task bg "<cmd>" --notify <url>`).
*   **Log Tailing:** Instantly tail the stdout/stderr of any background task (`mops task logs <id> --tail`).
*   **Zombie Sweeper:** Scan the process tree for zombie processes and orphaned workers (`mops task clean`).


Build from Source
-----------------
You will need `gcc` (or `clang`), `make`, and `libsqlite3-dev`.

    # Install dependencies (on Debian/Ubuntu)
    sudo apt-get update && sudo apt-get install -y build-essential libsqlite3-dev

    # Build standard release
    make

    # Build with developer tools (enables 'mops task' module)
    make dev


Installation (Debian/Ubuntu)
----------------------------
Generate a `.deb` package and install it system-wide.

    # 1. Create the package
    make deb

    # 2. Install the package
    sudo apt install ./mops_1.0.0_amd64.deb

    # 3. Use the CLI and its man page
    mops --help
    man mops


Usage
-----
mops is designed to be self-documenting. Use `--help` on any command for a full list of subcommands and options.

    mops --help
    mops sys --help
    mops gcp --help


Author
------
Chaipat J.
*   GitHub: AppleBoiy
*   Email: contact.chaipat@gmail.com
