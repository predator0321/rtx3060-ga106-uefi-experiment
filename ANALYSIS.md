> 문서 상태: 아래는 실기 테스트 이전 개발 기록을 보존한 문서입니다. 이후 한 대의 RTX 3060에서 확인한 결과는 [현재 실험 기록](README.md)을 참고하세요. 전체 하드웨어 지원을 뜻하지 않습니다.

# GA106 포팅 근거

2026-10-19. 사용자 제공 정보: RTX 3060, PCI ID `0x250410DE`, Windows 드라이버 `610.88`, VBIOS `94.06.25.00.b6`, 메모리 `12288 MiB`. 사용자의 subsystem ID나 cold 레지스터 값은 제공되지 않았다. 다른 보드 값을 대신 넣지 않는다.

NVIDIA-kernel-module-source-610.43.03.tar.xz의 SHA-256은 `7e118923c7a23edc36114d63273a46e3e04e9af98695a42203e7ac2dfe9fc1dc`로 검증했다. 공식 `src/nvidia/generated/g_kernel_gsp_nvoc.c`의 GA106 분기에서 다음을 확인했다.

| 항목 | 선택되는 구현 |
|---|---|
| BooterLoad | `kgspGetBinArchiveBooterLoadUcode_GA102` |
| BooterUnload | `kgspGetBinArchiveBooterUnloadUcode_GA102` |
| GSP boot image | `kgspGetBinArchiveGspRmBoot_GA102` |
| pre-scrubbed top FB | `kgspGetPrescrubbedTopFbSize_13afc5` |
| 최소 WPR heap | `kgspGetMinWprHeapSizeMB_d2c3c5` |

`tools/prepare.py`는 이 분기를 재검사하고 `build/offline-audit.json`에 위치와 출력 해시를 기록한다. 펌웨어 이름의 `ga102`는 공용 자산의 출처를 나타낸다. 임의의 GA106용 ROP 주소를 새로 만들지 않았다.

원본 GA107의 부분 DMEM 전송 회귀 fixture는 GA107 실측 geometry에 기반한 **합성** fixture로 보존했다. GA106의 VBIOS를 덤프했거나 그 FWSEC 버전을 확인했다고 주장하지 않는다. 실제 FWSEC는 대상 카드의 PROM에서 파싱한다.

SS0/SS1의 개별 연산 비트 의미와 Windows 드라이버 handoff는 이 작업에서 확인하지 않았다. 원본의 3050 실측과 성능 수치는 `research/upstream-ga107/`에만 남겼다.
