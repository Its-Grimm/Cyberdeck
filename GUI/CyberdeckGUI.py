import sys
import math
import serial
from PyQt6.QtGui import QFont
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtCore import QEasingCurve
from PyQt6.QtGui import QPainter, QColor
from PyQt6.QtCore import QPropertyAnimation
from PyQt6.QtWidgets import QApplication, QWidget

SERIAL_PORT = "/dev/ttyACM0"
BAUD = 115200

j1_button_down = False
cap_status = 0
radial_menu_colour = QColor(0, 0, 0, 160)
pending_modifier = ""
letters_per_block = [ # BUTTON UP
                     ['w', ':', '.', '\"', 'q', '\'', ',', ';'], # UP
                     ['o', 'y', 'e', 'n', 'a', 't', 'i', 'u'],   # DOWN
                     ['k', 'c', 'l', 'r', 'j', 's', 'h', 'd'],   # RIGHT
                     ['f', 'v', 'p', 'x', 'g', 'm', 'z', 'b'],   # LEFT
                      # CAPSLOCK
                     ['W', ':', '.', '\"', 'Q', '\'', ',', ';'], # UP
                     ['O', 'Y', 'E', 'N', 'A', 'T', 'I', 'U'],   # down
                     ['K', 'C', 'L', 'R', 'J', 'S', 'H', 'D'],   # right
                     ['F', 'V', 'P', 'X', 'G', 'M', 'Z', 'B'],   # left
                      # BUTTON DOWN
                     ['=', '+', '9', '-', '*', '/', '8', '%'],   # UP
                     ['UP', 'PUP', 'RHT', 'PDN', 'DWN', 'HME', 'LFT', 'END'],  # DOWN
                     ['>', '}', ')', ']', '<', '[', '(', '{'],   # RIGHT
                     ['0', '1', '2', '3', '4', '5', '6', '7'],   # LEFT
                     # J1 BUTTON UP
                     ['BCK', '?', 'ENT', 'ESC', 'SPC', '$', 'TAB', '!'],
                     # J1 BUTTON UP + CAPSLOCK
                     ['CBCK', 'SHFT', 'CTRL', '\\', 'META', '|', 'ALT', '!']
                    ]

class RadialOverlay(QWidget):
    def __init__(self):
        super().__init__()

        self.visible_overlay = False
        self.setWindowOpacity(0.0)
        self.anim = QPropertyAnimation(self, b"windowOpacity")
        self.anim.setDuration(120)

        self.dial_visible  = False
        self.dial_mode = "SCROLL"
        
        self.dial_timer = QTimer()
        self.dial_timer.setSingleShot(True)
        self.dial_timer.timeout.connect(self.hide_dial_overlay)


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
            self.dial_visible = False


    def move_to_bottom_center(self):
        screen = QApplication.primaryScreen().geometry()
        x = ((screen.width() - self.width()) // 2) + 50
        y = screen.height() - self.height() + 20
        self.move(int(x), int(y))

    def paintEvent(self, event):
        if not self.visible_overlay and not self.dial_visible:
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w, h = 220, 220
        cx, cy = w // 2, h // 2
        radius = 85

        painter.setBrush(radial_menu_colour) # Sets radial menu colour
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

        if self.dial_visible:
            mode_text = ""

            if self.dial_mode == "SCROLL":
                mode_text = "SCR"
            elif self.dial_mode == "VOLUME":
                mode_text = "VOL"
            elif self.dial_mode == "BRIGHTNESS":
                mode_text = "BRT"

            center_radius = 38

            painter.setBrush(QColor(20, 20, 20, 240))
            painter.setPen(Qt.PenStyle.NoPen)

            painter.drawEllipse(
                cx - center_radius,
                cy - center_radius,
                center_radius * 2,
                center_radius * 2
            )

            painter.setPen(QColor(255, 255, 255))

            font = QFont()
            font.setPointSize(12)
            font.setBold(True)

            painter.setFont(font)

            text_rect_size = 80

            painter.drawText(
                cx - text_rect_size // 2,
                cy - 15,
                text_rect_size,
                30,
                Qt.AlignmentFlag.AlignCenter,
                mode_text
            ) 

    def show_dial_overlay(self, mode):
        self.dial_mode = mode
        self.dial_visible = True

        self.anim.stop()
        self.setWindowOpacity(1.0)

        self.dial_timer.start(1000)
        self.update()

    def hide_dial_overlay(self):
        self.anim.stop()
        self.anim.setStartValue(self.windowOpacity())
        self.anim.setEndValue(0.0)
        self.anim.start()


app = QApplication(sys.argv)
overlay = RadialOverlay()
overlay.show()

# ---- Serial listener ----

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=0)

def poll_serial():
    global current_block
    global pending_modifier
    global radial_menu_colour

    while ser.in_waiting:
        line = ser.readline().decode(errors="ignore").strip()

        if not line:
            continue

        if "J1:" in line:
            if "CENTER" in line:
                overlay.hide_overlay()
            else:
                overlay.show_overlay()

            if "BUTTON" not in line:
                if "CAPS" not in line:
                    if "UP" in line:
                        current_block = 0
                    elif "DOWN" in line:
                        current_block = 1
                    elif "RIGHT" in line:
                        current_block = 2
                    elif "LEFT" in line:
                        current_block = 3
                elif "CAPS" in line:
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

        if "DIAL:" in line:
            if "SCROLL" in line:
                overlay.show_dial_overlay("SCROLL")
            elif "VOLUME" in line:
                overlay.show_dial_overlay("VOLUME")
            elif "BRIGHTNESS" in line:
                overlay.show_dial_overlay("BRIGHTNESS")

            current_block = 12                
            overlay.update()

        if "MOD:" in line:
            if "SHIFT" in line:
                pending_modifier = "SHIFT"
                radial_menu_colour = QColor(50, 0, 0, 160)
            elif "CTRL" in line:
                pending_modifier = "CTRL"
                radial_menu_colour = QColor(0, 0, 50, 160)
            elif "ALT" in line:
                pending_modifier = "ALT"
                radial_menu_colour = QColor(0, 50, 0, 160)
            elif "META" in line:
                pending_modifier = "META"
                radial_menu_colour = QColor(50, 0, 50, 160)
            elif "CLEAR" in line:
                pending_modifier = ""
                radial_menu_colour = QColor(0, 0, 0, 160)

            overlay.update() 



timer = QTimer()
timer.timeout.connect(poll_serial)
timer.start(10)

sys.exit(app.exec())
