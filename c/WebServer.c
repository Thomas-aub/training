// Thomas AUBOURG G3S4


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>          
#include <assert.h>



/* Lit la requete du client. Met le nom du fichier demande dans string.
* Si la syntaxe est incorrecte ou si il manque des retour charriots
* on renvoi -1. Autrement la fonction renvoie 0.
* requestFromClient est la chaine de 1000 octets censee contenir la requete provenant du client.
* requestSize doit etre egale a 1000 (et pas a la taille de la chaine de caractere). 
*/

int parseRequest(char* requestFromClient, int requestSize, char* string, int stringSize)
{
    /* charPtr[4] est un tableau de 4 pointeurs pointant sur le debut de la chaine, les 2 espaces   */
    /* de la requete (celui apres le GET et celui apres le nom de fichier) et sur le premier '\r'.  */
    /* Le pointeur end sera utilise pour mettre un '\0' a la fin du doubl retour charriot.      */

    char *charPtr[4], *end;

    /* On cherche le double retour charriot dans requestFromClient
    * suivant les systemes, on utilise \r ou \n (new line, new feed)
    * par convention en http on utilise les deux \r\n mais cela represente en pratique un seul retour charriot.
    * Pour simplifier ici, on ne recherche que les '\n'.
    * On placera un '\0' juste apres le double retour charriot permettant de traiter la requete 
    * comme une chaine de caractere et d'utiliser les fcts de la bibliotheque string.h. 
    */

    /* Lecture jusqu'au double retour charriot  */
    requestFromClient[requestSize-1]='\0';//Permet d'utiliser strchr() - attention ne marche pas si requestSize indique la taille de la chaine de caractere

    if( (end=strstr(requestFromClient,"\r\n\r\n"))==NULL) return(-1);
    *(end+4)='\0';
    
    // Verification de la syntaxe (GET fichier HTTP/1.1)        
    charPtr[0]=requestFromClient;   //Debut de la requete (GET en principe)
    //On cherche le premier espace, code ascii en 0x20 (en hexa), c'est le debut du nom du fichier
    charPtr[1]=strchr(requestFromClient,' ');   
    if(charPtr[1]==NULL) return(-1);
    charPtr[2]=strchr(charPtr[1]+1,' ');    
    if(charPtr[2]==NULL) return(-1);
    charPtr[3]=strchr(charPtr[2]+1,'\r');   
    if(charPtr[3]==NULL) return(-1);

    //On separe les chaines
    *charPtr[1]='\0';
    *charPtr[2]='\0';
    *charPtr[3]='\0';

    if(strcmp(charPtr[0],"GET")!=0) return(-1);
    if(strcmp(charPtr[2]+1,"HTTP/1.1")!=0) return(-1);
    strncpy(string,charPtr[1]+2,stringSize);//On decale la chaine de 2 octets: le premier octet est le '\0', le deuxieme decalage permet de retirer le "/" 

    //Si stringSize n'est pas suffisement grand, la chaine ne contient pas de '\0'. Pour verifier il suffit de tester string[stringSize-1] qui
    // doit etre = '\0' car strncpy remplit la chaine avec des '\0' quand il y a de la place.
    if(string[stringSize-1]!='\0'){
        fprintf(stderr,"Erreur parseRequest(): la taille de la chaine string n'est pas suffisante (stringSize=%d)\n",stringSize);
        exit(3);
    }
    
    //DEBUG - Vous pouvez le supprimer si vous le souhaitez.
    if( *(charPtr[1]+2) == '\0') fprintf(stderr,"DEBUG-SERVEUR: le nom de fichier demande est vide -\nDEBUG-SERVEUR: - on associe donc le fichier par defaut index.html\n");
    else fprintf(stderr,"DEBUG-SERVEUR: le nom de fichier demande est %s\n",string);

    if( *(charPtr[1]+2) == '\0') strcpy(string,"index.html");

    return(0);
}


// part 1


