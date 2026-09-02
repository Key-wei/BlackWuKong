# M1 骨架搭建 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用真正初始化的 GAS 角色层级替换 ThirdPerson 模板类，建立属性系统、原生 Tag 声明、自动化测试模块与 Gameplay Debugger，使后续所有战斗功能有可依附的地基。

**Architecture:** 新建 `CCCharacterBase`（ACharacter + IAbilitySystemInterface，持有真正 `CreateDefaultSubobject` 出来的 ASC 与 AttributeSet），派生 `CCPlayerCharacter` 与 `CCEnemyCharacter`。属性初始化走 `UCCCharacterInitData` 数据资产。伤害在 `PostGameplayEffectExecute` 中统一结算。所有 Tag 以原生 `UE_DEFINE_GAMEPLAY_TAG` 声明。测试放在独立的 `CoreCombatTests` 模块，只覆盖纯逻辑。

**Tech Stack:** UE 5.8.1、C++、GameplayAbilities / GameplayTags / GameplayDebugger 模块、UE Automation Framework

## Global Constraints

- 引擎路径：`F:\20_Areas\GameDev\UE_5.8`（Build.bat 与 UnrealEditor-Cmd.exe 均在此）
- 项目路径：`F:\10_Projects\CoreCombat`，模块名 `CoreCombat`
- 目标平台仅 Win64，仅 Development Editor 配置
- **不做网络复制/预测**：不写 `GetLifetimeReplicatedProps`，`ReplicationMode` 一律 `Minimal`
- 状态互斥一律通过 `ActivationBlockedTags` 声明，**禁止在能力里写 if 判断状态**
- 伤害公式全局唯一，只存在于 `UCCAttributeSet::PostGameplayEffectExecute`
- Tag 一律原生声明于 `CCGameplayTags.h/.cpp`，禁止在代码里用 `RequestGameplayTag(FName("..."))` 拼字符串
- 所有新文件放在 `Source/CoreCombat/` 下的子目录中，遵循 spec §3.2 的目录结构
- 提交信息用中文，每个任务结束提交一次

---

## File Structure

| 文件 | 职责 |
|---|---|
| `Source/CoreCombat/CoreCombat.Build.cs` | 增加 `GameplayDebugger` 依赖 |
| `Source/CoreCombat/AbilitySystem/CCGameplayTags.h/.cpp` | 全项目原生 Tag 的唯一声明处 |
| `Source/CoreCombat/AbilitySystem/CCAttributeSet.h/.cpp` | 6 组属性 + 伤害结算唯一入口 |
| `Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.h/.cpp` | ASC 子类，M1 只做空壳 + Tag 查询封装 |
| `Source/CoreCombat/Data/CCCharacterInitData.h/.cpp` | 初始属性与默认能力集数据资产 |
| `Source/CoreCombat/Character/CCCharacterBase.h/.cpp` | 玩家与 BOSS 共同基类，持有 ASC/AttributeSet |
| `Source/CoreCombat/Character/CCPlayerCharacter.h/.cpp` | 相机臂 + Enhanced Input |
| `Source/CoreCombat/Character/CCEnemyCharacter.h/.cpp` | AI 侧角色，M1 只做最小可用 |
| `Source/CoreCombat/Framework/CCPlayerController.h/.cpp` | 输入映射上下文 |
| `Source/CoreCombat/Framework/CCGameMode.h/.cpp` | 默认 Pawn/Controller 指向新类 |
| `Source/CoreCombat/Debug/CCGameplayDebuggerCategory.h/.cpp` | 实时显示 Tag / 属性 |
| `Source/CoreCombatTests/CoreCombatTests.Build.cs` | 测试模块构建规则 |
| `Source/CoreCombatTests/CoreCombatTests.cpp` | 测试模块入口 |
| `Source/CoreCombatTests/CCAttributeSetTest.cpp` | 伤害公式与属性钳制测试 |
| `Source/CoreCombatEditor.Target.cs` | 注册测试模块 |

**删除**：`CoreCombatCharacter.h/.cpp`、`CoreCombatGameMode.h/.cpp`、`CoreCombatPlayerController.h/.cpp`（模板遗留，Task 8 统一清理）

---

### Task 1: 测试模块脚手架

先建测试模块，后续每个任务才有地方写测试。

**Files:**
- Create: `Source/CoreCombatTests/CoreCombatTests.Build.cs`
- Create: `Source/CoreCombatTests/CoreCombatTests.cpp`
- Create: `Source/CoreCombatTests/CCSanityTest.cpp`
- Modify: `Source/CoreCombatEditor.Target.cs`
- Modify: `CoreCombat.uproject`

**Interfaces:**
- Consumes: 无
- Produces: 名为 `CoreCombatTests` 的模块，测试可通过 `UnrealEditor-Cmd.exe` 的 `-ExecCmds="Automation RunTests CoreCombat"` 运行

- [ ] **Step 1: 写验证模块加载的测试**

创建 `Source/CoreCombatTests/CCSanityTest.cpp`：

```cpp
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCSanityTest,
    "CoreCombat.Sanity.ModuleLoads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCSanityTest::RunTest(const FString& Parameters)
{
    // 验证测试模块到主模块的依赖真的接上了：
    // 若 CoreCombatTests.Build.cs 的依赖配置有误，主模块不会被加载。
    TestTrue(TEXT("CoreCombat 主模块已加载"),
        FModuleManager::Get().IsModuleLoaded(TEXT("CoreCombat")));

    TestTrue(TEXT("CoreCombatTests 测试模块已加载"),
        FModuleManager::Get().IsModuleLoaded(TEXT("CoreCombatTests")));

    return true;
}
```

- [ ] **Step 2: 创建模块构建规则**

创建 `Source/CoreCombatTests/CoreCombatTests.Build.cs`：

```csharp
using UnrealBuildTool;

public class CoreCombatTests : ModuleRules
{
	public CoreCombatTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"CoreCombat"
		});
	}
}
```

- [ ] **Step 3: 创建模块入口**

创建 `Source/CoreCombatTests/CoreCombatTests.cpp`：

```cpp
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CoreCombatTests);
```

- [ ] **Step 4: 在 Editor Target 注册测试模块**

修改 `Source/CoreCombatEditor.Target.cs`，在 `ExtraModuleNames.Add("CoreCombat");` 之后添加一行：

```csharp
		ExtraModuleNames.Add("CoreCombatTests");
```

- [ ] **Step 5: 在 .uproject 声明模块**

