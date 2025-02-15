# Get the catalog or base directory for replication

# Sets the following variables
# $SLAVE_DATADIR    The slave datadir directory
# $SLAVE_TOPDIR     The slave topdir (select @@datadir)

# Note that the connection is set to master at end of script

--disable_connect_log
# Remember dirs set by mtr
--let $MASTER_TOPDIR=$MARIADB_TOPDIR
--let $MASTER_DATADIR=$MARIADB_DATADIR
--connection slave
--source include/get_datadir.inc
--let $SLAVE_TOPDIR=$MARIADB_TOPDIR
--let $SLAVE_DATADIR=$MARIADB_DATADIR
--let $MARIADB_TOPDIR=$MASTER_TOPDIR
--let $MARIADB_DATADIR=$MASTER_DATADIR
--connection master
--enable_connect_log
