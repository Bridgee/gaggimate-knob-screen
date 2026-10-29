# Contributing

Keep changes focused on the K718 display port. Use English for documentation, comments and user-facing additions.
Preserve upstream page flows, controller protocol and coffee-control logic unless a change explicitly requires otherwise.

1. Use the pinned tools and dependencies described in [Building](docs/BUILDING.md).
2. Edit source inputs rather than generated native UI alone.
3. Run `python scripts/check_public.py` and `python scripts/k718.py check`.
4. Describe the behavior changed and how it was checked. Include hardware checks only when actually performed.

Do not commit build outputs, credentials, device backups or personal development records.
For an upstream version change, follow [Maintaining](docs/MAINTAINING.md).
