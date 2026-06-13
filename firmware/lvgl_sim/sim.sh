#!/usr/bin/env bash
# LVGL 시뮬레이터: 빌드 → 실행 → PNG 스크린샷
# 사용법: ./sim.sh
set -e
cd "$(dirname "$0")"

[ -d build ] || cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

mkdir -p shots
./build/sim shots
for n in 1 2; do
  sips -s format png "shots/sim_frame${n}.bmp" --out "shots/sim_frame${n}.png" >/dev/null
done
echo "스크린샷: shots/sim_frame1.png, shots/sim_frame2.png"
