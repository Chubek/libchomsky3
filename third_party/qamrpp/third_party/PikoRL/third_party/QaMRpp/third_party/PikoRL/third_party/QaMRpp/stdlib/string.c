#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static const char* sarg(qamrpp_value** argv, size_t argc, size_t i, size_t* len) {
    if (i >= argc) { if (len) *len = 0; return ""; }
    const char* s = g_api->value_as_string(argv[i], len);
    if (!s) { if (len) *len = 0; return ""; }
    return s;
}

static qamrpp_value* string_len(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n = 0; (void)sarg(argv, argc, 0, &n); return g_api->value_int(ctx, (int64_t)n); }
static qamrpp_value* string_lower(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n=0; const char* s=sarg(argv,argc,0,&n); char* b=(char*)malloc(n+1); for(size_t i=0;i<n;++i)b[i]=(char)tolower((unsigned char)s[i]); b[n]=0; qamrpp_value* v=g_api->value_string(ctx,b,n); free(b); return v; }
static qamrpp_value* string_upper(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n=0; const char* s=sarg(argv,argc,0,&n); char* b=(char*)malloc(n+1); for(size_t i=0;i<n;++i)b[i]=(char)toupper((unsigned char)s[i]); b[n]=0; qamrpp_value* v=g_api->value_string(ctx,b,n); free(b); return v; }
static qamrpp_value* string_reverse(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n=0; const char* s=sarg(argv,argc,0,&n); char* b=(char*)malloc(n+1); for(size_t i=0;i<n;++i)b[i]=s[n-1-i]; b[n]=0; qamrpp_value* v=g_api->value_string(ctx,b,n); free(b); return v; }
static qamrpp_value* string_sub(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t n=0; const char* s=sarg(argv,argc,0,&n); long a=(long)g_api->value_as_int(argc>1?argv[1]:0); long b=(long)g_api->value_as_int(argc>2?argv[2]:0);
    if (a<=0) a=1; if (b<=0 || b>(long)n) b=(long)n; if (a>b) return g_api->value_string(ctx,"",0);
    size_t start=(size_t)(a-1), len=(size_t)(b-a+1); return g_api->value_string(ctx,s+start,len);
}
static qamrpp_value* string_rep(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t n=0; const char* s=sarg(argv,argc,0,&n); int times=(int)g_api->value_as_int(argc>1?argv[1]:0); if(times<=0)return g_api->value_string(ctx,"",0);
    size_t outn=n*(size_t)times; char* b=(char*)malloc(outn+1); size_t p=0; for(int i=0;i<times;++i){memcpy(b+p,s,n);p+=n;} b[outn]=0;
    qamrpp_value* v=g_api->value_string(ctx,b,outn); free(b); return v;
}
static qamrpp_value* string_find(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t sn=0,pn=0; const char* s=sarg(argv,argc,0,&sn); const char* p=sarg(argv,argc,1,&pn); const char* f=strstr(s,p); if(!f) return g_api->value_nil(ctx); return g_api->value_int(ctx,(int64_t)(f-s+1));
}
static qamrpp_value* string_match(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t sn=0,pn=0; const char* s=sarg(argv,argc,0,&sn); const char* p=sarg(argv,argc,1,&pn); const char* f=strstr(s,p); if(!f) return g_api->value_nil(ctx); return g_api->value_string(ctx,f,pn);
}
static qamrpp_value* string_gsub(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t sn=0,pn=0,rn=0; const char* s=sarg(argv,argc,0,&sn); const char* p=sarg(argv,argc,1,&pn); const char* r=sarg(argv,argc,2,&rn);
    if(pn==0) return g_api->value_string(ctx,s,sn);
    const char* at=strstr(s,p); if(!at) return g_api->value_string(ctx,s,sn);
    size_t left=(size_t)(at-s), right=sn-left-pn, outn=left+rn+right;
    char* b=(char*)malloc(outn+1); memcpy(b,s,left); memcpy(b+left,r,rn); memcpy(b+left+rn,at+pn,right); b[outn]=0;
    qamrpp_value* v=g_api->value_string(ctx,b,outn); free(b); return v;
}
static qamrpp_value* string_byte(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n=0; const char* s=sarg(argv,argc,0,&n); if(!n) return g_api->value_nil(ctx); return g_api->value_int(ctx,(unsigned char)s[0]); }
static qamrpp_value* string_char(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { char* b=(char*)malloc(argc+1); for(size_t i=0;i<argc;++i)b[i]=(char)g_api->value_as_int(argv[i]); b[argc]=0; qamrpp_value* v=g_api->value_string(ctx,b,argc); free(b); return v; }
static qamrpp_value* string_format(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { size_t n=0; const char* fmt=sarg(argv,argc,0,&n); (void)fmt; if(argc<2) return g_api->value_string(ctx,"",0); return g_api->value_string(ctx,sarg(argv,argc,1,&n),n); }
static qamrpp_value* passthrough_nil(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"string_byte", string_byte}, {"string_char", string_char}, {"string_dump", passthrough_nil}, {"string_find", string_find},
    {"string_format", string_format}, {"string_gmatch", passthrough_nil}, {"string_gsub", string_gsub}, {"string_len", string_len},
    {"string_lower", string_lower}, {"string_match", string_match}, {"string_pack", passthrough_nil}, {"string_packsize", passthrough_nil},
    {"string_rep", string_rep}, {"string_reverse", string_reverse}, {"string_sub", string_sub}, {"string_unpack", passthrough_nil}, {"string_upper", string_upper}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) { (void)ctx; g_api = host_api; return 0; }

static const qamrpp_library_descriptor kDescriptor = { QAMRPP_LIBRARY_API_VERSION, "string", kBindings, sizeof(kBindings)/sizeof(kBindings[0]), on_load, 0 };
QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
