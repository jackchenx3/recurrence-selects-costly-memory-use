#!/usr/bin/env python3
"""Render the five manuscript figures from the saved analyzer records of the three studies.

Inputs are named explicitly. Study 001 (PHASE2-MUTABLE-MEMORY-001): the 22-record
estimate file (analysis/estimates_22.json, a JSON list of records addressed by their
"name" field) and the decision record (analysis/decision.json). Study 002
(PHASE2-PERFORMANCE-CONVERSION-002): the 19-record estimate file
(results/estimates_19.json) and its decision record (results/decision.json). Study 003
(PHASE2-TORUS-MEMORY-003): the 22-record estimate file (estimates.json, an object whose
"records" list is addressed by "name") and its decision record (decisions.json). No value
is embedded in this script; every plotted value is read from those files and written, with
input SHA-256 values, to FIGURE_DATA.json in the output directory. Each input must match
the SHA-256 authenticated in its study's acceptance record unless --skip-sha-check is
given, in which case the mismatch is recorded in FIGURE_DATA.json. Figure 1 is a
deterministic schematic containing no outcome values. Only the Python standard library
and matplotlib are used; no producer, analyzer or auditor code is imported.

Outputs: figure1_design, figure2_allele and figure3_accuracy (Study 001),
figure4_information (Study 002) and figure5_operator_bundles (Study 001 and Study 003
side by side; two operator bundles and independent cohorts, never pooled), each as PNG,
PDF and SVG, plus FIGURE_DATA.json.
"""
import argparse
import hashlib
import json
from decimal import Decimal
from fractions import Fraction
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Patch  # noqa: E402

ARMS = ("ACTIVE", "SHAM")
LAWS = ("ZERO", "HALF")
STARTS = ("ALL_F", "ALL_M")
PRIMARY_ENRICHMENT = ("C_abs", "C_rec")
PRIMARY_GATE = ("D_HALF", "D_ZERO")
SECONDARY = ("P_abs", "P_rec")
DELTA = 1.0 / 32.0

# Study 002 record names (PHASE2-PERFORMANCE-CONVERSION-002).
S2_ARMS = ("INFO", "NONINFO", "SHAM")
S2_PRIMARY = ("Delta_P", "D_INFO", "D_NONINFO")
S2_ALLELE = ("E_INFO", "E_NONINFO")
S2_PERFORMANCE = ("B_INFO", "B_NONINFO")
S2_SAVED_ESTIMATES = 19
S2_LABELS = {
    "Delta_P": "Delta_P\n(INFO - NONINFO accuracy)",
    "D_INFO": "D_INFO\n(INFO start contrast)",
    "D_NONINFO": "D_NONINFO\n(NONINFO start contrast)",
    "E_INFO": "E_INFO\n(INFO - NONINFO frequency)",
    "E_NONINFO": "E_NONINFO\n(NONINFO frequency - 1/2)",
    "B_INFO": "B_INFO\n(INFO - SHAM accuracy)",
    "B_NONINFO": "B_NONINFO\n(NONINFO - SHAM accuracy)",
}

# Study 003 record names (PHASE2-TORUS-MEMORY-003). Figure 5 compares the Study 001 and
# Study 003 primary allele and secondary performance estimands, which share names and the
# declared 1/32 scales but come from different operator bundles and independent cohorts.
S3_ESTIMATES_SCHEMA = "PHASE2-TORUS-MEMORY-003-ESTIMATES-v1"
S3_DECISIONS_SCHEMA = "PHASE2-TORUS-MEMORY-003-DECISIONS-v1"
S3_SAVED_ESTIMATES = 22
S3_PRIMARY = ("C_abs", "C_rec", "D_HALF", "D_ZERO")
S3_PERFORMANCE = ("P_abs", "P_rec")
STUDY_LABEL = {"001": "Study 001", "003": "Study 003"}
STUDY_COLOR = {"001": "#1f5f8b", "003": "#a4502a"}

