> 문서 상태: 아래는 실기 테스트 이전 개발 기록을 보존한 문서입니다. 이후 한 대의 RTX 3060에서 확인한 결과는 [현재 실험 기록](README.md)을 참고하세요. 전체 하드웨어 지원을 뜻하지 않습니다.

# Linux에서만 사용하는 수동 실험 절차

**Windows에서는 이 절차를 실행할 수 없다.** 해당 Linux 커널에 맞는 모듈과 NVIDIA 드라이버가 준비된 실험 환경이 필요하다. 아래 명령은 자동 실행되지 않는다. 성공한 하드웨어 릴리스가 아니라 검증 전 실험 포트다.

## 준비와 조회

README의 다운로드·빌드·테스트를 완료한 뒤 실행한다.

```bash
python3 tools/preflight.py
modinfo kernel/gpu_unlock/ga106_unlock.ko
```

`preflight`는 읽기만 수행한다. RTX 3060이 여러 개면 `--target 0000:실제주소`를 지정한다. `one_shot_boot_args`에는 실제 PCI 주소와 subsystem IDs가 포함된다. `ready_for_pre_driver_attempt=false`는 데스크톱에서 NVIDIA가 사용 중이라면 정상이다. unbind로 이 값을 바꾸지 않는다.

## CachyOS/Arch의 systemd mkinitcpio 이미지

다른 배포판의 initramfs-tools/dracut 통합은 이 포트에 구현하지 않았다. 아래 훅을 다른 initramfs 생성기에 복사하지 않는다. 기존 정상 부팅 항목을 유지한다.

```bash
sudo install -Dm644 kernel/gpu_unlock/ga106_unlock.ko \
  "/usr/lib/modules/$(uname -r)/extra/ga106_unlock.ko"
sudo install -dm755 /usr/lib/firmware/ga106-unlock/610.43.03
sudo install -m644 build/firmware/*.bin /usr/lib/firmware/ga106-unlock/610.43.03/
sudo install -dm755 /usr/share/licenses/ga106-unlock
sudo install -m644 build/firmware/NVIDIA-LICENSE licenses/*.txt firmware/NOTICE.md /usr/share/licenses/ga106-unlock/
sudo depmod -a
sudo install -Dm755 boot/ga106-order.install /etc/initcpio/install/ga106-order
sudo install -Dm644 boot/zz-ga106-unlock.conf /etc/mkinitcpio.conf.d/zz-ga106-unlock.conf
sudo mkinitcpio -P
```

Limine이 별도 배포 이미지 갱신을 관리하는 환경은 그 환경의 `limine-mkinitcpio`를 사용한다. 생성된 이미지에 대해 `python3 tools/inspect_initramfs.py /실제/initramfs/경로`로 순서 설정을 확인한다. 훅은 NVIDIA·udev가 장치를 초기화하기 전에 모듈을 완료하도록 한다.

전원을 완전히 종료한 후, 부트로더 편집 화면에서 기존 root 등 정상 인자를 유지하면서 **preflight의 `one_shot_boot_args`만 한 번 추가**한다. 기본은 SS1을 보존한다. TF32 실험이 필요할 때만 `ga106_unlock.ss1_full=1`을 추가한다. 파일에 영구 적용하는 자동 설정기는 제공하지 않는다.

드라이버가 이미 사용한 GPU나 warm reboot 상태는 지원하지 않는다. `execute=0` 또는 인자 생략 시 실행하지 않는다. 모듈 서명 강제 환경에서는 해당 배포판의 모듈 서명 절차가 별도로 필요하다.

## 실행 후

```bash
sudo journalctl -k -b --no-pager -o short-monotonic > boot-kernel.log
rg 'ga106_unlock|gpu_unlock|NVRM|실제PCI주소' boot-kernel.log
nvidia-smi
python3 bench/benchmark_torch.py --output after-3060.json
```

언락을 실행하지 않은 cold boot에서도 같은 Python 환경·행렬 크기로 `baseline-3060.json`을 측정해 비교한다. SS0 readback과 FLR 완료 메시지만으로 성능 향상이나 정확성을 보장하지 않는다. ComfyUI는 동일한 모델·시드·해상도·스텝·정밀도로 별도 비교한다.

## 복구

다음 부팅에서 `ga106_unlock.execute=1`을 제거하거나 `=0`으로 바꾸고 전원을 완전히 차단했다가 켠다. 화면 진입이 안 되면 정상 부팅 항목/복구 환경에서 실행 인자를 제거한다. 펌웨어가 멈추지 않아 DMA 버퍼가 유지되는 경우 강제 rmmod나 재실행을 하지 않는다.

설정 파일 제거 후 해당 배포판의 initramfs를 재생성하면 다음 부팅부터 통합을 제거할 수 있다.

```bash
sudo rm -f /etc/mkinitcpio.conf.d/zz-ga106-unlock.conf
sudo rm -f /etc/initcpio/install/ga106-order
sudo rm -f "/usr/lib/modules/$(uname -r)/extra/ga106_unlock.ko"
sudo depmod -a
sudo mkinitcpio -P
```

이 파일 제거는 현재 GPU 상태를 즉시 되돌리는 명령이 아니다. 완전 종료가 필요하다.
