/**
*                      REAL TIME SYSTEMS
* ---------------------------------------------------------------------------------
*
*    Title: RTEMS program for reading an audio file and send to an arduino controller
*
*    Authors: Diego del Río Manzanas / Jaime Ortega Pérez / Fernando Burrieza Galán
*
*    Version: 1.1
*
*/


/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <rtems.h>
#include <rtems/shell.h>
#include <rtems/untar.h>
#include <bsp.h>
#include <string.h>

#ifdef RASPBERRYPI
#include <bsp/i2c.h>
#endif


/**********************************************************
 *  CONSTANTS
 *********************************************************/
#define NSEC_PER_SEC 1000000000UL

#ifdef RASPBERRYPI
#define DEV_NAME "/dev/i2c"
#else
#define DEV_NAME "/dev/com1"
#endif
#define FILE_NAME "/let_it_be.raw"

#define PERIOD_TASK_READ  32000000    /* Period of read and send task (32 milliseconds) */
#define PERIOD_TASK_COMMANDS 2        /* Period of receive_commands (in seconds) */
#define PERIOD_TASK_SHOW_STATE 5     /* Period of show_state (in seconds) */
#define SEND_SIZE 128               /* BYTES */

#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

#define SLAVE_ADDR 0x8

/**********************************************************
 *  GLOBALS
 *********************************************************/
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;

// Global variable that stores the state of the reproduction
int commandPause = 0;

// Structures with the priorities for our tasks
struct sched_param readSendPriority = {
    sched_priority:3
};
struct sched_param receiveCommandsPriority = {
    sched_priority:2
};
struct sched_param showStatePriority = {
    sched_priority:1
};

// Definition of the mutex and the mutexattr
pthread_mutex_t mutex;
pthread_mutexattr_t mutexattr;

// Definition of the threads and the threads attributes
pthread_t readSend;
pthread_attr_t readSendAttr;
pthread_t receiveCommands;
pthread_attr_t receiveCommandsAttr;
pthread_t showState;
pthread_attr_t showStateAttr;

/**********************************************************
 * Function: diffTime
 *********************************************************/
void diffTime(struct timespec end,
              struct timespec start,
              struct timespec *diff)
{
    if (end.tv_nsec < start.tv_nsec) {
        diff->tv_nsec = NSEC_PER_SEC - start.tv_nsec + end.tv_nsec;
        diff->tv_sec = end.tv_sec - (start.tv_sec+1);
    } else {
        diff->tv_nsec = end.tv_nsec - start.tv_nsec;
        diff->tv_sec = end.tv_sec - start.tv_sec;
    }
}

/**********************************************************
 * Function: addTime
 *********************************************************/
void addTime(struct timespec end,
              struct timespec start,
              struct timespec *add)
{
    unsigned long aux;
    aux = start.tv_nsec + end.tv_nsec;
    add->tv_sec = start.tv_sec + end.tv_sec +
                  (aux / NSEC_PER_SEC);
    add->tv_nsec = aux % NSEC_PER_SEC;
}

/**********************************************************
 * Function: compTime
 *********************************************************/
int compTime(struct timespec t1,
              struct timespec t2)
{
    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec == t2.tv_nsec) {
            return (0);
        } else if (t1.tv_nsec > t2.tv_nsec) {
            return (1);
        } else if (t1.tv_sec < t2.tv_sec) {
            return (-1);
        }
    } else if (t1.tv_sec > t2.tv_sec) {
        return (1);
    } else if (t1.tv_sec < t2.tv_sec) {
        return (-1);
    }
    return (0);
}

/**********************************************************
 *  Function: read_send_audio
 *********************************************************/
void * read_send_audio(void *param) {

    // Time structs for time computings
    struct timespec start,end,diff,cycle;

    // Variables
    unsigned char buf[SEND_SIZE];
    int fd_file = -1;
    int fd_serie = -1;
    int ret = 0;

    #ifdef RASPBERRYPI
        // Init the i2C driver
        rpi_i2c_init();

        // Bus registering, this init the ports needed for the conexion
        // and register the device under /dev/i2c
    #define I2C_HZ 1000000
        printf("Register I2C device %s (%d, Hz) \n",DEV_NAME, I2C_HZ);
        rpi_i2c_register_bus("/dev/i2c", I2C_HZ);

        // Open device file
        printf("open I2C device %s \n",DEV_NAME);
        fd_serie = open(DEV_NAME, O_RDWR);
        if (fd_serie < 0) {
            printf("open: error opening serial %s\n", DEV_NAME);
            exit(-1);
        }

        // Register the address of the slave to comunicate with
        ioctl(fd_serie, I2C_SLAVE, SLAVE_ADDR);
    #else
        /* Open serial port */
        printf("open serial device %s \n",DEV_NAME);
        fd_serie = open (DEV_NAME, O_RDWR);
        if (fd_serie < 0) {
            printf("open: error opening serial %s\n", DEV_NAME);
            exit(-1);
        }
    #endif

    /* Open music file */
    printf("open file %s begin\n",FILE_NAME);
    fd_file = open (FILE_NAME, O_RDWR);
    if (fd_file < 0) {
        perror("open: error opening file \n");
        exit(-1);
    }

    // Loading cycle time
    cycle.tv_sec=0;
    cycle.tv_nsec=PERIOD_TASK_READ;

    // Getting start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        // Locking mutex for reading the critical section to see if reproduction is paused
        pthread_mutex_lock(&mutex);

        if (commandPause == 1){

            // Unlocking mutex after reading
            pthread_mutex_unlock(&mutex);

            // Writing only 0s on the serial/I2C port as reproduction is paused
            memset(buf, 0, SEND_SIZE);
            ret=write(fd_serie,buf,SEND_SIZE);

        } else {

            // Unlocking mutex after reading
            pthread_mutex_unlock(&mutex);

            // Read from music file
            ret=read(fd_file,buf,SEND_SIZE);
            if (ret < 0) {
                printf("read: error reading file\n");
                exit(-1);
            }

            // Write on the serial/I2C port
            ret=write(fd_serie,buf,SEND_SIZE);

        }

        // Checking if any error while writing
        if (ret < 0) {
            printf("write: error writing serial\n");
            exit(-1);
        }

        // Get end time, calculate lapso, sleep and compute new start time
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
            exit(-1);
        }
        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }
}