# SHA-256 values authenticated in PHASE2_STUDY_001_ACCEPTED.json (authenticated_outputs),
# PHASE2_STUDY_002_ACCEPTED.json (analysis) and PHASE2_STUDY_003_ACCEPTED.json (analysis).
AUTHENTICATED_SHA256 = {
    "estimates": "83828a49936bb8364813e2634f11cb28c4de2212b4b162d96e616ddbc7b4f8c0",
    "decision": "5b3aea3c18c4530b2be9f29d5c64fdd8173b6b323823bf9cc3361bd4e7010fc0",
    "s2_estimates": "5efa213cdfeb985a20fd7bbc65fb36dd505c568670ad350b3223c2139eb0b68d",
    "s2_decision": "3f28b82d0cb40357a1068c464d1c5158c36ba8fbfe775026ce22c70113b3b84a",
    "s3_estimates": "65a31d9f8c73459aa29c3bc4388ed22a156952a18d71388440bffb38f4c073c4",
    "s3_decisions": "84f88bb17c20593abd222b8e099e2981e725b28968c650dbf306f43441892f09",
}
INPUT_STUDY = {"s2_": "PHASE2-PERFORMANCE-CONVERSION-002", "s3_": "PHASE2-TORUS-MEMORY-003"}

INK = "#20262c"
MUTED = "#55636e"
GRID = "#dde4e8"
BAND = "#e4eef5"
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


def load_s2_records(estimates_path, decision_path):
    with open(str(estimates_path)) as handle:
        records = json.load(handle)
    with open(str(decision_path)) as handle:
        decision = json.load(handle)
    if not isinstance(records, list) or len(records) != S2_SAVED_ESTIMATES:
        raise SystemExit("Study 002 estimate file must be a list of 19 records")
    by_name = {rec["name"]: rec for rec in records}
    required = list(S2_PRIMARY + S2_ALLELE + S2_PERFORMANCE)
    required += ["%s|%s|%s" % (p, a, s) for p in ("M_FREQUENCY_LATE", "ACCURACY_LATE")
                 for a in S2_ARMS for s in STARTS]
    missing = [name for name in required if name not in by_name]
    if missing:
        raise SystemExit("missing Study 002 estimate records: " + ", ".join(missing))
    return by_name, decision


def load_s3_records(estimates_path, decisions_path):
    with open(str(estimates_path)) as handle:
        data = json.load(handle)
    with open(str(decisions_path)) as handle:
        decisions = json.load(handle)
    if data.get("schema") != S3_ESTIMATES_SCHEMA or decisions.get("schema") != S3_DECISIONS_SCHEMA:
        raise SystemExit("Study 003 estimate or decision schema differs from the accepted schema")
    records = data["records"]
    if not isinstance(records, list) or len(records) != S3_SAVED_ESTIMATES:
        raise SystemExit("Study 003 estimate file must contain 22 records")
    if data.get("namespace") != decisions.get("namespace"):
        raise SystemExit("Study 003 estimate and decision namespaces differ")
    by_name = {rec["name"]: rec for rec in records}
    missing = [name for name in S3_PRIMARY + S3_PERFORMANCE if name not in by_name]
    if missing:
        raise SystemExit("missing Study 003 estimate records: " + ", ".join(missing))
    return by_name, decisions


def interval_record(rec):
    return {"name": rec["name"], "estimate": float(rec["estimate"]), "lower": float(rec["lower"]),
            "upper": float(rec["upper"]), "estimate_exact": rec["estimate_exact"],
            "half_width": float(rec["half_width"]),
            "classification": rec.get("classification", rec.get("family_decision"))}


def s3_interval_record(rec, decisions):
    # Study 003 records carry an exact rational value and high-precision display strings.
    display = rec["display"]
    item = {"name": rec["name"], "estimate": float(Fraction(rec["exact_value"])),
            "lower": float(Decimal(display["lower"])), "upper": float(Decimal(display["upper"])),
            "estimate_exact": rec["exact_value"], "estimate_display": display["estimate"],
            "lower_display": display["lower"], "upper_display": display["upper"],
            "half_width": float(Decimal(display["half_width"])), "family": rec["family"],
            "classification_code": rec["classification_code"]}
    if rec["name"] in decisions.get("performance_classes", {}):
        item["classification"] = decisions["performance_classes"][rec["name"]]
    return item


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


