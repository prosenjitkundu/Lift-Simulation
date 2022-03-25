#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#define NFLOORS 5
#define Lift 3
#define Pass 8

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct Floor_info
{
    int floor_no;        /* The floor index */
    int waitingtogoup;   /* The number of people waiting to go up */
    int waitingtogodown; /* The number of people waiting to go down */
    int up_arrow;        /* people going up wait on this */
    int down_arrow;      /* people going down wait on this */
    int lift_no_loc;
} Floor_info;
typedef struct LIFT
{
    int no;               /* which lift is it */
    int position;         /* which floor it is on */
    int direction;        /* going UP or DOWN */
    int peopleinlift;     /* number of people in the lift */
    int stops[NFLOORS];   /* for each stop how many people are waiting to get off */
    int stopsem[NFLOORS]; /* people in the lift wait to go up/down in one of the floor */
} LIFT;

int main()
{
    int process_id[Lift + Pass];
    /* Person *people[Pass]; */
    LIFT *l;
    LIFT *lp;
    LIFT *lf;
    Floor_info *f;
    Floor_info *fp;
    Floor_info *ff;

    int id1s[1000];
    int id2s[1000];
    int id3s[1000];

    int i, j, k, m, a, b;

    int xx = 0, yy = 0, zz = 0;

    // LIFT

    int id1 = shmget(IPC_PRIVATE, Lift * sizeof(LIFT), 0777 | IPC_CREAT); /* Acquire a shared memory size of LIFT structure multiply with number of Lift */
    l = (LIFT *)shmat(id1, NULL, 0);

    for (int i = 0; i < Lift; i++)
    {

        int j = ((Lift * (i + 1)) % NFLOORS);
        l[i].no = i;
        l[i].position = j; // Assign Random Position for the Lift

        if (l[i].position == 0 || l[i].position == 1 || l[i].position == 2)
        {
            l[i].direction = 1; //Direction of the Lift is Up
        }
        else
        {
            l[i].direction = 0; //Direction of the lift is Down
        }
        l[i].peopleinlift = 0; //Initially No people is in the Lift

        for (int x = 0; x < NFLOORS; x++)
        {
            l[i].stops[x] = 0;
        }

        for (k = 0; k < NFLOORS; k++)
        {
            l[i].stopsem[k] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
            semctl(l[i].stopsem[k], 0, SETVAL, 0);
        }
    }
    /*for (int i = 0; i < Lift; i++)
    {
        printf("%d, %d\n", l[i].no, l[i].position);
    }*/
    // FLOOR

    int id2 = shmget(IPC_PRIVATE, NFLOORS * sizeof(Floor_info), 0777 | IPC_CREAT); // Create the share memory for floor
    f = (Floor_info *)shmat(id2, NULL, 0);

    for (j = 0; j < NFLOORS; j++)
    {

        f[j].floor_no = j;
        /*printf("%d\n", f[j].floor_no);*/
        f[j].waitingtogodown = 0; //Number of people waiting to go down from jth floor
        f[j].waitingtogodown = 0; //Number of people waiting to go up from jth floor

        f[j].up_arrow = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT); //taking the size of the semaphore as 1
        semctl(f[j].up_arrow, 0, SETVAL, 0);                      //initialize the semaphore with value 1 and the size of the semaphore_arr is 0

        f[j].down_arrow = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT); //taking the size of the semaphore as 1
        semctl(f[j].down_arrow, 0, SETVAL, 0);                      //initialize the semaphore with value 1 and the size of the semaphore_arr is 0

        f[j].lift_no_loc = 100;
        for (m = 0; m < Lift; m++)
        {
            if (f[j].floor_no == l[m].position)
            {
                /*printf("%d, %d, %d\n", f[j].floor_no, l[m].position, l[m].no);*/
                f[j].lift_no_loc = l[m].no;
            }
        }
    }

    /*for (m = 0; m < NFLOORS; m++)
    {
        printf("%d, %d, %d, %d, %d, %d\n", f[m].floor_no, f[m].down_arrow, f[m].up_arrow, f[m].waitingtogodown, f[m].waitingtogoup, f[m].lift_no_loc);
    }*/

    int semaphore_arr[NFLOORS][Lift];
    struct sembuf pop, vop;

    int psem, psem2, liftcon[NFLOORS];

    for (i = 0; i < NFLOORS; i++)
    {
        for (int j = 0; j < Lift; j++)
        {
            semaphore_arr[i][j] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT); //taking the size of the semaphore_arrs as 1
            semctl(semaphore_arr[i][j], 0, SETVAL, 1);                      //initialize the semaphore_arr with value 1 and the size of the semaphore_arr is 0
        }
    }

    psem = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
    semctl(psem, 0, SETVAL, 1);

    psem2 = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
    semctl(psem2, 0, SETVAL, 1);

    for (i = 0; i < NFLOORS; i++)
    {
        liftcon[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        semctl(liftcon[i], 0, SETVAL, 0); // Initialize the semaphore_arr with value 0 and the size of the semaphore_arr is 0
    }

    pop.sem_num = 0;
    vop.sem_num = 0;
    pop.sem_flg = SEM_UNDO;
    vop.sem_flg = SEM_UNDO;
    pop.sem_op = -1;
    vop.sem_op = 1;

    //Forking The Processes for Person

    for (i = 0; i < Pass; i++)
    {
        pid_t pid1 = fork();
        if (pid1 == 0)
        {
            srand(i);

            fp = (Floor_info *)shmat(id2, NULL, 0); /* Attach to the child */
            lp = (LIFT *)shmat(id1, NULL, 0);       /* Attach to the child */
            int pres;
            pres = rand() % NFLOORS;
            int dest;
            dest = 0;

            while (1)
            {
                dest = rand() % NFLOORS;

                while (pres == dest)
                {
                    dest = rand() % NFLOORS;
                }

                if (pres < dest)
                {
                    /*printf("%d, %d\n", pres, dest);*/
                    P(psem);
                    fp[pres].waitingtogoup = fp[pres].waitingtogoup + 1;
                    V(psem);
                    /*printf("%d\n",fp[pres].waitingtogoup);*/
                    P(fp[pres].up_arrow);
                    /*printf("%d, %d\n", pres, dest);*/
                    int ldc = fp[pres].lift_no_loc;
                    /*printf("%d\n", lift_on_kth_floor);*/
                    P(psem);

                    lp[ldc].peopleinlift = lp[ldc].peopleinlift + 1;
                    /*printf("%d", lp[ldc].peopleinlift);*/
                    fp[pres].waitingtogoup = fp[pres].waitingtogoup - 1;
                    lp[ldc].stops[dest] = lp[ldc].stops[dest] + 1;
                    V(psem);
                    P(lp[ldc].stopsem[dest]); // clicking on the lift's floor botton
                    lp[ldc].stops[dest] = lp[ldc].stops[dest] - 1;
                }

                else
                {
                    /*printf("%d, %d\n", pres, dest);*/
                    P(psem);
                    fp[pres].waitingtogodown = fp[pres].waitingtogodown + 1;
                    V(psem);
                    /*printf("%d\n",fp[pres].waitingtogodown);*/
                    P(fp[pres].down_arrow);

                    int ldc = fp[pres].lift_no_loc; // Lift number at location pres
                    /*printf("%d\n", lift_on_kth_floor);*/
                    P(psem);
                    lp[ldc].peopleinlift = lp[ldc].peopleinlift + 1;
                    fp[pres].waitingtogodown = fp[pres].waitingtogodown - 1;
                    lp[ldc].stops[dest] = lp[ldc].stops[dest] + 1;
                    V(psem);
                    P(lp[ldc].stopsem[dest]); // clicking on the lift's floor botton
                    lp[ldc].stops[dest] = lp[ldc].stops[dest] - 1;
                }
                pres = dest;
            }
        }
    }

    //Forking The Processes for LIFT
    for (int j = 0; j < Lift; j++)
    {
        pid_t pidl = fork();
        if (pidl == 0)
        {
            ff = (Floor_info *)shmat(id2, NULL, 0); /* Attach to the child */
            lf = (LIFT *)shmat(id1, NULL, 0);       /* Attach to the child */

            while (1)
            {
                for (i = 0; i < NFLOORS; i++)
                {
                    if (lf[j].position == ff[i].floor_no) // IF the lift j is at i floor
                    {
                        P(semaphore_arr[i][j]);

                        P(psem2);

                        ff[i].lift_no_loc = lf[j].no;

                        /*printf("%d, %d\n",ff[i].lift_no_loc, ff[i].floor_no);*/

                        while (ff[i].waitingtogoup != 0) //someone wants to go up
                        {
                            if (lf[j].direction == 1)
                            {
                                P(psem);
                                V(ff[i].up_arrow);
                                V(psem);
                                sleep(2);
                            }
                            else
                                break;
                        }
                        /*printf("%d\n", lf[i].position);*/
                        while (ff[i].waitingtogodown != 0) //someone wants to go down
                        {
                            if (lf[j].direction == 0)
                            {
                                P(psem);
                                V(ff[i].down_arrow);
                                V(psem);
                                sleep(2);
                            }
                            else
                                break;
                        }

                        /*printf("%d, %d\n", ff[i].lift_no_loc, ff[i].floor_no);*/
                        for (m = 0; m < lf[j].stops[i]; m++)
                        {
                            P(psem);
                            /*printf("%d\n", lf[j].stops[i]);*/
                            V(lf[j].stopsem[i]); // Lift j on floor i if it is the destination floor for some person

                            lf[j].peopleinlift = lf[j].peopleinlift - 1;
                            
                            V(psem);
                        }

                        // LEFT IS AT GROUND FLOOR
                        if (lf[j].position == 0 && lf[j].direction == 0)
                        {
                            P(psem);
                            lf[j].direction = 1;
                            V(psem);
                        }
                        // LIFT IS AT TOP FLOOR
                        if ((lf[j].position == (NFLOORS - 1)) && lf[j].direction == 1)
                        {
                            P(psem);
                            lf[j].direction = 0;
                            V(psem);
                        }
                        V(psem2);
                        // Lift Movement in up or down

                        if (lf[j].direction == 1)
                        {
                            P(psem);
                            lf[j].position = lf[j].position + 1; // Lift Position is incremented by one
                            ff[++i].lift_no_loc = lf[j].no;      //store the lift at the specific floor after movement
                            V(psem);
                        }
                        if (lf[j].direction == 0)
                        { // LIFT j and FLOOR i
                            P(psem);
                            lf[j].position = lf[j].position - 1;
                            ff[--i].lift_no_loc = lf[j].no; // store the lift at the specific floor after movement
                            V(psem);
                        }
                        V(semaphore_arr[i][j]);
                    }
                }
            }
        }
    }
    while (1)
    {
        system("clear");
        for (j = 0; j < Lift; j++)
        {
            if (l[j].direction == 1)
            {
                printf("             _____\n");
                printf("LIFT NO %d,   | %d |  FLOOR %d DIRECTION   UP > \n", l[j].no + 1, l[j].peopleinlift, l[j].position + 1);
            }
            if (l[j].direction == 0)
            {
                printf("             _____\n");
                printf("LIFT NO %d,   | %d |  FLOOR %d DIRECTION   DOWN < \n", l[j].no + 1, l[j].peopleinlift, l[j].position + 1);
            }
            sleep(1);
        }
        sleep(2);
    }
}