#!/usr/bin/env python3
"""Analyze slicing scenario runs from run-scenario.sh.

Reads scripts-oai/results/<S>/{gnb.log,phases.csv,goodput.csv,nrMAC_stats.log,iperf_p*_ue*.log},
writes results/summary.md and plots/<S>_prbs.png, plots/<S>_goodput.png, plots/overview.png.
usage: python3 analyze.py [S1 S2 ...]     (default: every directory in results/)
"""
import csv
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
RESULTS = HERE / "results"
PLOTS = HERE / "plots"
N_RB = 106
SKIP = 5  # samples (~s) ignored at the start of each phase (iperf ramp-up, queue fill)
NO_SLICE = 0
COLORS = {1: "#2a78d6", 2: "#eb6834", NO_SLICE: "#1baf7a"}  # categorical slots 1-3
INK, MUTED, GRID = "#2b2b2a", "#6b6a63", "#e4e3dd"

ANSI = re.compile(r"\x1b\[[0-9;]*m")
RE_SLICE = re.compile(r"added slice (\d+): .*dedicated ([\d.]+)% min ([\d.]+)% max ([\d.]+)%")
RE_UE_SLICE = re.compile(r"UE ([0-9a-f]{4}): DL slice (\d+)")
RE_PRB = re.compile(r"DL slice (\d+) PRBs: (\d+) in (\d+) slots")
RE_PHASE = re.compile(r"### PHASE (\d+)")
RE_CUID = re.compile(r"UE RNTI ([0-9a-f]{4}) CU-UE-ID (\d+)")
RE_IPERF = re.compile(r"\]\s+([\d.]+)-([\d.]+)\s+sec\s+[\d.]+\s+\w?Bytes\s+([\d.]+)\s+([KMG]?)bits/sec")


def slice_name(s):
    return "no slice" if s == NO_SLICE else f"slice {s}"


def mean(v):
    return sum(v) / len(v) if v else float("nan")


