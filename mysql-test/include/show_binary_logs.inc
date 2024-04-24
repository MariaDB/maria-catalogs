# show binary logs

--let $include_filename= show_binary_logs.inc
--source include/begin_include_file.inc

if (!$use_current_connection)
{
--disable_connect_log
  if ($CURRENT_CONNECTION == "master")
  {
    --connection master_config
  }
  if ($CURRENT_CONNECTION == "slave")
  {
    --connection slave_config
  }
--enable_connect_log
}

# mask out the binlog position
-- replace_column 2 #
show binary logs;

--let $include_filename= show_binary_logs.inc
--source include/end_include_file.inc
