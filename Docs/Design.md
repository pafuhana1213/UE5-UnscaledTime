# UnscaledTime 設計メモ

## 標準 Timer / Delay / Tick が止まる理由

環境内で確認できた Engine source は `C:\UE5\57\UnrealEngine574\Engine\Windows\Engine\Source` の UE 5.7.4 系です。計画にあった UE 5.8 の行番号そのものは検証できなかったため、ここでは確認できた source の file / function / line を基準にします。

`UWorld::Tick` は入力された real delta を保存した後、world settings の effective time dilation を掛けた `DeltaSeconds` を game time として使います。

```cpp
// LevelTick.cpp:1567
DeltaSeconds *= Info->GetEffectiveTimeDilation();
```

同じ `UWorld::Tick` 内で latent action と world timer は pause guard の内側にあります。

```cpp
// LevelTick.cpp:1764-1768
if( !bIsPaused )
{
	CurrentLatentActionManager.ProcessLatentActions(nullptr, DeltaSeconds);
}
```

```cpp
// LevelTick.cpp:1783-1787
if (TickType != LEVELTICK_TimeOnly && !bIsPaused)
{
	GetTimerManager().Tick(DeltaSeconds);
}
```

このため標準 Delay / Timer は time dilation 後の `DeltaSeconds` で進み、pause 中は処理されません。Actor / component / GameplayTask の通常 tick も world tick 由来の `DeltaTime` に乗るため、同じ前提を共有します。

## 実デルタ源

UnscaledTime は `UUnscaledTimeSubsystem::Tick` で `World->GetTime().GetDeltaRealTimeSeconds()` を読み、`MaxRealDeltaSeconds` で clamp した値を使います。

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
// LatentActionManager.cpp:332-334
if (UFunction* ExecutionFunction = CallbackTarget->FindFunction(LinkInfo.ExecutionFunction))
{
	CallbackTarget->ProcessEvent(ExecutionFunction, &(LinkInfo.LinkID));
}
```

重複判定 key は callback target の raw pointer と latent `UUID` です。`Unscaled Delay` は同じ key が pending の場合は無視し、`Unscaled Retriggerable Delay` は timer を張り直して `ExecutionFunction` / `Linkage` / target を更新します。これは vanilla Delay / Retriggerable Delay と同じ使い分けです。

`DurationSeconds <= 0` は `SetTimerForNextTick` を使い、次の unscaled timer tick で再開します。

## Pending Timer

UnscaledTime は Engine の `FTimerManager` をそのまま使うため、pending timer の仕様も vanilla と同じです。

`FTimerManager::InternalSetTimer` は manager が同一 frame で tick 済みなら active heap へ入れます。まだ tick していない場合は `PendingTimerSet` に入れ、`Tick` の末尾で active 化します。確認箇所は `TimerManager.cpp:638-651` と `TimerManager.cpp:1147-1162` です。

この仕様により、manager 未 tick の frame で `SetTimer` した timer はその frame の処理対象ではなく、次回 tick 以降に有効になります。テストでは `FUnscaledTimeTestFixture::ArmPendingTimers()` が `PumpFrames(1, 0.f)` を呼び、この pending timer を明示的に arm しています。

## GAS 設計

`UAbilityTask_UnscaledTick` は `bTickingTask` による `TickTask` を使わず、Subsystem の `GetOnUnscaledTick(Clock)` delegate に登録します。

理由は、GameplayTasks の ticking が通常の component tick に乗るためです。確認した `UGameplayTasksComponent::TickComponent` は `DeltaTime` をそのまま task に渡します。

```cpp
// GameplayTasksComponent.cpp:280
TickingTask->TickTask(DeltaTime);
```

この `DeltaTime` は通常 world tick 由来で、dilation / pause の影響を受ける tick chain です。UnscaledTime の GAS task はそれを避けるため、Subsystem delegate で実デルタを受けます。

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
- standalone `FTimerManager` を使うため、短周期 loop の catch-up と `bMaxOncePerFrame` は Engine timer と同じ意味です。

## 将来拡張

- HitStop utility: game 全体の dilation/pause と UnscaledTime を組み合わせる helper。
- 実フレームカウント版: 秒換算ではなく tick 回数で満了する Delay / Timer。
- 名前付き custom clock: gameplay system ごとに独立した unscaled clock を持つ。
- `MinTickInterval`: Subsystem delegate や timer 更新の最小間隔を設定し、超高頻度 callback を抑える。
