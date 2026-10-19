# RTX 3060 GA106 UEFI Unlock — Windows / ComfyUI 실험 기록

RTX 3060 12GB에서 Windows 시작 전에 UEFI 프로그램을 실행하고, FP16·BF16 행렬 연산과 ComfyUI Anima 생성 시간을 비교한 실험 기록입니다. USB로 먼저 검증한 뒤 내부 SSD의 별도 EFI 시스템 파티션으로 옮겨 실행했습니다.

**측정한 한 대의 시스템에서는 FP16·BF16 행렬 연산 처리량이 약 1.85배 증가했습니다. Anima에서는 같은 실행 옵션끼리 비교할 때 생성 시간이 약 30.8% 줄었고, 기존에 사용하던 최적화 설정과 비교하면 약 17.8% 줄었습니다.**

> 실험용 구현입니다. 다른 RTX 3060, 드라이버, VBIOS, 모델에서 같은 동작이나 성능을 보장하지 않습니다. GPU 초기화와 부팅 구성을 다루므로 화면 정지나 Windows 진입 실패가 발생할 수 있습니다. 기존 Windows Boot Manager와 복구 수단을 유지하고 진행하세요.

> **이 업로드 묶음은 소스 전용입니다.** `uefi/prebuilt/`의 EFI 바이너리는 포함하지 않았습니다. USB 준비 도구 사용 전 [소스 빌드 및 prebuilt 준비](docs/BUILD.md)를 먼저 진행하세요.

## 1. 확인한 범위

| 항목 | 상태 |
|---|---|
| UEFI 실행 후 SS0 변경 | 실제 로그에서 확인 |
| GPU FLR 후 SS0 유지 | 실제 로그에서 확인 |
| SS1 보존 | 실제 로그에서 `0x2` 유지 확인 |
| USB를 통한 실행 및 Windows 진입 | 테스트 시스템에서 확인 |
| 내부 SSD의 별도 EFI 파티션에서 실행 | 등록 및 실행 로그 확인, Windows에서 로그 수집 |
| FP16·BF16 행렬 연산 | 처리량 증가 및 일부 계산 샘플 검사 통과 |
| Anima 이미지 생성 | 아래 4가지 조건으로 측정 |
| 모든 보드·드라이버 지원 | 확인하지 않음 |
| 장시간 안정성·수면 복귀·게임 성능 | 확인하지 않음 |
| SSD 자동 부팅 우선순위 설정 후 반복 검증 | 별도 측정 기록 없음 |

이 문서에서 “언락”은 부팅 전 실험 프로그램으로 GPU 설정을 변경하는 과정을 뜻합니다. VRAM 증설이나 VBIOS 플래싱을 뜻하지 않습니다. 전원을 끈 뒤에도 유지되는 영구 적용으로 간주하지 않으며, 적용이 필요한 부팅에서 UEFI 프로그램을 다시 실행합니다.

## 2. 테스트 환경

| 항목 | 환경 |
|---|---|
| GPU | NVIDIA GeForce RTX 3060 12GB / GA106 |
| PCI 장치 ID | `10de:2504` |
| 테스트 보드 subsystem | vendor `0x1043`, device `0x881d` |
| 시스템 RAM | 32GB |
| OS | Windows — 정확한 버전·빌드는 미기록 |
| NVIDIA 드라이버 | `610.88` — 벤치마크 출력 기준 |
| VBIOS | `94.06.25.00.b6` — 벤치마크 출력 기준 |
| PyTorch / CUDA | `2.13.0+cu130` / `13.0` — 벤치마크 출력 기준 |
| UEFI 실험 프로그램 | `0.3.1` |
| 프로그램 / 생성 모델 | ComfyUI Portable / Anima |
| 해상도 / 배치 | **1024 × 1536 / 1** |
| Sampler / Scheduler | **`euler_ancestral` / `simple`** |
| Steps / CFG | **30 / 5.0** |
| 메모리 로딩 | dynamic VRAM loading 사용 로그 확인 |
| 기타 | MultiGPU Runtime 로그, `--use-ck-attention` 사용 |

