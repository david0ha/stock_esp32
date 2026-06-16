# econ_proxy — economic-calendar proxy for the ESP32 monitor.
# Multi-arch base (works on x86 and on a Raspberry Pi / arm64).
FROM python:3.12-slim

WORKDIR /app

# Install the optional Cloudflare-resilience deps first (better layer caching).
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY econ_proxy.py .

# Run as a non-root user.
RUN useradd --create-home --uid 10001 app && chown -R app /app
USER app

ENV PORT=8000
ENV PYTHONUNBUFFERED=1
EXPOSE 8000

# Liveness without hammering investing.com (hits the local /health route).
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD python3 -c "import os,urllib.request; urllib.request.urlopen('http://127.0.0.1:%s/health' % os.environ.get('PORT','8000'), timeout=3)" || exit 1

CMD ["python3", "econ_proxy.py"]
