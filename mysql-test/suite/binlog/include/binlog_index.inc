#
# This include file is used by more than one test suite
# (currently binlog and binlog_encryption).
# Please check all dependent tests after modifying it
#

#
# testing of purging of binary log files bug#18199/Bug#18453
#
source include/have_log_bin.inc;
source include/not_embedded.inc;
# Don't test this under valgrind, memory leaks will occur
--source include/not_valgrind.inc
source include/have_debug.inc;
# Avoid CrashReporter popup on Mac
--source include/not_crashrep.inc
call mtr.add_suppression('Attempting backtrace');
call mtr.add_suppression('MYSQL_BIN_LOG::purge_logs failed to process registered files that would be purged.');
call mtr.add_suppression('MYSQL_BIN_LOG::open failed to sync the index file');
call mtr.add_suppression('Turning logging off for the whole duration of the MariaDB server process.');
call mtr.add_suppression('Could not use .*');
call mtr.add_suppression('MYSQL_BIN_LOG::purge_logs failed to clean registers before purging logs.');
flush tables;

SET @saved_dbug = @@SESSION.debug_dbug;

RESET MASTER;

let $INDEX=$MARIADB_TOPDIR/master-bin.index;

#
# testing purge binary logs TO
#

flush logs;
flush logs;
flush logs;

source include/show_binary_logs.inc;
remove_file $MARIADB_TOPDIR/master-bin.000001;
flush tables;

# there must be a warning with file names
replace_regex /\.[\\\/]master/master/;
--source include/wait_for_binlog_checkpoint.inc
purge binary logs TO 'master-bin.000004';

--echo *** must show a list starting from the 'TO' argument of PURGE ***
source include/show_binary_logs.inc;

#
# testing purge binary logs BEFORE
#

reset master;

flush logs;
flush logs;
flush logs;
remove_file $MARIADB_TOPDIR/master-bin.000001;

--echo *** must be a warning master-bin.000001 was not found ***
let $date=`select NOW()  +  INTERVAL 1 MINUTE`;
--disable_query_log
replace_regex /\.[\\\/]master/master/;
--source include/wait_for_binlog_checkpoint.inc
eval purge binary logs BEFORE '$date';
--enable_query_log

--echo *** must show one record, of the active binlog, left in the index file after PURGE ***
source include/show_binary_logs.inc;

#
# testing a fatal error
# Turning a binlog file into a directory must be a portable setup
# 

reset master;

flush logs;
flush logs;
flush logs;

remove_file $MARIADB_TOPDIR/master-bin.000001;
mkdir $MARIADB_TOPDIR/master-bin.000001;

--source include/wait_for_binlog_checkpoint.inc
--error ER_BINLOG_PURGE_FATAL_ERR
purge binary logs TO 'master-bin.000002';
replace_regex /\.[\\\/]master/master/;
show warnings;
rmdir $MARIADB_TOPDIR/master-bin.000001;
--disable_warnings
reset master;
--enable_warnings

--echo # crash_purge_before_update_index
flush logs;

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_purge_before_update_index";
--source include/wait_for_binlog_checkpoint.inc
--error 2013
purge binary logs TO 'master-bin.000002';

--enable_reconnect
--source include/wait_until_connected_again.inc

file_exists $MARIADB_TOPDIR/master-bin.000001;
file_exists $MARIADB_TOPDIR/master-bin.000002;
file_exists $MARIADB_TOPDIR/master-bin.000003;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # crash_purge_non_critical_after_update_index
flush logs;

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_purge_non_critical_after_update_index";
--source include/wait_for_binlog_checkpoint.inc
--error 2013
purge binary logs TO 'master-bin.000004';

--enable_reconnect
--source include/wait_until_connected_again.inc

--error 1
file_exists $MARIADB_TOPDIR/master-bin.000001;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000002;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000003;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # crash_purge_critical_after_update_index
flush logs;

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_purge_critical_after_update_index";
--source include/wait_for_binlog_checkpoint.inc
--error 2013
purge binary logs TO 'master-bin.000006';

--enable_reconnect
--source include/wait_until_connected_again.inc

--error 1
file_exists $MARIADB_TOPDIR/master-bin.000004;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000005;
file_exists $MARIADB_TOPDIR/master-bin.000006;
file_exists $MARIADB_TOPDIR/master-bin.000007;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000008;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # crash_create_non_critical_before_update_index
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_create_non_critical_before_update_index";
--error 2013
flush logs;

--enable_reconnect
--source include/wait_until_connected_again.inc

file_exists $MARIADB_TOPDIR/master-bin.000008;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000009;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # crash_create_critical_before_update_index
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_create_critical_before_update_index";
--error 2013
flush logs;

--enable_reconnect
--source include/wait_until_connected_again.inc

file_exists $MARIADB_TOPDIR/master-bin.000009;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000010;
--error 1
file_exists $MARIADB_TOPDIR/master-bin.000011;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # crash_create_after_update_index
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION debug_dbug="+d,crash_create_after_update_index";
--error 2013
flush logs;

--enable_reconnect
--source include/wait_until_connected_again.inc

file_exists $MARIADB_TOPDIR/master-bin.000010;
file_exists $MARIADB_TOPDIR/master-bin.000011;
--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo #
--echo # This should put the server in unsafe state and stop
--echo # accepting any command. If we inject a fault at this
--echo # point and continue the execution the server crashes.
--echo #

--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # fault_injection_registering_index
SET SESSION debug_dbug="+d,fault_injection_registering_index";
-- replace_regex /\.[\\\/]master/master/
-- error ER_CANT_OPEN_FILE
flush logs;

--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--source include/restart_mysqld.inc

--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--echo # fault_injection_updating_index
SET SESSION debug_dbug="+d,fault_injection_updating_index";
-- replace_regex /\.[\\\/]master/master/
-- error ER_CANT_OPEN_FILE
flush logs;

--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

--source include/restart_mysqld.inc

--chmod 0644 $INDEX
-- replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
-- eval SET @index=LOAD_FILE('$index')
-- replace_regex /\.[\\\/]master/master/
SELECT @index;

SET @@SESSION.debug_dbug = @saved_dbug;

--echo End of tests
