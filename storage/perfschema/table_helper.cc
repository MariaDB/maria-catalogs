/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335  USA */

/**
  @file storage/perfschema/table_helper.cc
  Performance schema table helpers (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_engine_table.h"
#include "table_helper.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"
#include "pfs_instr.h"
#include "pfs_program.h"
#include "field.h"
#include "pfs_variable.h"

int PFS_host_row::make_row(PFS_host *pfs)
{
  m_hostname_length= pfs->m_hostname_length;
  if (m_hostname_length > sizeof(m_hostname))
    return 1;
  if (m_hostname_length > 0)
    memcpy(m_hostname, pfs->m_hostname, sizeof(m_hostname));
  return 0;
}

void PFS_host_row::set_field(Field *f)
{
  if (m_hostname_length > 0)
    PFS_engine_table::set_field_char_utf8(f, m_hostname, m_hostname_length);
  else
    f->set_null();
}

int PFS_user_row::make_row(PFS_user *pfs)
{
  m_username_length= pfs->m_username_length;
  if (m_username_length > sizeof(m_username))
    return 1;
  if (m_username_length > 0)
    memcpy(m_username, pfs->m_username, sizeof(m_username));
  return 0;
}

void PFS_user_row::set_field(Field *f)
{
  if (m_username_length > 0)
    PFS_engine_table::set_field_char_utf8(f, m_username, m_username_length);
  else
    f->set_null();
}

int PFS_account_row::make_row(PFS_account *pfs)
{
  m_username_length= pfs->m_username_length;
  if (m_username_length > sizeof(m_username))
    return 1;
  if (m_username_length > 0)
    memcpy(m_username, pfs->m_username, sizeof(m_username));

  m_hostname_length= pfs->m_hostname_length;
  if (m_hostname_length > sizeof(m_hostname))
    return 1;
  if (m_hostname_length > 0)
    memcpy(m_hostname, pfs->m_hostname, sizeof(m_hostname));

  return 0;
}

void PFS_account_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* USER */
      if (m_username_length > 0)
        PFS_engine_table::set_field_char_utf8(f, m_username, m_username_length);
      else
        f->set_null();
      break;
    case 1: /* HOST */
      if (m_hostname_length > 0)
        PFS_engine_table::set_field_char_utf8(f, m_hostname, m_hostname_length);
      else
        f->set_null();
      break;
    default:
      assert(false);
      break;
  }
}

int PFS_digest_row::make_row(PFS_statements_digest_stat* pfs)
{
  m_schema_name_length= pfs->m_digest_key.m_schema_name_length;
  if (m_schema_name_length > sizeof(m_schema_name))
    m_schema_name_length= 0;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, pfs->m_digest_key.m_schema_name, m_schema_name_length);

  size_t safe_byte_count= pfs->m_digest_storage.m_byte_count;
  if (safe_byte_count > pfs_max_digest_length)
    safe_byte_count= 0;

  /*
    "0" value for byte_count indicates special entry i.e. aggregated
    stats at index 0 of statements_digest_stat_array. So do not calculate
    digest/digest_text as it should always be "NULL".
  */
  if (safe_byte_count > 0)
  {
    /*
      Calculate digest from MD5 HASH collected to be shown as
      DIGEST in this row.
    */
    MD5_HASH_TO_STRING(pfs->m_digest_storage.m_md5, m_digest);
    m_digest_length= MD5_HASH_TO_STRING_LENGTH;

    /*
      Calculate digest_text information from the token array collected
      to be shown as DIGEST_TEXT column.
    */
    compute_digest_text(&pfs->m_digest_storage, &m_digest_text);

    if (m_digest_text.length() == 0)
      m_digest_length= 0;
  }
  else
  {
    m_digest_length= 0;
  }

  return 0;
}

