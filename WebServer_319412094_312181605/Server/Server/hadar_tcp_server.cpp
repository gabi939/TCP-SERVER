#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#include <winsock2.h>
#include <string.h>
#include <time.h>
# include <ctime>
#include <fstream>

#include <filesystem>
#include  "dirent.h"
#include <sstream>      // std::stringstream


#pragma comment(lib, "Ws2_32.lib")

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	string sendSubType;	// Sending sub-type
	int messageType;
	char buffer[10000];
	int len;
};

const int TIME_PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int DEFAULT = 1;
const int GET = 11;
const int PUT = 22;
const int HEAD = 33;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
inline bool exists_test3(const std::string& name);
bool fileExists(const std::string& filename)
{
	struct stat buf;
	if (stat(filename.c_str(), &buf) != -1)
	{
		return true;
	}
	return false;
}
struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

void main()
{
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Error at WSAStartup()\n";
		return;
	}

	//socket listener starts working
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)& serverService, sizeof(serverService)))
	{
		cout << "Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{

		fd_set waitRecv;// sets sockets that are waiting for rec
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);// sets sockets that are waiting for send
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}


		//set timeout for select
		struct timeval timeout;
		timeout.tv_sec = 120;
		timeout.tv_usec = 0; 
		int nfd;
		
		//getting number of sockets that need a service
		nfd = select(0, &waitRecv, &waitSend, NULL, &timeout);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}
		if (nfd == 0)
		{
			cout <<"  time out  ";
		}
		if (nfd < 0)
		{
			cout<<"  Error  ";
			break; 
		}


		// handling requests
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:

					receiveMessage(i);
					break;
				}
			}
		}

		
		// handling send requests
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{

			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:

					sendMessage(i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Time Server: Closing Connection.\n" << endl;
;
	closesocket(listenSocket);
	WSACleanup();
}


// adds new sockets when connection is accepted
bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

//remove sockect when  all requests are handeled
void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

// establish connection with client
void acceptConnection(int index)
{
	int bytesSent = 0;
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*) & from, &fromLen);

	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}


	return;
}


// handling rec message
void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n" << endl;;

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{

			char* temp =  _strdup(sockets[index].buffer);

			// handling types of requests & identifying files needed
			string messageType = strtok(temp, " ");
			char* fileName = strtok(NULL, " ");
			
			sockets[index].send = SEND;
			sockets[index].sendSubType = fileName;
		
			
			if (messageType == "GET") { sockets[index].messageType = GET; }
			else if (messageType == "PUT") {sockets[index].messageType = PUT; }
			else if (messageType == "HEAD") { sockets[index].messageType = HEAD; }

			memcpy(sockets[index].buffer, &sockets[index].buffer[strlen(fileName)], sockets[index].len - bytesRecv);
			sockets[index].len -= bytesRecv;
			return;

		}
	}

}


