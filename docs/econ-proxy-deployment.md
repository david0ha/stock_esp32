# Economic Calendar Proxy Deployment Guide (`econ_proxy`)

The economic calendar screen (KEY+BOOT) of the ESP32-S3-RLCD-4.2 monitor uses FMP
as its default data source, but both FMP and Finnhub expose their calendar endpoints
as **paid-plan only**, so a free key returns `HTTP 402`. `tools/econ_proxy` is a
self-hosted proxy that scrapes investing.com's public calendar and serves it back as
**JSON in the same shape as FMP**. The device needs no firmware changes — just point
`CONFIG_STOCK_ECON_BASE_URL` at this proxy.

- **Default port: `8442`** (changeable via the `PORT` environment variable or compose port mapping)
- Endpoint: `GET /economic-calendar?from=YYYY-MM-DD&to=YYYY-MM-DD`
- Health check: `GET /health` → `{"ok": true}` (no upstream call)
- Response timestamps are in UTC; the device converts them to its own timezone (KST by default)

> ⚠️ investing.com's terms of service discourage scraping. Use it for **personal,
> low-frequency purposes** only (the device only calls it when you press a button).

---

## 1. Recommended Setup: Linux VM on a Mac mini

```
[ESP32-S3]  --(WiFi/LAN)-->  [Linux VM : econ_proxy:8442]  --(internet)-->  investing.com
                                   └ Mac mini host
```

### Architecture
- Apple Silicon Mac mini → **arm64** Linux VM. The container base image
  `python:3.12-slim` is multi-arch, so it is pulled as arm64, and the dependencies
  (`cloudscraper`, `requests`) are pure Python, so they install without compilation.
- Intel Mac mini → x86_64. Works the same way.

### ⚠️ The single most important thing — the VM network mode
The ESP32 is a **separate device** on WiFi, so the VM's `8442` port must be
reachable **from the LAN**. If the VM uses the default **NAT mode, the VM only gets a
host-internal IP and the ESP32 cannot reach it.** Configure it one of two ways:

1. **(Recommended) Bridged network** — the VM gets its own LAN IP from the router
   (e.g. `192.168.0.50`). On the device, use `http://192.168.0.50:8442/economic-calendar`.
2. **NAT + port forwarding** — forward the Mac host's `8442` → VM's `8442`, and on the
   device use `http://<Mac mini's LAN IP>:8442/economic-calendar`.

Then allow `8442` inbound on the VM/host firewall.

---

## 2. Deployment (Docker, recommended)

Inside the VM:

```bash
git clone <this repo> && cd <repo>/tools/econ_proxy
docker compose up -d            # build + run on port 8442, auto-restart on boot/crash
docker compose logs -f          # request logs
docker compose down             # stop
```

- `restart: unless-stopped` in `docker-compose.yml` makes the container recover automatically.
- To change the port, edit only the host side of `ports:` (e.g. `"9000:8442"`).
- Fully unattended operation: ① make the VM's Docker start automatically on boot
  (`systemctl enable docker`), and ② make **the VM itself start automatically when the
  Mac boots** (UTM/Parallels autostart option).

### Without Docker
```bash
python3 econ_proxy.py            # zero dependencies (stdlib). Change with PORT=9000
pip install -r requirements.txt  # (optional) cloudscraper/requests for Cloudflare bypass
```

---

## 3. Verifying it works (in order)

```bash
# (1) Locally on the VM — check the server itself
curl -s localhost:8442/health
#   → {"ok": true}
curl -s "localhost:8442/economic-calendar?from=2026-06-15&to=2026-06-21" | head -c 200
#   → [{"date":"...","country":"USD",...}]  (an array means it works)

# (2) From another device (laptop/phone) — check LAN access  ★ most important
curl -s "http://<VM-LAN-IP>:8442/health"
#   → {"ok": true} must appear for the ESP32 to reach it too.
```

If (2) fails while (1) works, it's a **network mode problem** (section 1 above — bridge/port forwarding).

---

## 4. Device Configuration

