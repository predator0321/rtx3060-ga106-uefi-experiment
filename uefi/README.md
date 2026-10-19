> 문서 상태: 아래는 실기 테스트 이전 개발 기록을 보존한 문서입니다. 이후 한 대의 RTX 3060에서 확인한 결과는 [현재 실험 기록](../README.md)을 참고하세요. 전체 하드웨어 지원을 뜻하지 않습니다.

# RTX 3060 12GB — Windows 부팅 전 UEFI 실험판

**버전 0.3.1-experimental. EFI 빌드와 가상 UEFI 실행만 확인했다. 실제 RTX 3060 언락 및 Windows 610.88 인계는 미검증이다.** Windows에서 더블클릭하는 EXE가 아니라 x86-64 UEFI 앱이다. Linux 설치나 드라이버 교체는 필요하지 않다.

대상은 `10de:2504`, VRAM 12288 MiB다. 사용자가 제공한 드라이버 610.88/VBIOS 94.06.25.00.b6에서 동작했다고 주장하지 않는다. Windows 드라이버·VBIOS·eFuse를 쓰거나 패치하는 기능, BootOrder·Secure Boot 설정 변경 기능은 없다. 빌드된 EFI는 서명되지 않았으므로 Secure Boot 정책에 따라 실행이 차단될 수 있다.

## 1. 먼저 장치 조회만 실행

배포 ZIP의 **`USB_PROBE` 폴더 안 내용**을 기존 FAT32 USB의 루트에 복사한다. 이 폴더에는 `EFI/BOOT/BOOTX64.EFI`와 비활성 설정만 들어 있다. 기존 USB 부팅 파일이 있다면 다른 USB를 사용한다. 디스크 포맷 도구나 자동 부팅 설정 변경기는 포함하지 않는다.

1. Windows의 열린 작업을 저장하고 완전히 종료한다.
2. 제조사의 일회성 부팅 메뉴에서 USB의 UEFI 항목을 선택한다.
3. `Probe complete`가 표시되면 종료하고 USB를 빼고 평소처럼 Windows로 부팅한다.
4. USB 루트의 **`ga106-log.txt`**를 확인한다.

이 조회는 NVIDIA PCI ID·subsystem ID·BDF·PCI command만 읽는다. GPU MMIO·펌웨어·리셋·PCI 설정 쓰기는 하지 않는다. 로그 파일을 USB에 쓴다. 조회용 바이너리는 `execute=1` 설정이 있어도 실행을 무시하도록 컴파일했다.

## 2. Windows에서 실험용 USB 파일 준비

아래는 **실제 GPU 실행을 예약하는 설정 파일을 만드는 명령**이다. 기본 조회가 정상인지 먼저 확인하고, 성능 기준값도 `bench/benchmark_torch.py`로 기록한다. 이 실험판은 실패할 수 있으며 기본 디스플레이 GPU의 UEFI 사용 상태 때문에 실행을 거절할 수도 있다. PCI BME/WPR2/cold-state 검사를 억지로 해제하지 않는다.

압축을 푼 `ga106-unlock` 폴더에서 Python 3.12 이상으로 실행한다. ComfyUI Portable의 `python_embeded/python.exe`를 사용해도 된다. `python`이 해당 Python을 가리키지 않으면 전체 경로로 바꾼다.

```powershell
# Python 3.12/3.13에서 필요한 압축 해제 라이브러리
python -m pip install zstandard

# 조회 파일만 새 폴더에 만들기: GPU 변경 없음, 펌웨어 다운로드 없음
python .\uefi\prepare_usb.py --output .\USB_CHECK

# 실험 실행용 폴더: 정확한 3060 보드 자동 감지 후 pinned 파일 다운로드/검증
python .\uefi\prepare_usb.py --output .\USB_EXPERIMENT --mode unlock --execute
```

Python 3.14는 기본 zstd tar 지원을 사용할 수 있다. 실험용 준비는 NVIDIA 소스 약 25 MiB와 패키지 약 301 MiB를 내려받고 해시를 검사한다. 보드가 정확히 한 대로 식별되지 않으면 실행 설정을 만들지 않는다. 다른 GPU의 subsystem 값을 예제로 대신 넣지 않는다. 생성된 NVIDIA 자산과 라이선스는 본인이 준비한 폴더에만 저장하며 배포 ZIP에는 포함하지 않았다.

이 도구는 **새 일반 폴더만 만든다**. 기존 폴더, USB 드라이브 루트 또는 Windows EFI 파티션에 직접 실행하지 않는다. 기존 경로는 거절한다. 실제 드라이브 포맷·Windows 부팅 구성 수정·GPU 조작은 하지 않는다.

`USB_EXPERIMENT` 안 내용을 FAT32 USB 루트에 복사한다. 파일 배치는 다음과 같다.

```text
EFI/BOOT/BOOTX64.EFI
ga106/config.txt
ga106/firmware/booter_load_ga102.bin
ga106/firmware/gsp_ga10x.bin
ga106/firmware/gsp_boot_ga102.bin
ga106/firmware/gsp_boot_desc_ga102.bin
ga106/firmware/v67-ga102-610.43.03.bin
ga106/firmware/NVIDIA-LICENSE
```

