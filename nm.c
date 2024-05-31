#define _GNU_SOURCE
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdatomic.h>

#define MAX_THREADS 2

typedef void (*taskFunc) (void);
struct Worker
{
    pthread_t handle;
    sigjmp_buf jmp;
    int state;
    int currTask;
    taskFunc funcs[3];
};
struct Worker workers[MAX_THREADS];
thread_local struct Worker *thread_data = nullptr;
atomic_uint thread_pending = 0;

/*Random dummy tasks to run on the workers*/
static void
task_foo (void)
{
    for (int i = 0; i < 20; i++)
    {
        printf ("foo'ing from %i, %p...\n", i, thread_data);
        sleep (1);
    }
}

atomic_int shared_value = 1;
static void
task_bar (void)
{
    while (shared_value >= 0)
    {
        printf ("bar'ing from %u, %p...\n", shared_value, thread_data);
        atomic_fetch_add (&shared_value, 0x10000);
        sleep (1);
    }
}

static void
task_qux (void)
{
    while (1)
    {
        printf ("doing some indefinite task on %p..\n", thread_data);
        sleep (1);
    }
}

/*Signal handler for the process, invoked on worker threads via pthread_kill*/
static void
signal_handler (int signal)
{
    switch (signal)
    {
    case SIGUSR1:
        siglongjmp(thread_data->jmp, -1);
        break;
    case SIGUSR2:
        thread_data->currTask = 0;
        siglongjmp(thread_data->jmp, -1);
        break;
    default:
        break;
    }
}

/*Main thread body*/
static void *
thread_main (void *user)
{   /*Detach self to simplify clean up*/
    pthread_detach (pthread_self ());

    struct Worker *state = (struct Worker *)user;
    state->state = 1;
    state->currTask = 0;
    state->funcs[0] = task_foo;
    state->funcs[1] = task_bar;
    state->funcs[2] = task_qux;

    /*Save off TLS pointer to our data for easy reference*/
    thread_data = state;

    /*Done setting up*/
    atomic_fetch_sub(&thread_pending, 1);

    sigsetjmp(state->jmp, -1);
    while (1)
    {
        while (state->currTask >= 3)
        {
            printf ("waiting for new work... %p\n", state);
            sleep (1);
        }
        int index = state->currTask++;
        state->funcs[index] ();
    }
    return nullptr;
}

int
main (int argc, char **argv)
{   /*Initialise the signal handler for the process.
    we use the SIGUSR1 and SIGUSR2 signals to interrupt workers*/
    struct sigaction axn;
    memset (&axn, 0, sizeof axn);
    axn.sa_handler = signal_handler;
    axn.sa_flags = 0;
    sigemptyset(&axn.sa_mask);
    if (sigaction (SIGUSR1, &axn, nullptr))
    {
        printf ("Failed to install signal handler for SIGUSR1\n");
        return -1;
    }
    if (sigaction (SIGUSR2, &axn, nullptr))
    {
        printf ("Failed to install signal handler for SIGUSR2\n");
        return -1;
    }

    /*Spawn worker threads*/
    for (unsigned i = 0; i < MAX_THREADS; i++)
    {
        atomic_fetch_add(&thread_pending, 1);
        memset (&workers[i], 0, sizeof workers[0]);
        pthread_create (
            &workers[i].handle,
            nullptr,
            thread_main,
            &workers[i]);
    }

    /*Wait until threads are initialised*/
    atomic_uint want = 0;
    while (!atomic_compare_exchange_strong(&thread_pending, &want, 0));

    /*User command loop*/
    while (1)
    {
        char data;
        scanf ("%c", &data);
        switch (data)
        {
        case 'a':
            pthread_kill (workers[0].handle, SIGUSR1);
            break;
        case 'A':
            pthread_kill (workers[0].handle, SIGUSR2);
            break;       
        case 'z':
            pthread_kill (workers[1].handle, SIGUSR1);
            break;
        case 'Z':
            pthread_kill (workers[0].handle, SIGUSR2);
            break;  
        case 'q':
            goto Exit;
        }
    }

    /*Kill workers and exit*/
Exit:
    for (unsigned i = 0; i < MAX_THREADS; i++)
    {
        pthread_kill (workers[i].handle, SIGKILL);
    }
    return 0;
}
/*compile with gcc -std=c2x*/
