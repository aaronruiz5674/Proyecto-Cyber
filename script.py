# Script equivalente en Python (para que lo entendamos mejor)
import pygame
import random
import math
import threading
import sys
import subprocess
from ctypes import *

# Platform-specific imports for Windows
if sys.platform == 'win32':
    import win32api
    import win32con
    import win32gui
    from ctypes import windll
    try:
        from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
        from ctypes import POINTER, cast
    except ImportError:
        pass

# Platform-specific imports for macOS
if sys.platform == 'darwin':
    try:
        from Cocoa import NSApplication, NSWindow
    except ImportError:
        pass

# Configuración de pantalla (lo mismo de antes)
SCREEN_W = 1920
SCREEN_H = 1080

# Tamaño de imagen
IMG_W = 150
IMG_H = 150
MAX_IMAGES = 20

# Palabra secreta
SECRET_ANSWER = "vinicius"

# Clase para las imágenes que rebotan
class BouncingImg:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.vx = 0
        self.vy = 0
        self.angle = 0
        self.va = 0
        self.active = False

class Popup:
    def __init__(self):
        self.visible = False
        self.input = ""
        self.wrong = False

# Función para controlar el volumen del sistema (multiplataforma)
def volume_watcher_thread():
    if sys.platform == 'win32':
        try:
            devices = AudioUtilities.GetSpeakers()
            interface = devices.Activate(IAudioEndpointVolume._iid_, 0, None)
            volume = cast(interface, POINTER(IAudioEndpointVolume))
            
            while True:
                volume.SetMasterVolumeLevelScalar(0.05, None)
                volume.SetMute(False, None)
                pygame.time.delay(200)
        except:
            pass
    
    elif sys.platform == 'darwin':  # macOS
        try:
            while True:
                # Establecer volumen al 5% usando osascript
                subprocess.run(['osascript', '-e', 
                               'set volume output volume 5'], 
                              stderr=subprocess.DEVNULL)
                pygame.time.delay(200)
        except:
            pass
    
    elif sys.platform.startswith('linux'):  # Linux
        try:
            while True:
                # Intentar con PulseAudio primero
                try:
                    subprocess.run(['pactl', 'set-sink-volume', '0', '5%'],
                                  stderr=subprocess.DEVNULL, timeout=1)
                except:
                    # Fallback a ALSA
                    subprocess.run(['amixer', 'set', 'Master', '5%'],
                                  stderr=subprocess.DEVNULL, timeout=1)
                pygame.time.delay(200)
        except:
            pass

# Función para dibujar texto
def draw_text(surface, font, text, x, y, color):
    if not font or not text:
        return
    text_surface = font.render(text, True, color)
    surface.blit(text_surface, (x, y))

# Función para dibujar el popup
def draw_popup(surface, font, popup):
    if not popup.visible:
        return
    
    # Overlay oscuro
    overlay = pygame.Surface((SCREEN_W, SCREEN_H))
    overlay.set_alpha(160)
    overlay.fill((0, 0, 0))
    surface.blit(overlay, (0, 0))
    
    # Caja de diálogo
    bx = SCREEN_W // 2 - 220
    by = SCREEN_H // 2 - 100
    box_rect = pygame.Rect(bx, by, 440, 200)
    
    pygame.draw.rect(surface, (240, 240, 240), box_rect)
    pygame.draw.rect(surface, (80, 80, 80), box_rect, 2)
    
    # Colores
    black = (20, 20, 20)
    red = (200, 0, 0)
    gray = (100, 100, 100)
    
    # Textos
    draw_text(surface, font, "¿Cual es el mejor del mundo?", bx + 70, by + 20, black)
    draw_text(surface, font, "Responde correctamente para parar el virus.", bx + 30, by + 55, gray)
    
    # Campo de entrada
    inp_rect = pygame.Rect(bx + 20, by + 95, 400, 38)
    pygame.draw.rect(surface, (255, 255, 255), inp_rect)
    pygame.draw.rect(surface, (100, 100, 200), inp_rect, 2)
    
    display = popup.input + "|"
    draw_text(surface, font, display, bx + 28, by + 100, black)
    
    # Mensaje de error o instrucción
    if popup.wrong:
        draw_text(surface, font, "Incorrecto. Intentalo de nuevo.", bx + 80, by + 148, red)
    else:
        draw_text(surface, font, "ENTER para confirmar", bx + 140, by + 148, gray)

