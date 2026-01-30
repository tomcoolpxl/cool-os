import socket
import sys
import time
import subprocess
import os
import json

# QMP Client
class QMPClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.file = self.sock.makefile('r')
        # QMP handshake
        self.recv() 
        self.send_cmd("qmp_capabilities")

    def recv(self):
        while True:
            line = self.file.readline()
            if not line: return None
            data = json.loads(line)
            if 'return' in data or 'QMP' in data:
                return data
            # Ignore events

    def send_cmd(self, cmd, args=None):
        payload = {"execute": cmd}
        if args:
            payload["arguments"] = args
        self.sock.sendall(json.dumps(payload).encode('utf-8'))
        return self.recv()

    def send_key(self, key):
        print(f"TEST: Sending key '{key}' via QMP...")
        # Send press
        self.send_cmd("send-key", {"keys": [{"type": "qcode", "data": key}]})

# Configuration
QMP_SOCK = "qmp.sock"
BUILD_DIR = "build"
DIST_DIR = f"{BUILD_DIR}/dist"
IMG = f"{DIST_DIR}/cool-os-debug.img"

# Find OVMF
ovmf_paths = [
    os.environ.get("OVMF_CODE"),
    "/usr/share/edk2/x64/OVMF_CODE.4m.fd",
    "/usr/share/OVMF/OVMF_CODE.fd",
    "/usr/share/qemu/OVMF.fd"
]
OVMF_CODE = next((p for p in ovmf_paths if p and os.path.exists(p)), None)

if not OVMF_CODE:
    print("Error: OVMF firmware not found.")
    sys.exit(1)

# Ensure OVMF_VARS exists
OVMF_VARS = f"{BUILD_DIR}/OVMF_VARS.4m.fd"
if not os.path.exists(OVMF_VARS):
    # Try to find a source template
    vars_paths = [
        "/usr/share/edk2/x64/OVMF_VARS.4m.fd",
        "/usr/share/OVMF/OVMF_VARS.fd"
    ]
    vars_src = next((p for p in vars_paths if os.path.exists(p)), None)
    if vars_src:
        subprocess.run(["cp", vars_src, OVMF_VARS])
    else:
        # Create empty if needed (might fail boot, but better than nothing)
        subprocess.run(["touch", OVMF_VARS])

# Clean up old socket
if os.path.exists(QMP_SOCK):
    os.remove(QMP_SOCK)

CMD = [
    "qemu-system-x86_64",
    "-enable-kvm",
    "-cpu", "host",
    "-m", "256M",
    "-no-reboot",
    "-no-shutdown",
    "-drive", f"if=pflash,format=raw,readonly=on,file={OVMF_CODE}",
    "-drive", f"if=pflash,format=raw,file={OVMF_VARS}",
    "-drive", f"format=raw,file={IMG}",
    # Note: We rely on the standard PS/2 keyboard provided by the 'pc' machine.
    # This validates the kbd.c driver. Real USB keyboards with Legacy Emulation
    # appear as PS/2 devices to the OS, so this test covers that path.
    "-qmp", f"unix:{QMP_SOCK},server,nowait",
    "-serial", "stdio",
    "-display", "none",
    "-machine", "pc" # Keep PS/2 enabled to test Handoff correctly (Handoff should silence PS/2 emulation)
]

print(f"TEST: Starting QEMU...", flush=True)
proc = subprocess.Popen(CMD, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
os.set_blocking(proc.stdout.fileno(), False)
qmp = None

try:
    start_time = time.time()
    usb_ready = False
    key_received = False
    
    # Non-blocking read loop
    while time.time() - start_time < 30: # 30s Timeout
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None: break
            time.sleep(0.1) # Sleep briefly to avoid CPU spin
            continue
            
        print(f"OS: {line.strip()}", flush=True)
        
        if "cool-os: entering idle loop" in line:
            print("TEST: OS Idle (Interrupts Enabled). Connecting QMP...", flush=True)
            time.sleep(1) # Give QEMU a moment
            try:
                qmp = QMPClient(QMP_SOCK)
                qmp.send_key("q")
            except Exception as e:
                print(f"TEST ERROR: Failed to connect/send QMP: {e}", flush=True)
                break

        # Look for the debug message from kbd.c
        if "KBD: Scancode:" in line:
            print("TEST: PASS! Received key press via PS/2.", flush=True)
            key_received = True
            break

    if not key_received:
        print("TEST: FAIL - OS did not receive key press.", flush=True)
        sys.exit(1)

    sys.exit(0)

except KeyboardInterrupt:
    print("\nTEST: Aborted.")
finally:
    if proc.poll() is None:
        proc.terminate()
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)
