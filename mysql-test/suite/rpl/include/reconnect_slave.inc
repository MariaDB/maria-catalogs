--echo # Ensure that all connections to slave are active again

let $org_connection=$CURRENT_CONNECTION;
--disable_connect_log
--connection slave_config
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection slave
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection server_2
--enable_reconnect
--source include/wait_until_connected_again.inc
--connection $org_connection
--enable_connect_log
