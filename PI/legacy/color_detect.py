import cv2
import numpy as np

# 定义全局变量
selection_roi = None
selecting = False
start_point = (0, 0)
end_point = (0, 0)

# 鼠标回调函数
def select_roi(event, x, y, flags, param):
    global start_point, end_point, selecting, selection_roi
    if event == cv2.EVENT_LBUTTONDOWN:
        selecting = True
        start_point = (x, y)
    elif event == cv2.EVENT_MOUSEMOVE:
        if selecting:
            end_point = (x, y)
    elif event == cv2.EVENT_LBUTTONUP:
        selecting = False
        end_point = (x, y)
        selection_roi = (start_point, end_point)

def get_color_name(h, s, v):
    """根据 HSV 值粗略判断颜色名称"""
    if s < 40: return "White/Grey"
    if v < 40: return "Black"
    
    if h < 10 or h > 156: return "Red"
    if 11 <= h <= 25: return "Orange"
    if 26 <= h <= 34: return "Yellow"
    if 35 <= h <= 77: return "Green"
    if 78 <= h <= 99: return "Cyan"
    if 100 <= h <= 124: return "Blue"
    if 125 <= h <= 155: return "Purple"
    return "Unknown"

# 初始化摄像头
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_AUTO_WB, 1)
cv2.namedWindow("Color Detector")
cv2.setMouseCallback("Color Detector", select_roi)

print("操作说明：用鼠标左键在画面上拖拽框选，按 'q' 退出")

while True:
    ret, frame = cap.read()
    if not ret: break

    # 如果正在框选，画出动态矩形
    if selecting:
        cv2.rectangle(frame, start_point, end_point, (0, 255, 0), 2)
    
    # 如果已经选好了区域，进行处理
    if selection_roi:
        p1, p2 = selection_roi
        # 确保坐标顺序正确
        x1, y1 = min(p1[0], p2[0]), min(p1[1], p2[1])
        x2, y2 = max(p1[0], p2[0]), max(p1[1], p2[1])
        
        roi = frame[y1:y2, x1:x2]
        if roi.size > 0:
            # 1. 转换为 HSV 空间
            hsv_roi = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
            # 2. 计算均值
            avg_hsv = cv2.mean(hsv_roi)[:3]
            h, s, v = avg_hsv
            
            # 3. 识别颜色名
            color_name = get_color_name(h, s, v)
            
            # 4. 显示结果
            cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 0, 0), 2)
            label = f"{color_name} (H:{int(h)} S:{int(s)} V:{int(v)})"
            cv2.putText(frame, label, (x1, y1 - 10), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

    cv2.imshow("Color Detector", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()