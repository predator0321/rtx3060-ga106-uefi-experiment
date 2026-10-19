# UEFI 소스 빌드

이 묶음은 소스 전용입니다. NVIDIA 펌웨어와 빌드된 EFI 바이너리는 포함하지 않습니다.
기존 `prepare_usb.py`는 `uefi/prebuilt/`의 바이너리와 manifest를 읽으므로 아래 준비가 먼저 필요합니다.

## Linux x86-64에서 빌드

GCC, binutils, Python 3.12 이상, GNU EFI 개발 헤더·라이브러리가 필요합니다. 기존 빌드는 GNU EFI 3.0.15를 사용했습니다.

```bash
bash uefi/build.sh
python3 -m unittest discover -s tests -p 'test_uefi*.py' -v
```

헤더·라이브러리가 기본 위치에 없으면 `EFI_INCLUDE`, `EFI_LIB`를 지정합니다.
빌드 결과는 `build/uefi/GA106-PROBE.efi`, `build/uefi/GA106-UNLOCK.efi`입니다.
모의 테스트나 빌드 성공은 실제 GPU·Windows 부팅 성공을 보장하지 않습니다.

## USB 준비 도구용 파일 배치

빌드한 바이너리를 `uefi/prebuilt/`에 복사하고 해당 빌드의 SHA-256 manifest를 만듭니다. 저장소 루트에서 실행합니다.

```bash
python3 - <<'PYBUILD'
from pathlib import Path
import hashlib, json, shutil
root = Path.cwd()
prebuilt = root / 'uefi/prebuilt'
prebuilt.mkdir(parents=True, exist_ok=True)
sha = {}
for name in ('GA106-PROBE.efi', 'GA106-UNLOCK.efi'):
    source = root / 'build/uefi' / name
    if not source.is_file():
        raise SystemExit(f'Build first: {source}')
    target = prebuilt / name
    shutil.copy2(source, target)
    sha[name] = hashlib.sha256(target.read_bytes()).hexdigest()
manifest = {
    'version': '0.3.1-experimental',
    'architecture': 'x86_64 UEFI',
    'signed': False,
    'hardware_tested': False,
    'sha256': sha,
}
(prebuilt / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
PYBUILD
```

새 빌드의 `hardware_tested=false`는 새 바이너리에 대해 다시 하드웨어 검증하지 않았다는 뜻입니다.
`uefi/prebuilt/`가 준비된 소스 폴더를 Windows로 옮긴 뒤 `uefi/README.md` 및 루트 README의 준비 절차를 따릅니다.
펌웨어를 생성하려면 네트워크와 원본 자산이 필요하며 별도 출처·라이선스 고지를 유지합니다.

기존 테스트용 바이너리는 이 묶음에 포함하지 않았습니다. 해당 바이너리를 추후 Release로 배포할 때는 원래 manifest의 해시와 일치하는지 확인하고, 대응 소스와 GNU-EFI 고지도 함께 제공합니다.
