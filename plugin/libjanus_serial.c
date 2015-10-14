/*! \file   janus_serial.c
 * \author Giovanni Panice <mosfet@paranoici.org>
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


/* Plugin information */
#define JANUS_SERIAL_VERSION			1
#define JANUS_SERIAL_VERSION_STRING		"0.0.1"
#define JANUS_SERIAL_DESCRIPTION		"This is a Serial plugin for Janus"
#define JANUS_SERIAL_NAME			"JANUS Serial plugin"
#define JANUS_SERIAL_AUTHOR			"Mosfet & Friends"
#define JANUS_SERIAL_PACKAGE			"janus.plugin.serial"

/* Plugin methods */
janus_plugin *create(void);
int janus_serial_init(janus_callbacks *callback, const char *config_path);
void janus_serial_destroy(void);
int janus_serial_get_api_compatibility(void);
int janus_serial_get_version(void);
const char *janus_serial_get_version_string(void);
const char *janus_serial_get_description(void);
const char *janus_serial_get_name(void);
const char *janus_serial_get_author(void);
const char *janus_serial_get_package(void);
void janus_serial_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_serial_handle_message(janus_plugin_session *handle, char *transaction, char *message, char *sdp_type, char *sdp);
void janus_serial_setup_media(janus_plugin_session *handle);
void janus_serial_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_serial_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_serial_incoming_data(janus_plugin_session *handle, char *buf, int len);
void janus_serial_slow_link(janus_plugin_session *handle, int uplink, int video);
void janus_serial_hangup_media(janus_plugin_session *handle);
void janus_serial_destroy_session(janus_plugin_session *handle, int *error);
char *janus_serial_query_session(janus_plugin_session *handle);


//COM
char *portname = "/dev/ttyACM0";
int fd,n;


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
		.incoming_rtp = janus_serial_incoming_rtp,
		.incoming_rtcp = janus_serial_incoming_rtcp,
		.incoming_data = janus_serial_incoming_data,
		.slow_link = janus_serial_slow_link,
		.hangup_media = janus_serial_hangup_media,
		.destroy_session = janus_serial_destroy_session,
		.query_session = janus_serial_query_session,
	);

/* Plugin creator */
janus_plugin *create(void) {
	JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_SERIAL_NAME);
	return &janus_serial_plugin;
}


/* Useful stuff */
static volatile gint initialized = 0, stopping = 0;
static janus_callbacks *gateway = NULL;
static GThread *handler_thread;
static GThread *watchdog;
static void *janus_serial_handler(void *data);

typedef struct janus_serial_message {
	janus_plugin_session *handle;
	char *transaction;
	char *message;
	char *sdp_type;
	char *sdp;
} janus_serial_message;
static GAsyncQueue *messages = NULL;

typedef struct janus_serial_session {
	janus_plugin_session *handle;
	gboolean has_audio;
	gboolean has_video;
	gboolean audio_active;
	gboolean video_active;
	uint64_t bitrate;
	janus_recorder *arc;	/* The Janus recorder instance for this user's audio, if enabled */
	janus_recorder *vrc;	/* The Janus recorder instance for this user's video, if enabled */
	guint16 slowlink_count;
	volatile gint hangingup;
	gint64 destroyed;	/* Time at which this session was marked as destroyed */
} janus_serial_session;
static GHashTable *sessions;
static GList *old_sessions;
static janus_mutex sessions_mutex;

void janus_serial_message_free(janus_serial_message *msg);
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


/* Error codes */
#define JANUS_SERIAL_ERROR_NO_MESSAGE			411
#define JANUS_SERIAL_ERROR_INVALID_JSON		412
#define JANUS_SERIAL_ERROR_INVALID_ELEMENT	413


/* EchoTest watchdog/garbage collector (sort of) */
void *janus_serial_watchdog(void *data);
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
	
	// init part
	/* Open the file descriptor in non-blocking mode */
	if(fd = open(portname,O_RDWR | O_NOCTTY | O_NONBLOCK)){
    		//printf("stream aperto\n");
		JANUS_LOG(LOG_INFO, "stream aperto - Janus Serial\n");
	}else{
		//printf("errora nell'apertura dello stream\n");
		JANUS_LOG(LOG_INFO, "errore nell'apertura dello stream\n");
		return 0;
	}
	/* Set up the control structure */
	struct termios toptions;

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

	/* Flush anything already in the serial buffer */
	tcflush(fd, TCIFLUSH);
	
	write(fd,"k",1);	
	
	JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_SERIAL_NAME);
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
	session->has_audio = FALSE;
	session->has_video = FALSE;
	session->audio_active = TRUE;
	session->video_active = TRUE;
	session->bitrate = 0;	/* No limit */
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

