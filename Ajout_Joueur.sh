#!/bin/bash

fichier="Stats_Joueurs.txt"

# on récupère le nom du joueur
joueur=$1

# on regarde s'il est présent dans le fichier
stats=$(grep "^${joueur}" "${fichier}")

# si ce n'est pas le cas, on l'ajoute
if [ -z "${stats}" ]; then
    echo "${joueur} : 0 victoire(s), 0 défaite(s), score moyen : 0" >> "${fichier}"
fi