`Target.cs` 里的 `ExtraModuleNames` 只驱动**编译**，不负责**运行时加载**——UE 的模块管理器是从 `.uproject` 描述文件加载项目模块的。少了这一步，模块能编译出 dll 但不会被加载，测试运行时会报 `No automation tests matched`。

修改 `CoreCombat.uproject`，在 `Modules` 数组中 `CoreCombat` 那一项之后追加：

```json
		{
			"Name": "CoreCombatTests",
			"Type": "Editor",
			"LoadingPhase": "Default"
		}
```

`Type` 必须是 `Editor`：UE 打包时会整体跳过 Editor 类型模块，因此不会进入 Game/Client/Server 目标，对发行版无影响。

- [ ] **Step 6: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 结尾输出 `Total execution time:` 且无 error，`Binaries/Win64/` 下出现 `UnrealEditor-CoreCombatTests.dll`

- [ ] **Step 7: 运行测试**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject" -ExecCmds="Automation RunTests CoreCombat.Sanity;Quit" -unattended -nopause -nullrhi -log
```

Expected: 日志含 `Test Completed. Result={Success}` 与 `CoreCombat.Sanity.ModuleLoads`

- [ ] **Step 8: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombatTests Source/CoreCombatEditor.Target.cs CoreCombat.uproject
git commit -m "test: 建立 CoreCombatTests 自动化测试模块"
```

---

### Task 2: 原生 GameplayTag 声明

**Files:**
- Create: `Source/CoreCombat/AbilitySystem/CCGameplayTags.h`
- Create: `Source/CoreCombat/AbilitySystem/CCGameplayTags.cpp`
- Create: `Source/CoreCombatTests/CCGameplayTagsTest.cpp`

**Interfaces:**
- Consumes: 无
- Produces: 命名空间 `CCTags` 下的 `FNativeGameplayTag` 变量，供后续所有任务引用。M1 阶段声明的变量名：
  `State_Attacking`、`State_Dodging`、`State_HitReacting`、`State_Immobilized`、`State_Dead`、`State_Invincible`、`State_PerfectDodgeWindow`、`Ability_Light`、`Ability_Heavy`、`Ability_Charged`、`Ability_Dodge`、`Ability_Spell_Immobilize`、`Event_Hit_Received`、`Event_Combo_Advance`、`Event_PerfectDodge`、`Buff_PerfectDodgeFollowUp`、`Block_Input`、`Data_Damage`

- [ ] **Step 1: 写失败的测试**

创建 `Source/CoreCombatTests/CCGameplayTagsTest.cpp`：

```cpp
#include "Misc/AutomationTest.h"
#include "AbilitySystem/CCGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCGameplayTagsTest,
    "CoreCombat.Tags.NativeTagsRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCGameplayTagsTest::RunTest(const FString& Parameters)
{
    // FNativeGameplayTag 本身没有 IsValid()，须先 GetTag() 取出 FGameplayTag
    TestTrue(TEXT("State.Attacking 已注册"), CCTags::State_Attacking.GetTag().IsValid());
    TestTrue(TEXT("State.Dead 已注册"), CCTags::State_Dead.GetTag().IsValid());
    TestTrue(TEXT("Buff.PerfectDodgeFollowUp 已注册"), CCTags::Buff_PerfectDodgeFollowUp.GetTag().IsValid());
    TestTrue(TEXT("Data.Damage 已注册"), CCTags::Data_Damage.GetTag().IsValid());

    TestEqual(TEXT("State.Attacking 名称正确"),
        CCTags::State_Attacking.GetTag().GetTagName(), FName("State.Attacking"));

    return true;
}
```

- [ ] **Step 2: 运行测试确认失败**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译失败，报错 `Cannot open include file: 'AbilitySystem/CCGameplayTags.h'`

- [ ] **Step 3: 写头文件**

创建 `Source/CoreCombat/AbilitySystem/CCGameplayTags.h`：

```cpp
#pragma once

#include "NativeGameplayTags.h"

/**
 * 全项目 GameplayTag 的唯一声明处。
 * 用原生 Tag 而非字符串查询，编译期即可捕获拼写错误。
 */
namespace CCTags
{
	// 角色状态
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReacting);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immobilized);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invincible);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_PerfectDodgeWindow);

	// 能力标识
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Light);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Heavy);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Charged);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Spell_Immobilize);

	// 事件
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Received);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_Advance);
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PerfectDodge);

	// 增益
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Buff_PerfectDodgeFollowUp);

	// 输入屏蔽
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Input);

	// SetByCaller 数据通道
	CORECOMBAT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
}
```

- [ ] **Step 4: 写实现文件**

创建 `Source/CoreCombat/AbilitySystem/CCGameplayTags.cpp`：

```cpp
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
```

- [ ] **Step 5: 编译并运行测试**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject" -ExecCmds="Automation RunTests CoreCombat.Tags;Quit" -unattended -nopause -nullrhi -log
```

Expected: 编译通过；测试输出 `Result={Success}`

- [ ] **Step 6: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/AbilitySystem Source/CoreCombatTests/CCGameplayTagsTest.cpp
git commit -m "feat: 建立原生 GameplayTag 声明体系"
```

---

### Task 3: 属性集与伤害结算

这是 M1 最核心的一步，也是自动化测试真正有价值的地方。

**Files:**
- Create: `Source/CoreCombat/AbilitySystem/CCAttributeSet.h`
- Create: `Source/CoreCombat/AbilitySystem/CCAttributeSet.cpp`
- Create: `Source/CoreCombatTests/CCAttributeSetTest.cpp`

**Interfaces:**
- Consumes: `CCTags::State_Dead`（Task 2）
- Produces:
  - `UCCAttributeSet`，属性访问器 `GetHealth() / SetHealth(float) / GetMaxHealth() / ...`，覆盖 `Health`、`MaxHealth`、`Mana`、`MaxMana`、`Stance`、`MaxStance`、`Poise`、`MaxPoise`、`AttackPower`、`DefensePower`、`Damage`（元属性）
  - `static float UCCAttributeSet::CalculateDamage(float RawDamage, float DefensePower)` — 纯静态函数，伤害公式唯一实现，供测试直接调用
  - 广播委托 `FOnAttributeChangedSignature OnHealthChanged` / `OnStanceChanged`，签名 `(float NewValue, float MaxValue)`，供 UI 绑定

- [ ] **Step 1: 写失败的测试**

创建 `Source/CoreCombatTests/CCAttributeSetTest.cpp`：

