STRUCT	logpipe_conf
{
	STRUCT	inputs	ARRAY	10
	{
		STRING	256	input
	}
	
	STRUCT	outputs	ARRAY	10
	{
		STRING	256	output
	}
	
	STRUCT	rotate
	{
		INT	4	file_rotate_max_size
	}
	
	STRUCT	log
	{
		STRING	256	log_file
		STRING	10	log_level
	}
}

