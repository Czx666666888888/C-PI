import serial
import time
import cv2
from circle_detect import color_circle_position
from physical_detect import get_color_blocks_positions
from q2_detect import QRCodeDetector

class SerialCommunication:
    def __init__(self, port, baudrate=9600, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)

    def close(self):
        if self.ser.is_open:
            self.ser.close()

    def send_packet(self, packet):
        try:
            self.ser.write(packet)
            return True
        except Exception as e:
            print(f"发送数据失败: {e}")
            return False

    def receive_packet(self, expected_length):
        try:
            data = self.ser.read(expected_length)
            return data
        except Exception as e:
            print(f"接收数据失败: {e}")
            return b''

    def create_pc2mcu_packet(self, flag_code, data):
        header = b'\x66'
        tail = b'\x77'

        if flag_code == 2:
            if len(data) != 5:
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + b'\x00' + tail
        elif flag_code == 3:
            if len(data) != 6:
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + data[5:6] + tail
        elif flag_code == 9:
            if len(data) != 6:
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + data[5:6] + tail
        else:
            return None

        return packet

    def parse_mcu2pc_packet(self, packet):
        if len(packet) < 4:
            return None

        header = packet[0]
        task_code = packet[1]
        data1 = packet[2]
        checksum = packet[3]

        if header != 0x66:
            return None

        return {
            'task_code': task_code,
            'data1': data1,
            'checksum': checksum
        }

