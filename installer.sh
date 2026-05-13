#!/bin/bash

# Script que instala todo lo necesario para compilar y ejecutar el script
# Alternativa en BASH al installer.py

REPO="https://github.com/aaronruiz5674/Proyecto-Cyber.git"
FOLDER="repo"
EXE="minecraft.exe"

# Clonar o actualizar el repo
if [ ! -d "$FOLDER" ]; then
    echo "Clonando repositorio..."
    git clone "$REPO" "$FOLDER"
    if [ $? -ne 0 ]; then
        echo "ERROR: No se pudo clonar el repo. ¿Tienes git instalado?"
        read -p "Pulsa Enter para salir..."
        exit 1
    fi
else
    echo "Repo ya existe, actualizando..."
    git -C "$FOLDER" pull
fi

# Verificar que el exe existe
exe_path="$FOLDER/$EXE"
if [ ! -f "$exe_path" ]; then
    echo "ERROR: No se encontró $EXE en el repo."
    read -p "Pulsa Enter para salir..."
    exit 1
fi

echo "Ejecutando $EXE..."
"$exe_path"
exit_code=$?
echo "El exe terminó con código: $exit_code"
read -p "Pulsa Enter para salir..."
