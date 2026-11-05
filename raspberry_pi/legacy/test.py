import cv2
import numpy as np

# =========================
# 颜色名称判定
# =========================
def hsv_to_color_name(h, s, v):
    # OpenCV里 H: 0~179, S: 0~255, V: 0~255
    if v < 50:
        return "black"
    if s < 40 and v > 180:
        return "white"
    if s < 40:
        return "gray"

    if h < 8 or h >= 170:
        return "red"
    elif h < 20:
        return "orange"
    elif h < 35:
        return "yellow"
    elif h < 85:
        return "green"
    elif h < 105:
        return "cyan"
    elif h < 130:
        return "blue"
    elif h < 150:
        return "purple"
    else:
        return "magenta"


# =========================
# 读取图像
# =========================
import os

# 获取当前脚本所在目录
base_dir = os.path.dirname(os.path.abspath(__file__))
img_path = os.path.join(base_dir, "img.png")

print("正在读取图片：", img_path)

img = cv2.imread(img_path)

if img is None:
    print("读取失败：找不到 img.png")
    exit()

output = img.copy()
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
gray = cv2.GaussianBlur(gray, (9, 9), 2)

# =========================
# 霍夫圆检测
# =========================
circles = cv2.HoughCircles(
    gray,
    cv2.HOUGH_GRADIENT,
    dp=1.2,
    minDist=10,
    param1=100,
    param2=20,
    minRadius=10,
    maxRadius=0
)

if circles is None:
    print("没有检测到圆，请调整参数")
    cv2.imshow("result", output)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    exit()

circles = np.round(circles[0, :]).astype(int)

# =========================
# 收集圆心，估计公共中心
# 用中位数更抗噪声
# =========================
xs = circles[:, 0]
ys = circles[:, 1]
rs = circles[:, 2]

center_x = int(np.median(xs))
center_y = int(np.median(ys))

# 去掉半径太接近的重复圆
radii = sorted(rs.tolist())
filtered_radii = []
min_gap = 8  # 半径差小于这个值就认为是重复检测

for r in radii:
    if len(filtered_radii) == 0 or abs(r - filtered_radii[-1]) > min_gap:
        filtered_radii.append(r)

# 转 HSV 方便判断颜色
hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

# 画出估计圆心
cv2.circle(output, (center_x, center_y), 6, (255, 0, 255), -1)
cv2.putText(
    output,
    f"Center=({center_x},{center_y})",
    (center_x + 10, center_y - 10),
    cv2.FONT_HERSHEY_SIMPLEX,
    0.6,
    (255, 0, 255),
    2
)

print("估计圆心:", (center_x, center_y))
print("检测到的有效半径:", filtered_radii)

# =========================
# 分析每个圆环颜色
# 相邻半径构成一个环带
# =========================
ring_infos = []

for i in range(len(filtered_radii) - 1):
    r_in = filtered_radii[i]
    r_out = filtered_radii[i + 1]

    # 避免环带太窄
    if r_out - r_in < 5:
        continue

    # 生成环形 mask
    mask_outer = np.zeros(gray.shape, dtype=np.uint8)
    mask_inner = np.zeros(gray.shape, dtype=np.uint8)

    cv2.circle(mask_outer, (center_x, center_y), r_out, 255, -1)
    cv2.circle(mask_inner, (center_x, center_y), r_in, 255, -1)

    ring_mask = cv2.subtract(mask_outer, mask_inner)

    # 取环带平均 HSV
    mean_h, mean_s, mean_v, _ = cv2.mean(hsv, mask=ring_mask)
    color_name = hsv_to_color_name(mean_h, mean_s, mean_v)

    ring_infos.append({
        "r_in": r_in,
        "r_out": r_out,
        "r_mid": (r_in + r_out) // 2,
        "hsv": (int(mean_h), int(mean_s), int(mean_v)),
        "color": color_name
    })

# =========================
# 画圆和颜色标签
# =========================
for idx, info in enumerate(ring_infos, start=1):
    r_mid = info["r_mid"]
    color_name = info["color"]
    h_val, s_val, v_val = info["hsv"]

    # 画中间半径作为参考圈
    cv2.circle(output, (center_x, center_y), r_mid, (0, 255, 0), 1)

    # 在圆环上方写颜色
    text_x = center_x + 10
    text_y = center_y - r_mid

    cv2.putText(
        output,
        f"{idx}:{color_name}",
        (text_x, text_y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        2
    )

    print(
        f"环 {idx}: 半径范围=({info['r_in']}, {info['r_out']}), "
        f"HSV={info['hsv']}, 颜色={color_name}"
    )

# =========================
# 同时把原始检测到的圆也画出来
# =========================
for x, y, r in circles:
    cv2.circle(output, (x, y), r, (255, 255, 0), 1)
    cv2.circle(output, (x, y), 1, (0, 0, 255), 2)

# 保存结果
cv2.imwrite("result_marked.png", output)

# 显示结果
cv2.imshow("result", output)
cv2.waitKey(0)
cv2.destroyAllWindows()
