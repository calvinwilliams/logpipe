# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			press_logpipe.c \
			test_logpipe.c \
			press_LOGC.c \

###### 目标文件、安装目录配置区
include makeinstall
BIN		=	press_logpipe test_logpipe press_LOGC
BININST		=	$(_BININST)

###### 编译选项
CFLAGS		=	$(_CFLAGS) \
			-std=gnu99 \
			-I$(HOME)/include/logpipe \

###### 链接选项
LFLAGS		=	$(_LFLAGS) \
			-llogpipe_api \

###### 额外宏定义区
CLEAN_ADDITION	=

PREMAKE		=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_BININST
#@ make_install_DFTHDERINST
#@ make_uninstall
#@ make_uninstall_BININST
#@ make_uninstall_DFTHDERINST
include $(MKTPL2_HOME)/makeobj_$(MKTPL2_OS).inc

###### 目标文件依赖关系
press_logpipe	:	press_logpipe.o
	$(CC) -o $@ press_logpipe.o $(LFLAGS)

test_logpipe	:	test_logpipe.o
	$(CC) -o $@ test_logpipe.o $(LFLAGS)

press_LOGC	:	press_LOGC.o
	$(CC) -o $@ press_LOGC.o $(LFLAGS)