위 subsystem 값은 테스트 보드의 값입니다. 다른 보드의 설정에 그대로 복사하지 마세요. 모델 파일명·해시·정밀도, ComfyUI 커밋, 커스텀 노드 버전, 프롬프트와 실제 시드 값은 제공된 기록에 없습니다. 모델 로딩 로그의 `3988MB Staged`만으로 모델 정밀도를 확정할 수 없습니다.

## 3. 측정 결과

### 3.1 FP16·BF16 행렬 연산

`benchmark_torch.py`를 같은 ComfyUI Portable Python으로 실행했습니다. 행렬 크기는 4096, 반복 횟수는 20, 샘플 수는 5이며 `reduced_precision_reduction=false`로 출력됐습니다.

| 연산 | 일반 부팅 시간 | 언락 시간 | 일반 부팅 처리량 | 언락 처리량 | 처리량 증가 |
|---|---:|---:|---:|---:|---:|
| FP16 | 5.3386ms | 2.8883ms | 25.74 TFLOPS | 47.58 TFLOPS | 약 84.8% |
| BF16 | 5.2980ms | 2.8645ms | 25.94 TFLOPS | 47.98 TFLOPS | 약 84.9% |

두 상태에서 `finite=true`, `correctness_sample_passed=true`였습니다. 검사 범위는 32개 행의 계산 샘플이며 전체 연산·모델의 정확도를 보장하는 검사는 아닙니다.

| 연산 | 상대 L2 오차 — 두 상태 동일 |
|---|---:|
| FP16 | 0.0002070283 |
| BF16 | 0.0016614620 |

이후 언락 상태 재측정에서도 FP16 **47.61 TFLOPS**, BF16 **48.39 TFLOPS**가 관찰됐습니다.

출력의 `baseline-only; no unlock performed`는 벤치마크 자체가 언락을 수행하지 않는다는 뜻입니다. 부팅 전에 적용된 상태를 판정한 문구는 아닙니다. `nvidia-smi` 정보도 부하 전 측정이므로 실제 연산 중 클럭·전력 비교 자료로 해석하지 않습니다.

### 3.2 ComfyUI Anima 생성 시간

공통 실행 옵션:

```text
--windows-standalone-build --fast fp8_matrix_mult --disable-api-nodes --disable-pinned-memory --use-ck-attention
```

아래 “사용” 조건에는 `--fast` 항목으로 `fp16_accumulation`을 추가했습니다. 시간은 터미널의 `Prompt executed in … seconds` 값입니다.

| 부팅 상태 | `fp16_accumulation` | 1회 | 2회 | 3회 | 평균 |
|---|---|---:|---:|---:|---:|
| 일반 부팅 | 사용 | 58.07초 | 57.31초 | 57.50초 | **57.63초** |
| 언락 적용 | 사용 | 57.84초 | 57.83초 | 57.40초 | **57.69초** |
| 일반 부팅 | 제외 | 68.45초 | 68.64초 | 68.50초 | **68.53초** |
| 언락 적용 | 제외 | 47.54초 | 47.30초 | 47.35초 | **47.40초** |

| 비교 | 생성 시간 감소 | 처리 속도 증가 |
|---|---:|---:|
| 옵션 제외 조건: 일반 부팅 → 언락 | **30.8%** | **44.6%** |
| 기존 일반 부팅·옵션 사용 → 언락·옵션 제외 | **17.8%** | **21.6%** |
| 옵션 사용 조건: 일반 부팅 → 언락 | 향상 관찰되지 않음 | 사실상 동일 |

계산식은 다음과 같습니다. 표시는 반올림했으며 계산은 원래 실행 시간의 평균으로 수행했습니다.

