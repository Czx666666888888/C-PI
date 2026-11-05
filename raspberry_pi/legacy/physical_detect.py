import cv2
import numpy as np
import time

# 定义要追踪的三种物块颜色字典
colors_to_track = {
    "Red": {
        "lower1": np.array([0, 100, 50]), "upper1": np.array([10, 255, 255]),
        "lower2": np.array([170, 100, 50]), "upper2": np.array([180, 255, 255]),
        "bgr": (0, 0, 255)
    },
    "Green": {
        "lower": np.array([25, 80, 100]), "upper": np.array([90, 255, 255]),
        "bgr": (0, 255, 0)
    },
    "Blue": {
        "lower": np.array([100, 80, 80]), "upper": np.array([120, 255, 255]),
        "bgr": (255, 0, 0)
    }
}

def get_color_blocks_positions(img):
    """
    输入一帧图像，返回画面中红、绿、蓝物块的中心坐标（相对于夹爪中心的坐标）。
    返回值示例: {'Red': (150, 120), 'Blue': (200, 90)}
    """
    positions = {} # 用来装最终坐标的"空篮子"

    # 夹爪中心坐标（参考circle_detect.py）
    GRIPPER_CX = 290
    GRIPPER_CY = 260
    
    # 在画面上标记夹爪中心
    cv2.drawMarker(img, (GRIPPER_CX, GRIPPER_CY), (255, 255, 255), markerType=cv2.MARKER_CROSS, markerSize=20, thickness=2)

    # 1. 基础预处理
    b, g, r_channel = cv2.split(img)
    r_channel = cv2.multiply(r_channel, 0.9).astype(np.uint8)
    processed_img = cv2.merge([b, g, r_channel])

    blurred_img = cv2.GaussianBlur(processed_img, (5, 5), 0)
    hsv = cv2.cvtColor(blurred_img, cv2.COLOR_BGR2HSV)

    # 2. 遍历三种颜色，寻找物块
    for color_name, params in colors_to_track.items():
        if color_name == "Red":
            mask1 = cv2.inRange(hsv, params["lower1"], params["upper1"])
            mask2 = cv2.inRange(hsv, params["lower2"], params["upper2"])
            mask = cv2.bitwise_or(mask1, mask2)
        else:
            mask = cv2.inRange(hsv, params["lower"], params["upper"])

        # 形态学清理噪点
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)

        # 3. 寻找该颜色的轮廓
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        if contours:
            # 取该颜色面积最大的轮廓 (认为它是我们要抓的物块，而不是背景里的杂色)
            max_cnt = max(contours, key=cv2.contourArea)

            if cv2.contourArea(max_cnt) > 500: # 面积阈值，过滤小斑点
                x, y, w, h = cv2.boundingRect(max_cnt)
                cx, cy = x + w//2, y + h//2
                
                # 计算相对坐标：相对X = 像素X - 基准X，相对Y = 基准Y - 像素Y
                rel_x = cx - GRIPPER_CX
                rel_y = GRIPPER_CY - cy

                # 【核心】：把计算好的相对坐标存入字典
                positions[color_name] = (rel_x, rel_y)

                # 顺便在画面上画框，方便调试
                draw_color = params["bgr"]
                cv2.rectangle(img, (x, y), (x + w, y + h), draw_color, 2)
                cv2.circle(img, (cx, cy), 5, draw_color, -1)
                cv2.putText(img, f"{color_name} {rel_x},{rel_y}", (x, y - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, draw_color, 2)

    # 返回收集到的坐标字典
    return positions


# ================= 主循环测试 =================
if __name__ == "__main__":
    cap = cv2.VideoCapture('/dev/video0', cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)

    print("开始实时提取三色物块坐标...")

    while True:
        ret, frame = cap.read()
        if not ret: break

        # 1. 调用函数，拿到坐标字典
        block_coords = get_color_blocks_positions(frame)

        # 2. 演示如何提取并使用这些坐标
        if block_coords: # 如果字典不为空（说明至少看到了一个物块）
            print(f"当前找到的物块: {block_coords}")

            # 举个例子：如果你想专门拿到红色物块的坐标发给 STM32
            if "Red" in block_coords:
                rx, ry = block_coords["Red"]
                # 这里可以写串口发送代码，例如：
                # serial_msg = f"R:{rx},{ry}\n"
                # print(f"准备发送给STM32: {serial_msg}")

        cv2.imshow("Block Coordinate Extractor", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()