`idf.py menuconfig` → **Stock Monitor**:
- `Economic calendar base URL` = `http://<VM-LAN-IP>:8442/economic-calendar`
- `Financial Modeling Prep (FMP) API key` = any non-empty value (the proxy ignores it)
- (optional) `Economic calendar: minimum importance` — 1=all / 2=Medium+High (default) / 3=High only

Then `idf.py build flash`. Opening the calendar with KEY+BOOT shows investing.com data
(including actual figures); KEY=previous week / BOOT=next week.

---

## 5. Reviewing this branch on a remote host before merging

Proxy/deployment changes are safest to judge by **running them once on the actual host
(the Mac mini VM) before merging.** On the host:

```bash
# 1) Fetch the repo and check out the branch to review (just fetch if already cloned)
git fetch origin
git checkout econ-proxy-config          # or the relevant PR branch name

# 2) Verify the proxy in isolation (no firmware build needed)
cd tools/econ_proxy
docker compose up -d --build            # or: python3 econ_proxy.py
curl -s localhost:8442/health           # {"ok": true}
curl -s "localhost:8442/economic-calendar?from=$(date +%F)&to=$(date +%F)" | head -c 300

# 3) (optional) Check out per-PR with the GitHub CLI
gh pr checkout <PR number>

# 4) Clean up when the review is done
docker compose down
```

Things to check:
- `/health` returns 200, `/economic-calendar` returns an array (JSON)
- `http://<VM-LAN-IP>:8442/health` is reachable from another device outside the VM
- Requests are logged without errors (`docker compose logs -f`)

If there are no issues, merge the PR.

---

## 6. Troubleshooting

| Symptom | Cause / fix |
|------|-----------|
| `localhost:8442` works locally but not via the LAN IP | VM network is NAT → switch to bridge or port-forward from the host. Allow 8442 in the firewall |
| `/economic-calendar` returns 502 + a Cloudflare message | Datacenter/VPN IP is blocked. Run `pip install cloudscraper` (already included in Docker) and retry; a residential connection is recommended |
| Device screen shows `HTTP 402` | The base URL is still FMP. Check that you changed it to the proxy URL in menuconfig |
| Device screen shows `network error` | The device can't reach the proxy (check same LAN/port/firewall). Get step 3-(2) above to pass first |
| `docker compose` ARM build fails | Almost never happens (pure Python). If it still gets stuck, run `python3 econ_proxy.py` without Docker |

For detailed code/endpoints, see [`tools/econ_proxy/README.md`](../tools/econ_proxy/README.md).

---

## 7. Exposing to the internet: Cloudflare Tunnel + shared secret (no LAN/NAT needed)

Instead of the bridge/port forwarding in §1, exposing the proxy on a fixed HTTPS
hostname via **Cloudflare Tunnel** means the ESP32 doesn't have to be on the same LAN
(the NAT problem disappears entirely). The device firmware already bundles the
Mozilla CA bundle (`esp_crt_bundle`) for communication, so you can use an
`https://...` hostname as-is with no extra code.

### 7-1. Shared secret (required when exposed)
Once it's public, anyone could use this proxy to scrape investing.com, so **always
set a token.** When `ECON_PROXY_TOKEN` is set, the proxy requires a matching
`?apikey=<token>` on `/economic-calendar` (mismatch → `401`). `/health` stays open.
The firmware already appends the `CONFIG_STOCK_FMP_API_KEY` value as `apikey`, so just
**set that config value equal to the token** and you're done.

```bash
# Generate a token → inject into the container via host env (or .env)
export ECON_PROXY_TOKEN=$(openssl rand -hex 16)
cd tools/econ_proxy && docker compose up -d --build
echo "Put this value in the FMP API key in menuconfig: $ECON_PROXY_TOKEN"
```

When deploying via CI/CD, store this value as the **GitHub repo secret `ECON_PROXY_TOKEN`**
(see §9 below). Device menuconfig: `Economic calendar base URL =
https://econ.<domain>/economic-calendar`, `FMP API key = <token>`.

