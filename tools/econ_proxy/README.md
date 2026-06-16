# econ_proxy — free economic-calendar source for the ESP32 monitor

FMP and Finnhub both paywall their economic calendar (the device shows
`HTTP 402 Restricted Endpoint` on a free key). This tiny proxy scrapes
investing.com's public calendar and re-serves it as **FMP-shaped JSON**, so the
firmware needs no code change — you just point it at this host.

## Run it (on a PC / Raspberry Pi on the same LAN as the device)

### Option A — Docker (recommended for always-on)

```bash
cd tools/econ_proxy
docker compose up -d            # build + run on :8000, restarts on boot/crash
docker compose logs -f          # watch requests
docker compose down             # stop
```

The image bundles `cloudscraper`/`requests` (Cloudflare resilience) plus a
`/health` healthcheck, and runs as a non-root user. It's multi-arch, so the same
command works on a Raspberry Pi (arm64). To relocate the port, edit the host side
of the `ports:` mapping in `docker-compose.yml` (e.g. `"9000:8000"`).

Without compose:

```bash
docker build -t econ-proxy .
docker run -d --name econ-proxy --restart unless-stopped -p 8000:8000 econ-proxy
```

### Option B — plain Python

```bash
python3 econ_proxy.py            # listens on 0.0.0.0:8000
# or:  PORT=9000 python3 econ_proxy.py
```

No dependencies required (uses only the Python stdlib). If investing.com starts
returning Cloudflare challenges, install the extras and the proxy uses them
automatically:

```bash
pip install -r requirements.txt   # cloudscraper + requests
```

## Point the device at it

`idf.py menuconfig` → **Stock Monitor**:
- `Economic calendar base URL` = `http://<this-host-ip>:8000/economic-calendar`
- `Financial Modeling Prep (FMP) API key` = any non-empty placeholder (ignored by the proxy)

Then `idf.py build flash`. KEY+BOOT opens the calendar; it now serves
investing.com data (with **actual** values and full prev/next-week navigation).

## API

```
GET /economic-calendar?from=YYYY-MM-DD&to=YYYY-MM-DD[&apikey=ignored]
-> [ {"date":"YYYY-MM-DD HH:MM:SS",        # UTC
      "country":"USD","event":"Core CPI (MoM)",
      "estimate":"0.3%","actual":"","previous":"0.2%","impact":"High"}, ... ]
```

Times are UTC (the device converts to its own timezone). Importance: 3 bull
icons = High, 2 = Medium, 1 = Low.

`GET /health` returns `{"ok": true}` (used by the Docker healthcheck; no upstream call).

> investing.com's terms discourage scraping; this is for personal, low-volume
> use only (the device only fetches on a button press).
