#ifndef PROC_MAPS_H
#define PROC_MAPS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>    /* size_t */
#include <sys/types.h> /* off_t pid_t */
#include <sys/mman.h>  /* PROT_* MAP_PRIVATE MAP_SHARED */

typedef struct _proc_maps_record
{
    void * addr_beg;
    void * addr_end;
    int prot;
    int flag; /* MAP_PRIVATE or MAP_SHARED */
    off_t offset;
    unsigned int dev_major;
    unsigned int dev_minor;
    int inode;
    char const * pathname;
}
proc_maps_record;

/*
 * On fail function returns negative value.
 * To stop iteration return non-zero value from callback.
*/
int proc_maps_iterate(
        pid_t pid_,
        int(* callback_)(proc_maps_record const * record_, void * context_),
        void * context_   /* = NULL */,
        char * rbuf_      /* = NULL */,
        size_t rbuf_size_ /* = 0 */);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* PROC_MAPS_H */
