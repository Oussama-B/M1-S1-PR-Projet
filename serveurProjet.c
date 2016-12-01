#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#define TAILLE_MSG 256
#define TAILLE_CONTENU 4096
#define TAILLE_FILE 5

/* TODO :
   - Finir l'analyse de requete
   - case 200 (completer)
   - Faire le client et tester la Q1
*/

/* Fonction d'analyse de requete */
int analyse_requete(char *msg, int *resultat){
  int resultat_analyse1; /* Analyse de "GET /" */
  int resultat_analyse2; /* Analyse de " HTTP/1.1 \n Host: 127.0.0.1 \n \n" */
  int fd; /* Descripteur du fichier demande */
  int i, j;
  char debut_msg[5];
  char nom_fichier[TAILLE_MSG];

  j = 0;

  /* Decoupage pour la premiere analyse */
  for(i = 0; i < 5; i++){
    debut_msg[i] = msg[i];
  }

  /* Premiere analyse */
  resultat_analyse1 = strcmp(debut_msg, "GET /");
  if(resultat_analyse1 != 0){
    printf("[ERREUR] Mauvaise syntaxe de requete ! \n");
    return -1;
  }
  printf("[DEBUG] Premiere analyse : OK ! \n");

  /* TODO : Faire les autres analyses */

  /* Decoupage pour trouver le nom du fichier */
  while(msg[i] != ' '){
    nom_fichier[j] = msg[i];
    j++;
    i++;
    if(j == TAILLE_MSG){
      printf("[ERREUR] Nom du fichier trop long ! \n");
      return -1;
    }
  }
  nom_fichier[j] = '\0';
  printf("[DEBUG] Nom du fichier trouve %s ! \n", nom_fichier);

  /* Verification de l'existence et des droits sur le fichier */
  if(access(nom_fichier, R_OK) != 0){
    if(errno == ENOENT){
      printf("[ERREUR] Fichier introuvable ! \n");
      return 404;
    }else if(errno == EACCES){
      printf("[ERREUR] Permission refusée ! \n");
      return 403;
    }
  }
  printf("[DEBUG] Existence et droits du fichier : OK ! \n");
  *resultat = open(nom_fichier, O_RDONLY);
  return 200;
}

void* run(void *arg){
  int *sock_com_thread = (int *)arg; /* Descripteur de la socket de communication */
  int resultat; /* Stocke le descripteur de fichier */
  int resultat_fonction; /* Retour de la fonction d'analyse */
  char **msg; /* Stocke la requete du client */
  char *envoi; /* Stocke la reponse que l'on va envoyer au client */
  char *lecture; /* Stocke le contenu du fichier */
  
  *msg = malloc(sizeof(char) * TAILLE_MSG);
  
  /* Reception de la requete du client */
  if(read(*sock_com_thread, *msg, (sizeof(char) * TAILLE_MSG)) < 0){
    perror("[ERREUR] read() chemin ! \n");
    return;
  }
  printf("[DEBUG] Thread %d | Reception d'une requete du client : %s \n", (int)pthread_self(), *msg);
  
  /* Traitement */
  resultat_fonction = analyse_requete(*msg, &resultat);
  switch(resultat_fonction){
  case 200:
    envoi = malloc(sizeof(char) * TAILLE_CONTENU);
    lecture = malloc(sizeof(char) * TAILLE_CONTENU);
    strcpy(envoi, "HTTP/1.1 200 OK \nContent-Type: text/html \n\n");

    printf("[DEBUG] Descripteur du fichier : %d ! \n", resultat);
    if(read(resultat, lecture, TAILLE_CONTENU) < 0){
      perror("[ERREUR] Lecture du fichier ! \n");
      return;
    }
    strcat(envoi, lecture);

    printf("Le serveur va repondre : %s \n", envoi);
    if(write(*sock_com_thread, (char *) envoi, strlen(envoi)) == -1){
      perror("[ERREUR] write() 200 ! \n");
      return;
    }
    printf("[DEBUG] Thread %d | Envoie de la reponse 200 au client reussie : OK ! \n", (int)pthread_self());
    break;
  case 403:
    if(write(*sock_com_thread, "HTTP/1.1 403 Forbidden \n", 25) == -1){
      perror("[ERREUR] write() 403 ! \n");
      return;
    }
    printf("[DEBUG] Thread %d | Envoie de la reponse 403 au client reussie : OK ! \n", (int)pthread_self());
    break;
  case 404:
    if(write(*sock_com_thread, "HTTP/1.1 404 Not Found \n", 25) == -1){
      perror("[ERREUR] write() 404 ! \n");
      return;
    }
    printf("[DEBUG] Thread %d | Envoie de la reponse 404 au client : OK ! \n", (int)pthread_self());
    break;
  default:
    printf("[ERREUR] Thread %d | WTF ! \n", (int)pthread_self());
    break;
  }
	
  /* Liberation des ressources */
  /* free(msg); */
  close(resultat); /* /!\ */
  shutdown(*sock_com_thread, 2);
  close(*sock_com_thread);
  printf("[DEBUG] Thread %d | Fermeture de la socket et liberation memoire : OK ! \n", (int)pthread_self());
  return;
}

