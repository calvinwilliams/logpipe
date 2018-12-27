# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			logpipe-input-file.c \
			logpipe-output-file.c \
			logpipe-input-tcp.c \
			logpipe-output-tcp.c \
			logpipe-input-exec.c \
			logpipe-output-hdfs.c \
			logpipe-output-es.c \
			logpipe-output-kafka.c \
			logpipe-output-ingeek.c \
			logpipe-output-linemode-template.c \

###### 目标文件、安装目录配置区
include makeinstall
LIB		=	\
			logpipe-input-file.so \
			logpipe-output-file.so \
			logpipe-input-tcp.so \
			logpipe-output-tcp.so \
			logpipe-input-exec.so \
			logpipe-output-es.so \
			logpipe-output-ingeek.so \
			logpipe-output-linemode-template.so \

LIBINST		=	$(_LIBINST)

###### 编译选项
CFLAGS		=	$(_CFLAGS) \
			-std=gnu99 \
			-I$(HOME)/include/logpipe \

###### 链接选项
LFLAGS		=	$(_LFLAGS) \
			-L$(HOME)/lib \
			-llogpipe_api \
			-rdynamic \
			-lm \

###### 额外宏定义区
CLEAN_ADDITION	=	fasterhttp.o \
			logpipe-output-hdfs.so \
			logpipe-output-kafka.so \

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_LIBINST
#@ make_uninstall
#@ make_uninstall_LIBINST
include $(MKTPL2_HOME)/makeobj_$(MKTPL2_OS).inc

###### 目标文件依赖关系
logpipe-input-file.so		:	logpipe-input-file.o
	$(CC) -o $@ logpipe-input-file.o $(SOFLAGS) $(LFLAGS)

logpipe-output-file.so		:	logpipe-output-file.o
	$(CC) -o $@ logpipe-output-file.o $(SOFLAGS) $(LFLAGS)

logpipe-input-tcp.so		:	logpipe-input-tcp.o
	$(CC) -o $@ logpipe-input-tcp.o $(SOFLAGS) $(LFLAGS)

logpipe-output-tcp.so		:	logpipe-output-tcp.o
	$(CC) -o $@ logpipe-output-tcp.o $(SOFLAGS) $(LFLAGS)

logpipe-input-exec.so		:	logpipe-input-exec.o
	$(CC) -o $@ logpipe-input-exec.o $(SOFLAGS) $(LFLAGS)

fasterhttp.o			:	fasterhttp.c
	$(CC) $(CFLAGS) -c fasterhttp.c

CFLAGS_hdfs			=	$(CFLAGS) -I/home/hdfs/expack/hadoop/include

logpipe-output-hdfs.o		:	logpipe-output-hdfs.c
	$(CC) $(CFLAGS_hdfs) -c logpipe-output-hdfs.c

LFLAGS_hdfs			=	$(LFLAGS) -L/home/hdfs/expack/hadoop/lib/native -lhdfs -L$(HOME)/expack/jdk1.8.0_152/jre/lib/amd64/server -ljvm

logpipe-output-es.so		:	logpipe-output-es.o fasterhttp.o
	$(CC) -o $@ logpipe-output-es.o fasterhttp.o $(SOFLAGS) $(LFLAGS)

logpipe-output-hdfs.so		:	logpipe-output-hdfs.o
	$(CC) -o $@ logpipe-output-hdfs.o $(SOFLAGS) $(LFLAGS_hdfs)

CFLAGS_kafka			=	$(CFLAGS) -I$(HOME)/include/librdkafka

logpipe-output-kafka.o		:	logpipe-output-kafka.c
	$(CC) $(CFLAGS_kafka) -c logpipe-output-kafka.c

LFLAGS_kafka			=	$(LFLAGS) -L$(HOME)/lib -lrdkafka

logpipe-output-kafka.so		:	logpipe-output-kafka.o
	$(CC) -o $@ logpipe-output-kafka.o $(SOFLAGS) $(LFLAGS_kafka)

logpipe-output-ingeek.so	:	logpipe-output-ingeek.o
	$(CC) -o $@ logpipe-output-ingeek.o $(SOFLAGS) $(LFLAGS)

logpipe-output-linemode-template.so	:	logpipe-output-linemode-template.o
	$(CC) -o $@ logpipe-output-linemode-template.o $(SOFLAGS) $(LFLAGS)

