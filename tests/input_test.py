import socket
import sys
import time
import subprocess
import os

# QMP Client
class QMPClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.recv() # Read greeting

    def recv(self):
        return self.sock.recv(4096).decode('utf-8')

    def send(self, cmd):
        self.sock.sendall(cmd.encode('utf-8'))
        return self.recv()

    def send_key(self, key):
        print(f"Sending key: {key}")
        cmd = f'{{"execute": "send-key", "arguments": {{ "keys": [ {{"type": "qcode", "data": "{key}"}} ] }} }}\n'
        self.send(cmd)

# Path to QMP socket
QMP_SOCK = "qmp.sock"

# Start QEMU with USB Keyboard Only (Machine pc, i8042=off removes PS/2 controller)
# We assume 'make run-usb' or similar starts QEMU.
# But we need to control QEMU launch to add QMP and flags.
# So we launch QEMU directly here.

CMD = [
    "qemu-system-x86_64",
    "-enable-kvm",
    "-cpu", "host",
    "-m", "256M",
    "-no-reboot",
    "-no-shutdown",
    "-drive", "if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd",
    "-drive", "if=pflash,format=raw,file=build/OVMF_VARS.4m.fd",
    "-drive", "format=raw,file=build/dist/cool-os-debug.img",
    "-device", "qemu-xhci",
    "-device", "usb-kbd",
    "-qmp", f"unix:{QMP_SOCK},server,nowait",
    "-serial", "stdio",
    "-display", "none", # Headless
    "-machine", "pc,i8042=off" # Disable PS/2 hardware
    # Note: i8042=off might not be supported on all QEMU versions. 
    # If it fails, we rely on checking if USB receives events.
]

# Ensure OVMF_VARS exists
if not os.path.exists("build/OVMF_VARS.4m.fd"):
    subprocess.run(["cp", "/usr/share/edk2/x64/OVMF_VARS.4m.fd", "build/OVMF_VARS.4m.fd"])

print("Starting QEMU...")
proc = subprocess.Popen(CMD, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

try:
    # Wait for OS to boot (look for prompt or "Listening!")
    print("Waiting for boot...")
    start_time = time.time()
    usb_ready = False
    
    while time.time() - start_time < 10:
        line = proc.stdout.readline()
        if not line: break
        print(f"OS: {line.strip()}")
        if "XHCI: USB Keyboard Ready" in line:
            usb_ready = True
            break
            
    if not usb_ready:
        print("FAIL: OS did not report USB Keyboard Ready")
        sys.exit(1)
        
    print("PASS: USB Stack Initialized and Ready!")
    sys.exit(0)

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
finally:
    proc.terminate()
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)
