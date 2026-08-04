"""向 CYD Kimi 用量显示器发送串口命令并等待 OK:/ERR: 响应。"""
import argparse
import sys
import time

import serial


def send_command(port: str, command: str, baud: int = 115200, timeout: float = 3.0) -> int:
    for attempt in range(2):
        try:
            with serial.Serial(port, baud, timeout=timeout) as ser:
                time.sleep(0.3)  # 打开串口可能触发复位
                ser.reset_input_buffer()
                ser.write((command + "\n").encode("utf-8"))
                deadline = time.time() + timeout
                while time.time() < deadline:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if line.startswith(("OK:", "ERR:")):
                        print(line)
                        return 0 if line.startswith("OK:") else 1
        except serial.SerialException as e:
            print(f"ERR:SERIAL:{e}", file=sys.stderr)
            return 1
    print("ERR:NO_RESPONSE", file=sys.stderr)
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Send a command to CYD Kimi usage display")
    ap.add_argument("command", help='如 "GET:CONFIG" / "SET:INTERVAL:120" / "REFRESH"')
    ap.add_argument("--port", default="COM7", help="串口，默认 COM7")
    args = ap.parse_args()
    return send_command(args.port, args.command)


if __name__ == "__main__":
    sys.exit(main())