# Función principal
def main():
    pygame.init()
    
    # Iniciar hilo de control de volumen (multiplataforma)
    try:
        volume_thread = threading.Thread(target=volume_watcher_thread, daemon=True)
        volume_thread.start()
    except:
        pass
    
    # Crear ventana
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H))
    pygame.display.set_caption("")
    
    # Configurar ventana según la plataforma
    if sys.platform == 'win32':
        try:
            hwnd = pygame.display.get_surface().get_abs_offset()
            # Convertir a ventana sin bordes
            win32gui.SetWindowLong(hwnd, win32con.GWL_STYLE, 
                                   win32gui.GetWindowLong(hwnd, win32con.GWL_STYLE) & ~win32con.WS_CAPTION)
            win32gui.SetWindowPos(hwnd, win32con.HWND_TOPMOST, 0, 0, SCREEN_W, SCREEN_H,
                                 win32con.SWP_NOMOVE | win32con.SWP_NOSIZE)
        except:
            pass
    
    elif sys.platform == 'darwin':  # macOS
        try:
            # Hacer la ventana siempre al frente
            subprocess.run(['osascript', '-e',
                           f'tell application "System Events" to keystroke tab using cmd + option'],
                          stderr=subprocess.DEVNULL)
        except:
            pass
    
    elif sys.platform.startswith('linux'):  # Linux
        try:
            # Usar wmctrl para hacer la ventana siempre al frente
            # Este comando requiere wmctrl instalado
            subprocess.Popen(['wmctrl', '-r', ':ACTIVE:', '-b', 'add,above,sticky'],
                            stderr=subprocess.DEVNULL)
        except:
            pass
    
    clock = pygame.time.Clock()
    
    # Cargar imagen
    img_surface = None
    try:
        img_surface = pygame.image.load("goat.png")
        img_surface = pygame.transform.scale(img_surface, (IMG_W, IMG_H))
    except:
        print("Advertencia: No se pudo cargar 'goat.png'")
    
    # Cargar música
    try:
        pygame.mixer.music.load("Balada.wav")
        pygame.mixer.music.play(-1)
        pygame.mixer.music.set_volume(1.0)
    except:
        print("Advertencia: No se pudo cargar 'Balada.wav'")
    
    # Cargar fuente
    font = None
    font_paths = ["font.ttf"]
    
    # Rutas específicas por plataforma
    if sys.platform == 'win32':
        font_paths.extend([
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/Arial.ttf"
        ])
    elif sys.platform == 'darwin':  # macOS
        font_paths.extend([
            "/System/Library/Fonts/Arial.ttf",
            "/Library/Fonts/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc"
        ])
    elif sys.platform.startswith('linux'):  # Linux
        font_paths.extend([
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
        ])
    
    for path in font_paths:
        try:
            font = pygame.font.Font(path, 20)
            break
        except:
            continue
    
    # Fallback a fuente por defecto
    if font is None:
        font = pygame.font.Font(None, 20)
    
    # Estado del programa
    imgs = [BouncingImg() for _ in range(MAX_IMAGES)]
    img_count = 0
    popup = Popup()
    running = True
    
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            
            elif event.type == pygame.KEYDOWN:
                mods = pygame.key.get_mods()
                
                # Ctrl+Q para mostrar popup
                if event.key == pygame.K_q and (mods & pygame.KMOD_CTRL) and not popup.visible:
                    popup.visible = True
                    popup.input = ""
                    popup.wrong = False
                
                if popup.visible:
                    if event.key == pygame.K_BACKSPACE:
                        popup.input = popup.input[:-1]
                    
                    elif event.key == pygame.K_RETURN:
                        if popup.input == SECRET_ANSWER:
                            running = False
                        else:
                            popup.wrong = True
                            popup.input = ""
            
            elif event.type == pygame.TEXTINPUT and popup.visible:
                if event.text.isprintable():
                    popup.input += event.text
        
        # Detectar clicks del ratón para crear nuevas imágenes
        if pygame.mouse.get_pressed()[0] and not popup.visible:
            if img_count < MAX_IMAGES:
                imgs[img_count].x = random.randint(0, SCREEN_W - IMG_W)
                imgs[img_count].y = random.randint(0, SCREEN_H - IMG_H)
                
                spd = 2.5 + (random.randint(0, 29) / 10.0)
                ang = (random.randint(0, 359) * math.pi / 180.0)
                
                imgs[img_count].vx = math.cos(ang) * spd
                imgs[img_count].vy = math.sin(ang) * spd
                
                imgs[img_count].angle = 0
                imgs[img_count].va = ((random.randint(0, 199) / 100.0) - 3.0)
                
                imgs[img_count].active = True
                img_count += 1
        
        # Actualizar posiciones de imágenes
        for i in range(img_count):
            if not imgs[i].active:
                continue
            
            imgs[i].x += imgs[i].vx
            imgs[i].y += imgs[i].vy
            
            imgs[i].angle += imgs[i].va
            
            if imgs[i].angle > 15 or imgs[i].angle < -15:
                imgs[i].va *= -1
            
            if imgs[i].x <= 0 or imgs[i].x >= SCREEN_W - IMG_W:
                imgs[i].vx *= -1
            
            if imgs[i].y <= 0 or imgs[i].y >= SCREEN_H - IMG_H:
                imgs[i].vy *= -1
        
        # Dibujar fondo
        if popup.visible:
            screen.fill((30, 30, 30))
        else:
            screen.fill((1, 1, 1))
        
        # Dibujar imágenes
        for i in range(img_count):
            if img_surface and imgs[i].active:
                rotated_img = pygame.transform.rotate(img_surface, imgs[i].angle)
                screen.blit(rotated_img, (int(imgs[i].x), int(imgs[i].y)))
        
        # Dibujar popup
        draw_popup(screen, font, popup)
        
        # Actualizar pantalla
        pygame.display.flip()
        clock.tick(60)
    
    pygame.quit()

if __name__ == "__main__":
    main()
