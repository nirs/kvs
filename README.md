# kvs

This is an experimental simple key value store for shared storage.

The database is on logical volume on a shared LUN, or on NFS or
GlusterFS mount.

The user must ensure that there is a single writer in the cluster. One
way is using sanlock exclusive lease.

Multiple readers can access the database while the writer is modifying
it without any locking.

Multiple concurrent readers on a host can see stale pages mapped by a
concurrent reader instead of fresh pages read from the shared storage.
This can be solved by ensuring single reader, but I'm not sure it is
really needed if all readers are quick.

## Building

Install these packages:
- lmdb-devel for Fedora/CentoOS (from EPEL).
- gcc
- make

To build:

    make

## Creating a database

To create a database on NFS:

    $ ./kvs /mnt/kvs.db init 50

This creates a 50 MiB database.

To create a new database on block device, create a database file and
copy it into the block device:

    $ sudo lvchange -ay kvs/db
    $ chown user:group /dev/kvs/db
    $ ./kvs tmp.db init 50
    $ dd if=tmp.db of=/dev/kvs/db
    $ rm tmp.db

lmbd does not support yet block device on master. Using rawpart branch
this hack should not be needed.

## Setting key

The value is read from stdin:

    $ echo "it works!" | kvs /dev/kvs/db set foo

## Getting keys

    $ kvs /dev/kvs/db get foo
    it works!

Nothing else is implemented yet.

## Todo

- ensure single reader on a host
- bulk operations
