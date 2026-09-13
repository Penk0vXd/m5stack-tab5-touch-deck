import queue
import sys
import threading
import unittest
from pathlib import Path
from unittest.mock import patch


HOST_AGENT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST_AGENT_DIR))

from agent import CommandRunner, command_loop  # noqa: E402
from protocol import ACK_OK, ACK_UNKNOWN_COMMAND, MSG_ACK, MSG_CMD, Packet  # noqa: E402


class FakeLink:
    def __init__(self) -> None:
        self.sent: list[Packet] = []
        self.sent_event = threading.Event()

    def send(self, packet: Packet) -> None:
        self.sent.append(packet)
        self.sent_event.set()


class FakeRunner:
    def run(self, name: str) -> tuple[int, str]:
        return ACK_OK, f"ran {name}"


class CommandTests(unittest.TestCase):
    def test_unknown_command_is_refused(self) -> None:
        status, detail = CommandRunner({}).run("not_allowed")

        self.assertEqual(status, ACK_UNKNOWN_COMMAND)
        self.assertEqual(detail, "unknown")

    @patch("agent.subprocess.Popen")
    def test_detached_command_returns_without_waiting(self, popen) -> None:
        runner = CommandRunner({"notes": {"argv": ["notepad.exe"], "detach": True}})

        status, detail = runner.run("notes")

        self.assertEqual(status, ACK_OK)
        self.assertEqual(detail, "launched")
        popen.assert_called_once()

    def test_command_worker_sends_ack_without_using_receive_loop(self) -> None:
        link = FakeLink()
        requests: queue.Queue[Packet] = queue.Queue()
        stop = threading.Event()
        worker = threading.Thread(
            target=command_loop,
            args=(link, FakeRunner(), requests, stop),
        )
        worker.start()

        requests.put(Packet(MSG_CMD, 19, b"git_status"))
        self.assertTrue(link.sent_event.wait(1.0))
        stop.set()
        worker.join(1.0)

        self.assertFalse(worker.is_alive())
        self.assertEqual(len(link.sent), 1)
        self.assertEqual(link.sent[0].msg_type, MSG_ACK)
        self.assertEqual(link.sent[0].seq, 19)
        self.assertEqual(link.sent[0].payload[0], ACK_OK)


if __name__ == "__main__":
    unittest.main()
