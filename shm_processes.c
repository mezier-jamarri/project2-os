#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

void ParentProcess(int *SharedMem, sem_t *mutex, int parent_id);
void StudentProcess(int *SharedMem, sem_t *mutex, int id);

int main(int argc, char *argv[])
{
    int ShmID;
    int *ShmPTR;
    pid_t pid;
    int status;
    sem_t *mutex;

    int num_parents = 1;
    int num_children = 1;

    if (argc == 3) {
        num_parents = atoi(argv[1]);
        num_children = atoi(argv[2]);
        if ((num_parents != 1 && num_parents != 2) || num_children <= 0) {
            printf("usage:  ref_psdd <parents> <children> (where parent = [1 | 2], children = [N > 0]\n");
            exit(1);
        }
    }

    ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }

    ShmPTR = (int *) shmat(ShmID, NULL, 0);
    if (*ShmPTR == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }

    ShmPTR[0] = 0; // Initialize BankAccount to 0

    sem_unlink("psddsemaphore");
    if ((mutex = sem_open("psddsemaphore", O_CREAT, 0644, 1)) == SEM_FAILED) {
        perror("semaphore initilization");
        exit(1);
    }

    // Fork Poor Student process(es)
    for (int i = 1; i <= num_children; i++) {
        pid = fork();
        if (pid < 0) {
            printf("*** fork error (child) ***\n");
            exit(1);
        } else if (pid == 0) {
            StudentProcess(ShmPTR, mutex, i);
            exit(0);
        }
    }

    // Fork Loveable Mom if num_parents == 2
    if (num_parents == 2) {
        pid = fork();
        if (pid < 0) {
            printf("*** fork error (child) ***\n");
            exit(1);
        } else if (pid == 0) {
            ParentProcess(ShmPTR, mutex, 2);
            exit(0);
        }
    }

    // Main parent runs Dear Old Dad
    ParentProcess(ShmPTR, mutex, 1);

    for (int i = 0; i < num_children + (num_parents - 1); i++) {
        wait(&status);
        printf("Process has detected the completion of its child...\n");
    }
    shmdt((void *) ShmPTR);
    shmctl(ShmID, IPC_RMID, NULL);
    exit(0);
}

void ParentProcess(int *SharedMem, sem_t *mutex, int parent_id)
{
    srandom(getpid());
    int localBalance = 0;

    if (parent_id == 1) {
        const char *name = "Dear Old Dad";
        while (1) {
            sleep(random() % 6);
            printf("%s: Attempting to Check Balance\n", name);

            sem_wait(mutex);
            int rand_num = random();
            if (rand_num % 2 == 0) {
                if (localBalance < 100) {
                    localBalance = SharedMem[0];
                    int amount = random() % 101;
                    if (amount % 2 == 0) {
                        localBalance += amount;
                        printf("%s: Deposits $%d / Balance = $%d\n", name, amount, localBalance);
                        SharedMem[0] = localBalance;
                    } else {
                        printf("%s: Doesn't have any money to give\n", name);
                    }
                } else {
                    printf("%s: Thinks Student has enough Cash ($%d)\n", name, localBalance);
                }
            } else {
                localBalance = SharedMem[0];
                printf("%s: Last Checking Balance = $%d\n", name, localBalance);
            }
            sem_post(mutex);
        }
    } else {
        const char *name = "Loveable Mom";
        while (1) {
            sleep(random() % 11);
            printf("%s: Attempting to Check Balance\n", name);

            sem_wait(mutex);
            localBalance = SharedMem[0];
            if (localBalance <= 100) {
                int amount = random() % 126;
                localBalance += amount;
                printf("%s: Deposits $%d / Balance = $%d\n", name, amount, localBalance);
                SharedMem[0] = localBalance;
            }
            sem_post(mutex);
        }
    }
}

void StudentProcess(int *SharedMem, sem_t *mutex, int id)
{
    srandom(getpid());
    int localBalance = 0;

    while (1) {
        sleep(random() % 6);
        printf("Poor Student #%d: Attempting to Check Balance\n", id);

        sem_wait(mutex);
        int rand_num = random();
        if (rand_num % 2 == 0) {
            localBalance = SharedMem[0];
            int need = random() % 51;
            printf("Poor Student needs $%d\n", need);
            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student #%d: Withdraws $%d / Balance = $%d\n", id, need, localBalance);
            } else {
                printf("Poor Student #%d: Not Enough Cash ($%d)\n", id, localBalance);
            }
            SharedMem[0] = localBalance;
        } else {
            localBalance = SharedMem[0];
            printf("Poor Student #%d: Last Checking Balance = $%d\n", id, localBalance);
        }
        sem_post(mutex);
    }
}