void PFS_digest_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* SCHEMA_NAME */
      if (m_schema_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_schema_name,
                                                 m_schema_name_length);
      else
        f->set_null();
      break;
    case 1: /* DIGEST */
      if (m_digest_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_digest,
                                                 m_digest_length);
      else
        f->set_null();
      break;
    case 2: /* DIGEST_TEXT */
      if (m_digest_text.length() > 0)
        PFS_engine_table::set_field_longtext_utf8(f, m_digest_text.ptr(),
                                                  m_digest_text.length());
      else
        f->set_null();
      break;
    default:
      assert(false);
      break;
  }
}

int PFS_object_row::make_row(PFS_table_share *pfs)
{
  m_object_type= pfs->get_object_type();

  m_schema_name_length= pfs->m_schema_name_length;
  if (m_schema_name_length > sizeof(m_schema_name))
    return 1;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, pfs->m_schema_name, sizeof(m_schema_name));

  m_object_name_length= pfs->m_table_name_length;
  if (m_object_name_length > sizeof(m_object_name))
    return 1;
  if (m_object_name_length > 0)
    memcpy(m_object_name, pfs->m_table_name, sizeof(m_object_name));

  return 0;
}

int PFS_object_row::make_row(PFS_program *pfs)
{
  m_object_type= pfs->m_type;

  m_schema_name_length= pfs->m_schema_name_length;
  if (m_schema_name_length > sizeof(m_schema_name))
    return 1;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, pfs->m_schema_name, sizeof(m_schema_name));

  m_object_name_length= pfs->m_object_name_length;
  if (m_object_name_length > sizeof(m_object_name))
    return 1;
  if (m_object_name_length > 0)
    memcpy(m_object_name, pfs->m_object_name, sizeof(m_object_name));

  return 0;
}

int PFS_object_row::make_row(const MDL_key *mdl)
{
  MDL_key user_lock_workaround;
  switch(mdl->mdl_namespace())
  {
  case MDL_key::BACKUP:
    m_object_type= OBJECT_TYPE_BACKUP;
    m_schema_name_length= 0;
    m_object_name_length= 0;
    break;
  case MDL_key::SCHEMA:
    m_object_type= OBJECT_TYPE_SCHEMA;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= 0;
    break;
  case MDL_key::TABLE:
    m_object_type= OBJECT_TYPE_TABLE;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::FUNCTION:
    m_object_type= OBJECT_TYPE_FUNCTION;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::PROCEDURE:
    m_object_type= OBJECT_TYPE_PROCEDURE;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::PACKAGE_BODY:
    m_object_type= OBJECT_TYPE_PACKAGE_BODY;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::TRIGGER:
    m_object_type= OBJECT_TYPE_TRIGGER;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::EVENT:
    m_object_type= OBJECT_TYPE_EVENT;
    m_schema_name_length= mdl->db_name_length();
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::USER_LOCK:
    m_object_type= OBJECT_TYPE_USER_LEVEL_LOCK;
    user_lock_workaround.mdl_key_init(MDL_key::USER_LOCK, mdl->catalog(),
                                      "", mdl->db_name());
    mdl=& user_lock_workaround;
    m_schema_name_length= 0;
    m_object_name_length= mdl->name_length();
    break;
  case MDL_key::NAMESPACE_END:
  default:
    m_object_type= NO_OBJECT_TYPE;
    m_schema_name_length= 0;
    m_object_name_length= 0;
    break;
  }

  if (m_schema_name_length > sizeof(m_schema_name))
    return 1;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, mdl->db_name(), m_schema_name_length);

  if (m_object_name_length > sizeof(m_object_name))
    return 1;
  if (m_object_name_length > 0)
    memcpy(m_object_name, mdl->name(), m_object_name_length);

  return 0;
}