/**********************************************************
 *  Function: receive_commands
 *********************************************************/
void * receive_commands(void *param) {

    // Time structs for time computings
    struct timespec start,end,diff,cycle;

    // Aux variable for the reading of the character
    char received = '1';

	  // Loading cycle time
    cycle.tv_sec=PERIOD_TASK_COMMANDS;
    cycle.tv_nsec=0;

    // Getting start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        // Checking if we received from keyboard a '1' or a '0' to resume or pause the reproduction
        if(received == '1'){

            // We use mutex as we change a global variable that is used by another task too
            pthread_mutex_lock(&mutex);
            commandPause = 0;
            pthread_mutex_unlock(&mutex);

        } else if(received == '0'){

            // We use mutex as we change a global variable that is used by another task too
            pthread_mutex_lock(&mutex);
            commandPause = 1;
            pthread_mutex_unlock(&mutex);

        }

        // Wait until a character is read from keyboard
        while(0 >= scanf("%c", &received));

        // Get end time, calculate lapso, sleep and compute new start time
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        diff.tv_nsec = diff.tv_nsec % PERIOD_TASK_COMMANDS;
        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }
}

/**********************************************************
 *  Function: show_state
 *********************************************************/
void * show_state(void *param) {

    // Time structs for time computings
    struct timespec start,end,diff,cycle;

    // Loading cycle time
    cycle.tv_sec=PERIOD_TASK_SHOW_STATE;
    cycle.tv_nsec=0;

    // Getting start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        // Locking mutex for reading the critical section to see if reproduction is paused
        pthread_mutex_lock(&mutex);

        if(commandPause == 1){

            // Unlocking mutex after reading and printing state
            pthread_mutex_unlock(&mutex);
            printf("Reproduction paused\n");

        } else{

            // Unlocking mutex after reading and printing state
            pthread_mutex_unlock(&mutex);
            printf("Reproduction resumed\n");

        }

        // Get end time, calculate lapso, sleep and compute new start time
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
            exit(-1);
        }
        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }
}

/*****************************************************************************
 * Function: Init()
 *****************************************************************************/
rtems_task Init (rtems_task_argument ignored) {

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

   // Initializing mutex
   pthread_mutexattr_init(&mutexattr);
   pthread_mutexattr_setprotocol(&mutexattr, PTHREAD_PRIO_PROTECT);
   pthread_mutexattr_setprioceiling(&mutexattr, 3);
   pthread_mutex_init(&mutex, &mutexattr);

   // Initializing attributes
   pthread_attr_init(&readSendAttr);
   pthread_attr_setschedpolicy(&readSendAttr, SCHED_FIFO);
   pthread_attr_setinheritsched(&readSendAttr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_init(&receiveCommandsAttr);
   pthread_attr_setschedpolicy(&receiveCommandsAttr, SCHED_FIFO);
   pthread_attr_setinheritsched(&receiveCommandsAttr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_init(&showStateAttr);
   pthread_attr_setschedpolicy(&showStateAttr, SCHED_FIFO);
   pthread_attr_setinheritsched(&showStateAttr, PTHREAD_EXPLICIT_SCHED);

   // Setting the priorities for the priority scheduler
   pthread_attr_setschedparam(&readSendAttr, &readSendPriority);
   pthread_attr_setschedparam(&receiveCommandsAttr, &receiveCommandsPriority);
   pthread_attr_setschedparam(&showStateAttr, &showStatePriority);

   // Create the threads to do the different tasks
   pthread_create(&readSend, &readSendAttr, &read_send_audio, NULL);
   pthread_create(&receiveCommands, &receiveCommandsAttr, &receive_commands, NULL);
   pthread_create(&showState, &showStateAttr, &show_state, NULL);

   // Ending and destroying the threads and attributes
   pthread_join(readSend, NULL);
   pthread_join(receiveCommands, NULL);
   pthread_join(showState, NULL);
   pthread_attr_destroy(&readSendAttr);
   pthread_attr_destroy(&receiveCommandsAttr);
   pthread_attr_destroy(&showStateAttr);

    // Destroying the mutex
    pthread_mutex_destroy(&mutex);

    exit(0);

} /* End of Init() */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_INIT
#include <rtems/confdefs.h>