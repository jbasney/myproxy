#include "globus_i_gram_protocol.h"

gss_cred_id_t				globus_i_gram_protocol_credential;
globus_mutex_t				globus_i_gram_protocol_mutex;
globus_cond_t				globus_i_gram_protocol_cond;

globus_list_t *				globus_i_gram_protocol_listeners;
globus_list_t *				globus_i_gram_protocol_connections;
globus_bool_t 				globus_i_gram_protocol_shutdown_called;
globus_io_attr_t			globus_i_gram_protocol_default_attr;
int					globus_i_gram_protocol_num_connects;
globus_gram_protocol_handle_t		globus_i_gram_protocol_handle;

static int globus_l_gram_protocol_activate(void);
static int globus_l_gram_protocol_deactivate(void);

globus_module_descriptor_t globus_i_gram_protocol_module =
{
    "globus_gram_protocol",
    globus_l_gram_protocol_activate,
    globus_l_gram_protocol_deactivate,
    GLOBUS_NULL
};


static
int
globus_l_gram_protocol_activate(void)
{
    OM_uint32				major_status;
    OM_uint32				minor_status;
    globus_result_t			result;

    result = globus_module_activate(GLOBUS_IO_MODULE);
    if(result != GLOBUS_SUCCESS)
    {
	return GLOBUS_GRAM_PROTOCOL_ERROR_NO_RESOURCES;
    }
    /*
     * Get the GSSAPI security credential for this process.
     * we save it in static storage, since it is only
     * done once and can be shared by many threads.
     * with some GSSAPI implementations a prompt to the user
     * may be done from this routine.
     *
     * we will use the assist version of acquire_cred
     */

    major_status = globus_gss_assist_acquire_cred(&minor_status,
                        GSS_C_BOTH,
                        &globus_i_gram_protocol_credential);

    if (major_status != GSS_S_COMPLETE)
    {
        globus_gss_assist_display_status(stderr,
                "gram_init failure:",
                major_status,
                minor_status,
                0);

	globus_module_deactivate(GLOBUS_IO_MODULE);

        return GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION; /* need better return code */
    }

    globus_i_gram_protocol_listeners = GLOBUS_NULL;
    globus_i_gram_protocol_connections = GLOBUS_NULL;
    globus_i_gram_protocol_shutdown_called = GLOBUS_FALSE;
    globus_i_gram_protocol_num_connects = 0;
    globus_mutex_init(&globus_i_gram_protocol_mutex, GLOBUS_NULL);
    globus_cond_init(&globus_i_gram_protocol_cond, GLOBUS_NULL);
    globus_gram_protocol_setup_attr(&globus_i_gram_protocol_default_attr);

    return GLOBUS_SUCCESS;
}


static int
globus_l_gram_protocol_deactivate(void)
{
    globus_i_gram_protocol_listener_t *  listener;

    /*
     * wait for in-progress commands to complete
     */
    globus_mutex_lock( &globus_i_gram_protocol_mutex );
    {
	globus_i_gram_protocol_shutdown_called = GLOBUS_TRUE;

	/* finish all outstanding accepts */
	while (!globus_list_empty(globus_i_gram_protocol_listeners))
	{
	    listener = (globus_i_gram_protocol_listener_t *)
		globus_list_first(globus_i_gram_protocol_listeners);

	    globus_i_gram_protocol_callback_disallow(listener);
	}

	/* wait for all outgoing connections to get replies */
	while (globus_i_gram_protocol_num_connects != 0)
	{
	    globus_cond_wait(&globus_i_gram_protocol_cond,
			     &globus_i_gram_protocol_mutex);
	}
    }
    globus_mutex_unlock(&globus_i_gram_protocol_mutex);
    globus_mutex_destroy(&globus_i_gram_protocol_mutex);

    globus_module_deactivate(GLOBUS_IO_MODULE);
    /*
     * GSSAPI - cleanup of the credential
     * don't really care about returned status
     */
    if (globus_i_gram_protocol_credential != GSS_C_NO_CREDENTIAL)
    {
        OM_uint32 minor_status;
        gss_release_cred(&minor_status,
                         &globus_i_gram_protocol_credential);
	globus_i_gram_protocol_credential = GSS_C_NO_CREDENTIAL;
    }

    globus_i_gram_protocol_listeners = GLOBUS_NULL;
    return GLOBUS_SUCCESS;
}

/************************* help function *********************************/

int
globus_gram_protocol_setup_attr(globus_io_attr_t *  attr)
{
    globus_result_t                        res;
    globus_io_secure_authorization_data_t  auth_data;

    /* acquire mutex */
    if ( (res = globus_io_tcpattr_init(attr))
	 || (res = globus_io_secure_authorization_data_initialize(
	                &auth_data))
	 || (res = globus_io_attr_set_secure_authentication_mode(
	                attr,
			GLOBUS_IO_SECURE_AUTHENTICATION_MODE_MUTUAL,
			globus_i_gram_protocol_credential))
	 || (res = globus_io_attr_set_secure_authorization_mode(
	                attr,
			GLOBUS_IO_SECURE_AUTHORIZATION_MODE_SELF,
			&auth_data))
	 || (res = globus_io_attr_set_secure_channel_mode(
	                attr,
			GLOBUS_IO_SECURE_CHANNEL_MODE_SSL_WRAP)) )
    {
	globus_object_t *  err = globus_error_get(res);
	globus_object_free(err);
	/* release mutex */
	return GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED;
    }

    /* release mutex */
    return GLOBUS_SUCCESS;
}
