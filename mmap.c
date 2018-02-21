#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#define LENGTH 80

struct db {
    sem_t *sem;
    int locked;
    int fd;
    char *addr;
};

static char *flatten(const char *path)
{
    char *res;
    char *p;

    res = strdup(path);

    for (p = res + 1; *p; p++) {
        if (*p == '/')
            *p = '.';
    }

    return res;
}

static int open_db_fd(struct db *db, const char *name, int writeable)
{
    db->fd = open(name, writeable ? O_RDWR : O_RDONLY);
    if (db->fd == -1)
        return errno;

    return 0;
}

static int lock_db_sem(struct db *db, const char *name, int timeout)
{
    struct timespec deadline;
    char *semname;
    int rc;
    int err = 0;

    if (clock_gettime(CLOCK_REALTIME, &deadline))
        return errno;

    deadline.tv_sec += timeout;
    semname = flatten(name);

    db->sem = sem_open(semname, O_CREAT, 0600, 1);
    if (db->sem == SEM_FAILED)
        err = errno;

    free(semname);

    if (err)
        return err;

    do {
        rc = sem_timedwait(db->sem, &deadline);
    } while (rc == -1 && errno == EINTR);

    if (rc == -1)
        return errno;

    db->locked = 1;

    return 0;
}

static int map_db_addr(struct db *db, int writeable)
{
    int prot;

    prot = PROT_READ;
    if (writeable)
        prot |= PROT_WRITE;

    db->addr = mmap(NULL, LENGTH, prot, MAP_SHARED, db->fd, 0);
    if (db->addr == MAP_FAILED)
        return errno;

    return 0;
}

static void close_db(struct db *db)
{
    if (db->addr != MAP_FAILED) {
        munmap(db->addr, LENGTH);
        db->addr = MAP_FAILED;
    }

    if (db->locked) {
        sem_post(db->sem);
        db->locked = 0;
    }

    if (db->sem != SEM_FAILED) {
        sem_close(db->sem);
        db->sem = SEM_FAILED;
    }

    if (db->fd != -1) {
        close(db->fd);
        db->fd = -1;
    }
}

static int open_db(struct db *db, const char *name, int writeable, int timeout) {
    int err = 0;

    /* Init db */
    db->fd = -1;
    db->sem = SEM_FAILED;
    db->locked = 0;
    db->addr = MAP_FAILED;

    err = open_db_fd(db, name, writeable);
    if (err)
        goto out;

    err = lock_db_sem(db, name, timeout);
    if (err)
        goto out;

    err = map_db_addr(db, writeable);
    if (err)
        goto out;

out:
    if (err)
        close_db(db);

    return err;
}

int do_get(const char *path)
{
    struct db db;
    int err;

    err = open_db(&db, path, 0 /* writeable */, 2 /* timeout */);
    if (err) {
        fprintf(stderr, "open_db: (%d) %s\n", err, strerror(err));
        return 1;
    }

    fwrite(db.addr, 1, LENGTH, stdout);

    close_db(&db);

    return 0;
}

int do_put(const char *path, const char *value)
{
    struct db db;
    int err;
    int res = 0;

    if (value == NULL) {
        fprintf(stderr, "Usage: mmap PATH put VALUE\n");
        return 2;
    }

    err = open_db(&db, path, 1 /* writeable */, 2 /* timeout */);
    if (err) {
        fprintf(stderr, "open_db: (%d) %s\n", err, strerror(err));
        return 1;
    }

    snprintf(db.addr, LENGTH, "%s\n", value);

    if (msync(db.addr, LENGTH, MS_SYNC)) {
        fprintf(stderr, "msync: (%d) %s\n", errno, strerror(errno));
        res = 1;
    }

    close_db(&db);

    return res;
}

int do_sleep(const char *path, int timeout)
{
    struct db db;
    int err;

    err = open_db(&db, path, 0 /* writeable */, 2 /* timeout */);
    if (err) {
        fprintf(stderr, "open_db: (%d) %s\n", err, strerror(err));
        return 1;
    }

    sleep(timeout);

    close_db(&db);

    return 0;
}

int main(int argc, char *argv[])
{
    const char *path;
    const char *cmd;

    if (argc < 3) {
        fprintf(stderr, "Usage: mmap PATH [ get | put VALUE | sleep SEC ]\n");
        exit(2);
    }

    path = argv[1];
    cmd = argv[2];

    if (strcmp(cmd, "get") == 0) {
        return do_get(path);
    } else if (strcmp(cmd, "put") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: mmap PATH put VALUE\n");
            return 2;
        }
        return do_put(path, argv[3]);
    } else if (strcmp(cmd, "sleep") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: mmap PATH sleep SEC\n");
            return 2;
        }
        return do_sleep(path, atoi(argv[3]));
    } else {
        fprintf(stderr, "Unkown command: '%s'\n", cmd);
        exit(2);
    }
}
