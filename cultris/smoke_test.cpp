#include "c2_ai_api.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <thread>
using Create=int64_t(*)(); using Destroy=void(*)(int64_t); using Step=void(*)(int64_t,const C2StepInput*,C2StepOutput*); using Name=const char*(*)();
bool valid_identity(const char*name){const char*p=std::strrchr(name,'/');if(!p||p[1]!=' ')return false;p+=2;if(std::strcmp(p,"source-package")==0)return true;std::size_t n=std::strlen(p);if(n>6&&std::strcmp(p+n-6,"-dirty")==0)n-=6;if(n<7||n>40)return false;for(std::size_t i=0;i<n;++i)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return false;return true;}
int main(int argc,char**argv){const char* p=argc>1?argv[1]:"../libs/libcultrisai.dylib";void*l=dlopen(p,RTLD_NOW);if(!l){std::fprintf(stderr,"%s\n",dlerror());return 1;}auto c=(Create)dlsym(l,"c2ai_create");auto d=(Destroy)dlsym(l,"c2ai_destroy");auto s=(Step)dlsym(l,"c2ai_step");auto n=(Name)dlsym(l,"c2ai_name");auto v=(Name)dlsym(l,"c2ai_version");if(!c||!d||!s||!n||!v)return 2;const char*name=n();if(!name||!*name||!std::strstr(name,"C2 ZZZ")||!std::strstr(name,"combo-table-16")||!valid_identity(name))return 4;for(const unsigned char*q=(const unsigned char*)name;*q;++q)if(*q>127)return 5;auto h=c();C2StepInput in{};in.api_version=C2_AI_API_VERSION;in.cur_kind=C2_KIND_T;in.cur_x=3;in.cur_y=8;in.next_kind=C2_KIND_I;in.attack_curve_scale=1.95f;in.piece_serial=1;in.board_gen=1;C2StepOutput out{};out.status=C2_STATUS_SEARCHING;for(int i=0;i<200&&out.status!=C2_STATUS_OK;++i){s(h,&in,&out);std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::printf("status=%d op=%d name=%s version=%s\n",out.status,out.next_op,name,v());d(h);dlclose(l);return out.status==C2_STATUS_OK?0:3;}
