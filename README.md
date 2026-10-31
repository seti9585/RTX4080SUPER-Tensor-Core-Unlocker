# RTX 4080 SUPER Tensor Core Unlocker

**English** | [日本語](#日本語)

A UEFI application that unlocks Tensor Core performance on a **tested RTX 4080 SUPER** before Windows starts.

After the unlock and GPU reset, it restores the display using the NVIDIA GOP from **the same GPU's own Option ROM**, then starts Windows. On the tested system, **one RTX 4080 SUPER handles both display and compute**—no integrated graphics or second GPU is required.

In measured dense matrix multiplication, the unlocked GPU achieved approximately **1.7–2.0× FP16/BF16/FP8 throughput**, depending on the workload. This is **not** a measured Stable Diffusion or application-level speedup.

```text
                        Normal boot     Unlocked       Speedup
FP16  4096 x 4096         91.45         167.19 TFLOPS   1.83x
BF16  4096 x 4096         96.18         167.77 TFLOPS   1.74x
FP8   4096 x 4096        179.85         360.22 TFLOPS   2.00x
```

These numbers are from the tested hardware, not a general performance guarantee. See [Benchmarks](#benchmarks) for the measurement method and all results.

## What it does

- **Unlock before Windows boots.** Checks the required hardware and firmware conditions, performs the unlock, and verifies the resulting state.
- **Recover the display on the same GPU.** After a GPU function-level reset (FLR), loads the native NVIDIA GOP to restore video output and continue to Windows.
- **No bundled NVIDIA firmware payloads.** The EFI retrieves the GOP from the GPU's Option ROM at boot. A Windows setup tool locally obtains, extracts or generates the five required runtime firmware files.
- **Check before changing hardware state.** GPU identity, ROM/GOP hashes, runtime assets, PCI resources, driver ownership and DMA cleanup are checked. The hardware attempt requires an explicit **`U`** confirmation.
- **Boot from removable media.** No VBIOS flashing or installation into the internal EFI System Partition is required.

## Tested configuration

| Item | Verified environment |
| --- | --- |
| GPU | ZOTAC GeForce RTX 4080 SUPER 16 GB (AD103) |
| PCI device / subsystem | `10de:2702` / `19da:4702` |
| System BIOS | Version 3607 on the tested PC; this number alone does not identify a compatible motherboard |
| Operating system | Windows 10 Pro 22H2, 64-bit |
| NVIDIA driver | 616.92 |
| Display | Single RTX 4080 SUPER; no integrated or secondary GPU |
| Boot device | Writable FAT32 USB SSD, x64 UEFI |
| Secure Boot | **Disabled** (the EFI refuses to proceed when enabled) |

**Compatibility is deliberately narrow.** The program checks specific GPU, Option ROM and firmware/driver profiles. Another RTX 4080 SUPER board, VBIOS, motherboard, GPU model or operating system has **not** been validated. Matching the product name alone is not sufficient.

## Installation

### 1. Prepare the USB drive

Download the repository with **Code → Download ZIP**, or clone it.

You need **Windows**, **Python 3.12 or newer** and the Windows **`py` launcher** to prepare the runtime assets. `prepare-assets.bat` installs `zstandard==0.25.0` into a private `.setup-venv` when needed. Python and CUDA are **not** required to run the prepared EFI.

Use a writable **FAT32** USB drive. **Back up any existing boot files before copying anything.** Copy the repository's `EFI` and `RTX4080-EXPERIMENT` folders to the **root** of the intended USB drive. This does not require formatting the drive or modifying the PC's internal boot partition.

Initially, `RTX4080-EXPERIMENT/ASSETS` contains only `.gitkeep`; the five runtime files are prepared in the next step.

### 2. Identify the correct drive

Open a command prompt in the extracted repository folder. Replace `X:\` with the USB drive's actual root path:

```bat
prepare-assets.bat --inspect-target X:\
```

Check the displayed **volume GUID, label, filesystem, EFI path and EFI SHA-256**. The setup tool does not rely on a fixed personal drive GUID or volume label. It rejects the system volume and requires FAT32 and the exact release EFI.

### 3. Prepare and install the firmware locally

Review the [upstream terms](#license--ライセンス), then use the GUID displayed by the inspection command:

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "GUID_FROM_INSPECTION" --accept-terms
```

The tool obtains the specified source archives, checks their hashes, extracts or generates the required firmware and verifies the final five files before installing them.

- If **all five** existing files already match, acquisition and copying are skipped.
- If only some are missing, **only the missing files** are added.
- Existing files with **unexpected hashes are not overwritten**.
- The target volume is rechecked before writes. Recoverable failures remove newly added files; power-loss recovery is not guaranteed.

**Offline preparation**, using previously cached archives with the pinned hashes:

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "GUID_FROM_INSPECTION" --offline --cache local\cache
```

The first-time Python dependency installation may still need network access.

**Verify the five files without deploying anything:**

```bat
prepare-assets.bat --verify-only X:\RTX4080-EXPERIMENT\ASSETS
```

After successful preparation, the USB layout is:

```text
X:\
├─ EFI\
│  ├─ BOOT\
│  │  └─ BOOTX64.EFI
│  └─ RTX4080SUPER-TENSOR-CORE-UNLOCKER.EFI
└─ RTX4080-EXPERIMENT\
   └─ ASSETS\
      ├─ booter_load_ad102.bin
      ├─ gsp_boot_ad102.bin
      ├─ gsp_boot_desc_ad102.bin
      ├─ gsp_ga10x.bin
      └─ v67-ad102-610.43.03-four-plm.bin
```

Do not rename these runtime paths. The five firmware files, downloaded archives, caches and locally extracted GOP images are **not included in the repository**.

## Booting and recovery

1. Save your work and select the USB drive's **UEFI** entry from your firmware's one-time boot menu.
2. The program checks the hardware and prepares the native GOP **before** prompting for an unlock attempt.
3. If all preflight checks pass, press **`U` once** to proceed. **Esc**, or no confirmation within **30 seconds**, cancels.
4. On success, the program restores the display and starts Windows Boot Manager. Check the GPU in **Device Manager** before running workloads.

**Important:** The application changes GPU state and performs a reset. A black screen, freeze or failed boot is possible. If **STOP** appears after the hardware attempt, or display/Windows recovery fails, **power off** and examine the report before retrying.

Reports are saved under `RTX4080-EXPERIMENT/RUNxx/REPORT.TXT` on the USB drive.

To boot without the unlocker, choose the normal **Windows Boot Manager** instead of the USB drive. Restore any backed-up USB boot files manually if necessary. The application does **not** flash the VBIOS or establish a permanent unlock; behavior across reboot, sleep and other power-state transitions has not been established.

## Benchmarks

The following measurements compare normal Windows startup (October 30, 2026) with startup through the unlocker (November 1, 2026) on the tested system.

| Benchmark | Matrix | Input | Normal (TFLOPS) | Unlocked (TFLOPS) |
| --- | ---: | --- | ---: | ---: |
| PyTorch | 4096 | FP16 | 91.45 | 167.19 |
| PyTorch | 4096 | BF16 | 96.18 | 167.77 |
| PyTorch | 4096 | FP32 control | 32.48 | 32.76 |
| PyTorch | 8192 | FP16 | 101.72 | 168.06 |
| PyTorch | 8192 | BF16 | 101.02 | 172.10 |
| PyTorch | 8192 | FP32 control | 33.48 | 33.64 |
| cuBLASLt | 4096 | FP8 E4M3/E4M3 | 179.85 | 360.22 |
| cuBLASLt | 4096 | FP8 E4M3/E5M2 | 186.78 | 360.10 |
| cuBLASLt | 4096 | BF16 control | 91.24 | 177.21 |
| cuBLASLt | 8192 | FP8 E4M3/E4M3 | 200.98 | 360.80 |
| cuBLASLt | 8192 | FP8 E4M3/E5M2 | 203.70 | 358.77 |
| cuBLASLt | 8192 | BF16 control | 102.96 | 196.97 |

**Measurement conditions**

- **PyTorch:** Python 3.12.10, PyTorch `2.11.0+cu128`, CUDA runtime 12.8; dense `torch.mm`, TF32 disabled.
- **Native FP8:** Separate C++/CUDA program, CUDA Toolkit 13.0, cuBLASLt reported version 130100, SM 8.9. FP8 inputs E4M3/E4M3 or E4M3/E5M2, BF16 output, FP32 compute, scale 1, fast accumulation disabled, TN column-major. FP8 algorithm ID 35; BF16 control ID 21. Executable and cuBLASLt DLL hashes matched between comparisons.
- **Timing:** 8 warm-up calls, followed by 7 samples of 15 calls, using CUDA Events. Results are medians of time per call.
- **Correctness checks:** All 12 cases passed finite-output checks and sampled reference comparisons (1 element for PyTorch, 16 for native CUDA).

**Limits:** These results measure **dense matrix throughput**, not Stable Diffusion generation speed, full-model accuracy or full-matrix numerical equivalence. GPU clocks, temperature, power and competing workloads were not fixed. Repeated-boot performance varied, and the cause of that variation has not been established. Long-term stability and broad hardware compatibility remain untested; unlock registers were not independently read back from Windows.

## Release files and building from source

Both included EFI copies have the same content:

```text
File size: 1,277,440 bytes
SHA-256:   b6e0ecea47b8ca2d8b17c742d3960e60016a792d0b86fa86cefc13a72a1fcb35
```

See [`SHA256SUMS.txt`](SHA256SUMS.txt) for the repository file hashes.

The corresponding production source is included under [`source/`](source/) with [`build-efi.ps1`](build-efi.ps1). Rebuilding with **Zig 0.14.1** reproduced the distributed EFI byte-for-byte:

```powershell
.\build-efi.ps1 -Zig 'C:\path\to\zig.exe'
```

The build writes an EFI file under `build/` without deploying or executing it. No NVIDIA firmware assets or extracted GOP are needed to compile the EFI.

---

# 日本語

Windows起動前に、**RTX 4080 SUPERのTensor Core性能制限を解除するUEFIアプリケーション**です。

解除後はGPUをリセットし、そのGPU自身のOption ROMに収録されている**NVIDIA純正GOP**を使って画面を復帰させ、Windowsを起動します。実機検証では、**内蔵GPUや予備のグラフィックボードを使わず、RTX 4080 SUPERの1枚だけで画面表示と演算を両立**できました。

密行列積の実測では、FP16・BF16・FP8の演算性能が、条件により**約1.7～2.0倍**になりました。ただし、これは**Stable Diffusionの画像生成速度を測定した結果ではありません**。

```text
                        通常起動       解除後         倍率
FP16  4096 x 4096       91.45        167.19 TFLOPS   1.83倍
BF16  4096 x 4096       96.18        167.77 TFLOPS   1.74倍
FP8   4096 x 4096      179.85        360.22 TFLOPS   2.00倍
```

測定条件と全12項目の数値は、後述の[性能測定](#性能測定)に掲載しています。

## 主な機能

- **Windows起動前に解除：** GPUとファームウェアの状態を確認し、解除処理を実行して結果を照合します。
- **GPU単独で画面を復帰：** FLR（GPUの機能単位リセット）後にNative NVIDIA GOPを読み込み、画面表示を復帰してWindowsへ引き渡します。
- **NVIDIA由来のファームウェア本体を非同梱：** GOPは起動時にGPU自身のROMから取得します。必要な外部5ファイルは、利用者のWindows上で取得・抽出・生成します。
- **異常時は処理を中断：** GPU識別情報、ROM/GOPのハッシュ、必要ファイル、PCI資源、デバイス所有権、DMAの後始末などを検査します。解除には明示的な**`U`キー確認**が必要です。
- **USBから一時起動：** VBIOSの書き換えや、内蔵ディスクのEFIシステムパーティションへのインストールは不要です。

## 対応確認済み環境

| 項目 | 実機検証した構成 |
| --- | --- |
| GPU | ZOTAC GeForce RTX 4080 SUPER 16GB（AD103） |
| PCI device / subsystem | `10de:2702` / `19da:4702` |
| システムBIOS | 検証機のバージョン3607（番号だけで対応マザーボードを特定できるわけではありません） |
| OS | Windows 10 Pro 22H2、64-bit |
| NVIDIAドライバ | 616.92 |
| 画面表示 | RTX 4080 SUPERのみ。内蔵GPU・追加GPUなし |
| 起動媒体 | 書き込み可能なFAT32のUSB SSD、x64 UEFI |
| Secure Boot | **無効**（有効状態ではEFIが実行を拒否します） |

**動作条件は限定的です。** GPU名だけでなく、Option ROMやファームウェア・ドライバの固有条件も照合します。同じRTX 4080 SUPERでも、別メーカー・別VBIOS・異なる環境での動作は確認していません。

## 導入方法

### 1. USB起動媒体を用意する

GitHubの**Code → Download ZIP**からダウンロードして展開するか、リポジトリをcloneします。

外部ファームウェアの準備には、**Windows、Python 3.12以降、`py`ランチャー**が必要です。`prepare-assets.bat`は、必要に応じて専用環境`.setup-venv`を作成し、`zstandard==0.25.0`を導入します。準備済みEFIの**実行自体にPythonやCUDAは不要**です。

FAT32形式の書き込み可能なUSB媒体を用意してください。**既存の起動ファイルはコピー前にバックアップしてください。** リポジトリ内の`EFI`と`RTX4080-EXPERIMENT`を、使用するUSBの**ルート**にコピーします。USBのフォーマットや内蔵ディスクの起動領域の変更は不要です。

この時点では`RTX4080-EXPERIMENT/ASSETS`に`.gitkeep`しかなく、外部ファームウェア5ファイルは次の手順で準備します。

### 2. USBを識別する

展開したフォルダーでコマンドプロンプトを開きます。`X:\`を実際のUSBドライブ文字に置き換えて実行してください。

```bat
prepare-assets.bat --inspect-target X:\
```

表示された**ボリュームGUID、ラベル、ファイルシステム、EFIの場所、SHA-256**を確認します。個人固有のGUIDや特定のボリュームラベルは固定されていません。ツールはシステムドライブを拒否し、FAT32と配布EFIのハッシュを照合します。

### 3. ファームウェアを取得・生成して配置する

末尾の[利用条件](#license--ライセンス)と取得元を確認したうえで、手順2で表示されたGUIDを指定します。

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "表示されたGUID" --accept-terms
```

必要な原本を取得してハッシュを確認し、外部5ファイルを抽出・生成します。生成したファイルも照合してからUSBへ配置します。

- **5ファイルすべて一致済み：** ダウンロード・生成・コピーを省略します。
- **不足ファイルがある：** 不足分だけを追加します。
- **既存ファイルのハッシュが違う：** 自動上書きせずに停止します。
- **配置中の異常：** 書き込み前に媒体を再確認します。復元可能な失敗では新規追加分を取り除きますが、停電や媒体切断からの完全復旧は保証しません。

**原本アーカイブを取得済みの場合（オフライン準備）：**

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "表示されたGUID" --offline --cache local\cache
```

初回のPython依存パッケージ導入にはネット接続が必要な場合があります。

**配置済み5ファイルの照合だけを行う場合：**

```bat
prepare-assets.bat --verify-only X:\RTX4080-EXPERIMENT\ASSETS
```

準備後のUSB内の構成は次のとおりです。

```text
X:\
├─ EFI\
│  ├─ BOOT\
│  │  └─ BOOTX64.EFI
│  └─ RTX4080SUPER-TENSOR-CORE-UNLOCKER.EFI
└─ RTX4080-EXPERIMENT\
   └─ ASSETS\
      ├─ booter_load_ad102.bin
      ├─ gsp_boot_ad102.bin
      ├─ gsp_boot_desc_ad102.bin
      ├─ gsp_ga10x.bin
      └─ v67-ad102-610.43.03-four-plm.bin
```

実行用のフォルダー名・ファイル名は変更しないでください。**完成済みのNVIDIAファームウェア5ファイルや抽出済みGOPは、リポジトリに同梱していません。**

## 起動方法・復旧方法

1. 作業中のファイルを保存し、PCの一時起動メニューからUSBの**UEFI起動**を選びます。
2. GPUの識別・状態検査とNative GOPの準備が実行されます。いずれかに失敗した場合、解除処理へ進みません。
3. 確認画面で**`U`を1回**押すと解除を実行します。**Esc**または**30秒間操作しない場合**はキャンセルします。
4. 正常終了すると画面表示を復帰し、Windows Boot Managerへ引き渡します。起動後、デバイスマネージャーでGPUの状態を確認してください。

**重要：** GPUの状態変更とリセットを行うため、画面消失・フリーズ・起動失敗の可能性があります。解除処理開始後に`STOP`が出た場合や画面・Windowsの復帰に失敗した場合は、**電源を切り、ログを確認するまで再試行しないでください。**

実行ログはUSBの`RTX4080-EXPERIMENT/RUNxx/REPORT.TXT`に保存されます。

通常のWindows起動に戻す場合は、USBではなく**Windows Boot Manager**を選びます。USB上の起動ファイルを戻す場合は、保存したバックアップからWindows上で手動復元してください。本ツールはVBIOSを書き換えず、恒久的な解除を行うものでもありません。再起動・スリープ等をまたぐ解除状態の維持は未検証です。

## 性能測定

以下は、検証機における**通常起動（2026年10月30日）**と**本ツールによる解除後（2026年11月1日）**の実測結果です。

| 測定 | 行列サイズ | 入力精度 | 通常起動 TFLOPS | 解除後 TFLOPS |
| --- | ---: | --- | ---: | ---: |
| PyTorch | 4096 | FP16 | 91.45 | 167.19 |
| PyTorch | 4096 | BF16 | 96.18 | 167.77 |
| PyTorch | 4096 | FP32対照 | 32.48 | 32.76 |
| PyTorch | 8192 | FP16 | 101.72 | 168.06 |
| PyTorch | 8192 | BF16 | 101.02 | 172.10 |
| PyTorch | 8192 | FP32対照 | 33.48 | 33.64 |
| cuBLASLt | 4096 | FP8 E4M3/E4M3 | 179.85 | 360.22 |
| cuBLASLt | 4096 | FP8 E4M3/E5M2 | 186.78 | 360.10 |
| cuBLASLt | 4096 | BF16対照 | 91.24 | 177.21 |
| cuBLASLt | 8192 | FP8 E4M3/E4M3 | 200.98 | 360.80 |
| cuBLASLt | 8192 | FP8 E4M3/E5M2 | 203.70 | 358.77 |
| cuBLASLt | 8192 | BF16対照 | 102.96 | 196.97 |

**測定条件**

- **PyTorch：** Python 3.12.10、PyTorch `2.11.0+cu128`、CUDAランタイム12.8。`torch.mm`による正方密行列積でTF32は無効。
- **FP8：** 別のC++/CUDAプログラム、CUDA Toolkit 13.0、cuBLASLt報告バージョン130100、SM 8.9。入力E4M3/E4M3またはE4M3/E5M2、出力BF16、演算FP32、scale 1、fast accumulation無効、TN column-major。FP8 algorithm ID 35、BF16対照ID 21。比較間で実行ファイル・cuBLASLt DLLのハッシュ一致を確認。
- **時間：** ウォームアップ8回、15回ずつ7区間をCUDA Eventで測定し、1回当たりの時間の中央値を採用。
- **出力：** 全12条件で有限値・参照要素の検査に合格（PyTorchは1要素、CUDAプログラムは16要素）。

**評価範囲：** これは**密行列積の演算性能**であり、Stable Diffusionの生成速度やモデル全体の精度を測った結果ではありません。行列全要素の一致確認も行っていません。クロック・温度・電力・他プロセス負荷は固定しておらず、起動ごとに測定値の変動がありましたが、その原因は未確定です。長時間安定性、別環境への互換性、Windows実行中の解除レジスタの直接読取りも未検証です。

## 配布EFI・ソースコード

配布する2つのEFIは内容が一致しています。

```text
サイズ：1,277,440バイト
SHA-256：b6e0ecea47b8ca2d8b17c742d3960e60016a792d0b86fa86cefc13a72a1fcb35
```

その他のファイルのハッシュは[`SHA256SUMS.txt`](SHA256SUMS.txt)を参照してください。

[`source/`](source/)と[`build-efi.ps1`](build-efi.ps1)に配布EFIの対応ソースとビルド手順を収録しています。**Zig 0.14.1**を使用した再ビルドでは、配布EFIとのバイト単位の一致を確認しています。

```powershell
.\build-efi.ps1 -Zig 'C:\path\to\zig.exe'
```

EFIは`build/`に生成されます。ビルドだけではUSBへの配置や実機実行は行いません。コンパイルにNVIDIA純正ファームウェアや抽出済みGOPは不要です。

---

# License / Acknowledgements / Sources

## License / ライセンス

The included source and third-party components carry **their respective licenses and notices**, not one package-wide MIT license. Refer to [`LICENSES/`](LICENSES/), [`LICENSES/UPSTREAM-FIRMWARE-NOTICE.md`](LICENSES/UPSTREAM-FIRMWARE-NOTICE.md), [`source/THIRD_PARTY_NOTICES.md`](source/THIRD_PARTY_NOTICES.md), and individual source headers. Relevant notices include GPL-2.0-only, NVIDIA MIT and EDK II BSD-2-Clause-Patent. Python dependencies have their own license terms.

収録ソースと第三者由来の構成要素には、**それぞれのライセンス・権利表示**が適用されます。配布物全体を一律のMITライセンスとして扱うものではありません。詳しくは[`LICENSES/`](LICENSES/)、[`LICENSES/UPSTREAM-FIRMWARE-NOTICE.md`](LICENSES/UPSTREAM-FIRMWARE-NOTICE.md)、[`source/THIRD_PARTY_NOTICES.md`](source/THIRD_PARTY_NOTICES.md)、各ソースファイルの表記を参照してください。

The five runtime firmware files are prepared on the user's computer; the native GOP is extracted from the user's GPU. **Not bundling them does not establish permission for every acquisition, extraction, use or redistribution.** The `--accept-terms` flag records an acknowledgement of the upstream terms; it does not grant new rights. Do not redistribute firmware, extracted GOP images or caches based solely on this repository's code licenses.

外部ファームウェア5ファイルは利用者のPCで準備し、GOPは利用者のGPUから取得します。ただし、**バイナリを同梱しないことと、取得・抽出・利用・再配布に関する条件がすべて解決していることは別問題**です。`--accept-terms`も追加の権利を与えるものではありません。コード側のライセンスだけを根拠に、生成ファームウェア・抽出GOP・キャッシュを再配布しないでください。

## Acknowledgements / 謝辞

- [TOTORONG/AD103_win_unlocker](https://huggingface.co/TOTORONG/AD103_win_unlocker) — foundation of the unlock and recovery integration / 解除・復帰処理の基盤。
- [Goldlionren/gpu-unlock-4080s](https://github.com/Goldlionren/gpu-unlock-4080s) — related RTX 4080 SUPER implementation / RTX 4080 SUPER向けの関連実装。
- [TianoCore EDK II](https://github.com/tianocore/edk2) — UEFI headers and decompression/driver references / UEFIヘッダーや展開・ドライバ処理の参照。

The projects above have their own supported configurations and test results; these should not be confused with this release's measurements. / 各プロジェクトの対応環境や測定結果は、本プロジェクトの実機測定とは別です。

## Firmware sources and references / 取得元・典拠

- [NVIDIA kernel-module source 610.43.03](https://download.nvidia.com/XFree86/NVIDIA-kernel-module-source/NVIDIA-kernel-module-source-610.43.03.tar.xz) — source archive for three AD102 boot assets / AD102起動用3ファイルの取得元。
- [CachyOS nvidia-utils 610.43.03-1](https://archive.cachyos.org/archive/cachyos-v3/nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst) — pinned archive containing the NVIDIA GSP firmware / NVIDIA GSPファームウェアの取得元。
- **V67** — generated locally by `ad102_payload.payload()`; the completed binary is not downloaded / 既存関数でローカル生成し、完成済みバイナリは取得しません。
- [NVIDIA Driver License Agreement](https://www.nvidia.com/en-us/drivers/nvidia-license/) and [upstream license policy](https://huggingface.co/TOTORONG/AD103_win_unlocker/blob/51df9c1/LICENSE-POLICY.md) — terms to review, not a blanket grant of redistribution permission / 適用条件の参照先であり、包括的な再配布許諾を示すものではありません。
- [PyTorch `torch.mm`](https://docs.pytorch.org/docs/stable/generated/torch.mm.html) and [NVIDIA cuBLAS documentation](https://docs.nvidia.com/cuda/cublas/index.html) — benchmark API references; the performance figures above are from this project's saved measurements / 測定APIの参照先。性能数値は本プロジェクトの保存済み実測値です。
