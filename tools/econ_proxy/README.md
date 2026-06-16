# econ_proxy — free economic-calendar source for the ESP32 monitor

FMP and Finnhub both paywall their economic calendar (the device shows
`HTTP 402 Restricted Endpoint` on a free key). This tiny proxy scrapes
investing.com's public calendar and re-serves it as **FMP-shaped JSON**, so the
firmware needs no code change — you just point it at this host.

## Run it (on a PC / Raspberry Pi on the same LAN as the device)

```bash
python3 econ_proxy.py            # listens on 0.0.0.0:8000
# or:  PORT=9000 python3 econ_proxy.py
```

No dependencies required (uses only the Python stdlib). If investing.com starts
returning Cloudflare challenges, install either and the proxy uses it
automatically:

```bash
pip install cloudscraper      # most robust
# or: pip install requests
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

> investing.com's terms discourage scraping; this is for personal, low-volume
> use only (the device only fetches on a button press).
