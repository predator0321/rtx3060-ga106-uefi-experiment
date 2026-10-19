# GitHub 웹 업로드 순서

ZIP을 먼저 압축 해제합니다. GitHub는 ZIP을 올려도 소스 폴더로 자동 해제하지 않습니다.

1. 저장소 최상위에서 **Add file → Upload files**를 엽니다.
2. 압축 해제한 `ga106-github-upload` 폴더 안에서 **`tests` 폴더만 제외한 나머지 파일·폴더**를 끌어다 놓습니다. 바깥쪽 `ga106-github-upload` 폴더 자체를 끌어다 놓지 않습니다.
3. README와 `.gitignore`는 기존 파일을 교체합니다. Windows 탐색기에서 `.gitignore`도 선택됐는지 확인합니다.
4. `feat: add GA106 UEFI source and experiment documentation` 메시지로 main에 커밋합니다.
5. 다시 저장소 최상위에서 **Add file → Upload files**를 엽니다.
6. 이번에는 `tests` 폴더 자체를 끌어다 놓아 저장소의 `tests/` 경로로 올립니다.
7. `test: add existing host validation tests` 메시지로 커밋합니다.

웹 업로드는 한 번에 최대 100개 파일이므로 두 차례로 나눕니다.
저장소 최상위에 `README.md`, `LICENSE`, `.gitignore`, `uefi/`, `kernel/`, `firmware/`, `tests/` 등이 있는지 확인합니다.

이 묶음은 소스 전용입니다. 펌웨어·EFI 실행 파일·전체 원본 측정 로그·개인별 부팅 설정은 포함하지 않습니다.
처음에는 Private를 유지하고 문서와 소스·라이선스를 검토한 뒤 Public으로 전환합니다.

## 묶음 검증

- 기존 런타임 C 소스와 준비·벤치마크 Python 소스는 원본 파일과 동일합니다.
- 일반 빌드 산출물·펌웨어·개인 파티션 GUID·기존 git 이력은 제외했습니다.
- 소스 묶음에서 UEFI 관련 unittest 8개 실행: 6개 통과, 2개 건너뜀.
- 건너뛴 항목: 준비되지 않은 펌웨어 해시 검사, GNU EFI 헤더가 필요한 프로토콜 콜백 검사.
- 이번 묶음 준비에서 GPU 실행이나 Windows 부팅을 새로 시험하지 않았습니다.
- 빌드 및 USB 준비 전 `docs/BUILD.md`를 확인하세요.

공식 안내: https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository
