#include "c2_ai_api.h"
#include <chrono>
#include <cstdio>
#include <dlfcn.h>
#include <thread>
using Create=int64_t(*)();using Destroy=void(*)(int64_t);using Step=void(*)(int64_t,const C2StepInput*,C2StepOutput*);
int main(int argc,char**argv){const char*p=argc>1?argv[1]:"../libs/libcultrisai.dylib";void*l=dlopen(p,RTLD_NOW);if(!l)return 1;auto c=(Create)dlsym(l,"c2ai_create");auto d=(Destroy)dlsym(l,"c2ai_destroy");auto s=(Step)dlsym(l,"c2ai_step");int fail=0;for(int kind=0;kind<7;++kind){auto h=c();C2StepInput in{};in.api_version=C2_AI_API_VERSION;in.cur_kind=kind;in.cur_x=kind==C2_KIND_O?4:3;in.cur_y=kind==C2_KIND_I?7:8;in.next_kind=C2_KIND_I;in.piece_serial=kind+1;in.board_gen=1;C2StepOutput out{};out.status=C2_STATUS_SEARCHING;for(int i=0;i<200&&out.status!=C2_STATUS_OK;++i){s(h,&in,&out);std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::printf("kind=%d status=%d op=%d\n",kind,out.status,out.next_op);fail+=out.status!=C2_STATUS_OK;d(h);}dlclose(l);return fail?2:0;}
