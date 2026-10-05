import cv2
import socket
import numpy as np

ZYNQ_IP      = "10.101.4.161"   # ← change to your MicroZed IP
FRAME_WIDTH  = 320
FRAME_HEIGHT = 240
FRAME_SIZE   = FRAME_WIDTH * FRAME_HEIGHT * 4

# Connect to MicroZed
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((ZYNQ_IP, 5000))
print("Connected to FPGA server")

# Open webcam
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("Cannot open camera")
    exit()

while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, (FRAME_WIDTH, FRAME_HEIGHT))
    rgba_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGBA)

    # 1. Send frame
    s.sendall(rgba_frame.tobytes())

    # 2. Receive processed frame
    data = bytearray()
    while len(data) < FRAME_SIZE:
        packet = s.recv(min(4096, FRAME_SIZE - len(data)))
        if not packet:
            print("Connection closed by server")
            break
        data.extend(packet)

    if len(data) != FRAME_SIZE:
        print(f"Incomplete frame received ({len(data)} bytes)")
        continue

    # 3. Display
    processed = np.frombuffer(data, dtype=np.uint8).reshape((FRAME_HEIGHT, FRAME_WIDTH, 4))
    display = cv2.cvtColor(processed, cv2.COLOR_RGBA2BGR)

    cv2.imshow("Original Webcam", frame)
    cv2.imshow("FPGA Sobel Edge Detection", display)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
s.close()