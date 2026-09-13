"""Raw HID wire protocol shared by the agent and the firmware.

Every packet is exactly 64 bytes:

    byte 0      message type
    byte 1      sequence number, echoed in the ACK
    byte 2      payload length (0..61)
    bytes 3..63 payload, zero padded

Windows hidapi expects the report ID as an extra leading byte on writes, so
outgoing buffers are 65 bytes with a leading 0x00.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

PACKET_SIZE = 64
HEADER_SIZE = 3
MAX_PAYLOAD = PACKET_SIZE - HEADER_SIZE

# Message types
MSG_HELLO = 0x01  # device -> agent, sent on connect
MSG_CMD = 0x02  # device -> agent, run a whitelisted command
MSG_ACK = 0x03  # agent -> device, result of a CMD
MSG_TELEMETRY = 0x04  # agent -> device, small status values
MSG_PROFILE = 0x05  # agent -> device, active window/profile name
MSG_PING = 0x06  # either direction, keeps the link observable
MSG_CFG_BEGIN = 0x07  # agent -> device, start of a config.json upload
MSG_CFG_CHUNK = 0x08  # agent -> device, ordered body bytes
MSG_CFG_END = 0x09  # agent -> device, upload finished

ACK_OK = 0x00
ACK_UNKNOWN_COMMAND = 0x01
ACK_NOT_ALLOWED = 0x02
ACK_FAILED = 0x03


@dataclass
class Packet:
    msg_type: int
    seq: int
    payload: bytes

    def to_bytes(self, report_id: int = 0x00) -> bytes:
        if len(self.payload) > MAX_PAYLOAD:
            raise ValueError(f"payload too long: {len(self.payload)} > {MAX_PAYLOAD}")
        body = struct.pack("BBB", self.msg_type, self.seq, len(self.payload))
        body += self.payload
        body += bytes(PACKET_SIZE - len(body))
        return bytes([report_id]) + body

    @classmethod
    def from_bytes(cls, raw: bytes) -> "Packet":
        if len(raw) < HEADER_SIZE:
            raise ValueError("short packet")
        msg_type, seq, length = struct.unpack_from("BBB", raw, 0)
        length = min(length, MAX_PAYLOAD)
        return cls(msg_type, seq, bytes(raw[HEADER_SIZE:HEADER_SIZE + length]))

    def text(self) -> str:
        return self.payload.decode("utf-8", errors="replace").rstrip("\x00")


def make_ack(seq: int, status: int, detail: str = "") -> Packet:
    payload = bytes([status]) + detail.encode("utf-8")[: MAX_PAYLOAD - 1]
    return Packet(MSG_ACK, seq, payload)
