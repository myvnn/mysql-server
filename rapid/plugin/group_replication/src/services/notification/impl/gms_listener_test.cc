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

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/group_member_status_listener.h>
#include <mysql/components/services/group_membership_listener.h>

#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_log.h"
#include "plugin/group_replication/include/services/notification/impl/gms_listener_test.h"
#include "plugin/group_replication/include/services/notification/notification.h"
#include "plugin/group_replication/include/sql_service/sql_service_command.h"
#include "plugin/group_replication/include/sql_service/sql_service_interface.h"

#define UNREGISTER 1
#define REGISTER 0

static SERVICE_TYPE(group_membership_listener) svc_gms_def= {
  group_membership_listener_example_impl::notify_view_change,
  group_membership_listener_example_impl::notify_quorum_lost
};

static SERVICE_TYPE(group_member_status_listener) svc_gmst_def= {
  group_member_status_listener_example_impl::notify_member_role_change,
  group_member_status_listener_example_impl::notify_member_state_change
};

my_h_service h_gms_listener_example= (my_h_service) &svc_gms_def;
my_h_service h_gmst_listener_example= (my_h_service) &svc_gmst_def;

bool
log_notification_to_test_table(std::string msg)
{
  int res= 0;
  Sql_resultset rset;
  ulong srv_err= 0;
  bool was_read_only= false;
  Sql_service_command_interface *sql_cmd= new Sql_service_command_interface();
  Sql_service_interface *sql_intf= NULL;
  enum_plugin_con_isolation trx_iso=
    current_thd == NULL ? PSESSION_INIT_THREAD : PSESSION_USE_THREAD;
  std::stringstream ss;

  ss.str("");
  ss.clear();
  ss << "Openning session.";
  if (sql_cmd->establish_session_connection(trx_iso, GROUPREPL_USER,
                                            get_plugin_pointer()))
  {
    res= 1;    /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  ss.str("");
  ss.clear();
  if (!(sql_intf= sql_cmd->get_sql_service_interface()))
  {
    res= 2;    /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  ss.str("");
  ss.clear();
  ss << "SET SESSION SQL_LOG_BIN=0";
  if ((srv_err= sql_intf->execute_query(ss.str())))
  {
    res= 3;    /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  if (sql_cmd->get_server_super_read_only())
  {
    /*
      When joining the group the server is in super_read_only.
      Unset this temporarily.
    */
    was_read_only= true;
    ss.str("");
    ss.clear();
    ss << "SET GLOBAL super_read_only=0";
    if ((srv_err= sql_intf->execute_query(ss.str())))
    {
      res= 4;    /* purecov: inspected */
      goto end;  /* purecov: inspected */
    }
  }

  ss.str("");
  ss.clear();
  ss << "CREATE TABLE IF NOT EXISTS test.gms_listener_example";
  ss << "(log_message TEXT)";
  if ((srv_err= sql_intf->execute_query(ss.str())))
  {
    res= 5;    /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  /* Create the message string */
  ss.str("");
  ss.clear();
  ss << "INSERT INTO test.gms_listener_example VALUES ('" << msg << "')";
  if ((srv_err= sql_intf->execute_query(ss.str())))
  {
    res= 6;    /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

end:
  if (res)
  {
    log_message(MY_WARNING_LEVEL, "Unable to log notification to table ("
      "errno: %lu) (res: %d)! Message: %s",
      srv_err, res, ss.str().c_str()); /* purecov: inspected */
  }

  if (was_read_only)
  {
    /* Revert back if it was set to super read only. */
    ss.str("");
    ss.clear();
    ss << "SET GLOBAL super_read_only=1";
    if ((srv_err= sql_intf->execute_query(ss.str())))
    {
      res= 7;   /* purecov: inspected */
      goto end; /* purecov: inspected */
    }
  }

  delete sql_cmd;
  return res != 0;
}

/**
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(
  group_membership_listener_example_impl::notify_view_change, (const char* v))
{
  std::stringstream ss;
  ss << "VIEW CHANGED: " << v;
  log_notification_to_test_table(ss.str());

  return false;
}

DEFINE_BOOL_METHOD(group_membership_listener_example_impl::notify_quorum_lost,(const char* v))
{
  std::stringstream ss;
  ss << "QUORUM LOST: " << v;
  log_notification_to_test_table(ss.str());

  return false;
}

DEFINE_BOOL_METHOD(group_member_status_listener_example_impl::notify_member_role_change,
  (const char* v))
{
  std::stringstream ss;
  ss << "ROLE CHANGED: " << v;
  log_notification_to_test_table(ss.str());

  return false;
}

DEFINE_BOOL_METHOD(group_member_status_listener_example_impl::notify_member_state_change,
  (const char* v))
{
  std::stringstream ss;
  ss << "STATE CHANGED: " << v;
  log_notification_to_test_table(ss.str());

  return false;
}

/*** Auxiliary functions for registering / unregistering the listener. */

static void handle_example_listener(int action)
{
  SERVICE_TYPE(registry) *r= mysql_plugin_registry_acquire();
  SERVICE_TYPE(registry_registration) *reg_reg= NULL;
  my_h_service h_reg_svc= NULL;
  int error= 0;

  if(!r)
    goto err; /* purecov: inspected */

  // handle to registry query
  if (r->acquire("registry_registration", &h_reg_svc) || !h_reg_svc)
    goto err; /* purecov: inspected */

  reg_reg= reinterpret_cast<SERVICE_TYPE(registry_registration) *>(h_reg_svc);

  switch(action)
  {
  case REGISTER:
    error= reg_reg->register_service(GMS_LISTENER_EXAMPLE_NAME,
                                     h_gms_listener_example);
    error= reg_reg->register_service(GMST_LISTENER_EXAMPLE_NAME,
                                     h_gmst_listener_example) || error;
    if (error)
      goto err; /* purecov: inspected */
    break;
  case UNREGISTER:
    error= reg_reg->unregister(GMS_LISTENER_EXAMPLE_NAME);
    error= reg_reg->unregister(GMST_LISTENER_EXAMPLE_NAME) || error;
    if (error)
      goto err;
    break;
  default:
    DBUG_ASSERT(0); /* purecov: inspected */
  }

err:
  // release the registry
  if (h_reg_svc)
    r->release(h_reg_svc);

  // release the handle
  if (r)
    mysql_plugin_registry_release(r);
}

void unregister_listener_service_gr_example()
{
  handle_example_listener(UNREGISTER);
}

void register_listener_service_gr_example()
{
  handle_example_listener(REGISTER);
}
