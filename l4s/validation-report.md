# IMQUIC L4S environment validation

Date: 2026-07-16

## Revisions

- IMQUIC base: `0b4def337956f039753f9358fbdd194db0311e8d`
- IMQUIC environment commit: `c903440b7520c5bd6fe202fe6eaf991af78ad271`
- IMQUIC branch: `codex/imquic-l4s-prague`
- picoquic upstream: `https://github.com/private-octopus/picoquic`
- picoquic pin: `13671ce7bdf58c278a29da2d49a32f76c21d6c6d`
- picoquic local branch: `codex/prague-params`
- picoquic tracking: Git submodule at `.deps/picoquic-l4s`
- picotls pin: `bfa67875982afc4c24f21e146cef4747fa189c2f`

Both repositories were clean when the results below were recorded.

Initialize the tracked dependency in a fresh checkout with:

```sh
git submodule update --init --recursive
```

## Selected profile

`picoquic-observed-default` renders as:

```text
alpha_gain=1/16,ce_response=1/2,loss_beta=1/2,sudden_ce_threshold=1/2
```

The profile is installed and validated, but this environment commit does not yet patch picoquic to parse these options. The values therefore were not passed to an active controller.

## Commands and results

```sh
cmake -S . -B build-l4s \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DPICOQUIC_FETCH_PTLS=Y \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-l4s -j"$(nproc)"
ctest --test-dir build-l4s --output-on-failure
```

Result: picoquic built successfully; `picoquic_ct` and `picohttp_ct` passed (2/2).

The dependency was also built in place with PIC so the current IMQUIC static-library discovery can link it:

```sh
cmake -S . -B . \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DPICOQUIC_FETCH_PTLS=Y \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . -j"$(nproc)"
```

IMQUIC validation:

```sh
sh autogen.sh
./configure --with-picoquic="$PWD/.deps/picoquic-l4s"
make -j"$(nproc)"
make check
```

Result: configuration, compilation, linking, and the available check targets passed. IMQUIC currently defines no executable test suite under `make check`.

Profile validation:

```sh
python3 tools/l4s/prague_profile.py validate config/l4s-prague.yaml
python3 tools/l4s/prague_profile.py render-options \
  config/l4s-prague.yaml picoquic-observed-default
```

Result: validation passed and produced the normalized string shown above.

## L4S evidence and limitations

- ECT(1) packet evidence: not collected.
- CE marking evidence: not collected.
- QUIC ACK ECN feedback evidence: not collected.
- Prague alpha evidence: not collected.
- Congestion-window response: not measured.
- Privileged namespace and DualPI2 validation was unavailable because the session was not root.
- DualPI2 availability was not established; no classic ECN AQM was substituted.

> Build and structural validation completed. Packet-level L4S validation was not performed because the required Linux namespace, DualPI2, privilege, or capture capability was unavailable.

The current evidence level is **Build** for dependency plumbing only. It is not evidence that IMQUIC selects Prague, emits ECT(1), processes CE feedback, or responds as an L4S flow.

## Remaining work

1. Add test-first picoquic parsing and fixed-point behavior for `alpha_gain`, `ce_response`, `loss_beta`, and `sudden_ce_threshold` while preserving upstream defaults.
2. Expose typed controller selection, option propagation, and stable transport metrics through IMQUIC without including picoquic internal headers.
3. Run packet marking, feedback, and congestion-response validation on a root-capable Linux host with DualPI2.

## Scientific and standards basis

The experimental interpretation should follow the L4S architecture and ECN protocol in RFC 9330 and RFC 9331, the DualQ AQM requirements in RFC 9332, and QUIC transport and recovery behavior in RFC 9000 and RFC 9002. Prague estimator choices should be discussed as implementation behavior, informed by Briscoe and De Schepper's 2019 scaling analysis and the DCTCP estimator background, rather than presented as standardized parameter values.
