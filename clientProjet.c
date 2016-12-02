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

/* Client TCP */
/*	TODO :
	- Recuperer le numero de port et l'adresse IP
	- Construire la requete /!\ C'est du char** cf TD /!\
	- Coder le traitement
*/

int main(int argc, char* argv[]){
	struct sockaddr_in dest; /* Socket d'envoi */
	struct addrinfo *result; /* Infos du serveur */
	int sock; /* Descripteur du socket d'envoi */
	int port; /* Port du serveur */
	int sockaddr_in_size; /* Taille de la struct sockaddr_in */
	char *response; /* Stocke la reponse du serveur */
	char *envoi; /* Stocke le message d'envoi au serveur */
	
	/* Verification des arguments */
	
	if(argc != 3){
	  perror("[ERREUR] Nombre args ! \n");
	}
	
	/* Recuperation des arguments (IP, port), malloc... */
	port = atoi(argv[2]);
	sockaddr_in_size = sizeof(dest);
	response = malloc(sizeof(char) * TAILLE_MSG);
	envoi = malloc(sizeof(char) * TAILLE_MSG); /* 48 */
	/* strcpy(envoi, argv[3]); */
	read(STDIN_FILENO, envoi, TAILLE_MSG);
	
	/* Creation de la socket du client */
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Erreur socket ! \n");
		return EXIT_FAILURE;
	}
	
	/* Initialisation de la structure du destinataire */
	if((getaddrinfo(argv[1], NULL, NULL, &result)) != 0){
		perror("Erreur getaddrinfo ! \n");
		return EXIT_FAILURE;
	}
	
	memset((char *)&dest, 0, sizeof(dest));
	dest.sin_addr = ((struct sockaddr_in*)result->ai_addr)->sin_addr;
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);
	
	/* Creation de la connection au serveur */
	if(connect(sock, (struct sockaddr *)&dest, sizeof(dest)) == -1){
		perror("Erreur connect ! \n");
		return EXIT_FAILURE;
	}

	printf("[DEBUG] Ce que le client va envoyer : %s \n", envoi);
	/* Envoie au serveur du nom du fichier */
	if(write(sock, (char *) envoi, strlen(envoi)) == -1){
		perror("Erreur write ! \n");
		return EXIT_FAILURE;
	}
	printf("[DEBUG] Le client a envoye sa requete ! \n");
	
	/* Attente de la reponse du serveur */
	if(read(sock, response, (sizeof(char) * TAILLE_MSG)) == -1){
		perror("Erreur server first response ! \n");
		return EXIT_FAILURE;
	}
	printf("[DEBUG] Le client a recu la reponse : %s \n", response);
	
	/* Traitement */
	
	/* Fermeture du socket */
	free(response);
	shutdown(sock, 2);
	close(sock);
	printf("[DEBUG] Fermeture de la socket et liberation memoire : OK ! \n");
	return EXIT_SUCCESS;
}
