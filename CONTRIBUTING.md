# Contributing

- Every commit needs a `Signed-off-by:` (DCO 1.1) under a known identity —
  the same rule as upstream U-Boot. No CLA, ever.
- One logical change per commit; each commit should build; follow upstream
  U-Boot style (checkpatch-clean where practical).
- Work on branches named `review/<your-handle>/<topic>` and open a PR
  against `oneplus-15`. `oneplus-15` and `master` are never force-pushed.
- If an AI coding tool materially contributed to a change, say so in the
  PR (and carry the disclosure into any upstream submission per the
  destination project's current policy — Linux and U-Boot both have one).
- The read-only storage policy is intentional. PRs that add storage write
  or erase paths to the default config will be declined.
- Anything destined for upstream U-Boot gets re-curated as a proper
  mailing-list series with maintainer-required provenance; landing here
  first is encouraged but does not skip that step.
