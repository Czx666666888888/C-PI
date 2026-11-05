import serial
import time
import cv2
import numpy as np
from circle_detect import color_circle_position
from physical_detect import get_color_blocks_positions
import q2_detect

class SerialCommunication:
    def __init__(self, port, baudrate=9600, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # 等待串口初始化
    
    def close(self):
        if self.ser.is_open:
            self.ser.close()
    
    def send_packet(self, packet):
        """发送数据包"""
        try:
            self.ser.write(packet)
            return True
        except Exception as e:
            print(f"发送数据失败: {e}")
            return False
    
    def receive_packet(self, expected_length):
        """接收数据包"""
        try:
            data = self.ser.read(expected_length)
            return data
        except Exception as e:
            print(f"接收数据失败: {e}")
            return b''
    
    def create_pc2mcu_packet(self, flag_code, data):
        """创建PC2MCU数据包"""
        header = b'\x66'
        tail = b'\x77'
        
        if flag_code == 1:  # 物料盘位置（中心坐标）
            if len(data) != 4:  # x高, x低, y高, y低
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + b'\x00\x00' + tail
        elif flag_code == 2:  # 物料位置
            if len(data) != 5:  # 物料颜色, x低, y高, y低
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + b'\x00' + tail
        elif flag_code == 3:  # 色环位置
            if len(data) != 6:  # 中心x高, 中心x低, 中心y高, 中心y低, z高, z低
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + data[5:6] + tail
        elif flag_code == 4:  # 码垛时识别物料位置
            if len(data) != 4:  # 中心x高, 中心x低, 中心y高, 中心y低
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + b'\x00\x00' + tail
        elif flag_code == 5:  # 决赛物料位置（中心坐标）
            if len(data) != 4:  # x高, x低, y高, y低
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + b'\x00\x00' + tail
        elif flag_code == 6:  # 决赛物料放置指令
            if len(data) != 1:  # 物料颜色
                return None
            packet = header + bytes([flag_code]) + data[0:1] + b'\x00\x00\x00\x00\x00' + tail
        elif flag_code == 7:  # 初赛方案2抓取物料指令
            if len(data) != 1:  # 物料颜色
                return None
            packet = header + bytes([flag_code]) + data[0:1] + b'\x00\x00\x00\x00\x00' + tail
        elif flag_code == 8:  # 决赛抓取物料校准和放置顺序
            if len(data) != 7:  # 中心x高, 中心x低, 中心y高, 中心y低, z低, 物料放置顺序
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + data[5:6] + data[6:7] + tail
        elif flag_code == 9:  # 识别二维码
            if len(data) != 6:  # 顺序1-6
                return None
            packet = header + bytes([flag_code]) + data[0:1] + data[1:2] + data[2:3] + data[3:4] + data[4:5] + data[5:6] + tail
        elif flag_code == 0xAA:  # 调试
            if len(data) != 1:  # 1开灯/2关灯/3就位
                return None
            packet = header + bytes([flag_code]) + data[0:1] + b'\x00\x00\x00\x00\x00' + tail
        else:
            return None
        
        return packet
    
    def parse_mcu2pc_packet(self, packet):
        """解析MCU2PC数据包"""
        if len(packet) < 4:  # 包头+任务码+数据1+校验
            return None
        
        header = packet[0]
        task_code = packet[1]
        data1 = packet[2]
        checksum = packet[3]
        
        if header != 0x66:
            return None
        
        result = {
            'task_code': task_code,
            'data1': data1,
            'checksum': checksum
        }
        
        return result

class QRCodeDetector:
    def __init__(self):
        self.detector = cv2.QRCodeDetector()
    
    def detect_qr_code(self, frame):
        """识别二维码"""
        if frame is None:
            return None
        
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blur = cv2.GaussianBlur(gray, (5, 5), 0)
        data, points, _ = self.detector.detectAndDecode(blur)
        
        if points is not None and data:
            return data
        return None

# 示例使用
def main():
    # 初始化串口
    ser = SerialCommunication('COM1')  # 替换为实际的串口
    qr_detector = QRCodeDetector()
    
    # 初始化摄像头
    cap = cv2.VideoCapture(0)  # 替换为实际的摄像头编号
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            
            # 接收MCU指令
            received_data = ser.receive_packet(4)
            if received_data:
                parsed_data = ser.parse_mcu2pc_packet(received_data)
                if parsed_data:
                    task_code = parsed_data['task_code']
                    print(f"接收到任务码: {task_code}")
                    
                    if task_code == 2:  # 识别物料
                        positions = get_color_blocks_positions(frame)
                        if positions:
                            # 发送第一个找到的物料
                            for color_name, (x, y) in positions.items():
                                if color_name == "Red":
                                    color_code = 1
                                elif color_name == "Green":
                                    color_code = 2
                                elif color_name == "Blue":
                                    color_code = 3
                                else:
                                    continue
                                
                                x_high = (x>>8) & 0xFF
                                x_low = x & 0xFF
                                y_high = (y >> 8) & 0xFF
                                y_low = y & 0xFF
                                data = bytes([color_code, x_high, x_low, y_high, y_low, 0])
                                packet = ser.create_pc2mcu_packet(2, data)
                                if packet:
                                    print(f"发送物料位置数据包: {packet.hex()}")
                                    ser.send_packet(packet)
                                break
                    
                    elif task_code == 3:  # 识别色环位置校准
                        coords = color_circle_position(frame)
                        if coords:
                            # 使用中间圆的坐标
                            x = coords[2]
                            y = coords[3]
                            # 计算z值（左减右）
                            z = coords[0] - coords[4]  # 左侧x坐标减右侧x坐标
                            
                            x_high = (x >> 8) & 0xFF
                            x_low = x & 0xFF
                            y_high = (y >> 8) & 0xFF
                            y_low = y & 0xFF
                            z_high = (abs(z) >> 8) & 0xFF
                            z_low = abs(z) & 0xFF
                            if z < 0:
                                z_low |= 0x80  # 符号位
                            
                            data = bytes([x_high, x_low, y_high, y_low, z_high, z_low])
                            packet = ser.create_pc2mcu_packet(3, data)
                            if packet:
                                print(f"发送色环位置数据包: {packet.hex()}")
                                ser.send_packet(packet)
                    
                    elif task_code == 9:  # 识别二维码
                        qr_data = qr_detector.detect_qr_code(frame)
                        if qr_data:
                            print(f"识别到二维码: {qr_data}")
                            # 假设二维码内容是6位数字
                            if len(qr_data) == 6 and qr_data.isdigit():
                                data = bytes([int(qr_data[0]), int(qr_data[1]), int(qr_data[2]), 
                                             int(qr_data[3]), int(qr_data[4]), int(qr_data[5])])
                                packet = ser.create_pc2mcu_packet(9, data)
                                if packet:
                                    print(f"发送二维码数据数据包: {packet.hex()}")
                                    ser.send_packet(packet)
            
            # 显示画面
            cv2.imshow("Main", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
                
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        cap.release()
        cv2.destroyAllWindows()
        ser.close()

if __name__ == "__main__":
    main()