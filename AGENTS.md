# Project entry point

This is Little King, a UE 5.8 C++/Paper2D game. User communication and most project docs are in Chinese.

- Start with `docs/64-ProjectHandoff.md`, then README and the current release notes. Read only the feature documents relevant to the task; old Sprint records contain superseded rules.
- Verify `git status`, current version, and actual code/report evidence before relying on chat memory. Latest explicit user instructions take precedence over this handoff.
- Current v0.8.5 includes an offline Battle Designer and standalone JSON-to-battle loader. Automatic assignment of authored layouts to expedition nodes is still future work.
- Preserve collaborators' work and player saves. Runtime tuning uses private copies of authored assets. Record behavior/interface changes and validation in repository docs.
- Built-in unit/card identity and behavior come from C++ defaults; data tables tune values/presentation. Keep `UnitId` and `CardId` distinct and synchronize editor catalogs when registered content changes.
- Test proportionately: existing validation is not a reason to rerun the whole suite for documentation-only edits. After behavior changes, run relevant checks and report what was actually exercised.
- Use `rg --files` before guessing paths. Read summary fields of large validation reports. PowerShell Git refs containing `^{}` must be quoted.
- The user's Git HTTP proxy is `http://127.0.0.1:18081`; use per-command configuration. Release requests include version metadata, notes, commit, tag, push, and remote verification.
- Packaging and hardware testing remain deferred; Sprint 6 was intentionally skipped. New feature scope comes from the user, not from optional suggestions in the handoff.
