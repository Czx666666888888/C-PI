import cv2
import time

cap = cv2.VideoCapture('/dev/video2', cv2.CAP_V4L2)

if not cap.isOpened():
    print("无法打开摄像头 /dev/video0")
    exit()

# 设置为商家给的参数
cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'YUYV'))
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

time.sleep(2)

print("摄像头已打开，按 q 退出")

while True:
    ret, frame = cap.read()
    if not ret:
        print("无法读取画面")
        break

    cv2.imshow("USB Camera", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
