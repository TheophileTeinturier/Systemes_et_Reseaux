#!/bin/bash

# on récupère le nom du fichier.txt
nom_fichier_txt="./resumes/$1.txt"

# on récupère la chaîne de caractères
ligne=$2

# on ajoute la chaîne de caractères donnée en paramètre dans le fichier résumé
echo "$ligne">>"$nom_fichier_txt"