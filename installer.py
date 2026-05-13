import os
import sys
import subprocess

REPO   = "https://github.com/aaronruiz5674/Proyecto-Cyber.git"
FOLDER = "repo"
EXE    = "minecraft.exe"

print("Iniciando launcher...")

# Clonar o actualizar el repo
if not os.path.exists(FOLDER):
    print("Clonando repositorio...")
    result = subprocess.run(["git", "clone", REPO, FOLDER])
    if result.returncode != 0:
        print("ERROR: No se pudo clonar el repo. ¿Tienes git instalado?")
        input("Pulsa Enter para salir...")
        sys.exit(1)
else:
    print("Repo ya existe, actualizando...")
    subprocess.run(["git", "pull"], cwd=FOLDER)

# Verificar que el exe existe
exe_path = os.path.join(FOLDER, EXE)
if not os.path.exists(exe_path):
    print(f"ERROR: No se encontró {EXE} en el repo.")
    input("Pulsa Enter para salir...")
    sys.exit(1)

print(f"Ejecutando {EXE}...")
result = subprocess.run([exe_path], cwd=FOLDER)
print(f"El exe terminó con código: {result.returncode}")
input("Pulsa Enter para salir...")