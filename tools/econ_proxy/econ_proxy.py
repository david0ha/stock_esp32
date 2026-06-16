#!/usr/bin/env python3
"""
econ_proxy — a tiny self-hosted economic-calendar proxy for the ESP32 monitor.

The FMP / Finnhub calendars are paywalled, so this scrapes investing.com's
public economic-calendar AJAX endpoint and re-serves it as FMP-shaped JSON, so
the firmware needs no code change — just point CONFIG_STOCK_ECON_BASE_URL at
this host (e.g. http://192.168.0.10:8000/economic-calendar).

    GET /economic-calendar?from=YYYY-MM-DD&to=YYYY-MM-DD[&apikey=ignored]
    ->  [ {"date":"YYYY-MM-DD HH:MM:SS",   # UTC
           "country":"USD", "event":"Core CPI (MoM)",
           "estimate":"0.3%", "actual":"", "previous":"0.2%",
           "impact":"High"}, ... ]

Run:  python3 econ_proxy.py            # listens on 0.0.0.0:8000
      PORT=9000 python3 econ_proxy.py

Times are requested in GMT (investing timeZone=55) so the JSON is UTC; the
device applies its own timezone. Importance comes from the bull-icon count
(3=High, 2=Medium, 1=Low); holidays/0 are emitted as impact "None".

Note: investing.com's terms discourage scraping — this is intended for personal,
low-volume use only. Be polite (the device polls at most on button presses).
"""
import html
import json
import os
import re
import sys
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

INVESTING_URL = "https://www.investing.com/economic-calendar/Service/getCalendarFilteredData"
UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/124.0 Safari/537.36")
TIMEZONE_GMT = "55"        # investing.com tz id whose times are UTC
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


def fetch_investing(date_from, date_to):
    """POST the investing.com AJAX endpoint; return the raw HTML rows string.

    Tries cloudscraper -> requests -> urllib so it works out of the box (urllib,
    zero deps) but can transparently upgrade if Cloudflare starts blocking."""
    fields = [
        ("importance[]", "1"), ("importance[]", "2"), ("importance[]", "3"),
        ("timeZone", TIMEZONE_GMT), ("timeFilter", "timeRemain"),
        ("currentTab", "custom"), ("limit_from", "0"),
        ("dateFrom", date_from), ("dateTo", date_to),
    ]
    body = urllib.parse.urlencode(fields)
    headers = {
        "User-Agent": UA,
        "X-Requested-With": "XMLHttpRequest",
        "Referer": "https://www.investing.com/economic-calendar/",
        "Content-Type": "application/x-www-form-urlencoded",
        "Accept": "*/*",
    }

    # cloudscraper / requests if available (more robust against Cloudflare)
    for mod in ("cloudscraper", "requests"):
        try:
            lib = __import__(mod)
            sess = lib.create_scraper() if mod == "cloudscraper" else lib
            r = sess.post(INVESTING_URL, data=body, headers=headers, timeout=25)
            r.raise_for_status()
            return r.json()["data"]
        except ImportError:
            continue

    req = urllib.request.Request(INVESTING_URL, data=body.encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=25) as resp:
        return json.loads(resp.read().decode("utf-8", "replace"))["data"]


def _text(cell_html):
    """Strip tags + unescape entities -> single-spaced trimmed text."""
    return re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", " ", cell_html))).strip()


IMPACT_BY_BULLS = {3: "High", 2: "Medium", 1: "Low", 0: "None"}


def parse_rows(rows_html):
    """investing.com event rows -> list of FMP-shaped event dicts."""
    events = []
    for tr in re.findall(r'<tr id="eventRowId_\d+".*?</tr>', rows_html, re.S):
        dt = re.search(r'data-event-datetime="([^"]+)"', tr)
        if not dt:
            continue
        # "2026/06/16 12:00:00" (UTC) -> "2026-06-16 12:00:00"
        when = dt.group(1).replace("/", "-")

        tds = re.findall(r"<td[^>]*>(.*?)</td>", tr, re.S)
        if len(tds) < 7:
            continue
        bulls = tds[2].count("grayFullBullishIcon")
        events.append({
            "date":     when,
            "country":  _text(tds[1]),                 # currency code, e.g. "USD"
            "event":    _text(tds[3]),
            "actual":   _text(tds[4]),
            "estimate": _text(tds[5]),                 # investing "forecast"
            "previous": _text(tds[6]),
            "impact":   IMPACT_BY_BULLS.get(bulls, "None"),
        })
    return events


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path.rstrip("/") != "/economic-calendar":
            self._send(404, {"Error Message": "not found"})
            return
        q = urllib.parse.parse_qs(parsed.query)
        date_from = (q.get("from") or [""])[0]
        date_to = (q.get("to") or [date_from])[0]
        if not DATE_RE.match(date_from) or not DATE_RE.match(date_to):
            self._send(400, {"Error Message": "from/to must be YYYY-MM-DD"})
            return
        try:
            rows = fetch_investing(date_from, date_to)
            events = parse_rows(rows)
        except Exception as e:                          # noqa: BLE001
            self._send(502, {"Error Message": "investing.com fetch failed: %s" % e})
            return
        self._send(200, events)


def main():
    port = int(os.environ.get("PORT", "8000"))
    srv = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print("econ_proxy listening on http://0.0.0.0:%d/economic-calendar" % port,
          file=sys.stderr)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
