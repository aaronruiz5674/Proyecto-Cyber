
import os
import subprocess

print("Instalador iniciado...")

# Si estás usando Git:
REPO = "https://github.com/aaronruiz5674/Proyecto-Cyber.git"
FOLDER = "repo"

if not os.path.exists(FOLDER):
    subprocess.run(["git", "clone", REPO, FOLDER])

os.chdir(FOLDER)

print("Ejecutando minecraft.exe...")

subprocess.run(["minecraft.exe"])