# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			main.c \
			monitor.c \
			worker.c \

c_FILE2		=	\
			list.c \
			rbtree.c \
			fasterjson.c \
			LOGC.c \
			config.c \
			env.c \
			util.c \
			output.c \

###### 目标文件、安装目录配置区
include makeinstall
BIN		=	logpipe
BININST		=	$(_BININST)
LIB		=	liblogpipe_api.so
LIBINST		=	$(_LIBINST)
HDER		= \
			rbtree.h \
			LOGC.h \
			fasterjson.h \
			rbtree_tpl.h \
			logpipe_api.h \

HDERINST	=	$(_HDERINST)

###### 编译选项
CFLAGS		=	$(_CFLAGS) \
			-std=gnu99 \

###### 链接选项
LFLAGS		=	$(_LFLAGS) \
			-rdynamic \
			-ldl \
			-lz \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_BININST
#@ make_install_LIBINST
#@ make_install_HDERINST
#@ make_uninstall
#@ make_uninstall_BININST
#@ make_uninstall_LIBINST
#@ make_uninstall_HDERINST
include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
logpipe		:	$(c_FILE_o) liblogpipe_api.so
	$(CC) -o $@ $(c_FILE_o) $(LFLAGS) -llogpipe_api

liblogpipe_api.so	:	$(c_FILE2_o)
	$(CC) -o $@ $(c_FILE2_o) $(SOFLAGS) $(LFLAGS)