void PFS_object_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
      set_field_object_type(f, m_object_type);
      break;
    case 1: /* SCHEMA_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_schema_name, m_schema_name_length);
      break;
    case 2: /* OBJECT_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_object_name, m_object_name_length);
      break;
    default:
      assert(false);
  }
}

void PFS_object_row::set_nullable_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
      if (m_object_type != NO_OBJECT_TYPE)
        set_field_object_type(f, m_object_type);
      else
        f->set_null();
      break;
    case 1: /* SCHEMA_NAME */
      if (m_schema_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_schema_name, m_schema_name_length);
      else
        f->set_null();
      break;
    case 2: /* OBJECT_NAME */
      if (m_object_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_object_name, m_object_name_length);
      else
        f->set_null();
      break;
    default:
      assert(false);
  }
}

int PFS_index_row::make_row(PFS_table_share *pfs,
                            PFS_table_share_index *pfs_index,
                            uint table_index)
{
  if (m_object_row.make_row(pfs))
    return 1;

  if (pfs_index == NULL)
  {
    if (table_index < MAX_INDEXES)
    {
      m_index_name_length= sprintf(m_index_name, "(index %d)", table_index);
    }
    else
    {
      m_index_name_length= 0;
    }
    return 0;
  }

  if (table_index < MAX_INDEXES)
  {
    m_index_name_length= pfs_index->m_key.m_name_length;
    if (m_index_name_length > sizeof(m_index_name))
      return 1;

    memcpy(m_index_name, pfs_index->m_key.m_name, sizeof(m_index_name));
  }
  else
  {
    m_index_name_length= 0;
  }

  return 0;
}

void PFS_index_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_index_name, m_index_name_length);
      else
        f->set_null();
      break;
    default:
      assert(false);
  }
}

void PFS_statement_stat_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* COUNT_STAR */
    case 1: /* SUM_TIMER_WAIT */
    case 2: /* MIN_TIMER_WAIT */
    case 3: /* AVG_TIMER_WAIT */
    case 4: /* MAX_TIMER_WAIT */
      m_timer1_row.set_field(index, f);
      break;
    case 5: /* SUM_LOCK_TIME */
      PFS_engine_table::set_field_ulonglong(f, m_lock_time);
      break;
    case 6: /* SUM_ERRORS */
      PFS_engine_table::set_field_ulonglong(f, m_error_count);
      break;
    case 7: /* SUM_WARNINGS */
      PFS_engine_table::set_field_ulonglong(f, m_warning_count);
      break;
    case 8: /* SUM_ROWS_AFFECTED */
      PFS_engine_table::set_field_ulonglong(f, m_rows_affected);
      break;
    case 9: /* SUM_ROWS_SENT */
      PFS_engine_table::set_field_ulonglong(f, m_rows_sent);
      break;
    case 10: /* SUM_ROWS_EXAMINED */
      PFS_engine_table::set_field_ulonglong(f, m_rows_examined);
      break;
    case 11: /* SUM_CREATED_TMP_DISK_TABLES */
      PFS_engine_table::set_field_ulonglong(f, m_created_tmp_disk_tables);
      break;
    case 12: /* SUM_CREATED_TMP_TABLES */
      PFS_engine_table::set_field_ulonglong(f, m_created_tmp_tables);
      break;
    case 13: /* SUM_SELECT_FULL_JOIN */
      PFS_engine_table::set_field_ulonglong(f, m_select_full_join);
      break;
    case 14: /* SUM_SELECT_FULL_RANGE_JOIN */
      PFS_engine_table::set_field_ulonglong(f, m_select_full_range_join);
      break;
    case 15: /* SUM_SELECT_RANGE */
      PFS_engine_table::set_field_ulonglong(f, m_select_range);
      break;
    case 16: /* SUM_SELECT_RANGE_CHECK */
      PFS_engine_table::set_field_ulonglong(f, m_select_range_check);
      break;
    case 17: /* SUM_SELECT_SCAN */
      PFS_engine_table::set_field_ulonglong(f, m_select_scan);
      break;
    case 18: /* SUM_SORT_MERGE_PASSES */
      PFS_engine_table::set_field_ulonglong(f, m_sort_merge_passes);
      break;
    case 19: /* SUM_SORT_RANGE */
      PFS_engine_table::set_field_ulonglong(f, m_sort_range);
      break;
    case 20: /* SUM_SORT_ROWS */
      PFS_engine_table::set_field_ulonglong(f, m_sort_rows);
      break;
    case 21: /* SUM_SORT_SCAN */
      PFS_engine_table::set_field_ulonglong(f, m_sort_scan);
      break;
    case 22: /* SUM_NO_INDEX_USED */
      PFS_engine_table::set_field_ulonglong(f, m_no_index_used);
      break;
    case 23: /* SUM_NO_GOOD_INDEX_USED */
      PFS_engine_table::set_field_ulonglong(f, m_no_good_index_used);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_transaction_stat_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* COUNT_STAR */
    case 1: /* SUM_TIMER_WAIT */
    case 2: /* MIN_TIMER_WAIT */
    case 3: /* AVG_TIMER_WAIT */
    case 4: /* MAX_TIMER_WAIT */
      m_timer1_row.set_field(index, f);
      break;
    case 5: /* COUNT_READ_WRITE */
    case 6: /* SUM_TIMER_READ_WRITE */
    case 7: /* MIN_TIMER_READ_WRITE */
    case 8: /* AVG_TIMER_READ_WRITE */
    case 9: /* MAX_TIMER_READ_WRITE */
      m_read_write_row.set_field(index-5, f);
      break;
    case 10: /* COUNT_READ_ONLY */
    case 11: /* SUM_TIMER_READ_ONLY */
    case 12: /* MIN_TIMER_READ_ONLY */
    case 13: /* AVG_TIMER_READ_ONLY */
    case 14: /* MAX_TIMER_READ_ONLY */
      m_read_only_row.set_field(index-10, f);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_connection_stat_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* CURRENT_CONNECTIONS */
      PFS_engine_table::set_field_ulonglong(f, m_current_connections);
      break;
    case 1: /* TOTAL_CONNECTIONS */
      PFS_engine_table::set_field_ulonglong(f, m_total_connections);
      break;
    default:
      assert(false);
      break;
  }
}

