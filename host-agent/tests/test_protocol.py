import sys
import unittest
from pathlib import Path


HOST_AGENT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST_AGENT_DIR))

from protocol import (  # noqa: E402
    ACK_OK,
    MAX_PAYLOAD,
    MSG_ACK,
    MSG_CMD,
    PACKET_SIZE,
    Packet,
    make_ack,
)


class PacketTests(unittest.TestCase):
    def test_round_trip_preserves_packet(self) -> None:
        packet = Packet(MSG_CMD, 42, b"git_status")

        encoded = packet.to_bytes()
        decoded = Packet.from_bytes(encoded[1:])

        self.assertEqual(len(encoded), PACKET_SIZE + 1)
        self.assertEqual(decoded, packet)

    def test_payload_limit_is_enforced(self) -> None:
        with self.assertRaises(ValueError):
            Packet(MSG_CMD, 0, bytes(MAX_PAYLOAD + 1)).to_bytes()

    def test_ack_detail_is_truncated_to_packet_capacity(self) -> None:
        packet = make_ack(7, ACK_OK, "x" * 200)

        self.assertEqual(packet.msg_type, MSG_ACK)
        self.assertEqual(packet.seq, 7)
        self.assertEqual(packet.payload[0], ACK_OK)
        self.assertEqual(len(packet.payload), MAX_PAYLOAD)


if __name__ == "__main__":
    unittest.main()
