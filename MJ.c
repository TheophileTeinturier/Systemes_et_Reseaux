#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_JOUEURS 10 // le nombre de joueurs ne peut pas dépasser 10
int nbRobots=0; // nombre de joueurs robots choisi
int nbHumains=0; // nombre de joueurs humains choisi
int finPartie=0; // pour savoir si la partie est finie
char nomFichier[100]; // pour stocker le nom du fichier qui contiendra le résumé de la partie
int numManche=1; // pour savoir dans le nombre de manches jouées

// pour synchroniser les threads
#define nbMutexsConds 4
int enAttente=0; // pour savoir combien de threads sont en attente pour la synchronisation
pthread_mutex_t mutexs[nbMutexsConds];
pthread_cond_t conds[nbMutexsConds];

// structure pour les cartes
typedef struct{
    int numero;
    int nbTetes;
}Carte;

Carte* listeCartes; // liste des 104 cartes du jeu


// structure pour gérer les joueurs
typedef struct{
    int ind; // indice dans la liste de joueurs
    int socket;
    char nom[100];
    int score;
    Carte* cartes;
    int carteChoisie;
    int victoire; // pour savoir si le joueur a gagné (1) ou non (0)
}infosJoueur;

infosJoueur listeInfosJoueurs[MAX_JOUEURS]; // pour pouvoir stocker les informations des joueurs et les donner aux threads


// structure pour gérer les tas de cartes "sur la table"
typedef struct{
    int nbCartesTas[4]; // tableau qui stocke le nombre de cartes dans chaque tas
    Carte tab[4][5]; // tableau qui stocke les cartes de nos tas
}Jeu;

Jeu jeu; // les tas de cartes sur la table

// structure pour gérer les robots
typedef struct{
    int ind; // indice dans la liste
    int score;
    Carte* cartes;
    int carteChoisie;
}infosRobot;

infosRobot listeInfosRobots[MAX_JOUEURS]; // pour pouvoir stocker les informations des robots






/********************************************************************
 * MÉTHODES POUR GÉRER LES CARTES
********************************************************************/

// méthode pour remplir notre liste de cartes
void remplirListeCartes(){
    listeCartes=(Carte*) malloc(104*sizeof(Carte));
    for(int i=0; i<104; i++){
        listeCartes[i].numero=i+1;
        if((i+1)%10==0){
            listeCartes[i].nbTetes=3;
        }
        else{
            if((i+1)%5==0&&(i+1)%11==0){
                listeCartes[i].nbTetes=7;
            }
            else{
                if((i+1)%5==0){
                    listeCartes[i].nbTetes=2;
                }
                else{
                    if((i+1)%11==0){
                        listeCartes[i].nbTetes=5;
                    }
                    else{
                        listeCartes[i].nbTetes=1;
                    }
                }
            }
        }
    }
}

// méthode pour libérer l'espace mémoire de la liste
void freeListeCartes(){
    free(listeCartes);
}

// méthode pour mélanger les cartes
void melangeListeCartes(){
    //pour le mélange des cartes
    srand(time(NULL));
    
    for (int i=0; i<1000; i++){
        int a=rand()%104;
        int b=rand()%104;
        Carte stock=listeCartes[a];
        listeCartes[a]=listeCartes[b];
        listeCartes[b]=stock;
    }
}

// méthode pour libérer l'espace mémoire du tas de cartes du joueur
void freeListeCartesJoueur(infosJoueur* infos){
    free(infos->cartes);
}

// méthode pour libérer l'espace mémoire du tas de cartes du robot
void freeListeCartesRobot(infosRobot* infos){
    free(infos->cartes);
}

// méthode pour récupérer 10 cartes (pour les joueurs)
void recupCartes(infosJoueur* infos){
    infos->cartes=(Carte*) realloc(infos->cartes,10*sizeof(Carte));
    for(int i=0; i<10; i++){
        infos->cartes[i]=listeCartes[10*(infos->ind)+i];
    }
}

// méthode pour récupérer 10 cartes (pour les robots)
void recupCartesRobot(infosRobot* infos){
    infos->cartes=(Carte*) realloc(infos->cartes,10*sizeof(Carte));
    for(int i=0; i<10; i++){
        infos->cartes[i]=listeCartes[10*((infos->ind)+nbHumains)+i];
    }
}

// méthode pour "enlever" la carte choisie par le joueur
void enleveCarte(infosJoueur* infos, int nbCartes){
    infos->cartes[infos->carteChoisie].numero=infos->cartes[nbCartes-1].numero;
    infos->cartes[infos->carteChoisie].nbTetes=infos->cartes[nbCartes-1].nbTetes;
}

// méthode pour "enlever" la carte choisie par le robot
void enleveCarteRobot(infosRobot* infos, int nbCartes){
    infos->cartes[infos->carteChoisie].numero=infos->cartes[nbCartes-1].numero;
    infos->cartes[infos->carteChoisie].nbTetes=infos->cartes[nbCartes-1].nbTetes;
}



