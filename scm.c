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
    char *base; /* root address (usig char* is more convenient) */
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
    char *base = NULL;
    size_t used = 0;

    fd = open(pathname, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
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

    base = mmap((void *)VIRT_ADDR, info.st_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (base == MAP_FAILED)
    {
        TRACE("mmap() failed");
        close(fd);
        free(scm);
        return NULL;
    }

    if (!truncate)
    {
        /* find the previous utilized address */
        off_t end;

        if ((end = lseek(fd, -sizeof(size_t), SEEK_END)) == -1)
        {
            close(fd);
            free(scm);
            return NULL;
        }

        if (read(fd, &used, sizeof(size_t)) == -1)
        {
            close(fd);
            free(scm);
            return NULL;
        }
    }

    scm->fd = fd;
    scm->size = info.st_size;
    scm->utilized = used;
    scm->base = base;

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
        /* record the memory we have utilized */
        if (lseek(scm->fd, -sizeof(size_t), SEEK_END) != (off_t)-1)
        {
            if (write(scm->fd, &(scm->utilized), sizeof(scm->utilized)) == -1)
            {
                TRACE("close write() failed");
                close(scm->fd);
                return;
            }
        }
        /* use msync() to sync the content to disk */
        msync(scm->base, scm->size, MS_SYNC);

        /* unmap the memory */
        munmap(scm->base, scm->size);
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

    if (!scm || n == 0)
    {
        TRACE("invalid input");
        return NULL;
    }

    if (scm->utilized + n >= scm->size)
    {
        TRACE("out of scm memory");
        return NULL;
    }
    /* linear allocator */
    pos = scm->base + scm->utilized;
    scm->utilized += n;

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
    if (!pos)
    {
        TRACE("scm_malloc() failed");
        return NULL;
    }

    strncpy(pos, s, len);

    return pos;
}

/**
 * Analogous to the standard C free function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * p  : a pointer to the start of a previously allocated memory
 */

/*
void scm_free(struct scm *scm, void *p)
{
    // extra point
}
*/

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
        return scm->base;
    }

    return NULL;
}