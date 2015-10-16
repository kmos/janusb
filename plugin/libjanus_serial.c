/*! \file   janus_serial.c
 * \author Giovanni Panice <mosfet@paranoici.org>
 * \author Antonio Tammaro <ntonjeta@autistici.org>
 * \copyright GNU General Public License v3
 * \brief  Janus Serial plugin
 * 
 *
 */

#include "../janus-gateway/plugin.h"

#include <jansson.h>

#include "../janus-gateway/debug.h"
#include "../janus-gateway/apierror.h"
#include "../janus-gateway/config.h"
#include "../janus-gateway/mutex.h"
#include "../janus-gateway/record.h"
#include "../janus-gateway/rtcp.h"
#include "../janus-gateway/utils.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
//#include <errno.h>


/* Plugin information */
#define JANUS_SERIAL_VERSION			1
#define JANUS_SERIAL_VERSION_STRING	 "0.0.1"
#define JANUS_SERIAL_DESCRIPTION		 "This is a Serial plugin for Janus"
#define JANUS_SERIAL_NAME			       "JANUS Serial plugin"
#define JANUS_SERIAL_AUTHOR			     "Mosfet & Friends"
#define JANUS_SERIAL_PACKAGE			   "janus.plugin.serial"

/* Error codes */
#define JANUS_SERIAL_ERROR_NO_MESSAGE      411
#define JANUS_SERIAL_ERROR_INVALID_JSON    412
#define JANUS_SERIAL_ERROR_INVALID_ELEMENT 413

/* Use Variable */
char *portname = "/dev/ttyACM0";
int fd;
struct termios toptions;

/* Useful stuff */
static volatile gint initialized = 0, stopping = 0;
static janus_callbacks *gateway = NULL;
//Thread 
static GThread *handler_thread;
static GThread *watchdog;
//Mesage queue
static GAsyncQueue *messages = NULL;
//Hash Table
static GHashTable *sessions;
//List
static GList *old_sessions;
//Mutex
static janus_mutex sessions_mutex;


//Messaggio JSON di sessione
typedef struct janus_serial_message {
  janus_plugin_session *handle;
  char *transaction;
  char *message;
  char *sdp_type;
  char *sdp;     
} janus_serial_message;


/*Plugin session typedef */
typedef struct janus_serial_session {
  //Puttana Eva
  janus_plugin_session *handle;
  //uint64_t bitrate;
  
  guint16 slowlink_count;
  volatile gint hangingup; /*Indica lo stato in cui la sessione si è chiusa e si prova un riaggancio */
  gint64 destroyed; /* Time at which this session was marked as destroyed */
} janus_serial_session;

/* Plugin methods */
janus_plugin *create(void);
int janus_serial_init(janus_callbacks *callback, const char *config_path);
void janus_serial_destroy(void);
void janus_serial_message_free(janus_serial_message *msg);
/* Plugin Get method */
int janus_serial_get_api_compatibility(void);
int janus_serial_get_version(void);
const char *janus_serial_get_version_string(void);
const char *janus_serial_get_description(void);
const char *janus_serial_get_name(void);
const char *janus_serial_get_author(void);
const char *janus_serial_get_package(void);
/* Plugin Session Method */
void janus_serial_create_session(janus_plugin_session *handle, int *error);
void janus_serial_destroy_session(janus_plugin_session *handle, int *error);
char *janus_serial_query_session(janus_plugin_session *handle);
/* Plugin Callback Method */
struct janus_plugin_result *janus_serial_handle_message(janus_plugin_session *handle, char *transaction, char *message, char *sdp_type, char *sdp);
void janus_serial_setup_media(janus_plugin_session *handle);
void janus_serial_hangup_media(janus_plugin_session *handle);
/* Plugin Thread Method */
static void *janus_serial_handler(void *data); //Cosa sei???
void *janus_serial_watchdog(void *data);