char* checkExtension(char *string, char *success){ //Fonction pour vérifier l'extension

	if (strstr(string, ".png") != NULL){		// On vérifie si string contient png
	
		strcpy(success,"HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n"); 	// Si c'est un point png on modifie le content-type
		
	} else if(strstr(string, ".jpg") != NULL){ 	// On vérifie si string contient jpg
	
		strcpy(success,"HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n\r\n");	// Si c'est un point jpg on modifie le content-type
	}
	
	return success;
	
}

void affichage(int file, char *lect, int nbOctets, int confd){ //Fonction qui read la page

	int rd;

	while ((rd = read(file, lect, nbOctets)) > 0) { 	
			if ( (send(confd, lect, rd, 0)<0)){ perror("Erreur send()"); }	//On envoie les éléments à afficher
	} 
		    
	if (rd <0 ) perror("Error read()"); 
		    
	if (nbOctets < 0) { perror ("error recv"); exit(2); }
	    
}





void client(int confd){
	char buffer[1000], string[1000], lect[1000];
	char success[100] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"; // On initialise le content-type en texte
	int nbOctets, file, result,  code;

	if ((nbOctets = recv(confd, buffer, sizeof(buffer),0)) < 0 ){ perror("Requete trop grande");} 

	if ((result = parseRequest(buffer, sizeof(buffer), string, sizeof(string))) < 0) { 
		fprintf(stderr, "Erreur dans la requete\n"); // On gere les erreurs 400
		strcpy(success, "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/html\r\n\r\n"); //On modifie le content-type pour indiquer l'erreur 400
		file = open("./file400.html", O_RDWR);
	}
	
	checkExtension(string, success); // On appelle une fonction qui vérifie l'extension
	      
	if ((file = open(string, O_RDWR)) < 0){
	 
		perror("Error open()");
		if (errno == EACCES) { 	// Si l'acces à la page est refusée
			strcpy(success,  "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/html\r\n\r\n");	// On modifie le content-type pour indiquer l'erreur 500
			file = open("./file500.html", O_RDWR);	// On redirige vers la page d'erreurs 500
		}else {	// On gere les erreurs 404
			strcpy(success, "HTTP/1.1 404 FILE NOT FOUND\r\nContent-Type: text/html\r\n\r\n");	// On modifie le content-typepour indiquer l'erreur 404
			file = open("./file404.html", O_RDWR);	// On redirige vers la page d'erreurs 404
		}
	}
	
	if ( (code = send(confd, success, strlen(success), 0))<0) perror("Erreur send()");	// On envoie le content-type
	
	affichage(file, lect, nbOctets, confd);   // On appelle une fonction qui read et le contenue de la page 
	
	 close(confd); 
}







int main(int argc, char* argv[]){

    struct addrinfo hints, *res;
    int sockfd, error, confd,  tr, retourFork, status; 
    tr=1 ;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6; 	//IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; 	//TCP
    hints.ai_flags = AI_PASSIVE; 	//Permet d'associer la socket a toutes les adresses locales

    if((error = getaddrinfo(NULL,"80",&hints,&res))!=0)  gai_strerror(error);

    if((sockfd=socket(res->ai_family,res->ai_socktype,res->ai_protocol))<0) perror("Erreur socket():");

    
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) perror("Erreur setsockopt() SO_REUSEADDR"); 
    
    if(bind(sockfd, res->ai_addr, res->ai_addrlen)<0) {perror("Erreur bind()");return(2);}

    if(listen(sockfd, 1)<0) {perror("Erreur listen()"); return(2);}
    
    while ((confd = accept(sockfd, NULL, NULL))>0){ 
    
    	if ((retourFork = fork()) < 0) { perror("Erreur fork()"); return(2); } // On fork à chaque connection
    	
    	if (retourFork == 0){ client(confd); } 	// Si on est dans un processus fils on appelle une fonction qui gere le client
	   
	if  (retourFork > 0){				// si on est dans le processus pere
		close(confd);				// on ferme la socket du serveur
		waitpid(-1, &status, WNOHANG); 	// on gere les zombies
	}
	
    }
	
    close(sockfd); 	// on ferme la socket
    
    freeaddrinfo(res); 	//Ne pas oublier de libérer la liste chaînée
    
    return 0;

}