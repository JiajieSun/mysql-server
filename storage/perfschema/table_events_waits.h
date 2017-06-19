/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_WAITS_H
#define TABLE_EVENTS_WAITS_H

/**
  @file storage/perfschema/table_events_waits.h
  Table EVENTS_WAITS_xxx (declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_events_waits.h"
#include "table_helper.h"

struct PFS_thread;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of table_events_waits_common. */
struct row_events_waits
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column EVENT_ID. */
  ulonglong m_event_id;
  /** Column END_EVENT_ID. */
  ulonglong m_end_event_id;
  /** Column NESTING_EVENT_ID. */
  ulonglong m_nesting_event_id;
  /** Column NESTING_EVENT_TYPE. */
  enum_event_type m_nesting_event_type;
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column TIMER_START. */
  ulonglong m_timer_start;
  /** Column TIMER_END. */
  ulonglong m_timer_end;
  /** Column TIMER_WAIT. */
  ulonglong m_timer_wait;
  /** Column OBJECT_TYPE. */
  const char *m_object_type;
  /** Length in bytes of @c m_object_type. */
  uint m_object_type_length;
  /** Column OBJECT_SCHEMA. */
  char m_object_schema[COL_OBJECT_SCHEMA_SIZE];
  /** Length in bytes of @c m_object_schema. */
  uint m_object_schema_length;
  /** Column OBJECT_NAME. */
  char m_object_name[COL_OBJECT_NAME_EXTENDED_SIZE];
  /** Length in bytes of @c m_object_name. */
  uint m_object_name_length;
  /** Column INDEX_NAME. */
  char m_index_name[COL_INDEX_NAME_SIZE];
  /** Length in bytes of @c m_index_name. */
  uint m_index_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  intptr m_object_instance_addr;
  /** Column SOURCE. */
  char m_source[COL_SOURCE_SIZE];
  /** Length in bytes of @c m_source. */
  uint m_source_length;
  /** Column OPERATION. */
  enum_operation_type m_operation;
  /** Column NUMBER_OF_BYTES. */
  ulonglong m_number_of_bytes;
  /** Column FLAGS. */
  uint m_flags;
};

/** Position of a cursor on PERFORMANCE_SCHEMA.EVENTS_WAITS_CURRENT. */
struct pos_events_waits_current : public PFS_double_index
{
  pos_events_waits_current() : PFS_double_index(0, 0)
  {
  }

  inline void
  reset(void)
  {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void
  next_thread(void)
  {
    m_index_1++;
    m_index_2 = 0;
  }
};

/** Position of a cursor on PERFORMANCE_SCHEMA.EVENTS_WAITS_HISTORY. */
struct pos_events_waits_history : public PFS_double_index
{
  pos_events_waits_history() : PFS_double_index(0, 0)
  {
  }

  inline void
  reset(void)
  {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void
  next_thread(void)
  {
    m_index_1++;
    m_index_2 = 0;
  }
};

class PFS_index_events_waits : public PFS_engine_index
{
public:
  PFS_index_events_waits()
    : PFS_engine_index(&m_key_1, &m_key_2),
      m_key_1("THREAD_ID"),
      m_key_2("EVENT_ID")
  {
  }

  ~PFS_index_events_waits()
  {
  }

  bool match(PFS_thread *pfs);
  bool match(PFS_events_waits *pfs);

private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

/**
  Adapter, for table sharing the structure of
  PERFORMANCE_SCHEMA.EVENTS_WAITS_CURRENT.
*/
class table_events_waits_common : public PFS_engine_table
{
protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_events_waits_common(const PFS_engine_table_share *share, void *pos);

  ~table_events_waits_common()
  {
  }

  void clear_object_columns();
  int make_table_object_columns(PFS_events_waits *wait);
  int make_file_object_columns(PFS_events_waits *wait);
  int make_socket_object_columns(PFS_events_waits *wait);
  int make_metadata_lock_object_columns(PFS_events_waits *wait);

  int make_row(PFS_events_waits *wait);

  /** Current row. */
  row_events_waits m_row;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_CURRENT. */
class table_events_waits_current : public table_events_waits_common
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int index_init(uint idx, bool sorted);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual int index_next();
  virtual void reset_position(void);

protected:
  table_events_waits_current();

public:
  ~table_events_waits_current()
  {
  }

private:
  friend class table_events_waits_history;
  friend class table_events_waits_history_long;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  PFS_events_waits *get_wait(PFS_thread *pfs_thread, uint index_2);
  int make_row(PFS_thread *thread, PFS_events_waits *wait);

  /** Current position. */
  pos_events_waits_current m_pos;
  /** Next position. */
  pos_events_waits_current m_next_pos;

  PFS_index_events_waits *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_HISTORY. */
class table_events_waits_history : public table_events_waits_common
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

protected:
  table_events_waits_history();

public:
  ~table_events_waits_history()
  {
  }

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  PFS_events_waits *get_wait(PFS_thread *pfs_thread, uint index_2);
  int make_row(PFS_thread *thread, PFS_events_waits *wait);

  /** Current position. */
  pos_events_waits_history m_pos;
  /** Next position. */
  pos_events_waits_history m_next_pos;

  PFS_index_events_waits *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_HISTORY_LONG. */
class table_events_waits_history_long : public table_events_waits_common
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  table_events_waits_history_long();

public:
  ~table_events_waits_history_long()
  {
  }

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
