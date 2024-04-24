--echo # Ensure that all connections to master are active again

let $org_connection=$CURRENT_CONNECTION;
--disable_connect_log
--connection default
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection master_config
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection master
--enable_reconnect
--source include/wait_until_connected_again.inc
# rpl_end.inc needs to use the connection server_1
--connection server_1
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection $org_connection
--enable_connect_log
