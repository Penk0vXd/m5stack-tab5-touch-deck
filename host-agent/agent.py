"""Touch Deck host agent.

Listens on the device's raw HID interface, runs whitelisted commands and pushes
small telemetry values back to the screen.

Security model: the device is untrusted input. It can only name commands that
exist in config.toml, and those are executed as argv lists without a shell.
"""

from __future__ import annotations

import argparse
import logging
import subprocess
import sys
import threading
import time
import tomllib
from pathlib import Path

import hid
import psutil

from protocol import (
    ACK_FAILED,
    ACK_NOT_ALLOWED,
    ACK_OK,
    ACK_UNKNOWN_COMMAND,
    MAX_PAYLOAD,
    MSG_CFG_BEGIN,
    MSG_CFG_CHUNK,
    MSG_CFG_END,
    MSG_CMD,
    MSG_ACK,
    MSG_HELLO,
    MSG_PING,
    MSG_PROFILE,
    MSG_TELEMETRY,
    PACKET_SIZE,
    Packet,
    make_ack,
)

LOG = logging.getLogger("touchdeck")


class DeviceLink:
    """Owns the HID handle and serialises writes from several threads.

    Uses the cython-hidapi `hid.device()` API (module name is still `hid`,
    package name on PyPI is `hidapi` - the plain `hid` package on PyPI is a
    different, ctypes-based wrapper that needs a separately installed
    hidapi.dll and is NOT what requirements.txt pins).
    """

    def __init__(self, vendor_id: int, product_id: int, usage_page: int):
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.usage_page = usage_page
        self._dev: hid.device | None = None
        self._write_lock = threading.Lock()

    def find_path(self) -> bytes | None:
        for info in hid.enumerate(self.vendor_id, self.product_id):
            if info.get("usage_page") == self.usage_page:
                return info["path"]
        return None

    def open(self) -> bool:
        path = self.find_path()
        if path is None:
            return False
        dev = hid.device()
        dev.open_path(path)
        dev.set_nonblocking(False)
        LOG.info("connected to %s %s", dev.get_manufacturer_string(), dev.get_product_string())
        self._dev = dev
        return True

    def close(self) -> None:
        if self._dev is not None:
            try:
                self._dev.close()
            finally:
                self._dev = None

    def read(self, timeout_ms: int = 500) -> Packet | None:
        if self._dev is None:
            return None
        raw = self._dev.read(PACKET_SIZE, timeout_ms=timeout_ms)
        if not raw:
            return None
        return Packet.from_bytes(bytes(raw))

    def send(self, packet: Packet) -> None:
        if self._dev is None:
            return
        with self._write_lock:
            self._dev.write(packet.to_bytes())


class CommandRunner:
    def __init__(self, commands: dict):
        self.commands = commands

    def run(self, name: str) -> tuple[int, str]:
        entry = self.commands.get(name)
        if entry is None:
            LOG.warning("refused unknown command %r", name)
            return ACK_UNKNOWN_COMMAND, "unknown"

        argv = entry.get("argv")
        if not argv or not isinstance(argv, list):
            LOG.error("command %r has no valid argv", name)
            return ACK_NOT_ALLOWED, "bad config"

        cwd = entry.get("cwd")
        LOG.info("running %r: %s", name, " ".join(argv))
        try:
            completed = subprocess.run(
                argv,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=entry.get("timeout", 30),
                shell=False,
            )
        except FileNotFoundError:
            return ACK_FAILED, "not found"
        except subprocess.TimeoutExpired:
            return ACK_FAILED, "timeout"
        except OSError as exc:
            LOG.error("command %r failed: %s", name, exc)
            return ACK_FAILED, str(exc)[:48]

        if completed.returncode != 0:
            LOG.warning("command %r exited %d: %s", name, completed.returncode,
                        completed.stderr.strip()[:200])
            return ACK_FAILED, f"exit {completed.returncode}"
        return ACK_OK, "ok"


def telemetry_loop(link: DeviceLink, interval: float, stop: threading.Event) -> None:
    """Sends a compact "cpu;mem;disk" string; the device parses and renders it."""
    seq = 0
    psutil.cpu_percent(interval=None)  # prime the counter
    while not stop.wait(interval):
        cpu = psutil.cpu_percent(interval=None)
        mem = psutil.virtual_memory().percent
        disk = psutil.disk_usage("C:/").percent if sys.platform == "win32" else psutil.disk_usage("/").percent
        payload = f"{cpu:.0f};{mem:.0f};{disk:.0f}".encode("utf-8")
        seq = (seq + 1) & 0xFF
        try:
            link.send(Packet(MSG_TELEMETRY, seq, payload))
        except OSError as exc:
            LOG.debug("telemetry send failed: %s", exc)
            return


