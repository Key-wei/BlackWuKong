# 黑神话悟空 BOSS 战 Demo — 设计文档

- **日期**：2026-09-02
- **项目**：CoreCombat（UE 5.8）
- **目标**：以黑神话悟空资产为皮，构建一套数据驱动的 GAS 战斗框架，交付一场完整的「悟空 vs 杨戬」BOSS 战

---

## 1. 背景与动机

**首要目标是学习 UE 战斗框架架构**，而非最快出画面。因此在「代码质量与可扩展性」和「出效果速度」之间，一律倒向前者：招式是数据不是代码，状态互斥靠 Tag 不靠 if，伤害公式只有一处。

Demo 完成态定义为：一张竞技场关卡 + 可操控悟空（移动/闪避/锁定/轻重连招/蓄力/定身术）+ 两阶段杨戬 BOSS + 血条棍势条 + 胜负结算。

### 现有资产盘点

| 项 | 状态 |
|---|---|
| C++ 模块 `CoreCombat` | 410 行，ThirdPerson 模板原样 + 一个**未初始化**的裸 `UAbilitySystemComponent*` 指针 |
| 插件 | GameplayAbilities / StateTree / GameplayStateTree 已启用 |
| 悟空资产 | SkeletalMesh + Skeleton + PhysicsAsset + **693 个 AnimSequence**，骨架身份问题已修复，动画可正常预览 |
| 杨戬资产 | 数百个 `atk` / `bh_*` 受击 / `bianshen` 变身 / `buff` / `die`，完整度最高 |
| 已有蓝图 | `BP_WuKong` / `ABP_WuKong` / `BS_WuKong_Locomotion` / `GM_WuKong`（内容较薄） |
| git | **仓库无任何 commit** |
| 默认地图 | `Lvl_ThirdPerson`（模板遗留，将替换） |

---

## 2. 关键决策

| 决策点 | 选择 | 理由 |
|---|---|---|
| 能力底层 | **GAS 核心子集** | ASC + AttributeSet + Ability + Effect + Tag。不做网络预测/复制——单机 Demo 下是过度工程 |
| 招式组织 | **单一连招 Ability + 数据资产招式表** | 693 个动画的体量下，招式必须是可填表的数据。每招一个 Ability 类会膨胀成几十个复制粘贴类 |
| BOSS | **杨戬** | 唯一动画完整度足以支撑两阶段完整招式的角色，零骨骼重定向风险 |
| 手感 | **黑神话风：轻攻连段 + 棍势 + 闪身** | 棍势资源条正好把 GAS 的 Attribute / Cost / Effect 三套机制练全 |
| 法术 | **仅定身术** | 一个做透 > 三个做浅。定身术是 GameplayEffect + Tag 阻断 AI 的教科书案例 |
| BOSS 硬直 | **单向（BOSS 不被打断）** | 保持 BOSS 压迫感，见 §4.5 |
| 蓄力攻击 | **纳入第一版** | 资产齐全（`xuli_start/loop/attack_1..3`），机制上是重攻变体，边际成本低 |
| AI | **StateTree** | BOSS 战本质是状态机（阶段/变身/死亡），行为树表达「阶段」需靠黑板硬拗；StateTree 亦是 Epic 主推方向 |
| 模板类 | **替换 + 重新父类化 BP_WuKong** | BP_WuKong 内容薄、仓库无 commit，此刻迁移成本最低 |

---

## 3. 架构

### 3.1 分层

四层，单向依赖，不反向引用：

```
输入层    PlayerController → Enhanced Input → GameplayTag 事件
能力层    ASC + GameplayAbility（激活/取消/Cost/Cooldown/Tag 互斥）
数据层    ComboDataAsset（招式表）+ AttributeSet（属性）
表现层    Montage + AnimNotify（命中盒、连招窗口、特效）
```

**核心约束：能力层不认识具体招式。** `UCCGA_ComboAttack` 只知道「播第 N 段」，第 N 段是什么由数据资产回答。这是数据驱动方案成立的前提。

### 3.2 目录与类

