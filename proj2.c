#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>

// named constants
#define REQ_ARG 5
#define BASE_TEN 10
#define MIN_TI 0
#define MAX_TI 1000
#define MIN_TB 0
#define MAX_TB 1000

#define write_string(atom, i, string) do{ \
    sem_wait(s_write);\
    fprintf(output,"%d:\t%c\t%d:\t%s\n",(*counter_lines)++,atom,i,string);\
    fflush(output);\
    sem_post(s_write);}\
    while (0)

#define write_string_molecule(atom, i) do{ \
    sem_wait(s_write);\
    fprintf(output,"%d:\t%c\t%d:\tmolecule %d created\n",(*counter_lines)++,atom,i,(*molecule_counter));\
    fflush(output);\
    sem_post(s_write);}\
    while (0)

#define write_string_creating_molecule(atom, i) do{ \
    sem_wait(s_write);\
    fprintf(output,"%d:\t%c\t%d:\tcreating molecule %d\n",(*counter_lines)++,atom,i,(*molecule_counter));\
    fflush(output);\
    sem_post(s_write);}\
    while (0)

#define wait_till_done(atoms) do{\
                sem_wait(s_block);\
                (*atom3)++;\
                if (*atom3 == 3){\
                (*molecule_counter)++;\
                if(*molecule_counter == atoms)\
                    sem_post(s_finished);\
                    (*atom3) = 0;\
                }\
                sem_post(s_block);}\
                while (0)
enum Errors {
    Errors_NONE,
    Errors_SEM,
    Errors_SHM
};

// global variables
int NO, NH, TI, TB;
FILE *output;

int initial_counter_lines=1;
int initial_molecule_counter=1;
//
//shared variables
int *hydrogens = NULL,    //hydrogens counter
*oxygens = NULL,     //oxygens counter
*counter_lines = NULL, //number of output line
*molecule_counter = NULL, //created molecules counter
*atom3 = NULL;        //atoms counter

//semaphores inicialization
sem_t *s_oxygen;
sem_t *s_hydrogen;
sem_t *barrier;
sem_t *s_creation;
sem_t *s_block;
sem_t *s_finished;
sem_t *s_write;

// forward declarations
void argument_parsing(int argc, char *argv[]); //reading arguments
int str_to_int(char *str, int *out); //convert string to integer
int initialize(void); //initialzation of semahores, output file
void create_oxygen_process(int NO, int TI, int TB); //creation the process Kyslik
void create_hydrogen_process(int n, int gh, int b); //creation the process Vodik
void create_molecule(char atom, int i, int TB); //creation of molecules
int all_free(void); //free memory

void create_oxygen_process(int NO, int TI, int TB){
    srand(getpid());
    pid_t pid;
    pid_t *children = malloc(NO*sizeof(pid_t));
    int i;
    int delay_time = 0;
    for (i = 1; i <= NO; i++){
        if (TI != 0){
            delay_time = (rand() % TI);
            usleep(delay_time * 1000);
            pid = fork();
        }
        if (pid < 0) {
            fprintf(stderr,"Fork failed\n");
            free(children);
            all_free();
        }
        else if (pid == 0){
            srand(getpid());
            write_string('O',i,"started");
            sem_wait(barrier);
            if (*hydrogens > 1){
                write_string('O',i,"going to queue");
                (*hydrogens)--;
                (*hydrogens)--;
                sem_post(s_hydrogen);
                sem_post(s_hydrogen);
                create_molecule('O',i,TB);
                sem_wait(s_creation);
                write_string_molecule('O',i);
                wait_till_done(NO);
                sem_post(barrier);
            }
            else {
                write_string('O', i, "going to queue");
                (*oxygens)++;
                sem_post(barrier);
                sem_wait(s_oxygen);
                create_molecule('O', i, TB);
                sem_wait(s_creation);
                write_string_molecule('O', i);
                wait_till_done(NO);
            }

                exit(0);
        }
    }
    for (int j = 0; j < NO; j++)
        if(waitpid(children[j], NULL, 0) == -1){
            fprintf(stderr,"%s","Fork failed\n");
        }
            free(children);
            exit (0);
}

void create_hydrogen_process(int NH, int TI, int TB){
    srand(getpid());
    pid_t pid;
    pid_t *children = malloc(NH*sizeof(pid_t));
    int i;
    int delay_time = 0;
    for (i = 1; i <= NH; i++){
        if (TI != 0 ) {
            delay_time = (rand() % TI);
            usleep(delay_time * 1000);
            pid = fork();
        }
        if (pid < 0) {
            fprintf(stderr,"Fork failed\n\n");
            free(children);
            all_free();
        }
        else if (pid == 0){
            srand(getpid());
            write_string('H', i, "started");
            sem_wait(barrier);
            if (*hydrogens>0 && *oxygens > 0){
                write_string('H',i,"going to queue");
                (*hydrogens)--;
                (*oxygens)--;
                sem_post(s_hydrogen);
                sem_post(s_oxygen);
                create_molecule('H',i,TB);
                sem_wait(s_creation);
                write_string_molecule('H',i);
                wait_till_done(NH/2);
                sem_post(barrier);
            }
            else {
                write_string('H', i, "going to queue");
                (*hydrogens)++;
                sem_post(barrier);
                sem_wait(s_hydrogen);
                create_molecule('H', i, TB);
                sem_wait(s_creation);
                write_string_molecule('H', i);
                wait_till_done(NH / 2);
            }
            exit(0);
        }
    }
    for (int j = 0; j < NH; j++)
        if(waitpid(children[j], NULL, 0) == -1){
            fprintf(stderr,"%s","Fork failed\n");
        }
    free(children);
    exit (0);
}