/********************************************************************
 * FIN MÉTHODES POUR GÉRER LES CARTES
********************************************************************/

















/********************************************************************
 * MÉTHODES POUR GÉRER LE JEU "SUR LA TABLE"
********************************************************************/

// méthode pour initialiser les tas de cartes
void initTasDeCartesJeu(){

    // initialisation du nombre de cartes dans les tas
    for(int i=0; i<4; i++){
        jeu.nbCartesTas[i]=1;
    }

    // on met les dernières cartes du paquet sur le jeu (elles ne peuvent pas être distribuées aux joueurs)
    for(int i=0; i<4; i++){
        jeu.tab[i][0].nbTetes=listeCartes[100+i].nbTetes;
        jeu.tab[i][0].numero=listeCartes[100+i].numero;
    }

}

/********************************************************************
 * FIN MÉTHODES POUR GÉRER LE JEU ("SUR LA TABLE")
********************************************************************/
















/********************************************************************
 * MÉTHODES COMMUNES AUX THREADS JOUEURS ET ROBOTS
********************************************************************/

// méthode pour savoir si l'indice d'un joueur est dans une liste
int estDansListe(int* liste, int taille, int ind){
    for(int i=0; i<taille; i++){
        if(liste[i]==ind){
            return 1;
        }
    }
    return 0;
}

// méthode pour connaître le joueur avec la carte la plus petite (en prenant en compte les joueurs déjà traités)
int joueurAvecCarteLaPlusPetite(int* listeJoueursTraites, int taille){
    int ind=-1;
    Carte laPlusPetite;
    for(int i=0; i<nbHumains; i++){
        Carte carteJoueurI=listeInfosJoueurs[i].cartes[listeInfosJoueurs[i].carteChoisie];
        if(ind==-1){
            if(estDansListe(listeJoueursTraites,taille,i)==0){
                ind=i;
                laPlusPetite=carteJoueurI;
            }
        }
        else{
            if((carteJoueurI.numero<laPlusPetite.numero)&&(estDansListe(listeJoueursTraites,taille,i)==0)){
                ind=i;
                laPlusPetite=carteJoueurI;
            }
        }
    }
    return ind;
}

// méthode pour connaître le robot avec la carte la plus petite (en prenant en compte les robots déjà traités)
int robotAvecCarteLaPlusPetite(int* listeRobotsTraites, int taille){
    int ind=-1;
    Carte laPlusPetite;
    for(int i=0; i<nbRobots; i++){
        Carte carteRobotI=listeInfosRobots[i].cartes[listeInfosRobots[i].carteChoisie];
        if(ind==-1){
            if(estDansListe(listeRobotsTraites,taille,i)==0){
                ind=i;
                laPlusPetite=carteRobotI;
            }
        }
        else{
            if((carteRobotI.numero<laPlusPetite.numero)&&(estDansListe(listeRobotsTraites,taille,i)==0)){
                ind=i;
                laPlusPetite=carteRobotI;
            }
        }
    }
    return ind;
}

// méthode pour savoir dans quel tas il faut placer la carte d'un joueur
int placerCarteJoueur(int ind){
    int indTas=-1;
    Carte carteJoueur=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie];
    Carte carteTasChoisi;
    for(int i=0; i<4; i++){
        Carte carteTas=jeu.tab[i][jeu.nbCartesTas[i]-1];
        if(indTas==-1){
            if(carteTas.numero<carteJoueur.numero){
                indTas=i;
                carteTasChoisi=jeu.tab[indTas][jeu.nbCartesTas[indTas]-1];
            }
        }
        else{
            if((carteTas.numero<carteJoueur.numero)&&(carteTas.numero>carteTasChoisi.numero)){
                indTas=i;
                carteTasChoisi=jeu.tab[indTas][jeu.nbCartesTas[indTas]-1];
            }
        }
    }
    return indTas;
}

// méthode pour savoir dans quel tas il faut placer la carte d'un robot
int placerCarteRobot(int ind){
    int indTas=-1;
    Carte carteRobot=listeInfosRobots[ind].cartes[listeInfosRobots[ind].carteChoisie];
    Carte carteTasChoisi;
    for(int i=0; i<4; i++){
        Carte carteTas=jeu.tab[i][jeu.nbCartesTas[i]-1];
        if(indTas==-1){
            if(carteTas.numero<carteRobot.numero){
                indTas=i;
                carteTasChoisi=jeu.tab[indTas][jeu.nbCartesTas[indTas]-1];
            }
        }
        else{
            if((carteTas.numero<carteRobot.numero)&&(carteTas.numero>carteTasChoisi.numero)){
                indTas=i;
                carteTasChoisi=jeu.tab[indTas][jeu.nbCartesTas[indTas]-1];
            }
        }
    }
    return indTas;
}

