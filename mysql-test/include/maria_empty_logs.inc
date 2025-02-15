# Maria help script.
# Cleans up all logs to give recovery a fresh start.
#
# Note that this script relies on the number of threads connect at start of
# this script, so one should run this script with a freshly started server
# for it to work.
#
# API: set mel_keep_control_file=1 if want to keep control file;
# uses vardir, port and socket.

connection default;
let $default_db=`select database()`;

#it will used at end of test for wait_for_status_var.inc primitive
#let $status_var= Threads_connected;
#let $status_var_value= query_get_value(SHOW STATUS LIKE 'Threads_connected', Value, 1);

connection admin;
-- echo * shut down mysqld, removed logs, restarted it
append_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
wait-maria_empty_logs.inc
EOF

--source include/mysqladmin_shutdown.inc

--disable_warnings
if (!$mel_keep_control_file)
{
  --error 0,1
  remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log_control;
}
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000001;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000002;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000003;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000004;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000005;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000006;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000007;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000008;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000009;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000010;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000011;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000012;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000013;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000014;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000015;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000016;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000017;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000018;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000019;
-- error 0,1
remove_file $MARIADB_TOPDIR/$MARIA_LOG/aria_log.00000020;
# hope there are not more than these logs...

-- error 0,1
remove_file $MARIADB_TOPDIR/aria_recovery.trace;
--enable_warnings

append_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
restart-maria_empty_logs.inc
EOF

connection default;
--source include/wait_until_connected_again.inc

# Make sure that all connections are restored
# This is disabled as 'Threads_connected' can't be trusted'
# (It may be affected by 'check_testcase()')
# --source include/wait_for_status_var.inc
# Restore current database as the effect of "use" was lost after restart

--disable_query_log
eval use $default_db;
--enable_query_log

#
# Ensure that we don't get warnings from mysql.priv (used by check_mysqld),
# sys_config or from test running after this one.
#
--disable_query_log
--disable_warnings
--disable_result_log
# Zerofill all Aria tables in mysql and sys
show table status from mysql;
show table status from sys;
--enable_result_log
--enable_warnings
--enable_query_log
