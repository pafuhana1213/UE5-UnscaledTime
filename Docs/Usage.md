# UnscaledTime 使い方

## 概要

UnscaledTime は、`SetGlobalTimeDilation` と `SetGamePaused` の影響を受けない実時間ベースの Delay / Timer / Tick を追加する UE 5.8 向け Runtime plugin です。

標準の Delay / Timer / Tick は world のゲーム時間で進むため、グローバル time dilation や pause 中に停止します。この plugin は `UUnscaledTimeSubsystem` が world ごとに実デルタを取得し、独立した `FTimerManager` と delegate で Blueprint / C++ / GAS 向け API を提供します。

`bTickWhilePaused` の意味は全 API で共通です。

| 値 | 挙動 |
| --- | --- |
| `true` | pause 中も実時間で進みます。デフォルトです。 |
| `false` | pause 中は停止し、unpause 後に続きます。 |

どちらの場合も `SetGlobalTimeDilation` は常に無視されます。

## インストール

1. プロジェクトの `Plugins/` 配下に `UnscaledTime` folder を配置します。
2. Editor の Plugins 画面で `UnscaledTime` を有効化します。
3. プロジェクトを再起動します。

`.uplugin` には `UnscaledTime` と `UnscaledTimeGAS` の 2 Runtime module があり、`GameplayAbilities` plugin を有効化する設定が含まれます。GAS 機能を使う場合はプロジェクト側でも Gameplay Ability System の初期化が必要です。

## Blueprint ノード

### Delay

Category は `UnscaledTime|FlowControl` です。

| ノード | パラメータ | 説明 |
| --- | --- | --- |
| `Unscaled Delay` | `Duration`, `LatentInfo`, `bTickWhilePaused=true` | 実時間 Delay。同じ latent UUID が実行中の場合、後続呼び出しは無視されます。 |
| `Unscaled Retriggerable Delay` | `Duration`, `LatentInfo`, `bTickWhilePaused=true` | 実時間 retriggerable Delay。同じ latent UUID の実行中に呼ぶと残り時間を `Duration` にリセットします。 |
| `Unscaled Delay (Frames)` | `FrameDuration`, `LatentInfo`, `bTickWhilePaused=true` | `FrameDuration / ReferenceFrameRate` 秒として扱う Delay。 |
| `Unscaled Retriggerable Delay (Frames)` | `FrameDuration`, `LatentInfo`, `bTickWhilePaused=true` | frame 換算版の retriggerable Delay。 |

Delay の `Duration <= 0` は次の unscaled timer tick で再開されます。

### Timer 作成

Category は `UnscaledTime|Timer` です。戻り値は `FUnscaledTimerHandle` です。

| ノード | パラメータ | 説明 |
| --- | --- | --- |
| `Set Unscaled Timer by Event` | `Event`, `Time`, `bLooping`, `bTickWhilePaused=true`, `bMaxOncePerFrame=false`, `InitialStartDelay=0`, `InitialStartDelayVariance=0` | dynamic event を実時間 timer に登録します。 |
| `Set Unscaled Timer by Function Name` | `Object`, `FunctionName`, `Time`, `bLooping`, `bTickWhilePaused=true`, `bMaxOncePerFrame=false`, `InitialStartDelay=0`, `InitialStartDelayVariance=0` | parameter なし関数名を実時間 timer に登録します。 |
| `Set Unscaled Timer by Event (Frames)` | `Event`, `FrameInterval`, `bLooping`, `bTickWhilePaused=true`, `bMaxOncePerFrame=false` | `FrameInterval / ReferenceFrameRate` 秒で登録します。 |
| `Set Unscaled Timer by Function Name (Frames)` | `Object`, `FunctionName`, `FrameInterval`, `bLooping`, `bTickWhilePaused=true`, `bMaxOncePerFrame=false` | 関数名指定の frame 換算版です。 |

`bMaxOncePerFrame=false` の looping timer は、大きい delta が入ったときに catch-up で同一 frame に複数回発火することがあります。1 frame 1 回までにしたい場合は `true` にします。

`Time <= 0` は警告を出し、内部の `FTimerManager` により handle が無効化されます。`InitialStartDelayVariance` は `InitialStartDelay` に `[-Variance, +Variance]` の乱数を加算してから `FirstDelay` に反映されます。標準 `UKismetSystemLibrary::K2_SetTimer` が `InitialStartDelayVariance` を転送しない挙動と異なり、この plugin は意図的に転送します。

### Timer 操作・照会