```
Source/CoreCombat/
├─ Framework/          CCGameMode, CCPlayerController
├─ Character/
│    CCCharacterBase        ACharacter + IAbilitySystemInterface，玩家与 BOSS 共同基类
│    CCPlayerCharacter      相机臂、输入、锁定
│    CCEnemyCharacter       AI 侧扩展
├─ AbilitySystem/
│    CCAbilitySystemComponent   输入缓冲、Tag 查询封装
│    CCAttributeSet             Health / Mana / Stance / Poise / Attack / Defense
│    CCGameplayTags             全项目 Tag 唯一声明处（原生 Tag，编译期防拼写错）
│    Abilities/
│      CCGameplayAbility        项目能力基类
│      CCGA_ComboAttack         轻攻击连段（数据驱动）
│      CCGA_HeavyAttack         重攻击 / 蓄力，消耗棍势
│      CCGA_Dodge               闪避 + 闪身判定
│      CCGA_Immobilize          定身术
│      CCGA_HitReact / CCGA_Death
├─ Data/
│    CCComboDataAsset       招式表（玩家一份、BOSS 一份）
│    CCCharacterInitData    初始属性、默认能力集
├─ Animation/
│    CCANS_HitBox           命中盒开关窗口
│    CCANS_ComboWindow      连招输入窗口
│    CCANS_Invincible       无敌帧 / 闪身窗口
├─ Combat/
│    CCLockOnComponent      锁定目标、镜头跟随
│    CCHitDetector          扫掠检测 + 去重
├─ AI/
│    CCBossController       StateTree 宿主
│    CCSTTask_ActivateAbility / CCSTTask_MoveToTarget / CCSTTask_Wait
└─ UI/                      HUD、血条、棍势条
```

### 3.3 模块边界

- `AbilitySystem` 不依赖 `AI` 与 `UI`
- `UI` 只通过 AttributeSet 的属性变化委托读取状态，**不 Tick 轮询**，不反向调用能力
- `AI` 通过 ASC 激活能力，不直接操作角色移动或播放动画
- `Data` 是纯数据，不含逻辑

---

## 4. 战斗核心机制

### 4.1 属性（CCAttributeSet）

| 属性 | 作用 | 玩家 | 杨戬 |
|---|---|---|---|
| `Health` / `MaxHealth` | 血量 | ✅ | ✅ |
| `Mana` / `MaxMana` | 法力，定身术消耗 | ✅ | — |
| `Stance` / `MaxStance` | 棍势，轻攻积累、重攻消耗 | ✅ | — |
| `Poise` / `MaxPoise` | 韧性，归零进入硬直 | ✅ | — |
| `AttackPower` / `DefensePower` | 伤害公式乘算项 | ✅ | ✅ |

韧性独立于血量，因其恢复规则不同（脱战后快速回满）。杨戬不设 `Poise`，见 §4.5。

**伤害统一走 `PostGameplayEffectExecute`**：GE 只携带 `Damage` 元属性，在 `PostGameplayEffectExecute` 中执行减防、扣血、判死。伤害公式全局唯一，后续插入 Buff / 减伤有单一切入点。

### 4.2 Tag 体系

在 `CCGameplayTags` 中以原生 Tag 声明（编译期检查，杜绝字符串拼写错误）：

```
State.Attacking / Dodging / HitReacting / Immobilized / Dead
State.Invincible            闪避无敌帧
State.PerfectDodgeWindow    闪身判定窗口
Ability.Light / Heavy / Charged / Dodge / Spell.Immobilize
Event.Hit.Received / Event.Combo.Advance / Event.PerfectDodge
Buff.PerfectDodgeFollowUp   闪身成功后的强化窗口
Block.Input                 硬直 / 死亡时屏蔽输入
```

**状态互斥全部通过 `ActivationBlockedTags` 声明，禁止写 `if` 判断。**
例：`CCGA_ComboAttack.ActivationBlockedTags = {State.Dodging, State.HitReacting, State.Dead}` —— 闪避中按攻击自动激活失败，零代码。

### 4.3 招式表（UCCComboDataAsset）

```cpp
USTRUCT()
struct FCCComboStep
{
    UAnimMontage* Montage;
    float  DamageCoefficient;     // 乘 AttackPower
    float  PoiseDamage;           // 削韧性
    float  StanceGain;            // 产棍势
    int32  StanceCost;            // 消耗棍势
    FGameplayTag HitReactLevel;   // 决定对方播哪档受击
    TArray<FHitBoxSpec> HitBoxes; // 骨骼名 + 半径
};

// DataAsset 成员：
TArray<FCCComboStep> LightCombo;    // V4_q1 → q2 → q3 → q4 → q5
TArray<FCCComboStep> HeavyCombo;    // z1 → z2 → ...
FCCComboStep         ChargedAttack; // xuli_*
```

玩家 `DA_Combo_Wukong`，杨戬 `DA_Combo_Yangjian`。**新增招式 = 数组加一行，不碰代码、不重编译。**

### 4.4 连招流程