```cpp
#include "Misc/AutomationTest.h"
#include "AbilitySystem/CCAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCDamageFormulaTest,
    "CoreCombat.Attributes.DamageFormula",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCDamageFormulaTest::RunTest(const FString& Parameters)
{
    // 防御为 0 时，伤害原样穿透
    TestEqual(TEXT("零防御全额承伤"),
        UCCAttributeSet::CalculateDamage(100.f, 0.f), 100.f);

    // 防御按公式 Damage * (100 / (100 + Defense)) 递减
    TestEqual(TEXT("防御 100 时伤害减半"),
        UCCAttributeSet::CalculateDamage(100.f, 100.f), 50.f);

    TestEqual(TEXT("防御 300 时伤害为四分之一"),
        UCCAttributeSet::CalculateDamage(100.f, 300.f), 25.f);

    // 负伤害不允许倒扣血量
    TestEqual(TEXT("负伤害钳制为 0"),
        UCCAttributeSet::CalculateDamage(-50.f, 0.f), 0.f);

    // 防御为负数不应放大伤害到荒谬值
    TestTrue(TEXT("负防御不会导致除零或负伤害"),
        UCCAttributeSet::CalculateDamage(100.f, -500.f) >= 0.f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCAttributeClampTest,
    "CoreCombat.Attributes.Clamping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCAttributeClampTest::RunTest(const FString& Parameters)
{
    // 注意：这里必须用 InitXxx() 而不是 SetXxx()。
    // GAMEPLAYATTRIBUTE_VALUE_SETTER 生成的 SetXxx() 会走
    // GetOwningAbilitySystemComponent()->SetNumericAttributeBase()，
    // 而裸 NewObject 出来的属性集没有宿主 ASC，会触发 ensure 且写入无效。
    // InitXxx() 直接写 BaseValue/CurrentValue，适合纯逻辑测试。
    UCCAttributeSet* Set = NewObject<UCCAttributeSet>();

    Set->InitMaxHealth(200.f);
    Set->InitHealth(500.f);
    Set->ClampAttributes();
    TestEqual(TEXT("血量不超过上限"), Set->GetHealth(), 200.f);

    Set->InitHealth(-30.f);
    Set->ClampAttributes();
    TestEqual(TEXT("血量不低于 0"), Set->GetHealth(), 0.f);

    Set->InitMaxStance(5.f);
    Set->InitStance(9.f);
    Set->ClampAttributes();
    TestEqual(TEXT("棍势不超过上限"), Set->GetStance(), 5.f);

    Set->InitMaxMana(100.f);
    Set->InitMana(-10.f);
    Set->ClampAttributes();
    TestEqual(TEXT("法力不低于 0"), Set->GetMana(), 0.f);

    Set->InitMaxPoise(80.f);
    Set->InitPoise(120.f);
    Set->ClampAttributes();
    TestEqual(TEXT("韧性不超过上限"), Set->GetPoise(), 80.f);

    return true;
}
```

- [ ] **Step 2: 运行测试确认失败**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译失败，报错 `Cannot open include file: 'AbilitySystem/CCAttributeSet.h'`

- [ ] **Step 3: 写头文件**

创建 `Source/CoreCombat/AbilitySystem/CCAttributeSet.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CCAttributeSet.generated.h"

// 模板遗留的 CoreCombatCharacter.h 也定义了同名宏，要到 Task 8 才删除。
// UBT 的 unity build 会把多个 cpp 合并进同一编译单元，两份定义可能撞车，
// 因此这里加 ifndef 保护。Task 8 之后本宏是全项目唯一定义。
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

/** 属性变化广播：当前值 + 上限值，供 UI 绑定 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeChangedSignature, float /*NewValue*/, float /*MaxValue*/);

/**
 * 项目属性集。
 * 伤害是唯一的元属性（Meta Attribute）：GE 只写入 Damage，
 * 真正的减防、扣血、判死全部在 PostGameplayEffectExecute 里完成，
 * 保证伤害公式全局只有一处。
 */
UCLASS()
class CORECOMBAT_API UCCAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCCAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/**
	 * 伤害公式的唯一实现。做成纯静态函数以便直接单测。
	 * 公式：RawDamage * (100 / (100 + Defense))，结果不小于 0。
	 */
	static float CalculateDamage(float RawDamage, float DefensePower);

	/** 把所有属性钳制回合法区间。测试与属性初始化后调用。 */
	void ClampAttributes();

	FOnAttributeChangedSignature OnHealthChanged;
	FOnAttributeChangedSignature OnManaChanged;
	FOnAttributeChangedSignature OnStanceChanged;

	// ---- 血量 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxHealth)

	// ---- 法力（定身术消耗）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxMana)

	// ---- 棍势（轻攻积累、重攻消耗）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Stance;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Stance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData MaxStance;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxStance)

	// ---- 韧性（仅玩家使用，杨戬不设，见 spec §4.5）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Poise;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Poise)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData MaxPoise;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxPoise)

	// ---- 伤害公式乘算项 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, DefensePower)

	// ---- 元属性：只作为 GE 的传输通道，不直接显示 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Damage)
};
```

- [ ] **Step 4: 写实现文件**

创建 `Source/CoreCombat/AbilitySystem/CCAttributeSet.cpp`：

```cpp
#include "AbilitySystem/CCAttributeSet.h"

#include "AbilitySystem/CCGameplayTags.h"
#include "GameplayEffectExtension.h"

UCCAttributeSet::UCCAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitMana(100.f);
	InitMaxMana(100.f);
	InitStance(0.f);
	InitMaxStance(4.f);
	InitPoise(100.f);
	InitMaxPoise(100.f);
	InitAttackPower(10.f);
	InitDefensePower(0.f);
	InitDamage(0.f);
}

float UCCAttributeSet::CalculateDamage(float RawDamage, float DefensePower)
{
	if (RawDamage <= 0.f)
	{
		return 0.f;
	}

	// 防御钳制到非负，避免负防御放大伤害或触发除零
	const float SafeDefense = FMath::Max(0.f, DefensePower);
	const float Mitigation = 100.f / (100.f + SafeDefense);

	return FMath::Max(0.f, RawDamage * Mitigation);
}

void UCCAttributeSet::ClampAttributes()
{
	// 用 InitXxx 而非 SetXxx：InitXxx 直接写 Base/Current 值，
	// 不依赖宿主 ASC，因此在无宿主的单元测试里同样有效。
	InitHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	InitMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	InitStance(FMath::Clamp(GetStance(), 0.f, GetMaxStance()));
	InitPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
}

void UCCAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetStanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStance());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
}

void UCCAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 伤害是元属性：在这里一次性完成减防、扣血、判死，然后清零。
	// 这是全项目唯一的伤害结算入口。
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float RawDamage = GetDamage();
		SetDamage(0.f);

		if (RawDamage <= 0.f)
		{
			return;
		}

		const float FinalDamage = CalculateDamage(RawDamage, GetDefensePower());
		SetHealth(FMath::Clamp(GetHealth() - FinalDamage, 0.f, GetMaxHealth()));

		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());

		if (GetHealth() <= 0.f)
		{
			Data.Target.AddLooseGameplayTag(CCTags::State_Dead.GetTag());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
	}
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
		OnManaChanged.Broadcast(GetMana(), GetMaxMana());
	}
	else if (Data.EvaluatedData.Attribute == GetStanceAttribute())
	{
		SetStance(FMath::Clamp(GetStance(), 0.f, GetMaxStance()));
		OnStanceChanged.Broadcast(GetStance(), GetMaxStance());
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
}
```

