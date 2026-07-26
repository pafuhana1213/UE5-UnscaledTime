# UnscaledTime 設計メモ

## 標準 Timer / Delay / Tick が止まる理由

本節以下の file / line は UE 5.8 の Engine source (`E:\Launcher\UE_5.8\Engine\Source`) で検証したものです。

`UWorld::Tick` は入力された real delta を保存した後、world settings の effective time dilation を掛けた `DeltaSeconds` を game time として使い、clamp します。

```cpp
// LevelTick.cpp:1592-1604
// Save off actual delta
float RealDeltaSeconds = DeltaSeconds;

// apply time multipliers
DeltaSeconds *= Info->GetEffectiveTimeDilation();

// Handle clamping of time to an acceptable value
const float GameDeltaSeconds = Info->FixupDeltaSeconds(DeltaSeconds, RealDeltaSeconds);

DeltaSeconds = GameDeltaSeconds;
DeltaTimeSeconds = DeltaSeconds;
DeltaRealTimeSeconds = RealDeltaSeconds;
```

`GetEffectiveTimeDilation()` の内訳は `TimeDilation * CinematicTimeDilation * DemoPlayTimeDilation` です(`WorldSettings.h:860-868`)。`bAllowTimeDilation` が false の場合は 1.0 を返します。

この後 dilation 済みの `DeltaSeconds` が latent action / world timer / tickable object のすべてに配られ、`DeltaRealTimeSeconds` だけが実時間として残ります。UnscaledTime が読むのは後者です。

同じ `UWorld::Tick` 内で latent action と world timer は pause guard の内側にあります。

```cpp
// LevelTick.cpp:1793-1798
if( !bIsPaused )
{
	CurrentLatentActionManager.ProcessLatentActions(nullptr, DeltaSeconds);
}
```

```cpp
// LevelTick.cpp:1812-1817
if (TickType != LEVELTICK_TimeOnly && !bIsPaused)
{
	GetTimerManager().Tick(DeltaSeconds);
}
```

tickable object にも同じ dilation 済み `DeltaSeconds` が渡ります。

```cpp
// LevelTick.cpp:1821
FTickableGameObject::TickObjects(this, TickType, bIsPaused, DeltaSeconds);
```

このため標準 Delay / Timer は time dilation 後の `DeltaSeconds` で進み、pause 中は処理されません。Actor / component / GameplayTask の通常 tick も world tick 由来の `DeltaTime` に乗るため、同じ前提を共有します。`UUnscaledTimeSubsystem::Tick(float DeltaTime)` の引数もこの dilation 済み `DeltaSeconds` なので、Subsystem はこの引数を時間計測に使いません。

### Timer の発火頻度が変わる理由

```cpp
// TimerManager.cpp:1160
InternalTime += DeltaTime;
```

`FTimerManager` は渡された delta を内部時間に足すだけで、満了判定はこの `InternalTime` に対して行われます。つまり `SetTimer` の `Rate` は実時間秒ではなく game time 秒です。global dilation `0.5` なら 1 秒 timer は実時間 2 秒後に発火し、looping timer の実時間あたりの発火回数は半分になります。「頻度が変わる」のはこの単位の帰結です。

### dilation を 0 にしても停止はしない

```cpp
// WorldSettings.cpp:345-349
float AWorldSettings::SetTimeDilation(float NewTimeDilation)
{
	TimeDilation = FMath::Clamp(NewTimeDilation, MinGlobalTimeDilation, MaxGlobalTimeDilation);
	return TimeDilation;
}
```

既定の下限は `MinGlobalTimeDilation=0.0001` です(`BaseGame.ini:200`)。`SetGlobalTimeDilation(0.f)` はこの値に丸められるため game time は極端に遅くなりますが停止はしません。完全に止めるには pause を使う必要があります。

### CustomTimeDilation は timer に効かない

```cpp
// Actor.cpp:6648-6654
float AActor::GetActorTimeDilation() const
{
	return CustomTimeDilation * GetWorldSettings()->GetEffectiveTimeDilation();
}
```

world timer manager に渡るのは global dilation のみを掛けた `DeltaSeconds` なので、特定 actor を `CustomTimeDilation` で減速してもその actor が張った timer は global 速度で進みます。`CustomTimeDilation` の影響を受けるのは actor / component の tick chain だけです。UnscaledTime の `UUnscaledTickComponent` は `PrimaryComponentTick.bCanEverTick = false` として component tick を使わないため、global dilation と `CustomTimeDilation` の両方から独立します。

### Engine 側にも frame delta clamp がある