char *janus_serial_query_session(janus_plugin_session *handle) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		return NULL;
	}	
	janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return NULL;
	}
	/* In the echo test, every session is the same: we just provide some configure info */
	json_t *info = json_object();
	json_object_set_new(info, "audio_active", json_string(session->audio_active ? "true" : "false"));
	json_object_set_new(info, "video_active", json_string(session->video_active ? "true" : "false"));
	json_object_set_new(info, "bitrate", json_integer(session->bitrate));
	if(session->arc || session->vrc) {
		json_t *recording = json_object();
		if(session->arc && session->arc->filename)
			json_object_set_new(recording, "audio", json_string(session->arc->filename));
		if(session->vrc && session->vrc->filename)
			json_object_set_new(recording, "video", json_string(session->vrc->filename));
		json_object_set_new(info, "recording", recording);
	}
	json_object_set_new(info, "slowlink_count", json_integer(session->slowlink_count));
	json_object_set_new(info, "destroyed", json_integer(session->destroyed));
	char *info_text = json_dumps(info, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
	json_decref(info);
	return info_text;
}

struct janus_plugin_result *janus_serial_handle_message(janus_plugin_session *handle, char *transaction, char *message, char *sdp_type, char *sdp) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized");
	janus_serial_message *msg = calloc(1, sizeof(janus_serial_message));
	if(msg == NULL) {
		JANUS_LOG(LOG_FATAL, "Memory error!\n");
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Memory error");
	}
	
	json_t *root,*data;
	json_error_t error;
	const char *str;

	msg->handle = handle;
	msg->transaction = transaction;
	msg->message = message;
	msg->sdp_type = sdp_type;
	msg->sdp = sdp;
	g_async_queue_push(messages, msg);
	//inserito da giovanni
	//Janus.log("gelosia messaggio" + message);
	//JANUS_LOG(LOG_INFO, "GELOSIA messaggio: %s \n", message);

	root = json_loads(message, 0, &error);
	data = json_object_get(root, "audio" );
	str  = json_string_value(data);	
	JANUS_LOG(LOG_INFO, "messaggio parsato: %s \n", str);
        char request[256];
        memset(request,'\0',256);
        strncpy(request,message,strlen(message));

        write(fd,request,strlen(request));
        usleep(25+strlen(request)*100);

	/* All the requests to this plugin are handled asynchronously */
	return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, "I'm taking my time!");
}

void janus_serial_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "WebRTC media is now available\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(session->destroyed)
		return;
	g_atomic_int_set(&session->hangingup, 0);
	/* We really don't care, as we only send RTP/RTCP we get in the first place back anyway */
}

void janus_serial_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	/* Simple echo test */
	if(gateway) {
		/* Honour the audio/video active flags */
		janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	
		if(!session) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		if(session->destroyed)
			return;
		if((!video && session->audio_active) || (video && session->video_active)) {
			/* Save the frame if we're recording */
			if(video && session->vrc)
				janus_recorder_save_frame(session->vrc, buf, len);
			else if(!video && session->arc)
				janus_recorder_save_frame(session->arc, buf, len);
			/* Send the frame back */
			gateway->relay_rtp(handle, video, buf, len);
		}
	}
}

void janus_serial_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	/* Simple echo test */
	if(gateway) {
		janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	
		if(!session) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		if(session->destroyed)
			return;
		if(session->bitrate > 0)
			janus_rtcp_cap_remb(buf, len, session->bitrate);
		gateway->relay_rtcp(handle, video, buf, len);
	}
}

void janus_serial_incoming_data(janus_plugin_session *handle, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	/* Simple echo test */
	if(gateway) {
		janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	
		if(!session) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		if(session->destroyed)
			return;
		if(buf == NULL || len <= 0)
			return;
		char *text = g_malloc0(len+1);
		memcpy(text, buf, len);
		*(text+len) = '\0';
		JANUS_LOG(LOG_VERB, "Got a DataChannel message (%zu bytes) to bounce back: %s\n", strlen(text), text);
		/* We send back the same text with a custom prefix */
		const char *prefix = "Janus Serial here! You wrote: ";
		char *reply = g_malloc0(strlen(prefix)+len+1);
		g_snprintf(reply, strlen(prefix)+len+1, "%s%s", prefix, text);
		g_free(text);
		gateway->relay_data(handle, reply, strlen(reply));
		g_free(reply);
	}
}

