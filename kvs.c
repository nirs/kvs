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
#include "lmdb.h"

#define MAX_VALUE_SIZE (1024*1024)

static int get(const char *path, char *key);
static int set(const char *path, char *key);
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
        if (argc != 4) {
            fprintf(stderr, "Usage: kvs PATH set KEY\n");
            exit(2);
        }
        return set(path, argv[3]);
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
            fprintf(stderr, "Invalid database size: '%d'\n", argv[3]);
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
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val kv;
    MDB_val dv;

	rc = mdb_env_create(&env);
    if (rc) {
        fprintf(stderr, "mbd_env_create error: %s\n", mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_env_open(env, path, MDB_NOSUBDIR | MDB_NOLOCK, 0664);
    if (rc) {
        fprintf(stderr, "mbd_env_open error: %s\n", mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc) {
        fprintf(stderr, "mbd_txn_begin: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
    if (rc) {
        fprintf(stderr, "mbd_dbi_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    kv.mv_data = key;
    kv.mv_size = strlen(key);

    rc = mdb_get(txn, dbi, &kv, &dv);
    if (rc) {
        fprintf(stderr, "mbd_get: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = fwrite(dv.mv_data, dv.mv_size, 1, stdout);
    if (rc != 1) {
        fprintf(stderr, "fwrite: (%d) %s\n", rc, strerror(errno));
        goto leave;
    }

	mdb_txn_abort(txn);
leave:
	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	return rc == 0 ? 0 : 1;
}

static int set(const char *path, char *key)
{
	int rc;
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val kv;
    MDB_val dv;

	rc = mdb_env_create(&env);
    if (rc) {
        fprintf(stderr, "mbd_env_create: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_env_open(env, path, MDB_NOSUBDIR | MDB_NOLOCK, 0664);
    if (rc) {
        fprintf(stderr, "mbd_env_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_txn_begin(env, NULL, 0, &txn);
    if (rc) {
        fprintf(stderr, "mbd_txn_begin: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
    if (rc) {
        fprintf(stderr, "mbd_dbi_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	kv.mv_data = key;
	kv.mv_size = strlen(kv.mv_data);

	dv.mv_data = malloc(MAX_VALUE_SIZE);
    if (dv.mv_data == NULL) {
        fprintf(stderr, "malloc: (%d) %s\n", errno, strerror(errno));
        goto leave;
    }

    dv.mv_size = fread(dv.mv_data, 1, MAX_VALUE_SIZE, stdin);

    if (ferror(stdin)) {
        fprintf(stderr, "fread: (%d) %s\n", errno, strerror(errno));
        goto leave;
    }

	rc = mdb_put(txn, dbi, &kv, &dv, 0);
	if (rc) {
		fprintf(stderr, "mdb_put: (%d) %s\n", rc, mdb_strerror(rc));
		goto leave;
	}

	rc = mdb_txn_commit(txn);
	if (rc) {
		fprintf(stderr, "mdb_txn_commit: (%d) %s\n", rc, mdb_strerror(rc));
		goto leave;
	}

leave:
    free(dv.mv_data);
	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	return rc == 0 ? 0 : 1;
}

static int del(const char *path, char *key)
{
    return 0;
}

static int init(const char *path, int size_mb)
{
	int rc;
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val key;
    MDB_val data;

	rc = mdb_env_create(&env);
    if (rc) {
        fprintf(stderr, "mbd_env_create: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

    rc = mdb_env_set_mapsize(env, size_mb * 1024 * 1024);
    if (rc) {
        fprintf(stderr, "mbd_env_set_mapsize: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_env_open(env, path, MDB_NOSUBDIR | MDB_NOLOCK, 0664);
    if (rc) {
        fprintf(stderr, "mbd_env_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_txn_begin(env, NULL, 0, &txn);
    if (rc) {
        fprintf(stderr, "mbd_txn_begin: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
    if (rc) {
        fprintf(stderr, "mbd_dbi_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto leave;
    }

	key.mv_data = ".kvs";
	key.mv_size = strlen(key.mv_data);

	data.mv_data =
        "{\n"
        "  \"version\": 1\n"
        "}\n";

	data.mv_size = strlen(data.mv_data);

	rc = mdb_put(txn, dbi, &key, &data, 0);
	if (rc) {
		fprintf(stderr, "mdb_put: (%d) %s\n", rc, mdb_strerror(rc));
		goto leave;
	}

	rc = mdb_txn_commit(txn);
	if (rc) {
		fprintf(stderr, "mdb_txn_commit: (%d) %s\n", rc, mdb_strerror(rc));
		goto leave;
	}

leave:
	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	return rc == 0 ? 0 : 1;
}
