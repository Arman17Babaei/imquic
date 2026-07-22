#!/usr/bin/env python3
"""Run paired IMQUIC L4S-on/off benchmarks in a one-switch Mininet topology."""

import argparse
import json
import os
import signal
import shutil
import subprocess
import sys
import time
from pathlib import Path

from mininet.link import TCLink
from mininet.net import Mininet
from mininet.node import OVSBridge


ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "src" / "imquic-l4s-test"
ANALYZER = ROOT / "tools" / "l4s" / "analyze_mininet_benchmark.py"


def command(args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)


def stop_process(process):
    if process is None or process.poll() is not None:
        return
    process.send_signal(signal.SIGINT)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def configure_dualpi2(switch, bottleneck):
    for interface in (f"{switch.name}-eth1", f"{switch.name}-eth2"):
        subprocess.run(
            ["tc", "qdisc", "del", "dev", interface, "root"],
            check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        command(["tc", "qdisc", "add", "dev", interface, "root", "handle", "1:",
                 "htb", "default", "1"])
        command(["tc", "class", "add", "dev", interface, "parent", "1:",
                 "classid", "1:1", "htb", "rate", bottleneck, "burst", "32k"])
        command(["tc", "qdisc", "add", "dev", interface, "parent", "1:1",
                 "handle", "10:", "dualpi2", "target", "1ms", "tupdate", "1ms"])


def run_case(client, server, switch, output, mode, background_mbps,
             transfer_bytes, background_seconds, bottleneck):
    case = output / f"{mode}-bg-{background_mbps:03d}mbps"
    case.mkdir(parents=True)
    configure_dualpi2(switch, bottleneck)
    metadata = {
        "mode": mode,
        "controller": "prague" if mode == "l4s-on" else "reno",
        "background_mbps": background_mbps,
        "background_transport": "TCP",
        "background_ecn": "disabled",
        "transfer_bytes": transfer_bytes,
        "bottleneck": bottleneck,
    }
    (case / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    captures = []
    server_process = None
    background_server = None
    background_client = None
    files = []
    try:
        for interface, name in ((f"{switch.name}-eth1", "switch-client.pcap"),
                                (f"{switch.name}-eth2", "switch-server.pcap")):
            log = (case / f"tcpdump-{interface}.log").open("w", encoding="utf-8")
            files.append(log)
            captures.append(subprocess.Popen(
                ["tcpdump", "-U", "-i", interface, "-w", str(case / name)],
                stdout=log, stderr=subprocess.STDOUT,
            ))

        server_log = (case / "server.log").open("w", encoding="utf-8")
        client_log = (case / "client.log").open("w", encoding="utf-8")
        files.extend((server_log, client_log))
        controller = metadata["controller"]
        server_process = server.popen(
            [str(BINARY), "--server", server.IP(), "4443", controller,
             str(transfer_bytes)],
            cwd=str(ROOT / "src"), stdout=server_log, stderr=subprocess.STDOUT,
        )
        time.sleep(0.5)

        if background_mbps > 0:
            iperf_server_log = (case / "iperf-server.json").open("w", encoding="utf-8")
            iperf_client_log = (case / "iperf-client.json").open("w", encoding="utf-8")
            files.extend((iperf_server_log, iperf_client_log))
            background_server = server.popen(
                ["iperf3", "-s", "-1", "-p", "5201", "--json"],
                stdout=iperf_server_log, stderr=subprocess.STDOUT,
            )
            time.sleep(0.3)
            background_client = client.popen(
                ["iperf3", "-c", server.IP(), "-p", "5201", "-t",
                 str(background_seconds), "-b", f"{background_mbps}M", "--json"],
                stdout=iperf_client_log, stderr=subprocess.STDOUT,
            )
            time.sleep(0.5)

        started = time.monotonic()
        client_process = client.popen(
            [str(BINARY), "--client", server.IP(), "4443", str(case / "metrics.csv"),
             controller, str(transfer_bytes)],
            cwd=str(ROOT / "src"), stdout=client_log, stderr=subprocess.STDOUT,
        )
        client_status = client_process.wait(timeout=180)
        server_status = server_process.wait(timeout=30)
        metadata["wall_duration_seconds"] = time.monotonic() - started
        metadata["client_status"] = client_status
        metadata["server_status"] = server_status
        (case / "metadata.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        if client_status != 0 or server_status != 0:
            raise RuntimeError(f"{case.name}: IMQUIC client/server failed")
        if background_client is not None and background_client.wait(timeout=30) != 0:
            raise RuntimeError(f"{case.name}: background TCP client failed")
        if background_server is not None and background_server.wait(timeout=10) != 0:
            raise RuntimeError(f"{case.name}: background TCP server failed")
    finally:
        for process in captures:
            stop_process(process)
        stop_process(background_client)
        stop_process(background_server)
        stop_process(server_process)
        for stream in files:
            stream.close()

    with (case / "dualpi2-stats.txt").open("w", encoding="utf-8") as stream:
        for interface in (f"{switch.name}-eth1", f"{switch.name}-eth2"):
            stream.write(f"device={interface}\n")
            result = command(
                ["tc", "-s", "qdisc", "show", "dev", interface],
                capture_output=True,
            )
            stream.write(result.stdout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--background-mbps", default="0,5,10,15")
    parser.add_argument("--bottleneck", default="20mbit")
    parser.add_argument("--transfer-bytes", type=int, default=4 * 1024 * 1024)
    parser.add_argument("--background-seconds", type=int, default=8)
    args = parser.parse_args()
    rates = [int(value) for value in args.background_mbps.split(",")]
    if not rates or any(rate < 0 for rate in rates):
        parser.error("background rates must be non-negative integers")
    if os.geteuid() != 0:
        raise SystemExit("Mininet benchmark must run as root inside QEMU")
    for program in ("iperf3", "mn", "ovs-vsctl", "tc", "tcpdump", "tshark"):
        if shutil.which(program) is None:
            raise SystemExit(f"missing guest benchmark command: {program}")
    for executable in (BINARY, ANALYZER):
        if not executable.exists():
            raise SystemExit(f"missing benchmark dependency: {executable}")
    command(["modprobe", "sch_dualpi2"])
    command(["service", "openvswitch-switch", "start"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    command(["mn", "-c"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    args.output.mkdir(parents=True, exist_ok=False)

    net = Mininet(controller=None, link=TCLink, switch=OVSBridge, autoSetMacs=True)
    client = net.addHost("client", ip="10.0.0.1/24")
    server = net.addHost("server", ip="10.0.0.2/24")
    switch = net.addSwitch("s1", failMode="standalone")
    net.addLink(client, switch)
    net.addLink(switch, server)
    try:
        net.start()
        client.cmd("sysctl -qw net.ipv4.tcp_ecn=0")
        server.cmd("sysctl -qw net.ipv4.tcp_ecn=0")
        if client.cmd(f"ping -c 1 -W 2 {server.IP()}").find("1 received") < 0:
            raise RuntimeError("Mininet client/server connectivity failed")
        for rate in rates:
            for mode in ("l4s-off", "l4s-on"):
                print(f"running {mode} with {rate} Mbps classic TCP background", flush=True)
                run_case(client, server, switch, args.output, mode, rate,
                         args.transfer_bytes, args.background_seconds, args.bottleneck)
    finally:
        net.stop()
        command(["mn", "-c"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    command([sys.executable, str(ANALYZER), str(args.output)])
    print(f"Mininet L4S benchmark: PASS ({args.output})")


if __name__ == "__main__":
    main()