// méthode pour que le robot prenne le tas avec le moins de score si sa carte est trop petite
int choixTasRobot(){
    int indTas=0;
    int scoreTas=1000; // valeur impossible à atteindre avec 5 cartes
    for(int i=0; i<4; i++){
        int somme=0;
        for(int j=0; j<jeu.nbCartesTas[i]; j++){
            somme+=jeu.tab[i][j].nbTetes;
        }
        if(somme<scoreTas){
            scoreTas=somme;
            indTas=i;
        }
    }
    return indTas;
}

// méthode pour faire les traitements dans le cas où un joueur humain à la carte la plus petite de tous les joueurs
void traitementJoueur(int ind, int nbTours){
    int indTas=placerCarteJoueur(ind);
            // si la carte du joueur est plus petite que celles en haut des tas de cartes, il choisit un tas qu'il va ramasser
            if(indTas==-1){
                int t; // pour récupérer le choix du joueur
                int choix=1;
                send(listeInfosJoueurs[ind].socket,&choix,sizeof(choix),0); // on envoi un message pour dire au joueur de choisir un tas
                send(listeInfosJoueurs[ind].socket,&jeu,sizeof(jeu),0); // on envoi le jeu
                int n=recv(listeInfosJoueurs[ind].socket,&t,sizeof(t),0); // on récupère le choix
                if (n==-1){
                    perror("Erreur, pas de chiffre reçu");
                }

                // on ajuste le score du joueur
                for(int j=0; j<jeu.nbCartesTas[t]; j++){
                    listeInfosJoueurs[ind].score+=jeu.tab[t][j].nbTetes;
                }

                // on remplace le tas par la carte du joueur
                jeu.tab[t][0].nbTetes=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].nbTetes;
                jeu.tab[t][0].numero=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].numero;
                jeu.nbCartesTas[t]=1;
            }
            else{
                int choix=0;
                send(listeInfosJoueurs[ind].socket,&choix,sizeof(choix),0); // on envoi un message pour ne pas faire le traitement du choix du tas
                
                // si le tas correspondant a 5 cartes, le joueur ramasse les cartes
                if(jeu.nbCartesTas[indTas]==5){
                    // on ajuste le score du joueur
                    for(int j=0; j<5; j++){
                        listeInfosJoueurs[ind].score+=jeu.tab[indTas][j].nbTetes;
                    }
                    // on remplace le tas par la carte du joueur
                    jeu.tab[indTas][0].nbTetes=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].nbTetes;
                    jeu.tab[indTas][0].numero=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].numero;
                    jeu.nbCartesTas[indTas]=1;
                }
                else{
                    // on ajoute la carte au tas
                    jeu.tab[indTas][jeu.nbCartesTas[indTas]].nbTetes=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].nbTetes;
                    jeu.tab[indTas][jeu.nbCartesTas[indTas]].numero=listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].numero;
                    jeu.nbCartesTas[indTas]++;
                }
            }

            // on enlève la carte du paquet du joueur
            enleveCarte(&listeInfosJoueurs[ind],nbTours);
}

// méthode pour faire les traitements dans le cas où un joueur robot à la carte la plus petite de tous les joueurs
void traitementRobot(int indR, int nbTours){
            int indTas=placerCarteRobot(indR);
            // si la carte du robot est plus petite que celles en haut des tas de cartes, il choisit un tas qu'il va ramasser
            if(indTas==-1){
                
                int t=choixTasRobot();

                //on ajuste le score du robot
                for(int j=0; j<jeu.nbCartesTas[t]; j++){
                    listeInfosRobots[indR].score+=jeu.tab[t][j].nbTetes;
                }

                // on remplace le tas par la carte du robot
                jeu.tab[t][0].nbTetes=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].nbTetes;
                jeu.tab[t][0].numero=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].numero;
                jeu.nbCartesTas[t]=1;
            }
            else{                
                // si le tas correspondant a 5 cartes, le robot ramasse les cartes
                if(jeu.nbCartesTas[indTas]==5){
                    // on ajuste le score au robot
                    for(int j=0; j<5; j++){
                        listeInfosRobots[indR].score+=jeu.tab[indTas][j].nbTetes;
                    }
                    // on remplace le tas par la carte du robot
                    jeu.tab[indTas][0].nbTetes=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].nbTetes;
                    jeu.tab[indTas][0].numero=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].numero;
                    jeu.nbCartesTas[indTas]=1;
                }
                else{
                    // on ajoute la carte au tas
                    jeu.tab[indTas][jeu.nbCartesTas[indTas]].nbTetes=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].nbTetes;
                    jeu.tab[indTas][jeu.nbCartesTas[indTas]].numero=listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].numero;
                    jeu.nbCartesTas[indTas]++;
                }
            }

            // on enlève la carte du paquet du robot
            enleveCarteRobot(&listeInfosRobots[indR],nbTours);
}