def interval_panel(ax, items, xlim, thresholds, xlabel, labels=None, band=None, colors=None):
    # colors is used only by figure 5; figures 2-4 keep the default single ink colour.
    names = labels if labels is not None else [item["name"] for item in items]
    if band is not None:
        # Shaded meaningful-scale band (-band, +band) with labelled edges.
        ax.axvspan(-band, band, facecolor=BAND, edgecolor="none", zorder=0)
        for value, text in ((-band, "-1/32"), (band, "+1/32")):
            ax.text(value, 0.02, text, transform=ax.get_xaxis_transform(), ha="center", va="bottom",
                    fontsize=6.6, color=MUTED, bbox={"facecolor": "white", "edgecolor": "none", "pad": 0.6})
    for k, item in enumerate(items):
        y = len(items) - 1 - k
        color = colors[k] if colors is not None else INK
        ax.plot([item["lower"], item["upper"]], [y, y], color=color, linewidth=2, solid_capstyle="round")
        ax.plot(item["estimate"], y, marker="o", markersize=7, color=color,
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


# ---------------------------------------------------------------------------------------
# Figure 4: Study 002 directional-information estimands
# ---------------------------------------------------------------------------------------
def figure4(output_dir, primary, allele, performance, decision):
    # Explicit margins leave room below panel c for the legend and the decision line.
    fig = plt.figure(figsize=(7.4, 7.4))
    grid = fig.add_gridspec(3, 1, height_ratios=[3, 2, 2], hspace=0.75, left=0.27, right=0.97,
                            top=0.95, bottom=0.17)
    a, b, c = (fig.add_subplot(grid[k, 0]) for k in range(3))
    near = (-0.048, 0.048)
    interval_panel(a, primary, near, [-DELTA, DELTA], "Estimate (proportion)",
                   labels=[S2_LABELS[item["name"]] for item in primary], band=DELTA)
    panel_label(a, "a", "Primary family: directional-information effect and start-state gate")
    high = max(item["upper"] for item in allele)
    interval_panel(b, allele, (near[0], high + 0.05), [-DELTA, DELTA], "Allele-frequency contrast (proportion)",
                   labels=[S2_LABELS[item["name"]] for item in allele], band=DELTA)
    panel_label(b, "b", "Secondary allele family")
    interval_panel(c, performance, near, [-DELTA, DELTA], "Accuracy contrast (proportion)",
                   labels=[S2_LABELS[item["name"]] for item in performance], band=DELTA)
    panel_label(c, "c", "Secondary no-memory performance family")
    handles = [Patch(facecolor=BAND, edgecolor="none",
                     label="meaningful scale (-1/32, +1/32): one correct bit or one expected individual"),
               plt.Line2D([], [], color=INK, linewidth=2, marker="o", markersize=6,
                          markeredgecolor="white", label="estimate, 95% familywise interval")]
    fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, 0.085), ncol=1, frameon=False,
               fontsize=7.2)
    inside = decision.get("bounded_interval_wholly_inside_minus_delta_plus_delta")
    fig.text(0.5, 0.0, "Primary decision (rule %s): %s; interval wholly inside (-1/32, +1/32): %s"
             % (decision["decision_rule_applied"], decision["primary_decision"], "yes" if inside else "no"),
             ha="center", fontsize=7.4, color=MUTED)
    return save(fig, output_dir, "figure4_information")


# ---------------------------------------------------------------------------------------
# Figure 5: Study 001 and Study 003 estimands at the declared 1/32 scales
# ---------------------------------------------------------------------------------------
def paired_rows(names, s1_items, s3_items):
    # One row per (estimand, study): Study 001 first, then Study 003. Never pooled.
    s1 = {item["name"]: item for item in s1_items}
    s3 = {item["name"]: item for item in s3_items}
    rows, labels, colors = [], [], []
    for name in names:
        for study, source in (("001", s1), ("003", s3)):
            rows.append(dict(source[name], study=study))
            labels.append("%s\n%s" % (name, STUDY_LABEL[study]))
            colors.append(STUDY_COLOR[study])
    return rows, labels, colors


