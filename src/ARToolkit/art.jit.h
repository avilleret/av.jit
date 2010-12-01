/*
 *  art.jit.h
 *  art.jit.simple
 *
 *  Created by Antoine Villeret on 19/09/10.
 */

typedef struct _art_jit_pattern
{
	char			name[256];
	//short			path_id;
	int             id;
	double          width;
	double          center[2];
	double          trans[3][4];
	
} t_art_jit_pattern;