"""Synthetic checks verify rejection/geometry, not real material accuracy."""
import json
import sys
import unittest
from pathlib import Path

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from vision.experiments.frustum_top_center import CONFIG, analyze, candidate, agreed_top


class FrustumExperimentTests(unittest.TestCase):
    def setUp(self):
        self.cfg = json.loads(CONFIG.read_text())

    def test_no_internal_edge_is_invalid(self):
        img = np.zeros((480, 640, 3), np.uint8)
        cv2.rectangle(img, (180, 140), (400, 350), (0, 0, 190), -1)
        result, *_ = analyze(img, self.cfg)
        self.assertIsNotNone(result['BoundingRect'])
        self.assertIsNone(result['A_gradient'])
        self.assertIsNone(result['B_contour'])
        self.assertIsNone(result['TopEllipse'])

    def test_internal_rim_and_offsets(self):
        img = np.zeros((480, 640, 3), np.uint8)
        cv2.fillConvexPoly(img, np.array([[190, 150], [390, 150], [420, 350], [160, 350]]), (0, 0, 100))
        cv2.ellipse(img, (290, 205), (82, 40), 0, 0, 360, (0, 0, 220), -1)
        result, *_ = analyze(img, self.cfg)
        for method in ('A_gradient', 'B_contour'):
            self.assertIsNotNone(result[method], method)
            np.testing.assert_allclose(result[method]['center'], (290, 205), atol=2)
        self.assertGreater(result['BoundingRect'][1] - result['A_gradient']['center'][1], 35)
        self.assertIsNotNone(result['TopEllipse'])

    def test_disagreement_or_single_candidate_is_invalid(self):
        a = {'center': (200, 200), 'residual': .5}
        b = {'center': (220, 200), 'residual': .3}
        self.assertIsNone(agreed_top(a, b, self.cfg))
        self.assertIsNone(agreed_top(a, None, self.cfg))

    def test_wrong_resolution_rejected(self):
        with self.assertRaises(ValueError):
            analyze(np.zeros((240, 320, 3), np.uint8), self.cfg)

    def test_short_arc_rejected_despite_zero_residual(self):
        angles = np.linspace(0, np.pi/4, 100)
        points = np.column_stack((290+80*np.cos(angles), 205+40*np.sin(angles)))
        roi = np.zeros((480, 640), np.uint8)
        roi[140:350, 160:420] = 255
        self.assertIsNone(candidate(points, ((290, 205), (160, 80), 0),
                                    roi, (160, 140, 260, 210), self.cfg))


if __name__ == '__main__':
    unittest.main()
