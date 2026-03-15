# remote_monitoring_qos_experiments_cpp

## 概要

遠隔監視（remote monitoring）を想定し、ROS2 Topic通信における

- レイテンシー（送信から受信までの遅延）
- メッセージ欠落（DROP）
- 受信側処理遅延
- QoS設定とqueue depthの影響

を観測するための実験リポジトリです。

特に、**通信環境が不安定な場所**や**監視側アプリケーションが重い状況**を想定し、  
QoSだけでなく、subscriber処理時間やキュー深さが通信品質にどう影響するかを確認します。

---

## 背景

遠隔監視系のシステムでは、単に「メッセージが届くかどうか」だけではなく、

- 最新状態をどれだけ早く見られるか
- 古いデータがキューに溜まって遅延しないか
- 重要な情報が欠落しないか

といった観点が重要になります。

本リポジトリでは、これらを簡易的なROS2ノード構成で再現し、  
**欠落と遅延のトレードオフ**を整理することを目的としています。

---

## 構成

### ノード

#### telemetry_publisher
ロボット側を模したノードです。

複数のトピックを異なる周期で publish します。

#### remote_monitor
遠隔監視側を模したノードです。

- 複数トピックを subscribe
- 送受時刻差分からレイテンシー計測
- 連番IDから欠番検出
- callback内で人工的な処理遅延を注入

を行います。

---

## トピック構成

| Topic | 想定用途 | 周期 | QoS | depth |
|---|---|---:|---|---:|
| `/telemetry/fast_state` | 高頻度監視データ | 10ms | best_effort | 1 |
| `/cmd/teleop` | 操作・制御コマンド | 50ms | reliable | 1 または 10 |
| `/telemetry/diagnostics` | 低頻度診断情報 | 1s | reliable | 10 |

---

## 実験の考え方

この実験では、publisher側で送信時刻と連番を含むメッセージを送信し、  
monitor側で以下を確認します。

### 1. レイテンシー
送信時刻と受信時刻の差分を計測し、遅延を可視化します。

### 2. 欠番（DROP?）
受信した連番が連続していない場合、取りこぼしの可能性として `DROP?` を表示します。

### 3. 受信側処理遅延の影響
subscriber callback 内で sleep を入れることで、  
監視側処理が重い状況を再現します。

---

## 実行方法

### Build

```bash
cd ~/ros2_ws
colcon build --packages-select remote_monitoring_qos_experiments_cpp
source install/setup.bash
```

### Run
ターミナルA:
```bash
ros2 run remote_monitoring_qos_experiments_cpp telemetry_publisher
```
ターミナルB:
```bash
ros2 run remote_monitoring_qos_experiments_cpp remote_monitor --ros-args -p fast_delay_ms:=30
```
## 観測内容
### レイテンシー計測

送信時刻 header.stamp と受信時刻 now() の差分からレイテンシーを算出します。

ログ例:
```Plain text
CMD   id=428 latency=49.08ms
FAST  id=2503 latency=33.97ms
```
### 欠番検出
メッセージに含まれる連番IDを用いて、取りこぼしを検出します。

ログ例:
```Plain text
FAST DROP? expected=2145 got=2147
CMD DROP? expected=429 got=430
```
## 実験結果
### Experiment A

条件:
- FAST topic
 - QoS: best_effort
 - depth: 1
 - publish周期: 10ms
- CMD topic
 - QoS: reliable
 - depth: 1
- monitor処理遅延
 - fast_delay_ms = 30

観測:
- FASTトピックでは頻繁に DROP? が発生
- CMDトピックでも欠番が発生する場合があった
- latency は数ms〜50ms程度で変動した

ログ例:
```Plain text
FAST DROP? expected=2145 got=2147
CMD DROP? expected=429 got=430
CMD id=428 latency=49.08ms
```
考察:

高頻度トピックに対して受信側処理が追いつかない場合、
best_effort + depth=1 の構成では古いデータが捨てられやすく、
最新値優先の代わりに欠番が発生しやすいことを確認できました。

また、CMDトピックは reliable であっても、
subscriber側の処理詰まりや小さなdepthにより、結果的に欠番が発生するケースがありました。

これは、QoSだけでなく受信処理時間とキュー深さも通信品質を左右することを示しています。

### Experiment B

条件:

- FAST topic
 - QoS: best_effort
 - depth: 1
- CMD topic
 - QoS: reliable
 - depth: 10
- monitor処理遅延
 - fast_delay_ms = 30

観測:
- CMDの DROP? は減少
- その一方で、CMDの latency が増加しやすくなった

考察:
CMDトピックの depth を増やすことで、
受信処理が一時的に詰まっても取りこぼしは減らせる一方、
キューにメッセージが溜まることで遅延が蓄積しやすくなります。

つまり、
- depth を小さくする → 欠落しやすいが遅延は溜めにくい
- depth を大きくする → 欠落は減るが遅延が増えやすい
というトレードオフが確認できました。

## 学び

この実験から、ROS2における遠隔監視系の通信設計では、
次の要素をセットで考える必要があると分かりました。
- QoS（reliable / best_effort）
- queue depth
- subscriber処理時間
- executor / callback実行モデル

QoSを選ぶだけでは不十分で、
「どのトピックに対して、欠落と遅延のどちらをより許容できるか」
を整理した上で設計する必要があります。

## 設計メモ
遠隔監視系では、トピックごとに要求が異なります。

### 高頻度監視データ
例: 状態可視化、映像メタデータ、姿勢情報
- 最新値を見たい
- 古いデータが溜まるとオペレーションしづらい
→ best_effort + 小さいdepth が有効な場合がある

### 診断・状態通知
例: diagnostics、fault、mode
- 低頻度でも確実に取りたい
→ reliable + 中程度のdepth が向く

### 操作・制御コマンド
例: teleop、mode切替
- 欠落も困る
- ただし古いコマンドが溜まるのも危険
→ reliable を前提にしつつ、depthは小さすぎず大きすぎず設計する必要がある