/* Plugin setup */
static janus_plugin janus_serial_plugin =
  JANUS_PLUGIN_INIT (
    .init = janus_serial_init,
    .destroy = janus_serial_destroy,

    .get_api_compatibility = janus_serial_get_api_compatibility,
    .get_version = janus_serial_get_version,
    .get_version_string = janus_serial_get_version_string,
    .get_description = janus_serial_get_description,
    .get_name = janus_serial_get_name,
    .get_author = janus_serial_get_author,
    .get_package = janus_serial_get_package,
		
    .create_session = janus_serial_create_session,
    .handle_message = janus_serial_handle_message,
    .setup_media = janus_serial_setup_media,
    .hangup_media = janus_serial_hangup_media,
    .destroy_session = janus_serial_destroy_session,
    .query_session = janus_serial_query_session,
  );

/* Plugin creator */
janus_plugin *create(void) {
  JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_SERIAL_NAME);
  return &janus_serial_plugin;
}

void janus_serial_message_free(janus_serial_message *msg) {
  if(!msg)
    return;

  msg->handle = NULL;

  g_free(msg->transaction);
  msg->transaction = NULL;
  g_free(msg->message);
  msg->message = NULL;
  g_free(msg->sdp_type);
  msg->sdp_type = NULL;
  g_free(msg->sdp);
  msg->sdp = NULL;

  g_free(msg);
}

/* Watchdog Thread Implementation */
/* Serial watchdog/garbage collector (sort of) */
void *janus_serial_watchdog(void *data) {
  JANUS_LOG(LOG_INFO, "Serial watchdog started\n");
  gint64 now = 0;
  while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
    janus_mutex_lock(&sessions_mutex);
    /* Iterate on all the sessions */
    now = janus_get_monotonic_time();
    if(old_sessions != NULL) {
      GList *sl = old_sessions;
      JANUS_LOG(LOG_HUGE, "Checking %d old Serial sessions...\n", g_list_length(old_sessions));
      while(sl) {
        janus_serial_session *session = (janus_serial_session *)sl->data;
	if(!session) {
	  sl = sl->next;
	  continue;
	}
	if(now-session->destroyed >= 5*G_USEC_PER_SEC) {
          /* We're lazy and actually get rid of the stuff only after a few seconds */
	  JANUS_LOG(LOG_VERB, "Freeing old Serial session\n");
	  GList *rm = sl->next;
	  old_sessions = g_list_delete_link(old_sessions, sl);
	  sl = rm;
	  session->handle = NULL;
	  g_free(session);
	  session = NULL;
	  continue;
	}
	sl = sl->next;
      }
    }
    janus_mutex_unlock(&sessions_mutex);
    g_usleep(500000);
  }
  JANUS_LOG(LOG_INFO, "Serial watchdog stopped\n");
  return NULL;
}

