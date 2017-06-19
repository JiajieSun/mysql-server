/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "sql/dd/upgrade/routine.h"
#include "sql/dd/upgrade/global.h"
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "log.h"                              // LogErr()
#include "my_user.h"                          // parse_user
#include "sql_base.h"                         // open_tables
#include "sp.h"                               // db_load_routine
#include "sp_head.h"                          // sp_head
#include "table.h"                            // Table_check_intact
#include "transaction.h"                      // trans_commit

namespace dd {
namespace upgrade {

static Check_table_intact table_intact;

/**
  Column definitions for 5.7 mysql.proc table (5.7.13 and up).
*/
static const
TABLE_FIELD_TYPE proc_table_fields[MYSQL_PROC_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("specific_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("language") },
    { C_STRING_WITH_LEN("enum('SQL')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_data_access") },
    { C_STRING_WITH_LEN("enum('CONTAINS_SQL','NO_SQL','READS_SQL_DATA','MODIFIES_SQL_DATA')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("is_deterministic") },
    { C_STRING_WITH_LEN("enum('YES','NO')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("security_type") },
    { C_STRING_WITH_LEN("enum('INVOKER','DEFINER')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("param_list") },
    { C_STRING_WITH_LEN("blob") },
    { NULL, 0 }
  },

  {
    { C_STRING_WITH_LEN("returns") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(93)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
        { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  proc_table_def= {MYSQL_PROC_FIELD_COUNT, proc_table_fields};


/**
  Column definitions for 5.7 mysql.proc table (before 5.7.13).
*/

static const
TABLE_FIELD_TYPE proc_table_fields_old[MYSQL_PROC_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("specific_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("language") },
    { C_STRING_WITH_LEN("enum('SQL')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_data_access") },
    { C_STRING_WITH_LEN("enum('CONTAINS_SQL','NO_SQL','READS_SQL_DATA','MODIFIES_SQL_DATA')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("is_deterministic") },
    { C_STRING_WITH_LEN("enum('YES','NO')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("security_type") },
    { C_STRING_WITH_LEN("enum('INVOKER','DEFINER')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("param_list") },
    { C_STRING_WITH_LEN("blob") },
    { NULL, 0 }
  },

  {
    { C_STRING_WITH_LEN("returns") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(77)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  proc_table_def_old= {MYSQL_PROC_FIELD_COUNT, proc_table_fields_old};



/**
  Set st_sp_chistics for routines.
*/

static bool set_st_sp_chistics(THD *thd, TABLE *proc_table,
                               st_sp_chistics *chistics)
{
  char *ptr;
  size_t length;
  char buff[65];
  String str(buff, sizeof(buff), &my_charset_bin);

  memset(chistics, 0, sizeof(st_sp_chistics));

  if ((ptr= get_field(thd->mem_root,
                      proc_table->field[MYSQL_PROC_FIELD_ACCESS])) == NULL)
    return true;

  switch (ptr[0]) {
  case 'N':
    chistics->daccess= SP_NO_SQL;
    break;
  case 'C':
    chistics->daccess= SP_CONTAINS_SQL;
    break;
  case 'R':
    chistics->daccess= SP_READS_SQL_DATA;
    break;
  case 'M':
    chistics->daccess= SP_MODIFIES_SQL_DATA;
    break;
  default:
    chistics->daccess= SP_DEFAULT_ACCESS_MAPPING;
  }

  // Deterministic
  if ((ptr=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_DETERMINISTIC])) == NULL)
    return true;

  chistics->detistic= (ptr[0] == 'N' ? FALSE : TRUE);

  // Security type
  if ((ptr=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_SECURITY_TYPE])) == NULL)
    return true;

  chistics->suid= (ptr[0] == 'I' ? SP_IS_NOT_SUID : SP_IS_SUID);

  // Fetch SP/SF comment
  proc_table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= strmake_root(thd->mem_root, str.ptr(), length);
  chistics->comment.str= ptr;
  chistics->comment.length= length;

  return false;
}


/**
  This function migrate one SP/SF from mysql.proc to routines DD table.
  One record in mysql.proc is metadata for one SP/SF. This function
  parses one record to extract metadata required and store it in DD table.
*/

static bool migrate_routine_to_dd(THD *thd, TABLE *proc_table)
{
  const char *params, *returns, *body, *definer;
  char *sp_db, *sp_name1;
  sp_head *sp= nullptr;
  enum_sp_type routine_type;
  LEX_USER user_info;

  // Fetch SP/SF name, datbase name, definer and type.
  if ((sp_db= get_field(thd->mem_root,
                        proc_table->field[MYSQL_PROC_FIELD_DB])) == NULL)
    return true;

  if ((sp_name1= get_field(thd->mem_root,
                           proc_table->field[MYSQL_PROC_FIELD_NAME])) == NULL)
    return true;

  if ((definer= get_field(thd->mem_root,
                          proc_table->field[MYSQL_PROC_FIELD_DEFINER])) == NULL)
    return true;

  routine_type=
      (enum_sp_type) proc_table->field[MYSQL_PROC_MYSQL_TYPE]->val_int();

  // Fetch SP/SF parameters string
  if ((params=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_PARAM_LIST])) == NULL)
    params= "";

  // Create return type string for SF
  if (routine_type == enum_sp_type::PROCEDURE)
    returns= "";
  else if ((returns=
            get_field(thd->mem_root,
                      proc_table->field[MYSQL_PROC_FIELD_RETURNS])) == NULL)
    return true;

  st_sp_chistics chistics;
  if (set_st_sp_chistics(thd, proc_table, &chistics))
    return true;

  // Fetch SP/SF created and modified timestamp
  longlong created= proc_table->field[MYSQL_PROC_FIELD_CREATED]->val_int();
  longlong modified= proc_table->field[MYSQL_PROC_FIELD_MODIFIED]->val_int();

  // Fetch SP/SF body
  if ((body= get_field(thd->mem_root,
                       proc_table->field[MYSQL_PROC_FIELD_BODY])) == NULL)
    return true;

  Routine_event_context_guard routine_ctx_guard(thd);

  thd->variables.sql_mode=
    (sql_mode_t) proc_table->field[MYSQL_PROC_FIELD_SQL_MODE]->val_int();

  LEX_CSTRING sp_db_str;
  LEX_STRING sp_name_str;

  sp_db_str.str= sp_db;
  sp_db_str.length= strlen(sp_db);
  sp_name_str.str= sp_name1;
  sp_name_str.length= strlen(sp_name1);

  sp_name sp_name_obj= sp_name(sp_db_str, sp_name_str, true);
  sp_name_obj.init_qname(thd);

  // Create SP creation context to be used in db_load_routine()
  Stored_program_creation_ctx *creation_ctx=
  Stored_routine_creation_ctx::load_from_db(thd, &sp_name_obj, proc_table);

  /*
    Update character set info in thread variable.
    Restore will be taken care by Routine_event_context_guard
  */
  thd->variables.character_set_client= creation_ctx->get_client_cs();
  thd->variables.collation_connection= creation_ctx->get_connection_cl();
  thd->update_charset();

  // Holders for user name and host name used in parse user.
  char definer_user_name_holder[USERNAME_LENGTH + 1];
  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  memset(&user_info, 0, sizeof(LEX_USER));
  user_info.user= { definer_user_name_holder, USERNAME_LENGTH };
  user_info.host= { definer_host_name_holder, HOSTNAME_LENGTH };

  // Parse user string to separate user name and host
  parse_user(definer, strlen(definer),
             definer_user_name_holder, &user_info.user.length,
             definer_host_name_holder, &user_info.host.length);

  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  // This function fixes sp_head to use in sp_create_routine()
  if (db_load_routine(thd, routine_type, sp_db_str.str, sp_db_str.length,
                      sp_name_str.str, sp_name_str.length, &sp,
                      thd->variables.sql_mode, params, returns, body, &chistics,
                      definer_user_name_holder, definer_host_name_holder,
                      created, modified, creation_ctx))
  {
    /*
      Parsing of routine body failed. Use empty routine body and report a
      warning if the routine does not belong to sys schema. Sys schema routines
      will get fixed when mysql_upgrade is executed.
    */
    if (strcmp(sp_db_str.str, "sys") != 0)
      LogErr(WARNING_LEVEL, ER_CANT_PARSE_STORED_ROUTINE_BODY,
             sp_db_str.str, sp_name_str.str);

    LEX_CSTRING sr_body;
    if (routine_type == enum_sp_type::FUNCTION)
      sr_body= { STRING_WITH_LEN("RETURN NULL") };
    else
      sr_body= { STRING_WITH_LEN("BEGIN END") };

    if (db_load_routine(thd, routine_type, sp_db_str.str, sp_db_str.length,
                        sp_name_str.str, sp_name_str.length, &sp,
                        thd->variables.sql_mode, params, returns, sr_body.str,
                        &chistics, definer_user_name_holder,
                        definer_host_name_holder, created,
                        modified, creation_ctx))
      goto err;

    // Set actual routine body.
    sp->m_body.str= const_cast<char*>(body);
    sp->m_body.length= strlen(body);
  }

  // Create entry for SP/SF in DD table.
  if (sp_create_routine(thd, sp, &user_info))
    goto err;

  if (sp != nullptr)            // To be safe
    sp_head::destroy(sp);

  return false;

err:
  LogErr(ERROR_LEVEL, ER_DD_CANT_CREATE_SP, sp_db_str.str, sp_name_str.str);
  if (sp != nullptr)            // To be safe
    sp_head::destroy(sp);
  return true;
}


/**
  Migrate Stored Procedure and Functions
  from mysql.proc to routines dd table.
*/

bool migrate_routines_to_dd(THD *thd)
{
  TABLE *proc_table;
  TABLE_LIST tables, *table_list;
  int error= 0;
  uint flags= MYSQL_LOCK_IGNORE_TIMEOUT;
  DML_prelocking_strategy prelocking_strategy;
  MEM_ROOT records_mem_root;

  tables.init_one_table("mysql", 5, "proc", 4, "proc", TL_READ);

  table_list= &tables;
  if (open_and_lock_tables(thd, table_list, flags, &prelocking_strategy))
  {
    close_thread_tables(thd);
    LogErr(ERROR_LEVEL, ER_CANT_OPEN_TABLE_MYSQL_PROC);
    return true;
  }

  proc_table= tables.table;
  proc_table->use_all_columns();

  if (table_intact.check(thd, proc_table, &proc_table_def))
  {
    // Check with old format too before returning error
    if (table_intact.check(thd, proc_table, &proc_table_def_old))
    {
      close_thread_tables(thd);
      return true;
    }
  }

  System_table_close_guard proc_table_guard(thd, proc_table);

  if (proc_table->file->ha_index_init(0, 1))
  {
    LogErr(ERROR_LEVEL, ER_CANT_READ_TABLE_MYSQL_PROC);
    return true;
  }

  // Read first record from mysql.proc table. Return if table is empty.
  if ((error= proc_table->file->ha_index_first(proc_table->record[0])))
  {
    if (error == HA_ERR_END_OF_FILE)
      return false;

    LogErr(ERROR_LEVEL, ER_CANT_READ_TABLE_MYSQL_PROC);
    return true;
  }

  init_sql_alloc(PSI_NOT_INSTRUMENTED, &records_mem_root,
                 MEM_ROOT_BLOCK_SIZE, 0);
  thd->mem_root= &records_mem_root;

  // Migrate first record read to dd routines table.
  if (migrate_routine_to_dd(thd, proc_table))
    goto err;

  // Read one record from mysql.proc table and
  // migrate it until all records are finished
  while (!(error= proc_table->file->ha_index_next(proc_table->record[0])))
  {
    if (migrate_routine_to_dd(thd, proc_table))
      goto err;
  }

  if (error != HA_ERR_END_OF_FILE)
  {
    LogErr(ERROR_LEVEL, ER_CANT_READ_TABLE_MYSQL_PROC);
    goto err;
  }

  free_root(&records_mem_root, MYF(0));
  return false;

err:
  free_root(&records_mem_root, MYF(0));
  return true;
}

} // namespace upgrade
} // namespace dd
