# Proyecto Cyberseguridad


**Versiones disponibles:**
- `script.c` - Versión en C (Windows únicamente)
- `script.py` - Versión en Python (Windows, macOS, Linux)

## Instalación y ejecución

### Windows (Usar script.c)

1. **Instalar compilador de C:**
   - Descargar e instalar [MinGW](https://www.mingw-w64.org/) o [Microsoft Visual C++](https://visualstudio.microsoft.com/es/downloads/)

2. **Instalar librerías SDL2:**
   - Descargar desde [libsdl.org](https://www.libsdl.org/)
   - SDL2, SDL2_image, SDL2_mixer, SDL2_ttf

3. **Compilar:**
```bash
gcc script.c -o script.exe -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lole32 -loleaut32 -luuid -lmmdeviceapi -lendpointvolume
```

4. **Ejecutar:**
```bash
script.exe
```

---

### macOS (Usar script.py)
1. **Instalar Python 3.8+:**
```bash
brew install python3
```

2. **Instalar dependencias:**
```bash
pip3 install pygame
```

3. **Instalar herramientas del sistema (opcional):**
```bash
brew install wmctrl
```

4. **Ejecutar:**
```bash
python3 script.py
```

---

### Linux (Usar script.py)


#### Ubuntu/Debian

1. **Instalar Python y dependencias del sistema:**
```bash
sudo apt update
sudo apt install python3 python3-pip pulseaudio-utils alsa-utils wmctrl
```

2. **Instalar dependencias Python:**
```bash
pip3 install pygame
```

3. **Ejecutar:**
```bash
python3 script.py
```

## Uso

- **Click del ratón**: Crea nuevas imágenes rebotando (máx. 20)
- **Ctrl+Q**: Abre el popup con pregunta de seguridad
- **ENTER**: Confirma tu respuesta
- **BACKSPACE**: Borra el último carácter