void set_field_object_type(Field *f, enum_object_type object_type)
{
  switch (object_type)
  {
  case OBJECT_TYPE_EVENT:
    PFS_engine_table::set_field_varchar_utf8(f, "EVENT", 5);
    break;
  case OBJECT_TYPE_FUNCTION:
    PFS_engine_table::set_field_varchar_utf8(f, "FUNCTION", 8);
    break;
  case OBJECT_TYPE_PROCEDURE:
    PFS_engine_table::set_field_varchar_utf8(f, "PROCEDURE", 9);
    break;
  case OBJECT_TYPE_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TABLE", 5);
    break;
  case OBJECT_TYPE_TEMPORARY_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TEMPORARY TABLE", 15);
    break;
  case OBJECT_TYPE_TRIGGER:
    PFS_engine_table::set_field_varchar_utf8(f, "TRIGGER", 7);
    break;
  case OBJECT_TYPE_BACKUP:
    PFS_engine_table::set_field_varchar_utf8(f, "BACKUP", 6);
    break;
  case OBJECT_TYPE_SCHEMA:
    PFS_engine_table::set_field_varchar_utf8(f, "SCHEMA", 6);
    break;
  case OBJECT_TYPE_PACKAGE_BODY:
    PFS_engine_table::set_field_varchar_utf8(f, "PACKAGE BODY", 12);
    break;
  case OBJECT_TYPE_USER_LEVEL_LOCK:
    PFS_engine_table::set_field_varchar_utf8(f, "USER LEVEL LOCK", 15);
    break;
  case NO_OBJECT_TYPE:
  default:
    assert(false);
    PFS_engine_table::set_field_varchar_utf8(f, "", 0);
    break;
  }
}

