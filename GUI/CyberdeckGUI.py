import sys
import math
import serial
from PyQt6.QtWidgets import QApplication, QWidget
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QPainter, QColor
from PyQt6.QtCore import QPropertyAnimation

SERIAL_PORT = "/dev/ttyACM0"
BAUD = 115200


j1_button_down = False
cap_status = 0
letters_per_block = [ # BUTTON UP
                     ['w', ':', '.', '\"', 'q', '\'', ',', ';'], # UP
                     ['o', 'y', 'e', 'n', 'a', 't', 'i', 'u'],  # DOWN
                     ['k', 'c', 'l', 'r', 'j', 's', 'h', 'd'],  # RIGHT
                     ['f', 'v', 'p', 'x', 'g', 'm', 'z', 'b'],  # LEFT
                      # CAPSLOCK
                     ['w', ';', '.', '|', 'q', '\\', '*', ','], # UP
                     ['O', 'Y', 'E', 'N', 'A', 'T', 'I', 'U'],  # down
                     ['K', 'C', 'L', 'R', 'J', 'S', 'H', 'D'],  # right
                     ['F', 'V', 'P', 'X', 'G', 'M', 'Z', 'B'],  # left
                      # BUTTON DOWN
                     ['=', '+', '9', '-', '*', '/', '8', '%'],  # UP
                     ['UP', 'PGUP', 'RHT', 'PGDN', 'DWN', 'HOME', 'LFT', 'END'],  # DOWN
                     ['>', '}', ')', ']', '<', '[', '(', '{'],  # RIGHT
                     ['0', '1', '2', '3', '4', '5', '6', '7']   # LEFT
                    ]

class RadialOverlay(QWidget):
    def __init__(self):
        super().__init__()

        self.visible_overlay = False
        self.setWindowOpacity(0.0)
        self.anim = QPropertyAnimation(self, b"windowOpacity")
        self.anim.setDuration(120)

        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )

        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.resize(300, 300)

        self.move_to_bottom_center()

    def show_overlay(self):
        self.visible_overlay = True
        self.anim.stop()
        self.anim.setStartValue(self.windowOpacity())
        self.anim.setEndValue(1.0)
        self.anim.start()

    def hide_overlay(self):
        self.anim.stop()
        self.anim.setStartValue(self.windowOpacity())
        self.anim.setEndValue(0.0)
        self.anim.start()
        self.anim.finished.connect(self._on_hidden)

    def _on_hidden(self):
        if self.windowOpacity() == 0.0:
            self.visible_overlay = False


    def move_to_bottom_center(self):
        screen = QApplication.primaryScreen().geometry()
        x = ((screen.width() - self.width()) // 2) + 50
        y = screen.height() - self.height() + 20
        self.move(int(x), int(y))

    def paintEvent(self, event):
        if not self.visible_overlay:
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w, h = 220, 220
        cx, cy = w // 2, h // 2
        radius = 85

        painter.setBrush(QColor(0, 0, 0, 160))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(0, 0, w, h)

        labels = letters_per_block[current_block]

        for i in range(8):
            angle = (math.pi / 4) * i - math.pi / 2
            x = cx + math.cos(angle) * radius
            y = cy + math.sin(angle) * radius

            painter.setPen(QColor(255, 255, 255))
            font = painter.font()
            font.setPointSize(14)
            font.setBold(True)
            painter.setFont(font)

            painter.drawText(int(x - 10), int(y + 5), labels[i])


app = QApplication(sys.argv)
overlay = RadialOverlay()
overlay.show()

# ---- Serial listener ----

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=0)

def poll_serial():
    global current_block
    global cap_status

    while ser.in_waiting:
        line = ser.readline().decode(errors="ignore").strip()

        if not line:
            continue

        if "J1:" in line:
            if "CAPS ON" in line:
                cap_status = 1
            else:
                cap_status = 0

            if "CENTER" in line:
                #overlay.visible_overlay = False
                overlay.hide_overlay()
            else:
                #overlay.visible_overlay = True
                overlay.show_overlay()

            if "BUTTON" not in line and not cap_status:
                if "UP" in line:
                    current_block = 0
                elif "DOWN" in line:
                    current_block = 1
                elif "RIGHT" in line:
                    current_block = 2
                elif "LEFT" in line:
                    current_block = 3
            elif cap_status == 1:
                if "UP" in line:
                    current_block = 4
                elif "DOWN" in line:
                    current_block = 5
                elif "RIGHT" in line:
                    current_block = 6
                elif "LEFT" in line:
                    current_block = 7
            else:
                if "UP" in line:
                    current_block = 8
                elif "DOWN" in line:
                    current_block = 9
                elif "RIGHT" in line:
                    current_block = 10
                elif "LEFT" in line:
                    current_block = 11


            overlay.update()


timer = QTimer()
timer.timeout.connect(poll_serial)
timer.start(10)

sys.exit(app.exec())