// méthode pour placer automatiquement les cartes et gérer le jeu
void gestionJeu(int nbTours){
    int listeJoueursTraites[nbHumains]; // pour savoir les joueurs dont la carte à déjà été traitée
    int listeRobotsTraites[nbRobots]; // idem avec les robots
    int nbJoueursTraites=0;
    int nbRobotsTraites=0;

    // initialisation avec des -1 pour pas poser de problèmes lors de la recherche du joueur avec la plus petite carte
    for(int i=0; i<nbHumains;i++){
        listeJoueursTraites[i]=-1;
    }

    // idem avec les robots
    for(int i=0; i<nbRobots;i++){
        listeRobotsTraites[i]=-1;
    }

    for(int i=0; i<(nbHumains+nbRobots); i++){

        //traitement s'il n'y a pas de robots
        if(nbRobots==0){
            // on récupère le joueur avec la carte la plus petite et on fait les traitements pour mettre à jour le jeu, son score...
            int ind=joueurAvecCarteLaPlusPetite(listeJoueursTraites,i);
            nbJoueursTraites++;
            listeJoueursTraites[i]=ind;
            traitementJoueur(ind,nbTours);
        }
        
        //idem s'il n'y a pas d'humains
        if(nbHumains==0){
            int indR=robotAvecCarteLaPlusPetite(listeRobotsTraites,i);
            nbRobotsTraites++;
            listeRobotsTraites[i]=indR;
            traitementRobot(indR,nbTours);
        }

        //traitement s'il y a ds robots et des humains
        if(nbHumains>0&&nbRobots>0){
            // on récupère le joueur avec la carte la plus petite, idem pour les robots
            int ind=joueurAvecCarteLaPlusPetite(listeJoueursTraites,nbJoueursTraites);
            int indR=robotAvecCarteLaPlusPetite(listeRobotsTraites,nbRobotsTraites);

            if(ind==-1){
                // il ne reste plus que des robots à traiter
                listeRobotsTraites[nbRobotsTraites]=indR;
                traitementRobot(indR,nbTours);
                nbRobotsTraites++;
            }
            else{
                if(indR==-1){
                    // il ne reste plus que des joueurs à traiter
                    listeJoueursTraites[nbJoueursTraites]=ind;
                    traitementJoueur(ind,nbTours);
                    nbJoueursTraites++;
                }
                else{
                     // on regarde si la carte du joueur est plus petite que celle du robot
                    if(listeInfosJoueurs[ind].cartes[listeInfosJoueurs[ind].carteChoisie].numero<listeInfosRobots[indR].cartes[listeInfosRobots[indR].carteChoisie].numero){
                        listeJoueursTraites[nbJoueursTraites]=ind;
                        traitementJoueur(ind,nbTours);
                        nbJoueursTraites++;
                        
                    }
                    else{
                        listeRobotsTraites[nbRobotsTraites]=indR;
                        traitementRobot(indR,nbTours);
                        nbRobotsTraites++;
                    }
                }
            }
        }
    }
}

// méthode pour savoir si la partie est finie et gérer tout ce qu'il y a à faire dans ce cas
void finDePartie(){
    for(int i=0; i<nbHumains;i++){
        if(listeInfosJoueurs[i].score>=20){
            finPartie=1;
        }
    }
    for(int i=0; i<nbRobots;i++){
        if(listeInfosRobots[i].score>=20){
            finPartie=1;
        }
    }
    numManche++;
}

// méthode pour indiquer qu'une nouvelle manche a commencée dans le résumé
void Nouvelle_Manche_Resume(){
    char s[20];
    snprintf(s, sizeof(s), "Manche %d !", numManche);
    char commande[200];
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,s);
    int n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout du numéro de manche");
    }
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"");
    n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout du numéro de manche");
    }

}

// méthode pour indiquer dans le résumé le choix de la carte des joueurs humains et robots
void ajouter_Choix_Resume(){
    for(int i=0; i<nbHumains; i++){
        char commande[300];
        Carte c=listeInfosJoueurs[i].cartes[listeInfosJoueurs[i].carteChoisie];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s : [%i , %i]\"",nomFichier,listeInfosJoueurs[i].nom,c.numero,c.nbTetes);
        int n=system(commande);
        if(n!=0){
            perror("Erreur dans le résumé lors de l'ajout des choix des cartes");
        }
    }

    for(int i=0; i<nbRobots; i++){
        char commande[300];
        Carte c=listeInfosRobots[i].cartes[listeInfosRobots[i].carteChoisie];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"Robot %i : [%i , %i]\"",nomFichier,i,c.numero,c.nbTetes);
        int n=system(commande);
        if(n!=0){
            perror("Erreur dans le résumé lors de l'ajout des choix des cartes");
        }
    }

    char commande[200];
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"");
    int n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout des choix des cartes");
    }
}

