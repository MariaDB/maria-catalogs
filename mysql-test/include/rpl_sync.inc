# ==== Purpose ====
#
# Sync all servers in an arbitrary replication topology. This works
# only if the servers have been configured with rpl_init.inc (and
# possibly rpl_change_topology.inc).
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl_sync.inc
#
# Parameters:
#   $rpl_only_running_threads
#     By default, this script assumes that both the IO thread and the
#     SQL thread are running and fails if one of them is stopped.  If
#     $rpl_only_running_threads is set, this script first checks
#     which slave threads are running: 
#      - If both threads are running, sync both threads with master.
#      - If only IO thread is running, sync IO thread with master.
#      - If only SQL thread is running, sync SQL thread with IO thread.
#      - If no thread is running, don't sync.
#
#   $slave_timeout
#     Set the timeout when waiting for threads to sync. See
#     include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc
#
#
# ==== Side effects ====
#
# Does not change the current connection (note that this is different
# from mysqltest's built-in sync_slave_with_master command).


--let $include_filename= rpl_sync.inc
--source include/begin_include_file.inc


# Compute $rpl_sync_chain if needed.  We could have done this in
# rpl_change_topology.inc, but instead we do it here because that
# means we only compute $rpl_sync_chain when it is needed.
if ($rpl_sync_chain_dirty)
{
  --source include/rpl_generate_sync_chain.inc
  --let $rpl_sync_chain_dirty= 0
}


if ($rpl_debug)
{
  --echo \$rpl_sync_chain = '$rpl_sync_chain' \$rpl_only_running_threads= $rpl_only_running_threads
}

if (!$rpl_server_count_length)
{
  --die \$rpl_server_count_length is not set. Did you call rpl_init.inc?
}


--let $_rpl_i= 1
--let $_rpl_connect= 0
while ($_rpl_i) {
  # $rpl_sync_chain consists of a sequence of sync chains. Each sync
  # chain has the form:
  #
  #   <space><server1_1><server1_2>...<server1_N>
  #
  # So the space character indicates that a new sync chain starts.
  --let $_rpl_server= `SELECT TRIM(SUBSTR('$rpl_sync_chain', 1 + ($_rpl_i - 1) * $rpl_server_count_length, $rpl_server_count_length))`

  if ($_rpl_server)
  {
    if ($rpl_debug)
    {
      --echo [sync server_$_rpl_prev_server -> server_$_rpl_server]
    }
    if ($rpl_only_running_threads)
    {
      disable_connect_log;
      --connection server_$_rpl_server
      enable_connect_log;
      --let $_rpl_slave_io_running= query_get_value(SHOW SLAVE STATUS, Slave_IO_Running, 1)
      --let $_rpl_slave_sql_running= query_get_value(SHOW SLAVE STATUS, Slave_SQL_Running, 1)
      if ($rpl_debug)
      {
        --echo Sync IO: $_rpl_slave_io_running; Sync SQL: $_rpl_slave_sql_running
      }
      --let $_rpl_slave_io_running= `SELECT IF('$_rpl_slave_io_running' != 'No', 1, '')`
      --let $_rpl_slave_sql_running= `SELECT IF('$_rpl_slave_sql_running' = 'Yes', 1, '')`
      if ($_rpl_slave_io_running)
      {
        disable_query_log;
        --connection server_$_rpl_prev_server
        enable_query_log;
        if ($_rpl_slave_sql_running)
        {
          if ($rpl_debug)
          {
            --let $_rpl_master_file= query_get_value("SHOW MASTER STATUS", File, 1)
            --let $_rpl_master_pos= query_get_value("SHOW MASTER STATUS", Position, 1)
            --echo syncing master_file='$_rpl_master_file' master_pos='$_rpl_master_pos'
          }
          disable_connect_log;
          disable_query_log;
          --sync_slave_with_master server_$_rpl_server server_$_rpl_prev_server
          enable_connect_log;
          enable_query_log;
        }
        if (!$_rpl_slave_sql_running)
        {
          --let $sync_slave_connection= server_$_rpl_server
          --source include/sync_slave_io_with_master.inc
        }
      }
      if (!$_rpl_slave_io_running)
      {
        if ($_rpl_slave_sql_running)
        {
          --source include/sync_slave_sql_with_io.inc
        }
      }
    }
    if (!$rpl_only_running_threads)
    {
      disable_connect_log;
      --connection server_$_rpl_prev_server
      if ($rpl_debug)
      {
        --let $_rpl_master_file= query_get_value("SHOW MASTER STATUS", File, 1)
        --let $_rpl_master_pos= query_get_value("SHOW MASTER STATUS", Position, 1)
        --echo syncing master_file='$_rpl_master_file' master_pos='$_rpl_master_pos'
      }
      disable_query_log;
      --sync_slave_with_master server_$_rpl_server server_$_rpl_prev_server
      enable_query_log;
      enable_connect_log;
    }
  }

  # This happens at the beginning of a new sync subchain and at the
  # end of the full sync chain.
  if (!$_rpl_server)
  {
    --inc $_rpl_i
    --let $_rpl_server= `SELECT TRIM(SUBSTR('$rpl_sync_chain', 1 + ($_rpl_i - 1) * $rpl_server_count_length, $rpl_server_count_length))`

    if (!$_rpl_server)
    {
      # terminate loop
      --let $_rpl_i= -1
    }
  }

  --let $_rpl_prev_server= $_rpl_server
  --inc $_rpl_i
}


--let $include_filename= rpl_sync.inc
--source include/end_include_file.inc