def figure5(output_dir, s1_primary, s1_performance, s3_primary, s3_performance, decision, s3_decisions):
    # Explicit margins leave room below panel b for the legend and two decision lines.
    fig = plt.figure(figsize=(7.4, 8.2))
    grid = fig.add_gridspec(2, 1, height_ratios=[8, 4], hspace=0.42, left=0.22, right=0.97,
                            top=0.95, bottom=0.21)
    a, b = fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[1, 0])
    rows_a, labels_a, colors_a = paired_rows(S3_PRIMARY, s1_primary, s3_primary)
    high = max(item["upper"] for item in rows_a)
    interval_panel(a, rows_a, (-0.05, high + 0.08), [-DELTA, DELTA], "Allele-frequency estimand (proportion)",
                   labels=labels_a, band=DELTA, colors=colors_a)
    panel_label(a, "a", "Primary allele family: enrichment and start-state gate")
    rows_b, labels_b, colors_b = paired_rows(S3_PERFORMANCE, s1_performance, s3_performance)
    low = min(item["lower"] for item in rows_b)
    high = max(item["upper"] for item in rows_b)
    interval_panel(b, rows_b, (min(low, -DELTA) - 0.01, max(high, DELTA) + 0.01), [-DELTA, DELTA],
                   "Performance contrast (fraction of each study's maximal loss)",
                   labels=labels_b, band=DELTA, colors=colors_b)
    panel_label(b, "b", "Secondary performance family")
    handles = [Patch(facecolor=STUDY_COLOR["001"], edgecolor="none",
                     label="Study 001: 32-bit genotypes, Hamming mismatch, weighted sampling without replacement"),
               Patch(facecolor=STUDY_COLOR["003"], edgecolor="none",
                     label="Study 003: 2^16-allele torus, graded circular loss, four-entry tournaments; "
                           "new code and cohort"),
               Patch(facecolor=BAND, edgecolor="none",
                     label="declared scale (-1/32, +1/32); each study has its own 95% familywise families")]
    fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, 0.125), ncol=1, frameon=False,
               fontsize=6.9)
    fig.text(0.5, 0.022, "Study 001: %s; P_abs %s; P_rec %s"
             % (decision["primary_decision"], decision["secondary_classifications"]["P_abs"],
                decision["secondary_classifications"]["P_rec"]),
             ha="center", fontsize=6.8, color=MUTED)
    fig.text(0.5, 0.0, "Study 003: %s; P_abs %s; P_rec %s"
             % (s3_decisions["primary_decision"], s3_decisions["performance_classes"]["P_abs"],
                s3_decisions["performance_classes"]["P_rec"]),
             ha="center", fontsize=6.8, color=MUTED)
    return save(fig, output_dir, "figure5_operator_bundles"), rows_a, rows_b


