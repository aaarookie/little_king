"""UE 5.8 editor Python: back up and update Heal Wave without changing its art."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
card = unreal.load_asset('/Game/Data/C_HealWave')
assert card is not None, 'Missing authored Heal Wave card'
backup = root / 'Saved/ExpeditionV082/HealWave.before.json'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    backup.write_text(json.dumps({key: card.get_editor_property(key) for key in
        ('cost', 'spell_value', 'spell_radius', 'hero_heal_percent')}, indent=2), encoding='utf-8')
card.set_editor_property('spell_value', 120.0)
card.set_editor_property('hero_heal_percent', 0.06)
assert unreal.EditorAssetLibrary.save_loaded_asset(card, only_if_is_dirty=False)
assert card.get_editor_property('spell_value') == 120.0
unreal.log('[V082] Heal Wave: 120 flat + 6% hero maximum health. Cost, radius, art unchanged.')
