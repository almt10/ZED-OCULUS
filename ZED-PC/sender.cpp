// sender.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <stddef.h>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>

#include <zed/Camera.hpp>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define DEFAULT_WEB_PORT "5000"
#define SERVER_IP "141.30.172.202"

int main()
{
	while (true) {
		//information for the socket that sends the pictures to the OCULUS PC
		WSADATA wsaData;
		SOCKET ConnectSocket = INVALID_SOCKET;
		struct addrinfo *result = NULL,
			*ptr = NULL,
			hints;
		int iResult;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			printf("WSAStartup failed with error: %d\n", iResult);
			continue;
			//return 1;
		}

		// Resolve the server address and port for information socket
		iResult = getaddrinfo(SERVER_IP, DEFAULT_PORT, &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			continue;
			//return 1;
		}
		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			// Create a SOCKET for connecting to server
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol);  
			if (ConnectSocket == INVALID_SOCKET) {
				printf("socket failed with error: %ld\n", WSAGetLastError());
				WSACleanup();
				break;
				//return 1;
			}
			// Connect to server.
			iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}
		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET) {
			printf("Unable to connect to server!\n");
			WSACleanup();
			continue;
			//return 1;
		}

		// Initialize the ZED Camera
		sl::zed::Camera* zed = 0;
		zed = new sl::zed::Camera(sl::zed::HD720);
		sl::zed::InitParams init_parameters(sl::zed::MODE::QUALITY);
		sl::zed::ERRCODE zederr = zed->init(init_parameters);
		if (zederr != sl::zed::SUCCESS) {
			std::cout << "ERROR: " << sl::zed::errcode2str(zederr) << std::endl;
			delete zed;
			closesocket(ConnectSocket);
			WSACleanup;
			continue;
			//return -1;
		}
		//get some parameters of the zed camera and send it to the server
		unsigned int zedWidth = zed->getImageSize().width;
		unsigned int zedHeight = zed->getImageSize().height;
		unsigned int focal = zed->getParameters()->LeftCam.fx;
		unsigned char *parameters = NULL;
		parameters = (unsigned char *)malloc((sizeof(unsigned int) * 3) + (1 * sizeof(unsigned char)));
		parameters[0] = 'A';
		memcpy(&parameters[1], &zedWidth, sizeof(unsigned int));
		memcpy(&parameters[5], &zedHeight, sizeof(unsigned int));
		memcpy(&parameters[9], &focal, sizeof(unsigned int));
		//we send some initial parameters from the camera
		iResult = send(ConnectSocket, (const char *)parameters, (sizeof(unsigned int) * 3) + (1 * sizeof(unsigned char)), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			continue;
			//return 1;
		}

		unsigned char *bufferL = NULL;
		unsigned char *bufferR = NULL;
		unsigned char *infoL = NULL;
		unsigned char *infoR = NULL;
		bufferL = (unsigned char *)malloc(4000000);
		bufferR = (unsigned char *)malloc(4000000);

		unsigned int sizeL = zedWidth * zedHeight * 4;
		unsigned int sizeR = zedWidth * zedHeight * 4;
		unsigned int size = 0;
		unsigned int bytesToSend = 0;
		int end = 0;


		//Main loop
		while (!end) {
			if (!zed->grab(sl::zed::SENSING_MODE::STANDARD, false, false)) {
				infoL = zed->retrieveImage(sl::zed::SIDE::LEFT).data;
				size = sizeL;

				bufferL = (unsigned char *)realloc(bufferL, (size + 7));
				//printf("\nsize of the image L: %d\n", size);
				bufferL[0] = 'a';
				bufferL[1] = 'a';
				bufferL[2] = 'L';
				memcpy(&bufferL[3], &size, sizeof(unsigned int));
				memcpy(&bufferL[7], infoL, size);
				bytesToSend = size + 7;
				//we send the information of the Left side
				iResult = send(ConnectSocket, (const char *)bufferL, bytesToSend, 0);
				if (iResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					break;
					//return 1;
				}

				  //now we do the same but with the Rigth image
				infoR = zed->retrieveImage(sl::zed::SIDE::RIGHT).data;
				size = sizeR;

				bufferR = (unsigned char *)realloc(bufferR, (size + 7));
				//printf("\nsize of the image L: %d\n", size);
				bufferR[0] = 'a';
				bufferR[1] = 'a';
				bufferR[2] = 'R';
				memcpy(&bufferR[3], &size, sizeof(unsigned int));
				memcpy(&bufferR[7], infoR, size);
				bytesToSend = size + 7;
				//we send the information of the Left side
				iResult = send(ConnectSocket, (const char *)bufferR, bytesToSend, 0);
				if (iResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					break;
					//return 1;
				}
			}
		}

		// shutdown the connection since no more data will be sent
		iResult = shutdown(ConnectSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			//return 1;
		}

		closesocket(ConnectSocket);
		WSACleanup();
		delete zed;
	}
    return 0;
}