Handle 系は `FUnscaledTimerHandle` に保存された clock へ routing します。Event 系は pause 中も進む `RealTime` manager を先に探し、次に pause 中停止の `RealTimeUnpaused` manager を探します。

| ノード | 対象 | 説明 |
| --- | --- | --- |
| `Clear and Invalidate Unscaled Timer by Handle` | Handle | timer を clear し、handle を invalidate します。 |
| `Pause Unscaled Timer by Handle` | Handle | timer を一時停止します。 |
| `Unpause Unscaled Timer by Handle` | Handle | timer を再開します。 |
| `Is Unscaled Timer Active by Handle` | Handle | active なら `true`。 |
| `Is Unscaled Timer Paused by Handle` | Handle | paused なら `true`。 |
| `Does Unscaled Timer Exist by Handle` | Handle | timer が存在すれば `true`。 |
| `Get Unscaled Timer Elapsed Time by Handle` | Handle | 経過秒を返します。無効 handle では `0`。 |
| `Get Unscaled Timer Remaining Time by Handle` | Handle | 残り秒を返します。無効 handle では `0`。 |
| `Is Valid Unscaled Timer Handle` | Handle | handle が一度 timer を参照した状態かを world なしで確認します。 |
| `Clear Unscaled Timer by Event` | Event | event に対応する timer を clear します。 |
| `Pause Unscaled Timer by Event` | Event | event に対応する timer を一時停止します。 |
| `Unpause Unscaled Timer by Event` | Event | event に対応する timer を再開します。 |
| `Is Unscaled Timer Active by Event` | Event | event timer が active なら `true`。 |
| `Is Unscaled Timer Paused by Event` | Event | event timer が paused なら `true`。 |
| `Does Unscaled Timer Exist by Event` | Event | event timer が存在すれば `true`。 |

### Tick component

`UUnscaledTickComponent` は `ClassGroup=(UnscaledTime)`、`BlueprintSpawnableComponent` です。

| メンバー | 種別 | 説明 |
| --- | --- | --- |
| `OnUnscaledTick(float RealDeltaSeconds)` | BlueprintAssignable | Subsystem の unscaled tick ごとに broadcast されます。 |
| `bTickWhilePaused=true` | EditAnywhere / BlueprintReadOnly | `true` なら pause 中も broadcast、`false` なら pause 中は停止します。 |

通常の `PrimaryComponentTick` は使わず、component の active/deactive/end/destroy に合わせて Subsystem delegate へ登録・解除します。

### Clock / Frames

Category は `UnscaledTime|Clock` / `UnscaledTime|Frames` です。これらは `DisplayName` metadata を持たないため、C++ 関数名由来の Blueprint ノードとして公開されます。

| 関数 | パラメータ | 説明 |
| --- | --- | --- |
| `GetUnscaledDeltaSeconds` | `WorldContextObject` | 直近 tick の clamp 後 real delta 秒。Subsystem がなければ `0`。 |
| `GetUnscaledTimeSeconds` | `WorldContextObject`, `Clock=RealTime` | 指定 clock の累積 unscaled 秒。Subsystem がなければ `0`。 |
| `UnscaledFramesToSeconds` | `Frames` | `Frames / ReferenceFrameRate` を返します。 |
| `UnscaledSecondsToFrames` | `Seconds` | `Seconds * ReferenceFrameRate` を四捨五入して frame 数にします。 |

`EUnscaledTimeClock` は `RealTime` と `RealTimeUnpaused` です。Blueprint 表示名はそれぞれ `Real Time (ticks while paused)`、`Real Time (stops while paused)` です。

## GAS

`UnscaledTimeGAS` module は Gameplay Ability System 用の AbilityTask を提供します。

| Task | パラメータ | Delegate | 説明 |
| --- | --- | --- | --- |
| `WaitUnscaledDelay` | `OwningAbility`, `Time`, `bTickWhilePaused=true` | `OnFinish` | 指定した実時間後に完了します。`Time <= 0` は次 tick で完了します。 |
| `UnscaledTick` | `OwningAbility`, `bTickWhilePaused=true` | `OnTick(float RealDeltaSeconds)` | AbilityTask が active な間、unscaled tick を broadcast します。 |

C++ では task を生成し、delegate を bind して `ReadyForActivation()` を呼びます。

```cpp
UAbilityTask_WaitUnscaledDelay* Task =
	UAbilityTask_WaitUnscaledDelay::WaitUnscaledDelay(this, 1.0f, true);
Task->OnFinish.AddDynamic(this, &ThisClass::HandleFinished);
Task->ReadyForActivation();
```