class ColorSequenceTester:
    COLOR_MAP = {
        1: "Red",
        2: "Green",
        3: "Blue"
    }

    def __init__(self, port='COM1', qr_camera=0, material_camera=1):
        self.ser = SerialCommunication(port)
        self.qr_detector = QRCodeDetector()
        self.qr_camera = qr_camera
        self.material_camera = material_camera
        self.qr_sequence = []
        self.qr_cap = None
        self.material_cap = None

    def init_qr_camera(self):
        print(f"正在初始化二维码摄像头 {self.qr_camera}...")
        try:
            if self.qr_cap:
                self.qr_cap.release()
            time.sleep(1)

            self.qr_cap = cv2.VideoCapture(self.qr_camera, cv2.CAP_V4L2)
            if not self.qr_cap.isOpened():
                print(f"无法打开二维码摄像头 {self.qr_camera}")
                return False

            self.qr_cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            self.qr_cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            self.qr_cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

            for i in range(5):
                ret, frame = self.qr_cap.read()
                if ret:
                    print(f"二维码摄像头 {self.qr_camera} 初始化成功")
                    return True
                time.sleep(0.5)

            print(f"二维码摄像头 {self.qr_camera} 读取失败")
            return False
        except Exception as e:
            print(f"二维码摄像头初始化错误: {e}")
            return False

    def init_material_camera(self):
        print(f"正在初始化物料摄像头 {self.material_camera}...")
        try:
            if self.material_cap:
                self.material_cap.release()
            time.sleep(1)

            self.material_cap = cv2.VideoCapture(self.material_camera, cv2.CAP_V4L2)
            if not self.material_cap.isOpened():
                print(f"无法打开物料摄像头 {self.material_camera}")
                return False

            self.material_cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
            self.material_cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
            self.material_cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

            for i in range(5):
                ret, frame = self.material_cap.read()
                if ret:
                    print(f"物料摄像头 {self.material_camera} 初始化成功")
                    return True
                time.sleep(0.5)

            print(f"物料摄像头 {self.material_camera} 读取失败")
            return False
        except Exception as e:
            print(f"物料摄像头初始化错误: {e}")
            return False

    def detect_qr_and_get_sequence(self):
        print("=" * 50)
        print("步骤1: 识别二维码，获取颜色顺序")
        print("=" * 50)

        if not self.init_qr_camera():
            return False

        while len(self.qr_sequence) < 3:
            ret, frame = self.qr_cap.read()
            if not ret:
                print("二维码摄像头读取失败")
                time.sleep(0.5)
                continue

            cv2.imshow("QR Detection", frame)
            qr_data = self.qr_detector.detect_qr_code(frame)

            if qr_data and len(qr_data) >= 3:
                self.qr_sequence = [int(qr_data[0]), int(qr_data[1]), int(qr_data[2])]
                print(f"二维码内容: {qr_data}")
                print(f"颜色顺序: {self.qr_sequence}")
                print(f"颜色映射: ", end="")
                for i, code in enumerate(self.qr_sequence):
                    color_name = self.COLOR_MAP.get(code, "Unknown")
                    print(f"{i+1}. {color_name}({code}) ", end="")
                print()
                time.sleep(1)
                break

            if cv2.waitKey(1) & 0xFF == ord('q'):
                print("用户退出")
                return False

        cv2.destroyWindow("QR Detection")
        self.qr_cap.release()
        self.qr_cap = None
        time.sleep(2)

        print("二维码识别完成，准备切换到物料摄像头")
        return len(self.qr_sequence) >= 3

    def detect_material_by_color(self, target_color_code):
        print("-" * 50)
        print(f"识别颜色: {self.COLOR_MAP.get(target_color_code, 'Unknown')}({target_color_code})")
        print("开始自动识别...")

        stable_count = 0
        position_history = []
        stability_threshold = 10
        found_color = False

        while True:
            ret, frame = self.material_cap.read()
            if not ret:
                print("摄像头读取失败，重试...")
                time.sleep(0.5)
                continue

            cv2.imshow("Material Detection", frame)
            cv2.waitKey(1)

            positions = get_color_blocks_positions(frame)
            target_color_name = self.COLOR_MAP.get(target_color_code)

            if positions and target_color_name in positions:
                x, y = positions[target_color_name]
                position_history.append((x, y))

                if len(position_history) >= 3:
                    recent_x = [p[0] for p in position_history[-3:]]
                    recent_y = [p[1] for p in position_history[-3:]]

                    x_range = max(recent_x) - min(recent_x)
                    y_range = max(recent_y) - min(recent_y)

                    if x_range < stability_threshold and y_range < stability_threshold:
                        stable_count += 1
                        if not found_color:
                            print("找到颜色，正在检测稳定性...")
                            found_color = True
                        print(f"稳定性检测: {stable_count}/3")
                        if stable_count >= 3:
                            print("颜色位置稳定!")
                            self.frame = frame
                            break
            else:
                if found_color:
                    print("颜色消失，重新寻找...")
                found_color = False
                stable_count = 0
                position_history = []

            time.sleep(0.1)

        positions = get_color_blocks_positions(self.frame)
        if not positions:
            print(f"未找到任何物料")
            return None

        target_color_name = self.COLOR_MAP.get(target_color_code)
        if target_color_name not in positions:
            print(f"未找到目标颜色: {target_color_name}")
            return None

        x, y = positions[target_color_name]
        print(f"找到 {target_color_name} 物料，坐标: ({x}, {y})")

        x_low = x & 0xFF
        y_high = (y >> 8) & 0xFF
        y_low = y & 0xFF
        data = bytes([target_color_code, x_low, y_high, y_low, 0])

        packet = self.ser.create_pc2mcu_packet(2, data)
        if packet:
            print(f"发送数据包: {packet.hex()}")
            self.ser.send_packet(packet)
            
            # 识别结束后等待选择
            print("\n识别完成! 等待指令...")
            print("按 1: 识别下一个颜色")
            print("按 2: 重新识别当前颜色")
            
            # 确保窗口在前台
            cv2.setWindowProperty("Material Detection", cv2.WND_PROP_TOPMOST, 1)
            
            start_time = time.time()
            while True:
                # 检查串口输入
                if self.ser.ser.in_waiting > 0:
                    try:
                        cmd = self.ser.ser.read(1)
                        if cmd == b'1':
                            print("收到指令1，准备识别下一个颜色")
                            cv2.destroyWindow("Material Detection")
                            return True
                        elif cmd == b'2':
                            print("收到指令2，重新识别当前颜色")
                            cv2.destroyWindow("Material Detection")
                            return 'retry'
                    except:
                        pass
                
                # 检查键盘输入 - 增加检测频率
                key = cv2.waitKey(10) & 0xFF
                if key != 255:  # 有按键输入
                    print(f"检测到按键: {key} (字符: {chr(key) if key < 128 else '特殊键'}")
                    if key == ord('1'):
                        print("按下1键，准备识别下一个颜色")
                        cv2.destroyWindow("Material Detection")
                        return True
                    elif key == ord('2'):
                        print("按下2键，重新识别当前颜色")
                        cv2.destroyWindow("Material Detection")
                        return 'retry'
                
                # 显示当前状态
                if time.time() - start_time > 1:
                    start_time = time.time()
                    ret, frame = self.material_cap.read()
                    if ret:
                        cv2.imshow("Material Detection", frame)
                        cv2.waitKey(1)
                
                time.sleep(0.05)

        return False

    def detect_color_circle(self):
        """
        识别色环位置，确认稳定后发送数据
        """
        print("-" * 50)
        print("任务3: 识别色环位置")
        print("开始自动识别...")

        stable_count = 0
        position_history = []
        stability_threshold = 5  # 位置变化阈值

        while True:
            ret, frame = self.material_cap.read()
            if not ret:
                print("摄像头读取失败，重试...")
                time.sleep(0.5)
                continue

            cv2.imshow("Color Circle Detection", frame)
            cv2.waitKey(1)

            # 调用色环识别函数
            coords = color_circle_position(frame)

            if coords:
                x, y, z = coords
                position_history.append((x, y))

                # 检查稳定性
                if len(position_history) >= 3:
                    recent_x = [p[0] for p in position_history[-3:]]
                    recent_y = [p[1] for p in position_history[-3:]]

                    x_range = max(recent_x) - min(recent_x)
                    y_range = max(recent_y) - min(recent_y)

                    if x_range < stability_threshold and y_range < stability_threshold:
                        stable_count += 1
                        print(f"稳定性检测: {stable_count}/3")
                        if stable_count >= 3:
                            print("色环位置稳定!")
                            break
            else:
                # 未找到色环，重置计数
                stable_count = 0
                position_history = []

            time.sleep(0.1)

        # 构建数据包
        x, y, z = coords
        x_high = (x >> 8) & 0xFF
        x_low = x & 0xFF
        y_high = (y >> 8) & 0xFF
        y_low = y & 0xFF
        z_high = (abs(z) >> 8) & 0xFF
        z_low = abs(z) & 0xFF
        if z < 0:
            z_low |= 0x80  # 设置符号位

        data = bytes([x_high, x_low, y_high, y_low, z_high, z_low])
        packet = self.ser.create_pc2mcu_packet(3, data)

        if packet:
            print(f"发送色环位置数据包: {packet.hex()}")
            self.ser.send_packet(packet)
            
            # 等待用户确认
            print("\n色环识别完成! 按 Enter 键继续...")
            input()
            cv2.destroyWindow("Color Circle Detection")
            return True

        return False

    def run(self):
        print("=" * 50)
        print("颜色顺序物料识别测试流程")
        print("=" * 50)

        if not self.detect_qr_and_get_sequence():
            print("二维码识别失败，退出测试")
            return False

        print("\n" + "=" * 50)
        print("步骤2: 初始化物料摄像头")
        print("=" * 50)

        if not self.init_material_camera():
            print("物料摄像头初始化失败，退出测试")
            return False

        print("\n" + "=" * 50)
        print("步骤3: 按颜色顺序识别物料")
        print("=" * 50)

        i = 0
        while i < len(self.qr_sequence):
            color_code = self.qr_sequence[i]
            print(f"\n第 {i+1} 次物料识别")

            result = self.detect_material_by_color(color_code)
            if result is None:
                print(f"未能识别颜色代码 {color_code} 的物料")
                i += 1
            elif result == 'retry':
                # 重新识别当前颜色
                continue
            else:
                # 识别成功，进入下一个颜色
                i += 1

        # 任务3: 识别色环位置
        print("\n" + "=" * 50)
        print("任务3: 识别色环位置")
        print("=" * 50)
        self.detect_color_circle()

        print("\n" + "=" * 50)
        print("测试完成!")
        print("=" * 50)

        return True

    def cleanup(self):
        if self.qr_cap:
            self.qr_cap.release()
        if self.material_cap:
            self.material_cap.release()
        cv2.destroyAllWindows()
        self.ser.close()

def main():
    # 使用 /dev/ttyS0 作为串口
    tester = ColorSequenceTester('/dev/ttyS0', qr_camera=2, material_camera=0)

    try:
        tester.run()
    except Exception as e:
        print(f"发生错误: {e}")
        import traceback
        traceback.print_exc()
    finally:
        tester.cleanup()

if __name__ == "__main__":
    main()