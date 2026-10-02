# Little King · 小小国王

[简体中文](README.md) | [English](README.en.md)

A single-player PvE auto-battler prototype built with **Unreal Engine 5.8, C++, Paper2D, GAS, and UMG**. Current version: **v0.8.3** (`ProjectVersion=0.8.3`). The game UI and detailed design documents are in Simplified Chinese.

Prepare three heroes and a deck at home, depart through the gate, and follow randomized routes across fixed regions. Deploy every hero before combat, command camps, and cycle cards. Bring gold and research materials home to develop the settlement.

## v0.8.3: researched spells and angel reinforcements

- Nine unlockable spells: Freeze, four Saint Maria reinforcement tiers, Black Cloud, Lightning, Hurricane, and Divine Blessing. Purchase rare spellbooks during expeditions, research them at home, then add the unlocked spells to the loadout.
- Eleven angel stat configurations share five animated character identities. Angels are summoned exclusively by spells and never become drawable cards. Priests use basic ranged healing; incapacitated heroes cannot be revived by healing.
- Black Cloud conceals both teams inside its area. Lightning strikes enemy nonheroes and chooses its second target after the delay. Hurricane forces movable characters on both teams rightward; buildings and camps remain fixed.
- Divine Blessing heals friendly units across the battlefield and cannot be drawn for 150 seconds after casting. Four unique hand slots stay full. Pause stops cooldowns; each battle resets them. Only fireballs cause camera shake.
- Includes five transparent 16-pose atlases, nine illustrated spell icons, and six original synthesized sound effects. Original images, full generation prompts, audio parameters, and import scripts are included.

See the [change record](docs/53-v0.8.3Changes.md), [asset provenance and prompts](docs/54-v0.8.3Assets.md), [UE 5.8 guide](docs/55-v0.8.3Guide.md), and [validation notes](docs/56-v0.8.3ReleaseNotes.md).

## v0.8.2 foundation: balance, research, and expedition recovery

- Hero and skeleton durability increased; enemy health and attack scale with route depth. Finite reinforcements are more frequent. Skeleton Circle is an enemy-only slowing and summoning spell; skeleton cards are also unavailable to the player. Necromancer sacrifice now costs 3% maximum health.
- Heal Wave restores 120 health to friendly targets plus 6% maximum health to heroes, with cost 2 and radius 300. Combat healing cannot revive incapacitated heroes.
- Depart with at least seven distinct cards. Default weighted capacity is eight: trolls cost two slots and the Colossus three. Four hand slots remain full and unique; a drawn card occupies one hand slot. Fixed limits require the minimum necessary discard; flexible intervals allow any replacement ending within the legal range. Keep at least five distinct cards during a run.
- Combat rewards and ordinary market stock grant temporary mercenaries only. Spell/building numerical upgrades are available exclusively at rest nodes: choose healing or one upgrade, multiplying values by 1.1 per level.
- Markets very rarely sell expensive spellbooks or building blueprints; advanced books require later regions. Consume purchased materials at the home library for permanent unlocks. v0.8.2 introduced seven research spells and the siege catapult blueprint; v0.8.3 expands research to sixteen spells.
- Failure returns 80% of remaining wallet gold, rounded down; abandonment returns 100%. Purchased research materials return in full for every outcome. Abandonment is unavailable during deployment or combat.
- Pause with Esc or the combat button. Resume, or exit to the start/save menu. Loading retries the selected battle with its original pre-battle heroes, cards, enemies, and seed; it cannot change the chosen branch or reroll market stock.
- Each region has roughly fifteen route layers, with fifteen visited stops and nine to ten battles. Only the current region is visible. Returning home during an expedition requires explicitly abandoning it.

See the [change record](docs/50-v0.8.2ExpeditionChanges.md), [UE 5.8 play/configuration guide](docs/51-v0.8.2Guide.md), and [release notes](docs/52-v0.8.2ReleaseNotes.md). These and the [current design](docs/01-GDD.md) supersede conflicting older tutorials.

## Current gameplay

Seven home buildings provide the Saint Maria Statue, Library, Gate, Hero House, Treasury, Barracks, and War Room. The statue grants post-battle recovery of 40/60/80/100% maximum health. Treasury silver capacities are 5/7/9/11/13, generation increases by 10% of base per level, and departure gold limits are 100/150/200/250/300. Expedition income is not limited by departure capacity; home bonuses are frozen at departure.

Five regions have fixed boundaries, entrances, and exits. Nodes and forward-only connections are generated and saved for each run. A route normally visits four regions, choosing one node per layer. Normal encounters feature one enemy hero, elite encounters several heroes, and boss encounters a boss with supporting heroes. Enemy units rebuild at full health each battle.

All three heroes must be deployed before combat; deployment has no timer. Camps have volume but cannot be attacked. Click a camp and a reachable location to issue a movement order; the hero stops attacking until arrival or a stuck timeout. The Mage enables full-field spell placement; without that trait, spells remain usable in the friendly half. Knight taunt takes priority over focus fire. Combat units have overhead health bars, attack-building placement previews show ranges, and a yellow line divides battlefield halves.

