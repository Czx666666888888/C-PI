import cv2
import time

class QRCodeDetector:
    def __init__(self):
        self.detector = cv2.QRCodeDetector()
    
    def detect_qr_code(self, frame):
        """
        识别二维码
        参数: frame - 输入图像帧
        返回: 识别到的二维码内容，未识别到返回None
        """
        if frame is None:
            return None
        
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # 1. 磨皮（高斯滤波）：去掉雪花点，这张图最适合用来识别！
        blur = cv2.GaussianBlur(gray, (5, 5), 0)

        # ========================================================
        # 【核心修复】：识别时，传入 blur (平滑后的灰度图) 而不是 binary
        # ========================================================
        data, points, _ = self.detector.detectAndDecode(blur)

        if points is not None and data:
            # 在图像上绘制二维码边框
            points = points[0].astype(int)
            for i in range(4):
                pt1 = tuple(points[i])
                pt2 = tuple(points[(i + 1) % 4])
                cv2.line(frame, pt1, pt2, (0, 255, 0), 2)
            
            return data
        
        return None

# 测试代码
if __name__ == "__main__":
    dev = "/dev/video2"
    cap = cv2.VideoCapture(dev, cv2.CAP_V4L2)

    if not cap.isOpened():
        print(f"无法打开摄像头 {dev}")
        exit()

    # 摄像头参数
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'YUYV'))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    # 摄像头预热
    time.sleep(2)
    for _ in range(5):
        cap.read()
        time.sleep(0.1)

    qr_detector = QRCodeDetector()
    last_text = ""

    print("开始识别二维码，按 q 退出")

    while True:
        start_time = time.time()
        ret, frame = cap.read()
        if not ret or frame is None:
            continue
            
        data = qr_detector.detect_qr_code(frame)

        if data and data != last_text:
            print("识别到二维码内容：", data)
            last_text = data

        # 【修复】：把 FPS 计算移到 if 外面，确保每一帧都实时刷新
        end_time = time.time()
        seconds = end_time - start_time
        fps = 1 / seconds if seconds > 0 else 0
                
        cv2.putText(frame, f"FPS: {fps:.2f}", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                    
        cv2.imshow("Original", frame)   

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()