// méthode pour ajouter le jeu au résumé
void ajouter_Jeu_Resume(){
    for(int i=0; i<4; i++){
        char s[300]="";
        for(int j=0; j<jeu.nbCartesTas[i]; j++){
            Carte c=jeu.tab[i][j];
            char s1[20];
            snprintf(s1,sizeof(s1)," [%i , %i]",c.numero,c.nbTetes);
            strcat(s,s1);
        }
        char commande[600];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,s);
        int n=system(commande);
        if(n!=0){
            perror("Erreur dans le résumé lors de l'ajout du jeu");
        }
    }

    char commande[200];
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"");
    int n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout du jeu");
    }
}

// méthode pour indiquer dans le résumé les scores en fin de manche des joueurs humains et robots
void ajouter_Scores_Resume(){
    for(int i=0; i<nbHumains; i++){
        char commande[300];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"score du joueur %s : %i\"",nomFichier,listeInfosJoueurs[i].nom,listeInfosJoueurs[i].score);
        int n=system(commande);
        if(n!=0){
            perror("Erreur dans le résumé lors de l'ajout des scores");
        }
    }

    for(int i=0; i<nbRobots; i++){
        char commande[300];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"score du robot %i : %i\"",nomFichier,i,listeInfosRobots[i].score);
        int n=system(commande);
        if(n!=0){
            perror("Erreur dans le résumé lors de l'ajout des scores");
        }
    }

    char commande[200];
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"");
    int n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout des scores");
    }
}

/********************************************************************
 * FIN MÉTHODES COMMUNES AUX THREADS JOUEURS ET ROBOTS
********************************************************************/
















/********************************************************************
 * MÉTHODES DES THREADS ROBOTS
********************************************************************/

// méthode pour choisir une carte, on prend la carte avec l'écart le plus petit possible avec l'une des cartes
//des tas
void choixCarteRobot(infosRobot* infos, int nbTours) {
    int indC=-1;
    int indT=-1;
    
    for (int i=0; i<nbTours; i++) {
        Carte cI=infos->cartes[i];
        
        for (int j=0; j<4; j++) {
            Carte carteTas=jeu.tab[j][jeu.nbCartesTas[j]-1];
            
            if (cI.numero>carteTas.numero && (indC==-1||(cI.numero-carteTas.numero)<(infos->cartes[indC].numero-jeu.tab[indT][jeu.nbCartesTas[indT]-1].numero))) {
                indC=i;
                indT=j;
            }
        }
    }

    if (indC==-1) {
        infos->carteChoisie=0;
    }
    else{
        infos->carteChoisie=indC;
    }
}

// méthode pour que les threads joueurs puissent faire une manche
void mancheRobots(infosRobot* infos){

    // le premier thread s'occupe de mélanger les cartes du jeu et d'initialiser les tas de cartes sur la table
    pthread_mutex_lock(&mutexs[1]);
        if(enAttente==0){
            melangeListeCartes();
            initTasDeCartesJeu();
            Nouvelle_Manche_Resume();
            ajouter_Jeu_Resume();
        }

        enAttente++;

        if (enAttente>=(nbHumains+nbRobots)){
            enAttente=0;
            pthread_cond_broadcast(&conds[1]);
        }
        else{
            pthread_cond_wait(&conds[1], &mutexs[1]);
        }
    pthread_mutex_unlock(&mutexs[1]);

    // on récupère les cartes
    recupCartesRobot(infos);


    for(int i=10; i>0; i--){

        // on récupère l'indice de la carte choisie par le robot
        choixCarteRobot(infos,i);

        // on attend que tous les joueurs renvoient une carte, le dernier thread fait le tri des cartes, gère les choix supplémentaires...
        pthread_mutex_lock(&mutexs[2]);
            enAttente++;
            if (enAttente>=(nbHumains+nbRobots)){
                enAttente=0;
                ajouter_Choix_Resume();
                gestionJeu(i);
                ajouter_Jeu_Resume();
                pthread_cond_broadcast(&conds[2]);
            }
            else{
                pthread_cond_wait(&conds[2], &mutexs[2]);
            }
        pthread_mutex_unlock(&mutexs[2]);
    }
}

// méthode pour la gestion des robots
void *gestionRobot(void *parametre){

    infosRobot* infos=(infosRobot*) parametre;
    
    // on attend que les joueurs rentrent leur nom
    pthread_mutex_lock(&mutexs[0]);
        enAttente++;
        if (enAttente>=(nbHumains+nbRobots)){
            pthread_cond_broadcast(&conds[0]);
            enAttente=0;
        }
        else{
            pthread_cond_wait(&conds[0], &mutexs[0]);
        }
    pthread_mutex_unlock(&mutexs[0]);

    while(!finPartie)
    {
        mancheRobots(infos);

        
        //le dernier thread regarde si c'est la fin de la partie, et lance les traitements à faire si nécessaire
        pthread_mutex_lock(&mutexs[3]);
            enAttente++;
            if (enAttente>=(nbHumains+nbRobots)){
                finDePartie();
                ajouter_Scores_Resume();
                pthread_cond_broadcast(&conds[3]);
                enAttente=0;
            }
            else{
                pthread_cond_wait(&conds[3], &mutexs[3]);
            }
        pthread_mutex_unlock(&mutexs[3]);
    }


    // libération de la mémoire
    freeListeCartesRobot(infos);

    pthread_exit(NULL);
}

