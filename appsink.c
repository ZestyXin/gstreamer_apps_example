#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

// videotestsrc --- videoconvert --- appsink

GMainLoop *loop;

GstElement *pipeline;
GstElement *src;
GstElement *convert;
GstElement *appsink;

int width = 640, height = 480;
int framerate = 30;

FILE *file;

/* The appsink has received a buffer */
static GstFlowReturn new_sample(GstElement *sink, gpointer data)
{
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;
    guint8 *raw;
    GstFlowReturn ret;
    static int frame_num;

    if(frame_num++ > 5) {
        GstMessage *msg;
        msg = gst_message_new_eos(GST_OBJECT(appsink));
        gst_element_post_message(pipeline, msg);
        //gst_message_unref(msg);
        //g_signal_emit_by_name(appsink, "eos", &ret);
        return TRUE;
    }

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample)
    {
        buffer = gst_sample_get_buffer(sample);
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        raw = (guint8*)map.data;

        //printf("size: %d\n", map.size);
        fwrite(raw, 1, map.size, file);
        
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    return GST_FLOW_ERROR;
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
    GstBus *bus;
    GstCaps *caps;

    gst_init(NULL, NULL);

    pipeline 	= gst_pipeline_new("test_pipeline");
    src 		= gst_element_factory_make("videotestsrc", "src");
    convert 	= gst_element_factory_make("videoconvert", "convert");
    appsink 	= gst_element_factory_make("appsink", "sink");

    if(!pipeline || !src || !convert || !appsink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    g_object_set(src, "pattern", 19, NULL);

    caps = gst_caps_new_simple("video/x-raw",\
        "format", G_TYPE_STRING, "I420",\
        "width", G_TYPE_INT, width, \
        "height", G_TYPE_INT, height, \
        "framerate", GST_TYPE_FRACTION, framerate, 1);
    
    g_object_set(appsink, "emit-signals", TRUE, "caps", caps, NULL);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(new_sample), NULL);
    
    gst_bin_add_many(GST_BIN(pipeline), src, convert, appsink, NULL);
    if(gst_element_link_filtered(src, convert, caps) != TRUE || \
        gst_element_link(convert, appsink) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", (GCallback)message_cb, NULL);
    gst_object_unref(bus);

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
    file = fopen("test.yuv", "wb");
    if(file == NULL) {
        printf("file open failed \n");
        return 0;
    }
    
    loop = g_main_loop_new(NULL, FALSE);

    if(init()) {
        g_main_loop_unref(loop);
        fclose(file);
        return 0;
    }

    g_main_loop_run(loop);

    deinit();

    fclose(file);

    return 0;
}