void set_field_lock_type(Field *f, PFS_TL_LOCK_TYPE lock_type)
{
  switch (lock_type)
  {
  case PFS_TL_READ:
    PFS_engine_table::set_field_varchar_utf8(f, "READ", 4);
    break;
  case PFS_TL_READ_WITH_SHARED_LOCKS:
    PFS_engine_table::set_field_varchar_utf8(f, "READ WITH SHARED LOCKS", 22);
    break;
  case PFS_TL_READ_HIGH_PRIORITY:
    PFS_engine_table::set_field_varchar_utf8(f, "READ HIGH PRIORITY", 18);
    break;
  case PFS_TL_READ_NO_INSERT:
    PFS_engine_table::set_field_varchar_utf8(f, "READ NO INSERT", 14);
    break;
  case PFS_TL_WRITE_ALLOW_WRITE:
    PFS_engine_table::set_field_varchar_utf8(f, "WRITE ALLOW WRITE", 17);
    break;
  case PFS_TL_WRITE_CONCURRENT_INSERT:
    PFS_engine_table::set_field_varchar_utf8(f, "WRITE CONCURRENT INSERT", 23);
    break;
  case PFS_TL_WRITE_LOW_PRIORITY:
    PFS_engine_table::set_field_varchar_utf8(f, "WRITE LOW PRIORITY", 18);
    break;
  case PFS_TL_WRITE:
    PFS_engine_table::set_field_varchar_utf8(f, "WRITE", 5);
    break;
  case PFS_TL_READ_EXTERNAL:
    PFS_engine_table::set_field_varchar_utf8(f, "READ EXTERNAL", 13);
    break;
  case PFS_TL_WRITE_EXTERNAL:
    PFS_engine_table::set_field_varchar_utf8(f, "WRITE EXTERNAL", 14);
    break;
  case PFS_TL_NONE:
    f->set_null();
    break;
  default:
    assert(false);
  }
}

void set_field_mdl_type(Field *f, opaque_mdl_type mdl_type, bool backup)
{
  if (backup)
  {
    switch (mdl_type)
    {
    case MDL_BACKUP_START:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_START"));
      break;
    case MDL_BACKUP_FLUSH:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_FLUSH"));
      break;
    case MDL_BACKUP_WAIT_FLUSH:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_WAIT_FLUSH"));
      break;
    case MDL_BACKUP_WAIT_DDL:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_WAIT_DDL"));
      break;
    case MDL_BACKUP_WAIT_COMMIT:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_WAIT_COMMIT"));
      break;
    case MDL_BACKUP_FTWRL1:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_FTWRL1"));
      break;
    case MDL_BACKUP_FTWRL2:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_FTWRL2"));
      break;
    case MDL_BACKUP_DML:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_DML"));
      break;
    case MDL_BACKUP_TRANS_DML:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_TRANS_DML"));
      break;
    case MDL_BACKUP_SYS_DML:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_SYS_DML"));
      break;
    case MDL_BACKUP_DDL:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_DDL"));
      break;
    case MDL_BACKUP_BLOCK_DDL:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_BLOCK_DDL"));
      break;
    case MDL_BACKUP_ALTER_COPY:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_ALTER_COPY"));
      break;
    case MDL_BACKUP_COMMIT:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_COMMIT"));
      break;
    case MDL_BACKUP_END:
      PFS_engine_table::set_field_varchar_utf8(f, STRING_WITH_LEN("BACKUP_END"));
      break;
    default:
      DBUG_ASSERT(false);
    }
  }
  else
  {
    enum_mdl_type e= (enum_mdl_type) mdl_type;
    switch (e)
    {
    case MDL_INTENTION_EXCLUSIVE:
      PFS_engine_table::set_field_varchar_utf8(f, "INTENTION_EXCLUSIVE", 19);
      break;
    case MDL_SHARED:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED", 6);
      break;
    case MDL_SHARED_HIGH_PRIO:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_HIGH_PRIO", 16);
      break;
    case MDL_SHARED_READ:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_READ", 11);
      break;
    case MDL_SHARED_WRITE:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_WRITE", 12);
      break;
    case MDL_SHARED_UPGRADABLE:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_UPGRADABLE", 17);
      break;
    case MDL_SHARED_NO_WRITE:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_NO_WRITE", 15);
      break;
    case MDL_SHARED_NO_READ_WRITE:
      PFS_engine_table::set_field_varchar_utf8(f, "SHARED_NO_READ_WRITE", 20);
      break;
    case MDL_EXCLUSIVE:
      PFS_engine_table::set_field_varchar_utf8(f, "EXCLUSIVE", 9);
      break;
    default:
      DBUG_ASSERT(false);
    }
  }
}

