#!/bin/bash

usage()
{
	echo "USAGE : logpipe.sh [ status | start | stop | kill | restart ]"
}

if [ $# -eq 0 ] ; then
	usage
	exit 9
fi

case $1 in
	status)
		ps -f -u $USER | grep "logpipe -f" | grep -v grep | awk '{if($3=="1")print $0}'
		ps -f -u $USER | grep "logpipe -f" | grep -v grep | awk '{if($3!="1")print $0}'
		;;
	start)
		ls -1 $HOME/etc/logpipe_*.conf | while read FILE ; do
			PID=`ps -f -u $USER | grep "logpipe -f $FILE" | grep -v grep | awk '{if($3=="1")print $2}'`
			if [ x"$PID" != x"" ] ; then
				echo "*** ERROR : logpipe existed"
				exit 1
			fi
			logpipe -f $FILE >/dev/null
			NRET=$?
			if [ $NRET -ne 0 ] ; then
				echo "*** ERROR : logpipe start error[$NRET]"
				exit 1
			fi
			while [ 1 ] ; do
				sleep 1
				PID=`ps -f -u $USER | grep "logpipe -f $FILE" | grep -v grep | awk '{if($3=="1")print $2}'`
				if [ x"$PID" != x"" ] ; then
					break
				fi
			done
			echo "logpipe start with $FILE ok"
		done
		logpipe.sh status
		;;
	stop)
		logpipe.sh status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		ls -1 $HOME/etc/logpipe_*.conf | while read FILE ; do
			PID=`ps -f -u $USER | grep "logpipe -f $FILE" | grep -v grep | awk '{if($3=="1")print $2}'`
			if [ x"$PID" = x"" ] ; then
				echo "*** ERROR : logpipe not existed"
				exit 1
			fi
			kill $PID
			C=0
			while [ $C -lt 5 ] ; do
				sleep 1
				PID=`ps -f -u $USER | grep "logpipe -f $FILE" | grep -v grep | awk '{if($3=="1")print $2}'`
				if [ x"$PID" = x"" ] ; then
					break
				fi
				C=`expr $C + 1`
			done
			if [ $C -ge 5 ] ; then
				ps -f -u $USER | grep "logpipe -f $FILE" | grep -v grep | awk '{print $2}' | while read PID ; do kill -9 $PID ; done
			fi
			echo "logpipe end ok"
		done
		;;
	kill)
		logpipe.sh status
		killall -9 logpipe
		;;
	restart)
		logpipe.sh stop
		sleep 1
		logpipe.sh start
		;;
	*)
		usage
		;;
esac