def median(v):
    v = sorted(v)
    return (v[(len(v) - 1) // 2] + v[len(v) // 2]) / 2 if v else float("nan")


def parse_gnb_log(path):
    """slices {id: (ded, min, max)}, rnti -> slice, prb samples [(phase, {slice: prbs/slot})]"""
    slices, rnti_slice, samples = {}, {}, []
    phase, cur = 0, {}
    for line in path.read_text(errors="replace").splitlines():
        line = ANSI.sub("", line)
        if m := RE_PHASE.search(line):
            phase = int(m.group(1))
        elif "### END" in line:
            phase = -1
        elif m := RE_SLICE.search(line):
            slices[int(m.group(1))] = tuple(float(x) for x in m.group(2, 3, 4))
        elif m := RE_UE_SLICE.search(line):
            rnti_slice[m.group(1)] = int(m.group(2))
        elif m := RE_PRB.search(line):
            s, prbs, slots = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if s in cur:  # a new group of per-slice lines starts
                samples.append(cur_sample)
                cur = {}
            cur[s] = prbs / slots if slots else 0.0
            cur_sample = (phase, cur)
    if cur:
        samples.append(cur_sample)
    return slices, rnti_slice, samples


def expected_prbs(slices, active, meas=None):
    """Long-run PRB budget per slice (model of slicing.c). A slice whose measured use stays well
    below its budget is demand-limited: later slices then see what it actually used."""
    order = sorted(slices) + [NO_SLICE]
    pol = dict(slices)
    pol[NO_SLICE] = (0.0, 0.0, 100.0)
    reserve = {s: N_RB * (pol[s][1] if s in active else pol[s][0]) / 100 for s in order}
    reserved, rb_left, n_act, out = sum(reserve.values()), float(N_RB), len(active), {}
    for s in order:
        if s not in active:
            continue
        reserved -= reserve[s]
        budget = min(reserve[s] + (rb_left - reserve[s] - reserved) / n_act, N_RB * pol[s][2] / 100)
        n_act -= 1
        out[s] = max(budget, 0.0)
        used = meas[s] if meas and s in meas and meas[s] < 0.85 * out[s] else out[s]
        rb_left -= used + max(0.0, N_RB * pol[s][0] / 100 - used)
    return out


def parse_iperf(path):
    vals = []
    for line in path.read_text(errors="replace").splitlines():
        if m := RE_IPERF.search(line):
            t0, t1, rate, unit = float(m.group(1)), float(m.group(2)), float(m.group(3)), m.group(4)
            if t1 - t0 <= 1.5 and t0 >= 3:  # 1-s intervals only, skip ramp-up
                vals.append(rate * {"K": 1e-3, "M": 1, "G": 1e3, "": 1e-6}[unit])
    return mean(vals)


def analyze(sdir):
    S = sdir.name
    slices, rnti_slice, samples = parse_gnb_log(sdir / "gnb.log")
    with open(sdir / "phases.csv") as f:
        phases = [r for r in csv.DictReader(f)]
    cuid = {}
    if (sdir / "nrMAC_stats.log").exists():
        cuid = {r: int(c) for r, c in RE_CUID.findall((sdir / "nrMAC_stats.log").read_text())}
    ue_rnti = {c: r for r, c in cuid.items()}
    n_ue = len(phases[0]["loads"].split())
    ue_slice = {i: rnti_slice.get(ue_rnti.get(i, ""), NO_SLICE) for i in range(1, n_ue + 1)}

    goodput = []  # (epoch, rnti, dl)
    if (sdir / "goodput.csv").exists():
        with open(sdir / "goodput.csv") as f:
            goodput = [(int(r["epoch"]), r["rnti"], float(r["dl_mbps"])) for r in csv.DictReader(f)]

    rows = []
    for p in phases:
        n, start, dur = int(p["phase"]), int(p["start_epoch"]), int(p["duration"])
        loads = p["loads"].split()
        active = {ue_slice[i] for i in range(1, n_ue + 1) if loads[i - 1] != "0"}
        meas = {}
        ph = [smp for ph_, smp in samples if ph_ == n][SKIP:]
        for s in sorted(slices) + [NO_SLICE]:
            if any(s in smp for smp in ph):
                meas[s] = median([smp.get(s, 0.0) for smp in ph])  # robust to ramp-up/drain edges
        exp = expected_prbs(slices, active, meas)
        ues = []
        for i in range(1, n_ue + 1):
            gp = median([d for t, r, d in goodput if r == ue_rnti.get(i) and start + SKIP <= t < start + dur])
            ip = sdir / f"iperf_p{n}_ue{i}.log"
            ues.append({"ue": i, "slice": ue_slice[i], "load": loads[i - 1], "goodput": gp,
                        "iperf": parse_iperf(ip) if ip.exists() else float("nan")})
        rows.append({"phase": n, "loads": loads, "active": active, "exp": exp, "meas": meas, "ues": ues,
                     "n_samples": len(ph)})
    plot_prbs(S, slices, samples, rows)
    plot_goodput(S, goodput, phases, ue_rnti, ue_slice)
    return {"S": S, "slices": slices, "ue_slice": ue_slice, "rows": rows}


def style(ax):
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelcolor=INK)
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def plot_prbs(S, slices, samples, rows):
    xs = [k for k, (ph, _) in enumerate(samples) if ph > 0]
    if not xs:
        return
    x0 = xs[0]
    fig, ax = plt.subplots(figsize=(9, 4))
    style(ax)
    for s in sorted(slices) + [NO_SLICE]:
        pts = [(k - x0, smp[s]) for k, (ph, smp) in enumerate(samples) if ph > 0 and s in smp]
        if not pts or max(v for _, v in pts) == 0 and s == NO_SLICE:
            continue
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color=COLORS[s], lw=2, label=slice_name(s))
    for r in rows:
        ks = [k - x0 for k, (ph, _) in enumerate(samples) if ph == r["phase"]]
        if not ks:
            continue
        ax.axvline(ks[0], color=MUTED, lw=0.8, ls="--")
        ax.text(ks[0] + 0.5, 1.01, f"phase {r['phase']}", color=MUTED, fontsize=9, transform=ax.get_xaxis_transform())
        for s, e in r["exp"].items():
            ax.hlines(e, ks[0], ks[-1], color=COLORS[s], lw=1, ls=":")
    ax.set_ylim(0, N_RB)
    ax.set_xlabel("time (s of radio time, 1 sample = 100 frames)", color=INK)
    ax.set_ylabel("DL PRBs / slot", color=INK)
    ax.set_title(f"{S}: DL PRBs per slice (dotted = expected)", color=INK, loc="left", pad=18)
    ax.legend(frameon=False, loc="upper left", bbox_to_anchor=(1.01, 1))
    fig.tight_layout()
    fig.savefig(PLOTS / f"{S}_prbs.png", dpi=130)
    plt.close(fig)


def plot_goodput(S, goodput, phases, ue_rnti, ue_slice):
    if not goodput:
        return
    t0 = int(phases[0]["start_epoch"])
    fig, ax = plt.subplots(figsize=(9, 3.6))
    style(ax)
    styles = ["-", "--", ":"]
    seen = {}
    for i, rnti in sorted(ue_rnti.items()):
        pts = [(t - t0, d) for t, r, d in goodput if r == rnti and t >= t0]
        if not pts:
            continue
        s = ue_slice.get(i, NO_SLICE)
        ls = styles[seen.get(s, 0) % len(styles)]
        seen[s] = seen.get(s, 0) + 1
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color=COLORS[s], lw=2, ls=ls,
                label=f"UE{i} ({slice_name(s)})")
    for p in phases:
        x = int(p["start_epoch"]) - t0
        ax.axvline(x, color=MUTED, lw=0.8, ls="--")
        ax.text(x + 0.5, 1.01, f"phase {p['phase']}", color=MUTED, fontsize=9, transform=ax.get_xaxis_transform())
    ax.set_xlabel("time (s, wall clock)", color=INK)
    ax.set_ylabel("MAC DL goodput (Mbps)", color=INK)
    ax.set_title(f"{S}: MAC DL goodput per UE", color=INK, loc="left", pad=18)
    ax.legend(frameon=False, loc="upper left", bbox_to_anchor=(1.01, 1))
    fig.tight_layout()
    fig.savefig(PLOTS / f"{S}_goodput.png", dpi=130)
    plt.close(fig)


