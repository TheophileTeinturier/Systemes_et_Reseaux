#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

char nom[100]; // nom du joueur
int score=0; // score du joueur
int finPartie=0; // pour savoir si la partie est finie ou non

// pour la connexion avec le serveur
int clientSocket;
struct sockaddr_in serverAddr;


// structure pour les cartes
typedef struct{
    int numero;
    int nbTetes;
}Carte;

// structure pour gérer les tas de cartes "sur la table"
typedef struct{
    int nbCartesTas[4]; // tableau qui stocke le nombre de cartes dans chaque tas
    Carte tab[4][5]; // tableau qui stocke les cartes de nos tas
}Jeu;


// méthode pour que le joueur puisse rentrer son nom
void entreeNom(){

    // on rentre le nom du joueur
    printf("Veuillez rentrer votre nom : \n");
    scanf("%s",nom);
    send(clientSocket,nom,strlen(nom),0);
}

// méthode pour l'affichage de la liste de carte du joueur
void affichageListeCarte(Carte* listeCartes, int n){
    printf("\n");
    for (int i=0; i<n; i++){
        printf("Carte %d : [%d, %d]\n",i,listeCartes[i].numero,listeCartes[i].nbTetes);
    }
    printf("\n");
}

// méthode pour afficher une carte
void afficheCarte(Carte c){
    printf("[%d , %d]",c.numero,c.nbTetes);
}

// méthode pour afficher le jeu
void afficheJeu(Jeu jeu){

    for(int i=0; i<4; i++){
        printf("Tas n%d : ",i);
        for(int j=0; j<jeu.nbCartesTas[i]; j++){
            afficheCarte(jeu.tab[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// méthode pour récupérer l'indice de la carte choisie par le joueur
int recupIndiceCarte(int nbCartes){
    int i;
    printf("Veuillez rentrer l'indice de la carte choisie :\n");
    scanf("%d",&i);
    return i;
}

// méthode pour récupérer l'indice du tas
int recupIndiceTas(){
    int i;
    scanf("%d",&i);
    return i;
}

// méthode pour "enlever" la carte choisie par le joueur
void enleveCarte(Carte* liste, int ind, int nbCartes){
    liste[ind].numero=liste[nbCartes].numero;
    liste[ind].nbTetes=liste[nbCartes].nbTetes;
}

// méthode pour jouer une manche de la partie
void manche(){
    
    // on récupère la liste de cartes
    Carte listeCartes[10];
    int n=recv(clientSocket, listeCartes, sizeof(listeCartes), 0);
    if (n==-1){
        perror("Erreur, pas de liste reçu");
    }

    printf("NOUVELLE MANCHE !\n\n");

    for(int i=10; i>0; i--){
        affichageListeCarte(listeCartes,i);

        // on récupère le jeu
        Jeu j;
        n=recv(clientSocket,&j,sizeof(j),0);
        if (n==-1){
            perror("Erreur, pas de jeu reçu");
        }

        // on affiche le jeu
        afficheJeu(j);

        // on donne l'indice de la carte choisie
        int ind=recupIndiceCarte(i);
        send(clientSocket,&ind,sizeof(int),0);

        // on regarde si le joueur doit choisir un tas
        int choix;
        n=recv(clientSocket,&choix,sizeof(choix),0);
        if (n==-1){
            perror("Erreur, pas de message reçu");
        }

        // cas où le joueur doit faire un choix
        if(choix==1){
            // on récupère le jeu et on l'affiche avec un message
            n=recv(clientSocket,&j,sizeof(j),0);
            if (n==-1){
                perror("Erreur, pas de jeu reçu");
            }
            printf("\nVotre carte est trop petite pour être placée sur un tas, veuillez choisir un tas : \n");
            afficheJeu(j);

            // on récupère l'indice du tas
            int t=recupIndiceTas();

            // on donne la réponse au serveur
            send(clientSocket,&t,sizeof(t),0);
        }

        // on modifie la liste de cartes
        enleveCarte(listeCartes,ind,i-1);

        // on récupère notre score
        n=recv(clientSocket,&score,sizeof(score),0);
        if (n==-1){
            perror("Erreur, pas de score reçu");
        }

        printf("Votre score : %d\n",score);
    }

    // on regarde si la partie est finie
    n=recv(clientSocket,&finPartie,sizeof(finPartie),0);
        if (n==-1){
            perror("Erreur, pas de message reçu pour savoir si la partie est finie");
        }
}

int main(int argc, char *argv[]) {

    // Création du socket
    clientSocket=socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket==-1){
        perror("Erreur lors de la création du socket client");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(9001);
    serverAddr.sin_addr.s_addr=inet_addr(argv[1]);

    // Connexion au serveur
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))==-1){
        perror("Erreur lors de la connexion au serveur");
        exit(EXIT_FAILURE);
    }

    //on fait rentrer le nom du joueur
    entreeNom();

    while(!finPartie){
        manche();
    }

    // Fermeture du socket client
    close(clientSocket);

    return 0;
}
