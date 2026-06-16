# 경제 캘린더 프록시 배포 가이드 (`econ_proxy`)

ESP32-S3-RLCD-4.2 모니터의 경제 캘린더 화면(KEY+BOOT)은 데이터 소스로 FMP를
기본 사용하지만, FMP·Finnhub 모두 캘린더 엔드포인트가 **유료 플랜 전용**이라
무료 키로는 `HTTP 402`가 뜬다. `tools/econ_proxy`는 investing.com의 공개 캘린더를
긁어 **FMP와 동일한 모양의 JSON**으로 내려주는 자체 호스팅 프록시다. 기기는
펌웨어 수정 없이 `CONFIG_STOCK_ECON_BASE_URL`만 이 프록시로 바꾸면 된다.

- **기본 포트: `8442`** (환경변수 `PORT` 또는 compose 포트 매핑으로 변경 가능)
- 엔드포인트: `GET /economic-calendar?from=YYYY-MM-DD&to=YYYY-MM-DD`
- 헬스체크: `GET /health` → `{"ok": true}` (업스트림 호출 없음)
- 응답 시각은 UTC, 기기가 자기 타임존(기본 KST)으로 변환

> ⚠️ investing.com 약관은 스크래핑을 권장하지 않는다. **개인·저빈도 용도**로만
> 사용한다(기기는 버튼을 누를 때만 호출한다).

---

## 1. 권장 구성: Mac mini 위 리눅스 VM

```
[ESP32-S3]  --(WiFi/LAN)-->  [리눅스 VM : econ_proxy:8442]  --(인터넷)-->  investing.com
                                   └ Mac mini 호스트
```

### 아키텍처
- Apple Silicon Mac mini → **arm64** 리눅스 VM. 컨테이너 베이스 이미지
  `python:3.12-slim`은 멀티아치라 arm64로 받아지고, 의존성(`cloudscraper`,
  `requests`)은 순수 파이썬이라 컴파일 없이 설치된다.
- Intel Mac mini → x86_64. 동일하게 동작.

### ⚠️ 가장 중요한 한 가지 — VM 네트워크 모드
ESP32는 WiFi에 붙은 **별도 기기**라서, VM의 `8442` 포트에 **LAN에서** 접근할 수
있어야 한다. VM 기본값인 **NAT 모드면 VM이 호스트 내부 IP만 받아 ESP32가 닿지
못한다.** 둘 중 하나로 구성한다:

1. **(권장) 브리지 네트워크** — VM이 공유기에서 자체 LAN IP(예 `192.168.0.50`)를
   받는다. 기기에는 `http://192.168.0.50:8442/economic-calendar`.
2. **NAT + 포트포워딩** — Mac 호스트의 `8442` → VM `8442`로 포워딩하고, 기기에는
   `http://<Mac mini의 LAN IP>:8442/economic-calendar`.

그리고 VM/호스트 방화벽에서 `8442` 인바운드를 허용한다.

---

## 2. 배포 (Docker, 권장)

VM 안에서:

```bash
git clone <이 저장소> && cd <repo>/tools/econ_proxy
docker compose up -d            # 빌드 + 8442 포트로 실행, 부팅/크래시 시 자동 재시작
docker compose logs -f          # 요청 로그
docker compose down             # 정지
```

- `docker-compose.yml`의 `restart: unless-stopped`로 컨테이너는 자동 복구된다.
- 포트를 바꾸려면 `ports:`의 호스트 쪽만 수정한다(예 `"9000:8442"`).
- 완전 무인 운용: ① VM의 Docker가 부팅 시 자동 시작되게 하고(`systemctl enable
  docker`), ② **VM 자체도 Mac 부팅 시 자동 시작**되게 한다(UTM/Parallels의
  autostart 옵션).

### Docker 없이
```bash
python3 econ_proxy.py            # 의존성 0 (stdlib). PORT=9000 로 변경 가능
pip install -r requirements.txt  # (선택) Cloudflare 우회용 cloudscraper/requests
```

