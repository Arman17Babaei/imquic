# Mininet L4S coexistence benchmark

Run the benchmark from the host with:

```sh
make l4s-mininet-benchmark-check
```

The command boots an ephemeral overlay of `~/sandbox/p4/work.qcow2`, builds the
committed IMQUIC and picoquic revisions inside it, and creates this Mininet
topology:

```text
client --- s1 (Open vSwitch, HTB 20 Mbit/s + DualPI2) --- server
```

The same IMQUIC transfer runs with Prague/ECT(1) (`l4s-on`) and Reno/Not-ECT
(`l4s-off`). At the same time, iperf3 sends paced TCP from client to server with
TCP ECN disabled. The default requested TCP rates are 0, 5, 10, and 15 Mbit/s.
Each case recreates both switch-port qdiscs so counters do not leak between
runs. The analyzer separates QUIC (`udp.port == 4443`) from background TCP
(`tcp.port == 5201`) and fails if the classic flow carries ECN marks, if the
off run emits ECT(1), or if the on matrix produces no CE marking.

## Latest default run

Kernel: `5.15.72-48b3db6b4-prague-111`; Mininet: `2.3.1b4`; transfer: 4 MiB.

| TCP target (Mbit/s) | TCP actual off/on | QUIC off (Mbit/s) | QUIC on (Mbit/s) | Off/on final RTT (us) | Prague CE feedback |
|---:|---:|---:|---:|---:|---:|
| 0 | 0.00 / 0.00 | 16.80 | 18.10 | 3284 / 1983 | 1567 |
| 5 | 4.99 / 4.98 | 15.44 | 13.56 | 3642 / 828 | 1522 |
| 10 | 9.96 / 10.00 | 15.36 | 17.90 | 2271 / 1392 | 1951 |
| 15 | 13.93 / 13.96 | 15.27 | 13.43 | 2980 / 1512 | 1980 |

These are benchmark observations, not a claim that Prague always has higher
goodput. Across this run, classic TCP had zero ECN-marked packets, all Reno
runs had zero ECT(1)/CE feedback, and all Prague runs had ECT(1), DualPI2 CE
marks, and QUIC ACK ECN feedback.

Override the matrix without editing scripts:

```sh
make l4s-mininet-benchmark-check \
  L4S_BENCHMARK_RATES=2,8,14,18 \
  L4S_BENCHMARK_BYTES=8388608 \
  L4S_BENCHMARK_BOTTLENECK=20mbit \
  L4S_BENCHMARK_SECONDS=12
```

Full CSV trajectories, iperf JSON, qdisc counters, logs, and pcaps are copied
to `results/l4s/qemu-mininet-benchmark-<timestamp>/`, which is ignored by Git.
