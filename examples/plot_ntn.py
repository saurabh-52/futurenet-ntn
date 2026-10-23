#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Plot slant range, link delay and the associated satellite of one ground terminal.

Reads the link-state CSV written by NtnHelper::EnableTracing():
    time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated

Usage:
    python3 plot_ntn.py ntn-advanced-link-state.csv --ground 12 --output ntn-plot.png

Optional helper only: the C++ module never depends on it. Needs matplotlib.
"""

import argparse
import csv
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")  # write files, no display needed (works inside Docker)
import matplotlib.pyplot as plt


def load_rows(path):
    """Return the CSV rows as dicts with numeric fields converted."""
    with open(path, newline="") as handle:
        rows = []
        for row in csv.DictReader(handle):
            rows.append(
                {
                    "time_s": float(row["time_s"]),
                    "ground_id": int(row["ground_id"]),
                    "satellite_id": int(row["satellite_id"]),
                    "visible": row["visible"] == "1",
                    "range_km": float(row["range_km"]),
                    "delay_ms": float(row["delay_ms"]),
                    "associated": row["associated"] == "1",
                }
            )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="link-state CSV file")
    parser.add_argument("--ground", type=int, help="ground node ID (default: first in file)")
    parser.add_argument("--output", default="ntn-plot.png", help="output image file")
    args = parser.parse_args()

    rows = load_rows(args.csv)
    if not rows:
        sys.exit("CSV has no data rows")
    ground = args.ground if args.ground is not None else rows[0]["ground_id"]
    rows = [r for r in rows if r["ground_id"] == ground]
    if not rows:
        sys.exit(f"ground {ground} not found in {args.csv}")

    # Range of every satellite while it is visible (gaps where it is below MinElevation).
    range_series = defaultdict(lambda: ([], []))
    for r in rows:
        if r["visible"]:
            times, ranges = range_series[r["satellite_id"]]
            times.append(r["time_s"])
            ranges.append(r["range_km"])

    # Delay of the active link and the associated satellite (-1 = none) per sample time.
    by_time = defaultdict(lambda: (None, -1))
    for r in rows:
        if r["associated"]:
            by_time[r["time_s"]] = (r["delay_ms"], r["satellite_id"])
        else:
            by_time.setdefault(r["time_s"], (None, -1))
    times = sorted(by_time)
    delays = [by_time[t][0] for t in times]
    associations = [by_time[t][1] for t in times]

    fig, (ax_range, ax_delay, ax_assoc) = plt.subplots(3, 1, sharex=True, figsize=(9, 8))
    for sat, (t, rng) in sorted(range_series.items()):
        ax_range.plot(t, rng, ".", markersize=2, label=f"sat {sat}")
    ax_range.set_ylabel("slant range [km]")
    ax_range.set_title(f"Ground terminal {ground}: visible satellites")
    if len(range_series) <= 12:
        ax_range.legend(fontsize="small", ncol=4)

    ax_delay.plot(times, [d if d is not None else float("nan") for d in delays])
    ax_delay.set_ylabel("link delay [ms]")

    ax_assoc.step(times, associations, where="post")
    ax_assoc.set_ylabel("associated satellite\n(-1 = none)")
    ax_assoc.set_xlabel("time [s]")

    fig.tight_layout()
    fig.savefig(args.output, dpi=120)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