Characters/buildings have five qualities and spells fourteen tiers. The current catalog contains 39 units and 41 registered cards: eleven units are summon-only; three cards are enemy-only, fourteen are temporary expedition mercenaries, and twenty-four are starter/research cards. Detailed values and abilities are in the [catalog](docs/19-ContentCatalog.md), [balance guide](docs/32-CharacterQualityAndBalance.md), [card interfaces](docs/34-CardDevelopmentInterfaces.md), and [troll/siege guide](docs/35-TrollAndSiegeCards.md).

## Build and play

Verified target: **UE 5.8.1 / Windows / Development Editor**. Install UE 5.8 and its supported Visual Studio C++ toolchain; Paper2D and GameplayAbilities are enabled.

1. Close any editor instance for this project and build from the repository root, adjusting the engine path:

   ```powershell
   $engineRoot = 'E:\epic\UE_5.8'
   $projectPath = (Resolve-Path '.\little_king.uproject').Path
   & "$engineRoot\Engine\Build\BatchFiles\Build.bat" little_kingEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE
   ```

2. Open `little_king.uproject` and click Play. `L_StartMenu` offers Continue, New Game, Load/Delete Game, and a Settings placeholder. Up to eight independent saves are supported.
3. Save three heroes and at least seven cards at the War Room. Set departure gold at the Gate and depart. Loading an active expedition resumes it directly.
4. Deploy heroes, begin combat, and play cards by selecting them and a valid location. Claim/skip rewards, follow the route, buy mercenaries/materials, and rest or upgrade.
5. Return home after victory/failure. To leave mid-expedition, exit to the start menu; to return home, confirm abandonment at a non-combat node.

The console command `show me the money` adds 100 home gold; `show me the money 500` specifies the amount. Runtime UI, maps, art, sound, and Chinese fonts are included. No manual Blueprint creation or asset download is required. Python, image generation, and Music3 are optional authoring tools. The [v0.8.3 guide](docs/55-v0.8.3Guide.md) includes isolated spell playtesting, cooldown UI previews, and sound review.

## Validation and saves

The UE 5.8.1 editor build succeeded and all **129/129** `LittleKing` automation tests passed, including twenty v0.8.3 tests. Real 720p and 1080p captures cover five character pose sets, walking in both directions, nine card icons, area spells, and the native UMG Blessing countdown. Tests use isolated storage; six existing save files (five player files and one historical fixture) remain byte-identical. Results, warning review, and screenshot hashes are in the [v0.8.3 validation](docs/validation/v0.8.3-Release.json); previous records remain historical.

Run schema 10 and profile schema 2 migrate compatible old saves. Existing maps remain frozen, settled receipts retain their amounts, and invalid files are preserved with an error. Old five/six-card loadouts can be read but must reach seven cards for the next departure. Balance V1 and v0.8.2 Heal Wave measurements remain historical baselines; long-term balance with the new spells and healing mercenaries still needs playtesting.

Packaging and hardware acceptance remain deferred; Sprint 6 stays skipped. Historical releases: [v0.8.1 presentation fixes](docs/47-v0.8.1ReleaseNotes.md), 87/87 tests; [v0.8 art/audio integration](docs/44-v0.8ReleaseNotes.md), 83/83 tests. The storybook assets and walking fixes for 24 biped characters are retained.

## Collaboration and documentation

| Path | Purpose |
|---|---|
| `Source/little_king` | Gameplay, native content, UI, run/profile systems, and tests |
| `Content` | Maps, Blueprints, data assets, and runtime artwork/audio |
| `Scripts` | Asset synchronization, import, and audit tools |
| `docs` | Design, collaboration records, tutorials, and validation |
| `ArtSource/StorybookV1` | Original assets, receipts, prompts, and licenses |
| `Saved` | Local logs, screenshots, and player saves; excluded from Git |

Built-in identities, skills, and behaviors have C++ defaults; tables tune values and presentation. Custom unit IDs remain supported. v0.8.2 updated authored unit/encounter tables, GameData, and Heal Wave. v0.8.3 adds native content and private runtime copies, with new resources under `Art/StorybookV1/V083`; shared maps, Blueprints, and data assets are not rewritten. See the [collaboration file index](docs/53-v0.8.3Changes.md).

Useful references: [design](docs/01-GDD.md), [architecture](docs/02-BattlePrototypeDesign.md), [dungeon D0–D5](docs/18-DungeonChangeLog.md), [home H0–H5](docs/27-HomeChangeLog.md), [optimization log](docs/29-OptimizationChangeLog.md), [asset inventory](docs/12-AssetRequest.md), and [asset provenance](docs/36-ArtAudioSources.md).

Assets retain their respective terms: Noto fonts include OFL licenses; MiniMax-Music3 music follows the archived Community License and keeps its menu credit; generated artwork is not labeled CC0. Exact sources, processing, and prompts are recorded in [36](docs/36-ArtAudioSources.md), [39](docs/39-BattleArtPrompts.md), [41](docs/41-WorldArtPrompts.md), [43](docs/43-PolishArtPrompts.md), and [46](docs/46-MovementFixPrompts.md). New v0.8.3 images, full prompts, and synthesized sound provenance are recorded in [54](docs/54-v0.8.3Assets.md); no manual download is required.
