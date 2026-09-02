#include "AbilitySystem/CCGameplayTags.h"

namespace CCTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "角色正在攻击");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "角色正在闪避");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReacting, "State.HitReacting", "角色处于受击硬直");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immobilized, "State.Immobilized", "角色被定身");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "角色已死亡");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "无敌帧，伤害作废");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PerfectDodgeWindow, "State.PerfectDodgeWindow", "闪身判定窗口");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Light, "Ability.Light", "轻攻击连段");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Heavy, "Ability.Heavy", "重攻击");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Charged, "Ability.Charged", "蓄力攻击");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "闪避");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Spell_Immobilize, "Ability.Spell.Immobilize", "定身术");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Received, "Event.Hit.Received", "受击方收到命中事件");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_Advance, "Event.Combo.Advance", "连招推进到下一段");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PerfectDodge, "Event.PerfectDodge", "闪身判定成功");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Buff_PerfectDodgeFollowUp, "Buff.PerfectDodgeFollowUp", "闪身后的重攻强化窗口");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Block_Input, "Block.Input", "屏蔽玩家输入");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller 伤害数值通道");
}
