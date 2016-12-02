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
#define NOM_LOG "tmp/http3300807.log"

/* TODO :
   - Q1 Faire le mime.
   - Q1 Regler l'erreur de segmentation quand deux clients a la suite se connectent.
   - Q1 Regler le clase du close(resultat) si le fichier n'existe pas / n'a pas les droits.

   - Q2 Ecrire dans le log.
*/


char * get_mime_type(char* path){
  char * content_type = (char *) malloc (sizeof(char) * 32);
  char * line = (char *) malloc (sizeof(char) * 256); /* Buffer pour chaque ligne */
  FILE * fic;
  int read = 0;
  int i = 0;
  size_t size = 256;
  char * token = malloc (256 * sizeof(char));
  char * error = "Error (Incompatible or wrong extension)";
  fic = fopen("/etc/mime.types", "r");

  if (fic == NULL){
    perror("Erreur fichier /etc/mime.types\n");
    return NULL;
  }


  while (path[0] != '.'){ /* Permet d'avancer dans le chemin jusqu'a l'extension */
    path++;
  }

  path++ ; /* On se decale a 1 char apres le . */

  while (getline(&line,&size,fic) != -1){ /* Boucle sur toutes les lignes deu fichier */
    if (line[0] == 't'){ /* Detecte les lignes "text/"  */
      token = strtok(line," \t\r\n\v\f"); /* Eclate les lignes */
      strcpy(content_type,token);
      while (token != NULL){
        if (strcmp(token,path)==0){
           printf("--------------> %s\n",content_type);
           return content_type; /* Compare extension et token */
        }
        token = strtok(NULL, " \t\r\n\v\f");
      }
    }
    i++;

  }

  return error;

}






pthread_mutex_t mutex_fichier;

/* Fonction d'analyse de requete */
int analyse_requete(char *msg, int *resultat){
  char *message; /* Stocke le message */
  char *token[10]; /* Stocke des parties de la requetes */
  char *chemin;
  char delimiteurs[]=" \t\r\n\v\f";  /* Stocke les delimiteurs */
  int flag_requete; /* Restera a 1 si la requete est syntaxiquement correct */
  int i;
  
  /* Valeur des tokens utilise par strtok :
     "GET " = token[0],
     /chemin = token[1],
     "HTTP/1.1" = token[2]
     "Host:" = token[3]
     "127.0.0.1" = token[4]
   */

  /* Initialisation des variables */
  flag_requete = 1;
  i = 0;
  message = malloc(sizeof(char) * strlen(msg));
  chemin = malloc(sizeof(char) * (strlen(msg)-1));
  strcpy(message, msg); /* On est oblige car strtok modifie la 'source' */
  printf("[DEBUG] Toutes les initilisations d'analyse_requete : OK ! \n");
  printf("[DEBUG] String qui va etre utilisee par strtok() : %s \n", message);

  /* 'Decoupage' de la requete */
  token[i] = strtok(message, delimiteurs); /* Prend le "GET " */
  while(token[i] != NULL){
    printf("[DEBUG] token[%d] = %s \n", i, token[i]);
    i++;
    token[i] = strtok(NULL, delimiteurs);
  }
  
  /* Comparaison de chaque tokens */
  if(strcmp(token[0], "GET") != 0){
    printf("[ERREUR] Methode ! \n");
    flag_requete = 0;
  }
  
  if(strcmp(token[2],"HTTP/1.1") != 0){
    printf("[ERREUR] Protocole !\n");
    flag_requete = 0;
  }
  
  if(strcmp(token[3],"Host:") != 0){
    printf("[ERREUR] Host ! \n");
    flag_requete = 0;
  }
  
  if(strcmp(token[4],"127.0.0.1") != 0){
    printf("[ERREUR] Adresse ! \n");
    flag_requete = 0;
  }

  /* Si flag_requete != 1 alors la requete n'est pas bien formee */
  if(flag_requete != 1){
    printf("[ERREUR] Requete mal formee ! \n");
    return -1;
  }

  chemin = token[1] + 1;
  printf("[DEBUG] Nom du fichier trouve %s ! \n", chemin);
  
  /* Verification de l'existence et des droits sur le fichier */
  if(access(chemin, R_OK) != 0){
    if(errno == ENOENT){
      printf("[ERREUR] Fichier introuvable ! \n");
      return 404;
    }else if(errno == EACCES){
      printf("[ERREUR] Permission refusée ! \n");
      return 403;
    }
  }
  printf("[DEBUG] Existence et droits du fichier : OK ! \n");
  *resultat = open(chemin, O_RDONLY);
  return 200;
}

void* run(void *arg){
  int *sock_com_thread = (int *)arg; /* Descripteur de la socket de communication */
  int resultat; /* Stocke le descripteur de fichier */
  int resultat_fonction; /* Retour de la fonction d'analyse */
  int fd; /* Descripteur du fichier log */
  char msg[200]; /* Stocke la requete du client */
  char *envoi; /* Stocke la reponse que l'on va envoyer au client */
  char *lecture; /* Stocke le contenu du fichier */

  
  fd = open(NOM_LOG, O_CREAT | O_SYNC | O_RDWR, 0600);
  if(fd < 0){
    perror("[ERREUR] open() du fichier log ! \n");
    return;
  }
  printf("[DEBUG] Ouverture du fichier log : OK ! \n");
  
  /* Reception de la requete du client */
  if(read(*sock_com_thread, msg, (sizeof(char) * TAILLE_MSG)) < 0){
    perror("[ERREUR] read() chemin ! \n");
    return;
  }
  printf("[DEBUG] Thread %d | Reception d'une requete du client : %s \n", (int)pthread_self(), msg);
  
  /* Traitement */
  resultat_fonction = analyse_requete(msg, &resultat);
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

  /* TODO : Ajout dans le log */
  pthread_mutex_lock(&mutex_fichier);
  if(0){
  
  }
  
  pthread_mutex_unlock(&mutex_fichier);
  
  /* Liberation des ressources */
  free(envoi);
  free(lecture);
  close(resultat); /* TODO : /!\ */
  close(fd);
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
  pthread_mutex_init(&mutex_fichier, NULL);
	
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