```cpp
// WorldSettings.cpp:334-343
float const Dilation = GetEffectiveTimeDilation();
float const MinFrameTime = MinUndilatedFrameTime * Dilation;
float const MaxFrameTime = MaxUndilatedFrameTime * Dilation;

// clamp frame time according to desired limits
return FMath::Clamp(DeltaSeconds, MinFrameTime, MaxFrameTime);
```

既定値は `MinUndilatedFrameTime=0.0005`(2000 fps 相当)と `MaxUndilatedFrameTime=0.4`(2.5 fps 相当)で、`BaseGame.ini:198-199` にあります。0.4 秒を超える hitch では vanilla timer も game time を取りこぼします。UnscaledTime の `MaxRealDeltaSeconds` は同じ考え方を実時間側に適用したものです。

## 実デルタ源

UnscaledTime は `UUnscaledTimeSubsystem::Tick` で `World->GetTime().GetDeltaRealTimeSeconds()` を読み、`MaxRealDeltaSeconds` で clamp した値を使います。

ただし、`World->GetTime().GetDeltaRealTimeSeconds()` が返すのは `UWorld::Tick` に入力された frame delta であり、OS の wall clock そのものではありません。この値の元は `FApp::GetDeltaTime()` で、次の影響を受けます。

- fixed frame rate / fixed time step 有効時は合成値になる(`UnrealEngine.cpp:3129-3141`)。
- frame rate smoothing の対象になる。
- `UGameEngine::MaxDeltaTime > 0` の standalone authority game では clamp される(`UnrealEngine.cpp:3168-3195`)。既定は `MaxDeltaTime=0` で無効です(`BaseEngine.ini:1581`)。
- `t.OverrideFPS` で上書きできる(`UnrealEngine.cpp:3201-3211`)。

したがって UnscaledTime が保証するのは「global time dilation と pause から独立していること」であり、「wall clock と厳密に一致すること」ではありません。

この source を選んだ理由は次の通りです。

- world ごとの time source なので PIE の複数 world と相性がよい。
- `FTestWorldWrapper::TickTestWorld(DeltaSeconds)` から決定論的に delta を注入できる。
- engine の platform wall-clock を直接読むより、world lifecycle と test world の制御に乗せやすい。

`MaxRealDeltaSeconds` clamp は必須です。editor break、window stall、OS sleep の直後に大きな real delta が来ると、looping timer の catch-up 発火や大量 callback が 1 frame に集中します。既定値 `0.5` はその上限です。`0` を指定した場合のみ clamp を無効化します。

## デュアル TimerManager

Subsystem は standalone の `FTimerManager` を 2 つ持ちます。

| Clock | Manager | pause 中 |
| --- | --- | --- |
| `EUnscaledTimeClock::RealTime` | `RealTimeManager` | tick する |
| `EUnscaledTimeClock::RealTimeUnpaused` | `UnpausedManager` | tick しない |

`Tick` ではまず clamp 後 real delta で `RealTimeManager` と `OnTickRealTime` を進めます。`World->IsPaused()` が false のときだけ `UnpausedManager` と `OnTickUnpaused` を進めます。

生の `FTimerHandle` は manager 間で衝突しうるため、Blueprint API は `FUnscaledTimerHandle` を返します。これは `FTimerHandle Handle` と `EUnscaledTimeClock Clock` の wrapper で、Clear / Pause / Unpause / query 系は clock を見て正しい manager に routing します。

## Delay 実装

標準 latent Delay をそのまま使わない理由は、`FLatentActionManager::ProcessLatentActions` 自体が `UWorld::Tick` の `!bIsPaused` guard 内にあり、pause 中に処理されないためです。

UnscaledTime の Delay は `FLatentActionManager` に action を登録せず、Subsystem の timer が満了した時点で latent resume point を直接 `ProcessEvent` します。これは Engine の latent manager が execution link を発火する流儀と同じです。

```cpp
// LatentActionManager.cpp:366-369
if (UFunction* ExecutionFunction = CallbackTarget->FindFunction(LinkInfo.ExecutionFunction))
{
	CallbackTarget->ProcessEvent(ExecutionFunction, &(LinkInfo.LinkID));
}
```

重複判定 key の `FUnscaledDelayKey` は callback target を raw pointer ではなく `FWeakObjectPtr` として保持し、比較には `HasSameIndexAndSerialNumber()` を使います(`UnscaledTimeSubsystem.h:39-53`)。これは破棄済み object と同じ index を再利用した別 object が、同じ latent `UUID` で衝突することを避けるためです。`Unscaled Delay` は同じ key が pending の場合は無視し、`Unscaled Retriggerable Delay` は timer を張り直して `ExecutionFunction` / `Linkage` / target を更新します。これは vanilla Delay / Retriggerable Delay と同じ使い分けです。

`DurationSeconds <= 0` は `SetTimerForNextTick` を使い、次の unscaled timer tick で再開します。

