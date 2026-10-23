#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Configure, build, test, style-check and run all futurenet-ntn examples and validation cases.
# Run from the ns-3.46.1 root directory, e.g. inside the Docker container:
#   contrib/futurenet-ntn/tools/run-checks.sh
# Example outputs go to ./ntn-output/ ; validation results to contrib/futurenet-ntn/doc/validation/.
set -euo pipefail

MODULES="core,network,mobility,internet,point-to-point,applications,flow-monitor,futurenet-ntn"
OUT=ntn-output
VALIDATION=contrib/futurenet-ntn/doc/validation

step() { echo; echo "===== $* ====="; }

step "1/6 configure (re-reads the module Git commit for run-info files)"
./ns3 configure --enable-examples --enable-tests -G Ninja --enable-modules="${MODULES}" > /dev/null

step "2/6 build (warnings from futurenet-ntn are listed below; none expected)"
./ns3 build 2>&1 | { grep -E "futurenet-ntn.*(warning|error)" || true; }

step "3/6 test suite"
./test.py --no-build -s futurenet-ntn

step "4/6 clang-format style check"
python3 utils/check-style-clang-format.py contrib/futurenet-ntn | tail -3

step "5/6 examples"
mkdir -p "${OUT}"
./ns3 run --no-build --cwd="${OUT}" ntn-example-basic | tail -8
./ns3 run --no-build --cwd="${OUT}" "ntn-example-advanced --numGround=20 --numSats=12" | tail -4
./ns3 run --no-build --cwd="${OUT}" ntn-example-constellation | tail -5
./ns3 run --no-build --cwd="${OUT}" ntn-example-isl | tail -4

step "6/6 validation"
mkdir -p "${VALIDATION}"
./ns3 run --no-build "ntn-validation --outputDir=${VALIDATION}"

echo
echo "All checks finished. Example outputs are in ${OUT}/"