---

## 3. 동작 검증 (순서대로)

```bash
# (1) VM 로컬에서 — 서버 자체 확인
curl -s localhost:8442/health
#   → {"ok": true}
curl -s "localhost:8442/economic-calendar?from=2026-06-15&to=2026-06-21" | head -c 200
#   → [{"date":"...","country":"USD",...}]  (배열이 나오면 정상)

# (2) 다른 기기(노트북/폰)에서 — LAN 접근 확인  ★ 가장 중요
curl -s "http://<VM-LAN-IP>:8442/health"
#   → {"ok": true} 가 떠야 ESP32도 닿는다.
```

(2)가 안 되고 (1)만 되면 **네트워크 모드 문제**다(위 1번 섹션 — 브리지/포트포워딩).

---

## 4. 기기 설정

`idf.py menuconfig` → **Stock Monitor**:
- `Economic calendar base URL` = `http://<VM-LAN-IP>:8442/economic-calendar`
- `Financial Modeling Prep (FMP) API key` = 아무 비어있지 않은 값(프록시가 무시)
- (선택) `Economic calendar: minimum importance` — 1=전부 / 2=Medium+High(기본) / 3=High만

이후 `idf.py build flash`. KEY+BOOT로 캘린더를 열면 investing.com 데이터(실제치
포함)가 표시되고, KEY=저번주 / BOOT=다음주로 이동한다.

---

## 5. 이 브랜치를 머지 전에 원격 호스트에서 검토하기

프록시/배포 변경은 **머지 전에 실제 호스트(Mac mini VM)에서 한 번 돌려보고**
판단하는 것이 안전하다. 호스트에서:

```bash
# 1) 저장소를 받고 검토할 브랜치로 체크아웃 (이미 클론돼 있으면 fetch만)
git fetch origin
git checkout econ-proxy-config          # 또는 해당 PR 브랜치명

# 2) 프록시만 단독 검증 (펌웨어 빌드 불필요)
cd tools/econ_proxy
docker compose up -d --build            # 또는: python3 econ_proxy.py
curl -s localhost:8442/health           # {"ok": true}
curl -s "localhost:8442/economic-calendar?from=$(date +%F)&to=$(date +%F)" | head -c 300

# 3) (선택) GitHub CLI 로 PR 단위 체크아웃
gh pr checkout <PR번호>

# 4) 검토가 끝나면 정리
docker compose down
```

확인 포인트:
- `/health` 200, `/economic-calendar`가 배열(JSON) 반환
- VM 밖의 다른 기기에서 `http://<VM-LAN-IP>:8442/health` 접근 가능
- 로그(`docker compose logs -f`)에 에러 없이 요청이 찍히는지

문제 없으면 PR을 머지한다.

---

## 6. 트러블슈팅

| 증상 | 원인/해결 |
|------|-----------|
| 로컬 `localhost:8442`는 되는데 LAN IP로는 안 됨 | VM 네트워크가 NAT → 브리지로 바꾸거나 호스트에서 포트포워딩. 방화벽 8442 허용 |
| `/economic-calendar`가 502 + Cloudflare 메시지 | 데이터센터/VPN IP가 차단됨. `pip install cloudscraper`(Docker는 이미 포함) 후 재시도, 가정용 회선 사용 권장 |
| 기기 화면에 `HTTP 402` | base URL이 아직 FMP. menuconfig에서 프록시 URL로 변경했는지 확인 |
| 기기 화면에 `network error` | 기기가 프록시에 못 닿음(같은 LAN/포트/방화벽 확인). 위 3-(2) 먼저 통과시킬 것 |
| `docker compose` ARM 빌드 실패 | 거의 없음(순수 파이썬). 그래도 막히면 Docker 없이 `python3 econ_proxy.py` |

자세한 코드/엔드포인트는 [`tools/econ_proxy/README.md`](../tools/econ_proxy/README.md) 참고.
