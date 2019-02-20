/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * video encoding with libavcodec API example
 *
 * @example encode_video.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
}


static int encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                  FILE *outfile) {
    int ret;

    /* send the frame to the encoder */
    if ((ret = avcodec_send_frame(enc_ctx, frame)) < 0) {
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);

        if (ret < 0) {
            return ret;
        }
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
    return 0;
}

// 编码生成一个1秒的mp4视频
char *encode_video(char **argv) {
    const char *filename;
    const AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y;
    FILE *f;
    AVFrame *frame;
    AVPacket *pkt;
    uint8_t endcode[] = {0, 0, 1, 0xb7};

    filename = argv[0];

    avcodec_register_all();

    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder_by_name("mpeg4");
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        ret = -1111;
        goto end;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        ret = -1112;
        goto end;
    }

    //压缩帧
    pkt = av_packet_alloc();
    if (!pkt) {
        ret = -1113;
        goto end;
    }

    /* put sample parameters */
    c->bit_rate = 400000;   // 比特率 = 采样率 * 量化格式
    /* resolution must be a multiple of two */
    c->width = 352;
    c->height = 288;
    /* frames per second */
    c->time_base = (AVRational) {1, 25};  //时间单位
    c->framerate = (AVRational) {25, 1};  // 帧率

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;  // 帧组
    c->max_b_frames = 1;   // B帧上限
    c->pix_fmt = AV_PIX_FMT_YUV420P;  // 像素格式

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        ret = -1114;
        goto end;
    }

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        ret = -1115;
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        ret = -1116;
        goto end;
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    if ((ret = av_frame_get_buffer(frame, 32)) < 0) {
        goto end;
    }

    // 编码一个 352 * 288的视频
    /* encode 1 second of video ， 25 帧 */
    for (i = 0; i < 25; i++) {
        fflush(stdout);

        /* make sure the frame data is writable */
        if ((ret = av_frame_make_writable(frame)) < 0) {
            goto end;
        }

        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < c->height; y++) {
            for (x = 0; x < c->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        /* Cb and Cr */
        for (y = 0; y < c->height / 2; y++) {
            for (x = 0; x < c->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        /* encode the image */
        encode(c, frame, pkt, f);

    }

    /* flush the encoder */
    encode(c, NULL, pkt, f);

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    end:
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    if (ret < 0) {
        char buf2[500] = {0};
        if (ret == -1111) {
            return (char *) "codec not found";
        } else if (ret == -1112) {
            return (char *) "Could not allocate video codec context";
        } else if (ret == -1113) {
            return (char *) "could not allocate packet";
        } else if (ret == -1114) {
            return (char *) "Could not open codec";
        } else if (ret == -1115) {
            return (char *) "Could not open input file";
        } else if (ret == -1116) {
            return (char *) "Could not allocate video frame";
        }
        av_strerror(ret, buf2, 1024);
        return buf2;
    } else {
        return (char *) "编码成功";
    }
}
