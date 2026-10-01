#!/usr/bin/env python3
"""Render the three manuscript figures from the saved analyzer records.

Inputs are named explicitly: the 22-record estimate file (analysis/estimates_22.json,
a JSON list of records addressed by their "name" field) and the decision record
(analysis/decision.json). No value is embedded in this script; every plotted value is
read from those files and written, with input SHA-256 values, to FIGURE_DATA.json in
the output directory. Figure 1 is a deterministic schematic containing no outcome
values. Only the Python standard library and matplotlib are used; no producer,
analyzer or auditor code is imported.

Outputs: figure1_design, figure2_allele and figure3_accuracy, each as PNG, PDF and SVG,
plus FIGURE_DATA.json.
"""
import argparse
import hashlib
import json
from fractions import Fraction
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch  # noqa: E402

ARMS = ("ACTIVE", "SHAM")
LAWS = ("ZERO", "HALF")
STARTS = ("ALL_F", "ALL_M")
PRIMARY_ENRICHMENT = ("C_abs", "C_rec")
PRIMARY_GATE = ("D_HALF", "D_ZERO")
SECONDARY = ("P_abs", "P_rec")
DELTA = 1.0 / 32.0

INK = "#20262c"
MUTED = "#55636e"
GRID = "#dde4e8"
ARM_COLOR = {"ACTIVE": "#1f5f8b", "SHAM": "#7a8288"}
START_MARKER = {"ALL_F": ("o", True), "ALL_M": ("s", False)}

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 8.5, "axes.edgecolor": MUTED,
    "axes.labelcolor": INK, "xtick.color": MUTED, "ytick.color": MUTED,
    "axes.spines.top": False, "axes.spines.right": False, "svg.hashsalt": "phase2-mmem-figures",
    "savefig.dpi": 300,
})


def sha256_file(path):
    digest = hashlib.sha256()
    with open(str(path), "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def save(fig, output_dir, stem):
    written = []
    for ext, metadata in (("png", None), ("pdf", {"CreationDate": None, "ModDate": None}),
                          ("svg", {"Date": None})):
        path = output_dir / (stem + "." + ext)
        try:
            if metadata is None:
                fig.savefig(str(path), bbox_inches="tight", facecolor="white")
            else:
                fig.savefig(str(path), bbox_inches="tight", facecolor="white", metadata=metadata)
        except TypeError:
            fig.savefig(str(path), bbox_inches="tight", facecolor="white")
        written.append(path.name)
    plt.close(fig)
    return written


def load_records(estimates_path, decision_path):
    with open(str(estimates_path)) as handle:
        records = json.load(handle)
    with open(str(decision_path)) as handle:
        decision = json.load(handle)
    by_name = {rec["name"]: rec for rec in records}
    required = list(PRIMARY_ENRICHMENT + PRIMARY_GATE + SECONDARY)
    required += ["%s|%s|%s|%s" % (p, a, l, s) for p in ("M_FREQUENCY_LATE", "ACCURACY_LATE")
                 for a in ARMS for l in LAWS for s in STARTS]
    missing = [name for name in required if name not in by_name]
    if missing:
        raise SystemExit("missing estimate records: " + ", ".join(missing))
    return by_name, decision


def interval_record(rec):
    return {"name": rec["name"], "estimate": float(rec["estimate"]), "lower": float(rec["lower"]),
            "upper": float(rec["upper"]), "estimate_exact": rec["estimate_exact"],
            "half_width": float(rec["half_width"]),
            "classification": rec.get("classification", rec.get("family_decision"))}


def cell_values(by_name, prefix):
    out = []
    for arm in ARMS:
        for law in LAWS:
            for start in STARTS:
                rec = by_name["%s|%s|%s|%s" % (prefix, arm, law, start)]
                out.append({"arm": arm, "law": law, "start": start, "record": rec["name"],
                            "value": float(Fraction(rec["estimate_exact"])),
                            "estimate_exact": rec["estimate_exact"]})
    return out


# ---------------------------------------------------------------------------------------
# Figure 1: deterministic schematic
# ---------------------------------------------------------------------------------------
def box(ax, x, y, w, h, text, face="#f3f6f8", edge="#9fb0bc", size=7.6, weight="normal"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.008,rounding_size=0.015",
                                facecolor=face, edgecolor=edge, linewidth=0.8))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=size, color=INK,
            weight=weight, wrap=True)


def arrow(ax, start, end, style="-|>", dashed=False, rad=0.0):
    ax.add_patch(FancyArrowPatch(start, end, arrowstyle=style, mutation_scale=9, color=MUTED,
                                 linewidth=0.9, linestyle=(0, (3, 2)) if dashed else "solid",
                                 connectionstyle="arc3,rad=%g" % rad))


