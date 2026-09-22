import cv2
import socket
import numpy as np

ZYNQ_IP = "10.101.4.161" # CHANGE THIS TO YOUR MICROZED IP
FRAME_WIDTH = 320
FRAME_HEIGHT = 240
FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 4

# Connect to MicroZed
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((ZYNQ_IP, 5000))

# Open PC Webcam
cap = cv2.VideoCapture(0)

while True:
    ret, frame = cap.read()
    if not ret: break

    frame = cv2.resize(frame, (FRAME_WIDTH, FRAME_HEIGHT))
    rgba_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGBA)

    print("1. Sending frame to FPGA...")
    s.sendall(rgba_frame.tobytes())

    print("2. Waiting for FPGA to finish...")
    data = bytearray()
    while len(data) < FRAME_SIZE:
        packet = s.recv(FRAME_SIZE - len(data))
        if not packet: break
        data.extend(packet)

    print("3. Displaying frame...")
    processed_array = np.frombuffer(data, dtype=np.uint8).reshape((FRAME_HEIGHT, FRAME_WIDTH, 4))
    display_frame = cv2.cvtColor(processed_array, cv2.COLOR_RGBA2BGR)

    cv2.imshow("Original Webcam", frame)
    cv2.imshow("FPGA Sobel Edge Detection", display_frame)
    cv2.waitKey(1)

cap.release()
cv2.destroyAllWindows()