```text
시간 감소율 = (기준 시간 − 비교 시간) / 기준 시간 × 100
처리 속도 증가율 = (기준 시간 / 비교 시간 − 1) × 100
```

**현재 기록에서 가장 빠른 조합은 언락 적용 + `fp16_accumulation` 제외입니다.** 옵션 효과가 부팅 상태에 따라 반대로 나타난 내부 원인은 아직 확정하지 않았습니다. 이를 특정 누산 경로나 커널의 효과라고 단정하지 않습니다.

### 3.3 측정의 한계

- 조건별 3회 측정이며 여러 번의 독립적인 부팅 세션에 걸친 반복 검증은 아닙니다.
- 각 조건에서 예열 1회 후 측정하도록 진행했지만, 제공된 발췌 로그만으로 예열 전 과정을 독립 검증할 수는 없습니다.
- 시드는 생성 후 랜덤 변경 설정으로 진행됐으며 같은 시드 집합을 양쪽에서 사용했는지는 확인되지 않았습니다.
- 생성 이미지의 품질·일관성·수치적 동일성은 별도로 평가하지 않았습니다.
- 테스트 간 온도·부하 중 클럭·전력·백그라운드 작업을 정밀하게 통제하지 않았습니다.
- 이 결과는 Anima의 해당 워크플로에 해당하며 다른 모델·정밀도·해상도의 개선율을 예측하지 않습니다.
- SSD 설치 이후 같은 4조건 벤치마크를 다시 수행한 기록은 아직 없습니다.

## 4. 부팅 전 실행 구조

```text
UEFI의 GA106 Unlock 항목
  → 실행 파티션에서 설정·펌웨어 로드
  → 대상 GPU 및 Windows 부팅 파티션 검사
  → GPU 설정 변경 및 FLR 후 검증
  → 기존 Windows Boot Manager 실행
  → Windows 진입
```

실행 프로그램은 자신이 시작된 파티션의 루트에서 `ga106/config.txt`와 `ga106/firmware/`를 읽습니다. 따라서 USB와 내부 SSD 모두 사용할 수 있지만, 실행 파일만 다른 파티션으로 옮기면 필요한 파일을 찾지 못할 수 있습니다.

확인된 로그의 핵심 부분:

```text
Unique Windows Boot Manager selected by GPT partition GUID
selector write: SS0=0x88888888
SS1 preserved: 0x2 (no write)
pre-FLR selectors: ss0=0x88888888 expected=0x88888888 ss1=0x2 expected=0x2
PCIe FLR returned: result=0
post-FLR SEC2 reset released, GSP halted, BME=0 pending=0 WPR2=1ffffe00:00000000 SS0=88888888 SS1=00000002
driver probe resources: phase=handoff empty=1
SS0/SS1 readback and FLR containment passed; actual Windows handoff and speedup still require measurement
```

로그의 `hardware execution and Windows handoff unvalidated`는 0.3.1 바이너리에 포함된 고정 문구입니다. 로그만으로 Windows 진입이나 성능 향상을 판정하지 않고, 실제 Windows 진입과 별도 측정 결과를 함께 확인했습니다.

## 5. USB에서 먼저 검증

실제 도구 소스를 함께 공개한다면 `uefi/README.md`의 상세 절차와 빌드 정보를 함께 유지하세요. 아래 경로는 `ga106-unlock` 폴더를 저장소 루트로 사용하는 구성을 기준으로 합니다.

1. `GA106-PROBE.efi`로 PCI 장치 정보를 조회합니다.
2. 실제 장치의 PCI 주소와 subsystem ID를 확인합니다.
3. 일반 부팅 상태의 기준 벤치마크를 기록합니다.
4. 실험용 펌웨어를 준비하고 해시를 검증합니다.
5. 실험용 `GA106-UNLOCK.efi`와 실행 설정을 FAT32 USB에 배치합니다.
6. 완전히 종료한 뒤 USB의 UEFI 부팅 항목으로 실행합니다.
7. Windows 진입, 로그, 계산 샘플 검사와 성능을 확인합니다.