void set_field_mdl_duration(Field *f, opaque_mdl_duration mdl_duration)
{
  enum_mdl_duration e= (enum_mdl_duration) mdl_duration;
  switch (e)
  {
  case MDL_STATEMENT:
    PFS_engine_table::set_field_varchar_utf8(f, "STATEMENT", 9);
    break;
  case MDL_TRANSACTION:
    PFS_engine_table::set_field_varchar_utf8(f, "TRANSACTION", 11);
    break;
  case MDL_EXPLICIT:
    PFS_engine_table::set_field_varchar_utf8(f, "EXPLICIT", 8);
    break;
  case MDL_DURATION_END:
  default:
    assert(false);
  }
}

void set_field_mdl_status(Field *f, opaque_mdl_status mdl_status)
{
  MDL_ticket::enum_psi_status e= static_cast<MDL_ticket::enum_psi_status>(mdl_status);
  switch (e)
  {
  case MDL_ticket::PENDING:
    PFS_engine_table::set_field_varchar_utf8(f, "PENDING", 7);
    break;
  case MDL_ticket::GRANTED:
    PFS_engine_table::set_field_varchar_utf8(f, "GRANTED", 7);
    break;
  case MDL_ticket::PRE_ACQUIRE_NOTIFY:
    PFS_engine_table::set_field_varchar_utf8(f, "PRE_ACQUIRE_NOTIFY", 18);
    break;
  case MDL_ticket::POST_RELEASE_NOTIFY:
    PFS_engine_table::set_field_varchar_utf8(f, "POST_RELEASE_NOTIFY", 19);
    break;
  default:
    assert(false);
  }
}

void PFS_memory_stat_row::set_field(uint index, Field *f)
{
  ssize_t val;

  switch (index)
  {
    case 0: /* COUNT_ALLOC */
      PFS_engine_table::set_field_ulonglong(f, m_stat.m_alloc_count);
      break;
    case 1: /* COUNT_FREE */
      PFS_engine_table::set_field_ulonglong(f, m_stat.m_free_count);
      break;
    case 2: /* SUM_NUMBER_OF_BYTES_ALLOC */
      PFS_engine_table::set_field_ulonglong(f, m_stat.m_alloc_size);
      break;
    case 3: /* SUM_NUMBER_OF_BYTES_FREE */
      PFS_engine_table::set_field_ulonglong(f, m_stat.m_free_size);
      break;
    case 4: /* LOW_COUNT_USED */
      val= m_stat.m_alloc_count - m_stat.m_free_count - m_stat.m_free_count_capacity;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    case 5: /* CURRENT_COUNT_USED */
      val= m_stat.m_alloc_count - m_stat.m_free_count;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    case 6: /* HIGH_COUNT_USED */
      val= m_stat.m_alloc_count - m_stat.m_free_count + m_stat.m_alloc_count_capacity;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    case 7: /* LOW_NUMBER_OF_BYTES_USED */
      val= m_stat.m_alloc_size - m_stat.m_free_size - m_stat.m_free_size_capacity;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    case 8: /* CURRENT_NUMBER_OF_BYTES_USED */
      val= m_stat.m_alloc_size - m_stat.m_free_size;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    case 9: /* HIGH_NUMBER_OF_BYTES_USED */
      val= m_stat.m_alloc_size - m_stat.m_free_size + m_stat.m_alloc_size_capacity;
      PFS_engine_table::set_field_longlong(f, val);
      break;
    default:
      assert(false);
      break;
  }
}