void janus_serial_slow_link(janus_plugin_session *handle, int uplink, int video) {
	/* The core is informing us that our peer got or sent too many NACKs, are we pushing media too hard? */
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;	
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(session->destroyed)
		return;
	session->slowlink_count++;
	if(uplink && !video && !session->audio_active) {
		/* We're not relaying audio and the peer is expecting it, so NACKs are normal */
		JANUS_LOG(LOG_VERB, "Getting a lot of NACKs (slow uplink) for audio, but that's expected, a configure disabled the audio forwarding\n");
	} else if(uplink && video && !session->video_active) {
		/* We're not relaying video and the peer is expecting it, so NACKs are normal */
		JANUS_LOG(LOG_VERB, "Getting a lot of NACKs (slow uplink) for video, but that's expected, a configure disabled the video forwarding\n");
	} else {
		/* Slow uplink or downlink, maybe we set the bitrate cap too high? */
		if(video) {
			/* Halve the bitrate, but don't go too low... */
			session->bitrate = session->bitrate > 0 ? session->bitrate : 512*1024;
			session->bitrate = session->bitrate/2;
			if(session->bitrate < 64*1024)
				session->bitrate = 64*1024;
			JANUS_LOG(LOG_WARN, "Getting a lot of NACKs (slow %s) for %s, forcing a lower REMB: %"SCNu64"\n",
				uplink ? "uplink" : "downlink", video ? "video" : "audio", session->bitrate);
			/* ... and send a new REMB back */
			char rtcpbuf[200];
			memset(rtcpbuf, 0, 200);
			/* FIXME First put a RR (fake)... */
			int rrlen = 32;
			rtcp_rr *rr = (rtcp_rr *)&rtcpbuf;
			rr->header.version = 2;
			rr->header.type = RTCP_RR;
			rr->header.rc = 1;
			rr->header.length = htons((rrlen/4)-1);
			/* ... then put a SDES... */
			int sdeslen = janus_rtcp_sdes((char *)(&rtcpbuf)+rrlen, 200-rrlen, "janusvideo", 10);
			if(sdeslen > 0) {
				/* ... and then finally a REMB */
				janus_rtcp_remb((char *)(&rtcpbuf)+rrlen+sdeslen, 24, session->bitrate);
				gateway->relay_rtcp(handle, 1, rtcpbuf, rrlen+sdeslen+24);
			}
			/* As a last thing, notify the user about this */
			json_t *event = json_object();
			json_object_set_new(event, "serial", json_string("event"));
			json_t *result = json_object();
			json_object_set_new(result, "status", json_string("slow_link"));
			json_object_set_new(result, "bitrate", json_integer(session->bitrate));
			json_object_set_new(event, "result", result);
			char *event_text = json_dumps(event, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
			json_decref(event);
			json_decref(result);
			event = NULL;
			gateway->push_event(session->handle, &janus_serial_plugin, NULL, event_text, NULL, NULL);
			g_free(event_text);
		}
	}
}

void janus_serial_hangup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "No WebRTC media anymore\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_serial_session *session = (janus_serial_session *)handle->plugin_handle;
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(session->destroyed)
		return;
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
	/* Get rid of the recorders, if available */
	if(session->arc) {
		janus_recorder_close(session->arc);
		JANUS_LOG(LOG_INFO, "Closed audio recording %s\n", session->arc->filename ? session->arc->filename : "??");
		janus_recorder_free(session->arc);
	}
	session->arc = NULL;
	if(session->vrc) {
		janus_recorder_close(session->vrc);
		JANUS_LOG(LOG_INFO, "Closed video recording %s\n", session->vrc->filename ? session->vrc->filename : "??");
		janus_recorder_free(session->vrc);
	}
	session->vrc = NULL;
	/* Reset controls */
	session->has_audio = FALSE;
	session->has_video = FALSE;
	session->audio_active = TRUE;
	session->video_active = TRUE;
	session->bitrate = 0;
}

