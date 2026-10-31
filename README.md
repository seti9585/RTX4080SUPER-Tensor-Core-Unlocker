# RTX 4080 SUPER Tensor Core Unlocker

English | [日本語](#日本語)

Unlock Tensor Core performance before Windows starts, restore display through the native NVIDIA GOP, and use the same tested RTX 4080 SUPER for display and compute without an integrated or secondary GPU.

The application uses the GPU's own native display firmware to restore the screen after unlocking. It reads and decompresses the GOP from the GPU's Option ROM and verifies it before asking you to proceed. A Windows setup tool downloads the required source archives, extracts or generates the runtime firmware, and verifies each file before placing it on the boot drive.

## Tested scope

| Item | Tested configuration |
|---|---|
| GPU | ZOTAC RTX 4080 SUPER 16 GB, AD103 |
| PCI device / subsystem | `10de:2702` / `19da:4702` |
| System BIOS | 3607 on the tested machine; the version number alone does not identify a compatible motherboard |
| OS / NVIDIA driver | Windows 10 Pro 22H2 64-bit / 616.92 |
| Display | Same GPU for display and compute; no additional GPU used |
| Boot media | FAT32-formatted USB flash drive, x64 UEFI |
| Secure Boot | Disabled; the application refuses an enabled state |

Compatibility remains tied to the pinned GPU, Option ROM and firmware/driver profiles. Dynamic extraction does not add support for other RTX 4080 SUPER boards, VBIOS versions, other GPUs or operating systems.

Before unlocking, the application checks the GPU identity, ROM and GOP hashes, executable structure, PCI resources, ownership and DMA state. The boot screen displays `EXPERIMENTAL ARM` and requires explicit confirmation before changing GPU state.

## Package and setup

Requirements for setup: Windows, Python **3.12 or newer** with the `py` launcher, and `zstandard==0.25.0`. The batch launcher installs that dependency into a private `.setup-venv` when preparation requires it. Read-only inspection and verification do not create that environment. Python and the CUDA Toolkit are not required to execute the prepared EFI.

1. Extract the package and verify files against `SHA256SUMS.txt`.
2. Back up existing boot files. Copy `EFI/` and `RTX4080-EXPERIMENT/` to the intended FAT32-formatted USB flash drive's root.
3. Inspect the selected drive. `X:` below is an example drive letter, not a required one:

```bat
prepare-assets.bat --inspect-target X:\
```

4. Check the displayed GUID, label, filesystem, EFI path and SHA-256 against the intended drive. After reviewing the upstream terms below, explicitly confirm the displayed GUID to prepare and deploy:

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "GUID_FROM_INSPECTION" --accept-terms
```

The setup requires FAT32, the explicitly confirmed volume GUID and the exact release EFI hash. The system drive and aliases of its volume are refused. There is no fixed personal GUID or required volume label. Identity is rechecked before replacement.

All five matching files cause acquisition, generation, backup and copying to be skipped. If some files are missing, only those files are added; existing mismatched files are refused. Staged copies and final files are verified. Recoverable failures remove newly added files and preserve existing ones. If the volume identity is lost or changes, the tool stops without further target writes; staging files may require manual review on the original volume. Power-loss recovery is not guaranteed.

For already downloaded pinned archives:

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "GUID_FROM_INSPECTION" --offline --cache local\cache
```

The batch launcher's dependency installation can still need network access on first use. To verify files only, with no deployment:

```bat
prepare-assets.bat --verify-only X:\RTX4080-EXPERIMENT\ASSETS
```

After setup, the runtime layout is:

```text
EFI/BOOT/BOOTX64.EFI
EFI/RTX4080SUPER-TENSOR-CORE-UNLOCKER.EFI
RTX4080-EXPERIMENT/ASSETS/booter_load_ad102.bin
RTX4080-EXPERIMENT/ASSETS/gsp_boot_ad102.bin
RTX4080-EXPERIMENT/ASSETS/gsp_boot_desc_ad102.bin
RTX4080-EXPERIMENT/ASSETS/gsp_ga10x.bin
RTX4080-EXPERIMENT/ASSETS/v67-ad102-610.43.03-four-plm.bin
```

Do not rename runtime directories or assets. Keep generated firmware, downloaded archives and caches for your own use; see the license section before redistributing third-party material.

## Boot and rollback

Choose the USB flash drive's UEFI entry in the one-time boot menu. GOP preparation must succeed before the confirmation prompt. Press **`U` once** to attempt the unlock; Esc or 30 seconds without confirmation cancels. Full success restores display and hands control to Windows Boot Manager. Check Device Manager before using workloads.

**After a hardware attempt, STOP, failed display recovery or failed Windows boot requires power-off. Do not retry before reviewing the log.** Reports are saved under `RTX4080-EXPERIMENT/RUNxx/REPORT.TXT`. Preflight failure before an attempt is handled separately.

The EFI does not restore or overwrite its own boot file. Restore preserved boot files manually from Windows if needed. To boot Windows without running the unlocker, choose Windows Boot Manager rather than the USB drive. This is not a VBIOS flashing or permanent-install procedure. Reboot, sleep and other power-state persistence have not been established.

Both distributed EFI copies are **1,277,440 bytes**, SHA-256:

```text
b6e0ecea47b8ca2d8b17c742d3960e60016a792d0b86fa86cefc13a72a1fcb35
```

## Measurements

Python **3.12.10**, PyTorch **2.11.0+cu128**, CUDA runtime **12.8**, NVIDIA driver **616.92**. PyTorch used dense square `torch.mm` with TF32 disabled. FP8 was a separate native C++/CUDA test: CUDA Toolkit **13.0**, cuBLASLt reported version **130100**, SM 8.9, E4M3/E4M3 or E4M3/E5M2 input, BF16 output, FP32 compute, scale 1, fast accumulation disabled, TN column-major. Algorithm IDs: FP8 35, BF16 control 21. The FP8 executable and DLL hashes matched between comparisons.

Both suites used 8 warm-up calls, followed by 7 samples of 15 calls, measured with CUDA Events using the median time per call. Normal-boot results were measured on October 30, 2026; unlocked results below were measured on November 1, 2026 on the same system.

| Suite | Size | Input type | Normal TFLOPS | Unlocked TFLOPS |
|---|---:|---|---:|---:|
| PyTorch | 4096 | float16 | 91.45 | 167.19 |
| PyTorch | 4096 | bfloat16 | 96.18 | 167.77 |
| PyTorch | 4096 | float32 | 32.48 | 32.76 |
| PyTorch | 8192 | float16 | 101.72 | 168.06 |
| PyTorch | 8192 | bfloat16 | 101.02 | 172.10 |
| PyTorch | 8192 | float32 | 33.48 | 33.64 |
| cuBLASLt | 4096 | E4M3_E4M3 | 179.85 | 360.22 |
| cuBLASLt | 4096 | E4M3_E5M2 | 186.78 | 360.10 |
| cuBLASLt | 4096 | BF16_CONTROL | 91.24 | 177.21 |
| cuBLASLt | 8192 | E4M3_E4M3 | 200.98 | 360.80 |
| cuBLASLt | 8192 | E4M3_E5M2 | 203.70 | 358.77 |
| cuBLASLt | 8192 | BF16_CONTROL | 102.96 | 196.97 |

All 12 cases passed output checks: finite outputs plus one reference element for PyTorch, and 16 reference elements for native CUDA. This is not full-matrix accuracy validation. Clocks, temperature, power and competing workloads were not fixed, so differences between boots are not attributed to a specific cause. These are dense-matrix TFLOPS, not sparse peak specifications or measured image-generation speedups. Long-duration stability, broad hardware compatibility, full-model accuracy and image equivalence remain outside these tests. The unlock registers were not read directly from Windows.

## Corresponding source

`source/` and `build-efi.ps1` provide the corresponding source and build instructions for the distributed EFI. A rebuild from the distributed production source with Zig **0.14.1** reproduced the distributed EFI byte-for-byte. No firmware assets or extracted GOP are required to compile it.

```powershell
.\build-efi.ps1 -Zig 'C:\path\to\zig.exe'
```

The output is `build/DYNAMIC-GOP-CANDIDATE.EFI`; the build does not deploy or execute it. `source/test-dynamic.ps1` is a host-test helper requiring separately held, pinned fixtures. No such binary fixtures are included.

---

# 日本語

Windows起動前にTensor Core性能を解除し、Native NVIDIA GOPで画面表示を復帰するUEFIアプリケーションです。検証構成では、追加GPUや内蔵GPUなしで、同じRTX 4080 SUPERからWindowsの表示・演算まで動作しました。

GPU自身の表示用ファームウェアを利用し、解除後に画面を復帰します。GPUのOption ROMからGOPを読み出して展開し、実行確認前に検証します。必要な実行用ファームウェアは、Windows上のセットアップツールで取得・抽出・生成し、ファイルの整合性を確認して起動媒体に配置します。

## 対応確認済み環境

ZOTAC RTX 4080 SUPER 16 GB / AD103、PCI `10de:2702`、subsystem `19da:4702`、検証機BIOS 3607、Windows 10 Pro 22H2 64-bit、NVIDIAドライバ616.92。起動媒体にはFAT32でフォーマットしたUSBメモリを使用し、x64 UEFIで起動します。Secure Bootは無効にします。BIOSの番号だけで対応マザーボードを特定することはできません。

対応するGPU・VBIOS・ファームウェアを照合し、ROMとGOPのハッシュ、実行形式、PCI資源・DMA・所有権を検査します。同じRTX 4080 SUPERでも、別ボードや別VBIOSでの動作を保証するものではありません。起動画面には`EXPERIMENTAL ARM`と表示され、GPUの状態を変更する前に実行確認を求めます。

## 導入

セットアップにはWindows、Python **3.12以降**と`py`ランチャーが必要です。バッチは必要時に専用環境へ`zstandard==0.25.0`を導入します。読み取り専用の確認では専用環境を作成しません。EFIの起動自体にはPythonやCUDA Toolkitは不要です。

1. 展開後、`SHA256SUMS.txt`と照合します。
2. 既存の起動ファイルをバックアップし、`EFI`と`RTX4080-EXPERIMENT`を目的のFAT32 USBメモリのルートへコピーします。
3. 次のコマンドで対象を表示します。`X:`は例であり、使用するドライブ文字へ置き換えます。

```bat
prepare-assets.bat --inspect-target X:\
```

4. 表示されたGUID・ラベル・FAT32・EFIパス・EFIのSHA-256を確認します。上流の利用条件を確認したうえで、表示されたGUIDを明示して実行します。

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "表示されたGUID" --accept-terms
```

利用者が指定したGUID、FAT32、配布EFIのハッシュが一致した場合だけ配置します。個人固有GUIDや特定のラベルは固定していません。システムドライブとその別名を拒否し、書き込み直前にも再照合します。

5ファイルが一致済みなら取得・生成・バックアップ・コピーを省略します。一部不足なら不足分だけ配置し、既存の不一致ファイルを自動上書きしません。コピー前後に照合し、復元可能な途中エラーでは今回追加したファイルを取り除きます。媒体の識別が変わったり取得できなくなった場合は、それ以上書き込まず停止します。その場合、元の媒体の一時ファイルを確認する必要があります。停電・切断時まで復元を保証するものではありません。

取得済みの固定アーカイブを使う場合：

```bat
prepare-assets.bat --target X:\ --confirm-volume-guid "表示されたGUID" --offline --cache local\cache
```

初回の依存パッケージ導入には別途ネット接続が必要な場合があります。検証だけを行う場合：

```bat
prepare-assets.bat --verify-only X:\RTX4080-EXPERIMENT\ASSETS
```

セットアップ後の配置は英語セクションの構成図を参照してください。実行用フォルダー名・ファイル名は変更しないでください。取得・生成したファイルを第三者へ配布する場合は、末尾のライセンスに関する説明を確認してください。

## 起動と切り戻し

一時起動メニューからUSBメモリのUEFI起動を選びます。GOP準備が成功した後の確認画面で`U`を1回押すと、解除・FLR・GOP復帰を実行してWindowsへ引き渡します。Esc、または30秒間確認しない場合はキャンセルします。WindowsでGPUが正常認識されていることを確認してください。

**実行開始後のSTOP・画面復帰失敗・Windows起動失敗では電源を切り、記録確認前に再試行しないでください。** ログは`RTX4080-EXPERIMENT/RUNxx/REPORT.TXT`へ保存します。実行前の検査失敗は別の終了経路です。

EFI自身による起動ファイルの自動復元は行いません。必要ならWindowsから保全済み起動ファイルを手動復元します。アンロッカーを使わず通常起動する場合は、USBではなくWindows Boot Managerを選びます。VBIOSを書き換える導入手順ではなく、再起動・スリープ等をまたぐ効果の維持は検証していません。

2つの配布EFIは同一で、サイズは**1,277,440バイト**、SHA-256は次の値です。

```text
b6e0ecea47b8ca2d8b17c742d3960e60016a792d0b86fa86cefc13a72a1fcb35
```

## 実測結果

Python **3.12.10** / PyTorch **2.11.0+cu128** / PyTorch CUDA **12.8** / NVIDIA driver **616.92**。PyTorchは`torch.mm`による正方密行列積、TF32無効です。

FP8は別のC++/CUDAプログラムで、CUDA Toolkit **13.0**、cuBLASLt報告バージョン**130100**、SM 8.9。入力E4M3/E4M3またはE4M3/E5M2、出力BF16、演算FP32、scale 1、fast accumulation無効、TN column-major。algorithm IDはFP8が35、BF16対照が21です。比較間でFP8実行ファイルとDLLのハッシュ一致を確認しました。

両測定ともウォームアップ8回、15回ずつ7区間をCUDA Eventで測り、1回当たり時間の中央値を使用しました。通常起動は2026年10月30日、解除後は2026年11月1日に同じ構成で測定した値です。

| 測定 | サイズ | 入力精度 | 通常 TFLOPS | 解除後 TFLOPS |
|---|---:|---|---:|---:|
| PyTorch | 4096 | float16 | 91.45 | 167.19 |
| PyTorch | 4096 | bfloat16 | 96.18 | 167.77 |
| PyTorch | 4096 | float32 | 32.48 | 32.76 |
| PyTorch | 8192 | float16 | 101.72 | 168.06 |
| PyTorch | 8192 | bfloat16 | 101.02 | 172.10 |
| PyTorch | 8192 | float32 | 33.48 | 33.64 |
| cuBLASLt | 4096 | E4M3_E4M3 | 179.85 | 360.22 |
| cuBLASLt | 4096 | E4M3_E5M2 | 186.78 | 360.10 |
| cuBLASLt | 4096 | BF16_CONTROL | 91.24 | 177.21 |
| cuBLASLt | 8192 | E4M3_E4M3 | 200.98 | 360.80 |
| cuBLASLt | 8192 | E4M3_E5M2 | 203.70 | 358.77 |
| cuBLASLt | 8192 | BF16_CONTROL | 102.96 | 196.97 |

全12条件で出力検査に合格しました。全出力の有限値と、PyTorchは1要素、ネイティブCUDAは16要素の参照値を確認しています。行列全要素の高精度一致検査ではありません。温度・クロック・電力・他プロセス負荷は固定しておらず、起動間の差の原因は特定していません。数値は密行列積の換算性能で、疎行列の公称値や画像生成速度とは区別してください。

長時間安定性、広範な機種互換性、モデル全体の精度、生成画像の同等性は別の評価です。Windows上で解除レジスタを直接読み直した測定ではありません。

## 対応ソース

`source/`と`build-efi.ps1`に、配布EFIの対応ソースとビルド手順を収録しています。Zig **0.14.1**で配布する製品側ソースから再ビルドし、配布EFIとのバイト一致を確認しました。コンパイルに純正ファームウェアや抽出済みGOPは不要です。

```powershell
.\build-efi.ps1 -Zig 'C:\path\to\zig.exe'
```

出力は`build/DYNAMIC-GOP-CANDIDATE.EFI`です。ビルドは配置や起動を行いません。`source/test-dynamic.ps1`は別途保持する固定入力が必要なホスト試験補助です。その入力バイナリは同梱していません。

---

# License / Acknowledgements / Sources

## License / ライセンス

See also `source/THIRD_PARTY_NOTICES.md` for provenance, missing historical-notice limitations and the retained upstream firmware notice. Component terms and notices are retained in `LICENSES/` and source headers: GPL-2.0-only, NVIDIA MIT, EDK II BSD-2-Clause-Patent and relevant upstream notices. Setup scripts preserve GPL-2.0-only notices. This is not a single MIT license over the entire package. Python dependencies have their own licenses.

各構成要素の条件・表示は`LICENSES/`とソース内に保持しています。配布物全体を一律にMITとするものではありません。セットアップコードのGPLは、取得・生成するNVIDIAファームウェアに同じ権利が及ぶことを意味しません。

Third-party firmware and the GOP are obtained on the user's computer or GPU. Their acquisition and use remain subject to the applicable terms. `--accept-terms` records the user's review decision for acquisition; it does not grant additional rights. Do not redistribute generated firmware, extracted GOPs or caches based solely on this package's code licenses.

第三者のファームウェアとGOPは、利用者のPCまたはGPU上で取得します。取得・抽出・利用には、それぞれの適用条件を確認してください。`--accept-terms`も新しい権利を与えるものではありません。コードのライセンスだけを根拠に、生成ファームウェアやGOP・キャッシュを再配布しないでください。

## Acknowledgements / 謝辞

- [TOTORONG/AD103_win_unlocker](https://huggingface.co/TOTORONG/AD103_win_unlocker): foundation for the existing unlock/recovery integration / 解除・復旧統合の基盤。
- [Goldlionren/gpu-unlock-4080s](https://github.com/Goldlionren/gpu-unlock-4080s): related implementation consulted during development / 開発時の比較参照。
- [TianoCore EDK II](https://github.com/tianocore/edk2): UEFI headers and decompression/driver references / UEFIヘッダーと展開・ドライバ処理の参照。

Their supported environments and measurements are separate from this release's evidence. / 各プロジェクトの対応環境・測定値は本配布物の実機結果とは別です。

## Acquisition and references / 取得元・典拠

- [NVIDIA 610.43.03 source archive](https://download.nvidia.com/XFree86/NVIDIA-kernel-module-source/NVIDIA-kernel-module-source-610.43.03.tar.xz): source of three AD102 boot assets; archive and source-member hashes are pinned in the setup code / 3つのAD102アセットの元データ。アーカイブと個別ソースのハッシュを固定。
- [CachyOS nvidia-utils 610.43.03-1 archive](https://archive.cachyos.org/archive/cachyos-v3/nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst): downstream package containing the pinned NVIDIA GSP firmware; archive hash and extracted bytes are checked / 固定したNVIDIA GSPファームウェアを含む下流パッケージ。原本と抽出後の両方を照合。
- V67: locally generated by the existing `ad102_payload.payload()` function, not downloaded as an upstream binary / 既存関数でローカル生成し、完成済みバイナリの取得は行いません。
- [NVIDIA Driver License Agreement](https://www.nvidia.com/en-us/drivers/nvidia-license/) and [upstream license policy](https://huggingface.co/TOTORONG/AD103_win_unlocker/blob/51df9c1/LICENSE-POLICY.md): review the terms applicable to your inputs and use; these links do not assert universal permission / 利用する入力や用途に適用される条件を確認してください。一律の許諾を示すリンクではありません。
- [PyTorch torch.mm](https://docs.pytorch.org/docs/stable/generated/torch.mm.html), [NVIDIA cuBLAS documentation](https://docs.nvidia.com/cuda/cublas/index.html): benchmark API references, not sources of the measured speedups / 測定APIの資料であり、本配布物の性能向上率の出典ではありません。
