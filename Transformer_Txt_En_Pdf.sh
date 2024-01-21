#!/bin/bash

# on récupère le nom du fichier.txt
nom_fichier_txt="./resumes/$1.txt"

# on prépare le nom du fichier .ps
nom_fichier_ps="./resumes/$1.ps"

# on prépare le nom du fichier .pdf
nom_fichier_pdf="./resumes/$1.pdf"

# on convertit le .txt en .ps
enscript -B -o "$nom_fichier_ps" "$nom_fichier_txt"

# on convertit le .ps en .pdf
ps2pdf "$nom_fichier_ps" "$nom_fichier_pdf"

# on supprime le .txt et le .ps
rm "$nom_fichier_txt"
rm "$nom_fichier_ps"