def active_window_title() -> str:
    """Windows only; other platforms get an empty title and no page switching."""
    if sys.platform != "win32":
        return ""
    import ctypes

    user32 = ctypes.windll.user32
    handle = user32.GetForegroundWindow()
    if not handle:
        return ""
    length = user32.GetWindowTextLengthW(handle)
    if length <= 0:
        return ""
    buffer = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(handle, buffer, length + 1)
    return buffer.value


def profile_loop(link: DeviceLink, interval: float, stop: threading.Event) -> None:
    seq = 0
    last_title = None
    while not stop.wait(interval):
        title = active_window_title()
        if not title or title == last_title:
            continue
        last_title = title
        seq = (seq + 1) & 0xFF
        try:
            link.send(Packet(MSG_PROFILE, seq, title.encode("utf-8")[:MAX_PAYLOAD]))
        except OSError as exc:
            LOG.debug("profile send failed: %s", exc)
            return


def serve(config: dict) -> None:
    dev_cfg = config["device"]
    agent_cfg = config.get("agent", {})
    link = DeviceLink(dev_cfg["vendor_id"], dev_cfg["product_id"], dev_cfg["usage_page"])
    runner = CommandRunner(config.get("commands", {}))
    reconnect_delay = float(agent_cfg.get("reconnect_delay", 1.5))
    telemetry_interval = float(agent_cfg.get("telemetry_interval", 2.0))

    while True:
        if not link.open():
            LOG.info("device not found, retrying in %.1fs", reconnect_delay)
            time.sleep(reconnect_delay)
            continue

        stop = threading.Event()
        threading.Thread(
            target=telemetry_loop, args=(link, telemetry_interval, stop), daemon=True
        ).start()
        threading.Thread(
            target=profile_loop,
            args=(link, float(agent_cfg.get("profile_interval", 1.0)), stop),
            daemon=True,
        ).start()

        try:
            while True:
                packet = link.read()
                if packet is None:
                    continue
                if packet.msg_type == MSG_HELLO:
                    LOG.info("device says hello: %s", packet.text())
                elif packet.msg_type == MSG_PING:
                    link.send(Packet(MSG_PING, packet.seq, b""))
                elif packet.msg_type == MSG_CMD:
                    status, detail = runner.run(packet.text())
                    link.send(make_ack(packet.seq, status, detail))
                else:
                    LOG.debug("unhandled message type 0x%02x", packet.msg_type)
        except OSError as exc:
            LOG.warning("link lost: %s", exc)
        finally:
            stop.set()
            link.close()
            time.sleep(reconnect_delay)


def push_config(config: dict, deck_config: Path) -> int:
    """Uploads a config.json to the device over raw HID and waits for the ACK."""
    if not deck_config.exists():
        LOG.error("no such file: %s", deck_config)
        return 2

    body = deck_config.read_bytes()
    dev_cfg = config["device"]
    link = DeviceLink(dev_cfg["vendor_id"], dev_cfg["product_id"], dev_cfg["usage_page"])
    if not link.open():
        LOG.error("device not found")
        return 1

    try:
        seq = 0
        link.send(Packet(MSG_CFG_BEGIN, seq, deck_config.name.encode("utf-8")[:MAX_PAYLOAD]))
        for offset in range(0, len(body), MAX_PAYLOAD):
            seq = (seq + 1) & 0xFF
            link.send(Packet(MSG_CFG_CHUNK, seq, body[offset:offset + MAX_PAYLOAD]))
            time.sleep(0.002)  # let the device drain its queue
        seq = (seq + 1) & 0xFF
        link.send(Packet(MSG_CFG_END, seq, b""))
        LOG.info("sent %d bytes, waiting for ACK", len(body))

        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            packet = link.read(timeout_ms=500)
            if packet is None or packet.msg_type != MSG_ACK:
                continue
            status = packet.payload[0] if packet.payload else ACK_FAILED
            detail = packet.payload[1:].decode("utf-8", errors="replace").rstrip("\x00")
            if status == ACK_OK:
                LOG.info("device applied the config: %s", detail)
                return 0
            LOG.error("device rejected the config: %s", detail)
            return 1
        LOG.error("no answer from device")
        return 1
    finally:
        link.close()


def load_config(path: Path) -> dict:
    if not path.exists():
        raise SystemExit(f"config not found: {path} (copy config.example.toml)")
    with path.open("rb") as handle:
        return tomllib.load(handle)


def main() -> None:
    parser = argparse.ArgumentParser(description="Touch Deck host agent")
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("config.toml"))
    parser.add_argument(
        "--push-config",
        type=Path,
        metavar="DECK_CONFIG",
        help="upload a deck config.json to the device and exit",
    )
    args = parser.parse_args()

    config = load_config(args.config)
    logging.basicConfig(
        level=config.get("agent", {}).get("log_level", "INFO"),
        format="%(asctime)s %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.push_config is not None:
        raise SystemExit(push_config(config, args.push_config))

    try:
        serve(config)
    except KeyboardInterrupt:
        LOG.info("stopped")


if __name__ == "__main__":
    main()
