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

#ifndef KVS_DB_H
#define KVS_DB_H

#include <lmdb.h>

#define DB_MAX_VALUE_SIZE (1024*1024)

struct db {
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    int mapsize;
};

int db_open(struct db *db, const char *path, int readonly);
int db_get(struct db *db, char *key, MDB_val *value);
int db_put(struct db *db, char *key, MDB_val *value);
int db_del(struct db *db, char *key);
int db_commit(struct db *db);
void db_abort(struct db *db);
void db_close(struct db *db);

#endif
