/*
 * QuickJS stand alone interpreter
 * 
 * Copyright (c) 2017-2020 Fabrice Bellard
 * Copyright (c) 2017-2020 Charlie Gordon
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

#include "cutils.h"
#include "quickjs-libc.h"

/* enable bignums */
#define CONFIG_BIGNUM

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;

static BOOL byte_swap = FALSE;


static void dump_hex(FILE *f, const uint8_t *buf, size_t len)
{
    size_t i;
    for(i = 0; i < len; i++) {
        fprintf(f, " 0x%02x,", (uint8_t) buf[i]);
    }
}

static void output_object_code(JSContext *ctx,
                               const char *bytefilename,
                                JSValueConst obj)
{
    int flags;
    flags = JS_WRITE_OBJ_BYTECODE;
    if (byte_swap) {
        flags |= JS_WRITE_OBJ_BSWAP;
    }
    
    size_t out_buf_len;
    uint8_t * out_buf = JS_WriteObject(ctx, &out_buf_len, obj, flags);
    if (!out_buf) {
        js_std_dump_error(ctx);
        exit(1);
    }
    
    FILE *fo = fopen(bytefilename, "wb");  // w for write, b for binary
    if (!fo) {
        fprintf(stderr, "Failed open file '%s' for writing \n", bytefilename);
        exit(1);
    }

    // first line: length in bytes, needed?
    fprintf(fo, "%u\n", (unsigned int) out_buf_len);

    // followed by the binary data
    fwrite(out_buf, out_buf_len, 1, fo);

    // dump_hex(fo, out_buf, out_buf_len); 

    fclose(fo);
    printf("Wrote %ldbytes to file '%s'\n", out_buf_len, bytefilename);
    js_free(ctx, out_buf);
}


JSValue read_js_from_cache(JSContext *ctx, const char *bytefilename) {
    if( access( bytefilename, F_OK ) == -1 ) {
        printf("WARN: Cache file '%s' npn-existant. \n", bytefilename);
        return JS_UNDEFINED;
    }

    FILE *fi = fopen(bytefilename, "rb");
    if (!fi) {
        fprintf(stderr, "Failed open file '%s' for reading \n", bytefilename);
        exit(1);
    }

    uint8_t *in_buf;
    unsigned int in_buf_len;
    fscanf(fi, "%u\n", &in_buf_len);

    in_buf = malloc(in_buf_len+1);
    fread(in_buf, in_buf_len, 1, fi);

    printf("Read %ubytes of bytecode from file '%s'\n", in_buf_len,  bytefilename);

    JSValue jsCode = JS_ReadObject(ctx, in_buf, in_buf_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(jsCode)) {
        fprintf(stderr, "Failed reading JS of read file '%s' into QuickJS.\n", bytefilename);
        js_std_dump_error(ctx);
        exit(1);
    }

    return jsCode;
}


JSValue read_js_compile_and_cache_file(JSContext *ctx, 
                         const char *jsfilename,
                         const char *bytefilename)                        
{
    uint8_t *buf;
    size_t buf_len;
    
    /* load JS from file to buffer */
    buf = js_load_file(ctx, &buf_len, jsfilename);
    if (!buf) {
        fprintf(stderr, "Could not load '%s'\n", jsfilename);
        exit(1);
    }
    printf("Loaded file '%s' \n", jsfilename);

    /* since we want to ave the byte code, we only compile but don't run the code. */
    int eval_flags = JS_EVAL_FLAG_COMPILE_ONLY;

    /* we don't consider modules here */
    eval_flags |= JS_EVAL_TYPE_GLOBAL;

    /* compile the JS */
    JSValue jsCode = JS_Eval(ctx, (const char *)buf, buf_len, jsfilename, eval_flags);
    if (JS_IsException(jsCode)) {
        fprintf(stderr, "Failed compiling JS of file  '%s'\n", jsfilename);
        js_std_dump_error(ctx);
        exit(1);
    }
    
    /* put the bytecode to cachefile */
    output_object_code(ctx, bytefilename, jsCode);
    
    return jsCode;
}


int main(int argc, char **argv)
{
    JSRuntime *rt = JS_NewRuntime();
    if (!rt)
    {
        fprintf(stderr, "qjs: cannot allocate JS runtime\n");
        exit(2);
    }

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx)
    {
        fprintf(stderr, "qjs: cannot allocate JS context\n");
        exit(2);
    }

    js_std_add_helpers(ctx, argc - optind, argv + optind);

    // read the (textual JS) from file into QuickJS bytecode, dump bytecode to cache file
    JSValue origJsCode = read_js_compile_and_cache_file(ctx, argv[1], argv[2]);
    JS_FreeValue(ctx, origJsCode);

    // read back the cache file into QuickJS bytecode
    JSValue cachedJsCode = read_js_from_cache(ctx, argv[2]);

    // execute the JS
    printf("Executing JS ....\n");
    JSValue result = JS_EvalFunction(ctx, cachedJsCode);
    if (JS_IsException(result)) {
        fprintf(stderr, "Failed executing cached JS");
        js_std_dump_error(ctx);
    }
    
    js_std_loop(ctx);
    printf("DONE executing JS.\n");

    JS_FreeValue(ctx, result);
    
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}
