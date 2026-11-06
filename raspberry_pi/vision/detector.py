"""Adapter preserving the archived detector and its pixel offsets."""
from legacy.physical_detect import get_color_blocks_positions

COLORS = {'Red': 1, 'Yellow': 2, 'Blue': 3, 'Green': 4, 'Black': 5, 'Light Blue': 6}
GRIPPER_CX, GRIPPER_CY = 290, 260


def select_target(positions, requested=None):
    names = [requested] if requested else list(COLORS)
    for name in names:
        if name in positions:
            dx, dy = positions[name]
            if -32768 <= dx <= 32767 and -32768 <= dy <= 32767:
                return COLORS[name], 1, int(dx), int(dy)
    return 0, 0, 0, 0


def detect(frame, requested=None):
    # Original function draws in place; keep acquisition image untouched.
    annotated = frame.copy()
    positions = get_color_blocks_positions(annotated)
    return select_target(positions, requested), positions, annotated
