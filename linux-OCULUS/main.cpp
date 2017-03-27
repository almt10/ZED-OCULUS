#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <array>

#include <chrono>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

#define RESIZE_WIDTH 160
#define RESIZE_HEIGTH 90

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main() {
    //part in which we initialize the socket that will SEND the information
    int sockfd2, portno2, n2;
    struct sockaddr_in serv_addr2;
    struct hostent *server;
    const char *receiver = "141.30.172.80";

    portno2 = 30000;
    sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd2 < 0)
        error("ERROR opening socket");
    server = gethostbyname(receiver);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr2, sizeof(serv_addr2));
    serv_addr2.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr2.sin_addr.s_addr, server->h_length);
    serv_addr2.sin_port = htons(portno2);
    if (connect(sockfd2, (struct sockaddr *) &serv_addr2, sizeof(serv_addr2)) < 0)
        error("ERROR connecting");


    //part in which we initialize the socket that will RECEIVE the information
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    unsigned char *buffer = NULL;
    buffer = (unsigned char *) malloc(40);
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 15000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    printf("Listenning....\n");
    listen(sockfd, 5); //5 means the number
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    printf("Conection accepted....\n");
    if (newsockfd < 0)
        error("ERROR on accept");


    //part in which we initialize the socket that will RECEIVE the information
    int sockfdFPS, newsockfdFPS, portnoFPS;
    socklen_t clilenFPS;
    unsigned char *bufferFPS = NULL;
    bufferFPS = (unsigned char *) malloc(40);
    struct sockaddr_in serv_addrFPS, cli_addrFPS;
    int nFPS;

    sockfdFPS = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfdFPS < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addrFPS, sizeof(serv_addr));
    portnoFPS = 18000;
    serv_addrFPS.sin_family = AF_INET;
    serv_addrFPS.sin_addr.s_addr = INADDR_ANY;
    serv_addrFPS.sin_port = htons(portnoFPS);
    if (bind(sockfdFPS, (struct sockaddr *) &serv_addrFPS, sizeof(serv_addrFPS)) < 0)
        error("ERROR on binding");
    printf("Listenning FPS....\n");
    listen(sockfdFPS, 5); //5 means the number
    clilenFPS = sizeof(cli_addrFPS);
    newsockfdFPS = accept(sockfdFPS, (struct sockaddr *) &cli_addrFPS, &clilenFPS);
    printf("Conection accepted FPS....\n");
    if (newsockfdFPS < 0)
        error("ERROR on accept");


    //we get some useful information from the socket
    int zedWidth = 0;
    int zedHeight = 0;
    int focal = 0;
    int resizeWidth = RESIZE_WIDTH;
    int resizeHeigth = RESIZE_HEIGTH;
    n = read(newsockfd, buffer, 13);
    if (n < 0) error("ERROR reading from socket");
    if (n > 0) {
        if (buffer[0] != 'A') error("ERROR reading from socket the initial label 'A' ");
        else {
            zedWidth = *(int *) &buffer[1];
            zedHeight = *(int *) &buffer[5];
            focal = *(int *) &buffer[9];

            memcpy(&buffer[13], &resizeHeigth, sizeof(int));
            memcpy(&buffer[17], &resizeWidth, sizeof(int));
            n2 = write(sockfd2, buffer, 21);
            if (n2 < 0) error("ERROR writing to socket");
        }
    }

    //variables to receive the data and send it to the end user
    int size = 0;
    int length_toread = 0;
    int c = 0;
    bool both = false;
    int bytesToSend = 0;
    char side;
    unsigned char *infoL = NULL;
    unsigned char *infoR = NULL;
    unsigned char *info2 = NULL;
    unsigned char *config = NULL;
    char test;
    int data = 0;
    double time_inic;
    double time_end;
    double old_bandwidth;
    double actual_bandwidth;
    size_t t = 100;
    infoL = (unsigned char *) malloc(15000000);
    infoR = (unsigned char *) malloc(15000000);
    config = (unsigned char *) malloc(100);
    info2 = (unsigned char *) malloc(15000000);



    //variables related with sockets
    fd_set readfdsFPS;
    fd_set original_socketFPS;
    fd_set original_stdinFPS;

    FD_ZERO(&readfdsFPS);
    FD_ZERO(&original_socketFPS);
    FD_ZERO(&original_stdinFPS);
    struct timeval tvFPS;
    tvFPS.tv_sec = 0;
    tvFPS.tv_usec = 0;

    FD_SET(newsockfdFPS, &original_socketFPS);
    FD_SET(0, &original_stdinFPS);
    FD_SET(newsockfdFPS, &readfdsFPS);

    //variables to measure the amount of FPS we have
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds;

    int e=0;
    double difference= 0;
    int recvc = 0;
    int sendc = 0;
    int old_FPS = 0;
    int oculus_FPS = 0;
    int actual_FPS = 0;
    int receivedFPS =0;
    int sendFPS= 0;
    int CameraFPS=0;
    int category = 8;
    int ImageSize = 0;

    //variables to process the image
    Mat image0 = Mat(zedHeight, zedWidth, CV_8UC4, 1);
    Mat imageResized = Mat(RESIZE_HEIGTH, RESIZE_WIDTH, CV_8UC4, 1);
    double ratio=0.0;
    int next_bandwidth=0;
    int next_category = 0;
    int resize_width = 0;
    int resize_heigth = 0;

    //main lopp
    int i = 1;
    while (i < 2) {
        //we control the number of frames per second we send and we receive
        elapsed_seconds= end-start;
        if( (elapsed_seconds.count() * 1000)> 1000){
            FD_SET(newsockfdFPS, &original_socketFPS);
            FD_SET(0, &original_stdinFPS);
            FD_SET(newsockfdFPS, &readfdsFPS);
            int max_fd = newsockfdFPS;
            //we check if there is some information available in the socket that sends the camera FPS
            int readableFPS = select(max_fd+1, &readfdsFPS, NULL, NULL, &tvFPS);
            if (readableFPS>0)
            {
                nFPS = read(newsockfdFPS, bufferFPS, 4);
                if(nFPS >0){
                    CameraFPS = *(int *)bufferFPS;
                }
            }
            else if(readableFPS < 0){
                printf("Error with select\n");

            }
            else{
                printf("select not working\n");
            }

            receivedFPS = recvc;
            sendFPS = sendc ;
            oculus_FPS = sendFPS;
            sendc=0;
            recvc=0;
            start = end;

            if(CameraFPS!=0 && oculus_FPS!=0){
                if((category >1) && (oculus_FPS < (0.4 * CameraFPS))){
                    category--;
                }
                else{
                    if(category<8) {
                        actual_bandwidth = oculus_FPS * category * category * RESIZE_WIDTH * RESIZE_HEIGTH;
                        next_category = category +1;
                        next_bandwidth = next_category * next_category * RESIZE_WIDTH * RESIZE_HEIGTH;
                        if((actual_bandwidth/(next_bandwidth))>(0.6 * CameraFPS)){
                            category = next_category;
                        }
                    }
                }
            }
           printf("FPS received : %d ..... sent: %d .... Camera : %d\n",receivedFPS, sendFPS, CameraFPS);
        }


        //we receive some data
        n = read(newsockfd, buffer, 2);
        if (n < 0) error("ERROR reading from socket");
        if (n > 0) {
            if ((buffer[0] != 'a') || (buffer[1] != 'a'))error("ERROR reading from socket the initial labels");
            else {
                n = read(newsockfd, buffer, 5);
                if (n < 0) error("ERROR reading from socket");
                if (n > 0) {
                    side = buffer[0];
                    size = *(int *) &buffer[1];
                    length_toread = size;
                    c = 0;
                    //finally we read the information of the image
                    while (length_toread > 0) {
                        int x = min(400000, length_toread);
                        n = read(newsockfd, buffer, min(400000, length_toread));
                        if (n < 0) error("ERROR reading from socket the image data");
                        if (n > 0) {
                            if (side == 'L') {
                                memcpy(&infoL[c], buffer, n);
                                length_toread -= n;
                                c += n;
                            } else if (side == 'R') {
                                memcpy(&infoR[c], buffer, n);
                                c += n;
                                length_toread -= n;
                                if (c == size) {
                                    both = true;
                                    recvc++;
                                    //end = std::chrono::system_clock::now();
                                    e++;
                                }
                            } else {
                                error("ERROR copying the data in the buffers");
                            }

                        }
                    }
                }
            }
        }


        //here we process the images
        if(category != 8) {
            if (side == 'L') memcpy(image0.data, infoL, size);
            else if (side == 'R') memcpy(image0.data, infoR, size);
            else printf("error when copying the information into the image\n");

            resize_width = RESIZE_WIDTH *category ;
            resize_heigth = RESIZE_HEIGTH *category;

            resize(image0, imageResized, Size(resize_width, resize_heigth));

            if (side == 'L') memcpy(infoL, imageResized.data, resize_width*resize_heigth*4);
            else if (side == 'R') memcpy(infoR, imageResized.data, resize_width*resize_heigth*4);
            else printf("error when copying the information into the image\n");

        }

        //now we create the info we want to send
        if (side == 'L') {
            info2[0] = 'a';
            info2[1] = 'a';
            info2[2] = 'L';
            memcpy(&info2[3], &size, sizeof(unsigned int));
            memcpy(&info2[7], infoL, size);
            bytesToSend = size + 7;
        }
        else if (side == 'R') {
            //realloc(info2, size + 7);
            info2[0] = 'a';
            info2[1] = 'a';
            info2[2] = 'R';
            memcpy(&info2[3], &size, sizeof(unsigned int));
            memcpy(&info2[7], infoR, size);
            bytesToSend = size + 7;
        }
        else{
            printf("sdsd");
        }

        //we send this data
        n2 = write(sockfd2, info2, bytesToSend);
        if (n2 < 0)
            error("ERROR writing to socket");


        if(side == 'R'){
            sendc++;
            end = std::chrono::system_clock::now();
            n2 = read(sockfd2, config, 10);
            if (n2 > 0) {
                if ((config[0] == 'c') && (config[0] == 'c')) {
                    data = *(int *) &config[2];
                    actual_FPS = *(int *) &config[6];
                    if(abs(actual_FPS - old_FPS) > 1) {
                        difference = (time_end - time_inic) / CLOCKS_PER_SEC;
                        actual_bandwidth= old_FPS * data * 2 * 8;
                        //printf("\nFPS received in Oculus PC: %d", actual_FPS);
                        oculus_FPS = actual_FPS;
                    }
                    old_FPS = actual_FPS;
                }
            } else {
                printf("Error when reading the bandwidth information");
            }
        }


    }

    return 0;
}