조회 전용 `GA106-PROBE.efi`와 실험용 `GA106-UNLOCK.efi`를 혼동하지 마세요. 조회 전용 이미지는 실행 설정이 있어도 GPU 변경을 수행하지 않습니다.

USB 준비 도구가 포함된 소스 트리에서는 새 일반 폴더에 파일을 준비할 수 있습니다.

```powershell
python -m pip install zstandard
python .\uefi\prepare_usb.py --output .\USB_CHECK
python .\uefi\prepare_usb.py --output .\USB_EXPERIMENT --mode unlock --execute
```

이 명령은 해당 버전의 도구가 포함된 경우에 사용합니다. USB 준비 도구가 만든 일반 폴더의 내용을 USB 루트로 복사하며, 준비 도구의 출력 경로를 기존 USB 루트나 Windows EFI 파티션으로 직접 지정하지 않습니다.

필요한 파일 배치:

```text
EFI/BOOT/BOOTX64.EFI                 # 실험용 GA106-UNLOCK.efi의 복사본
ga106/config.txt
ga106/firmware/booter_load_ga102.bin
ga106/firmware/gsp_ga10x.bin
ga106/firmware/gsp_boot_ga102.bin
ga106/firmware/gsp_boot_desc_ga102.bin
ga106/firmware/v67-ga102-610.43.03.bin
ga106/firmware/NVIDIA-LICENSE
```

설정 예시입니다. 꺾쇠괄호 부분은 실제 장치에서 확인한 값으로 바꿔야 하며 그대로 실행할 수 없습니다.

```ini
target=<GPU의 PCI 주소>
board_vendor=<실제 subsystem vendor ID>
board_device=<실제 subsystem device ID>
execute=1
ss1_full=0
windows=1
windows_partition=<실제 Windows 시스템 EFI 파티션 GUID>
```

이 실험에서는 `SS1`을 보존하는 `ss1_full=0`을 사용했습니다. Windows EFI 파티션이 여러 개라면 기존 파일을 삭제하지 말고 현재 시스템 파티션을 확인합니다.

```powershell
Get-Partition |
    Where-Object { $_.IsSystem } |
    Format-List DiskNumber,PartitionNumber,Guid,GptType
```

`windows_partition`에는 위 출력의 **파티션 Guid**를 중괄호 없이 입력합니다. 디스크 GUID, 볼륨 GUID, `GptType`을 넣지 않습니다. 대상 파티션 안에서도 Windows Boot Manager가 정확히 하나여야 합니다.

## 6. USB 없이 내부 SSD에서 실행

### 6.1 용량과 설치 위치

이 테스트의 기존 Windows EFI 파티션은 전체 약 96MB, 여유 약 61.2MB였습니다. `gsp_ga10x.bin` 하나가 **84,277,400바이트, 약 80.4MiB**여서 기존 EFI에 넣을 수 없었습니다.

따라서 데이터 SSD의 파티션을 1GiB 줄여 별도의 **1GiB FAT32 EFI 시스템 파티션**을 만들었습니다. Windows EFI 파티션을 정리하거나 기존 부팅 파일을 덮어쓰지 않았습니다. 1GiB는 이번에 선택한 여유 있는 크기이며 프로그램의 최소 요구 용량을 뜻하지 않습니다.

파티션 변경 전 중요한 자료를 백업하고, 디스크 모델·파티션 번호·축소 가능 용량을 확인합니다. 다른 PC의 디스크 번호와 드라이브 문자를 그대로 복사해 사용하지 않습니다.

```powershell
Get-Disk
Get-Partition

# D:는 예시입니다. 실제로 사용할 데이터 파티션으로 바꾸세요.
Get-PartitionSupportedSize -DriveLetter D
```

새 파티션을 만든 뒤 임시로 S:를 연결했다고 가정하면 배치는 다음과 같습니다.

