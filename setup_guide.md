# Setup Guide — futurenet-ntn on a New Laptop

This guide takes a fresh laptop to a working build of the `futurenet-ntn` ns-3 module, with
all tests and examples running. Two routes are described:

* **Route A — Docker (recommended, works on Windows, macOS and Linux).** Everyone in the team
  gets exactly the same compiler, ns-3 version and tools.
* **Route B — Native Ubuntu 24.04** (a Linux laptop or a WSL Ubuntu install).

Time needed: about 30–45 minutes, most of it waiting for ns-3 to compile.

---

## 1. What you need

| Item | Why | Minimum |
|---|---|---|
| ~15 GB free disk | ns-3 source + build + Docker image | |
| 8 GB RAM (16 GB better) | compiling C++ | Docker must get ≥ 6 GB |
| Internet | downloads ns-3 (~ 60 MB) and packages | |
| Git | to get the module source | any recent version |
| Docker Desktop (Route A) | Linux build environment | 4.x |

ns-3.46.1 needs a C++20 compiler (GCC ≥ 11 or Clang ≥ 14) and CMake ≥ 3.13. Old Windows
toolchains such as MinGW GCC 6 **cannot** build it — that is why we use Docker or Ubuntu.

---

## 2. Get the module source

The team repository is on GitHub:

```bash
git clone https://github.com/saurabh-52/futurenet-ntn.git futurenet-ntn
cd futurenet-ntn
git checkout main
```

If you received a zip/copy of the folder instead, unzip it and open a terminal inside the
`futurenet-ntn` folder. Check you have the right thing:

```bash
git log --oneline -1          # shows the latest commit on main
ls                            # CMakeLists.txt  README.md  doc  examples  helper  model  test  tools ...
```