// send message func 
void sendMessage(int index)
{

	int bytesSent = 0;
	char sendBuff[10000];

	SOCKET msgSocket = sockets[index].id;

	switch (sockets[index].messageType) {

	// sending the appropriate head response for the status of a file req
	case HEAD: {
		string fileName = "files" + sockets[index].sendSubType;
		ifstream infile(fileName);
		
		if (infile.good()) {
			bytesSent = send(msgSocket, "HTTP/1.1 200 OK\r\n\r\n", strlen("HTTP/1.1 200 OK\r\n\r\n"), 0);
			if (SOCKET_ERROR == bytesSent)
			{
				cout << "Error at send(): " << WSAGetLastError() << endl;
			}
		}
		else {
			bytesSent = send(msgSocket, "HTTP/1.1 404 Not Found\r\n\r\n", strlen("HTTP/1.1 404 Not Found\r\n\r\n"), 0);
			if (SOCKET_ERROR == bytesSent)
			{
				cout << "Error at send(): " << WSAGetLastError() << endl;
			}

		}
		break;
	}
	
	
	
	// updating the files according to the protocol & responding accordingly
	case PUT: {

		string fileName = "files" + sockets[index].sendSubType;

		string message(sockets[index].buffer);
		string messageBody = message.substr(message.find("\r\n\r\n") + 4, message.size() - message.find("\r\n\r\n"));

		if (fileExists(fileName)){
			ofstream newFile(fileName);

			newFile.is_open();
			newFile << messageBody;

			string temp = "HTTP/1.1 204 No Content\r\nContent-Location: " + sockets[index].sendSubType + "\r\n\r\n";
			char* reply = (char*)malloc(temp.length() + 1);
			strcpy(reply, temp.c_str());
			

			bytesSent = send(msgSocket, reply, (int)strlen(reply), 0);
			if (SOCKET_ERROR == bytesSent)
			{
				cout << "Error at send(): " << WSAGetLastError() << endl;
			}
			newFile.close();

		}
		else{
			ofstream newFile(fileName);
			newFile.is_open();
			newFile << messageBody;


			string temp = "HTTP/1.1 201 Created\r\nContent-Location: " + sockets[index].sendSubType + "\r\nContent-Length: 45\r\n\r\n<html><body><p>File Created</p></body></html>";
			char* reply = (char*)malloc(temp.length() + 1);
			strcpy(reply, temp.c_str());
			

			bytesSent = send(msgSocket, reply, (int)strlen(reply), 0);
			if (SOCKET_ERROR == bytesSent)
			{
				cout << "Error at send(): " << WSAGetLastError() << endl;
			}
			newFile.close();

		}



		break; }


	// getting files according to request
	case GET:{

		//case main page requested, send index file 
		if (sockets[index].sendSubType == "/") {

			string fileName = "files\\index.html";

			string STRING, hold;
			std::ifstream infile(fileName);

			//checking if file exists
			if (infile.good()) {
				while (!infile.eof())
				{
					getline(infile, STRING);
					hold += STRING;
				}

				infile.close();
				strcpy_s(sendBuff, _countof(sendBuff), hold.c_str());

			}
			else {//case file doesnt send back 404


				string fileName = "files\\404.html";
				string STRING, hold;
				ifstream infile(fileName);

				
					while (!infile.eof())
					{
						getline(infile, STRING);
						hold += STRING;
					}

					infile.close();
					strcpy_s(sendBuff, _countof(sendBuff), hold.c_str());
					string s(sendBuff);

					string header = "HTTP/1.1 404 Not Found\r\nContent-Type: text\\html\r\nContent-Length: " + to_string(strlen(sendBuff)) + "\r\n\r\n" + s;
					char* reply = (char*)malloc(header.length() + 1);
					strcpy(reply, header.c_str());
					
					bytesSent = send(msgSocket, reply, (int)strlen(reply), 0);
					if (SOCKET_ERROR == bytesSent)
					{
						cout << "Error at send(): " << WSAGetLastError() << endl;
					}

				sockets[index].send = IDLE;
				return;
			}
		}
		else {//case asking for specific file 

			string fileName = "files" + sockets[index].sendSubType;
			string STRING, hold;
			ifstream infile(fileName);

			if (infile.good()) {// case exists
				while (!infile.eof())
				{
					getline(infile, STRING);
					hold += STRING;
				}

				infile.close();
				strcpy_s(sendBuff, _countof(sendBuff), hold.c_str());

			}
			else {// case not exist 
				string fileName = "files\\404.html";
				string STRING, hold;
				ifstream infile(fileName);


				while (!infile.eof())
				{
					getline(infile, STRING);
					hold += STRING;
				}

				infile.close();
				strcpy_s(sendBuff, _countof(sendBuff), hold.c_str());
				string s(sendBuff);
				string header = "HTTP/1.1 404 Not Found\r\nContent-Type: text\\html\r\nContent-Length: " + to_string(strlen(sendBuff)) + "\r\n\r\n" + s;
				char* reply = (char*)malloc(header.length() + 1);
				strcpy(reply, header.c_str());
				
				bytesSent = send(msgSocket, reply, (int)strlen(reply), 0);
				if (SOCKET_ERROR == bytesSent)
				{
					cout << "Error at send(): " << WSAGetLastError() << endl;
				}

				sockets[index].send = IDLE;
				return;
			}
		}


		//sending response for successful GET req with content 

		string s(sendBuff);
		string header = "HTTP/1.1 200 OK\r\nContent-Type: text\\html\r\nContent-Length: " + to_string(strlen(sendBuff)) + "\r\n\r\n" + s;
		char* reply = (char*)malloc(header.length() + 1);
		strcpy(reply, header.c_str());

		bytesSent = send(msgSocket, reply, (int)strlen(reply), 0);
		if (SOCKET_ERROR == bytesSent)
		{
			cout << "Error at send(): " << WSAGetLastError() << endl;
			return;
		}

		cout << "Sent: " << bytesSent << "\\" << strlen(reply) << " bytes of \"" << reply << "\" message.\n" << endl;;
		break;}
	}
	sockets[index].send = IDLE;

}