def plot_overview(results):
    labels, bars = [], []
    for res in results:
        for r in res["rows"]:
            # a demand-limited slice is drawn at what it used, not at its budget
            exp = {k: (r["meas"][k] if r["meas"].get(k, e) < 0.85 * e else e) for k, e in r["exp"].items()}
            for kind, d in (("expected", exp), ("measured", r["meas"])):
                labels.append(f"{res['S']} p{r['phase']} {kind}")
                bars.append(d)
    if not bars:
        return
    fig, ax = plt.subplots(figsize=(9, 0.32 * len(bars) + 1.2))
    style(ax)
    ax.grid(axis="x", color=GRID, linewidth=0.8)
    ax.grid(axis="y", visible=False)
    y = list(range(len(bars)))[::-1]
    drawn = set()
    for yi, d in zip(y, bars):
        left = 0.0
        for s in [1, 2, NO_SLICE]:
            v = d.get(s, 0.0)
            if v < 1:
                continue
            ax.barh(yi, v - 0.6, left=left, color=COLORS[s], height=0.7,
                    label=None if s in drawn else slice_name(s))
            drawn.add(s)
            if v >= 8:
                ax.text(left + v / 2, yi, f"{v:.0f}", color="white", ha="center", va="center", fontsize=8)
            left += v
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=8)
    ax.set_xlim(0, N_RB)
    ax.set_xlabel("DL PRBs / slot (106 = whole carrier)", color=INK)
    ax.set_title("Expected vs. measured DL PRBs per phase", color=INK, loc="left")
    ax.legend(frameon=False, ncol=3, loc="lower right", bbox_to_anchor=(1, 1.0))
    fig.tight_layout()
    fig.savefig(PLOTS / "overview.png", dpi=130)
    plt.close(fig)


def fmt(v, nd=1):
    return "–" if v != v else f"{v:.{nd}f}"


def write_summary(results):
    out = ["# Slicing scenario results", "",
           f"Per-phase medians (first {SKIP} samples skipped). Expected = PRB budget "
           "from a model of `slicing.c`; a slice using < 85 % of it is demand-limited (*).", ""]
    for res in results:
        pol = ", ".join(f"slice {s}: {d:g}/{m:g}/{x:g}" for s, (d, m, x) in sorted(res["slices"].items()))
        out += [f"## {res['S']}", "", f"Policies (dedicated/min/max %): {pol}  ",
                "UE → slice: " + ", ".join(f"UE{i}→{slice_name(s)}" for i, s in res["ue_slice"].items()), "",
                "| phase | offered load (UE1..) | slice | expected PRBs/slot | measured PRBs/slot | measured % |",
                "|---|---|---|---|---|---|"]
        for r in res["rows"]:
            for s in [x for x in sorted(res["slices"]) + [NO_SLICE] if x in r["exp"] or x in r["meas"]]:
                m, e = r["meas"].get(s, float("nan")), r["exp"].get(s, 0.0)
                if e == 0 and not m >= 0.5:
                    continue  # idle and unused
                star = " *" if m < 0.85 * e else ""
                out.append(f"| {r['phase']} | {' / '.join(r['loads'])} | {slice_name(s)} | "
                           f"{fmt(e)} | {fmt(m)}{star} | {fmt(100 * m / N_RB, 0)} |")
        out += ["", "| phase | UE | slice | offered | iperf Mbps | MAC goodput Mbps |", "|---|---|---|---|---|---|"]
        for r in res["rows"]:
            for u in r["ues"]:
                out.append(f"| {r['phase']} | UE{u['ue']} | {slice_name(u['slice'])} | {u['load']} | "
                           f"{fmt(u['iperf'])} | {fmt(u['goodput'])} |")
        out.append("")
    (RESULTS / "summary.md").write_text("\n".join(out))


def main():
    PLOTS.mkdir(exist_ok=True)
    names = sys.argv[1:] or sorted(d.name for d in RESULTS.iterdir() if (d / "gnb.log").exists())
    results = [analyze(RESULTS / n) for n in names]
    plot_overview(results)
    write_summary(results)
    print((RESULTS / "summary.md").read_text())


if __name__ == "__main__":
    main()
