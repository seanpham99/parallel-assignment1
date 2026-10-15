#!/usr/bin/env python3
"""plot.py — aggregate results_*.csv into PNGs for the report.

Usage: python3 plot.py [results_dir] [out_dir]
Falls back to printing a median table if matplotlib is missing.
"""
import csv
import glob
import os
import statistics
import sys
from collections import defaultdict

results_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "results")
out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "plots")
os.makedirs(out_dir, exist_ok=True)

rows = []
for f in glob.glob(os.path.join(results_dir, "results_*.csv")):
    with open(f) as fh:
        r = csv.DictReader(fh)
        for row in r:
            row["value"] = float(row["value"])
            row["threads"] = int(row["threads"])
            row["outside_work"] = int(row["outside_work"])
            rows.append(row)

if not rows:
    print("no results_*.csv found in", results_dir)
    sys.exit(0)

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    HAVE_MPL = True
except ImportError:
    HAVE_MPL = False
    print("matplotlib not installed — printing tables only")


def medians(task, metric, key="threads"):
    """{(platform, alg, outside): {threads: median(value)}}"""
    groups = defaultdict(lambda: defaultdict(list))
    for r in rows:
        if int(r["task"]) == task and r["metric"] == metric:
            k = (r["platform"], r["algorithm"], r["outside_work"])
            groups[k][r[key]].append(r["value"])
    return {k: {t: statistics.median(v) for t, v in d.items()}
            for k, d in groups.items()}


def plot_task1():
    data = medians(1, "wall_seconds")
    # One figure per outside_work, curves = algorithm per platform.
    figs = defaultdict(lambda: plt.figure(figsize=(7, 4.5)))
    for (plat, alg, ow), series in data.items():
        xs = sorted(series)
        ax = figs[ow].gca()
        ax.plot(xs, [series[x] for x in xs], marker="o",
                label=f"{alg}@{plat}")
    for ow, fig in figs.items():
        ax = fig.gca()
        ax.set_xlabel("threads"); ax.set_ylabel("wall time (s)")
        ax.set_title(f"Task 1 locks — outside_work={ow}")
        ax.legend(fontsize=7); fig.tight_layout()
        fig.savefig(os.path.join(out_dir, f"task1_ow{ow}.png"), dpi=140)
        plt.close(fig)


def plot_task2():
    data = medians(2, "episodes_per_sec")
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for (plat, alg, ow), series in data.items():
        xs = sorted(series)
        ax.plot(xs, [series[x] for x in xs], marker="o",
                label=f"{alg}@{plat}")
    ax.set_xlabel("threads"); ax.set_ylabel("episodes/s")
    ax.set_title("Task 2 barrier throughput")
    ax.legend(fontsize=7); fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "task2_barrier.png"), dpi=140)
    plt.close(fig)


def plot_task3():
    data = medians(3, "wall_seconds")
    figs = defaultdict(lambda: plt.figure(figsize=(7, 4.5)))
    for (plat, alg, ow), series in data.items():
        xs = sorted(series)
        figs[ow].gca().plot(xs, [series[x] for x in xs], marker="o",
                            label=f"{alg}@{plat}")
    for ow, fig in figs.items():
        ax = fig.gca()
        ax.set_xlabel("threads"); ax.set_ylabel("wall time (s)")
        ax.set_title(f"Task 3 counters — outside_work={ow}")
        ax.legend(fontsize=7); fig.tight_layout()
        fig.savefig(os.path.join(out_dir, f"task3_ow{ow}.png"), dpi=140)
        plt.close(fig)


def print_table(task, metric):
    data = medians(task, metric)
    for (plat, alg, ow), series in sorted(data.items()):
        pts = ", ".join(f"t{t}:{v:.3f}" for t, v in sorted(series.items()))
        print(f"task{task} {metric:16s} {alg:18s} ow={ow} {plat:18s} {pts}")


if HAVE_MPL:
    plot_task1(); plot_task2(); plot_task3()
    print("plots ->", out_dir)
else:
    for task, metric in [(1, "wall_seconds"), (2, "episodes_per_sec"),
                         (3, "wall_seconds"), (4, "wall_seconds")]:
        print_table(task, metric)
