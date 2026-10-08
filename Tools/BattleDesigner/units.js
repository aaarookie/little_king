/* Canonical LKUnitContent registry, including heroes, summons, mercenaries and buildings. */
(function(root){
  const units = [
  {
    "id": "Hero_Knight",
    "label": "骑士"
  },
  {
    "id": "Hero_Mage",
    "label": "法师"
  },
  {
    "id": "Hero_Ranger",
    "label": "游侠"
  },
  {
    "id": "Unit_Swordsman",
    "label": "剑士"
  },
  {
    "id": "Unit_Archer",
    "label": "弓箭手"
  },
  {
    "id": "Unit_Shieldbearer",
    "label": "盾卫"
  },
  {
    "id": "Building_ArrowTower",
    "label": "箭塔"
  },
  {
    "id": "Building_Barracks",
    "label": "兵营"
  },
  {
    "id": "Unit_Skeleton",
    "label": "骷髅兵"
  },
  {
    "id": "Unit_SkeletonArcher",
    "label": "骷髅射手"
  },
  {
    "id": "Hero_Necromancer",
    "label": "死灵法师"
  },
  {
    "id": "Hero_SkeletonGiant",
    "label": "骷髅巨人"
  },
  {
    "id": "Boss_SkeletonKing",
    "label": "骷髅王"
  },
  {
    "id": "Unit_ElfArcher",
    "label": "精灵弓箭手"
  },
  {
    "id": "Unit_ElfWarrior",
    "label": "精灵战士"
  },
  {
    "id": "Unit_ElfGuard",
    "label": "精灵盾卫"
  },
  {
    "id": "Unit_ElfPriest",
    "label": "精灵牧师"
  },
  {
    "id": "Unit_GoblinRogue",
    "label": "哥布林大盗"
  },
  {
    "id": "Unit_Thief",
    "label": "窃贼"
  },
  {
    "id": "Unit_ApprenticeMage",
    "label": "学徒法师"
  },
  {
    "id": "Unit_GoblinBlade",
    "label": "开道利刃"
  },
  {
    "id": "Unit_TrollWarrior",
    "label": "巨魔战士"
  },
  {
    "id": "Unit_TrollSpearman",
    "label": "巨魔投矛手"
  },
  {
    "id": "Unit_TrollMage",
    "label": "巨魔法师"
  },
  {
    "id": "Unit_TrollKing",
    "label": "巨魔王"
  },
  {
    "id": "Unit_TwoHeadedDragon",
    "label": "双头龙"
  },
  {
    "id": "Building_SiegeCatapult",
    "label": "攻城投石炮"
  },
  {
    "id": "Unit_Colossus",
    "label": "擎天巨像"
  },
  {
    "id": "Unit_AngelWarrior_Uncommon",
    "label": "战斗天使（优秀）"
  },
  {
    "id": "Unit_AngelArcher_Uncommon",
    "label": "弓箭手天使（优秀）"
  },
  {
    "id": "Unit_AngelPriest_Uncommon",
    "label": "牧师天使（优秀）"
  },
  {
    "id": "Unit_AngelWarrior_Rare",
    "label": "战斗天使（稀有）"
  },
  {
    "id": "Unit_AngelArcher_Rare",
    "label": "弓箭手天使（稀有）"
  },
  {
    "id": "Unit_AngelPriest_Rare",
    "label": "牧师天使（稀有）"
  },
  {
    "id": "Unit_AngelWarrior_Epic",
    "label": "战斗天使（史诗）"
  },
  {
    "id": "Unit_AngelArcher_Epic",
    "label": "弓箭手天使（史诗）"
  },
  {
    "id": "Unit_AngelPriest_Epic",
    "label": "牧师天使（史诗）"
  },
  {
    "id": "Unit_DivineJudge",
    "label": "圣玛丽亚的神圣审判者"
  },
  {
    "id": "Unit_ChosenHighPriest",
    "label": "圣玛丽亚的神选牧师长"
  },
  {
    "id": "Unit_WallCommon",
    "label": "普通防御墙"
  },
  {
    "id": "Unit_WallRare",
    "label": "稀有防御墙"
  },
  {
    "id": "Unit_WallEpic",
    "label": "史诗防御墙"
  },
  {
    "id": "Unit_WallImperial",
    "label": "禁军之墙"
  },
  {
    "id": "Unit_SiegeCart",
    "label": "攻城车"
  },
  {
    "id": "Unit_MageTower",
    "label": "法师塔"
  },
  {
    "id": "Unit_HealingStation",
    "label": "牧师治疗站"
  }
];
  if(typeof module === "object" && module.exports) module.exports = units;
  if(root) root.LKUnitCatalog = units;
})(typeof globalThis !== "undefined" ? globalThis : this);
