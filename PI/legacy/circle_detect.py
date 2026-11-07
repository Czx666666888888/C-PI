import cv2
import numpy as np
import time

def color_circle_position(img):
    """
    色环识别函数 (带坐标系平移)
    返回相对于夹爪中心 (290, 260) 的坐标
    """
    # 1. 双边滤波 - 保留边缘的同时平滑图像
    bilateral = cv2.bilateralFilter(img, 9, 75, 75)

    # 2. 转换为灰度
    gray = cv2.cvtColor(bilateral, cv2.COLOR_BGR2GRAY)

    # 3. 自适应直方图均衡化 - 增强对比度
    clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8, 8))
    enhanced = clahe.apply(gray)

    # 4. 高斯模糊 - 减少噪声
    blurred = cv2.GaussianBlur(enhanced, (5, 5), 0)

    # 5. 多种阈值方法结合
    # 方法1: Otsu's 二值化
    _, thresh1 = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    # 方法2: 自适应阈值
    thresh2 = cv2.adaptiveThreshold(blurred, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                     cv2.THRESH_BINARY, 11, 2)

    # 合并两种阈值结果
    thresh = cv2.bitwise_or(thresh1, thresh2)

    # 6. 形态学操作 - 增强圆形
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    morph = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
    morph = cv2.morphologyEx(morph, cv2.MORPH_CLOSE, kernel)

    # 7. 边缘检测 - Canny
    edges = cv2.Canny(morph, 50, 150, apertureSize=3)

    # 8. 轮廓检测 - 找圆形轮廓
    contours, hierarchy = cv2.findContours(edges, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    # 筛选近似圆形的轮廓
    circles = []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        perimeter = cv2.arcLength(cnt, True)
        if perimeter == 0:
            continue
        circularity = 4 * np.pi * area / (perimeter * perimeter)

        # 圆形度检查 (接近1为圆形)
        if 0.5 < circularity < 1.2:
            # 计算最小外接圆
            (x, y), radius = cv2.minEnclosingCircle(cnt)
            # 过滤太小的圆
            if radius > 5:
                circles.append((int(x), int(y), int(radius)))

    # 夹爪中心坐标
    GRIPPER_CX = 290
    GRIPPER_CY = 260

    # 在画面上标记夹爪中心
    cv2.drawMarker(img, (GRIPPER_CX, GRIPPER_CY), (255, 255, 255),
                   markerType=cv2.MARKER_CROSS, markerSize=20, thickness=2)

    # 显示调试图像
    cv2.imshow("Enhanced", enhanced)
    cv2.imshow("Threshold", thresh)
    cv2.imshow("Edges", edges)

    try:
        if len(circles) >= 2:
            # 按半径排序（从大到小）
            circles_sorted = sorted(circles, key=lambda x: x[2], reverse=True)

            # 取前3个最大的圆
            selected_circles = circles_sorted[:3]

            # 过滤掉圆心距离太远的圆
            if len(selected_circles) >= 2:
                mean_x = np.mean([c[0] for c in selected_circles])
                mean_y = np.mean([c[1] for c in selected_circles])

                filtered_circles = []
                for c in selected_circles:
                    distance = np.sqrt((c[0] - mean_x)**2 + (c[1] - mean_y)**2)
                    if distance < 20:  # 距离阈值
                        filtered_circles.append(c)

                selected_circles = filtered_circles

            if len(selected_circles) >= 2:
                for circle in selected_circles:
                    # 画圆轮廓
                    cv2.circle(img, (circle[0], circle[1]), circle[2], (0, 0, 255), 2)
                    # 画圆心
                    cv2.circle(img, (circle[0], circle[1]), 2, (255, 0, 0), 2)

                # 计算圆心的平均值作为色环的中心
                center_x = int(np.mean([c[0] for c in selected_circles]))
                center_y = int(np.mean([c[1] for c in selected_circles]))

                # 标记中心
                cv2.circle(img, (center_x, center_y), 5, (0, 255, 0), -1)

                # 坐标系转换：相对X = 像素X - 基准X，相对Y = 基准Y - 像素Y
                rel_x = center_x - GRIPPER_CX
                rel_y = GRIPPER_CY - center_y

                # 返回转换后的相对坐标
                return (rel_x, rel_y, 0)

    except Exception as e:
        print(f"识别错误: {e}")

    return None


# ================= 主循环 =================
if __name__ == "__main__":
    cap = cv2.VideoCapture('/dev/video0', cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    print("启动测试：请在镜头前放置色环...")
    print("夹爪中心坐标: (290, 260)")
    print("按 'q' 退出")
    print("观察 Enhanced、Threshold、Edges 窗口了解处理过程")

    while True:
        start_time = time.time()
        ret, frame = cap.read()
        if not ret:
            print("摄像头读取失败")
            break

        coords = color_circle_position(frame)

        if coords:
            # 显示相对坐标
            cv2.putText(frame, f"Center: {coords[0]},{coords[1]}", (10, 50),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
            print(f"色环中心坐标: ({coords[0]},{coords[1]})")
        else:
            cv2.putText(frame, "No color circle detected", (10, 50),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

        fps = 1 / (time.time() - start_time)
        cv2.putText(frame, f"FPS: {fps:.1f}", (10, 20),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.imshow("Color Circle Detection", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()