/* Thread to handle incoming messages */
static void *janus_serial_handler(void *data) {
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
		/* Parse request */
		json_t *audio = json_object_get(root, "audio");
		if(audio && !json_is_boolean(audio)) {
			JANUS_LOG(LOG_ERR, "Invalid element (audio should be a boolean)\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Invalid value (audio should be a boolean)");
			goto error;
		}
		json_t *video = json_object_get(root, "video");
		if(video && !json_is_boolean(video)) {
			JANUS_LOG(LOG_ERR, "Invalid element (video should be a boolean)\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Invalid value (video should be a boolean)");
			goto error;
		}
		json_t *bitrate = json_object_get(root, "bitrate");
		if(bitrate && (!json_is_integer(bitrate) || json_integer_value(bitrate) < 0)) {
			JANUS_LOG(LOG_ERR, "Invalid element (bitrate should be a positive integer)\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Invalid value (bitrate should be a positive integer)");
			goto error;
		}
		json_t *record = json_object_get(root, "record");
		if(record && !json_is_boolean(record)) {
			JANUS_LOG(LOG_ERR, "Invalid element (record should be a boolean)\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Invalid value (record should be a boolean)");
			goto error;
		}
		json_t *recfile = json_object_get(root, "filename");
		if(recfile && !json_is_string(recfile)) {
			JANUS_LOG(LOG_ERR, "Invalid element (filename should be a string)\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Invalid value (filename should be a string)");
			goto error;
		}
		/* Enforce request */
		if(audio) {
			session->audio_active = json_is_true(audio);
			JANUS_LOG(LOG_VERB, "Setting audio property: %s\n", session->audio_active ? "true" : "false");
		}
		if(video) {
			if(!session->video_active && json_is_true(video)) {
				/* Send a PLI */
				JANUS_LOG(LOG_VERB, "Just (re-)enabled video, sending a PLI to recover it\n");
				char buf[12];
				memset(buf, 0, 12);
				janus_rtcp_pli((char *)&buf, 12);
				gateway->relay_rtcp(session->handle, 1, buf, 12);
			}
			session->video_active = json_is_true(video);
			JANUS_LOG(LOG_VERB, "Setting video property: %s\n", session->video_active ? "true" : "false");
		}
		if(bitrate) {
			session->bitrate = json_integer_value(bitrate);
			JANUS_LOG(LOG_VERB, "Setting video bitrate: %"SCNu64"\n", session->bitrate);
			if(session->bitrate > 0) {
				/* FIXME Generate a new REMB (especially useful for Firefox, which doesn't send any we can cap later) */
				char buf[24];
				memset(buf, 0, 24);
				janus_rtcp_remb((char *)&buf, 24, session->bitrate);
				JANUS_LOG(LOG_VERB, "Sending REMB\n");
				gateway->relay_rtcp(session->handle, 1, buf, 24);
				/* FIXME How should we handle a subsequent "no limit" bitrate? */
			}
		}
		if(record) {
			if(msg->sdp) {
				session->has_audio = (strstr(msg->sdp, "m=audio") != NULL);
				session->has_video = (strstr(msg->sdp, "m=video") != NULL);
			}
			gboolean recording = json_is_true(record);
			const char *recording_base = json_string_value(recfile);
			JANUS_LOG(LOG_VERB, "Recording %s (base filename: %s)\n", recording ? "enabled" : "disabled", recording_base ? recording_base : "not provided");
			if(!recording) {
				/* Not recording (anymore?) */
				if(session->arc) {
					janus_recorder_close(session->arc);
					JANUS_LOG(LOG_INFO, "Closed audio recording %s\n", session->arc->filename ? session->arc->filename : "??");
					janus_recorder_free(session->arc);
				}
				session->arc = NULL;
				if(session->vrc) {
					janus_recorder_close(session->vrc);
					JANUS_LOG(LOG_INFO, "Closed video recording %s\n", session->vrc->filename ? session->vrc->filename : "??");
					janus_recorder_free(session->vrc);
				}
				session->vrc = NULL;
			} else {
				/* We've started recording, send a PLI and go on */
				char filename[255];
				gint64 now = janus_get_monotonic_time();
				if(session->has_audio) {
					memset(filename, 0, 255);
					if(recording_base) {
						/* Use the filename and path we have been provided */
						g_snprintf(filename, 255, "%s-audio", recording_base);
						session->arc = janus_recorder_create(NULL, 0, filename);
						if(session->arc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an audio recording file for this EchoTest user!\n");
						}
					} else {
						/* Build a filename */
						g_snprintf(filename, 255, "serial-%p-%"SCNi64"-audio", session, now);
						session->arc = janus_recorder_create(NULL, 0, filename);
						if(session->arc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an audio recording file for this EchoTest user!\n");
						}
					}
				}
				if(session->has_video) {
					memset(filename, 0, 255);
					if(recording_base) {
						/* Use the filename and path we have been provided */
						g_snprintf(filename, 255, "%s-video", recording_base);
						session->vrc = janus_recorder_create(NULL, 1, filename);
						if(session->vrc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an video recording file for this EchoTest user!\n");
						}
					} else {
						/* Build a filename */
						g_snprintf(filename, 255, "serial-%p-%"SCNi64"-video", session, now);
						session->vrc = janus_recorder_create(NULL, 1, filename);
						if(session->vrc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an video recording file for this EchoTest user!\n");
						}
					}
					/* Send a PLI */
					JANUS_LOG(LOG_VERB, "Recording video, sending a PLI to kickstart it\n");
					char buf[12];
					memset(buf, 0, 12);
					janus_rtcp_pli((char *)&buf, 12);
					gateway->relay_rtcp(session->handle, 1, buf, 12);
				}
			}
		}
		/* Any SDP to handle? */
		if(msg->sdp) {
			JANUS_LOG(LOG_VERB, "This is involving a negotiation (%s) as well:\n%s\n", msg->sdp_type, msg->sdp);
			session->has_audio = (strstr(msg->sdp, "m=audio") != NULL);
			session->has_video = (strstr(msg->sdp, "m=video") != NULL);
		}
/*
		if(!audio && !video && !bitrate && !record && !msg->sdp) {
			JANUS_LOG(LOG_ERR, "No supported attributes (audio, video, bitrate, record, jsep) found\n");
			error_code = JANUS_SERIAL_ERROR_INVALID_ELEMENT;
			g_snprintf(error_cause, 512, "Message error: no supported attributes (audio, video, bitrate, record, jsep) found");
			goto error;
		}
*/
		json_decref(root);
		/* Prepare JSON event */
		json_t *event = json_object();
		json_object_set_new(event, "serial", json_string("event"));
		json_object_set_new(event, "result", json_string("ok"));
		char *event_text = json_dumps(event, JSON_INDENT(3) | JSON_PRESERVE_ORDER);
		json_decref(event);
		JANUS_LOG(LOG_VERB, "Pushing event: %s\n", event_text);
		if(!msg->sdp) {
			int ret = gateway->push_event(msg->handle, &janus_serial_plugin, msg->transaction, event_text, NULL, NULL);
			JANUS_LOG(LOG_VERB, "  >> %d (%s)\n", ret, janus_get_api_error(ret));
		} else {
			/* Forward the same offer to the gateway, to start the echo test */
			const char *type = NULL;
			if(!strcasecmp(msg->sdp_type, "offer"))
				type = "answer";
			if(!strcasecmp(msg->sdp_type, "answer"))
				type = "offer";
			/* Any media direction that needs to be fixed? */
			char *sdp = g_strdup(msg->sdp);
			if(strstr(sdp, "a=recvonly")) {
				/* Turn recvonly to inactive, as we simply bounce media back */
				sdp = janus_string_replace(sdp, "a=recvonly", "a=inactive");
			} else if(strstr(sdp, "a=sendonly")) {
				/* Turn sendonly to recvonly */
				sdp = janus_string_replace(sdp, "a=sendonly", "a=recvonly");
				/* FIXME We should also actually not echo this media back, though... */
			}
			/* Make also sure we get rid of ULPfec, red, etc. */
			if(strstr(sdp, "ulpfec")) {
				sdp = janus_string_replace(sdp, "100 116 117 96", "100");
				sdp = janus_string_replace(sdp, "a=rtpmap:116 red/90000\r\n", "");
				sdp = janus_string_replace(sdp, "a=rtpmap:117 ulpfec/90000\r\n", "");
				sdp = janus_string_replace(sdp, "a=rtpmap:96 rtx/90000\r\n", "");
				sdp = janus_string_replace(sdp, "a=fmtp:96 apt=100\r\n", "");
			}
			/* How long will the gateway take to push the event? */
			gint64 start = janus_get_monotonic_time();
			int res = gateway->push_event(msg->handle, &janus_serial_plugin, msg->transaction, event_text, type, sdp);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (took %"SCNu64" us)\n",
				res, janus_get_monotonic_time()-start);
			g_free(sdp);
		}
		g_free(event_text);
		janus_serial_message_free(msg);
		continue;
		
error:
		{
			if(root != NULL)
				json_decref(root);
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
