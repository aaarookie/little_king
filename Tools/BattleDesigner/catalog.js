/* Canonical card IDs checked against ULKGameData, LKExpeditionMercenaryContent and LKResearchContent. */
(function(root){
  const cards = [
  {
    "id": "Unit_Swordsman",
    "label": "剑士",
    "kind": "佣兵"
  },
  {
    "id": "Unit_Archer",
    "label": "弓箭手",
    "kind": "佣兵"
  },
  {
    "id": "Unit_Shieldbearer",
    "label": "盾卫",
    "kind": "佣兵"
  },
  {
    "id": "Spell_Fireball",
    "label": "火球术",
    "kind": "法术"
  },
  {
    "id": "Spell_HealWave",
    "label": "治疗波",
    "kind": "法术"
  },
  {
    "id": "Building_ArrowTower",
    "label": "箭塔",
    "kind": "建筑"
  },
  {
    "id": "Building_Barracks",
    "label": "兵营",
    "kind": "建筑"
  },
  {
    "id": "Unit_Skeleton",
    "label": "骷髅兵",
    "kind": "佣兵"
  },
  {
    "id": "Unit_SkeletonArcher",
    "label": "骷髅射手",
    "kind": "佣兵"
  },
  {
    "id": "Spell_SkeletonCircle",
    "label": "骷髅法阵",
    "kind": "法术"
  },
  {
    "id": "Unit_ElfArcher",
    "label": "精灵弓箭手",
    "kind": "佣兵"
  },
  {
    "id": "Unit_ElfWarrior",
    "label": "精灵战士",
    "kind": "佣兵"
  },
  {
    "id": "Unit_ElfGuard",
    "label": "精灵盾卫",
    "kind": "佣兵"
  },
  {
    "id": "Unit_ElfPriest",
    "label": "精灵牧师",
    "kind": "佣兵"
  },
  {
    "id": "Unit_GoblinRogue",
    "label": "哥布林大盗",
    "kind": "佣兵"
  },
  {
    "id": "Unit_Thief",
    "label": "窃贼",
    "kind": "佣兵"
  },
  {
    "id": "Unit_ApprenticeMage",
    "label": "学徒法师",
    "kind": "佣兵"
  },
  {
    "id": "Unit_GoblinBlade",
    "label": "开道利刃",
    "kind": "佣兵"
  },
  {
    "id": "Unit_TrollWarrior",
    "label": "巨魔战士",
    "kind": "佣兵"
  },
  {
    "id": "Unit_TrollSpearman",
    "label": "巨魔投矛手",
    "kind": "佣兵"
  },
  {
    "id": "Unit_TrollMage",
    "label": "巨魔法师",
    "kind": "佣兵"
  },
  {
    "id": "Unit_TrollKing",
    "label": "巨魔王",
    "kind": "佣兵"
  },
  {
    "id": "Unit_TwoHeadedDragon",
    "label": "双头龙",
    "kind": "佣兵"
  },
  {
    "id": "Building_SiegeCatapult",
    "label": "攻城投石炮",
    "kind": "建筑"
  },
  {
    "id": "Unit_Colossus",
    "label": "擎天巨像",
    "kind": "佣兵"
  },
  {
    "id": "Spell_ResearchFireball_N2",
    "label": "强化火球术",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchFireball_M1",
    "label": "烈焰火球",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchFireball_A1",
    "label": "灼日火球",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchFireball_D1",
    "label": "天火",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchHeal_M1",
    "label": "生命泉涌",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchHeal_A1",
    "label": "圣光潮汐",
    "kind": "法术"
  },
  {
    "id": "Spell_ResearchHeal_D1",
    "label": "生命礼赞",
    "kind": "法术"
  },
  {
    "id": "Card_WallCommon",
    "label": "普通防御墙",
    "kind": "建筑"
  },
  {
    "id": "Card_WallRare",
    "label": "稀有防御墙",
    "kind": "建筑"
  },
  {
    "id": "Card_WallEpic",
    "label": "史诗防御墙",
    "kind": "建筑"
  },
  {
    "id": "Card_WallImperial",
    "label": "禁军之墙",
    "kind": "建筑"
  },
  {
    "id": "Card_SiegeCart",
    "label": "攻城车",
    "kind": "建筑"
  },
  {
    "id": "Card_MageTower",
    "label": "法师塔",
    "kind": "建筑"
  },
  {
    "id": "Card_HealingStation",
    "label": "牧师治疗站",
    "kind": "建筑"
  },
  {
    "id": "Spell_Freeze",
    "label": "冰冻法术",
    "kind": "法术"
  },
  {
    "id": "Spell_MariaNovice",
    "label": "圣玛丽亚的初阶增援",
    "kind": "法术"
  },
  {
    "id": "Spell_MariaIntermediate",
    "label": "圣玛丽亚的中阶增援",
    "kind": "法术"
  },
  {
    "id": "Spell_MariaAdvanced",
    "label": "圣玛丽亚的高阶增援",
    "kind": "法术"
  },
  {
    "id": "Spell_MariaDivine",
    "label": "圣玛丽亚的神圣增援",
    "kind": "法术"
  },
  {
    "id": "Spell_BlackCloud",
    "label": "黑云法术",
    "kind": "法术"
  },
  {
    "id": "Spell_Lightning",
    "label": "雷电法术",
    "kind": "法术"
  },
  {
    "id": "Spell_Hurricane",
    "label": "飓风法术",
    "kind": "法术"
  },
  {
    "id": "Spell_DivineBlessing",
    "label": "神圣祝福",
    "kind": "法术"
  }
];
  if(typeof module === "object" && module.exports) module.exports = cards;
  if(root) root.LKCardCatalog = cards;
})(typeof globalThis !== "undefined" ? globalThis : this);
