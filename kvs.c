/*
 * kvs - simple key value store
 * Copyright (C) 2018  Nir Soffer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "db.h"

static int get(const char *path, char *key);
static int set(const char *path, char *key, char *value);
static int del(const char *path, char *key);
static int init(const char *path, int size_mb);

int main(int argc, char * argv[])
{
    const char *path;
    const char *cmd;

    if (argc < 3) {
        fprintf(stderr, "Usage: kvs PATH [ init | get KEY | set KEY | del KEY ]\n");
        exit(2);
    }

    path = argv[1];
    cmd = argv[2];

    if (strcmp(cmd, "get") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: kvs PATH get KEY\n");
            exit(2);
        }
        return get(path, argv[3]);
    } else if (strcmp(cmd, "set") == 0) {
        if (argc < 4 || argc > 5) {
            fprintf(stderr, "Usage: kvs PATH set KEY [VALUE]\n");
            exit(2);
        }
        return set(path, argv[3], argv[4]);
    } else if (strcmp(cmd, "del") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: kvs PATH del KEY\n");
            exit(2);
        }
        return del(path, argv[3]);
    } else if (strcmp(cmd, "init") == 0) {
        int size_mb;

        if (argc != 4) {
            fprintf(stderr, "Usage: kvs PATH init size-mb\n");
            exit(2);
        }

        size_mb = atoi(argv[3]);
        if (size_mb == 0) {
            fprintf(stderr, "Invalid database size: '%s'\n", argv[3]);
            exit(2);
        }

        return init(path, size_mb);
    } else {
        fprintf(stderr, "unknown command: '%s'\n", cmd);
        exit(2);
    }
}

static int get(const char *path, char *key)
{
    int rc;
    struct db db = {0};
    MDB_val value;

    rc = db_open(&db, path, 1);
    if (rc)
        goto leave;

    rc = db_get(&db, key, &value);
    if (rc) {
        fprintf(stderr, "mbd_get: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = fwrite(value.mv_data, value.mv_size, 1, stdout);
    if (rc != 1) {
        fprintf(stderr, "fwrite: (%d) %s\n", rc, strerror(errno));
        goto leave;
    }

leave:
    db_close(&db);

    return rc == 0 ? 0 : 1;
}

static int set(const char *path, char *key, char *value)
{
    int rc;
    struct db db = {0};
    MDB_val val = {0};

    rc = db_open(&db, path, 0);
    if (rc)
        goto leave;

    if (value) {
        val.mv_data = value;
        val.mv_size = strlen(value);
    } else {
        val.mv_data = malloc(DB_MAX_VALUE_SIZE);
        if (val.mv_data == NULL) {
            fprintf(stderr, "malloc: (%d) %s\n", errno, strerror(errno));
            goto leave;
        }

        val.mv_size = fread(val.mv_data, 1, DB_MAX_VALUE_SIZE, stdin);
        if (ferror(stdin)) {
            fprintf(stderr, "fread: (%d) %s\n", errno, strerror(errno));
            goto leave;
        }
    }

    rc = db_put(&db, key, &val);
    if (rc) {
        fprintf(stderr, "mdb_put: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = db_commit(&db);

leave:
    if (val.mv_data != value)
        free(val.mv_data);

    db_close(&db);

    return rc == 0 ? 0 : 1;
}

static int del(const char *path, char *key)
{
    int rc;
    struct db db = {0};

    rc = db_open(&db, path, 0);
    if (rc)
        goto leave;

    rc = db_del(&db, key);
    if (rc) {
        fprintf(stderr, "mbd_del: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = db_commit(&db);

leave:
    db_close(&db);

    return rc == 0 ? 0 : 1;
}

static int init(const char *path, int size_mb)
{
    int rc;
    struct db db = {0};
    MDB_val value;

    db.mapsize = size_mb * 1024 * 1024;

    rc = db_open(&db, path, 0);
    if (rc)
        goto leave;

    value.mv_data =
        "{\n"
        "  \"version\": 1\n"
        "}\n";

    value.mv_size = strlen(value.mv_data);

    rc = db_put(&db, ".kvs", &value);
    if (rc) {
        fprintf(stderr, "mdb_put: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = db_commit(&db);

leave:
    db_close(&db);

    return rc == 0 ? 0 : 1;
}
