# ==== Purpose ====
#
# Create a connection to a given numbered server.
#
# This script is normally used internally by rpl_init.inc and
# master-slave.inc, but it can also be used in test cases that need to
# create more connections or re-create connections after disconnect.
#
#
# ==== Usage ====
#
# --let $rpl_connection_name= <connection_name>
# --let $rpl_server_number= <server_number>
# [--let $rpl_debug= 1]
# --source include/rpl_connect.inc
#
# Parameters:
#   $rpl_connection_name
#     The name of the connection to create.
#
#   $rpl_server_number
#     The number of the server to connect to.
#
#   $rpl_catalog catalog name. 'def' is used if not set.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= rpl_connect.inc [creating $rpl_connection_name]
--source include/begin_include_file.inc


if (!$rpl_server_number)
{
  --die ERROR IN TEST: You must set $rpl_server_number before sourcing include/rpl_connect.inc
}
if (!$rpl_connection_name)
{
  --die ERROR IN TEST: You must set $rpl_connection_name before sourcing include/rpl_connect.inc
}

# Get port number
--let $_rpl_port= \$SERVER_MYPORT_$rpl_server_number
if (!$_rpl_port)
{
  --echo Bug in test case: '\$SERVER_MYPORT_$rpl_server_number' not initialized. Check the test's .cnf file.
  --die Not all SERVER_MYPORT_* environment variables are setup correctly.
}

--let $_rpl_catalog= $rpl_catalog
--let $rpl_catalog=

if (!$_rpl_catalog)
{
  --let $_rpl_catalog=def
}

# Create connection.
if ($rpl_debug)
{
  --echo connect ($rpl_connection_name,127.0.0.1,root,,$_rpl_catalog.test,$_rpl_port,)
}
disable_connect_log;
--connect ($rpl_connection_name,127.0.0.1,root,,$_rpl_catalog.test,$_rpl_port,)
enable_connect_log;


--let $include_filename= rpl_connect.inc
--source include/end_include_file.inc
