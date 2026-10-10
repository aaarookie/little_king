# Project entry point

This is Little King, a UE 5.8 C++/Paper2D game. User communication and most project docs are in Chinese.

- Start with `docs/64-ProjectHandoff.md`, then README and `docs/OptimizationImplementation.md` for the current v0.8.6 release; use `docs/62-TerrainAndEncounterDesigner.md` for the Battle Designer. Read only the topic documents relevant to the task; `docs/DevelopmentHistory.md` summarizes earlier stages.
- Verify `git status`, current version, and actual code/report evidence before relying on chat memory. Latest explicit user instructions take precedence over this handoff.
- Current v0.8.6 includes an offline Battle Designer and standalone JSON-to-battle loader. Automatic assignment of authored layouts to expedition nodes is still future work.
- v0.8.6 implements research-driven market, item storage, AI, animation and telemetry changes; read `docs/OptimizationImplementation.md` for scope and validation. Run/Profile schemas are 11/3; placeholder items have no combat attributes.
- Preserve collaborators' work and player saves. Runtime tuning uses private copies of authored assets. Record behavior/interface changes and validation in repository docs.
- Built-in unit/card identity and behavior come from C++ defaults; data tables tune values/presentation. Keep `UnitId` and `CardId` distinct and synchronize editor catalogs when registered content changes.
- Test proportionately: existing validation is not a reason to rerun the whole suite for documentation-only edits. After behavior changes, run relevant checks and report what was actually exercised.
- Keep documentation consolidated by current topic or development stage. Superseded plans and tutorials were removed at the user's request; preserve original validation reports and asset provenance instead of duplicating them in new documents.
- Use `rg --files` before guessing paths. Read summary fields of large validation reports. PowerShell Git refs containing `^{}` must be quoted.
- The user's Git HTTP proxy is `http://127.0.0.1:18081`; use per-command configuration. Release requests include version metadata, notes, commit, tag, push, and remote verification.
- Packaging and hardware testing remain deferred; Sprint 6 was intentionally skipped. New feature scope comes from the user, not from optional suggestions in the handoff.