- [ ] **Step 5: 编译并运行测试**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject" -ExecCmds="Automation RunTests CoreCombat.Attributes;Quit" -unattended -nopause -nullrhi -log
```

Expected: 两个测试均 `Result={Success}`

- [ ] **Step 6: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/AbilitySystem/CCAttributeSet.h Source/CoreCombat/AbilitySystem/CCAttributeSet.cpp Source/CoreCombatTests/CCAttributeSetTest.cpp
git commit -m "feat: 属性集与统一伤害结算入口"
```

---

### Task 4: ASC 子类

**Files:**
- Create: `Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.h`
- Create: `Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.cpp`

**Interfaces:**
- Consumes: `CCTags`（Task 2）
- Produces: `UCCAbilitySystemComponent`，方法 `bool HasMatchingTag(const FGameplayTag& Tag) const`、`void AddStateTag(const FGameplayTag& Tag)`、`void RemoveStateTag(const FGameplayTag& Tag)`。M2 会在此类上增加输入缓冲。

- [ ] **Step 1: 写头文件**

创建 `Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CCAbilitySystemComponent.generated.h"

/**
 * 项目 ASC。
 * M1 只封装 Tag 读写；M2 会在此加入跨能力存活的输入缓冲
 * （缓冲必须放在 ASC 而非 Ability，否则闪避取消攻击时会随能力一起销毁）。
 */
UCLASS()
class CORECOMBAT_API UCCAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UCCAbilitySystemComponent();

	/** 查询是否持有指定 Tag */
	UFUNCTION(BlueprintCallable, Category = "CoreCombat|Tags")
	bool HasMatchingTag(const FGameplayTag& Tag) const;

	/** 添加松散状态 Tag */
	UFUNCTION(BlueprintCallable, Category = "CoreCombat|Tags")
	void AddStateTag(const FGameplayTag& Tag);

	/** 移除松散状态 Tag */
	UFUNCTION(BlueprintCallable, Category = "CoreCombat|Tags")
	void RemoveStateTag(const FGameplayTag& Tag);
};
```

- [ ] **Step 2: 写实现文件**

创建 `Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.cpp`：

```cpp
#include "AbilitySystem/CCAbilitySystemComponent.h"

UCCAbilitySystemComponent::UCCAbilitySystemComponent()
{
	// 单机 Demo，不做网络复制
	SetIsReplicated(false);
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

bool UCCAbilitySystemComponent::HasMatchingTag(const FGameplayTag& Tag) const
{
	return HasMatchingGameplayTag(Tag);
}

void UCCAbilitySystemComponent::AddStateTag(const FGameplayTag& Tag)
{
	AddLooseGameplayTag(Tag);
}

void UCCAbilitySystemComponent::RemoveStateTag(const FGameplayTag& Tag)
{
	RemoveLooseGameplayTag(Tag);
}
```

- [ ] **Step 3: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 4: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.h Source/CoreCombat/AbilitySystem/CCAbilitySystemComponent.cpp
git commit -m "feat: ASC 子类与 Tag 读写封装"
```

---

### Task 5: 角色初始化数据资产

**Files:**
- Create: `Source/CoreCombat/Data/CCCharacterInitData.h`
- Create: `Source/CoreCombat/Data/CCCharacterInitData.cpp`

**Interfaces:**
- Consumes: 无
- Produces: `UCCCharacterInitData`，公开字段 `MaxHealth`、`MaxMana`、`MaxStance`、`MaxPoise`、`AttackPower`、`DefensePower`（均为 `float`）与 `TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities`。Task 6 的 `CCCharacterBase` 读取它初始化属性。

- [ ] **Step 1: 写头文件**

创建 `Source/CoreCombat/Data/CCCharacterInitData.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/GameplayAbility.h"
#include "CCCharacterInitData.generated.h"

/**
 * 角色初始属性与默认能力集。
 * 玩家与每个 BOSS 各一份资产，改数值不重编译。
 */
UCLASS(BlueprintType)
class CORECOMBAT_API UCCCharacterInitData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Vital")
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Vital")
	float MaxMana = 100.f;

	/** 棍势上限，黑神话中为 4 层 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float MaxStance = 4.f;

	/** 韧性上限。杨戬不使用韧性（见 spec §4.5），其资产此项可保持默认 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float MaxPoise = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackPower = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float DefensePower = 0.f;

	/** 角色启动时自动授予的能力 */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
};
```

- [ ] **Step 2: 写实现文件**

创建 `Source/CoreCombat/Data/CCCharacterInitData.cpp`：

```cpp
#include "Data/CCCharacterInitData.h"
```

- [ ] **Step 3: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 4: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/Data
git commit -m "feat: 角色初始化数据资产"
```

---

### Task 6: 角色基类与派生类

模板类里那个 ASC 指针从未 `CreateDefaultSubobject`，这一步把它换成真正能用的。

**Files:**
- Create: `Source/CoreCombat/Character/CCCharacterBase.h`
- Create: `Source/CoreCombat/Character/CCCharacterBase.cpp`
- Create: `Source/CoreCombat/Character/CCPlayerCharacter.h`
- Create: `Source/CoreCombat/Character/CCPlayerCharacter.cpp`
- Create: `Source/CoreCombat/Character/CCEnemyCharacter.h`
- Create: `Source/CoreCombat/Character/CCEnemyCharacter.cpp`