/* Plugin implementation */
int janus_serial_init(janus_callbacks *callback, const char *config_path) {
  if(g_atomic_int_get(&stopping)) {
    /* Still stopping from before */
    return -1;
  }
  if(callback == NULL || config_path == NULL) {
    /* Invalid arguments */
    return -1;
  }

  /* Read configuration */
  char filename[255];
  g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_SERIAL_PACKAGE);
  JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
  janus_config *config = janus_config_parse(filename);
  if(config != NULL)
    janus_config_print(config);
  /* This plugin actually has nothing to configure... */
  janus_config_destroy(config);
  config = NULL;
  
  sessions = g_hash_table_new(NULL, NULL);
  janus_mutex_init(&sessions_mutex);
  messages = g_async_queue_new_full((GDestroyNotify) janus_serial_message_free);
  /* This is the callback we'll need to invoke to contact the gateway */
  gateway = callback;
  g_atomic_int_set(&initialized, 1);

  if(fd = open(portname,O_RDWR | O_NOCTTY | O_NONBLOCK)){
        //printf("stream aperto\n");
    JANUS_LOG(LOG_INFO, "stream aperto - Janus Serial\n");
  }else{
    //printf("errora nell'apertura dello stream\n");
    JANUS_LOG(LOG_INFO, "errore nell'apertura dello stream\n");
    return 0;
  }
  /* Set up the control structure */

   /* Get currently set options for the tty */
  tcgetattr(fd, &toptions);

  /* Set custom options */

  /* 9600 baud */
  cfsetispeed(&toptions, B9600);
  cfsetospeed(&toptions, B9600);
  /* 8 bits, no parity, no stop bits */
  toptions.c_cflag &= ~PARENB;
  toptions.c_cflag &= ~CSTOPB;
  toptions.c_cflag &= ~CSIZE;
  toptions.c_cflag |= CS8;
  /* no hardware flow control */
  toptions.c_cflag &= ~CRTSCTS;
  /* enable receiver, ignore status lines */
  toptions.c_cflag |= CREAD | CLOCAL;
  /* disable input/output flow control, disable restart chars */
  toptions.c_iflag &= ~(IXON | IXOFF | IXANY);
  /* disable canonical input, disable echo,
  disable visually erase chars,
  disable terminal-generated signals */
  toptions.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  /* disable output processing */
  toptions.c_oflag &= ~OPOST;

  /* wait for 24 characters to come in before read returns */
  toptions.c_cc[VMIN] = 12;
  /* no minimum time to wait before read returns */
  toptions.c_cc[VTIME] = 0;

  /* commit the options */
  tcsetattr(fd, TCSANOW, &toptions);

  /* Wait for the Arduino to reset */
  usleep(1000*1000);

  //close(fd);
  /* Flush anything already in the serial buffer */
  tcflush(fd, TCIOFLUSH);
  
  JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_SERIAL_NAME);

  g_atomic_int_set(&initialized, 1);
  GError *error = NULL;
  /* Start the sessions watchdog */
  watchdog = g_thread_try_new("serial watchdog", &janus_serial_watchdog, NULL, &error);
  if(error != NULL) {
    g_atomic_int_set(&initialized, 0);
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Serial watchdog thread...\n", error->code, error->message ? error->message : "??");
    return -1;
  }
  /* Launch the thread that will handle incoming messages */
  handler_thread = g_thread_try_new("janus serial handler", janus_serial_handler, NULL, &error);
  if(error != NULL) {
    g_atomic_int_set(&initialized, 0);
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the serial handler thread...\n", error->code, error->message ? error->message : "??");
    return -1;
  }

  return 0;
}

void janus_serial_destroy(void) {
  if(!g_atomic_int_get(&initialized))
    return;
  g_atomic_int_set(&stopping, 1);

  if(handler_thread != NULL) {
    g_thread_join(handler_thread);
    handler_thread = NULL;
  }
  if(watchdog != NULL) {
    g_thread_join(watchdog);
    watchdog = NULL;
  }
  
  /* FIXME We should destroy the sessions cleanly */
  janus_mutex_lock(&sessions_mutex);
  g_hash_table_destroy(sessions);
  janus_mutex_unlock(&sessions_mutex);
  g_async_queue_unref(messages);
  messages = NULL;
  sessions = NULL;
  close(fd);

  g_atomic_int_set(&initialized, 0);
  g_atomic_int_set(&stopping, 0);
  JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_SERIAL_NAME);
}

int janus_serial_get_api_compatibility(void) {
  /* Important! This is what your plugin MUST always return: don't lie here or bad things will happen */
  return JANUS_PLUGIN_API_VERSION;
}

int janus_serial_get_version(void) {
  return JANUS_SERIAL_VERSION;
}

const char *janus_serial_get_version_string(void) {
  return JANUS_SERIAL_VERSION_STRING;
}

const char *janus_serial_get_description(void) {
  return JANUS_SERIAL_DESCRIPTION;
}

const char *janus_serial_get_name(void) {
  return JANUS_SERIAL_NAME;
}