```text
S:/EFI/GA106/GA106-UNLOCK.efi
S:/ga106/config.txt
S:/ga106/firmware/...
S:/ga106-log.txt                     # 실행 후 생성되는 로그
```

USB에서 이미 작동한 실행 파일·설정·펌웨어를 복사하고, 원본과 복사본의 SHA-256을 비교했습니다. 이 비교는 **복사 일치 확인**이며 출처 신뢰성 검증을 대체하지 않습니다. 원본 펌웨어 검증은 별도 manifest와 프로그램의 검증 절차를 따릅니다.

S:는 Windows에서 사용하는 임시 드라이브 문자입니다. 이후 문자 연결을 해제하더라도 등록된 부팅 장치는 해당 파티션을 가리키지만, 파티션 삭제·재생성 시에는 재등록이 필요할 수 있습니다.

### 6.2 별도 UEFI 부팅 항목 등록

아래는 등록 과정의 개요입니다. 관리자 PowerShell에서 대상 S:가 새 언락 파티션인지 확인한 뒤 실행하고, 각 단계의 성공 여부를 확인합니다. `<새 항목 GUID>`는 첫 명령 출력에 나온 값으로 바꿉니다. 기존 `{bootmgr}` 자체의 경로를 바꾸지 않습니다.

```powershell
# 먼저 별도 경로에 BCD 백업을 보관합니다.
bcdedit /export "D:\ga106-bcd-before"

# 출력되는 새 GUID를 기록합니다.
bcdedit /copy "{bootmgr}" /d "GA106 Unlock"

# 아래의 자리표시자를 새 GUID로 바꾼 뒤 실행합니다.
bcdedit /set "<새 항목 GUID>" device partition=S:
bcdedit /set "<새 항목 GUID>" path "\EFI\GA106\GA106-UNLOCK.efi"
bcdedit /set "{fwbootmgr}" displayorder "<새 항목 GUID>" /addlast

bcdedit /enum firmware
```

BCD 내보내기는 펌웨어 NVRAM 전체나 디스크 전체의 백업이 아닙니다. 기존 정상 부팅 항목을 유지합니다. 등록 과정에서 오류가 나면 반복 생성하지 말고 현재 항목을 먼저 확인합니다.

처음에는 USB를 제거한 뒤 제조사의 일회성 부팅 메뉴에서 `GA106 Unlock`을 직접 선택합니다. Windows로 진입한 뒤 새 파티션의 `ga106-log.txt`를 확인합니다.

```powershell
Get-Content S:\ga106-log.txt
```

S:가 사라졌다면 `Get-Partition`으로 **설치 당시 기록한 파티션 GUID**를 찾아 그 파티션에 문자를 연결합니다. `mountvol S: /S`는 Windows 시스템 EFI를 연결하므로 별도로 만든 언락 EFI에 연결하는 용도로 사용하지 않습니다.

### 6.3 자동 실행

직접 선택한 부팅이 정상 동작하는지 확인한 다음 BIOS/UEFI의 부팅 우선순위를 다음과 같이 설정할 수 있습니다.

| 순위 | 항목 |
|---|---|
| 1 | GA106 Unlock |
| 2 | 현재 정상 사용하는 Windows Boot Manager |

이는 부팅 때 언락 프로그램을 자동 실행하는 구성입니다. 영구적인 GPU 변경을 뜻하지 않습니다. 프로그램이 실행 도중 정지하면 두 번째 부팅 항목으로 자동 전환된다고 보장할 수 없습니다.

## 7. ComfyUI 실행 배치 파일

아래는 **언락 부팅 상태에서 가장 빨랐던 측정 조건**에 맞춘 예시입니다. `fp16_accumulation`을 제외했습니다. 배치 파일을 `python_embeded`와 `ComfyUI` 폴더가 있는 위치에 저장합니다.

