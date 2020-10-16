#include <gst/gst.h>

GstElement *pipeline;
GstElement *appsrc;
GstElement *encoder;
GstElement *parser;
GstElement *muxer;
GstElement *filesink;

GMainLoop *loop;
guint timer;

int width = 640, height = 480;
int framerate = 30;

static void get_frame(guint8 *data, guint width, guint height)
{
    const guint colors[7] = {0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff};
    gint a;

    for(guint i = 0; i < height; ++i) {
        for(guint j = 0; j < width; ++j) {
            data[i*width + j] = 128;
        }
    }
    data += (width*height);

    for(guint i = 0; i < height/4; ++i) {
        for(guint j = 0; j < width; ++j) {
            data[i*width + j] = 128;
        }
    }
    data += (width*height/4);


    for(guint i = 0; i < height/4; ++i) {
        for(guint j = 0; j < width; ++j) {
            data[i*width + j] = 128;
        }
    }

}

static gboolean push_data(gpointer *data)
{
    GstBuffer *buffer;
    GstMapInfo map;
    GstFlowReturn ret;
    static int frame_num;
    static int current_time;
    int i;
    guint8 *raw;

    ++current_time;
    if(current_time > 300) {
        g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
        //gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
        return TRUE;
    }

    /* Create a new empty buffer */
    buffer = gst_buffer_new_and_alloc(width*height*3);

    /* Set its timestamp and duration */
    ++frame_num;
    GST_BUFFER_TIMESTAMP(buffer) = gst_util_uint64_scale (frame_num, GST_SECOND, framerate);
    GST_BUFFER_DURATION(buffer)  = gst_util_uint64_scale (1, GST_SECOND, framerate);

    /* Generate some psychodelic waveforms */
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    raw = (guint8 *)map.data;
    get_frame(raw, 640, 480);
    gst_buffer_unmap (buffer, &map);

    /* Push the buffer into the appsrc */
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    //gst_app_src_push_buffer();

    /* Free the buffer now that we are done with it */
    gst_buffer_unref(buffer);

    if (ret != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        return FALSE;
    }

    return TRUE;
}

static void start_feed(GstElement *source, guint size, gpointer data) 
{
    if(timer == 0) {
        g_print ("Start feeding\n");
        timer = g_idle_add((GSourceFunc)push_data, NULL);
    }
}

static void stop_feed(GstElement *source, gpointer data)
{
    if (timer != 0) {
        g_print ("Stop feeding\n");
        g_source_remove(timer);
        timer = 0;
    }
}

static void message_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug_info;

        gst_message_parse_error(msg, &err, &debug_info);
        g_print("Error: %s\n", err->message);
        g_error_free(err);
        g_free(debug_info);

        gst_element_set_state(pipeline, GST_STATE_READY);
        g_main_loop_quit(loop);
        break;
    }
    case GST_MESSAGE_EOS: {
        /* end-of-stream */
        gst_element_set_state(pipeline, GST_STATE_READY);
        g_main_loop_quit(loop);
        break;
    }
    default:
        /* Unhandled message */
        break;
    }
}

int init()
{
    GstCaps *caps;
    GstBus *bus;

    gst_init(NULL, NULL);

    pipeline 	= gst_pipeline_new("test_pipeline");
    appsrc 		= gst_element_factory_make("appsrc", "src");
    encoder 	= gst_element_factory_make("x264enc", "encoder");
    parser 		= gst_element_factory_make("h264parse", "parser");
    muxer 		= gst_element_factory_make("mp4mux", "muxer");
    filesink 	= gst_element_factory_make("filesink", "sink");

    if(!pipeline || !appsrc || !encoder ||\
        !parser  || !muxer  || !filesink)
    {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    caps = gst_caps_new_simple("video/x-raw", \
        "format", G_TYPE_STRING, "I420", \
        "width", G_TYPE_INT, width, \
        "height", G_TYPE_INT, height, \
        "framerate", GST_TYPE_FRACTION, framerate, 1, \
        NULL);
    
    g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(appsrc, "need-data", G_CALLBACK(start_feed), NULL);
    g_signal_connect(appsrc, "enough-data", G_CALLBACK(stop_feed), NULL);

    g_object_set(filesink, "location", "test.mp4", NULL);

    gst_bin_add_many(GST_BIN(pipeline), appsrc, encoder, parser, muxer, filesink, NULL);
    if(gst_element_link_many(appsrc, encoder, parser, muxer, filesink, NULL) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        gst_caps_unref(caps);
        return -1;
    }

    gst_caps_unref(caps);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", (GCallback)message_cb, NULL);
    gst_object_unref (bus);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    return 0;
}

void deinit()
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

int main(int argc, char const *argv[])
{

    loop = g_main_loop_new(NULL, FALSE);

    if(init()) {
        g_main_loop_unref(loop);
        return 0;
    }

    g_main_loop_run(loop);

    deinit();

    return 0;
}