**Windows users:** stop Git converting line endings, otherwise shell scripts break inside
Linux (the repository's `.gitattributes` already forces LF, this is a second safety net):

```bash
git config --global core.autocrlf false
```

---

## 3. Route A — Docker (recommended)

### 3.1 Install and start Docker Desktop

1. Download Docker Desktop from <https://www.docker.com/products/docker-desktop/> and install
   it (on Windows accept the WSL 2 backend when asked; a reboot may be needed).
2. Start Docker Desktop and wait until it says **Engine running**.
3. Check in a terminal:

   ```bash
   docker version        # must show both Client and Server sections
   docker info --format "CPUs={{.NCPU}} MemBytes={{.MemTotal}}"
   ```

   If memory is below ~6 GB (6000000000 bytes), give Docker more:
   * Windows (WSL 2 backend): create `C:\Users\<you>\.wslconfig` containing
     ```ini
     [wsl2]
     memory=8GB
     ```
     then run `wsl --shutdown` in PowerShell and restart Docker Desktop.
   * macOS/Linux Docker Desktop: Settings → Resources → Memory.

### 3.2 Build the ns-3 image (one time, ~15–30 min)

From inside the `futurenet-ntn` folder:

```bash
docker build -t futurenet-ns3:3.46.1 tools/docker
```

What this does (see `tools/docker/Dockerfile`):

* Ubuntu 24.04 with g++ 13, CMake, Ninja, Python 3, matplotlib, Git, ccache, clang-tidy;
* clang-format **20** (the only version ns-3.46.1's style checker accepts);
* downloads `ns-allinone-3.46.1` from nsnam.org and builds only the modules this project needs
  (`core, network, mobility, internet, point-to-point, applications, flow-monitor`) with 4
  parallel jobs, so it fits in Docker's memory.

Expected end of the output: `naming to docker.io/library/futurenet-ns3:3.46.1 done`.

### 3.3 Start a development container

The container keeps ns-3 inside Linux (fast) and **mounts your module folder** into
`contrib/futurenet-ntn`, so you edit files on your laptop with any editor and compile inside
the container.

Linux / macOS / Git Bash (from inside the `futurenet-ntn` folder):

```bash
docker run -d --name ntn-dev \
  -v "$(pwd):/opt/ns-allinone-3.46.1/ns-3.46.1/contrib/futurenet-ntn" \
  -w /opt/ns-allinone-3.46.1/ns-3.46.1 \
  futurenet-ns3:3.46.1 sleep infinity
```

Windows PowerShell (adjust the path to where you cloned the module):

```powershell
docker run -d --name ntn-dev -v "D:\path\to\futurenet-ntn:/opt/ns-allinone-3.46.1/ns-3.46.1/contrib/futurenet-ntn" -w /opt/ns-allinone-3.46.1/ns-3.46.1 futurenet-ns3:3.46.1 sleep infinity
```

Open a shell inside it whenever you want to work:

```bash
docker exec -it ntn-dev bash
```

Everything below in section 5 is typed **inside that shell** (prompt `root@...:/opt/ns-allinone-3.46.1/ns-3.46.1#`).

Useful container commands (on the laptop, not inside):

```bash
docker stop ntn-dev      # stop (keeps the build)
docker start ntn-dev     # start again later
docker rm -f ntn-dev     # delete the container (the image and your source stay)
```

---

## 4. Route B — Native Ubuntu 24.04 (no Docker)

```bash
sudo apt update
sudo apt install -y g++ cmake ninja-build python3 python3-pip python3-matplotlib git ccache \
                    clang-tidy wget bzip2
pip3 install --user --break-system-packages "clang-format==20.1.8"   # style checker needs v20
export PATH="$HOME/.local/bin:$PATH"                                 # put it first in PATH

cd ~
wget https://www.nsnam.org/releases/ns-allinone-3.46.1.tar.bz2
tar xjf ns-allinone-3.46.1.tar.bz2
cd ns-allinone-3.46.1/ns-3.46.1
git clone <your-team-repo-url> contrib/futurenet-ntn      # or copy the folder there
```

Then continue with section 5 from the `ns-3.46.1` folder. (Do **not** build every ns-allinone
module with all cores on an 8 GB machine — the `--enable-modules` list below keeps it small.)

---

## 5. Configure, build, test, run

### 5.1 One command for everything

```bash
contrib/futurenet-ntn/tools/run-checks.sh
```

It configures, builds, runs the test suite, the clang-format check, all three examples and all
validation cases (about 30 s after the first build). Expected highlights:

```text
1 of 1 tests passed (1 passed, 0 skipped, 0 failed, 0 crashed, 0 valgrind errors)
- No files detected with bad code formatting
Total association changes: 68, total handovers: 48
Terminal association changes: 124, handovers: 104, satellite gateway changes: 32
V4 ns3::StickyAssociationPolicy: 299 handovers, mean elevation 24.91 deg, mean delay 5.479 ms
ALL VALIDATION CASES PASSED
All checks finished. Example outputs are in ntn-output/
```

### 5.2 The same, step by step

```bash
# configure (only the modules we need + ours)
./ns3 configure --enable-examples --enable-tests -G Ninja \
  --enable-modules=core,network,mobility,internet,point-to-point,applications,flow-monitor,futurenet-ntn

./ns3 build                                   # first time ~5 min, later only changed files

./test.py -s futurenet-ntn                    # automated tests
./build/utils/ns3.46.1-test-runner-default --test-name=futurenet-ntn --verbose   # one line per test case

mkdir -p demo
./ns3 run --cwd=demo ntn-example-basic                                     # one satellite pass
./ns3 run --cwd=demo "ntn-example-advanced --numGround=20 --numSats=12"    # 20 x 12 demo
./ns3 run --cwd=demo ntn-example-constellation                             # Phase 2: planes + gateways
./ns3 run --cwd=demo "ntn-example-constellation --policy=sticky --maxAssociations=2"
./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"

python3 utils/check-style-clang-format.py contrib/futurenet-ntn           # style check
python3 contrib/futurenet-ntn/examples/plot_ntn.py demo/ntn-advanced-link-state.csv --ground 12 --output demo/plot.png
```

Every example lists its options with `--PrintHelp`, e.g.
`./ns3 run "ntn-example-advanced --PrintHelp"`.

### 5.3 Getting output files onto your laptop (Docker route)

Files written inside `contrib/futurenet-ntn/` appear directly in your module folder on the
laptop (it is mounted). For anything else, copy it out:

```bash
docker cp ntn-dev:/opt/ns-allinone-3.46.1/ns-3.46.1/demo ./demo-output     # run on the laptop
```

Generated CSV/XML/PNG files are ignored by Git (except `doc/validation/`), so they will not be
committed by accident.

---

## 6. Daily workflow for team members

```bash
git checkout develop
git pull                                   # if a remote exists
git checkout -b feature/<what-you-do>      # e.g. feature/gateway
# ... edit code on the laptop ...
docker exec -it ntn-dev bash               # then inside:
./ns3 build && ./test.py -s futurenet-ntn
python3 utils/check-style-clang-format.py --fix contrib/futurenet-ntn
# back on the laptop:
git add <files> && git commit -m "Short summary of the change"
git checkout develop && git merge --no-ff feature/<what-you-do>
```

After `git commit`, re-run `./ns3 configure ...` (or `tools/run-checks.sh`) before producing
results you want to submit, so the run-info files record the new commit ID.

---

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `docker: ... dockerDesktopLinuxEngine ... cannot find the file` | Docker Desktop not running | Start Docker Desktop, wait for "Engine running". |
| Image build dies with `rpc error ... EOF` | Docker ran out of memory | Give Docker ≥ 6–8 GB (section 3.1) and retry; the Dockerfile already limits jobs (`BUILD_JOBS=4`, can be lowered: `docker build --build-arg BUILD_JOBS=2 ...`). |
| Docker Desktop shows "Remaining processes: com.docker.build.exe" | stuck build process after a crash | Task Manager → end `com.docker.build.exe`, click Quit, restart Docker Desktop. |
| `Could not find any supported version of clang-format ... [20-20]` | clang-format 18 from apt | `pip3 install --break-system-packages "clang-format==20.1.8"` and make sure `which clang-format` shows the pip one. |
| `./ns3 configure` builds only `core` | the module list got split by the shell (PowerShell + `;`) | Use the comma-separated list exactly as in 5.2, from a bash shell. |
| `bash\r: No such file` running `run-checks.sh` | Windows CRLF line endings | `git config core.autocrlf false`, then `git rm --cached -r . && git reset --hard` in the module folder. |
| `git ... detected dubious ownership` inside the container | mounted folder owned by another user | `git config --global --add safe.directory '*'` inside the container. |
| Examples not found by `./ns3 run` | configured without examples | Re-run configure with `--enable-examples --enable-tests`. |
| `Invalid value for attribute set (OrbitAltitude)` | value outside the project range | Expected behaviour (input validation); use 200–2000 km. |
| `... satellites cannot be divided equally between ... orbital planes` | `numSats` is not a multiple of `numPlanes` | Expected; e.g. 120 satellites with 2, 3, 4, 5, 6, 8, 10, 12, 15 or 20 planes. |
| `Gateways are disabled; ...` | `InstallGateway()` before `SetLinkAttribute("EnableGateway", BooleanValue(true))` | Enable gateways first. |
| Test program ends with `SIGSEGV` instead of `FAIL` | a test indexes a result list after a failed check (ns-3 keeps running after a failed assert) | Find the line: `apt-get install -y gdb` in the container, then `LD_LIBRARY_PATH=build/lib gdb -batch -ex run -ex bt --args build/utils/ns3.46.1-test-runner-default --suite=futurenet-ntn`. |
| `git push` asks for a password / is rejected | not logged in to GitHub, or no write access to the team repository | `gh auth login` (GitHub CLI) or a personal access token; ask the repository owner for access. |

---

## 8. Where to read next

* `README.md` — what the module does, API snippet, limitations.
* `doc/project-walkthrough.md` — the problem statement and how every part was built, with
  worked examples.
* `doc/demo-instructions.md` — what to show in a demo and the expected output.
* `doc/futurenet-ntn.rst` — full model documentation (formulas, parameters, traces).