**Interfaces:**
- Consumes: `UCCAbilitySystemComponent`（Task 4）、`UCCAttributeSet`（Task 3）、`UCCCharacterInitData`（Task 5）
- Produces:
  - `ACCCharacterBase`：`UAbilitySystemComponent* GetAbilitySystemComponent() const` override、`UCCAttributeSet* GetCCAttributeSet() const`、`bool IsAlive() const`、protected 虚函数 `virtual void InitializeAttributes()` 与 `virtual void GrantDefaultAbilities()`
  - `ACCPlayerCharacter`：`USpringArmComponent* GetCameraBoom() const`、`UCameraComponent* GetFollowCamera() const`、`virtual void DoMove(float Right, float Forward)`、`virtual void DoLook(float Yaw, float Pitch)`
  - `ACCEnemyCharacter`：M1 阶段仅继承，无新增公开成员

- [ ] **Step 1: 写基类头文件**

创建 `Source/CoreCombat/Character/CCCharacterBase.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "CCCharacterBase.generated.h"

class UCCAbilitySystemComponent;
class UCCAttributeSet;
class UCCCharacterInitData;

/**
 * 玩家与 BOSS 的共同基类。
 * 持有真正初始化的 ASC 与 AttributeSet，属性初值来自数据资产。
 */
UCLASS(abstract)
class CORECOMBAT_API ACCCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACCCharacterBase();

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	UFUNCTION(BlueprintPure, Category = "CoreCombat")
	UCCAttributeSet* GetCCAttributeSet() const { return AttributeSet; }

	/** 未持有 State.Dead 且血量大于 0 */
	UFUNCTION(BlueprintPure, Category = "CoreCombat")
	bool IsAlive() const;

protected:
	virtual void BeginPlay() override;

	/** 用 InitData 里的数值写入属性集 */
	virtual void InitializeAttributes();

	/** 授予 InitData 里列出的默认能力 */
	virtual void GrantDefaultAbilities();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CoreCombat")
	TObjectPtr<UCCAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UCCAttributeSet> AttributeSet;

	/** 初始属性与默认能力，在蓝图里指定 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CoreCombat")
	TObjectPtr<UCCCharacterInitData> InitData;
};
```

- [ ] **Step 2: 写基类实现**

创建 `Source/CoreCombat/Character/CCCharacterBase.cpp`：

```cpp
#include "Character/CCCharacterBase.h"

#include "AbilitySystem/CCAbilitySystemComponent.h"
#include "AbilitySystem/CCAttributeSet.h"
#include "AbilitySystem/CCGameplayTags.h"
#include "Data/CCCharacterInitData.h"

ACCCharacterBase::ACCCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UCCAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UCCAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ACCCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool ACCCharacterBase::IsAlive() const
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return false;
	}

	return !AbilitySystemComponent->HasMatchingTag(CCTags::State_Dead.GetTag())
		&& AttributeSet->GetHealth() > 0.f;
}

void ACCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	InitializeAttributes();
	GrantDefaultAbilities();
}

void ACCCharacterBase::InitializeAttributes()
{
	if (!InitData || !AttributeSet)
	{
		return;
	}

	// 用 InitXxx 写初值：直接设置 Base/Current，不触发 PreAttributeChange，
	// 因此不会出现"用旧上限钳制新血量"的顺序陷阱。
	// 但仍先写上限，保持语义清晰。
	AttributeSet->InitMaxHealth(InitData->MaxHealth);
	AttributeSet->InitMaxMana(InitData->MaxMana);
	AttributeSet->InitMaxStance(InitData->MaxStance);
	AttributeSet->InitMaxPoise(InitData->MaxPoise);

	AttributeSet->InitHealth(InitData->MaxHealth);
	AttributeSet->InitMana(InitData->MaxMana);
	AttributeSet->InitStance(0.f);
	AttributeSet->InitPoise(InitData->MaxPoise);

	AttributeSet->InitAttackPower(InitData->AttackPower);
	AttributeSet->InitDefensePower(InitData->DefensePower);

	AttributeSet->ClampAttributes();
}

void ACCCharacterBase::GrantDefaultAbilities()
{
	if (!InitData || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : InitData->DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
}
```

- [ ] **Step 3: 写玩家角色头文件**

创建 `Source/CoreCombat/Character/CCPlayerCharacter.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Character/CCCharacterBase.h"
#include "CCPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

/**
 * 玩家控制的悟空。
 * M1 只做相机臂与基础移动；锁定与 strafe 在 M5。
 */
UCLASS(abstract)
class CORECOMBAT_API ACCPlayerCharacter : public ACCCharacterBase
{
	GENERATED_BODY()

public:
	ACCPlayerCharacter();

	UFUNCTION(BlueprintPure, Category = "CoreCombat|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "CoreCombat|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintCallable, Category = "CoreCombat|Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "CoreCombat|Input")
	virtual void DoLook(float Yaw, float Pitch);

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MouseLookAction;
};
```

- [ ] **Step 4: 写玩家角色实现**

创建 `Source/CoreCombat/Character/CCPlayerCharacter.cpp`：

```cpp
#include "Character/CCPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "CoreCombat.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

ACCPlayerCharacter::ACCPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ACCPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACCPlayerCharacter::Move);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACCPlayerCharacter::Look);
		EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACCPlayerCharacter::Look);
	}
	else
	{
		UE_LOG(LogCoreCombat, Error,
			TEXT("'%s' 未找到 EnhancedInputComponent，输入将不可用"), *GetNameSafe(this));
	}
}

void ACCPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACCPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACCPlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() == nullptr)
	{
		return;
	}

	const FRotator YawRotation(0.f, GetController()->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void ACCPlayerCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() == nullptr)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}
```

- [ ] **Step 5: 写敌人角色**

创建 `Source/CoreCombat/Character/CCEnemyCharacter.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Character/CCCharacterBase.h"
#include "CCEnemyCharacter.generated.h"

/**
 * AI 控制的敌人基类。
 * M1 阶段仅继承基类；StateTree 与决策表在 M6。
 */
UCLASS(abstract)
class CORECOMBAT_API ACCEnemyCharacter : public ACCCharacterBase
{
	GENERATED_BODY()

public:
	ACCEnemyCharacter();
};
```

创建 `Source/CoreCombat/Character/CCEnemyCharacter.cpp`：

```cpp
#include "Character/CCEnemyCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

ACCEnemyCharacter::ACCEnemyCharacter()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
}
```

