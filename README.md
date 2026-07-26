# UE5-UnscaledTime

UE 5.8 向けの Runtime plugin です。`SetGlobalTimeDilation` や `SetGamePaused` の影響を受けない、実時間ベースの Delay / Timer / Tick を Blueprint、C++、Gameplay Ability System から使えるようにします。

## 主な機能

| 分類 | 内容 |
| --- | --- |
| Blueprint Delay | `Unscaled Delay` / `Unscaled Retriggerable Delay` と Frames 版 |
| Blueprint Timer | Event / Function Name 指定、Handle / Event による clear・pause・query |
| Tick | `UUnscaledTickComponent` と native `GetOnUnscaledTick` delegate |
| Clock | `GetUnscaledDeltaSeconds` / `GetUnscaledTimeSeconds` |
| Frames | `ReferenceFrameRate` 基準の frame ⇔ seconds 換算 |
| GAS | `WaitUnscaledDelay` / `UnscaledTick` AbilityTask |
| Debug | `UnscaledTime.DumpTimers`、`stat UnscaledTime`、debug CVars |

## 対応バージョン

- Unreal Engine 5.8

## リポジトリ構成

| Path | 内容 |
| --- | --- |
| `UnscaledTimeSample.uproject` | サンプルプロジェクト |
| `Plugins/UnscaledTime/` | plugin 本体 |
| `Plugins/UnscaledTime/Source/UnscaledTime/` | Blueprint / C++ 用 Runtime module |
| `Plugins/UnscaledTime/Source/UnscaledTimeGAS/` | GAS AbilityTask 用 Runtime module |
| `Docs/` | 利用方法と設計メモ |

## ドキュメント

- [使い方](Docs/Usage.md)
- [設計メモ](Docs/Design.md)

## ライセンス

このリポジトリは MIT License です。詳細は [LICENSE](LICENSE) を参照してください。