const char *janus_serial_get_author(void) {
  return JANUS_SERIAL_AUTHOR;
}

const char *janus_serial_get_package(void) {
  return JANUS_SERIAL_PACKAGE;
}

void janus_serial_create_session(janus_plugin_session *handle, int *error) {
  if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
    *error = -1;
  return;
  }	
  janus_serial_session *session = (janus_serial_session *)calloc(1, sizeof(janus_serial_session));
  if(session == NULL) {
    JANUS_LOG(LOG_FATAL, "Memory error!\n");
    *error = -2;
    return;
  }
  session->handle = handle;
  session->destroyed = 0;
  g_atomic_int_set(&session->hangingup, 0);
  handle->plugin_handle = session;
  janus_mutex_lock(&sessions_mutex);
  g_hash_table_insert(sessions, handle, session);
  janus_mutex_unlock(&sessions_mutex);

  return;
}

void janus_serial_destroy_session(janus_plugin_session *handle, int *error) {
  if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
    *error = -1;
    return;
  }	
  janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;
  if(!session) {
    JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
    *error = -2;
    return;
  }
  JANUS_LOG(LOG_VERB, "Removing Serial session...\n");
  janus_mutex_lock(&sessions_mutex);
  if(!session->destroyed) {
    session->destroyed = janus_get_monotonic_time();
    g_hash_table_remove(sessions, handle);
    /* Cleaning up and removing the session is done in a lazy way */
    old_sessions = g_list_append(old_sessions, session);
  }
  janus_mutex_unlock(&sessions_mutex);
  return;
}

/* INFO: La funzione interoga lo stato della sessione e restituisce al chiamante una stringa json */
char *janus_serial_query_session(janus_plugin_session *handle) {
  if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
  return NULL;
  }	
  janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;
  if(!session) {
    JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
    return NULL;
  }
  /* In the plugin, every session is the same: we just provide some configure info */
  json_t *info = json_object();
  /* Buil the json */
  
  json_object_set_new(info, "slowlink_count", json_integer(session->slowlink_count));
  json_object_set_new(info, "destroyed", json_integer(session->destroyed));
  
  char *info_text = json_dumps(info, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
  json_decref(info);
  return info_text;
}

/* CallBack Implementation */ 
struct janus_plugin_result *janus_serial_handle_message(janus_plugin_session *handle, char *transaction, char *message, char *sdp_type, char *sdp) {
  if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized");

  janus_serial_message *msg = calloc(1, sizeof(janus_serial_message));

  if(msg == NULL) {
    JANUS_LOG(LOG_FATAL, "Memory error!\n");
    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Memory error");
  }
	
  // Build the message
  msg->handle = handle;
  msg->transaction = transaction;
  msg->message = message;
  msg->sdp_type = sdp_type;
  msg->sdp = sdp;
  //Push in the thread queue
  g_async_queue_push(messages, msg);
	
  /* All the requests to this plugin are handled asynchronously */
  return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, "I'm taking my time!");
}

void janus_serial_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "WebRTC media is now available\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) return;

	janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	

	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(session->destroyed) return;

	g_atomic_int_set(&session->hangingup, 0);
	
}

void janus_serial_hangup_media(janus_plugin_session *handle) {
  JANUS_LOG(LOG_INFO, "No WebRTC media anymore\n");
  if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) return;

  janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;

  if(!session) {
    JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
    return;
  }
  if(session->destroyed) return;
  if(g_atomic_int_add(&session->hangingup, 1))
    return;
  /* Send an event to the browser and tell it's over */
  json_t *event = json_object();
  json_object_set_new(event, "serial", json_string("event"));
  json_object_set_new(event, "result", json_string("done"));
  char *event_text = json_dumps(event, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
  json_decref(event);
  JANUS_LOG(LOG_VERB, "Pushing event: %s\n", event_text);
  int ret = gateway->push_event(handle, &janus_serial_plugin, NULL, event_text, NULL, NULL);
  JANUS_LOG(LOG_VERB, "  >> %d (%s)\n", ret, janus_get_api_error(ret));
  g_free(event_text);

}