void set_field_isolation_level(Field *f, enum_isolation_level iso_level)
{
  switch (iso_level)
  {
  case TRANS_LEVEL_READ_UNCOMMITTED:
    PFS_engine_table::set_field_varchar_utf8(f, "READ UNCOMMITTED", 16);
    break;
  case TRANS_LEVEL_READ_COMMITTED:
    PFS_engine_table::set_field_varchar_utf8(f, "READ COMMITTED", 14);
    break;
  case TRANS_LEVEL_REPEATABLE_READ:
    PFS_engine_table::set_field_varchar_utf8(f, "REPEATABLE READ", 15);
    break;
  case TRANS_LEVEL_SERIALIZABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "SERIALIZABLE", 12);
    break;
  default:
    assert(false);
  }
}

void set_field_xa_state(Field *f, enum_xa_transaction_state xa_state)
{
  switch (xa_state)
  {
  case TRANS_STATE_XA_NOTR:
    PFS_engine_table::set_field_varchar_utf8(f, "NOTR", 4);
    break;
  case TRANS_STATE_XA_ACTIVE:
    PFS_engine_table::set_field_varchar_utf8(f, "ACTIVE", 6);
    break;
  case TRANS_STATE_XA_IDLE:
    PFS_engine_table::set_field_varchar_utf8(f, "IDLE", 4);
    break;
  case TRANS_STATE_XA_PREPARED:
    PFS_engine_table::set_field_varchar_utf8(f, "PREPARED", 8);
    break;
  case TRANS_STATE_XA_ROLLBACK_ONLY:
    PFS_engine_table::set_field_varchar_utf8(f, "ROLLBACK ONLY", 13);
    break;
  case TRANS_STATE_XA_COMMITTED:
    PFS_engine_table::set_field_varchar_utf8(f, "COMMITTED", 9);
    break;
  default:
    assert(false);
  }
}

void PFS_variable_name_row::make_row(const char* str, size_t length)
{
  assert(length <= sizeof(m_str));
  assert(length <= NAME_CHAR_LEN);

  m_length= MY_MIN(static_cast<uint>(length), NAME_CHAR_LEN); /* enforce max name length */
  if (m_length > 0)
    memcpy(m_str, str, length);
  m_str[m_length]= '\0';
}

void PFS_variable_value_row::make_row(const Status_variable *var)
{
  make_row(var->m_charset, var->m_value_str, var->m_value_length);
}

void PFS_variable_value_row::make_row(const System_variable *var)
{
  make_row(var->m_charset, var->m_value_str, var->m_value_length);
}

void PFS_variable_value_row::make_row(const CHARSET_INFO *cs, const char* str, size_t length)
{
  assert(cs != NULL);
  assert(length <= sizeof(m_str));
  if (length > 0)
  {
    memcpy(m_str, str, length);
  }
  m_length= static_cast<uint>(length);
  m_charset= cs;
}

void PFS_variable_value_row::set_field(Field *f)
{
  PFS_engine_table::set_field_varchar(f, m_charset, m_str, m_length);
}

void PFS_user_variable_value_row::clear()
{
  my_free(m_value);
  m_value= NULL;
  m_value_length= 0;
}

void PFS_user_variable_value_row::make_row(const char* val, size_t length)
{
  if (length > 0)
  {
    m_value= (char*) my_malloc(PSI_NOT_INSTRUMENTED, length, MYF(0));
    m_value_length= length;
    memcpy(m_value, val, length);
  }
  else
  {
    m_value= NULL;
    m_value_length= 0;
  }
}