def authenticate_inputs(paths, skip):
    hashes = {key: sha256_file(path) for key, path in paths.items()}
    mismatched = sorted(key for key in hashes if hashes[key] != AUTHENTICATED_SHA256[key])
    if mismatched and not skip:
        raise SystemExit("input SHA-256 differs from the authenticated value for: " + ", ".join(mismatched)
                         + " (use --skip-sha-check only for non-release test builds)")
    return hashes, mismatched


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--estimates", type=Path, required=True, help="Study 001 analysis/estimates_22.json")
    parser.add_argument("--decision", type=Path, required=True, help="Study 001 analysis/decision.json")
    parser.add_argument("--s2-estimates", type=Path, required=True, help="Study 002 results/estimates_19.json")
    parser.add_argument("--s2-decision", type=Path, required=True, help="Study 002 results/decision.json")
    parser.add_argument("--s3-estimates", type=Path, required=True, help="Study 003 estimates.json (22 records)")
    parser.add_argument("--s3-decisions", type=Path, required=True, help="Study 003 decisions.json")
    parser.add_argument("--output-dir", type=Path, required=True, help="directory for figures and FIGURE_DATA.json")
    parser.add_argument("--skip-sha-check", action="store_true",
                        help="allow inputs whose SHA-256 differs from the authenticated values (recorded)")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    input_paths = {"estimates": args.estimates, "decision": args.decision,
                   "s2_estimates": args.s2_estimates, "s2_decision": args.s2_decision,
                   "s3_estimates": args.s3_estimates, "s3_decisions": args.s3_decisions}
    input_hashes, mismatched = authenticate_inputs(input_paths, args.skip_sha_check)

    by_name, decision = load_records(args.estimates, args.decision)
    s2_by_name, s2_decision = load_s2_records(args.s2_estimates, args.s2_decision)
    s3_by_name, s3_decisions = load_s3_records(args.s3_estimates, args.s3_decisions)
    s3_primary = [s3_interval_record(s3_by_name[n], s3_decisions) for n in S3_PRIMARY]
    s3_performance = [s3_interval_record(s3_by_name[n], s3_decisions) for n in S3_PERFORMANCE]
    s2_primary = [interval_record(s2_by_name[n]) for n in S2_PRIMARY]
    s2_allele = [interval_record(s2_by_name[n]) for n in S2_ALLELE]
    s2_performance = [interval_record(s2_by_name[n]) for n in S2_PERFORMANCE]
    freq_cells = cell_values(by_name, "M_FREQUENCY_LATE")
    acc_cells = cell_values(by_name, "ACCURACY_LATE")
    enrichment = [interval_record(by_name[n]) for n in PRIMARY_ENRICHMENT]
    gate = [interval_record(by_name[n]) for n in PRIMARY_GATE]
    secondary = [interval_record(by_name[n]) for n in SECONDARY]

    figure5_files, figure5_rows_a, figure5_rows_b = figure5(
        args.output_dir, enrichment + gate, secondary, s3_primary, s3_performance, decision, s3_decisions)
    outputs = {
        "figure1_design": figure1(args.output_dir),
        "figure2_allele": figure2(args.output_dir, freq_cells, enrichment, gate, decision),
        "figure3_accuracy": figure3(args.output_dir, acc_cells, secondary, decision),
        "figure4_information": figure4(args.output_dir, s2_primary, s2_allele, s2_performance, s2_decision),
        "figure5_operator_bundles": figure5_files,
    }
    data = {
        "schema": "PHASE2-MMEM-FIGURE-DATA-3",
        "package_version": "1.2.0",
        "inputs": {
            key: {"path": str(path), "sha256": input_hashes[key],
                  "authenticated_sha256": AUTHENTICATED_SHA256[key],
                  "matches_authenticated": key not in mismatched,
                  "study": INPUT_STUDY.get(key[:3], "PHASE2-MUTABLE-MEMORY-001")}
            for key, path in sorted(input_paths.items())
        },
        "sha_check_enforced": not args.skip_sha_check,
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
        "figure4_information": {
            "study": "PHASE2-PERFORMANCE-CONVERSION-002",
            "panel_a_primary_estimands": s2_primary,
            "panel_b_allele_estimands": s2_allele,
            "panel_c_performance_estimands": s2_performance,
            "meaningful_band": [-DELTA, DELTA],
            "thresholds": {"delta": DELTA},
            "primary_decision": s2_decision["primary_decision"],
            "decision_rule_applied": s2_decision["decision_rule_applied"],
            "bounded_interval_wholly_inside_minus_delta_plus_delta":
                s2_decision.get("bounded_interval_wholly_inside_minus_delta_plus_delta"),
            "secondary_classifications": s2_decision["secondary_classifications"],
        },
        "figure5_operator_bundles": {
            "studies": {"001": "PHASE2-MUTABLE-MEMORY-001", "003": "PHASE2-TORUS-MEMORY-003"},
            "note": ("Different operator bundles and independent cohorts; each study's familywise families are "
                     "controlled separately; no estimate is pooled and effect magnitudes are not compared across "
                     "bundles. The allele scale 1/32 is one expected individual in both studies; the performance "
                     "scale 1/32 is one correct bit per survivor in Study 001 and 1/32 of maximal total torus loss "
                     "in Study 003."),
            "panel_a_primary_allele_estimands": figure5_rows_a,
            "panel_b_performance_estimands": figure5_rows_b,
            "meaningful_band": [-DELTA, DELTA],
            "thresholds": {"Delta": DELTA, "Delta_P": DELTA},
            "study_001_primary_decision": decision["primary_decision"],
            "study_001_secondary_classifications": decision["secondary_classifications"],
            "study_003_primary_decision": s3_decisions["primary_decision"],
            "study_003_performance_classes": s3_decisions["performance_classes"],
            "study_003_crossed_interpretation": s3_decisions["crossed_interpretation"],
            "study_003_identity_failure_count": s3_decisions["identity_failure_count"],
            "study_003_design_sha256": s3_decisions["design_sha256"],
            "study_003_production_manifest_sha256": s3_decisions["production_manifest_sha256"],
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