def panel_label(ax, letter, title):
    ax.text(0.0, 1.02, "(%s) %s" % (letter, title), transform=ax.transAxes, fontsize=9,
            weight="bold", color=INK, va="bottom")


def figure1(output_dir):
    # Explicit margins and gutters keep every heading and note inside its own panel and
    # away from the canvas edge; all multi-line text is broken by hand to fit one panel.
    fig = plt.figure(figsize=(7.4, 7.6))
    grid = fig.add_gridspec(2, 2, left=0.02, right=0.98, top=0.94, bottom=0.05, hspace=0.40, wspace=0.14)
    a, b = fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[0, 1])
    c, d = fig.add_subplot(grid[1, 0]), fig.add_subplot(grid[1, 1])
    for ax in (a, b, c, d):
        ax.set_xlim(0, 1)
        ax.set_ylim(0, 1)
        ax.axis("off")

    panel_label(a, "a", "Four candidates per parent per update")
    box(a, 0.02, 0.42, 0.22, 0.16, "Parent\ngenotype x", face="#e9eff4")
    labels = [
        "1  Parent: x",
        "2  Policy probe\nF: x XOR fresh mask\nvalid ACTIVE-M: cached genotype\nSHAM / invalid M: fresh",
        "3  Global scout: x XOR scout mask",
        "4  Local child: best of 1-3,\neach bit flipped w.p. 1/32",
    ]
    ys = [0.82, 0.50, 0.30, 0.06]
    hs = [0.12, 0.26, 0.12, 0.16]
    for text, y, h in zip(labels, ys, hs):
        face = "#dbe8f2" if text.startswith("2") else "#f3f6f8"
        box(a, 0.36, y, 0.62, h, text, face=face)
        arrow(a, (0.24, 0.50), (0.36, y + h / 2))
    a.text(0.02, -0.04, "Retrieval replaces one of two uniform\nfresh proposals; 128 objective queries\n"
           "per update in every cell.", fontsize=7.2, color=MUTED, va="top")

    panel_label(b, "b", "Population event order at update t")
    steps = [
        "Evaluate all 128 candidates\nHamming mismatch h to target T_t",
        "Draw 32 survivors without replacement\nexact integer weight 2^(32 - h)",
        "Inherit producing parent's policy\nflip M <-> F with probability 1/32",
        "Cache: M <- producing parent's genotype\nF <- invalid",
        "Record memory-use frequency\nand population accuracy",
    ]
    for k, text in enumerate(steps):
        y = 0.82 - k * 0.2
        box(b, 0.12, y, 0.76, 0.14, text, face="#e9eff4" if k == 1 else "#f3f6f8")
        if k:
            arrow(b, (0.5, y + 0.2), (0.5, y + 0.14))
    b.text(0.12, -0.04, "No label, cache or lineage identifier\nenters selection.", fontsize=7.2,
           color=MUTED, va="top")

    panel_label(c, "c", "Target laws (illustrative, not data)")
    zero = ["I1", "I2", "I3", "I4", "I5", "I6", "I7", "I8"]
    half = ["I1", "I2", "T1", "I4", "T3", "T4", "I7", "T6"]
    for row, (name, seq, y) in enumerate((("ZERO", zero, 0.70), ("HALF", half, 0.30))):
        c.text(0.0, y + 0.06, name, fontsize=8, weight="bold", color=INK, va="center")
        for k, token in enumerate(seq):
            x = 0.14 + k * 0.105
            recurrent = token.startswith("T")
            box(c, x, y, 0.085, 0.12, token, face="#dbe8f2" if recurrent else "#f3f6f8", size=7)
            if recurrent:
                source = int(token[1:]) - 1
                arrow(c, (0.14 + source * 0.105 + 0.042, y + 0.12),
                      (x + 0.042, y + 0.12), style="-|>", dashed=True, rad=-0.5)
        c.text(0.14, y - 0.06, "t = 1 ... 8", fontsize=6.8, color=MUTED, va="top")
    c.text(0.0, 0.17, "HALF: for t >= 3, T_t = T_(t-2) with probability\n"
           "1/2, else a fresh innovation; aligned with the\n"
           "one-generation cache delay by construction.",
           fontsize=7.2, color=MUTED, va="top")

    panel_label(d, "d", "Paired cells and inferential families")
    box(d, 0.02, 0.68, 0.96, 0.28,
        "Each block: 2 arms (ACTIVE, SHAM) x 2 laws (ZERO,\n"
        "HALF) x 2 starts (ALL_F, ALL_M) = 8 paired cells\n"
        "sharing all keyed random values. SHAM never reads\n"
        "labels or caches: exact neutral label benchmark.", size=7.2)
    box(d, 0.02, 0.36, 0.96, 0.26,
        "Primary family (alpha_each = 0.05/4): allele\n"
        "C_abs = A_HALF - 1/2;  C_rec = A_HALF - A_ZERO\n"
        "Start-state gate: D_HALF, D_ZERO\n"
        "inside (-1/32, +1/32)", face="#dbe8f2", size=7.2)
    box(d, 0.02, 0.04, 0.96, 0.26,
        "Secondary family (alpha_each = 0.05/2): accuracy\n"
        "P_abs (HALF: ACTIVE - SHAM); P_rec (interaction)\n"
        "Scale: one correct bit per survivor (1/32);\n"
        "cannot alter the primary decision", size=7.2)
    return save(fig, output_dir, "figure1_design")


