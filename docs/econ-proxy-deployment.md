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
포함)가 표시된다. 캘린더 안에서 **KEY=이번 주 다음 이벤트 페이지(주 안에서 순환)**,
**BOOT=다음 주(이번 달의 주들 안에서 순환)**, 다시 **KEY+BOOT=홈 복귀**다.

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

---

## 7. 인터넷 노출: Cloudflare Tunnel + 공유 시크릿 (LAN/NAT 불필요)

§1의 브리지/포트포워딩 대신, **Cloudflare Tunnel**로 프록시를 고정 HTTPS
호스트네임에 노출하면 ESP32가 같은 LAN에 없어도 된다(NAT 문제 자체가 사라진다).
기기 펌웨어는 이미 Mozilla CA 번들(`esp_crt_bundle`)을 붙여 통신하므로
`https://...` 호스트네임을 추가 코드 없이 그대로 쓸 수 있다.

### 7-1. 공유 시크릿 (공개 시 필수)
공개되면 누구나 이 프록시로 investing.com을 긁을 수 있으므로 **반드시 토큰을 건다.**
프록시는 `ECON_PROXY_TOKEN`이 설정되면 `/economic-calendar`에 일치하는
`?apikey=<token>`을 요구한다(불일치 → `401`). `/health`는 열려 있다. 펌웨어는 이미
`CONFIG_STOCK_FMP_API_KEY` 값을 `apikey`로 붙이므로, **그 설정값을 토큰과 동일하게**
두면 끝이다.

```bash
# 토큰 생성 → 호스트 env(또는 .env)로 컨테이너에 주입
export ECON_PROXY_TOKEN=$(openssl rand -hex 16)
cd tools/econ_proxy && docker compose up -d --build
echo "이 값을 menuconfig의 FMP API key 에 넣는다: $ECON_PROXY_TOKEN"
```

CI/CD로 배포할 때는 이 값을 **GitHub repo secret `ECON_PROXY_TOKEN`** 으로 넣는다
(아래 §9). 기기 menuconfig: `Economic calendar base URL =
https://econ.<도메인>/economic-calendar`, `FMP API key = <토큰>`.

> **토큰을 컨테이너에 넣는 방법(택1).** ① CD 워크플로 — GitHub secret에서 자동 주입,
> **서버에 `.env` 불필요**. ② 서버에서 수동 `docker compose up` — `tools/econ_proxy/.env`
> 파일이 필요(`cp .env.example .env` 후 값 입력; `.env`는 gitignore됨). ③ AWS — Secrets
> Manager/SSM → 태스크 env. ⚠️ 토큰 없이 수동 `up`을 돌리면 컨테이너가 **인증 OFF**로
> 재생성되니, 수동 운용 가능성이 있으면 ②의 `.env`를 함께 두는 것을 권장한다.

### 7-2. 전용 터널 만들기 (kanjis와 동일한 named-tunnel 패턴)
이미 `cloudflared`가 깔려 있고 `~/.cloudflared/cert.pem`(계정 로그인)이 있으면
재로그인 없이 새 터널을 만들 수 있다.

```bash
# 1) 전용 터널 생성 → ~/.cloudflared/<UUID>.json 자격증명 발급
cloudflared tunnel create econ-proxy

# 2) 공개 DNS 레코드(CNAME) 생성 — 보유 중인 존의 서브도메인
cloudflared tunnel route dns econ-proxy econ.<도메인>

# 3) 로컬 ingress 설정 (tj-song-search 터널과 같은 config 방식)
cat > ~/.cloudflared/econ-config.yml <<'YAML'
tunnel: econ-proxy
credentials-file: /home/utmgg/.cloudflared/<UUID>.json
ingress:
  - hostname: econ.<도메인>
    service: http://localhost:8442
  - service: http_status:404
YAML

# 4) 포그라운드로 동작 확인
cloudflared tunnel --config ~/.cloudflared/econ-config.yml run econ-proxy
#   다른 셸에서:  curl -s https://econ.<도메인>/health  → {"ok": true}
```

### 7-3. systemd 서비스로 상시 가동 (sudo 필요)
```ini
# /etc/systemd/system/cloudflared-econ.service
[Unit]
Description=cloudflared econ-proxy tunnel (econ.<도메인>)
After=network-online.target
Wants=network-online.target

[Service]
User=utmgg
ExecStart=/usr/local/bin/cloudflared --no-autoupdate --config /home/utmgg/.cloudflared/econ-config.yml tunnel run econ-proxy
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now cloudflared-econ
sudo systemctl status cloudflared-econ
```

---

## 8. 무료 호스트네임에 대하여

- **Cloudflare Quick Tunnel**(`cloudflared tunnel --url http://localhost:8442`)은
  계정·도메인 없이 즉시 `*.trycloudflare.com` 무료 주소를 주지만, **재시작마다
  주소가 바뀐다.** 기기에 URL을 고정 플래시하는 용도엔 부적합.
- 보유 도메인(`mojikun.com`/`tjsongsearch.org` 등)의 **서브도메인은 Cloudflare
  무료 플랜에서 추가 비용 0원**이다(터널·DNS 레코드 무료). 고정 주소가 필요하므로
  서브도메인 방식을 권장한다.

---

## 9. self-hosted runner로 자동배포 (CI/CD)

이 저장소에는 `.github/workflows/deploy-econ-proxy.yml`이 있다. main에 푸시되어
`tools/econ_proxy/**`가 바뀌면, **`econ-proxy` 라벨을 가진 self-hosted 러너**에서
`docker compose up -d --build` 로 컨테이너를 재배포하고 `/health`로 검증한다.
컨테이너의 `restart: unless-stopped` 가 평소 상시 가동을 담당하고, 이 워크플로는
코드가 바뀔 때만 재롤한다.

### 9-1. stock_esp32 전용 러너 등록 (기존 tj-song-search 러너와 별도)
GitHub 러너 1개는 1개 레포만 담당하므로, **별도 디렉터리에 두 번째 러너**를 둔다.

```bash
# 등록 토큰 발급 (repo admin 권한 필요)
gh api -X POST repos/david0ha/stock_esp32/actions/runners/registration-token -q .token

mkdir -p ~/actions-runner-stock && cd ~/actions-runner-stock
# 호스트 아키텍처에 맞는 러너 받기 (이 호스트는 arm64)
curl -fsSLO https://github.com/actions/runner/releases/download/v2.335.1/actions-runner-linux-arm64-2.335.1.tar.gz
tar xzf actions-runner-linux-arm64-2.335.1.tar.gz

# 라벨 econ-proxy 로 등록 (워크플로의 runs-on 과 일치해야 함)
./config.sh --url https://github.com/david0ha/stock_esp32 \
            --token <위에서 받은 토큰> \
            --name stock-esp32-econ --labels econ-proxy --unattended

# systemd 서비스로 상시 가동
sudo ./svc.sh install utmgg
sudo ./svc.sh start
```

### 9-2. repo secret 설정
```bash
gh secret set ECON_PROXY_TOKEN --repo david0ha/stock_esp32   # 프록시 토큰과 동일 값
```

러너가 `docker compose`를 쓰므로 러너 실행 계정(utmgg)이 `docker` 그룹에 있어야 한다
(`sudo usermod -aG docker utmgg` 후 재로그인). 이후 main 푸시 → 자동 빌드/배포.
