#!/usr/bin/env python3
"""Touch camera viewer for the TB311FU vendor capture backend."""
from datetime import datetime
from pathlib import Path
import struct
import sys

from PyQt6.QtCore import QProcess, QStandardPaths, Qt, QTimer
from PyQt6.QtGui import QImage, QPixmap, QTransform
from PyQt6.QtWidgets import QApplication, QLabel, QPushButton, QVBoxLayout, QHBoxLayout, QWidget


class CameraApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('Câmera')
        self.resize(800, 1100)
        self.camera = 1
        self.rotation = 270
        self.frame = QImage()
        self.frame_count = 0
        self.pending = bytearray()
        self.restart = False
        self.closing = False
        self.process = QProcess(self)
        self.process.readyReadStandardOutput.connect(self.receive)
        self.process.readyReadStandardError.connect(self.diagnostics)
        self.process.finished.connect(self.finished)
        self.status = QLabel('Iniciando câmera frontal…')
        self.status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.view = QLabel()
        self.view.setMinimumSize(200, 300)
        self.view.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.switch = QPushButton('Trocar câmera')
        self.switch.clicked.connect(self.change_camera)
        self.shutter = QPushButton('Tirar foto')
        self.shutter.clicked.connect(self.take_photo)
        self.shutter.setEnabled(False)
        rotate = QPushButton('Girar')
        rotate.clicked.connect(self.rotate)
        controls = QHBoxLayout()
        for button in (self.switch, self.shutter, rotate):
            button.setMinimumHeight(70)
            controls.addWidget(button)
        layout = QVBoxLayout(self)
        layout.addWidget(self.status)
        layout.addWidget(self.view, 1)
        layout.addLayout(controls)
        self.setStyleSheet('QWidget{background:#111;color:#eee;font-size:20px} '
                          'QPushButton{background:#303030;border-radius:12px;padding:14px} '
                          'QPushButton:pressed{background:#4e9a32} '
                          'QPushButton:disabled{color:#777}')
        self.log = Path.home() / '.cache/tb311fu-camera.log'
        self.log.parent.mkdir(parents=True, exist_ok=True)
        self.log.write_text('')
        QTimer.singleShot(0, self.start)

    def start(self):
        self.pending.clear()
        self.frame = QImage()
        self.frame_count = 0
        self.view.clear()
        self.shutter.setEnabled(False)
        self.status.setText('Iniciando câmera frontal…' if self.camera else 'Iniciando câmera traseira…')
        self.process.start('sudo', ['-n', '/usr/bin/python3',
                                   '/usr/local/libexec/tb311fu-camera/camera-frame.py',
                                   '--camera', str(self.camera), '--preview'])

    def diagnostics(self):
        with self.log.open('ab') as out:
            out.write(bytes(self.process.readAllStandardError()))

    def receive(self):
        self.pending.extend(bytes(self.process.readAllStandardOutput()))
        newest = None
        while len(self.pending) >= 4:
            length = struct.unpack_from('I', self.pending)[0]
            if not 0 < length < 8 * 1024 * 1024:
                self.status.setText('Falha na comunicação com a câmera')
                self.process.terminate()
                self.pending.clear()
                return
            if len(self.pending) < length + 4:
                break
            newest = bytes(self.pending[4:4+length])
            del self.pending[:4+length]
        if newest:
            image = QImage.fromData(newest, 'PPM')
            if not image.isNull():
                self.frame = image
                self.frame_count += 1
                self.render()
                self.shutter.setEnabled(True)
                self.status.setText('Câmera frontal' if self.camera else 'Câmera traseira')

    def render(self):
        if not self.frame.isNull():
            image = self.frame.transformed(QTransform().rotate(self.rotation))
            if self.camera:
                image = image.mirrored(True, False)
            self.view.setPixmap(QPixmap.fromImage(image).scaled(
                self.view.size(), Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation))

    def rotate(self):
        self.rotation = (self.rotation + 90) % 360
        self.render()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.render()

    def change_camera(self):
        self.camera = 1 - self.camera
        self.restart = True
        self.switch.setEnabled(False)
        self.process.terminate()
        if self.process.state() == QProcess.ProcessState.NotRunning:
            self.finished()

    def finished(self, *args):
        self.diagnostics()
        if self.closing:
            self.close()
        elif self.restart:
            self.restart = False
            self.switch.setEnabled(True)
            self.start()
        else:
            self.shutter.setEnabled(False)
            self.status.setText('Câmera encerrada. Toque em Trocar câmera para reabrir.')

    def take_photo(self):
        if self.frame.isNull():
            return
        directory = Path(QStandardPaths.writableLocation(QStandardPaths.StandardLocation.PicturesLocation)) / 'Camera'
        directory.mkdir(parents=True, exist_ok=True)
        filename = directory / (datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.jpg')
        image = self.frame.transformed(QTransform().rotate(self.rotation))
        if image.save(str(filename), 'JPEG', 95):
            self.status.setText(f'Foto salva em Imagens/Camera/{filename.name}')
            return filename

    def closeEvent(self, event):
        if self.process.state() != QProcess.ProcessState.NotRunning:
            stopping = self.closing or self.restart
            self.closing = True
            self.restart = False
            if not stopping:
                self.process.terminate()
            event.ignore()
        else:
            event.accept()


app = QApplication(sys.argv)
window = CameraApp()
window.show()
sys.exit(app.exec())