## Pending Timer

UnscaledTime は Engine の `FTimerManager` をそのまま使うため、pending timer の仕様も vanilla と同じです。

`FTimerManager::InternalSetTimer` は manager が同一 frame で tick 済みなら active heap へ入れます。まだ tick していない場合は `PendingTimerSet` に入れ、`Tick` の末尾で active 化します。確認箇所は active / pending 分岐が `TimerManager.cpp:712-726`、`Tick` 末尾の pending から active への昇格が `TimerManager.cpp:1377-1389` です。`Tick` 冒頭の二重 tick 早期 return は `TimerManager.cpp:1136`、`LastTickedFrame` の更新は `TimerManager.cpp:1374`、`HasBeenTickedThisFrame()` の実装は `LastTickedFrame == GFrameCounter` です(`TimerManager.h:466-469`)。

この仕様により、manager 未 tick の frame で `SetTimer` した timer はその frame の処理対象ではなく、次回 tick 以降に有効になります。テストでは `FUnscaledTimeTestFixture::ArmPendingTimers()` が `PumpFrames(1, 0.f)` を呼び、この pending timer を明示的に arm しています。

## GAS 設計

`UAbilityTask_UnscaledTick` は `bTickingTask` による `TickTask` を使わず、Subsystem の `GetOnUnscaledTick(Clock)` delegate に登録します。

理由は、GameplayTasks の ticking が通常の component tick に乗るためです。確認した `UGameplayTasksComponent::TickComponent` は `DeltaTime` をそのまま task に渡します。

```cpp
// GameplayTasksComponent.cpp:280
TickingTask->TickTask(DeltaTime);
```

この `DeltaTime` は通常 world tick 由来で、dilation / pause の影響を受ける tick chain です。`TickingTask->TickTask(DeltaTime);` は `GameplayTasksComponent.cpp:277-294` の範囲にあり、単一 task path は `:280`、複数 task path は `:294` で、複数 task path にも同じ `DeltaTime` が渡ります。UnscaledTime の GAS task はそれを避けるため、Subsystem delegate で実デルタを受けます。

`UAbilityTask_WaitUnscaledDelay` は Subsystem が見つかれば `GetTimerManager(RegisteredClock)` を使います。Subsystem が見つからない場合だけ world timer manager に fallback し、警告を出します。

## テスト戦略

Automation tests は `FTestWorldWrapper` で `EWorldType::Game` の test world を作り、`BeginPlayInTestWorld()` を呼んでいます。`UUnscaledTimeSubsystem` は `Game` と `PIE` のみ対応なので、BeginPlay 済み test world がないと subsystem/tick 前提が崩れます。

fixture は作成直後に `TickTestWorld(0.f)` を 1 回呼びます。コメントにある通り、standalone `FTimerManager` の `LastTickedFrame` が world 作成 frame の `GFrameCounter` と一致していると、最初の `Tick` が `HasBeenTickedThisFrame()` で落ちる可能性があるためです。

TimerManager 側では `HasBeenTickedThisFrame()` が `LastTickedFrame == GFrameCounter` を見る実装です。pending timer は `ArmPendingTimers()` のゼロデルタ pump で active 化します。

比較は `FMath::IsNearlyEqual(..., KINDA_SMALL_NUMBER)` を使う箇所と、発火回数や frame 換算の integer を strict に見る箇所を分けています。float 蓄積が入る elapsed / remaining / accumulated delta は nearly equal、callback count と rounded frame は strict 比較です。

## 既知の制限

- world ごとの local scheduler です。replication、server/client clock sync、save/load 復元は提供しません。
- subsystem 対応 world type は `Game` と `PIE` です。
- `RealTime` clock は debug pause 中も既定で進みます。`UnscaledTime.SuspendWhileDebugPaused` で抑止できます。
- pause 中 callback は他 system が停止した状態で実行されるため、game state 変更は呼び出し側で安全性を判断します。
- frame API は実フレーム数を数えず、`ReferenceFrameRate` による秒換算です。
- 実時間源は `FApp::GetDeltaTime()` 由来のため、fixed time step / `t.OverrideFPS` / frame rate smoothing 下では wall clock と一致しません。
- standalone `FTimerManager` を使うため、短周期 loop の catch-up と `bMaxOncePerFrame` は Engine timer と同じ意味です。

## 将来拡張

- HitStop utility: game 全体の dilation/pause と UnscaledTime を組み合わせる helper。
- 実フレームカウント版: 秒換算ではなく tick 回数で満了する Delay / Timer。
- 名前付き custom clock: gameplay system ごとに独立した unscaled clock を持つ。
- `MinTickInterval`: Subsystem delegate や timer 更新の最小間隔を設定し、超高頻度 callback を抑える。