/********************************************************************
 * FIN MÉTHODES DES THREADS ROBOTS
********************************************************************/

















/********************************************************************
 * MÉTHODES DES THREADS JOUEURS
********************************************************************/

// méthode pour que les threads joueurs puissent faire une manche
void manche(infosJoueur* infos){
    
    // le premier thread s'occupe de mélanger les cartes du jeu et d'initialiser les tas de cartes sur la table
    pthread_mutex_lock(&mutexs[1]);
        if(enAttente==0){
            melangeListeCartes();
            initTasDeCartesJeu();
            Nouvelle_Manche_Resume();
            ajouter_Jeu_Resume();
        }

        enAttente++;

        if (enAttente>=(nbHumains+nbRobots)){
            enAttente=0;
            pthread_cond_broadcast(&conds[1]);
        }
        else{
            pthread_cond_wait(&conds[1], &mutexs[1]);
        }
    pthread_mutex_unlock(&mutexs[1]);

    // on récupère les cartes
    recupCartes(infos);

    // on donne les cartes au joueur
    send(infos->socket,infos->cartes,10*sizeof(Carte),0);

    for(int i=10; i>0; i--){
        // on donne le jeu au joueur
        send(infos->socket,&jeu,sizeof(Jeu),0);

        // on récupère l'indice de la carte choisie par le joueur
        int n=recv(infos->socket,&infos->carteChoisie, sizeof(int), 0);
        if (n==-1){
            perror("Erreur, pas d'indice reçu");
        }

        // on attend que tous les joueurs renvoient une carte, le dernier thread fait le tri des cartes, gère les choix supplémentaires...
        pthread_mutex_lock(&mutexs[2]);
            enAttente++;
            if (enAttente>=(nbHumains+nbRobots)){
                enAttente=0;
                ajouter_Choix_Resume();
                gestionJeu(i);
                ajouter_Jeu_Resume();
                pthread_cond_broadcast(&conds[2]);
            }
            else{
                pthread_cond_wait(&conds[2], &mutexs[2]);
            }
        pthread_mutex_unlock(&mutexs[2]);
        
        //on donne le score au joueur
        send(infos->socket,&(infos->score),sizeof(infos->score),0);
    }
}

// méthode pour ajouter les noms des joueurs humains au résumé
void ajoutNomsJoueursResume(){
    char commande[256];
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"Nom(s) du (des) joueur(s) humain(s) de la partie :");
    int n=system(commande);
    if(n!=0){
        perror("Erreur lors de l'ajout des noms des joueurs dans le résumé");
    }
    for(int i=0; i<nbHumains; i++){
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,listeInfosJoueurs[i].nom);
        int n=system(commande);
        if(n!=0){
            perror("Erreur lors de l'ajout des noms des joueurs dans le résumé");
        }
    }
    snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"");
    n=system(commande);
    if(n!=0){
        perror("Erreur dans le résumé lors de l'ajout du numéro de manche");
    }
}

// méthode pour le fonctionnement des threads qui gèrent la communication entre le serveur et un joueur
void *gestionJoueur(void *parametre) {

    infosJoueur* infos=(infosJoueur*) parametre;


    // on récupère le nom du joueur
    int n=recv(infos->socket, infos->nom, sizeof(infos->nom), 0);
    if (n==-1){
        perror("Erreur, pas de nom reçu");
    }
    // on attend que tout le monde rentre son nom
    pthread_mutex_lock(&mutexs[0]);
        enAttente++;
        if (enAttente>=(nbHumains+nbRobots)){
            // le dernier thread ajoute les noms des joueurs au résumé
            ajoutNomsJoueursResume();

            pthread_cond_broadcast(&conds[0]);
            enAttente=0;
        }
        else{
            pthread_cond_wait(&conds[0], &mutexs[0]);
        }
    pthread_mutex_unlock(&mutexs[0]);

    while(!finPartie)
    {
        manche(infos);
        
        //le dernier thread regarde si c'est la fin de la partie, et lance les traitements à faire si nécessaire
        pthread_mutex_lock(&mutexs[3]);
            enAttente++;
            if (enAttente>=(nbHumains+nbRobots)){
                finDePartie();
                ajouter_Scores_Resume();
                pthread_cond_broadcast(&conds[3]);
                enAttente=0;
            }
            else{
                pthread_cond_wait(&conds[3], &mutexs[3]);
            }
        pthread_mutex_unlock(&mutexs[3]);

        // on dit au joueur si la partie est finie ou non
        send(infos->socket,&finPartie,sizeof(finPartie),0);
    }


    // libération de la mémoire du tas de cartes du joueurs
    freeListeCartesJoueur(infos);

    // fermeture du socket
    close(infos->socket);

    pthread_exit(NULL);
}

