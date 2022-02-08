/*

	Name: Lab 9 // Web Server // Final Project
	Author: Jacob Page
	Simple text chat server using threads

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "sockettome.h"

#include "/home/jplank/cs360/include/jrb.h"
#include "/home/jplank/cs360/include/fields.h"
#include "/home/jplank/cs360/include/dllist.h"

pthread_mutex_t LOCKS[999];

char ROOM_NAMES[32][999];
int ROOMWRITE[999];

Dllist ROOM_CHAT[999];
Dllist ROOM_MEMBERS[999];

typedef struct { //Structure for holding the information on a person including their name, their room, and their fd
	int fd;
	int roomNum;
	char name[32];
} Person;

typedef struct { //Structure used to hold the information needed to correctly display chats on chatroom threads
	Person person;
	char chat[512];
} ChatLog;

Person ALL_PEOPLE[999][999];
int NUM_PEOPLE[999];

/**
* @name chatroom_thread.
* @brief thread used to manage the chatroom and sending chats to everyone in that room.
* @param[in] a void* v that contains the number that chatroom is.
* @return nothing but just returns once the thread should exit.
*/
void* chatroom_thread(void* v) {

	int chatroomNum = *(int*) v;

	int* fdp, fd;
	Dllist tmp, temp;
	fdp = (int*) malloc(sizeof(int));

	FILE* fout;

	while(1) {
		if(ROOMWRITE[chatroomNum] != 0) {
			pthread_mutex_lock(&LOCKS[chatroomNum]);

			dll_traverse(tmp, ROOM_CHAT[chatroomNum]) {
				ChatLog* thisChat = (ChatLog*) tmp->val.v;
				char* message = thisChat->chat;
				char* person = thisChat->person.name;

				char actualMessage[512];
				memset(actualMessage, 0, 512);

				if(strcmp(message, "Joined!010") == 0) { //A user has joined and the chatroom has recieved the join code to print the join message
					strcpy(actualMessage, person);
					strcat(actualMessage, " has joined\n");

					dll_traverse(temp, ROOM_MEMBERS[chatroomNum]) {
						Person* thisPerson = (Person*) temp->val.v;
						fd = thisPerson->fd;
						*fdp = fd;
						if(fd != -1) {
							fout = fdopen(*fdp, "w");
							fputs(actualMessage, fout);
							fflush(fout);
						}
					}
				} else if (strcmp(message, "Exited!010") == 0) { //A user had left and the chatroom has recieved the exited code to print the leave message
					strcpy(actualMessage, person);
					strcat(actualMessage, " has left\n");

					dll_traverse(temp, ROOM_MEMBERS[chatroomNum]) {
						Person* thisPerson = (Person*) temp->val.v;
						fd = thisPerson->fd;
						*fdp = fd;
						if(fd != -1) {
							fout = fdopen(*fdp, "w");
							fputs(actualMessage, fout);
							fflush(fout);
						}
					}
				} else if (strcmp(message, "NULL") != 0) { //If the message has not already been printed, then print the message and mark as printed
					strcpy(actualMessage, person);
					strcat(actualMessage, ": ");
					strcat(actualMessage, message);

					dll_traverse(temp, ROOM_MEMBERS[chatroomNum]) { //Writes the message to all people in that chatroom
						Person* thisPerson = (Person*) temp->val.v;
						fd = thisPerson->fd;
						*fdp = fd;
						if(fd != -1) {
							fout = fdopen(*fdp, "w");
							fputs(actualMessage, fout);
							fflush(fout);
						}
					}
				}
				strcpy(thisChat->chat, "NULL");
				temp->val.v = thisChat;
			}
			ROOMWRITE[chatroomNum] = 0;
			pthread_mutex_unlock(&LOCKS[chatroomNum]);
		}
	}
}