/* Thread to handle incoming messages */
static void *janus_serial_handler(void *data) {
  JANUS_LOG(LOG_INFO, "JOIN Serial handler thread\n");
  JANUS_LOG(LOG_VERB, "Joining Serial handler thread\n");
  janus_serial_message *msg = NULL;
  int error_code = 0;
  char *error_cause = calloc(512, sizeof(char));
  
  if(error_cause == NULL) {
    JANUS_LOG(LOG_FATAL, "Memory error!\n");
    return NULL;
  }

  json_t *root = NULL;

  while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
    if(!messages || (msg = g_async_queue_try_pop(messages)) == NULL) {
      usleep(50000);
      continue;
    }

    janus_serial_session *session = (janus_serial_session *)msg->handle->plugin_handle;

    if(!session) {
      JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
      janus_serial_message_free(msg);
      continue;
    }
    if(session->destroyed) {
      janus_serial_message_free(msg);
      continue;
    }

    /* Handle request */
    error_code = 0;
    root = NULL;
    JANUS_LOG(LOG_VERB, "Handling message: %s\n", msg->message);
    
    if(msg->message == NULL) {
      JANUS_LOG(LOG_ERR, "No message??\n");
      error_code = JANUS_SERIAL_ERROR_NO_MESSAGE;
      g_snprintf(error_cause, 512, "%s", "No message??");
      goto error;
    }

    json_error_t error;
    root = json_loads(msg->message, 0, &error);
    if(!root) {
      JANUS_LOG(LOG_ERR, "JSON error: on line %d: %s\n", error.line, error.text);
      error_code = JANUS_SERIAL_ERROR_INVALID_JSON;
      g_snprintf(error_cause, 512, "JSON error: on line %d: %s", error.line, error.text);
      goto error;
    }
    if(!json_is_object(root)) {
      JANUS_LOG(LOG_ERR, "JSON error: not an object\n");
      error_code = JANUS_SERIAL_ERROR_INVALID_JSON;
      g_snprintf(error_cause, 512, "JSON error: not an object");
      goto error;
    }

    tcsetattr(fd, TCSANOW, &toptions);

    tcflush(fd,TCIFLUSH);

    //Local variable
    char request[256];
    char response[256];
    //Initializze local variable
    memset(request, '\0',256);
    strncpy(request,msg->message,strlen(msg->message));
    strcat(request,"\n");
    //Write on serial port
    if(write(fd,request,strlen(request)) == -1){
      JANUS_LOG(LOG_INFO,"errore nella scrittura su seriale\n");
    }
    tcdrain(fd); 

    //Active wait for MCU answer
    while(read(fd,&response,256) == -1){}
    tcflush(fd, TCIOFLUSH);
        
    int res = gateway->push_event(msg->handle, &janus_serial_plugin, NULL, response, NULL, NULL);
    janus_serial_message_free(msg);
    continue;
  		
error:
    {
      if(root != NULL) json_decref(root);
      /* Prepare JSON error event */
      json_t *event = json_object();
      json_object_set_new(event, "serial", json_string("event"));
      json_object_set_new(event, "error_code", json_integer(error_code));
      json_object_set_new(event, "error", json_string(error_cause));
      char *event_text = json_dumps(event, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
      json_decref(event);
      JANUS_LOG(LOG_VERB, "Pushing event: %s\n", event_text);
      int ret = gateway->push_event(msg->handle, &janus_serial_plugin, msg->transaction, event_text, NULL, NULL);
      JANUS_LOG(LOG_VERB, "  >> %d (%s)\n", ret, janus_get_api_error(ret));
      g_free(event_text);
      janus_serial_message_free(msg);
    }
  }
  g_free(error_cause);
  JANUS_LOG(LOG_VERB, "Leaving Serial handler thread\n");
  return NULL;
}
