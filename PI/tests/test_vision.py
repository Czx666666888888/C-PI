"""Run with Pi venv; synthetic geometry is not real-material acceptance."""
import sys
import threading
import unittest
from pathlib import Path
from unittest.mock import patch
import cv2
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import Observation, camera_worker
from vision.detector import detect, select_target


class VisionTests(unittest.TestCase):
    def test_colors_geometry_and_empty(self):
        for name, bgr, color in [('Red', (0, 0, 255), 1), ('Blue', (255, 0, 0), 3), ('Green', (0, 255, 0), 4)]:
            frame = np.zeros((480, 640, 3), dtype=np.uint8)
            cv2.rectangle(frame, (300, 255), (340, 295), bgr, -1)
            before = frame.copy()
            value, _, _ = detect(frame)
            self.assertEqual(value, (color, 1, 30, -15), name)
            self.assertTrue(np.array_equal(frame, before))
        self.assertEqual(detect(np.zeros((480, 640, 3), dtype=np.uint8))[0], (0, 0, 0, 0))
        self.assertEqual(select_target({'Green': (1, 2), 'Red': (3, 4)}), (1, 1, 3, 4))
        self.assertEqual(select_target({'Red': (1, 2)}, 'Yellow'), (0, 0, 0, 0))

    def test_camera_failure_invalidation_and_recovery(self):
        stop = threading.Event()
        observation = Observation()
        observation.set((1, 1, 10, 20), 1)
        released = []
        attempts = []

        class Camera:
            def isOpened(self):
                return True

            def set(self, *args):
                pass

            def release(self):
                released.append(True)

            def read(self):
                if len(attempts) == 1:
                    return False, None
                self_test.assertEqual(observation.value, (0, 0, 0, 0))
                stop.set()
                return True, np.zeros((480, 640, 3), dtype=np.uint8)

        def factory(*args):
            attempts.append(True)
            return Camera()

        self_test = self
        with patch('cv2.VideoCapture', factory):
            camera_worker(dict(camera='fake', target_color=None, log_seconds=5, retry_seconds=.01), observation, stop)
        self.assertEqual(len(attempts), 2)
        self.assertEqual(len(released), 2)
        self.assertEqual(observation.value, (0, 0, 0, 0))


if __name__ == '__main__':
    unittest.main()