# ---------------------------------------------------------------------------------------
# Figures 2 and 3: cell means and interval estimands
# ---------------------------------------------------------------------------------------
def cell_panel(ax, cells, ylabel, reference=None, reference_label=None):
    slots = [("ACTIVE", "ZERO"), ("ACTIVE", "HALF"), ("SHAM", "ZERO"), ("SHAM", "HALF")]
    for k, (arm, law) in enumerate(slots):
        for start, offset in (("ALL_F", -0.14), ("ALL_M", 0.14)):
            item = [c for c in cells if c["arm"] == arm and c["law"] == law and c["start"] == start][0]
            marker, filled = START_MARKER[start]
            ax.plot(k + offset, item["value"], marker=marker, markersize=7, linestyle="none",
                    markerfacecolor=ARM_COLOR[arm] if filled else "white",
                    markeredgecolor=ARM_COLOR[arm], markeredgewidth=1.4, zorder=3)
            ax.annotate("%.3f" % item["value"], (k + offset, item["value"]),
                        xytext=(-4 if start == "ALL_F" else 4, 7 if start == "ALL_F" else -7), textcoords="offset points",
                        ha="right" if start == "ALL_F" else "left", fontsize=6.6, color=INK)
    if reference is not None:
        ax.axhline(reference, color=MUTED, linestyle=(0, (4, 3)), linewidth=0.9, zorder=1)
        ax.text(3.45, reference, " " + reference_label, fontsize=6.8, color=MUTED, va="center")
    ax.set_xticks(range(4))
    ax.set_xticklabels(["ACTIVE\nZERO", "ACTIVE\nHALF", "SHAM\nZERO", "SHAM\nHALF"])
    ax.set_xlim(-0.6, 3.6)
    ax.set_ylabel(ylabel)
    ax.yaxis.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    handles = [plt.Line2D([], [], marker=START_MARKER[s][0], linestyle="none", markersize=6.5,
                          markerfacecolor=MUTED if START_MARKER[s][1] else "white",
                          markeredgecolor=MUTED, label="start " + s) for s in STARTS]
    ax.legend(handles=handles, frameon=False, fontsize=7, loc="upper right")


def interval_panel(ax, items, xlim, thresholds, xlabel):
    names = [item["name"] for item in items]
    for k, item in enumerate(items):
        y = len(items) - 1 - k
        ax.plot([item["lower"], item["upper"]], [y, y], color=INK, linewidth=2, solid_capstyle="round")
        ax.plot(item["estimate"], y, marker="o", markersize=7, color=INK,
                markeredgecolor="white", markeredgewidth=1.2, zorder=3)
        ax.annotate("%.6f\n[%.6f, %.6f]" % (item["estimate"], item["lower"], item["upper"]),
                    (item["estimate"], y), xytext=(0, 9), textcoords="offset points",
                    ha="center", va="bottom", fontsize=6.6, color=INK)
    ax.axvline(0, color=MUTED, linewidth=0.8)
    for value in thresholds:
        ax.axvline(value, color=MUTED, linestyle=(0, (1, 2)), linewidth=1.0)
    ax.set_yticks(range(len(items)))
    ax.set_yticklabels(list(reversed(names)))
    ax.set_ylim(-0.6, len(items) - 0.2)
    ax.set_xlim(*xlim)
    ax.set_xlabel(xlabel)
    ax.xaxis.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)