기본 SS1은 보존한다. SS1=8 실험을 명시적으로 선택하려면 준비 명령에 `--ss1-full`을 추가한다. 이것이 3060에서 TF32를 개선한다고 보장하지 않는다.

## 3. 실험 동작과 복구

전원을 완전히 종료한 뒤 USB UEFI 항목으로 한 번 부팅한다. `execute=1`일 때만 다음을 시도한다.

- 정확한 PCI 주소·subsystem ID·GA106 장치·FLR 지원·cold 상태·12 GiB 용량을 검사한다.
- USB의 자산 5개를 각각 SHA-256으로 검증하고, 대상 카드 자체 PROM의 FWSEC를 파싱한다.
- EFI PCI I/O의 CommonBuffer Map으로 DMA 주소를 얻는다. 호스트 포인터를 GPU 주소로 가정하지 않는다.
- FWSEC → V67 → SS0/선택적 SS1 → 서명 복원 → FLR → 정지·메모리 정리를 검사한다.
- Windows Boot Manager가 정확히 한 군데에서 발견·로드되고 모든 검사가 성공했을 때만 같은 부팅에서 Windows로 넘긴다. Windows까지 갔다는 사실만으로 성능 개선을 입증하지 않는다.

화면이 잠시 꺼질 수 있다. 하드웨어 실행 이후 실패하거나 DMA 메모리 정리가 불확실하면 **Windows로 넘어가지 않고 멈춘다**. 앱 watchdog도 실행 전에 해제해 실패 대기가 자동 재부팅으로 바뀌지 않게 했다. 이 경우 충분히 기다린 후 전원 버튼으로 완전히 종료하고 USB를 뺀 뒤 일반 Windows로 복구한다. 같은 전원 주기에 다시 시도하지 않는다. 로그는 USB의 `ga106-log.txt`에 기록한다. 시스템이 응답하지 않거나 펌웨어 USB 쓰기가 실패하면 마지막 로그가 저장되지 않을 수도 있다.

GPU 실행 전 설정/장치/Windows Boot Manager 검사에서 실패한 경우는 펌웨어로 반환할 수 있다. 실행 이후 실패·`windows=0` 진단 모드·Windows Boot Manager 반환은 모두 정지한다. 정상 Windows 부팅 항목은 수정하지 않는다.

Windows가 실제로 시작되면 같은 Python·모델·시드·정밀도·해상도·스텝으로 성능과 정확도를 비교한다. 이 버전에는 Windows GPU에서 결과를 확인한 기록이 없다.

## 개발자용 빌드·검사

Linux x86-64에서 GCC/binutils와 GNU EFI 3.0.15로 빌드했다.

```bash
bash uefi/build.sh
python3 -m unittest discover -s tests -p 'test_uefi*.py' -v
```

기본 경로가 다르면 `EFI_INCLUDE`와 `EFI_LIB`를 지정한다. 생성 결과는 `build/uefi/GA106-PROBE.efi`, `GA106-UNLOCK.efi`다. `.rodata*`와 relocation 섹션을 보존하고 미해결 ELF 심볼이 있으면 배포 이미지를 만들지 않는다. 배포판의 GNU EFI 고지는 `GNU-EFI-NOTICE.txt`에 포함했다.

빌드된 EFI는 NVIDIA 바이너리를 내장하지 않는다. 원본 Linux transaction/parsers를 그대로 컴파일하고 `uefi/runtime.c`에서 PCI/DMA/파일/시간 API를 연결한다. 따라서 코드 재사용에 대한 모의 테스트와 실제 UEFI 콜백 검사를 함께 수행한다. 가상 UEFI는 실제 GPU의 서명 수락·레지스터 효과·FLR·Windows 인계를 검증할 수 없다.

## Windows Boot Manager가 두 곳인 경우 (0.3.1)

다른 EFI 파일이나 부팅 항목을 삭제하지 않는다. 관리자 PowerShell에서
`Get-Partition | Where-Object IsSystem | Format-List DiskNumber,PartitionNumber,Guid,GptType`
으로 현재 부팅에 사용하는 시스템 EFI 파티션을 확인한다. GPT EFI 시스템 파티션
(`GptType=c12a7328-f81f-11d2-ba4b-00a0c93ec93b`) 한 개가 확인된 경우만
USB의 `ga106/config.txt`에 `windows_partition=해당-파티션의-Guid`를 추가한다.
값은 중괄호 없이 36자 GUID다. 볼륨 GUID, 디스크 GUID 또는 GptType을 넣지 않는다.
0.3.1 EFI는 장치 경로의 GPT 파티션 서명이 일치하는 파일시스템만 검색한다.
그 안에서도 Windows Boot Manager가 정확히 한 개여야 진행한다. 이 옵션이 없으면
기존과 같이 전체에서 정확히 한 개를 요구한다. `windows=0`으로 우회하지 않는다.
