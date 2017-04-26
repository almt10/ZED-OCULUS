#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <array>

#include <chrono>

using namespace std;
using namespace cv;

#define RESIZE_WIDTH 160
#define RESIZE_HEIGTH 90
#define SHMSZ 5000000
#define halfcode false

pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

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
    const char *receiver = "192.168.2.41";

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
    buffer = (unsigned char *) malloc(1000);
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

    char *recvBufferSize = "2000000";
    int g = setsockopt(newsockfd, SOL_SOCKET, SO_RCVBUF, recvBufferSize, sizeof(int));

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


    //we declare the variables related with the shared memory
    char cc;
    int shmidL, shmidR, shmidCon;
    key_t keyL, keyR, keyCon;
    char *shmL,*shmR;

    char *sL, *sR;
    char *shmCon, *sCon;

    keyL = 5678;
    keyR= 1567 ;

    if ((shmidL = shmget(keyL, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmgetL");
        exit(1);
    }
    if ((shmidR = shmget(keyR, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmgetR");
        exit(1);
    }

    if((shmL = (char *) shmat(shmidL, NULL, 0)) == (char *) -1){
        perror("shmatL");
        exit(1);
    }
    sL=shmL;
    if((shmR = (char *) shmat(shmidR, NULL, 0)) == (char *) -1){
        perror("shmat");
        exit(1);
    }
    sR=shmR;

    keyCon = 2000;
    if((shmidCon = shmget(keyCon, SHMSZ, IPC_CREAT | 0666))< 0){
        perror("shmgetR");
        exit(1);
    }

    if((shmCon = (char *) shmat(shmidCon, NULL, 0)) == (char *) -1){
        perror("shmat");
        exit(1);
    }
    sCon=shmCon;


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
    int sizeL = 0;
    int sizeR=0;
    int length_toread = 0;
    int c = 0;
    bool both = false;
    bool free = true;
    int bytesToSend = 0;
    char side='L';
    int seqNumberL=0;
    int seqNumberR =0;
    int oldseqNumberL = 0;
    int oldseqNumberR = 0;
    unsigned char *infoL = NULL;
    unsigned char *infoR = NULL;
    unsigned char *informationL = NULL;
    unsigned char *informationR = NULL;
    unsigned char *info2 = NULL;
    unsigned char *config = NULL;
    unsigned char *ready = NULL;
    char test;
    bool send = false;
    bool firstTime=true;
    int data = 0;
    double time_inic;
    double time_end;
    double old_bandwidth;
    double actual_bandwidth;
    size_t t = 100;
    ready= (unsigned char *) "more";
    infoL = (unsigned char *) malloc(15000000);
    infoR = (unsigned char *) malloc(15000000);
    informationL = (unsigned char *) malloc(15000000);
    informationR = (unsigned char *) malloc(15000000);
    config = (unsigned char *) malloc(100);
    info2 = (unsigned char *) malloc(15000000);

    infoL[0] = 'a';
    infoL[1] = 'a';
    infoL[2] = 'L';
    infoR[0] = 'a';
    infoR[1] = 'a';
    infoR[2] = 'R';


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

    fd_set read2;
    fd_set original_socket2;
    fd_set original_stdin2;

    FD_ZERO(&read2);
    FD_ZERO(&original_socket2);
    FD_ZERO(&original_stdin2);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_SET(sockfd2, &original_socket2);
    FD_SET(0, &original_stdin2);
    FD_SET(sockfd2, &read2);

    //variables to measure the amount of FPS we have
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds;

    int e=0;
    char testR[6];
    char testL[6];
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
    Mat imageL = Mat(zedHeight, zedWidth, CV_8UC4, 1);
    Mat imageR = Mat(zedHeight, zedWidth, CV_8UC4, 1);
    Mat imageResizedL = Mat(RESIZE_HEIGTH, RESIZE_WIDTH, CV_8UC4, 1);
    Mat imageResizedR = Mat(RESIZE_HEIGTH, RESIZE_WIDTH, CV_8UC4, 1);
    double ratio=0.0;
    int next_bandwidth=0;
    int next_category = 0;
    int resize_width = 0;
    int resize_heigth = 0;
    int sizeResized = 0;
    int capacityRigthArray[10000];
    int z=0;
    //variables in case the images are coded
    char encode;
    if(halfcode){
        encode = 'R';
    }
    else{
        encode ='Y';
    }
    std::vector<uchar> buffL;
    std::vector<uchar> buffR;
    std::vector<uchar> buffLresized;
    std::vector<uchar> buffRresized;
    int sizeBufferL =0;
    int sizeBufferR =0;
    int capacityLeft=0;
    int capacityRigth=0;
    int bytesToSendRigth=0;
    int bytesToSendLeft=0;

    //main lopp
    int i = 1;
    while (i < 2) {
        //we control the number of frames per second we send and we receive
        elapsed_seconds = end - start;
        if ((elapsed_seconds.count() * 1000) > 1000) {
            FD_SET(newsockfdFPS, &original_socketFPS);
            FD_SET(0, &original_stdinFPS);
            FD_SET(newsockfdFPS, &readfdsFPS);
            int max_fd = newsockfdFPS;
            //we check if there is some information available in the socket that sends the camera FPS
            int readableFPS = select(max_fd + 1, &readfdsFPS, NULL, NULL, &tvFPS);
            if (readableFPS > 0) {
                nFPS = read(newsockfdFPS, bufferFPS, 4);
                if (nFPS > 0) {
                    CameraFPS = *(int *) bufferFPS;
                }
            } else if (readableFPS < 0) {
                printf("Error with select\n");

            } else {
                printf("select not working\n");
            }

            receivedFPS = recvc;
            sendFPS = sendc;
            oculus_FPS = sendFPS;
            sendc = 0;
            recvc = 0;
            start = end;

            if (CameraFPS != 0 && oculus_FPS != 0) {
                if ((category > 1) && (oculus_FPS < (0.6 * CameraFPS))) {
                    category--;
                } else {
                    if (category < 8) {
                        actual_bandwidth = oculus_FPS * category * category * RESIZE_WIDTH * RESIZE_HEIGTH;
                        next_category = category + 1;
                        next_bandwidth = next_category * next_category * RESIZE_WIDTH * RESIZE_HEIGTH;
                        if (oculus_FPS > (0.9 * CameraFPS)) {
                            category = next_category;
                        }
                    }
                }
            }
            printf("FPS received from localhost: %d ..... sent to OCULUS: %d .... Camera(extra socket): %d , category : %d\n",
                   receivedFPS, sendFPS, CameraFPS,category);
        }

        //test if we can receive in localhost
        // while (select == true)
        //we receive some data

        n2= read(newsockfd, buffer, 3);
        if(n2>0){
            if((buffer[0]=='s')&&(buffer[1]=='m')&&(buffer[2]=='R')){
                seqNumberL = *(int *) sL;
                seqNumberR = *(int *) sR;

                sizeL = *(int *) &sL[8];
                sizeR = *(int *) &sR[8];
                infoL[3] = sL[7];
                infoR[3] = sR[7];

                memcpy(&infoL[4], &category, sizeof(int));
                memcpy(&infoR[4], &category, sizeof(int));


                if(infoL[3] == 'Y'){
                    capacityLeft = *(int *)&sL[12];
                    capacityRigth = *(int *)&sR[12];
                    if(z<10000){
                        capacityRigthArray[z] = capacityRigth;
                    }
                    if(encode == 'R'){

                        buffL.reserve(capacityLeft);
                        buffR.reserve(capacityRigth);

                        buffL.resize(sizeL);
                        buffR.resize(sizeR);

                        pthread_mutex_lock(&(init_lock));
                        memcpy(buffL.data(), &sL[16], sizeL);
                        memcpy(buffR.data(), &sR[16], sizeR);
                        pthread_mutex_unlock(&(init_lock));

                        imageL = imdecode(buffL, CV_LOAD_IMAGE_COLOR);
                        imageR = imdecode(buffR, CV_LOAD_IMAGE_COLOR);

                        sizeL = zedHeight * zedWidth * 3;
                        sizeR = sizeL;
                        infoL[3] = 'R';
                        infoR[3] = 'R';
                        memcpy(&infoL[8], &sizeL, sizeof(int));
                        memcpy(&infoR[8], &sizeR, sizeof(int));
                        memcpy(&infoL[12], imageL.data, sizeL);
                        memcpy(&infoR[12], imageR.data, sizeR);
                        bytesToSend = sizeR + 12;
                    }
                    else{
                        pthread_mutex_lock(&(init_lock));
                        memcpy(&infoL[16], &sL[16], sizeL);
                        memcpy(&infoR[16], &sR[16], sizeR);
                        pthread_mutex_unlock(&(init_lock));

                        memcpy(&infoL[8], &sizeL, sizeof(int));
                        memcpy(&infoR[8], &sizeR, sizeof(int));
                        memcpy(&infoL[12], &capacityLeft, sizeof(int));
                        memcpy(&infoR[12], &capacityRigth, sizeof(int));

                        bytesToSendLeft = sizeL+16;
                        bytesToSendRigth = sizeR+16;
                    }
                }
                else if(infoL[3] == 'N'){
                    memcpy(&infoL[8], &sizeL, sizeof(int));
                    memcpy(&infoR[8], &sizeR, sizeof(int));
                    pthread_mutex_lock(&(init_lock));
                    memcpy(&infoL[12], &sL[12], sizeL);
                    memcpy(&infoR[12], &sR[12], sizeR);
                    pthread_mutex_unlock(&(init_lock));
                    bytesToSend = sizeR + 12;
                }
                else{
                    printf("Error when separeting coded and not coded\n");
                }

                if(category != 8){
                    if((infoL[3]=='Y') && (encode == 'Y')){
                        buffL.reserve(capacityLeft);
                        buffR.reserve(capacityRigth);

                        buffL.resize(sizeL);
                        buffR.resize(sizeR);

                        memcpy(buffL.data(), &infoL[16], sizeL);
                        memcpy(buffR.data(), &infoR[16], sizeR);

                        imageL = imdecode(buffL, CV_LOAD_IMAGE_COLOR);
                        imageR = imdecode(buffR, CV_LOAD_IMAGE_COLOR);
                    }
                    else{
                        memcpy(imageL.data, &infoL[12], sizeL);
                        memcpy(imageR.data, &infoR[12], sizeR);
                    }

                    resize_width = RESIZE_WIDTH * category;
                    resize_heigth = RESIZE_HEIGTH * category;

                    Mat imageResizedL = Mat(resize_heigth, resize_width, CV_8UC4, 1);
                    Mat imageResizedR = Mat(resize_heigth, resize_width, CV_8UC4, 1);

                    resize(imageL, imageResizedL, Size(resize_width, resize_heigth));
                    resize(imageR, imageResizedR, Size(resize_width, resize_heigth));


                    if((infoL[3]=='Y')&&(encode =='Y')){
                        cv::imencode(".jpg", imageResizedL, buffLresized);
                        cv::imencode(".jpg", imageResizedR, buffRresized);
                        capacityLeft = buffLresized.capacity();
                        capacityRigth = buffRresized.capacity();
                        sizeL = buffLresized.size();
                        sizeR = buffRresized.size();

                        infoL[3] = 'Y';
                        memcpy(&infoL[4], &category, sizeof(int));
                        memcpy(&infoL[8], &sizeL, sizeof(int));
                        memcpy(&infoL[12], &capacityLeft, sizeof(int));
                        memcpy(&infoL[16], buffLresized.data(), sizeL);


                        infoR[3] = 'Y';
                        memcpy(&infoR[4], &category, sizeof(int));
                        memcpy(&infoR[8], &sizeR, sizeof(int));
                        memcpy(&infoR[12], &capacityRigth, sizeof(int));
                        memcpy(&infoR[16], buffRresized.data(), sizeR);

                        bytesToSendLeft = sizeL + 16;
                        bytesToSendRigth = sizeR + 16;
                    }
                    else{
                        if(infoL[3]=='N'){
                            sizeResized = resize_heigth * resize_width * 4;
                        }
                        else if(infoL[3]=='R'){
                            sizeResized = resize_heigth * resize_width * 3;
                        }
                        bytesToSend = sizeResized + 12;

                        memcpy(&infoL[4], &category, sizeof(int));
                        memcpy(&infoL[8], &sizeResized, sizeof(int));
                        memcpy(&infoL[12], imageResizedL.data, sizeResized);

                        memcpy(&infoR[4], &category, sizeof(int));
                        memcpy(&infoR[8], &sizeResized, sizeof(int));
                        memcpy(&infoR[12], imageResizedR.data, sizeResized);
                    }

                }

                //now we must check that the sequence number has increased
                if ((seqNumberL) > oldseqNumberL) {
                    both=false;
                    recvc++;
                    oldseqNumberL = seqNumberL;
                }

                while (!both) {
                    infoL[0] = 'a';
                    infoL[1] = 'a';
                    infoL[2] = 'L';
                    infoR[0] = 'a';
                    infoR[1] = 'a';
                    infoR[2] = 'R';
                    //now we create the info we want to send
                    if(encode == 'Y'){
                        if(side =='L')
                            n2 = write(sockfd2, infoL, bytesToSendLeft);
                        else if(side =='R')
                            n2 = write(sockfd2, infoR, bytesToSendRigth);

                        if (n2 < 0)
                            error("ERROR writing to socket");
                    }
                    else{
                        if(side =='L')
                            n2 = write(sockfd2, infoL, bytesToSend);
                        else if(side =='R')
                            n2 = write(sockfd2, infoR, bytesToSend);

                        if (n2 < 0)
                            error("ERROR writing to socket");
                    }

                    if (side == 'L'){
                        side = 'R';
                    }
                    else if (side == 'R') {
                        sendc++;
                        both = true;
                        firstTime = true;
                        side = 'L';
                        end = std::chrono::system_clock::now();


                        //we check if there is some information available in the socket that sends the camera FPS
                        n2 = read(sockfd2, config, 10);
                        if (n2 > 0) {
                            if ((config[0] == 'c') && (config[0] == 'c')) {
                                data = *(int *) &config[2];
                                actual_FPS = *(int *) &config[6];
                                if (abs(actual_FPS - old_FPS) > 1) {
                                    difference = (time_end - time_inic) / CLOCKS_PER_SEC;
                                    actual_bandwidth = old_FPS * data * 2 * 8;
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

            }
        }


        /*if(category != 8){
            if (side == 'L') memcpy(image0.data, infoL, size);
            else if (side == 'R') memcpy(image0.data, infoR, size);
            else printf("error when copying the information into the image\n");

            resize_width = RESIZE_WIDTH * category;
            resize_heigth = RESIZE_HEIGTH * category;

            resize(image0, imageResized, Size(resize_width, resize_heigth));

            if (side == 'L') memcpy(infoL, imageResized.data, resize_width * resize_heigth * 4);
            else if (side == 'R') memcpy(infoR, imageResized.data, resize_width * resize_heigth * 4);
            else printf("error when copying the information into the image\n");
        }*/

    }

    return 0;
}