# L4S Validation

## Evidence levels

Report the highest completed level; never collapse them into a single “works” claim.

1. **Structural:** Prague selected, normalized options recorded, code paths covered by tests.
2. **Build:** picoquic and IMQUIC build and their relevant tests pass.
3. **Marking:** outbound packets use ECT(1), and a controlled bottleneck emits CE marks.
4. **Feedback:** picoquic observes increasing ECT(1) and CE counters from ACK feedback.
5. **Response:** the selected profile changes Prague alpha and congestion-window evolution as predicted.

## Capability probe

Run the doctor first. Then inspect installed syntax rather than assuming it:

```bash
uname -a
ip -Version
tc -Version
tc qdisc help 2>&1 | grep -i dualpi2 || true
modinfo sch_dualpi2 2>/dev/null || true
tcpdump --version 2>/dev/null | head -n 1 || true
tshark --version 2>/dev/null | head -n 1 || true
```

If DualPI2 is not present, do not substitute a classic ECN AQM and label the result L4S. A classic AQM can be a separate fallback/coexistence experiment.

## Isolated topology

Use three namespaces: sender, bottleneck router, and receiver. Create two veth pairs, assign private addresses, enable forwarding only in the router namespace, and install cleanup traps before modifying links. Keep all names prefixed with `imq-l4s-` so cleanup is unambiguous.

Shape the router egress toward the receiver with an explicit rate class and attach DualPI2 below it. A typical structure is:

```bash
tc qdisc replace dev ROUTER_EGRESS root handle 1: htb default 10
tc class replace dev ROUTER_EGRESS parent 1: classid 1:10 htb rate 20mbit ceil 20mbit
tc qdisc replace dev ROUTER_EGRESS parent 1:10 handle 10: dualpi2
```

The installed `tc` help is authoritative for DualPI2 parameters. Record every non-default AQM value. Do not vary DualPI2 controller parameters in the same run as Prague parameters unless the experiment explicitly studies their interaction.

## Packet evidence

Capture outside the sender and on the bottleneck egress. For IPv4, ECN values are in the low two DS-field bits: Not-ECT `0`, ECT(1) `1`, ECT(0) `2`, CE `3`.

Example extraction with TShark:

```bash
tshark -r results/l4s/run.pcapng \
  -T fields -e frame.time_epoch -e ip.src -e ip.dst -e ip.dsfield.ecn \
  > results/l4s/ecn-fields.tsv
```

Count ECT(1) and CE separately and retain representative packet numbers or timestamps. For IPv6 use `ipv6.tclass`-based display fields supported by the installed TShark version.

## Feedback evidence

QUIC ACK frames are encrypted, so an ordinary capture does not prove ACK ECN counters. Use one of:

- an existing picoquic qlog event that records peer ECN counters;
- the public metrics accessor added by the patch;
- temporary test-only instrumentation guarded by a build option.

Record counter samples over time. CE must increase after CE-marked packets appear; a static counter is not feedback evidence.

## Response evidence

Log timestamps, bytes in flight, congestion window, pacing rate, ECT(1), CE, and Prague alpha. Compare the observed state transition with the rational profile and the controller's documented rounding rule. Run the upstream-default profile first, then one changed parameter at a time.

Useful negative controls:

- Prague with no bottleneck marking;
- Reno on the same path;
- Prague with the default profile;
- Prague with the experimental profile.

## Fallback report

When privileged validation is unavailable, include this statement verbatim:

> Build and structural validation completed. Packet-level L4S validation was not performed because the required Linux namespace, DualPI2, privilege, or capture capability was unavailable.

List the failed capability probes and preserve the commands needed to rerun on a suitable host.