/**
* @name client_thread.
* @brief thread used to manages its client and send the chat information to the chatroom thread
* @param[in] a void* v that contains the information for that person.
* @return nothing but just returns once the thread should exit.
*/
void* client_thread (void* v) {
	Person* thisPerson = (Person*) v;
	Dllist tmp;

	int* fdp, i, j;
	fdp = (int*) malloc(sizeof(int));
	int fd = thisPerson->fd;
	*fdp = fd;
	int fixError = 0;

	FILE* fin = fdopen(*fdp, "r");

	while(1) {
		usleep(10000);
		char fixMessage[100][512];
		char readMessage[512];
		memset(readMessage, 0, 512);
		memset(fixMessage, 0, 512);

		if(fgets(readMessage, 512, fin)) { //Waits for the user to input chat information
			int numberFix = 0;
			for(i = 0; i < (int) strlen(readMessage); i++) {
				if(readMessage[i] == '\n' && readMessage[i+1] != '\0') {
					fixError = 1;
					break;
				}
			}

			if(fixError == 1) { //Incase the fgets takes in two or more lines of input from one user this will split the messages how they are supposed to be
				char* ptr = strtok(readMessage, "\n");
				while(ptr != NULL) {
					strcpy(fixMessage[numberFix], ptr);
					fixMessage[numberFix][(int) strlen(fixMessage[numberFix])] = '\n';
					numberFix++;
					ptr = strtok(NULL, "\n");
				}
			}

			ChatLog* newChat; //Creates the chat log
			newChat = (ChatLog*) malloc(sizeof(ChatLog));
			newChat->person = *thisPerson;
			strcpy(newChat->chat, readMessage);

			pthread_mutex_lock(&LOCKS[thisPerson->roomNum]);
			ROOMWRITE[thisPerson->roomNum] = 1;
			if(fixError == 1) { //There was a problem with the chat message so multiple will need to be added
				ChatLog* fixChat[100];

				for(i = 0; i < numberFix; i++) {
					fixChat[i] = (ChatLog*) malloc(sizeof(ChatLog));
					for(j = 0; j < (int) strlen(fixMessage[i]); j++) {
						if(fixMessage[i][j] == '\n' && fixMessage[i][j + 1] != '\0') {
							fixMessage[i][j + 1] = '\0';
						}
					}
					fixChat[i]-> person = *thisPerson;
					strcpy(fixChat[i]->chat, fixMessage[i]);
					dll_append(ROOM_CHAT[thisPerson->roomNum], new_jval_v((void*) fixChat[i]));
				}
				fixError = 0;
			} else { //There was no problem so just upload the message as normal
				dll_append(ROOM_CHAT[thisPerson->roomNum], new_jval_v((void*) newChat));
			}
			pthread_mutex_unlock(&LOCKS[thisPerson->roomNum]);

		} else { //Detected that the user left
			ChatLog* exitLog;
			exitLog = (ChatLog*) malloc(sizeof(ChatLog));
			exitLog->person = *thisPerson;
			strcpy(exitLog->chat, "Exited!010");

			pthread_mutex_lock(&LOCKS[thisPerson->roomNum]);
			for(i = 0; i < NUM_PEOPLE[thisPerson->roomNum]; i++) { //Mark them as null so no messages will be sent to them now

				if(strcmp(thisPerson->name, ALL_PEOPLE[thisPerson->roomNum][i].name) == 0) {
					strcpy(thisPerson->name, "NULL");
					ALL_PEOPLE[thisPerson->roomNum][i] = *thisPerson;
				}
			}

			ROOMWRITE[thisPerson->roomNum] = 1;
			dll_append(ROOM_CHAT[thisPerson->roomNum], new_jval_v((void*) exitLog));

			dll_traverse(tmp, ROOM_MEMBERS[thisPerson->roomNum]) { //Close the fd
				Person* traversePerson = tmp->val.v;

				if(strcmp(traversePerson->name, thisPerson->name) == 0) {
					close(traversePerson->fd);
					traversePerson->fd = -1;
					tmp->val.v = traversePerson;

				}
			}
			pthread_mutex_unlock(&LOCKS[thisPerson->roomNum]); //End this thread since the user has left
			return NULL;
		}
	}
}

