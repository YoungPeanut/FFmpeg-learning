/*
 * Copyright (c) 2014 Stefano Sabatini
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
 * libavformat AVIOContext API example.
 *
 * Make libavformat demuxer access media content through a custom
 * AVIOContext read callback.
 * @example avio_reading.c
 */
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/file.h"
#include "libavutil/log.h"
}

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    if (!buf_size)
        return AVERROR_EOF;
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

// 读取多媒体文件数据流
char *av_io_reading(char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;  // unsigned char 两个指针
    size_t buffer_size, avio_ctx_buffer_size = 4096;  // 4096 4Kb ，上面指针指向的内存大小
    char *input_filename = NULL;
    int ret = 0;   //结果值
    struct buffer_data bd = { 0 };  //自定义的  处理内存文件数据的结构体

    input_filename = argv[0];

    // ffmpeg先注册
    av_register_all();

    // 将 input_filename 指向的文件数据读取出来，然后用 buffer 指针指向他，buffer_size 中存有 buffer 内存的大小
    if ((ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL)) < 0) {
        goto end;
    }
    bd.ptr  = buffer;
    bd.size = buffer_size;

    // 初始化一个AVFormatContext
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // 申请4Kb缓冲区，在后面作为内存对齐的标准使用
    if (!(avio_ctx_buffer = (uint8_t *) av_malloc(avio_ctx_buffer_size))) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // 初始化一个 AVIOContext,它对avio_ctx_buffer缓冲区进行生成/消费操作，
    // &read_packet  函数指针用于填充缓冲区，生成
    if (!(avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, &bd, &read_packet, NULL, NULL))) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;

    // 开始读取 fmt_ctx->pb 流
    if ((ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL)) < 0) {
        goto end;
    }

    // 解析流信息
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        goto end;
    }

    // 将流数据 dump 到文件中
    // 0 流的索引     0 标示这个AVFormatContext是读取源
    av_dump_format(fmt_ctx, 0, input_filename, 0);

    end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    av_file_unmap(buffer, buffer_size);


    if (ret < 0) { //出错
        char buf2[500] = {0};
        av_strerror(ret, buf2, 1024);
        return buf2;
    } else {
        return (char *) "解封装成功，请查看log";
    }
}