- [ ] **Step 6: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 7: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/Character
git commit -m "feat: 角色基类与玩家/敌人派生类，ASC 真正初始化"
```

---

### Task 7: GameMode 与 PlayerController

**Files:**
- Create: `Source/CoreCombat/Framework/CCPlayerController.h`
- Create: `Source/CoreCombat/Framework/CCPlayerController.cpp`
- Create: `Source/CoreCombat/Framework/CCGameMode.h`
- Create: `Source/CoreCombat/Framework/CCGameMode.cpp`

**Interfaces:**
- Consumes: `ACCPlayerCharacter`（Task 6）
- Produces: `ACCPlayerController`（含 `TArray<UInputMappingContext*> DefaultMappingContexts`）、`ACCGameMode`

- [ ] **Step 1: 写 PlayerController**

创建 `Source/CoreCombat/Framework/CCPlayerController.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CCPlayerController.generated.h"

class UInputMappingContext;

/**
 * 负责挂载 Enhanced Input 的映射上下文。
 * 模板里的移动端触屏控件已移除——本项目只面向 Win64。
 */
UCLASS(abstract)
class CORECOMBAT_API ACCPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Input|Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
};
```

创建 `Source/CoreCombat/Framework/CCPlayerController.cpp`：

```cpp
#include "Framework/CCPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

void ACCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			for (UInputMappingContext* Context : DefaultMappingContexts)
			{
				if (Context)
				{
					Subsystem->AddMappingContext(Context, 0);
				}
			}
		}
	}
}
```

- [ ] **Step 2: 写 GameMode**

创建 `Source/CoreCombat/Framework/CCGameMode.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CCGameMode.generated.h"

/**
 * BOSS 战 GameMode。
 * M7 会在此加入胜负结算。
 */
UCLASS(abstract)
class CORECOMBAT_API ACCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACCGameMode();
};
```

创建 `Source/CoreCombat/Framework/CCGameMode.cpp`：

```cpp
#include "Framework/CCGameMode.h"

#include "Framework/CCPlayerController.h"

ACCGameMode::ACCGameMode()
{
	PlayerControllerClass = ACCPlayerController::StaticClass();
	// DefaultPawnClass 在派生蓝图 GM_WuKong 里指向 BP_WuKong，
	// 避免 C++ 直接引用 Content 资产
}
```

- [ ] **Step 3: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 4: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/Framework
git commit -m "feat: GameMode 与 PlayerController"
```

---

### Task 8: 移除模板遗留类

必须在 Task 6/7 之后做——新类先就位，再拆旧的。

**Files:**
- Delete: `Source/CoreCombat/CoreCombatCharacter.h`
- Delete: `Source/CoreCombat/CoreCombatCharacter.cpp`
- Delete: `Source/CoreCombat/CoreCombatGameMode.h`
- Delete: `Source/CoreCombat/CoreCombatGameMode.cpp`
- Delete: `Source/CoreCombat/CoreCombatPlayerController.h`
- Delete: `Source/CoreCombat/CoreCombatPlayerController.cpp`
- Modify: `Config/DefaultEngine.ini`

**Interfaces:**
- Consumes: Task 6、Task 7 的新类必须已编译通过
- Produces: 无

- [ ] **Step 1: 确认没有残留引用**

```bash
cd F:/10_Projects/CoreCombat
grep -rn "CoreCombatCharacter\|CoreCombatGameMode\|CoreCombatPlayerController" Source/ --include=*.h --include=*.cpp --include=*.cs
```

Expected: 只匹配到即将删除的那 6 个文件自身；若匹配到其他文件，先改掉其中的引用

- [ ] **Step 2: 删除模板文件**

```bash
cd F:/10_Projects/CoreCombat
git rm Source/CoreCombat/CoreCombatCharacter.h Source/CoreCombat/CoreCombatCharacter.cpp \
       Source/CoreCombat/CoreCombatGameMode.h Source/CoreCombat/CoreCombatGameMode.cpp \
       Source/CoreCombat/CoreCombatPlayerController.h Source/CoreCombat/CoreCombatPlayerController.cpp
```

- [ ] **Step 3: 更新类重定向**

修改 `Config/DefaultEngine.ini`，把这一行：

```ini
+ActiveClassRedirects=(OldClassName="TP_ThirdPersonGameMode",NewClassName="CoreCombatGameMode")
```

替换为下面三行（让已存在的蓝图资产能找到新类，而不是加载时报缺失）：

```ini
+ActiveClassRedirects=(OldClassName="TP_ThirdPersonGameMode",NewClassName="CCGameMode")
+ActiveClassRedirects=(OldClassName="CoreCombatGameMode",NewClassName="CCGameMode")
+ActiveClassRedirects=(OldClassName="CoreCombatPlayerController",NewClassName="CCPlayerController")
```

注意：`CoreCombatCharacter` **不做**重定向。它到 `ACCPlayerCharacter` 的迁移在 Task 10 由编辑器内手动重新父类化完成——自动重定向会绕过父类变更时的属性重映射，反而制造隐蔽的数据丢失。

- [ ] **Step 4: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 5: 跑一遍全部测试确认没被破坏**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject" -ExecCmds="Automation RunTests CoreCombat;Quit" -unattended -nopause -nullrhi -log
```

Expected: 全部测试 `Result={Success}`

- [ ] **Step 6: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add -A
git commit -m "refactor: 移除 ThirdPerson 模板遗留类"
```

---

### Task 9: Gameplay Debugger 分类

没有这个，后面调战斗等于闭眼开车。

**Files:**
- Modify: `Source/CoreCombat/CoreCombat.Build.cs`
- Create: `Source/CoreCombat/Debug/CCGameplayDebuggerCategory.h`
- Create: `Source/CoreCombat/Debug/CCGameplayDebuggerCategory.cpp`
- Modify: `Source/CoreCombat/CoreCombat.cpp`

**Interfaces:**
- Consumes: `ACCCharacterBase`（Task 6）、`UCCAttributeSet`（Task 3）
- Produces: 名为 `CoreCombat` 的 Gameplay Debugger 分类，游戏中按 `'` 呼出后可切换

- [ ] **Step 1: 增加模块依赖**

修改 `Source/CoreCombat/CoreCombat.Build.cs`，在 `PublicDependencyModuleNames` 的 `"GameplayTasks"` 之后添加一行，并在其下方追加条件依赖块：