/**
* @name server_thread.
* @brief thread used to manage clients connecting to the server.
* @param[in] a void* v that contains the socket information
* @return nothing but just returns once the thread should exit.
*/
void* server_thread(void* v) {
	int *fdp, fd, i;
	int sock = *(int*) v;

	while(1) {
		fd = accept_connection(sock); //Waits for a client to try and connect.
		usleep(100000);
		fdp = (int*) malloc(sizeof(int));
		*fdp = fd;

		printf("Accepted Connection\n");

		FILE* fout = fdopen(*fdp, "w");
		FILE* fin = fdopen(*fdp, "r");

		char* writeMessage = "Chat Rooms:\n\n";
		char* enterName = "Enter your chat name (no spaces):\n";
		char* enterRoom = "Enter chat room:\n";

		char enteredName[32];
		char enteredRoom[32];
		
		memset(enteredName, 0, strlen(enteredName));
		memset(enteredRoom, 0, strlen(enteredRoom));

		fputs(writeMessage, fout);
		 
		JRB printList, temp;
		printList = make_jrb();

		for(i = 0; i < 999; i++) { //Puts all the room names into a jrb so they print out in alphabetical order
			if(strcmp(ROOM_NAMES[i], "") == 0) break;

			if(strcmp(ROOM_NAMES[i], "NULL") != 0) {
				jrb_insert_str(printList, ROOM_NAMES[i], new_jval_s(strdup(ROOM_NAMES[i])));
			}
		}

		jrb_traverse(temp, printList) { //Traves the tree and prints out the room name and every person currently in that room
			char* roomName = (char*) temp->val.s;
			char roomLine[512];
			int roomNum ;
			memset(roomLine, 0, 512);

			strcpy(roomLine, roomName);
			strcat(roomLine, ":");

			for(i = 0; i < 999; i++) {
				if(strcmp(ROOM_NAMES[i], temp->val.s) == 0) break;
			}
			roomNum = i;

			for(i = 0; i < NUM_PEOPLE[roomNum]; i++) {
				Person person = ALL_PEOPLE[roomNum][i];
				if (strcmp(person.name, "NULL") != 0) {
					strcat(roomLine, " ");
					strcat(roomLine, person.name);	
				}
			}

			strcat(roomLine, "\n");
			printf("%s", roomLine);
			fputs(roomLine, fout);
		}

		fputs("\n", fout);
		fputs(enterName, fout);
		fflush(fout);
		
		fgets(enteredName, 32, fin); //Gets name and room information from the client
		fputs(enterRoom, fout);
		fflush(fout);
		fgets(enteredRoom, 32, fin);

		enteredName[strlen(enteredName) - 1] = '\0';
		enteredRoom[strlen(enteredRoom) - 1] = '\0';

		for (i = 0; i < 999; i++) { //Makes sure they entered a proper room name
			if(strcmp(enteredRoom, ROOM_NAMES[i]) == 0) break;

			if(strcmp(ROOM_NAMES[i], "") == 0) {
				printf("They entered a bad room name\n");
				exit(1);
			}
		}

		Person* newPerson; //Creates the person information to be given to the client thread
		newPerson = (Person*) malloc(sizeof(Person));
		strcpy(newPerson->name, enteredName);
		newPerson->roomNum = i;
		newPerson->fd = fd;

		ChatLog* joinLog;
		joinLog = (ChatLog*) malloc(sizeof(ChatLog));
		joinLog->person = *newPerson;
		strcpy(joinLog->chat, "Joined!010");

		pthread_mutex_lock(&LOCKS[newPerson->roomNum]); //Creates the client thread and sets all information for the chatroom
		ROOMWRITE[newPerson->roomNum] = 1;
		dll_append(ROOM_MEMBERS[newPerson->roomNum], new_jval_v((void*) newPerson));
		ALL_PEOPLE[newPerson->roomNum][NUM_PEOPLE[newPerson->roomNum]] = *newPerson;
		NUM_PEOPLE[newPerson->roomNum]++;
		dll_append(ROOM_CHAT[newPerson->roomNum], new_jval_v((void*) joinLog));
		pthread_mutex_unlock(&LOCKS[newPerson->roomNum]);

		pthread_attr_t attr[1];
		pthread_attr_init(attr);
		pthread_attr_setscope(attr, PTHREAD_SCOPE_SYSTEM);

		//Create client thread fd, name, room#
		pthread_t* tidp;
		tidp = (pthread_t*) malloc(sizeof(pthread_t));
		pthread_create(tidp, attr, client_thread, newPerson);

	}
	return  NULL;
}

int main(int argc, char** argv) {

	int sock, i;
	pthread_t* tidp;
	pthread_attr_t attr[999];
	
	if (argc < 3) { //Makes sure this is enough information to properly start the server
		fprintf(stderr, "Not enough commands!\n");
		exit(1);
	}

	for(i = 1; i != argc; i++) { //Makes the intial list of all the room names given to the server
		if(i == 1) {
			strcpy(ROOM_NAMES[i - 1], "NULL");
		} else {
			strcpy(ROOM_NAMES[i - 1], argv[i]);
		}
	}

	sock = serve_socket(atoi(argv[1]));
	tidp = (pthread_t*) malloc(sizeof(pthread_t));

	for(i = 0; i < 999; i++) { //Intializes all the room variables and creates a thread for each room
		if(strcmp(ROOM_NAMES[i], "") == 0) break;

		ROOM_CHAT[i] = new_dllist();
		ROOM_MEMBERS[i] = new_dllist();
		ROOMWRITE[i] = 0;
		NUM_PEOPLE[i] = 0;

		pthread_attr_init(attr+i);
		pthread_attr_setscope(attr+i, PTHREAD_SCOPE_SYSTEM);
		pthread_mutex_init(&LOCKS[i], NULL);

		pthread_mutex_lock(&LOCKS[i]);
		pthread_create(tidp+i, attr+i, chatroom_thread, &i);
		pthread_mutex_unlock(&LOCKS[i]);


		usleep(250000);
	}
	printf("Success! Server had properly started and is ready for connections!\n");

	pthread_attr_init(attr+i);
	pthread_attr_setscope(attr+i, PTHREAD_SCOPE_SYSTEM);
	pthread_create(tidp+i, attr+i, server_thread, &sock); //Creates the main server thread
	pthread_exit(NULL);

	return (0);
}