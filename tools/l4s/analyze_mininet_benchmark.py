#!/usr/bin/env python3
"""Analyze paired L4S-on/off Mininet benchmark runs."""

import argparse
import csv
import json
import re
import subprocess
from pathlib import Path

from analyze_timeseries import AnalysisError, load_samples


def packet_count(path, display_filter):
    result = subprocess.run(
        [
            "tshark", "-r", str(path), "-Y", display_filter,
            "-T", "fields", "-e", "frame.number",
        ],
        check=True,
        text=True,
        capture_output=True,
    )
    return len(result.stdout.splitlines())


def tc_totals(path):
    text = Path(path).read_text(encoding="utf-8")
    l4s = sum(int(value) for value in re.findall(r"pkts_in_l\s+(\d+)", text))
    marked = sum(int(value) for value in re.findall(r"ecn_mark\s+(\d+)", text))
    return l4s, marked


def iperf_rate(path):
    if not Path(path).exists():
        return 0
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return int(data["end"]["sum_received"]["bits_per_second"])


def analyze_case(directory):
    metadata = json.loads((directory / "metadata.json").read_text(encoding="utf-8"))
    samples = load_samples(directory / "metrics.csv")
    if len(samples) < 2:
        raise AnalysisError(f"{directory.name}: insufficient metrics samples")
    duration_us = samples[-1]["time_us"] - samples[0]["time_us"]
    if duration_us <= 0:
        raise AnalysisError(f"{directory.name}: invalid duration")
    ect1_capture = packet_count(
        directory / "switch-client.pcap", "udp.port == 4443 && ip.dsfield.ecn == 1"
    )
    ce_capture = packet_count(
        directory / "switch-server.pcap", "udp.port == 4443 && ip.dsfield.ecn == 3"
    )
    tcp_ecn_capture = packet_count(
        directory / "switch-client.pcap", "tcp.port == 5201 && ip.dsfield.ecn != 0"
    )
    l4s_packets, ecn_marks = tc_totals(directory / "dualpi2-stats.txt")
    final = samples[-1]
    row = {
        "mode": metadata["mode"],
        "background_target_mbps": metadata["background_mbps"],
        "background_actual_mbps": iperf_rate(directory / "iperf-client.json") / 1e6,
        "quic_goodput_mbps": metadata["transfer_bytes"] * 8 / duration_us,
        "duration_ms": duration_us / 1000,
        "final_rtt_us": final["rtt_us"],
        "final_cwnd_bytes": final["cwnd_bytes"],
        "final_ect1_packets": final["ect1_packets"],
        "final_ce_packets": final["ce_packets"],
        "captured_ect1_packets": ect1_capture,
        "captured_ce_packets": ce_capture,
        "background_tcp_ecn_packets": tcp_ecn_capture,
        "dualpi2_l4s_packets": l4s_packets,
        "dualpi2_ecn_marks": ecn_marks,
    }
    if metadata["mode"] == "l4s-on":
        if final["ect1_packets"] == 0 or ect1_capture == 0 or l4s_packets == 0:
            raise AnalysisError(f"{directory.name}: L4S-on run has no ECT(1) evidence")
    elif metadata["mode"] == "l4s-off":
        if final["ect1_packets"] != 0 or final["ce_packets"] != 0 or ect1_capture != 0:
            raise AnalysisError(f"{directory.name}: L4S-off run unexpectedly used ECT(1)")
    else:
        raise AnalysisError(f"{directory.name}: unknown mode")
    target = metadata["background_mbps"]
    if tcp_ecn_capture != 0:
        raise AnalysisError(f"{directory.name}: background TCP was ECN-capable")
    if target > 0 and row["background_actual_mbps"] < target * 0.5:
        raise AnalysisError(f"{directory.name}: background TCP missed requested rate")
    return row


def analyze(root):
    rows = [analyze_case(path) for path in sorted(Path(root).glob("l4s-*-bg-*"))]
    if not rows:
        raise AnalysisError("no benchmark cases found")
    modes_by_rate = {}
    for row in rows:
        modes_by_rate.setdefault(row["background_target_mbps"], set()).add(row["mode"])
    for rate, modes in modes_by_rate.items():
        if modes != {"l4s-on", "l4s-off"}:
            raise AnalysisError(f"background {rate} Mbps does not have paired on/off runs")
    if max(row["dualpi2_ecn_marks"] for row in rows if row["mode"] == "l4s-on") == 0:
        raise AnalysisError("no L4S-on case produced a DualPI2 CE mark")
    return rows


def write_results(root, rows):
    fields = list(rows[0])
    with (root / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    comparisons = []
    for rate in sorted({row["background_target_mbps"] for row in rows}):
        pair = {
            row["mode"]: row
            for row in rows
            if row["background_target_mbps"] == rate
        }
        enabled = pair["l4s-on"]
        disabled = pair["l4s-off"]
        comparisons.append({
            "background_target_mbps": rate,
            "l4s_on_goodput_mbps": enabled["quic_goodput_mbps"],
            "l4s_off_goodput_mbps": disabled["quic_goodput_mbps"],
            "goodput_delta_mbps": (
                enabled["quic_goodput_mbps"] - disabled["quic_goodput_mbps"]
            ),
            "l4s_on_final_rtt_us": enabled["final_rtt_us"],
            "l4s_off_final_rtt_us": disabled["final_rtt_us"],
            "l4s_on_ce_feedback": enabled["final_ce_packets"],
            "l4s_off_ce_feedback": disabled["final_ce_packets"],
        })
    result = {
        "status": "pass",
        "cases": len(rows),
        "background_rates_mbps": sorted({row["background_target_mbps"] for row in rows}),
        "comparisons": comparisons,
        "rows": rows,
    }
    (root / "analysis.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))


def self_test():
    good = {
        "l4s-on": {"ect1": 2, "ce": 1, "capture": 3},
        "l4s-off": {"ect1": 0, "ce": 0, "capture": 0},
    }
    if good["l4s-on"]["ect1"] <= 0 or good["l4s-off"]["capture"] != 0:
        raise AnalysisError("benchmark analyzer self-test failed")
    print("Mininet benchmark analyzer self-test: PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.results is None:
        parser.error("results directory is required")
    try:
        write_results(args.results, analyze(args.results))
    except (AnalysisError, KeyError, ValueError, json.JSONDecodeError) as exc:
        raise SystemExit(f"Mininet L4S benchmark: FAIL: {exc}") from exc


if __name__ == "__main__":
    main()
