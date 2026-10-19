> 문서 상태: 아래는 실기 테스트 이전 개발 기록을 보존한 문서입니다. 이후 한 대의 RTX 3060에서 확인한 결과는 [현재 실험 기록](README.md)을 참고하세요. 전체 하드웨어 지원을 뜻하지 않습니다.

# 검증 기록 — 2026-10-19

상태: 호스트 검증 통과, 실제 GPU 실행 안 함. Linux 검증 후 UEFI 실험 구현을 추가했다. 아래 EFI 항목을 별도로 참조한다.

- NVIDIA 소스와 301 MiB nvidia-utils 입력 파일: 원본 pinned SHA-256 일치.
- GA106 공식 HAL 5개 분기 검사 및 생성 펌웨어 5개 해시 검사 통과.
- `make test`: Python 상위 테스트 5개 통과. 내부 C 하니스는 6개 구성요소를 경고를 오류로 처리하여 컴파일·실행한다.
- 커널 진입 테스트: 17개 시나리오. 정확한 PCI 주소, execute opt-in, subsystem pinning, 잘못된 칩/보드/바인딩 상태, SS1 정책 전달을 검사한다.
- unlock 어댑터: 핀된 실제 펌웨어와 합성 ROM·12 GiB 메모리의 전체 거래 70개 시나리오 및 단위 실패 검사 통과. 8 GiB 장치 거절, GA107 부분 DMEM 회귀, 서명 복원, FLR 전후 identity와 SS0/SS1 검증, DMA 이전 실패 시 해제 및 진행 중 DMA 버퍼 유지 포함.
- mkinitcpio 훅·drop-in의 중복 방지, systemd unit 문법, initramfs 내용 판독 검사 통과.
- 사용자별 보드 ID를 읽기 전용 preflight 결과에 반영하고, 잘못되거나 누락된 장치 정보로 실행 인자를 만들지 않는 검사 통과.
- Ubuntu 6.8.0-138-generic 헤더, GCC 13.3.0으로 `ga106_unlock.ko` 빌드 성공. 버전 `0.2.0-experimental`, vermagic `6.8.0-138-generic SMP preempt mod_unload modversions`. 컴파일러 패키지 이름 차이 경고와 vmlinux 부재로 BTF 생성 생략이 있었으며 컴파일·링크·MODPOST는 성공했다.
- `LLVM=0`이 커널에서 비어 있지 않은 문자열로 취급돼 Clang을 선택하던 빌드 전달을 수정했다.
- Windows/Linux PyTorch 벤치마크: Python 구문·CLI 검사만 수행. CUDA GPU와 Windows가 없어 실제 측정은 하지 않음.

기존 Linux ZIP에는 소스와 테스트·문서만 포함했다. 새 Windows/UEFI ZIP에는 아래에 명시한 EFI 바이너리도 포함한다. 커널 헤더·다운로드 캐시·NVIDIA 바이너리·컴파일 산출물·로컬 GPU 로그는 포함하지 않는다. `.ko`는 실제 사용할 Linux 커널의 헤더로 다시 빌드해야 한다.

이 결과는 실제 VBIOS의 FWSEC 파싱, 사용자 카드 fuse의 서명 수락, V67 실행, 설정 지속, NVIDIA 드라이버 초기화, 임의 입력 정확도·장시간 안정성·ComfyUI 성능 향상을 증명하지 않는다. 특히 Linux에서 적용한 상태가 Windows 재부팅 후 유지된다고 가정하지 않는다.

## UEFI 실험판 0.3.0

- GNU EFI 3.0.15, GCC 13.3.0, x86-64 PE32+ EFI 애플리케이션 두 종류 빌드. `-Wall -Wextra -Werror`, 미해결 ELF 심볼 0개 확인. `.rodata*`와 relocation 보존.
- `make test`: Python 상위 테스트 총 12개 통과. 기존 Linux/core 70개 전체 거래 시나리오와 17개 진입 시나리오를 유지한다.
- SHA-256: 빈 입력, abc, 100만 a, padding 경계값들을 Python hashlib와 비교. NVIDIA 자산 5개 정상 수락 및 1바이트 변조 거절 검사 통과.
- EFI 설정 parser: 정확한 주소·보드·execute, 중복/미지정/범위 초과/잘못된 SKU 및 모호한 장치 거절 검사 통과.
- 실제 `uefi/runtime.c`를 EFI ABI 프로토콜 콜백으로 연결해 DMA CommonBuffer Map/Unmap, 부분 매핑·32비트 경계 거절, Unmap 실패 시 미해제·정지 플래그, MMIO 범위·I/O 실패 이후 쓰기 차단과 포맷팅 검사 통과.
- QEMU 8.2.2 / OVMF 2024.02, GPU passthrough 없는 일반 FAT32 raw 이미지에서 조회용 EFI 부팅·정상 종료·로그 저장을 확인. 언락용 EFI도 기본 설정에서 조회만 수행함을 확인. 불완전한 execute=1 설정은 EFI_INVALID_PARAMETER로 거절하고 로그를 남김을 확인.
- QEMU vvfat 쓰기 백엔드의 잘못된 로그 내용을 관측했으므로 최종 로그 검증은 실제 FAT32 이미지 + mtools 추출로 수행했다. 물리 USB 동작을 가상 USB로 입증한 것은 아니다.
- Windows용 Python 자산 추출 경로를 동일한 pinned 캐시로 실행하고 Linux 생성 파일 5개와 일치함을 확인. 스크립트 실행 OS 자체는 Linux이며 Windows에서 실행 검증은 아직 하지 않았다.

물리 RTX 3060, GOP를 사용하는 기본 디스플레이, IOMMU 구현 차이, 실제 FLR 및 Windows Boot Manager 인계는 미검증이다. UEFI의 guard가 실행을 거절할 수 있으며 이를 해제해 억지로 실행하지 않는다. 성능 향상·장시간 안정성·ComfyUI 결과를 확인한 빌드가 아니다.
