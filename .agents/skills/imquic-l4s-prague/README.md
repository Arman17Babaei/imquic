# IMQUIC L4S Prague Codex Skill

A Codex skill for modifying an IMQUIC checkout to use a workspace-managed picoquic fork with configurable Prague parameters and honest L4S validation.

## Install for one repository

From the IMQUIC repository root:

```bash
mkdir -p .agents/skills
cp -R /path/to/imquic-l4s-prague .agents/skills/
```

Codex scans `.agents/skills` from the working directory up to the Git root. This makes the skill available in the Codex IDE extension used from VS Code.

## Install for your user account

```bash
mkdir -p "$HOME/.agents/skills"
cp -R /path/to/imquic-l4s-prague "$HOME/.agents/skills/"
```

Codex normally detects skill changes automatically. Restart the IDE extension or Codex session when the skill does not appear.

## Invoke

In the Codex composer, type `$` and select `imquic-l4s-prague`, or write:

```text
$imquic-l4s-prague Patch this IMQUIC checkout for configurable Prague/L4S use. Use the gentle-response profile and stop after local commits and validation.
```

## Package self-tests

```bash
python3 scripts/prague_profile.py validate assets/l4s-prague.yaml
python3 scripts/prague_profile.py render-options assets/l4s-prague.yaml gentle-response
python3 scripts/doctor.py /path/to/imquic --json
```

The package source repository also contains pytest contract tests. The skill itself does not require pytest when installed.

## Operational boundaries

The skill targets IMQUIC, creates a local picoquic clone under `.deps`, creates local Git commits, and does not push. It can perform packet-level L4S checks only on a Linux host that provides suitable privileges, network namespaces, an L4S-capable DualPI2 qdisc, and capture tooling.
