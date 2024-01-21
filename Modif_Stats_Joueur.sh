#!/bin/bash

# on récupère les paramètres
nom=$1
victoire=$2
score=$3

# fichier et  fichier temporaire que le script awk va remplir
fichier="Stats_Joueurs.txt"
fichier_temp=$(mktemp)

# on donne les paramètres au script awk, il met à jour les statistiques pour le joueur avec le nom donné en paramètre
awk -v nom="$nom" -v victoire="$victoire" -v score="$score" '
    $1==nom {
        $10=(($3+$5)*$10+score)/($3+$5+1)
        if (victoire==1) {
            $3++
        }
        else{
            $5++
        }
    }
    { print }
' "$fichier" > "$fichier_temp"

mv "$fichier_temp" "$fichier"