캐시는 배치 파일 위치 아래에 저장하도록 작성했습니다. 기존 측정에서는 캐시 환경변수가 별도 D: 경로를 가리켰으므로 이 부분은 배포 편의를 위한 경로 정리이며 새 경로로 추가 성능 측정한 결과는 아닙니다.

```bat
@echo off
cd /d "%~dp0"

set "MIMALLOC_SHOW_ERRORS=1"
set "TORCHINDUCTOR_CACHE_DIR=%~dp0cache\torch\_inductor"
set "TRITON_CACHE_DIR=%~dp0cache\torch\triton"

.\python_embeded\python.exe -s ComfyUI\main.py --windows-standalone-build --fast fp8_matrix_mult --disable-api-nodes --disable-pinned-memory --use-ck-attention

echo.
echo ComfyUI has exited. Check the messages above if an error occurred.
pause
```

일반 부팅에서는 측정상 `--fast fp16_accumulation fp8_matrix_mult`가 더 빨랐습니다. 위 배치 파일은 언락 여부를 자동 감지하지 않습니다. `--use-ck-attention`을 포함한 옵션은 테스트에 사용한 설치 환경에 해당하며 다른 ComfyUI 버전·확장 구성에서 지원되는지는 별도 확인이 필요합니다.

## 8. 재현 측정 권장 절차

1. 모델 파일·해시·정밀도, ComfyUI 커밋, 커스텀 노드 버전, 실행 옵션, 워크플로 JSON을 기록합니다.
2. 해상도 1024×1536, 배치 1, `euler_ancestral`, `simple`, 30 steps, CFG 5.0을 고정합니다.
3. 동일 프롬프트와 시드 집합을 사용합니다. 예열 시드 하나와 측정 시드 101·102·103을 양쪽에서 동일하게 사용하면 됩니다.
4. 일반 부팅에서 두 옵션 조건을 각각 예열 후 측정합니다.
5. 완전히 종료하고 언락 부팅한 뒤 같은 두 조건을 측정합니다.
6. 총 실행 시간과 샘플링 속도를 함께 저장합니다. 이전 결과가 캐시되어 샘플링이 생략되지 않았는지 확인합니다.
7. 여러 부팅 세션에서 재측정하고, 생성 이미지의 품질과 부하 중 온도·클럭·전력도 비교합니다.

TeaCache 등 연산 생략 기능, 추가 업스케일, LoRA·ControlNet 등은 사용 여부를 기록하고 양쪽에서 동일하게 유지합니다. 순수 기본 모델 비교를 원하면 모두 제외한 별도 조건을 만듭니다.

## 9. 문제 발생 시

| 증상 | 확인 사항 |
|---|---|
| Windows Boot Manager가 여러 개라 중단 | 실제 시스템 EFI의 파티션 GUID로 `windows_partition` 지정 |
| 펌웨어 파일을 찾지 못함 | 실행 EFI와 같은 파티션의 루트에 `ga106/firmware/`가 있는지 확인 |
| 적용 로그는 있으나 ComfyUI 차이가 없음 | 같은 부팅에서 행렬 벤치마크, 실행 옵션·정밀도·캐시 여부 확인 |
| EFI 실행이 차단됨 | 서명되지 않은 EFI에 대한 Secure Boot 정책 확인 |
| GPU 작업 이후 화면 정지 | 테스트에서는 5분 이상 반응이 없으면 완전히 종료 후 정상 부팅 항목으로 복구하도록 안내 |

GPU 작업 중 실패하면 같은 전원 주기에서 반복 시도하지 않습니다. 전원을 완전히 끈 뒤 기존 Windows Boot Manager로 부팅합니다. 언락 프로그램이 오류로 멈추는 경우 일반 부팅으로 자동 넘어간다고 가정하지 않습니다.

