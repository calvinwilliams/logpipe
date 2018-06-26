OS=`uname | awk '{print $1}'`
VERSION=`logpipe -v| tr 'v' ' ' | awk '{print $2}'`

BINFILES="	bin/logpipe
"

LIBFILES="	lib/liblogpipe_api.so
"

PLUGINFILES="	so/logpipe-*.so
"

SHFILES="	shbin/logpipe.sh shbin/logpipe_pack_bin.sh
"

cd $HOME
tar cvzf logpipe-${OS}-${VERSION}-bin.tar.gz $BINFILES $LIBFILES $PLUGINFILES $SHFILES

