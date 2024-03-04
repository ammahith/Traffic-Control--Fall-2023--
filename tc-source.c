
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <common.h>
#include <stdbool.h>
#include <semaphore.h>
#include <time.h> // Added for time functions

#define totalCarNumb 8
#define STRAIGHT false
#define LEFT true
#define oneHundredK 100000

bool crossHolder[4];
int crossing[4] = {0, 0, 0, 0};

pthread_mutex_t turnLock[4];
pthread_mutex_t updateLock[4];

sem_t enterSem[4];
sem_t exitSem;
sem_t printSem;

int elapsedTime = 0;
int actualTime = 0;

typedef struct _carInfo
{
    char originalDir;
    char targetDir;
    int cid;
    int delay;
} carInfo;

carInfo newCar[totalCarNumb];

void delay1(int howLong)
{
    int i;
    for (i = 0; i < howLong; i++)
    {
        usleep(oneHundredK);
        elapsedTime += 1;
    }
    actualTime = elapsedTime;
}

void delay2(int howLong, int *cur)
{
    int i;
    for (i = 0; i < howLong; i++)
        usleep(oneHundredK);
    *cur += howLong;
}

int dirToInt(char tempDir)
{
    if (tempDir == '^')
        return 0;
    else if (tempDir == 'v')
        return 2;
    else if (tempDir == '>')
        return 1;
    else
        return 3;
}

bool isRightTurn(int tempS, int tempD)
{
    int dir = tempS - tempD;
    if (dir == 3 || dir == -1)
        return true;
    else if (dir == -3 || dir == 1)
        return false;
}

void printCarStatus(carInfo *tempCar, const char *status)
{
    sem_wait(&printSem);
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    printf("Date and Time: %s", asctime(tm_info));
    printf("Time %d.%d: Car %d (%c %c) %s\n", (int)actualTime / 10, actualTime % 10, tempCar->cid, tempCar->originalDir, tempCar->targetDir, status);
    sem_post(&printSem);
}

void arriveAtIntersection(carInfo *tempCar)
{
    int source = dirToInt(tempCar->originalDir);
    int destination = dirToInt(tempCar->targetDir);
    int val;

    printCarStatus(tempCar, "arriving");

    sem_wait(&exitSem);

    if (source == destination)
    {
        pthread_mutex_lock(&updateLock[destination]);
        if (crossHolder[destination] == LEFT || crossing[destination] == 0)
        {
            pthread_mutex_unlock(&updateLock[destination]);
            pthread_mutex_lock(&turnLock[destination]);
            pthread_mutex_lock(&updateLock[destination]);
            crossHolder[destination] = STRAIGHT;
        }
        val = crossing[destination]++;
        pthread_mutex_unlock(&updateLock[destination]);

        if (val < 1)
        {
            sem_wait(&enterSem[destination]);
        }
    }
    else if (isRightTurn(source, destination) == true)
    {
        sem_wait(&enterSem[destination]);
        crossing[destination]++;
    }
    else if (isRightTurn(source, destination) == false)
    {
        pthread_mutex_lock(&updateLock[(source + 2) % 4]);
        if (crossHolder[(source + 2) % 4] == STRAIGHT || crossing[(source + 2) % 4] == 0)
        {
            pthread_mutex_unlock(&updateLock[(source + 2) % 4]);
            pthread_mutex_lock(&turnLock[(source + 2) % 4]);
            pthread_mutex_lock(&updateLock[(source + 2) % 4]);
            crossHolder[(source + 2) % 4] = LEFT;
        }
        val = crossing[(source + 2) % 4]++;
        pthread_mutex_unlock(&updateLock[(source + 2) % 4]);

        if (val < 1)
        {
            sem_wait(&enterSem[destination]);
        }
    }

    sem_post(&exitSem);
    tempCar->delay = actualTime;
}

void crossingIntersection(carInfo *tempCar)
{
    printCarStatus(tempCar, "crossing");

    int source = dirToInt(tempCar->originalDir);
    int destination = dirToInt(tempCar->targetDir);

    if (source == destination)
    {
        delay2(20, &tempCar->delay);
    }
    else if (isRightTurn(source, destination))
    {
        delay2(10, &tempCar->delay);
    }
    else
    {
        delay2(30, &tempCar->delay);
    }

    actualTime = tempCar->delay;
}

void exitIntersection(carInfo *tempCar)
{
    int source = dirToInt(tempCar->originalDir);
    int destination = dirToInt(tempCar->targetDir);

    if (source == destination)
    {
        pthread_mutex_lock(&updateLock[destination]);
        crossing[destination]--;

        if (crossing[destination] == 0)
        {
            pthread_mutex_unlock(&turnLock[destination]);
            sem_post(&enterSem[destination]);
        }

        pthread_mutex_unlock(&updateLock[destination]);
    }
    else if (isRightTurn(source, destination) == false)
    {
        pthread_mutex_lock(&updateLock[(source + 2) % 4]);
        crossing[(source + 2) % 4]--;

        if (crossing[(source + 2) % 4] == 0)
        {
            pthread_mutex_unlock(&turnLock[(source + 2) % 4]);
            sem_post(&enterSem[destination]);
        }

        pthread_mutex_unlock(&updateLock[(source + 2) % 4]);
    }
    else if (isRightTurn(source, destination) == true)
    {
        pthread_mutex_lock(&updateLock[destination]);
        crossing[destination]--;

        if (crossing[destination] == 0)
        {
            sem_post(&enterSem[destination]);
        }

        pthread_mutex_unlock(&updateLock[destination]);
    }

    printCarStatus(tempCar, "exiting");
}

void *startCrossing(void *args)
{
    carInfo *tempCar = (carInfo *)args;
    arriveAtIntersection(tempCar);
    crossingIntersection(tempCar);
    exitIntersection(tempCar);
    pthread_exit(NULL);
}

int main()
{
    int delays[totalCarNumb] = {10, 9, 13, 2, 7, 2, 13, 2};
    char originalDirData[totalCarNumb] = {'^', '^', '^', 'v', 'v', '^', '>', '<'};
    char targetDirData[totalCarNumb] = {'^', '^', '<', 'v', '>', '^', '^', '^'};
    pthread_t threads[totalCarNumb];
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for (int i = 0; i < 4; i++)
    {
        pthread_mutex_init(&updateLock[i], NULL);
        pthread_mutex_init(&turnLock[i], NULL);
        sem_init(&enterSem[i], 0, 1);
    }

    sem_init(&exitSem, 0, 1);
    sem_init(&printSem, 0, 1);

    for (int i = 0; i < totalCarNumb; i++)
    {
        delay1(delays[i]);
        newCar[i].delay = actualTime;
        newCar[i].originalDir = originalDirData[i];
        newCar[i].targetDir = targetDirData[i];
        newCar[i].cid = i;
        pthread_create(&threads[i], &attr, startCrossing, (void *)&newCar[i]);
    }

    for (int i = 0; i < totalCarNumb; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_exit(NULL);
    return 0;
}