/********************************************************************
 * FIN MÉTHODES DES THREADS JOUEURS
********************************************************************/
















/********************************************************************
 * MÉTHODES POUR LE MAIN
********************************************************************/

//méthode pour l'initialisation des mutex et des conditions
void initMutexCond(){
    for(int i=0; i<nbMutexsConds; i++){
        pthread_mutex_init(&mutexs[i],NULL);
        pthread_cond_init(&conds[i],NULL);
    }
}

//méthode pour détruire les mutex et les conditions
void destructionMutexCond(){
    for(int i=0; i<nbMutexsConds; i++){
        pthread_mutex_destroy(&mutexs[i]);
        pthread_cond_destroy(&conds[i]);
    }
}

// méthode pour pouvoir rentrer le nombre de joueurs et de robots présents sur la partie
void recupNbRobotsEtHumains(){
    int ok=0;
    while(ok==0){
        printf("Entrez le nombre de robots : ");
        scanf("%d", &nbRobots);
        printf("Entrez le nombre de joueurs : ");
        scanf("%d", &nbHumains);
        if((nbRobots+nbHumains)>10){
            printf("Le nombre de joueurs et de robots ne doit pas dépasser 10 !\n");
        }
        else{
            ok=1;
        }
    }
}

// méthode pour initialiser les informations des joueurs
void initInfosJoueurs(){
    for(int i=0; i<nbHumains;i++){
        listeInfosJoueurs[i].ind=i;
        listeInfosJoueurs[i].score=0;
        listeInfosJoueurs[i].victoire=0;
        listeInfosJoueurs[i].cartes=(Carte*) malloc(10*sizeof(Carte));
    }
}

// méthode pour initialiser les informations des robots
void initInfosRobots(){
    for(int i=0; i<nbRobots;i++){
        listeInfosRobots[i].ind=i;
        listeInfosRobots[i].score=0;
        listeInfosRobots[i].cartes=(Carte*) malloc(10*sizeof(Carte));
    }
}

// méthode pour ajouter les joueurs dans le fichier de statistiques s'ils n'ont encore jamais joué
void ajoutJoueursFichierStats(){
    for(int i=0; i<nbHumains; i++){
        char commande[256];
        snprintf(commande,sizeof(commande),"./Ajout_Joueur.sh %s",listeInfosJoueurs[i].nom);
        int n=system(commande);
        if(n!=0){
            perror("Erreur lors de l'ajout du joueur dans le fichier des statistiques");
        }
    }
}

// méthode pour modifier les statistiques des joueurs dans le fichier
void modifStatsJoueurs(){
    for(int i=0; i<nbHumains; i++){
        char commande[256];
        snprintf(commande,sizeof(commande),"./Modif_Stats_Joueur.sh %s %d %d",listeInfosJoueurs[i].nom,listeInfosJoueurs[i].victoire,listeInfosJoueurs[i].score);
        int n=system(commande);
        if(n!=0){
            perror("Erreur lors de modifications dans le fichier des statistiques");
        }
    }
}

// méthode pour chercher le score le plus petit parmi les joueurs et les robots
int chercherScoreMin(){
    int score=1000000; // valeur impossible à avoir
    
    // on regarde parmi les joueurs
    for(int i=0; i<nbHumains; i++){
        if(listeInfosJoueurs[i].score<score){
            score=listeInfosJoueurs[i].score;
        }
    }

    // idem avec les robots
    for(int i=0; i<nbRobots; i++){
        if(listeInfosRobots[i].score<score){
            score=listeInfosRobots[i].score;
        }
    }

    return score;
}

// méthode pour attribuer la victoire aux joueurs qui ont le plus petit score, elle ajoute le nom des vainqueurs dans le résumé
void attributionVictoire(){
    int score=chercherScoreMin(); // récupération du score le plus petit
    int ok=0; // pour savoir si des humains ont gagné

    // on regarde si l'un des humains a gagné
    for(int i=0; i<nbHumains; i++){
        if(listeInfosJoueurs[i].score==score){
            listeInfosJoueurs[i].victoire=1;
            ok=1;
        }
    }

    if(ok){
        char commande[256];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"Fin de la partie ! Voici le (les) vainqueur(s) :");
        int n=system(commande);
        if(n!=0){
            perror("Erreur lors de l'ajout des vainqueurs dans le résumé");
        }

        for(int i=0; i<nbHumains; i++){
            if(listeInfosJoueurs[i].victoire==1){
                char commande1[256];
                snprintf(commande1,sizeof(commande1),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,listeInfosJoueurs[i].nom);
                n=system(commande1);
                if(n!=0){
                    perror("Erreur lors de l'ajout des vainqueurs dans le résumé");
                }
            }
        }
    }
    else{
        char commande[256];
        snprintf(commande,sizeof(commande),"./Ajouter_Ligne_Au_Resume.sh %s \"%s\"",nomFichier,"Un robot gagne la partie !");
        int n=system(commande);
        if(n!=0){
            perror("Erreur lors de l'ajout des vainqueurs dans le résumé");
        }
    }
}