## C++ API

Subsystem は world ごとの `UTickableWorldSubsystem` です。対応 world type は `Game` と `PIE` です。

```cpp
if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(WorldContextObject))
{
	FTimerManager& Manager = Subsystem->GetTimerManager(EUnscaledTimeClock::RealTime);
	FTimerHandle RawHandle;
	Manager.SetTimer(RawHandle, Delegate, 0.5f, false);
}
```

`GetTimerManager(EUnscaledTimeClock::RealTime)` は pause 中も進む manager、`GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused)` は pause 中停止の manager を返します。

Tick は `GetOnUnscaledTick(EUnscaledTimeClock)` で native multicast delegate を取得して登録できます。

```cpp
DelegateHandle = Subsystem->GetOnUnscaledTick(EUnscaledTimeClock::RealTime)
	.AddUObject(this, &ThisClass::HandleUnscaledTick);
```

`FUnscaledTimerHandle` は `FTimerHandle Handle` と `EUnscaledTimeClock Clock` の wrapper です。内部 timer manager が world ごとに別なので、別 world へ持ち越せません。保存/復元用の永続 ID でもありません。

## プロジェクト設定

Editor の Project Settings では `Plugins > Unscaled Time` に表示されます。Config class は `UUnscaledTimeSettings` です。

| 設定名 | 既定値 | 説明 |
| --- | --- | --- |
| `ReferenceFrameRate` | `60.0` | frame API の換算基準です。秒 = frame 数 / `ReferenceFrameRate`。最低値は `1.0`。 |
| `MaxRealDeltaSeconds` | `0.5` | 1 tick の real delta clamp。`0` なら clamp 無効。sleep、breakpoint、window stall 後の大量 catch-up を抑えるための安全弁です。 |

## デバッグ

shipping build 以外で有効です。

| 種別 | 名前 | 説明 |
| --- | --- | --- |
| Console command | `UnscaledTime.DumpTimers` | 現在 world の clock、pending latent delay、2 つの timer manager の内容を log に出します。 |
| Stat | `stat UnscaledTime` | `Subsystem Tick` と `Pending Delays` を確認します。 |
| CVar | `UnscaledTime.Debug` | `0` 以外で world ごとの状態を on-screen debug 表示します。 |
| CVar | `UnscaledTime.SuspendWhileDebugPaused` | `0` 以外で debug pause 中の UnscaledTime clock 更新を止めます。 |

## 注意点・制限

- PIE の一時停止ボタンなどで `World->bDebugPauseExecution` が立っている間も、既定では `RealTime` clock は進みます。必要なら `UnscaledTime.SuspendWhileDebugPaused 1` を使います。
- ネットワーク同期機能はありません。world ごとのローカル scheduling であり、timer handle、発火時刻、tick は replication されません。server/client 同期が必要な場合は上位ロジックで同期してください。
- pause 中に callback が走ると、他 system が停止した状態で game state を変更できます。UI、入力、physics、AI などの停止状態を考慮して callback を設計してください。
- 短周期 looping timer は大きい delta で catch-up 発火します。`bMaxOncePerFrame=true` で同一 frame 1 回に制限できます。
- Timer の `Time <= 0` は timer を作らず handle が無効になります。Delay と GAS `WaitUnscaledDelay` の `0 以下` は次 tick 扱いです。
- `Set Unscaled Timer by Function Name` / `by Event` は `InitialStartDelayVariance` を意図的に転送します。標準 Blueprint timer の未転送挙動に依存している場合は差異になります。

## FAQ

### Q. time dilation 0.01 や 100 の間も同じ秒数で発火しますか？

はい。UnscaledTime は `World->GetTime().GetDeltaRealTimeSeconds()` を使い、dilation 後の `DeltaSeconds` を使いません。

### Q. pause 中だけ止めたい場合は？

各 API の `bTickWhilePaused` を `false` にします。dilation は無視したまま、pause 中だけ停止します。

### Q. frame 版は実フレーム数を数えますか？

いいえ。`ReferenceFrameRate` を使って秒へ換算する API です。既定では 60 frame = 1 秒です。

### Q. `FUnscaledTimerHandle` を save game に保存できますか？

できません。world 内の `FTimerManager` が持つ一時 handle と clock routing 情報です。

### Q. multiplayer の server/client で同時に発火しますか？

保証しません。各 world のローカル実時間で進むため、同期は gameplay code 側で行ってください。
