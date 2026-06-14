#!/usr/bin/env bash
# LVGL simulator: build -> fetch live data -> render -> PNG screenshots.
# Usage:  FINNHUB_KEY=xxxx STOCK_SYMBOL=AAPL ./sim.sh
set -e
cd "$(dirname "$0")"

[ -d build ] || cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

mkdir -p shots
./build/sim shots
for n in 0 1 2; do
  sips -s format png "shots/sim_page${n}.bmp" --out "shots/sim_page${n}.png" >/dev/null
done
echo "screenshots: shots/sim_page0.png (chart)  sim_page1.png (news)  sim_page2.png (metrics)"
