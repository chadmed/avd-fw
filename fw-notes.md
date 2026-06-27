# FW interface planning
Full disclosure I have absolutely no idea what I'm doing.

## Current state
The current firmware running on the CM3 is essentially just a dumb shim to shuttle IRQs between
the application cores and the decoders. The V4L2 driver programs the decoders directly and treats
AVD as stateless. We keep track of which variant has which decoders, but only so that the instruction
stream can be programmed correctly. The device is exposed to V4L2 as a single decoder behind a single
media controller and so V4L2 will queue and schedule frame decode requests in series. While this
works, it is not an effective use of the hardware.

## Problem scope
- V4L2 Request has very little support in upstream projects like ffmpeg
- AVD has only two interrupt lines to the application cores to signal when there is stuff in its
  mailboxes
- The kernel is not the right place for a state machine to keep track of each FFU and schedule jobs
- Vulkan Video has already sapped most resources out of V4L2 and VA-API development, and we should support it

## What we need
The CM3 is there for a reason. The firmware should expose a command interface that lets us feed in
some IOVAs and command data (job ID, etc.) and abstract away things like per-variant addressing mode
quirks itself.

### FW requirements
- Ring buffer to store commands from the kernel
- FFU state tracker
- Scheduler to dispatch decode jobs to available FFUs

The kernel should be able to submit something like:
```c
create_stream({
    .codec = AVD_CODEC_AVC,
    .profile = AVD_AVC_PROFILE_MAIN,
    .level = AVD_AVC_LEVEL_42,
    .pixfmt = AVD_PIXFMT_NV12,
    .width = 1920,
    .height = 1080,
    .initdata = iova_of_codec_init_data, /* SPS/Seq Hdr OBU/whatever */
    .initdata_sz = sizeof(*iova_of_codec_init_data),
});
```
to create a decode session. The firmware should then set up a stream with the correct metadata to
program the decoder for each frame, then return a stream ID to the kernel.

We would then enqueue frames for that stream when required:
```c
submit_job{{
    .stream_id = 1234,
    .frame_id = 5678,
    .src = iova_of_src_slice,
    .src_sz = sizeof(*iova_of_src_slice),
    .dst = iova_of_dst_buf,
});
```

The firmware should have basic priority scheduling. If a decoder is idle but has not been reprogrammed for
a different stream, schedule the queued frame on it to save a few microseconds. Only reprogram the decoder
if it is needed for a different stream.

## Brainfart
This is a sketch and almost certainly wouldn't even compile. Values are placeholders/bogus and just for
illustrative purposes to validate ideas.

```c
#define SRAM_BASE 0x10000000
#define CMDBUF_ADDR SRAM_BASE + 0x100

#define NUM_H264_SLOTS 4
#define MAX_STREAMS 4 /* Do we want to allow one in-flight stream per decoder? */
#define MAX_CMDS 512


/* Use the top 16 bits of the mailbox for status flags, bottom 16 bits for values */
#define AVD_STREAM_CREATED 1 << 16
#define AVD_STREAM_CREATE_FAILED 1 << 17
#define AVD_CMD_INVALID 1 << 18
#define AVD_INVALID_CODEC 1

enum avd_cmd_type {
    AVD_CMD_STREAM_CREATE, /* Create a stream to prepare for decoding */
    AVD_CMD_STREAM_DESTROY, /* Destroy a stream, we're done decoding */
    AVD_CMD_FRAME_QUEUE, /* Queue up a frame for a given stream */
    AVD_CMD_FLUSH, /* Flush all FW state (on reset?) */
    AVD_CMD_NOP,
};

enum avd_codec {
    AVD_CODEC_H264,
    AVD_CODEC_HEVC,
    AVD_CODEC_VP9,
    AVD_CODEC_AV1,
};

enum avd_pixfmt {
    AVD_PIXFMT_NV12,
    AVD_PIXFMT_NV16,
    AVD_PIXFMT_P010,
    AVD_PIXFMT_P210,
};

enum avd_stream_status {
    AVD_STREAM_FREE,
    AVD_STREAM_ASSIGNED,
};

struct avd_stream_create {
    u8 codec;
    u8 profile;
    u8 level;
    u8 pix_fmt;
    u8 width;
    u8 height;
    u32 initdata; /* We can't do 64-bit IOVAs from the CM3 right? */
    u8 initdata_sz;
} __packed;

struct avd_stream {
    u8 status;
    struct avd_stream_create stream;
};

struct avd_stream streams[MAX_STREAMS];

struct avd_stream_destroy {
    u8 stream_id;
} __packed;

struct avd_frame_queue {
    u8 stream_id;
    u8 frame_id; /* Do we need this */
    u32 src;
    u8 src_sz;
    u32 dst;
} __packed;

/* Kernel interface */
struct avd_cmd {
    u8 cmd_type;
    u8 cmd_sz;
    union {
        struct avd_stream;
        struct avd_stream_destroy;
        struct avd_frame_queue;
    } cmd_data;
};

struct avd_cmd *cmdbuf[MAX_CMDS] = CMDBUF_ADDR;

enum avd_decoder_status {
    AVD_DECODER_STATUS_IDLE,
    AVD_DECODER_STATUS_BUSY,
    AVD_DECODER_STATUS_ERR,
    AVD_DECODER_STATUS_UNK,
};

struct decoder_state {
    u8 stream_id;
    u8 status;
};

static struct decoder_state h264_state[NUM_H264_SLOTS];

static int create_stream_h264(struct avd_stream_create *s, struct avd_stream *stream)
{
    /* validate the H264 SPS and other garbage here, and return with an error
     * code if we fail
     */
     
     stream->status = AVD_STREAM_ASSIGNED;
     
     return 0;
}

static void create_stream(struct avd_stream_create *s)
{
    int i, sid, ret = 0;
    
    /* Get the first free stream */
    for (i = 0; i < MAX_STREAMS; i++)
        if (streams[i]->status == AVD_STREAM_FREE)
            sid = i
            break;
    
    if (!sid)
        write_mbox(AVD_STREAM_CREATE_FAILED | 0xffff); /* No free streams */
        
    switch (s->codec):
    case AVD_CODEC_H264:
        ret = create_stream_h264(s, &streams[sid]);
        break;
    default
        write_mbox(AVD_STREAM_CREATE_FAILED | AVD_INVALID_CODEC);
        return;
        
    if (ret)
        write_mbox(AVD_STREAM_CREATE_FAILED | ret);
    else
        write_mbox(AVD_STREAM_CREATED | sid);

    return;
}

/* The kernel hit the mailbox */
static void handle_cmd(struct avd_cmd *cmd)
{
    switch (cmd->cmd_type):
    case AVD_CMD_STREAM_CREATE:
        create_stream(&(struct avd_stream_create)cmd->cmd_data);
        break;
    case AVD_CMD_FRAME_QUEUE:
        decode_frame(&(struct avd_frame_queue)cmd->cmd_data);
    default:
        write_mbox(AVD_CMD_INVALID | 0);
        break;
    
    return;
}

static void dingdong()
{
    u32 mbox = reg_read(CM3_MBOX1_RX);
    
    if (mbox & BIT(AVD_CMD_RECV))
        handle_cmd(...)
}
```


