/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

/* research the above Needed API and design accordingly */
#define VIRT_ADDR 0x600000000000 /* the base address of the heap */

struct scm
{
    int fd;
    size_t size;
    size_t utilized;
    void *base; /* root address */
};

/**
 * Initializes an SCM region using the file specified in pathname as the
 * backing device, opening the regsion for memory allocation activities.
 *
 * pathname: the file pathname of the backing device
 * truncate: if non-zero, truncates the SCM region, clearning all data
 *
 * return: an opaque handle or NULL on error
 */

struct scm *scm_open(const char *pathname, int truncate)
{
    struct scm *scm;
    struct stat info;

    int fd;

    if ((fd = open(pathname, O_RDWR, S_IRUSR | S_IWUSR)) < 0)
    {
        TRACE("open file failed");
        return NULL;
    }
    if (fstat(fd, &info))
    {
        TRACE("fstat() failed");
        return NULL;
    }
    if (!S_ISREG(info.st_mode))
    {
        TRACE("not a regular file");
        return NULL;
    }

    if (!(scm = malloc(sizeof(struct scm))))
    {
        TRACE("out of memory");
        close(fd);
        return NULL;
    }

    scm->base = mmap((void *)VIRT_ADDR, info.st_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (scm->base == MAP_FAILED)
    {
        TRACE("mmap() failed");
        close(fd);
        free(scm);
        return NULL;
    }

    if (!truncate)
    {
        /* if not truncate, read the utilized in the header, use size_t space */
        printf("not truncate\n");
        scm->utilized = *(size_t *)scm->base;
        printf("utilized: %lu\n", scm->utilized);
    }
    else
    {
        /* if truncate, store the utilized in the header, use size_t space */
        printf("truncate\n");
        scm->utilized = 0;
        *(size_t *)scm->base = scm->utilized;
        printf("utilized: %lu\n", scm->utilized);
    }

    scm->fd = fd;
    scm->size = info.st_size;

    return scm;
}

/**
 * Closes a previously opened SCM handle.
 * Before closing, the SCM region is synced to the disk.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * Note: scm may be NULL
 */

void scm_close(struct scm *scm)
{
    if (scm)
    {
        if (msync(scm->base, scm->size, MS_SYNC) == -1)
        {
            TRACE("msync error");
        }

        if (munmap(scm->base, scm->size) == -1)
        {
            TRACE("munmap error");
        }

        close(scm->fd);

        memset(scm, 0, sizeof(struct scm));
        free(scm);
    }

    return;
}

/**
 * Analogous to the standard C malloc function, but using SCM region.
 * Allocate memory for input word(size n).
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * n  : the size of the requested memory in bytes
 *
 * return: a pointer to the start of the allocated memory or NULL on error
 */

void *scm_malloc(struct scm *scm, size_t n)
{
    void *pos = NULL;
    size_t *blockSize;

    if (!scm || n == 0)
    {
        TRACE("invalid input");
        return NULL;
    }

    if (scm->utilized + n + sizeof(size_t) > scm->size)
    {
        TRACE("out of scm memory");
        return NULL;
    }

    /* calculate the position of store the size */
    blockSize = (size_t *)((char *)scm->base + sizeof(size_t) + scm->utilized);
    *blockSize = n;

    printf("malloc blockSize: %lu\n", *blockSize);

    /* move the pointer to the actual start of the allocated block */
    pos = (void *)(blockSize + 1);
    printf("malloc pos: %p\n", pos);
    scm->utilized += (n + sizeof(size_t));

    /* update the memory header to store the new utilized value */
    *(size_t *)scm->base = scm->utilized;

    return pos;
}

/**
 * Analogous to the standard C strdup function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * s  : a C string to be duplicated.
 *
 * return: the base memory address of the duplicated C string or NULL on error
 */

char *scm_strdup(struct scm *scm, const char *s)
{
    char *pos = NULL;
    size_t len;

    if (!scm || !s)
    {
        TRACE("invalid input");
        return NULL;
    }

    /* plus a '\0' character to mark the end of the string */
    len = safe_strlen(s) + 1;

    if (scm->utilized + len > scm->size)
    {
        TRACE("out of scm memory");
        return NULL;
    }

    pos = scm_malloc(scm, len);
    printf("strdup pos: %p\n", pos);
    if (!pos)
    {
        TRACE("scm_malloc() failed");
        return NULL;
    }

    printf("strdup copy string: %s\n", pos);
    memcpy(pos, s, len);

    return pos;
}

/**
 * Analogous to the standard C free function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * p  : a pointer to the start of a previously allocated memory
 */

void scm_free(struct scm *scm, void *p)
{
    size_t size;
    if (!scm || !p)
    {
        TRACE("invalid input");
        return;
    }

    size = *(size_t *)((char *)p - sizeof(size_t)); /* get the size of the block by minus the metadata*/

    /* update utilized */
    /*scm->utilized = scm->utilized - size - sizeof(size_t);*/

    /* update the header information */
    /**(size_t *)scm->base = scm->utilized;*/

    return;
}

/**
 * Returns the number of SCM bytes utilized thus far.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the number of bytes utilized thus far
 */

size_t scm_utilized(const struct scm *scm)
{
    if (scm)
    {
        return scm->utilized;
    }

    return 0;
}

/**
 * Returns the number of SCM bytes available in total.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the number of bytes available in total
 */

size_t scm_capacity(const struct scm *scm)
{
    if (scm)
    {
        return scm->size - scm->utilized;
    }

    return 0;
}

/**
 * Returns the base memory address withn the SCM region, i.e., the memory
 * pointer that would have been returned by the first call to scm_malloc()
 * after a truncated initialization.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the base memory address within the SCM region
 *
 * maintain the root (first malloc) address
 */

void *scm_mbase(struct scm *scm)
{
    if (scm)
    {
        return (char *)scm->base + sizeof(size_t) + sizeof(size_t);
    }

    return NULL;
}