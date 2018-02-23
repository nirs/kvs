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

#include <stdio.h>
#include <string.h>

#include "db.h"

int db_open(struct db *db, const char *path, int readonly)
{
    int rc;

    rc = mdb_env_create(&db->env);
    if (rc) {
        fprintf(stderr, "mbd_env_create error: %s\n", mdb_strerror(rc));
        goto error;
    }

    if (db->mapsize > 0) {
        /* Must be done *before* starting the tranaction. */
        rc = mdb_env_set_mapsize(db->env, db->mapsize);
        if (rc) {
            fprintf(stderr, "mbd_env_set_mapsize: (%d) %s\n", rc, mdb_strerror(rc));
            goto error;
        }
    }

    rc = mdb_env_open(db->env, path, MDB_NOSUBDIR | MDB_NOLOCK, 0664);
    if (rc) {
        fprintf(stderr, "mbd_env_open error: %s\n", mdb_strerror(rc));
        goto error;
    }

    rc = mdb_txn_begin(db->env, NULL, readonly ? MDB_RDONLY : 0, &db->txn);
    if (rc) {
        fprintf(stderr, "mbd_txn_begin: (%d) %s\n", rc, mdb_strerror(rc));
        goto error;
    }

    rc = mdb_dbi_open(db->txn, NULL, 0, &db->dbi);
    if (rc) {
        fprintf(stderr, "mbd_dbi_open: (%d) %s\n", rc, mdb_strerror(rc));
        goto error;
    }

    return 0;

error:
    db_close(db);
    return rc;
}

int db_get(struct db *db, char *key, MDB_val *value)
{
    MDB_val kv;

    kv.mv_data = key;
    kv.mv_size = strlen(key);

    return mdb_get(db->txn, db->dbi, &kv, value);
}

int db_put(struct db *db, char *key, MDB_val *value)
{
    MDB_val kv;

    kv.mv_data = key;
    kv.mv_size = strlen(key);

    return mdb_put(db->txn, db->dbi, &kv, value, 0);
}

int db_del(struct db *db, char *key)
{
    MDB_val kv;

    kv.mv_data = key;
    kv.mv_size = strlen(key);

    return mdb_del(db->txn, db->dbi, &kv, NULL);
}

int db_commit(struct db *db)
{
    int rc;

    rc = mdb_txn_commit(db->txn);
    if (rc)
        fprintf(stderr, "mdb_txn_commit: (%d) %s\n", rc, mdb_strerror(rc));

    db->txn = NULL;

    return rc;
}

void db_abort(struct db *db)
{
    mdb_txn_abort(db->txn);
    db->txn = NULL;
}

void db_close(struct db *db)
{
    db_abort(db);

    mdb_dbi_close(db->env, db->dbi);
    mdb_env_close(db->env);
    db->env = NULL;
}