```csharp
			"GameplayTasks"
		});

		if (Target.bBuildDeveloperTools ||
		    (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
		{
			PrivateDependencyModuleNames.Add("GameplayDebugger");
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_CC=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER_CC=0");
		}
```

- [ ] **Step 2: 写 debugger 分类头文件**

创建 `Source/CoreCombat/Debug/CCGameplayDebuggerCategory.h`：

```cpp
#pragma once

#if WITH_GAMEPLAY_DEBUGGER_CC

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

/**
 * 战斗调试视图：实时显示属性数值与当前持有的 Tag。
 * 游戏中按 ' 呼出 Gameplay Debugger，再切到 CoreCombat 分类。
 */
class FCCGameplayDebuggerCategory : public FGameplayDebuggerCategory
{
public:
	FCCGameplayDebuggerCategory();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

protected:
	struct FRepData
	{
		FString ActorName;
		float Health = 0.f;
		float MaxHealth = 0.f;
		float Mana = 0.f;
		float MaxMana = 0.f;
		float Stance = 0.f;
		float MaxStance = 0.f;
		float Poise = 0.f;
		float MaxPoise = 0.f;
		FString ActiveTags;

		void Serialize(FArchive& Ar);
	};

	FRepData DataPack;
};

#endif // WITH_GAMEPLAY_DEBUGGER_CC
```

- [ ] **Step 3: 写 debugger 分类实现**

创建 `Source/CoreCombat/Debug/CCGameplayDebuggerCategory.cpp`：

```cpp
#include "Debug/CCGameplayDebuggerCategory.h"

#if WITH_GAMEPLAY_DEBUGGER_CC

#include "AbilitySystem/CCAbilitySystemComponent.h"
#include "AbilitySystem/CCAttributeSet.h"
#include "Character/CCCharacterBase.h"
#include "GameFramework/PlayerController.h"

FCCGameplayDebuggerCategory::FCCGameplayDebuggerCategory()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FCCGameplayDebuggerCategory::MakeInstance()
{
	return MakeShareable(new FCCGameplayDebuggerCategory());
}

void FCCGameplayDebuggerCategory::FRepData::Serialize(FArchive& Ar)
{
	Ar << ActorName;
	Ar << Health << MaxHealth;
	Ar << Mana << MaxMana;
	Ar << Stance << MaxStance;
	Ar << Poise << MaxPoise;
	Ar << ActiveTags;
}

void FCCGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	// 优先看被选中的 Actor，没选就看玩家自己
	AActor* Target = DebugActor;
	if (Target == nullptr && OwnerPC != nullptr)
	{
		Target = OwnerPC->GetPawn();
	}

	const ACCCharacterBase* Character = Cast<ACCCharacterBase>(Target);
	if (Character == nullptr)
	{
		DataPack = FRepData();
		DataPack.ActorName = TEXT("<无 CoreCombat 角色>");
		return;
	}

	DataPack.ActorName = GetNameSafe(Character);

	if (const UCCAttributeSet* Attributes = Character->GetCCAttributeSet())
	{
		DataPack.Health = Attributes->GetHealth();
		DataPack.MaxHealth = Attributes->GetMaxHealth();
		DataPack.Mana = Attributes->GetMana();
		DataPack.MaxMana = Attributes->GetMaxMana();
		DataPack.Stance = Attributes->GetStance();
		DataPack.MaxStance = Attributes->GetMaxStance();
		DataPack.Poise = Attributes->GetPoise();
		DataPack.MaxPoise = Attributes->GetMaxPoise();
	}

	DataPack.ActiveTags.Reset();
	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		for (const FGameplayTag& Tag : OwnedTags)
		{
			DataPack.ActiveTags += FString::Printf(TEXT("%s  "), *Tag.ToString());
		}

		if (DataPack.ActiveTags.IsEmpty())
		{
			DataPack.ActiveTags = TEXT("<无>");
		}
	}
}

void FCCGameplayDebuggerCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("{yellow}角色: {white}%s"), *DataPack.ActorName);
	CanvasContext.Printf(TEXT("{yellow}血量: {white}%.1f / %.1f"), DataPack.Health, DataPack.MaxHealth);
	CanvasContext.Printf(TEXT("{yellow}法力: {white}%.1f / %.1f"), DataPack.Mana, DataPack.MaxMana);
	CanvasContext.Printf(TEXT("{yellow}棍势: {white}%.1f / %.1f"), DataPack.Stance, DataPack.MaxStance);
	CanvasContext.Printf(TEXT("{yellow}韧性: {white}%.1f / %.1f"), DataPack.Poise, DataPack.MaxPoise);
	CanvasContext.Printf(TEXT("{yellow}Tags: {white}%s"), *DataPack.ActiveTags);
}

#endif // WITH_GAMEPLAY_DEBUGGER_CC
```

- [ ] **Step 4: 在模块启动时注册**

原 `CoreCombat.cpp` 中含有 `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ...)` 与 `DEFINE_LOG_CATEGORY(LogCoreCombat)`，下面的替换版本已把两者都保留（模块实现类换成了自定义的 `FCoreCombatModule`）。

把 `Source/CoreCombat/CoreCombat.cpp` 整体替换为：

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreCombat.h"

#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER_CC
#include "Debug/CCGameplayDebuggerCategory.h"
#include "GameplayDebugger.h"
#endif

/**
 * 项目主模块。注册 Gameplay Debugger 分类。
 */
class FCoreCombatModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER_CC
		IGameplayDebugger& DebuggerModule = IGameplayDebugger::Get();
		DebuggerModule.RegisterCategory(
			TEXT("CoreCombat"),
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FCCGameplayDebuggerCategory::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
		DebuggerModule.NotifyCategoriesChanged();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER_CC
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger::Get().UnregisterCategory(TEXT("CoreCombat"));
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCoreCombatModule, CoreCombat, "CoreCombat");

DEFINE_LOG_CATEGORY(LogCoreCombat);
```

- [ ] **Step 5: 编译**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
```

Expected: 编译通过，无 error

- [ ] **Step 6: 提交**

```bash
cd F:/10_Projects/CoreCombat
git add Source/CoreCombat/Debug Source/CoreCombat/CoreCombat.cpp Source/CoreCombat/CoreCombat.Build.cs
git commit -m "feat: Gameplay Debugger 战斗调试分类"
```

---

### Task 10: 编辑器内接线与手动验收

这一步在 Unreal Editor 里做，无法自动化。按顺序执行，每步都确认结果。