```
按下轻攻 → GA_ComboAttack 激活 → 播 Step[0].Montage
   ↓
ANS_ComboWindow 开启期间再次按下 → ASC 记录缓冲输入（不立即播放）
   ↓
窗口结束 / Montage BlendOut → 有缓冲则播 Step[N+1]，否则结束能力
```

**输入缓冲存放在 ASC 而非 Ability 内。** 闪避取消攻击时缓冲需跨能力存活；放在 Ability 内会随能力结束一并销毁。

蓄力攻击：长按重攻进入 `xuli_start → xuli_loop`，松手按蓄力档位播放 `xuli_attack_1/2/3`，档位同时决定伤害系数与棍势消耗。

### 4.5 BOSS 受击：反馈与打断分离

杨戬**不设 `Poise` 属性、不会被普通攻击打断行动**，但保留完整命中反馈：

- 受击闪白（复用已有 `MF_HitFlash` 材质函数）
- 命中顿帧
- Additive 受击层轻微抖动

即「打得到，但打不停」，BOSS 保持压迫感的同时玩家每一击都有回应。杨戬的 `bh_dep01/02` 多方向受击动画本版不接入，保留给后续开启打断时使用。

### 4.6 闪身（PerfectDodge）

闪避激活时 `ANS_Invincible` 施加 `State.Invincible`，起始若干帧额外附加 `State.PerfectDodgeWindow`。

**判定全部在受击方执行，攻击方只发送命中事件。** 否则每新增一个 BOSS 都要复制一遍判定逻辑。

受击方收到命中事件时：

1. 持有 `State.Invincible` → 伤害作废
2. 同时持有 `State.PerfectDodgeWindow` → 判定闪身成功：
   - 广播 `Event.PerfectDodge`
   - 播放留影特效 + 短暂慢镜
   - 授予限时 `Buff.PerfectDodgeFollowUp`，期间重攻直接接特殊招式

### 4.7 命中检测（CCHitDetector）

不使用碰撞体，采用**骨骼间扫掠**：`ANS_HitBox` 开窗期间，每帧对武器骨骼「上一帧位置 → 当前位置」执行 `SweepMultiByChannel`，以 `TSet<AActor*>` 去重，保证单次挥棍对同一目标只结算一次伤害。

优势：高速挥棍不穿透，开关窗口精确到帧。

---

## 5. BOSS AI

### 5.1 StateTree 结构

```
Root
├─ Phase1  [Health > 50%]
│   ├─ Approach      距离 > 600 → 跑近（lock_run 动画）
│   ├─ Strafe        中距离绕圈，累积攻击 CD
│   ├─ AttackCombo   从招式池按权重抽取
│   └─ Backstep      被贴脸 → dodge_b 拉开距离
├─ Transition [Health <= 50%，一次性]
│   └─ 播放 bianshen（变身），期间无敌 + 不可打断
├─ Phase2  [Health <= 50%]
│   └─ 同 Phase1，招式池更凶、间隔更短、加入 Buff 自增伤
└─ Death   [Health <= 0]  播放 die，禁用输入与碰撞
```

### 5.2 决策表（数据驱动）

`DA_Combo_Yangjian` 除招式表外，附带决策表：

```cpp
USTRUCT()
struct FCCBossAttackEntry
{
    FGameplayTag AbilityTag;    // 激活哪个能力
    float MinRange, MaxRange;   // 适用距离区间
    float Weight;               // 抽取权重
    float Cooldown;             // 单招 CD
    int32 PhaseMask;            // 适用阶段（1 / 2 / both）
};
```

`AttackCombo` 状态职责单一：按当前距离与阶段筛选候选 → 按权重随机抽取 → 通过 ASC 激活对应能力。

**调整 BOSS 难度与节奏 = 改表中数字，无需重编译**，与玩家侧同一哲学。

### 5.3 自定义 StateTree 任务节点

仅三个，不过度设计：`CCSTTask_ActivateAbility`、`CCSTTask_MoveToTarget`、`CCSTTask_Wait`。

---

## 6. 关卡、UI 与相机

**关卡**：新建 `Content/CoreCombat/Maps/L_BossArena` —— 封闭圆形竞技场，直径约 3000，简单地形 + 边界 + 定向光 + 后处理。**不做美术**，Demo 看点在战斗。默认地图由 `Lvl_ThirdPerson` 切换至此。

**UI**（UMG，全部绑定 AttributeSet 属性变化委托）：玩家血条、法力条、**棍势条**（分段显示）、BOSS 血条 + 名称。

**锁定与相机**（`CCLockOnComponent`）：按键在视锥内检索最近敌人；锁定后 Controller Yaw 插值朝向目标，角色移动切换为 strafe 模式（使用 `lock_run_*` 8 方向动画）。锁定 / 自由两套 locomotion 在 AnimBP 中以 bool 切换状态机分支。

