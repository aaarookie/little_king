# Little King · 小小国王

[简体中文](README.md) | [English](README.en.md)

A single-player PvE auto-battler prototype built with **Unreal Engine 5.8, C++, Paper2D, GAS, and UMG**. Current version: **v0.8.6** (`ProjectVersion=0.8.6`). The UI and detailed documentation are in Simplified Chinese.

Prepare three heroes and a deck at home, follow randomized routes across five fixed regions, and bring gold, items, and research materials back to develop seven home buildings. The game supports eight save slots, research unlocks, pause and expedition recovery. The catalog contains **46 units, 48 cards, 16 research spells, and 8 building blueprints**.

v0.8.6 adds three market categories, stage-based rarity and research supply, expedition inventory and home item storage. It improves enemy AI, animation interruption, save transactions, and battle telemetry. Accessories and equipment currently use replaceable placeholders with five rarities and no combat attributes. See the [release notes](docs/OptimizationImplementation.md#v086-发布记录).

The offline [Battle Workshop](Tools/BattleDesigner/index.html), introduced in v0.8.5, supports 32 modules, four terrain types, timed/conditional reinforcements, enemy card economy, and JSON roundtrips. The [standalone loader](Scripts/PlayBattleLayout.ps1) runs authored layouts in UE without writing expedition progress. Automatic assignment to expedition nodes is still pending.

## Documentation

Start with the [project handoff](docs/64-ProjectHandoff.md). Documents now cover current topics and consolidated development stages; superseded plans and tutorials have been removed.

| Document | Scope |
| --- | --- |
| [Game design](docs/01-GDD.md) | Current gameplay, expeditions, home, combat, and saves |
| [Architecture](docs/02-BattlePrototypeDesign.md) | Modules, state transitions, and data ownership |
| [Content catalog](docs/19-ContentCatalog.md) | Current units, cards, values, and abilities |
| [Content interfaces](docs/34-CardDevelopmentInterfaces.md) | Registration, identity, presentation, and extension contracts |
| [Art and audio](docs/36-ArtAudioSources.md) | Style, provenance, original prompts, and licenses |
| [Foundation history](docs/DevelopmentHistory.md) | Prototype, dungeon, home, optimization, and v0.8–v0.8.2 |
| [v0.8.3 spells and angels](docs/53-v0.8.3Changes.md) | Delivery and validation |
| [v0.8.4 walls and garrisons](docs/57-v0.8.4Changes.md) | Delivery and validation |
| [v0.8.5 Battle Workshop](docs/62-TerrainAndEncounterDesigner.md) | Terrain, JSON contracts, integration limits, and release evidence |
| [v0.8.6 research-driven optimization](docs/OptimizationImplementation.md) | Market, items, AI, animation, save protection, and release validation |
| [GitHub research and recommendations](docs/GitHubResearchAndRecommendations.md) | Chinese report on 24 projects: animation, AI, balance, economy, and shop probabilities; adopted changes are recorded in the v0.8.6 implementation report |

## Validation and limits

v0.8.6 validation records a successful UE 5.8.1 Development Editor build, **193/193** LittleKing tests (95 with warnings), **43/43** focused tests, and **37/37** Node tests. It includes 32,000 market generations, 18 scripted battles, and actual 720p/1080p market captures; six existing saves stayed byte-identical. Release preparation changes only version metadata and documentation, reusing results for the same tested source. See the [release evidence](docs/validation/v0.8.6-Release.json).

Run schema 11 and profile schema 3 migrate compatible old saves. Historical reports retain their original version context. Long-term balance still needs playtesting; Sprint 6 was skipped, and packaging and hardware acceptance remain deferred.

## Repository

- `Source/little_king`: gameplay, native UI, and tests.
- `Content`: maps, Blueprints, data assets, and runtime media.
- `Tools/BattleDesigner`: offline editor and layout format.
- `Scripts`: imports, audits, and development entry points.
- `ArtSource/StorybookV1`: original media, prompts, receipts, and licenses.
- `docs/validation` and `docs/images`: historical reports and actual captures.
- `Saved`: local logs and player saves, excluded from Git.

Built-in identities and behaviors have C++ defaults. Data tables tune values and presentation; runtime changes use private copies. Assets retain their respective terms: Noto fonts include OFL, MiniMax-Music3 music retains its license and credit, and generated art is not labeled CC0.
