STRUCT	logpipe_conf
{
	STRUCT	input	ARRAY	10
	{
		STRING	256	inotify_path
		STRING	20	listen_ip
		INT	4	listen_port
	}
	
	STRUCT	output	ARRAY	10
	{
		STRING	256	dump_path
		STRING	20	forward_ip
		INT	4	forward_port
	}
	
	STRUCT	log
	{
		STRING	256	log_file
		STRING	10	log_level
	}
}