---

## 7. 验证策略

UE gameplay 代码无法全面单测（能力依赖 ASC、动画依赖 Montage、AI 依赖世界）。分三档，各司其职，**不假装自动化覆盖手感**。

### 7.1 自动化测试（`CoreCombat.Tests` 模块，UE Automation Framework）

仅覆盖纯逻辑且易出错的部分，**这些先写测试后写实现**：

- **伤害公式**：给定攻防与系数的输出正确性；减到 0 不为负
- **棍势累积与消耗**：5 段连打累积量、重攻扣除校验、超上限不溢出
- **招式表推进**：`Step[N] → Step[N+1]`、末段回绕、空表不崩溃
- **BOSS 决策表筛选**：给定距离与阶段的候选集合正确性、权重抽取分布合理性

### 7.2 Gameplay Debugger

自定义 debugger category（`'` 键呼出）：实时显示当前 Tag 集合、激活中能力、属性数值、BOSS StateTree 当前状态、命中盒扫掠轨迹。

### 7.3 手动验收清单

每个里程碑配套「打开编辑器点什么、应看到什么」的清单。手感、动画衔接、打击感仅能人眼判断。

---

## 8. 里程碑

每个里程碑结束时项目**可编译、可进游戏、能看到东西**。

| # | 里程碑 | 验收标志 |
|---|---|---|
| **M1** | 骨架搭建 | 新类层级替换模板，BP_WuKong 重父类化，ASC 真正初始化，属性在 debugger 可见 |
| **M2** | 轻攻连段 | Python 批量生成 Montage，打出 5 段连招，命中盒可视化，能击中木桩 |
| **M3** | 伤害与棍势 | 木桩掉血会死，棍势条累积，重攻消耗棍势打出大伤害，蓄力攻击可用 |
| **M4** | 闪避与闪身 | 闪避有无敌帧，闪身成功触发慢镜 + 留影 + 后续重攻强化 |
| **M5** | 锁定与相机 | 锁定敌人，8 方向 strafe 移动，镜头跟随 |
| **M6** | 杨戬登场 | BOSS 位于竞技场，会追击、绕圈、按决策表出招 |
| **M7** | 两阶段与变身 | 半血触发变身，二阶段强化，BOSS 可被击杀，胜负结算 |
| **M8** | 定身术 | 法术命中冻结 BOSS，材质变化，法力消耗与 CD |
| **M9** | 打磨 | 顿帧、震屏、命中特效、UI 完整、音效占位 |

M1–M5 完成即构成手感完整的战斗原型；M6–M7 使其成为一场 BOSS 战。**M5 结束是最自然的方向调整分叉点。**

### 迁移与工具化说明

- **模板类替换**：现有 `ACoreCombatCharacter` 的 ASC 指针从未 `CreateDefaultSubobject`，当前 `GetAbilitySystemComponent()` 返回 null。M1 中以新类层级替换，并在编辑器内将 `BP_WuKong` 重新父类化至 `ACCPlayerCharacter`。
- **Montage 批量生成**：693 个资产均为 AnimSequence，连招需要 Montage 承载 Notify 窗口。沿用项目既有 Python 编辑器脚本习惯（`Scripts/` 下已有 4 个），编写脚本读取招式清单，自动创建 Montage、配置 Slot、按时长比例插入默认 Notify 窗口，后续在编辑器内微调帧位置。

---

## 9. 非目标（YAGNI）

明确不做，防止范围蔓延：

网络同步与预测、存档、装备 / 技能树、多 BOSS、过场动画、剧情、菜单系统、完整音频系统、性能优化、定身术以外的法术（身外身 / 毫毛 / 筋斗云）、BOSS 被打断硬直、杨戬多方向受击动画接入。

---

## 10. 风险

| 风险 | 影响 | 应对 |
|---|---|---|
| Montage 批量生成的 Notify 窗口时机不准 | 连招手感差 | 脚本只生成默认窗口，M2 验收时人工逐招微调 |
| 693 个动画中部分资产质量参差 | 某些招式无法使用 | 招式表数据驱动，替换 Montage 引用即可，不影响代码 |
| StateTree 在 UE 5.8 的 API 变动 | AI 实现受阻 | M6 前先做最小验证节点跑通再铺开 |
| 单向硬直导致战斗缺乏正反馈 | 手感偏弱 | M6 验收时评估；如需要，Poise 属性预留在 AttributeSet 中，开启成本低 |
