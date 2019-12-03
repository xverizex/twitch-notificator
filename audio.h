#include <glib-2.0/glib.h>
#include <gst/gst.h>
struct play_notification {
	GstElement *pipeline;
	GstElement *source;
	GstElement *volume;
	GstElement *demuxer;
	GstElement *decoder;
	GstElement *conv;
	GstElement *sink;
	gint64 duration;
};
