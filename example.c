#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "proc_maps.h"

static int proc_maps_callback(
        proc_maps_record const * record_, void * context_)
{
    (void)context_;
    char flag = '?';
    if (record_->flag == MAP_PRIVATE)
        flag = 'p';
    if (record_->flag == MAP_SHARED)
        flag = 's';
    printf("%08lx-%08lx %c%c%c%c %08lx %02d:%02d %-10d%s\n",
           (unsigned long int)record_->addr_beg,
           (unsigned long int)record_->addr_end,
           record_->prot & PROT_READ ? 'r' : '-',
           record_->prot & PROT_WRITE ? 'w' : '-',
           record_->prot & PROT_EXEC ? 'x' : '-',
           flag,
           record_->offset,
           record_->dev_major,
           record_->dev_minor,
           record_->inode,
           record_->pathname);
    return 0;
}

int main(int argc_, char * argv_[])
{
    unsigned long int pid = (unsigned long int)getpid();
    if (argc_ >= 2)
    {
        if (sscanf(argv_[1], "%lu", &pid) != 1)
        {
            fprintf(stderr, "Invalid command line!\n");
            fprintf(stderr, "Usage: %s [PID]\n", argv_[0]);
            return -1;
        }
    }

    {
        /* len("cat /proc/" + str(2**64) + "/maps") + 1 = 36 */
        char command[36];
        sprintf(command, "cat /proc/%lu/maps", pid);
        printf("$ %s\n", command);
        fflush(stdout);
        system(command);
    }

    printf("proc_maps_iterate:\n");
    int last_errno = errno;
    if (proc_maps_iterate(
                (pid_t)pid, proc_maps_callback, NULL, NULL, 0) < 0)
    {
        if (last_errno != errno)
            fprintf(stderr, "proc_maps_iterate failed: %s\n", strerror(errno));
        else
            fprintf(stderr, "proc_maps_iterate failed: parse error\n");
        return -1;
    }

    return 0;
}
