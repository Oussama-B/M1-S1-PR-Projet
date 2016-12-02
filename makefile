all: ./Serveur/serveurProjet ./Client/clientProjet

./Serveur/serveurProjet: ./Serveur/serveurProjet.c
	gcc -lpthread -o ./Serveur/serveurProjet ./Serveur/serveurProjet.c

./Client/clientProjet: ./Client/clientProjet.c
	gcc -o ./Client/clientProjet ./Client/clientProjet.c