적용을 중단하려면 BIOS/UEFI에서 기존 정상 Windows Boot Manager를 우선으로 설정하고 완전히 종료한 뒤 부팅합니다. 부팅 항목이나 파티션을 삭제할 때는 정확한 대상임을 확인해야 하므로 자동 삭제 명령은 이 문서에 포함하지 않습니다.

## 10. 저장소 구성과 공개 범위

다음은 실제 구현 소스를 함께 올릴 때의 권장 구성입니다. 문서만 공개하는 저장소라면 구현 파일이 포함됐다고 표시하지 마세요.

```text
README.md
LICENSE                         # 실제 포함 코드의 기존 라이선스 보존
uefi/                           # UEFI 소스·빌드·준비 도구·상세 문서
firmware/                       # 다운로드·추출 도구, manifest, 고지
bench/                          # 벤치마크 소스
tests/                          # 기존 검증 코드
results/                        # 실제로 확보한 원본 결과만 추가
docs/                           # 설치·복구·설계 기록
```

- 기존 도구의 루트 문서에는 실기 전 “미검증” 상태가 남아 있을 수 있습니다. 실제 측정 기록을 추가하되 한 대에서 확인한 범위를 전체 하드웨어 지원으로 확대하지 않습니다.
- 이 README는 제공된 로그와 대화의 측정값을 정리한 것입니다. 워크플로 JSON과 전체 원본 결과 파일을 포함하는 배포 패키지는 아닙니다.
- 빌드된 EFI를 배포한다면 대응하는 소스·빌드 방법·커밋·해시와 기존 라이선스 고지를 함께 제공합니다.
- 현재 소스 트리의 `GPL-2.0-only` 및 GNU-EFI 관련 고지를 보존합니다. 저장소 전체를 임의로 MIT 등 다른 라이선스로 재표기하지 않습니다.
- NVIDIA 펌웨어는 코드와 별도 조건을 적용받습니다. 기존 다운로드·추출 도구 및 출처·해시·라이선스 고지를 유지하고, 이 문서는 펌웨어 바이너리 재배포를 허가하는 근거가 아닙니다.
- 공개 문서에는 사용자별 파티션 GUID·부팅 항목 GUID·디스크 번호·개인 경로를 고정값으로 넣지 않습니다. 원본 로그를 올릴 때도 공개할 항목을 확인합니다.

## 11. 출처와 관련 문서

아래 프로젝트의 연구·소스 및 기존 고지를 함께 확인하세요. GA107·GA102 등의 결과가 RTX 3060에 그대로 적용된다는 뜻은 아닙니다. 실제로 포함하는 파일의 저작권·라이선스는 해당 파일과 고지를 기준으로 합니다.

- [IHaBiS02/ga107-unlock](https://github.com/IHaBiS02/ga107-unlock) — 현재 GA106 소스 문서에 기록된 포팅 기반. 기록된 커밋: `5e6b3600bb01142891d43d62120727e96911d1a6`.
- [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) — 기존 소스에 기록된 관련 프로젝트.
- [WildFlash1st/cmp90hx-unlock](https://github.com/WildFlash1st/cmp90hx-unlock) — 기존 manifest에 기록된 GA102 참고 패치. 기록된 리비전: `76940ae583281daa2b883518a2e89b56c11d3a46`.
- [WildFlash1st/cmp90hx-unlock-for-windows](https://github.com/WildFlash1st/cmp90hx-unlock-for-windows) — 관련 Windows 부팅 전 EFI 구현.
- [Microsoft: UEFI BCD 설정](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/bcd-system-store-settings-for-uefi).
- [Microsoft: EFI/GPT 파티션 구성](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-uefigpt-based-hard-drive-partitions).
- [Microsoft: Get-PartitionSupportedSize](https://learn.microsoft.com/en-us/powershell/module/storage/get-partitionsupportedsize).
- [Microsoft: mountvol](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/mountvol).

---

실험 기록: 2026년 10월 19–20일, 한국 시간. 수치는 제공된 테스트 시스템의 관찰 결과입니다.
