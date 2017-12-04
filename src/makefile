# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			IDL_logpipe_conf.dsc.c \
			fasterjson.c \
			list.c \
			rbtree.c \
			rbtree_ins.c \
			LOGC.c \
			util.c \
			main.c \
			config.c \
			env.c \
			monitor.c \
			worker.c \
			comm.c \
			file.c \
			output.c \

###### 目标文件、安装目录配置区
include makeinstall
BIN		=	logpipe
BININST		=	$(_BININST)
HDER		= \
			LOGC.h \
			logpipe_in.h \

###### 编译选项
CFLAGS		=	$(_CFLAGS) \
			-std=gnu99 \

###### 链接选项
LFLAGS		=	$(_LFLAGS) \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_BININST
#@ make_uninstall
#@ make_uninstall_BININST
include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
logpipe		:	$(c_FILE_o)
	$(CC) -o $@ $(c_FILE_o) $(LFLAGS)

