# ESP-IDF 개발 가이드 — ESP32-S3-RLCD-4.2

이 보드는 ESP-IDF로 개발한다. 이 문서는 환경 활성화부터 빌드/플래시/디버깅,
주변장치 드라이버 컴포넌트까지의 워크플로를 정리한다.

## 1. 환경 활성화

이 머신에는 ESP-IDF **v6.0.1**이 `~/.espressif/`에 설치되어 있다.
**새 터미널을 열 때마다** 아래를 실행해야 `idf.py`가 PATH에 등록된다:

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
```

확인:

```bash
idf.py --version      # ESP-IDF v6.0.1 출력되면 정상
```

## 2. 프로젝트 생성

```bash
# 예제 복사 방식
cp -r $IDF_PATH/examples/get-started/hello_world my_app
cd my_app

# 또는 빈 프로젝트
idf.py create-project my_app && cd my_app
```

## 3. 타겟 설정 (프로젝트당 1회)

```bash
idf.py set-target esp32s3
```

## 4. 보드 설정 (menuconfig)

```bash
idf.py menuconfig
```

이 보드에 맞춰 반드시 확인할 항목:

- **Serial flasher config → Flash size** → `16 MB`
- **Component config → ESP PSRAM**
  - `Support for external, SPI-connected RAM` 활성화
  - `SPI RAM config → Mode` → **Octal Mode PSRAM**
  - 속도 80MHz (필요 시)
- **Component config → ESP System Settings → Channel for console output**
  - USB-C 직결 사용 시 `USB Serial/JTAG Controller` 선택 가능
- Partition Table: 16MB Flash를 활용하려면 `partitions.csv` 커스텀 권장
  (앱 + SPIFFS/FAT + OTA 등).

> 위 설정은 `sdkconfig.defaults`에 적어두면 팀/CI에서 재현 가능하다. 예:
> ```
> CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
> CONFIG_SPIRAM=y
> CONFIG_SPIRAM_MODE_OCT=y
> CONFIG_SPIRAM_SPEED_80M=y
> ```

## 5. 빌드 / 플래시 / 모니터

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

- 시리얼 모니터 종료: `Ctrl + ]`
- `<PORT>` 찾기 (macOS):
  ```bash
  ls /dev/cu.*
  ```
  - 네이티브 USB Serial/JTAG: `/dev/cu.usbmodem*`
  - UART 브리지(CH34x/CP210x 등): `/dev/cu.usbserial-*` / `/dev/cu.wchusbserial*`
- 플래시 진입 실패 시: **BOOT 버튼 누른 채 RESET 클릭 → BOOT 떼기** 후 재시도.
- 개별 단계: `idf.py build`, `idf.py flash`, `idf.py monitor`, `idf.py app-flash` 등.

## 6. 디버깅 (JTAG / USB Serial/JTAG)

ESP32-S3는 네이티브 USB Serial/JTAG를 내장하므로 추가 어댑터 없이 OpenOCD/GDB 디버깅 가능:

```bash
idf.py openocd            # OpenOCD 서버 기동 (별도 터미널)
idf.py gdb                # GDB 접속
# 또는 한 번에
idf.py openocd gdbgui
```

- ESP-IDF에 포함된 Espressif 패치 빌드 OpenOCD를 사용한다(별도 설치 불필요).
- VS Code 사용 시 "Espressif IDF" 확장 + `idf.py`로 디버그 구성 가능.

## 7. 주변장치 드라이버 (ESP-IDF 컴포넌트)

핀맵은 [pinout.md](pinout.md) 참고. 컴포넌트는 ESP Component Registry
(`idf.py add-dependency "<name>"`)에서 가져온다.

### 디스플레이 (ST7305/ST7306, SPI, 400×300 모노크롬)

- `esp_lcd` 컴포넌트로 SPI 패널 IO를 만들고, ST7305/ST7306 전용 패널 드라이버 연결.
- IDF 기본에는 모노크롬 반사형 컨트롤러 드라이버가 없을 수 있으므로
  Component Registry에서 `esp_lcd_st7305` / `esp_lcd_st7306` 검색하거나 직접 포팅.
  ```bash
  idf.py add-dependency "espressif/esp_lcd_st7306"   # 존재 여부 확인 후
  ```
- 1bpp 프레임버퍼/부분 갱신 특성에 맞춰 LVGL 연동 시 색심도/플러시 콜백 조정.

### 터치 (정전식, I2C)

- `esp_lcd_touch` 계열 + 해당 터치 컨트롤러 드라이버. I2C0(GPIO13/14) 공유.

### 오디오 (ES8311 코덱 + ES7210 마이크 ADC, I2S)

- `esp_codec_dev` 또는 ESP-ADF의 `es8311`/`es7210` 드라이버 사용.
- I2S 데이터: [pinout.md](pinout.md)의 I2S0 핀, 제어: I2C0.

### 센서 / RTC

- SHTC3(0x70), PCF85063A(0x51) — I2C0. Component Registry에 드라이버 존재하거나
  데이터시트 기반 간단 구현.

### microSD

- `esp_driver_sdmmc` (SDMMC, 1-bit). FAT 마운트는 `esp_vfs_fat`.

## 8. 자주 쓰는 명령 요약

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"  # 환경
idf.py set-target esp32s3        # 타겟
idf.py menuconfig                # 설정
idf.py build                     # 빌드
idf.py -p <PORT> flash monitor   # 플래시+모니터
idf.py fullclean                 # 빌드 캐시 정리
idf.py size                      # 메모리 사용량
idf.py add-dependency "<comp>"   # 컴포넌트 추가
```

## 참고

- Zephyr/west 워크플로(`west flash`, `west espressif monitor`)는 Zephyr 보드 정의용이며,
  이 저장소는 **ESP-IDF**를 사용한다. 핀맵은 동일하므로 Zephyr devicetree를 핀 참조용으로만 활용.
- 원본 문서/데이터시트: [references.md](references.md)