// méthode pour rentrer le nom du fichier qui contiendra le résumé de la partie
void rentrerNomFichier(){
    printf("Entrez le nom du fichier qui contiendra le résumé de la partie (il sera présent dans le dossier resumes, PAS D'ESPACES OU DE CARACTÈRES SPÉCIAUX) : ");
    scanf("%s", nomFichier);
}

// méthode pour créer le fichier qui contiendra le résumé de la partie
void creationFichier(){
    char commande[256];
    snprintf(commande,sizeof(commande),"./Creer_Fichier_Resume.sh %s",nomFichier);
    int n=system(commande);
    if(n!=0){
        perror("Erreur lors de la création du fichier contenant le résumé de la partie");
    }
}

// méthode pour transformer le .txt en .pdf
void fichierTxtEnPdf(){
    char commande[256];
    snprintf(commande,sizeof(commande),"./Transformer_Txt_En_Pdf.sh %s",nomFichier);
    int n=system(commande);
    if(n!=0){
        perror("Erreur lors de la création du fichier contenant le résumé de la partie en pdf");
    }
}
/********************************************************************
 * FIN MÉTHODES POUR LE MAIN
********************************************************************/

















int main(){

    // on remplit la liste de cartes
    remplirListeCartes();

    // on récupère les nombres d'humains et de robots de la partie 
    recupNbRobotsEtHumains();

    // on récupère le nom du fichier qui contiendra le résumé de la partie
    rentrerNomFichier();

    // on crée le .txt qui nous permettra de garder le résumé de la partie
    creationFichier();

    // on initialise les informations des joueurs
    initInfosJoueurs();

    // même chose avec les robots
    initInfosRobots();

    // initialisation des variables pour la synchronisation des threads
    initMutexCond();

    // création des robots
    pthread_t threadsRobots[nbRobots];
    for(int i=0; i<nbRobots; i++){
        if (pthread_create(&threadsRobots[i], NULL, gestionRobot, &listeInfosRobots[i])!=0) {
            perror("Erreur lors de la création du robot");
        }
    }

    // on prépare la création du serveur
    int listenSocket;
    struct sockaddr_in serverAddr;
    pthread_t clientThreads[nbHumains];
    int nbConnectes=0;

    // création du socket d'écoute
    listenSocket=socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket==-1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // configuration de l'adresse du serveur
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=INADDR_ANY;
    serverAddr.sin_port=htons(9001);

    // liaison du socket à l'adresse
    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))==-1){
        perror("Erreur lors de la liaison du socket");
        exit(EXIT_FAILURE);
    }

    if (listen(listenSocket, nbHumains)==-1){
        perror("Erreur lors de l'écoute des connexions");
        exit(EXIT_FAILURE);
    }

    while (nbConnectes<nbHumains) {
        struct sockaddr_in clientAddr;
        socklen_t l=sizeof(clientAddr);

        // on attend la connexion d'un joueur
        int clientSocket=accept(listenSocket, (struct sockaddr *)&clientAddr, &l);
        if (clientSocket==-1) {
            perror("Erreur lors de l'acceptation de la connexion");
            continue;
        }

        printf("Nouveau Joueur connecté\n");

        listeInfosJoueurs[nbConnectes].socket=clientSocket;

        // création du thread pour s'occuper du joueur
        if (pthread_create(&clientThreads[nbConnectes], NULL, gestionJoueur, &listeInfosJoueurs[nbConnectes])!=0) {
            perror("Erreur lors de la création du thread");
            close(clientSocket);
        }
        else {
            nbConnectes++;
        }
    }

    // fermeture du socket d'écoute (tous les joueurs sont connectés, pas de gestion de reconnection)
    close(listenSocket);

    // on attend que les threads se terminent
    for(int i=0; i<nbHumains; i++){
        pthread_join(clientThreads[i],NULL);
    }
    for(int i=0; i<nbRobots; i++){
        pthread_join(threadsRobots[i],NULL);
    }

    // ajout des joueurs dans le fichier de statistiques s'ils n'y sont pas encore
    ajoutJoueursFichierStats();
    
    // on attribue la victoire aux joueurs qui ont le plus petit score
    attributionVictoire();

    // on modifie les statistiques des joueurs
    modifStatsJoueurs();

    // on transforme le .txt du fichier contenant le résumé de la partie en .pdf
    fichierTxtEnPdf();

    // libération de la mémoire...
    freeListeCartes();
    destructionMutexCond();

    return 0;
}