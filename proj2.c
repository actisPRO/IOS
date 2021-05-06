/*
 * Project #2
 * Brno University of Technology | Faculty of Information Technology
 * Course: Operation Systems | Summer semester 2021
 * Author: Denis Karev (xkarev00@stud.fit.vutbr.cz)
 * This project should not be used for non-educational purposes.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>  
#include <semaphore.h>
#include <sys/wait.h>
#include <errno.h>

typedef struct s_shmem
{
    int A;                      // Action number.
    sem_t *sem_elves;           // Semaphore for controlling elves, waiting on front of the workshop
    sem_t *sem_elves_help;      // Semaphore for controlling elves, inside of the workshop and waiting for Santa's help
    sem_t *sem_rdeer;           // Semaphore for controlling reindeer, waiting to get hitched
    sem_t *sem_santa;           // Semaphore for controlling Santa (and his sleep)
    sem_t *sem_santa_sleep;     // Semaphore for controlling Santa, while he is helping elves
    sem_t *mutex;       
    int elves;                  // Number of elves, waiting in front of the workshop
    int reindeer;               // Number of reindeer, waiting to get hitched

    int workshop_closed;        // Is workshop closed?
} SharedMemory;

SharedMemory *mem = NULL;

FILE *out;

int n_rdeer;
int n_elves;
int t_elf;
int t_rdeer;

void santa()
{
    sem_wait(mem->mutex);
        fprintf(out, "%d: Santa: going to sleep\n", mem->A++);
        fflush(out);
    sem_post(mem->mutex);

    while (!mem->workshop_closed)
    {
        sem_wait(mem->sem_santa); 
        sem_wait(mem->mutex);
            if (mem->reindeer == n_rdeer)
            {
                mem->workshop_closed = 1;
                fprintf(out, "%d: Santa: closing workshop\n", mem->A++);
                fflush(out);
                for (int i = 0; i < n_rdeer; ++i) sem_post(mem->sem_rdeer);  // allowing reindeer to get hitched              
                sem_post(mem->mutex);
            }
            else if (mem->elves == 3)
            { 
                fprintf(out, "%d: Santa: helping elves\n", mem->A++);
                fflush(out);
                for (int i = 0; i < 3; ++i) sem_post(mem->sem_elves_help);   // allowing elves in the workshop to get help
                sem_post(mem->mutex);

                sem_wait(mem->sem_santa_sleep);                            
                sem_wait(mem->mutex);
                    fprintf(out, "%d: Santa: going to sleep\n", mem->A++);  
                    fflush(out);
                sem_post(mem->mutex);
            }   
    }

    sem_wait(mem->sem_santa_sleep);
    sem_wait(mem->mutex);
        fprintf(out, "%d: Santa: Christmas started\n", mem->A++); // YAY!
        fflush(out);
        for (int i = 0; i < n_elves; ++i) sem_post(mem->sem_elves_help); // free all elves, waiting in the queue.
    sem_post(mem->mutex);
    exit(0);
}

void elf(int eID)
{
    sem_wait(mem->mutex);
        fprintf(out, "%d: Elf %d: started\n", mem->A++, eID);
        fflush(out);
    sem_post(mem->mutex);

    while (!mem->workshop_closed)
    {
        int time = (rand() % (t_elf - 0 + 1)) + 0;
        usleep(time * 1000);

        sem_wait(mem->mutex);
            fprintf(out, "%d: Elf %d: need help\n", mem->A++, eID);
            fflush(out);
        sem_post(mem->mutex);
        
        sem_wait(mem->sem_elves); // controlling number of elves in the workshop
        sem_wait(mem->mutex);
            mem->elves += 1;
            if (mem->elves == 3 && !mem->workshop_closed)
                sem_post(mem->sem_santa);
            else 
                sem_post(mem->sem_elves);
        sem_post(mem->mutex);
        
        sem_wait(mem->sem_elves_help);
        if (mem->workshop_closed) // freeing elves, stuck in the queue
        {
            sem_post(mem->sem_elves);
            break;
        }

        sem_wait(mem->mutex);
            fprintf(out, "%d: Elf %d: get help\n", mem->A++, eID);
            fflush(out);
        sem_post(mem->mutex);
        
        sem_wait(mem->mutex);
            mem->elves--;
            if (mem->elves == 0)
            {
                sem_post(mem->sem_santa_sleep); // let Santa sleep :zzz:
                sem_post(mem->sem_elves);
            }
        sem_post(mem->mutex);
    }    
        
    sem_wait(mem->mutex);
        fprintf(out, "%d: Elf %d: taking holidays\n", mem->A++, eID);
        fflush(out);
    sem_post(mem->mutex);
    exit(0);
}

void rdeer(int rID)
{
    sem_wait(mem->mutex);
        fprintf(out, "%d: RD %d: rstarted\n", mem->A++, rID);
        fflush(out);
    sem_post(mem->mutex);

    int time = (rand() % (t_rdeer - t_rdeer / 2 + 1)) + t_rdeer / 2;
    usleep(time * 1000);

    sem_wait(mem->mutex);
        fprintf(out, "%d: RD %d: return home\n", mem->A++, rID);
        fflush(out);

        mem->reindeer++;
        if (mem->reindeer == n_rdeer) sem_post(mem->sem_santa);
    sem_post(mem->mutex);  

    sem_wait(mem->sem_rdeer);
    sem_wait(mem->mutex);
        fprintf(out, "%d: RD %d: get hitched\n", mem->A++, rID);
        fflush(out);

        mem->reindeer--;
        if (mem->reindeer == 0) sem_post(mem->sem_santa_sleep);
    sem_post(mem->mutex);
    exit(0);
}

void clean()
{
    fclose(out);

    if (mem == NULL) return;

    sem_unlink("xkarev00_elves");
    sem_unlink("xkarev00_elves_help");
    sem_unlink("xkarev00_mutex");
    sem_unlink("xkarev00_rdeers");
    sem_unlink("xkarev00_santa");
    sem_unlink("xkarev00_santa_sleep");

    if (munmap(mem, sizeof(SharedMemory)) == -1)
    {
        fprintf(stderr, "Error: failed to remove shared memory.\n");
        exit(1);
    }    
}

int main(int argc, char *argv[]) 
{
    out = fopen("proj2.out", "w");

    // Check arguments
    if (argc != 5)
    {
        fprintf(stderr, "Error: unexpected amount of arguments. Use ./proj2 NE NR TE TR.\n");
        return 1;
    }

    n_elves = atoi(argv[1]); // number of elves
    if (n_elves <= 0 || n_elves > 1000)
    {
        fprintf(stderr, "Error: amount of elves must be between 0 and 1000.\n");
        return 1;
    }

    n_rdeer = atoi(argv[2]); // number of reindeers
    if (n_rdeer <= 0 || n_rdeer > 20)
    {
        fprintf(stderr, "Error: amount of reindeers must be between 0 and 20.\n");
        return 1;
    }

    t_elf = atoi(argv[3]); // max time for an elf to work alone
    if (t_elf < 0 || t_elf > 1000)
    {
        fprintf(stderr, "Error: elf time must be between 0 and 1000.\n");
        return 1;
    }
    
    t_rdeer = atoi(argv[4]); // max time for a reindeer to return from its holiday.
    if (t_rdeer < 0 || t_rdeer > 1000)
    {
        fprintf(stderr, "Error: elf time must be between 0 and 1000.\n");
        return 1;
    }

    // Create shared memory
    mem = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        fprintf(stderr, "Error: failed to create shared memory. Errno: %d.\n", errno);
        return 1;
    }

    mem->A = 1;

    // Semaphores
    mem->sem_rdeer = sem_open("xkarev00_rdeers", O_CREAT | O_EXCL, 0644, 0);
    if (mem->sem_rdeer == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d.\n", errno);
        clean();
        return 1;
    }
    mem->sem_elves = sem_open("xkarev00_elves", O_CREAT | O_EXCL, 0644, 1);    
    if (mem->sem_elves == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d.\n", errno);
        clean();
        return 1;
    }   
    mem->sem_elves_help = sem_open("xkarev00_elves_help", O_CREAT | O_EXCL, 0644, 0);
    if (mem->sem_elves_help == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d.\n", errno);
        clean();
        return 1;
    }
    mem->sem_santa = sem_open("xkarev00_santa", O_CREAT | O_EXCL, 0644, 0);    
    if (mem->sem_santa == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d.\n", errno);
        clean();
        return 1;
    }
    mem->sem_santa_sleep = sem_open("xkarev00_santa_sleep", O_CREAT | O_EXCL, 0644, 0);    
    if (mem->sem_santa_sleep == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d.\n", errno);
        clean();
        return 1;
    }
    mem->mutex = sem_open("xkarev00_mutex", O_CREAT | O_EXCL, 0644, 1);    
    if (mem->mutex == SEM_FAILED)
    {
        fprintf(stderr, "Error: failed to open a semaphore. Errno: %d. \n", errno);
        clean();
        return 1;
    }

    mem->elves = 0;
    mem->reindeer = 0;
    mem->workshop_closed = 0;

    pid_t santaPID = fork();
    if (santaPID == -1)
    {
        fprintf(stderr, "Error: can't fork Santa. Errno: %d.\n", errno);
        clean();
        return 1;
    }

    if (santaPID == 0)
        santa();

    // Create elves and reindeers.
    int elfId = 0;
    pid_t elfPID = 1;
    for (int i = 0; i < n_elves; ++i)
    {
        ++elfId;
        elfPID = fork();

        if (elfPID == -1)
        {
            fprintf(stderr, "Error: can't fork elf. Errno: %d.\n", errno);
            clean();
            return 1;
        }

        if (elfPID == 0)
            elf(elfId);
    }

    int reinId = 0;
    pid_t reinPID;
    for (int i = 0; i < n_rdeer; ++i)
    {
        ++reinId;
        reinPID = fork();

        if (reinPID == -1)
        {
            fprintf(stderr, "Error: can't fork reindeer. Errno: %d.\n", errno);
            clean();
            return 1;
        }

        if (reinPID == 0)
            rdeer(reinId);            
    }

    // Clean everything and exit
    while (wait(NULL) > 0); // Wait, until all the child processes aren't running.

    clean();

    return 0;
}