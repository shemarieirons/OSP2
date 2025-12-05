#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>

#define SEM_NAME "/bank_sem"

void dad_process(int *BankAccount, sem_t *mutex);
void mom_process(int *BankAccount, sem_t *mutex);
void student_process(int *BankAccount, sem_t *mutex);

int main(int argc, char *argv[]) {
    int ShmID;
    int *BankAccount;
    sem_t *mutex;
    pid_t pid;
    int num_parents, num_children;

    if (argc != 3) {
        printf("Usage: %s <num_parents> <num_children>\n", argv[0]);
        printf("num_parents: 1 (Dad only) or 2 (Dad and Mom)\n");
        printf("num_children: number of Poor Students\n");
        exit(1);
    }

    num_parents = atoi(argv[1]);
    num_children = atoi(argv[2]);

    if (num_parents < 1 || num_parents > 2 || num_children < 1) {
        printf("Invalid arguments. num_parents must be 1 or 2, num_children >= 1\n");
        exit(1);
    }

    srand(time(NULL));

    // Create shared memory for BankAccount
    ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("shmget");
        exit(1);
    }

    BankAccount = (int *)shmat(ShmID, NULL, 0);
    if ((long)BankAccount == -1) {
        perror("shmat");
        exit(1);
    }

    *BankAccount = 0;
    printf("Server: Shared BankAccount created, initial value = %d\n", *BankAccount);

    // Create semaphore
    mutex = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    printf("Server: Semaphore created.\n");

    // Fork Mom if num_parents == 2
    pid_t mom_pid = -1;
    if (num_parents == 2) {
        mom_pid = fork();
        if (mom_pid < 0) {
            perror("fork for mom");
            exit(1);
        } else if (mom_pid == 0) {
            // Mom process
            mom_process(BankAccount, mutex);
            exit(0);
        }
    }

    // Fork children (Students)
    for (int i = 0; i < num_children; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork for child");
            exit(1);
        } else if (pid == 0) {
            // Student process
            student_process(BankAccount, mutex);
            exit(0);
        }
    }

    // Main process is Dad
    dad_process(BankAccount, mutex);

    // Wait for all children
    for (int i = 0; i < num_children; i++) {
        wait(NULL);
    }

    // Wait for Mom if forked
    if (mom_pid != -1) {
        waitpid(mom_pid, NULL, 0);
    }

    // Cleanup
    sem_close(mutex);
    sem_unlink(SEM_NAME);
    shmdt((void *)BankAccount);
    shmctl(ShmID, IPC_RMID, NULL);
    printf("Server: Cleanup done. Exiting.\n");

    return 0;
}

void dad_process(int *BankAccount, sem_t *mutex) {
    int localBalance;
    while (1) {
        sleep(rand() % 6);  // 0-5 seconds
        printf("Dear Old Dad: Attempting to Check Balance\n");

        sem_wait(mutex);
        localBalance = *BankAccount;

        int r = rand() % 2;  // 0 = even, 1 = odd

        if (r == 0) {  // attempt deposit
            if (localBalance < 100) {
                int amount = rand() % 100;  // 0-99
                if (amount % 2 == 0) {
                    localBalance += amount;
                    printf("Dear Old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                    *BankAccount = localBalance;
                } else {
                    printf("Dear Old Dad: Doesn't have any money to give\n");
                }
            } else {
                printf("Dear Old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            }
        } else {
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }

        sem_post(mutex);
    }
}

void mom_process(int *BankAccount, sem_t *mutex) {
    int localBalance;
    while (1) {
        sleep(rand() % 11);  // 0-10 seconds
        printf("Lovable Mom: Attempting to Check Balance\n");

        sem_wait(mutex);
        localBalance = *BankAccount;

        if (localBalance <= 100) {
            int amount = rand() % 126;  // 0-125
            localBalance += amount;
            printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
            *BankAccount = localBalance;
        }

        sem_post(mutex);
    }
}

void student_process(int *BankAccount, sem_t *mutex) {
    int localBalance;
    while (1) {
        sleep(rand() % 6);  // 0-5 seconds
        printf("Poor Student: Attempting to Check Balance\n");

        sem_wait(mutex);
        localBalance = *BankAccount;

        int r = rand() % 2;  // 0 = even, 1 = odd

        if (r == 0) {  // attempt withdrawal
            int need = rand() % 50 + 1;  // 1-50
            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
            }

            *BankAccount = localBalance;
        } else {
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }

        sem_post(mutex);
    }
}