**Files:**
- Modify: `Content/Wukong/BluePrints/BP_WuKong.uasset`（编辑器内操作）
- Modify: `Content/Wukong/BluePrints/BP_WuKongPlayerController.uasset`（编辑器内操作）
- Modify: `Content/Wukong/BluePrints/GM_WuKong.uasset`（编辑器内操作）
- Create: `Content/CoreCombat/Data/DA_CharInit_Wukong.uasset`（编辑器内操作）
- Modify: `Config/DefaultEngine.ini`

**Interfaces:**
- Consumes: Task 1–9 全部产出
- Produces: 一个可 PIE 运行、ASC 已初始化、属性可在 debugger 中看到的悟空角色

**注意**：Content 不入版本控制（见 `.gitignore`），本任务的资产改动只提交 `Config/DefaultEngine.ini`。

- [ ] **Step 1: 打开编辑器**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject"
```

Expected: 编辑器正常启动。若弹出「缺失类」提示指向已删除的模板类，记录具体类名后继续——Step 2/3 会修复。

- [ ] **Step 2: 重新父类化 BP_WuKong**

在内容浏览器打开 `Content/Wukong/BluePrints/BP_WuKong`：
1. 菜单 `File → Reparent Blueprint`
2. 选择 `CCPlayerCharacter`
3. 编译并保存

Expected: 编译成功。细节面板 `CoreCombat` 分类下出现 `Init Data` 字段；组件树中出现 `AbilitySystemComponent`。

- [ ] **Step 3: 重新父类化另外两个蓝图**

- `BP_WuKongPlayerController` → 父类改为 `CCPlayerController`
- `GM_WuKong` → 父类改为 `CCGameMode`

各自编译并保存。

Expected: 均编译成功。`BP_WuKongPlayerController` 的细节面板中出现 `Default Mapping Contexts` 数组。

- [ ] **Step 4: 创建初始化数据资产**

1. 在内容浏览器新建目录 `Content/CoreCombat/Data`
2. 右键 → `Miscellaneous → Data Asset`
3. 类选择 `CCCharacterInitData`，命名 `DA_CharInit_Wukong`
4. 填入数值：`MaxHealth=200`、`MaxMana=100`、`MaxStance=4`、`MaxPoise=100`、`AttackPower=20`、`DefensePower=10`
5. 保存

- [ ] **Step 5: 把数据资产挂到角色上**

打开 `BP_WuKong`，在细节面板 `CoreCombat → Init Data` 处选择 `DA_CharInit_Wukong`。编译并保存。

- [ ] **Step 6: 确认输入映射仍然挂载**

打开 `BP_WuKongPlayerController`，确认 `Default Mapping Contexts` 数组中含有项目原有的 IMC 资产。若为空（重新父类化时属性未继承过来），手动添加 `Content/ThirdPerson/Input/` 下的 `IMC_Default`。

同时打开 `BP_WuKong`，确认 `Input` 分类下的 `Move Action` / `Look Action` / `Mouse Look Action` 均已指向对应的 `IA_*` 资产；若为空则手动指定。

- [ ] **Step 7: 设置默认地图与 GameMode**

`Edit → Project Settings → Maps & Modes`：
- `Default GameMode` 设为 `GM_WuKong`
- `Editor Startup Map` 与 `Game Default Map` 暂时保持 `Lvl_ThirdPerson`（`L_BossArena` 在 M6 创建）

保存项目设置。

- [ ] **Step 8: PIE 验收**

点击 Play，逐项确认：

| 检查项 | 期望 |
|---|---|
| 角色出现在关卡中 | 悟空模型可见，非默认小白人 |
| WASD 移动 | 角色移动，朝向跟随移动方向 |
| 鼠标视角 | 相机绕角色旋转 |
| 输出日志 | 无 `LogCoreCombat: Error`，无 ASC 相关 warning |

- [ ] **Step 9: Gameplay Debugger 验收**

PIE 中按 `'`（撇号）呼出 Gameplay Debugger，用数字键切换到 `CoreCombat` 分类。

Expected 屏幕显示：

```
角色: BP_WuKong_C_0
血量: 200.0 / 200.0
法力: 100.0 / 100.0
棍势: 0.0 / 4.0
韧性: 100.0 / 100.0
Tags: <无>
```

若血量显示 `100.0 / 100.0` 而非 `200.0`，说明 `Init Data` 未挂载或 `InitializeAttributes` 未被调用——回到 Step 5 检查。

- [ ] **Step 10: 关闭编辑器并提交配置**

```bash
cd F:/10_Projects/CoreCombat
git add Config/DefaultEngine.ini
git commit -m "chore: 默认 GameMode 指向 GM_WuKong"
```

若 `git status` 显示 `Config/DefaultEngine.ini` 无改动（项目设置写入了其他文件），改为：

```bash
cd F:/10_Projects/CoreCombat
git status --short Config/
git add Config/
git commit -m "chore: 更新项目配置以适配新类层级"
```

- [ ] **Step 11: 最终全量验证**

```bash
"F:/20_Areas/GameDev/UE_5.8/Engine/Build/BatchFiles/Build.bat" CoreCombatEditor Win64 Development -Project="F:/10_Projects/CoreCombat/CoreCombat.uproject" -WaitMutex
"F:/20_Areas/GameDev/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/10_Projects/CoreCombat/CoreCombat.uproject" -ExecCmds="Automation RunTests CoreCombat;Quit" -unattended -nopause -nullrhi -log
```

Expected: 编译无 error；全部测试 `Result={Success}`；日志末尾无 `Test Completed. Result={Fail}`

---

## M1 完成标准

- [ ] 项目编译通过，无 error
- [ ] 4 个自动化测试全部通过（Sanity、Tags、DamageFormula、Clamping）
- [ ] `BP_WuKong` 父类为 `ACCPlayerCharacter`，PIE 中可移动可转视角
- [ ] Gameplay Debugger 中能看到血量 200/200、棍势 0/4
- [ ] ThirdPerson 模板类已从 `Source/` 中彻底移除
- [ ] git 历史中有 9 个语义清晰的 commit

---

## 后续里程碑预告

M2 将在此基础上：编写 Python 脚本批量生成 Montage、实现 `UCCComboDataAsset` 与 `UCCGA_ComboAttack`、在 `UCCAbilitySystemComponent` 中加入跨能力输入缓冲、实现 `CCANS_ComboWindow` 与 `CCANS_HitBox`。M2 的测试重点是招式表推进逻辑（`Step[N] → Step[N+1]`、末段回绕、空表不崩溃）。