def figure2(output_dir, freq_cells, enrichment, gate, decision):
    fig = plt.figure(figsize=(7.4, 3.6))
    grid = fig.add_gridspec(2, 2, width_ratios=[1.15, 1], hspace=0.9, wspace=0.35)
    a = fig.add_subplot(grid[:, 0])
    b = fig.add_subplot(grid[0, 1])
    c = fig.add_subplot(grid[1, 1])
    cell_panel(a, freq_cells, "Late-window memory-use frequency", reference=0.5, reference_label="neutral 1/2")
    a.set_ylim(0, 1.0)
    panel_label(a, "a", "Cell means (descriptive)")
    high = max(item["upper"] for item in enrichment)
    interval_panel(b, enrichment, (-0.02, high + 0.08), [DELTA], "Estimate (proportion)")
    panel_label(b, "b", "Enrichment, 95% familywise")
    interval_panel(c, gate, (-0.045, 0.045), [-DELTA, DELTA], "Estimate (proportion); dotted: -1/32, +1/32")
    panel_label(c, "c", "Start-state gate")
    fig.text(0.5, -0.04, "Primary decision: " + decision["primary_decision"], ha="center",
             fontsize=7.4, color=MUTED)
    return save(fig, output_dir, "figure2_allele")


def figure3(output_dir, acc_cells, secondary, decision):
    fig = plt.figure(figsize=(7.4, 3.3))
    grid = fig.add_gridspec(1, 2, width_ratios=[1.15, 1], wspace=0.35)
    a, b = fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[0, 1])
    cell_panel(a, acc_cells, "Late-window population accuracy")
    values = [c["value"] for c in acc_cells]
    pad = 0.25 * (max(values) - min(values))
    a.set_ylim(min(values) - pad, max(values) + 1.6 * pad)
    panel_label(a, "a", "Cell means (descriptive)")
    low = min(item["lower"] for item in secondary)
    high = max(item["upper"] for item in secondary)
    # Short label; the caption explains the dotted one-bit thresholds.
    interval_panel(b, secondary, (min(low, -DELTA) - 0.01, max(high, DELTA) + 0.01), [-DELTA, DELTA],
                   "Accuracy contrast")
    panel_label(b, "b", "Performance, 95% familywise")
    labels = decision["secondary_classifications"]
    fig.text(0.5, -0.06, "P_abs: %s;  P_rec: %s" % (labels["P_abs"], labels["P_rec"]), ha="center",
             fontsize=7.4, color=MUTED)
    return save(fig, output_dir, "figure3_accuracy")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--estimates", type=Path, required=True, help="path to analysis/estimates_22.json")
    parser.add_argument("--decision", type=Path, required=True, help="path to analysis/decision.json")
    parser.add_argument("--output-dir", type=Path, required=True, help="directory for figures and FIGURE_DATA.json")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    by_name, decision = load_records(args.estimates, args.decision)
    freq_cells = cell_values(by_name, "M_FREQUENCY_LATE")
    acc_cells = cell_values(by_name, "ACCURACY_LATE")
    enrichment = [interval_record(by_name[n]) for n in PRIMARY_ENRICHMENT]
    gate = [interval_record(by_name[n]) for n in PRIMARY_GATE]
    secondary = [interval_record(by_name[n]) for n in SECONDARY]

    outputs = {
        "figure1_design": figure1(args.output_dir),
        "figure2_allele": figure2(args.output_dir, freq_cells, enrichment, gate, decision),
        "figure3_accuracy": figure3(args.output_dir, acc_cells, secondary, decision),
    }
    data = {
        "schema": "PHASE2-MMEM-FIGURE-DATA-1",
        "inputs": {
            "estimates": {"path": str(args.estimates), "sha256": sha256_file(args.estimates)},
            "decision": {"path": str(args.decision), "sha256": sha256_file(args.decision)},
        },
        "matplotlib_version": matplotlib.__version__,
        "outputs": outputs,
        "figure1_design": {"outcome_values": None, "note": "deterministic schematic; target pattern is illustrative"},
        "figure2_allele": {
            "cell_means_descriptive": freq_cells,
            "neutral_reference": 0.5,
            "enrichment_estimands": enrichment,
            "start_state_estimands": gate,
            "thresholds": {"Delta": DELTA},
            "primary_decision": decision["primary_decision"],
            "decision_rule_applied": decision["decision_rule_applied"],
        },
        "figure3_accuracy": {
            "cell_means_descriptive": acc_cells,
            "performance_estimands": secondary,
            "thresholds": {"Delta_P": DELTA},
            "secondary_classifications": decision["secondary_classifications"],
        },
    }
    out = args.output_dir / "FIGURE_DATA.json"
    out.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    for stem, files in outputs.items():
        for name in files:
            print(args.output_dir / name)
    print(out)


if __name__ == "__main__":
    main()
