MOPS - The Multipurpose Operations CLI
======================================

Mops is a lightweight, high-performance C utility designed for MLOps engineers and DevOps professionals. It provides a unified interface for system diagnostics, background task management, and cloud infrastructure operations, bridging the gap between raw system metrics and operational context.

CORE MODULES
------------
*  sys        : Deep system diagnostics (CPU, Memory, GPU/TPU utilization, OOM events).
*  task       : SQLite-backed asynchronous task scheduling and management.
*  worker     : Daemonized background processing for long-running batch jobs.
*  gcp        : Google Cloud integration (Spot preemption watching, IAP tunneling).
*  net        : Network observability (Port monitoring, active connections).
*  dashboard  : Real-time ncurses terminal dashboard for fleet monitoring.

BUILDING FROM SOURCE
--------------------
Dependencies: gcc/clang, make, libsqlite3-dev, libncurses-dev.

    # 1. Install dependencies (Debian/Ubuntu)
    sudo apt-get update && sudo apt-get install -y build-essential libsqlite3-dev libncurses-dev

    # 2. Build the binary
    make

    # 3. Verify the environment
    ./mops doctor

INSTALLATION
------------
You can install mops system-wide via Makefile or by generating a Debian package.

    # Option A: System Install
    sudo make install

    # Option B: Debian Package
    make deb
    sudo dpkg -i mops_*.deb

DEVELOPER WORKFLOW
------------------
The project maintains strict quality standards through automated tooling.

    # Run functional tests (Python/pytest)
    make test

    # Run static analysis (cppcheck)
    make lint

    # Apply consistent styling (clang-format)
    make format

AUTHOR
------
Chaipat J.
*   GitHub: AppleBoiy