int main(int argc, char* argv[]){
  struct sockaddr_in serv; /* Socket du serveur */
  struct sockaddr_in exp; /* Socket du client */
  int sock; /* Descripteur de la socket serveur */
  int sock_com; /* Descripteur de la socket de communication */
  int *psock_com; /* Eviter les problemes de concurrence */
  int port; /* Port serveur a associer a la socket */
  int sockaddr_in_size; /* Taille de la struct sockaddr_in */
  int *thread_number; /* ID d'un thread */
  int nbMaxClients; /* Nombre max de clients en simultane */
	
  /* Verification des arguments */
  if(argc != 3){
    perror("[ERREUR] Nombre args ! \n");
    return EXIT_FAILURE;
  }
	
  /* Recuperation des arguments (port), malloc... */
  sockaddr_in_size = sizeof(serv);
  *thread_number = 0;
  port = atoi(argv[1]);
  nbMaxClients = atoi(argv[2]);
	
  /* Creation de la socket du serveur */
  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("[ERREUR] socket() ! \n");
    return EXIT_FAILURE;
  }
  printf("[DEBUG] Creation de la socket : OK ! \n");
	
  /* Init de la structure du socket du serveur */
  memset((char *)&serv, 0, sizeof(serv));
  serv.sin_addr.s_addr = htonl(INADDR_ANY);
  serv.sin_port = htons(port);
  serv.sin_family = AF_INET;
  printf("[DEBUG] Initialisation de la structure du socket du serveur : OK ! \n");
	
  /* Association du socket et du port du serveur */
  if(bind(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0){
    perror("[ERREUR] bind() ! \n");
    return EXIT_FAILURE;
  }
  printf("[DEBUG] Binding : OK ! \n");
	
  /* Creation de la file d'attente */
  listen(sock, nbMaxClients);
  printf("[DEBUG] Creation de la file d'attente : OK ! \n");
  
  /* Boucle infini de traitement */
  while(1){
    /* Attente d'une connexion (bloquant) */
    if((sock_com = accept(sock, (struct sockaddr *)&exp, &sockaddr_in_size)) == -1){
      perror("[ERREUR] accept() ! \n");
      return EXIT_FAILURE;
    }
    psock_com = (int *)malloc(sizeof(sock_com));
    *psock_com = sock_com;
    printf("[DEBUG] Connexion d'un client : OK ! \n");
	
    /* Creation du thread qui va traiter les requetes du client */
    if(pthread_create(&thread_number, NULL, run, (void *)psock_com) != 0){
      fprintf(stderr, "[ERREUR] create() ! \n");
      return EXIT_FAILURE;
    }
    printf("[DEBUG] Creation du thread : OK ! \n");
    *thread_number++;
  }
	
  /* Dead code */
  close(sock);
  return EXIT_FAILURE;
}
