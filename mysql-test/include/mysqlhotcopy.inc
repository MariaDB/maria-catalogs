# Test of mariadb-hotcopy (perl script)
# Author: Horst Hunger
# Created: 2010-05-10

--source include/not_windows.inc
--source include/not_embedded.inc
--source include/have_dbi_dbd-mariadb.inc

if (!$MYSQLHOTCOPY)
{
  # Fail the test if the mysqlhotcopy script is missing.
  # If the tool's location changes, mysql-test-run.pl must be updated to
  # reflect this (look for "MYSQLHOTCOPY").
  die due to missing mariadb-hotcopy tool;
}

# NOTE (johnemb, 2011-01-26):
#       In this test mysqlhotcopy (a perl script) is executed as a standalone
#       executable, i.e. not necessarily using the perl interpreter in PATH,
#       because that is how the documentation demonstrates it.
#
#       We include have_dbi_dbd-mariadb.inc above so that the test will
#       be skipped if Perl modules required by the mysqlhotcopy tool are not
#       found when the script is run this way.

--disable_service_connection
--disable_warnings
DROP DATABASE IF EXISTS hotcopy_test;
--enable_warnings
CREATE DATABASE hotcopy_test;
USE hotcopy_test;
eval CREATE TABLE t1 (c1 int, c2 varchar(20)) ENGINE=$engine;
eval CREATE TABLE t2 (c1 int, c2 varchar(20)) ENGINE=$engine;
eval CREATE TABLE t3 (c1 int, c2 varchar(20)) ENGINE=$engine;

INSERT INTO t1 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');
INSERT INTO t2 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');
INSERT INTO t3 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');

--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_test

# backup into another database in the same directory
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save

--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save

USE hotcopy_save;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

# restore data into the original database with mysqlhotcopy
if(`SELECT engine= 'MyISAM' FROM information_schema.tables WHERE table_name='t1'`)
{
USE hotcopy_test;
DELETE FROM t1;
SELECT * FROM t1;

--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --addtodest -S $MASTER_MYSOCK -u root hotcopy_save hotcopy_test

USE hotcopy_save;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;
}

USE hotcopy_test;
DROP TABLE t2;
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_test

--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --addtodest -S $MASTER_MYSOCK -u root hotcopy_save hotcopy_test

FLUSH TABLES;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

# backup of db into a directory
USE hotcopy_test;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test $MYSQLTEST_VARDIR/tmp
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--list_files $MYSQLTEST_VARDIR/tmp/hotcopy_test
#--exec rm -rf $MYSQLTEST_VARDIR/tmp/hotcopy_test
--disable_warnings
--remove_files_wildcard $MYSQLTEST_VARDIR/tmp/hotcopy_test *
--enable_warnings
--rmdir $MYSQLTEST_VARDIR/tmp/hotcopy_test

# backup without full index files
# reproduction of bug#53556, "--list_files" shows MYI files, which is wrong.
DROP DATABASE hotcopy_save;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --noindices -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save

# test of option "allowold"
DROP DATABASE hotcopy_save;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--error 1,9,11,110,2304,255
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --allowold -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save

# test of option "keepold"
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --keepold -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save_old
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_save

# test of option "suffix"
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --suffix=_cpy -S $MASTER_MYSOCK -u root hotcopy_test
--replace_result $MARIADB_DATADIR MARIADB_DATADIR
--list_files $MARIADB_DATADIR/hotcopy_test_cpy
DROP DATABASE hotcopy_test_cpy;

DROP DATABASE hotcopy_test;
DROP DATABASE hotcopy_save;
DROP DATABASE hotcopy_save_old;
--enable_service_connection
