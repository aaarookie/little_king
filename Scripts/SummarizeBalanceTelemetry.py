"""Summarize bounded local battle observations; never interprets scripted tests as player data."""
from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = (len(ordered) - 1) * fraction
    lo, hi = math.floor(index), math.ceil(index)
    return round(ordered[lo] + (ordered[hi] - ordered[lo]) * (index - lo), 3)


def summarize(matches: list[dict], include_automation: bool = False) -> dict:
    groups: dict[tuple, list] = defaultdict(list)
    excluded = 0
    for match in matches:
        if match.get("mode") == "automation" and not include_automation:
            excluded += 1
            continue
        groups[(match.get("mode", "unknown"), match.get("encounterId", "unknown"),
                match.get("regionId", "unknown"))].append(match)
    cohorts = []
    for (mode, encounter, region), rows in sorted(groups.items()):
        cohort = {"mode": mode, "encounterId": encounter, "regionId": region, "samples": len(rows),
                  "playerWinFraction": sum(row.get("playerWon") is True for row in rows) / len(rows),
                  "overtimeFraction": sum(row.get("overtime") is True for row in rows) / len(rows)}
        for side in ("match", "player", "enemy"):
            fields = ("duration",) if side == "match" else (
                "cards", "damage", "healing", "silverSpent", "silverOverflow",
                "silverAtCapSeconds", "firstHeroDeathSeconds", "peakUnits")
            metrics = {}
            for field in fields:
                values = [(row if side == "match" else row.get(side, {})).get(field) for row in rows]
                values = [float(value) for value in values if isinstance(value, (int, float))
                          and not isinstance(value, bool) and math.isfinite(value) and value >= 0]
                metrics[field] = {"observed": len(values), "mean": round(statistics.mean(values), 3) if values else None,
                                  "p50": percentile(values, .5), "p90": percentile(values, .9)}
            cohort[side] = metrics
        cohorts.append(cohort)
    return {"version": 1, "excludedAutomationMatches": excluded, "cohorts": cohorts,
            "interpretation": "Descriptive local samples only. Forced outcomes, player skill, deck, upgrades and route selection are confounders; this is not a target win-rate estimate. Missing hero deaths are excluded from death-time averages."}


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", type=Path, default=root / "Saved/Telemetry/Battles.json")
    parser.add_argument("--output", type=Path, default=root / "Saved/Telemetry/BalanceSummary.json")
    parser.add_argument("--include-automation", action="store_true")
    args = parser.parse_args()
    document = json.loads(args.input.read_text(encoding="utf-8-sig"))
    if document.get("version") != 1 or not isinstance(document.get("matches"), list):
        raise SystemExit("Unsupported telemetry document")
    result = summarize(document["matches"], args.include_automation)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "cohorts": len(result["cohorts"]),
                      "excludedAutomationMatches": result["excludedAutomationMatches"]}))


if __name__ == "__main__":
    main()