void create_molecule(char atom, int i, int TB){
    sem_wait(s_block);
    if (*atom3 == 0)
        sem_wait(s_creation);
    write_string_creating_molecule(atom,i);
    int delay_time = 0;
    if (TB != 0 ) {

        srand(getpid());
        delay_time = (rand() % TB);
    }
    usleep(delay_time * 1000);
    (*atom3)++;
    if (*atom3 == 3){
        sem_post(s_block);
        sem_post(s_creation);
        sem_post(s_creation);
        sem_post(s_creation);
        sem_post(s_creation);
        (*atom3) = 0;
        return;
    }
    sem_post(s_block);
}
//
int str_to_int(char *str, int *output) {
    char *endptr = NULL;
    *output = (int)strtol(str, &endptr, BASE_TEN);
    return (*endptr != '\0' || *str == '\0');
}

void argument_parsing(int argc, char *argv[]) {
    if (argc != REQ_ARG) {
        fprintf(stderr, "Invalid argument count.\n");
        exit(EXIT_FAILURE);
    }
    int error = 0;
    error += str_to_int(argv[1], &NO);
    error += str_to_int(argv[2], &NH);
    error += str_to_int(argv[3], &TI);
    error += str_to_int(argv[4], &TB);
    if (error) {
        perror("Argument conversion to integer failed");
        exit(EXIT_FAILURE);
    }
    if (TI < MIN_TI || TI > MAX_TI) {
        fprintf(stderr, "The maximum time, that an oxygen / hydrogen atom waits after its formation before queuing to form molecules has to be from the interval <0,1000>.\n");
        exit(EXIT_FAILURE);
    }
    if (TB < MIN_TB || TB > MAX_TB) {
        fprintf(stderr, "The maximum time required to create a single molecule has to be from the interval <0,1000>.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    argument_parsing(argc, argv);
    initialize();
    pid_t create_oxyg, create_hyd=0;
    create_oxyg = fork();
    if (create_oxyg == 0){
        create_oxygen_process(NO, TI, TB);
    }
    else if (create_oxyg > 0){
        create_hyd = fork();
        if (create_hyd == 0){
            create_hydrogen_process(NH, TI, TB);
        }
        else if (create_hyd < 0){
            fprintf(stderr,"%s","Fork failed\n");
            all_free();
            return 2;
        }
    }
    else{
        fprintf(stderr,"%s","Fork failed\n");
        all_free();
        return 2;
    }
}

int initialize(void){
    output = fopen("proj2.out", "w");
    if (output == NULL) {
        perror("Failed to open proj2.out");
        exit(EXIT_FAILURE);
    }
    setlinebuf(output);

    int error = Errors_NONE;

    //Inicialisation of semaphores
    if ((s_oxygen= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((s_hydrogen= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((barrier= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((s_creation= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((s_block= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((s_finished= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    if ((s_write= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;

    if (error == Errors_NONE){
        if ((sem_init(s_oxygen, 1, 0) == -1) || (sem_init(s_hydrogen, 1, 0) == -1) || \
        (sem_init(barrier, 1, 1) == -1) || (sem_init(s_creation, 1, 1) == -1) || \
        (sem_init(s_block, 1, 1) == -1) || (sem_init(s_finished, 1, 0) == -1) || \
        (sem_init(s_write, 1, 1))) error = Errors_SEM;
    }

    //Inicialisation of shared variables
    if (error == Errors_NONE){
        if ((oxygens= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
        if ((hydrogens= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
        if ((counter_lines= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
        if ((molecule_counter= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
        if ((atom3= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED) error = Errors_SEM;
    }
    return error;
}

int all_free(void){
    fclose(output);
    int error = Errors_NONE;
    if ((sem_destroy(s_oxygen) == -1) || (sem_destroy(s_hydrogen) == -1) || (sem_destroy(barrier) == -1) || \
    (sem_destroy(s_creation) == -1) || (sem_destroy(s_block) == -1)  || (sem_destroy(s_finished) == -1) || \
    (sem_destroy(s_write) == -1)) error = Errors_SEM;
    if ( error == Errors_SEM) fprintf(stderr, "Semaphores  could not be destroyed\n");
    if (error == Errors_SHM) fprintf(stderr, "Memory could not be released\n");
    return error;
}
