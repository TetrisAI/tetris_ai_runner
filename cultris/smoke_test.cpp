#include "c2_ai_api.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <thread>
using Create=int64_t(*)(); using Destroy=void(*)(int64_t); using Step=void(*)(int64_t,const C2StepInput*,C2StepOutput*);
int main(int argc,char**argv){const char* p=argc>1?argv[1]:"../libs/libcultrisai.dylib";void*l=dlopen(p,RTLD_NOW);if(!l){std::fprintf(stderr,"%s\n",dlerror());return 1;}auto c=(Create)dlsym(l,"c2ai_create");auto d=(Destroy)dlsym(l,"c2ai_destroy");auto s=(Step)dlsym(l,"c2ai_step");if(!c||!d||!s)return 2;auto h=c();C2StepInput in{};in.api_version=C2_AI_API_VERSION;in.cur_kind=C2_KIND_T;in.cur_x=3;in.cur_y=8;in.next_kind=C2_KIND_I;in.attack_curve_scale=1.95f;in.piece_serial=1;in.board_gen=1;C2StepOutput out{};out.status=C2_STATUS_SEARCHING;for(int i=0;i<200&&out.status!=C2_STATUS_OK;++i){s(h,&in,&out);std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::printf("status=%d op=%d version=%s\n",out.status,out.next_op,((const char*(*)())dlsym(l,"c2ai_version"))());d(h);dlclose(l);return out.status==C2_STATUS_OK?0:3;}
