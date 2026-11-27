#!/usr/bin/env python3
"""Traffic client for the biquad bench Wi-Fi build (tools/biquad-bench).

Join the Wi-Fi network printed by the board (default SSID xover-bench, password
in main/Kconfig.projbuild), then run:

    python tools/biquad-bench/wifi_load_client.py

It sends and receives at full speed on one TCP connection to the board. Keep it
running until the board prints BENCH_DONE, then press Ctrl-C.
"""
import argparse
import socket
import threading
import time

CHUNK = bytes(1440)


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5001)
    ap.add_argument("--seconds", type=float, default=1200.0,
                    help="stop after this many seconds (default 1200)")
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=10)
    sock.settimeout(1.0)
    stop = threading.Event()
    lock = threading.Lock()
    counts = {"tx": 0, "rx": 0}

    def sender() -> None:
        while not stop.is_set():
            try:
                n = sock.send(CHUNK)
            except socket.timeout:
                continue
            except OSError:
                stop.set()
                return
            with lock:
                counts["tx"] += n

    def receiver() -> None:
        while not stop.is_set():
            try:
                data = sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                stop.set()
                return
            if not data:
                stop.set()
                return
            with lock:
                counts["rx"] += len(data)

    for fn in (sender, receiver):
        threading.Thread(target=fn, daemon=True).start()

    end = time.monotonic() + args.seconds
    last_tx = last_rx = 0
    try:
        while not stop.is_set() and time.monotonic() < end:
            time.sleep(1.0)
            with lock:
                tx, rx = counts["tx"], counts["rx"]
            print(f"to board {(tx - last_tx) / 1024:8.0f} kB/s   "
                  f"from board {(rx - last_rx) / 1024:8.0f} kB/s", flush=True)
            last_tx, last_rx = tx, rx
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
