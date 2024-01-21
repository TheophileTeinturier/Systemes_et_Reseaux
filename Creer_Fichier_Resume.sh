#!/bin/bash

# on récupère le nom du fichier.txt
nom_fichier="./resumes/$1.txt"

# on vérifie que le chemin vers le fichier existe bien
mkdir -p "$(dirname "$nom_fichier")"

# on ajoute le fichier
touch "$nom_fichier"