> **How to get the token into the container (pick one).** ① CD workflow — injected
> automatically from a GitHub secret, **no `.env` needed on the server**. ② Manual
> `docker compose up` on the server — requires a `tools/econ_proxy/.env` file
> (`cp .env.example .env`, then fill in the value; `.env` is gitignored). ③ AWS — Secrets
> Manager/SSM → task env. ⚠️ Running a manual `up` without the token recreates the
> container with **auth OFF**, so if manual operation is possible, it's recommended to
> keep the `.env` from ② in place as well.

### 7-2. Creating a dedicated tunnel
If `cloudflared` is already installed and you have `~/.cloudflared/cert.pem` (account
login), you can create a new tunnel without logging in again.

```bash
# 1) Create a dedicated tunnel → issues ~/.cloudflared/<UUID>.json credentials
cloudflared tunnel create econ-proxy

# 2) Create a public DNS record (CNAME) — a subdomain of a zone you own
cloudflared tunnel route dns econ-proxy econ.<domain>

# 3) Local ingress config
cat > ~/.cloudflared/econ-config.yml <<'YAML'
tunnel: econ-proxy
credentials-file: /home/deploy/.cloudflared/<UUID>.json
ingress:
  - hostname: econ.<domain>
    service: http://localhost:8442
  - service: http_status:404
YAML

# 4) Confirm it works in the foreground
cloudflared tunnel --config ~/.cloudflared/econ-config.yml run econ-proxy
#   In another shell:  curl -s https://econ.<domain>/health  → {"ok": true}
```

### 7-3. Run it permanently as a systemd service (sudo required)
```ini
# /etc/systemd/system/cloudflared-econ.service
[Unit]
Description=cloudflared econ-proxy tunnel (econ.<domain>)
After=network-online.target
Wants=network-online.target

[Service]
User=deploy
ExecStart=/usr/local/bin/cloudflared --no-autoupdate --config /home/deploy/.cloudflared/econ-config.yml tunnel run econ-proxy
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

## 8. About free hostnames

- **Cloudflare Quick Tunnel** (`cloudflared tunnel --url http://localhost:8442`) gives
  you a free `*.trycloudflare.com` address instantly with no account or domain, but
  **the address changes on every restart.** It's unsuitable for flashing a fixed URL
  onto the device.
- A **subdomain** of a domain you own (`example.com`/`example.org`, etc.) **costs $0
  extra on the Cloudflare free plan** (tunnels and DNS records are free). Since you
  need a fixed address, the subdomain approach is recommended.

---

## 9. Automated deployment with a self-hosted runner (CI/CD)

This repo includes `.github/workflows/deploy-econ-proxy.yml`. When a push to main
changes `tools/econ_proxy/**`, a **self-hosted runner with the `econ-proxy` label**
redeploys the container with `docker compose up -d --build` and verifies it via
`/health`. The container's `restart: unless-stopped` handles always-on operation
normally, and this workflow only re-rolls when the code changes.

### 9-1. Registering a dedicated runner for stock_esp32
A single GitHub runner serves only one repo, so put **a second runner in a separate directory.**

```bash
# Issue a registration token (requires repo admin permission)
gh api -X POST repos/youruser/stock_esp32/actions/runners/registration-token -q .token

mkdir -p ~/actions-runner-stock && cd ~/actions-runner-stock
# Download the runner for the host architecture (this host is arm64)
curl -fsSLO https://github.com/actions/runner/releases/download/v2.335.1/actions-runner-linux-arm64-2.335.1.tar.gz
tar xzf actions-runner-linux-arm64-2.335.1.tar.gz

# Register with the label econ-proxy (must match runs-on in the workflow)
./config.sh --url https://github.com/youruser/stock_esp32 \
            --token <the token issued above> \
            --name stock-esp32-econ --labels econ-proxy --unattended

# Run permanently as a systemd service
sudo ./svc.sh install deploy
sudo ./svc.sh start
```

### 9-2. Set the repo secret
```bash
gh secret set ECON_PROXY_TOKEN --repo youruser/stock_esp32   # same value as the proxy token
```

Since the runner uses `docker compose`, the runner's service account (deploy) must be in
the `docker` group (`sudo usermod -aG docker deploy`, then re-login). After that, a push
to main triggers an automatic build/deploy.
