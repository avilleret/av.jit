/* 
	Copyright 2010 - Antoine Villeret
	ARToolkit simple example ported to Jitter
*/

#include "jit.common.h"
#include "art.jit.h"

// include ARToolkit
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>
#include <AR/ar.h>

typedef struct _art_jit_simple 
{
	t_object				ob;
	
	// ARToolkit variables
	int             thresh;			// binarisation threshold
	int				count;
	
	int             size[2];
	
	/*
	char           *patt_name, *patt_name2;
	int             patt_id,patt_id2;
	double          patt_width;
	double          patt_center[2];
	double          patt_trans[3][4];
	*/
	
	t_art_jit_pattern	pattern[16];
	char           *cparam_name;
	ARParam         cparam;
	
} t_art_jit_simple;

void *_art_jit_simple_class;

t_art_jit_simple	*art_jit_simple_new(void);
void				art_jit_simple_free(t_art_jit_simple *x);
t_jit_err			art_jit_simple_matrix_calc(t_art_jit_simple *x, void *inputs, void *outputs);
void				art_jit_simple_calculate_ndim(t_art_jit_simple *x, long dimcount, long *dim, long planecount, 
												  t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop);
t_jit_err			art_jit_simple_init(void);
void				art_jit_simple_read(t_art_jit_simple *x, t_symbol *s, short argc, t_atom *argv);
void				art_jit_simple_load(t_art_jit_simple *x, t_art_jit_pattern *pattern); // load pattern parameters
int					art_jit_simple_settings(t_art_jit_simple *x); // set the ARToolkit environnement

t_jit_err art_jit_simple_init(void) 
{
	long attrflags=0;
	t_jit_object *attr, *mop, *output;
	
	_art_jit_simple_class = jit_class_new("art_jit_simple",(method)art_jit_simple_new,(method)art_jit_simple_free,
		sizeof(t_art_jit_simple),0L);

	//add mop
	mop = jit_object_new(_jit_sym_jit_mop,1,1);  // Object has one input and one output
	
   	jit_mop_single_type(mop,_jit_sym_char);   // Set input type and planecount
   	jit_mop_single_planecount(mop,1);  
   	
   	jit_mop_output_nolink(mop,1); // Turn off output linking so that output matrix does not adapt to input
   	
	output = jit_object_method(mop,_jit_sym_getoutput,1); // Get a pointer to the output matrix
	
   	jit_attr_setlong(output,_jit_sym_minplanecount,14); // Output has 14 planes : area, id, direction, confidence value, pos (2 planes), edges (last 8 planes)
  	jit_attr_setlong(output,_jit_sym_maxplanecount,14);
  	jit_attr_setlong(output,_jit_sym_mindim,1);
  	jit_attr_setlong(output,_jit_sym_maxdim,1);
	jit_attr_setlong(output,_jit_sym_mindimcount,1);
	jit_attr_setlong(output,_jit_sym_maxdimcount,1);
  	jit_attr_setsym(output,_jit_sym_types,_jit_sym_float32);
	
	
	jit_class_addadornment(_art_jit_simple_class,(t_jit_object *)mop);

	//add methods
	jit_class_addmethod(_art_jit_simple_class, (method)art_jit_simple_matrix_calc, 	"matrix_calc", 		A_CANT, 0L);
	jit_class_addmethod(_art_jit_simple_class, (method)art_jit_simple_read,			"read",				A_GIMME, 0L);

	//add attributes	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	attr = jit_object_new(_jit_sym_jit_attr_offset,"thresh",_jit_sym_char,attrflags,
		(method)0L,(method)0L,calcoffset(t_art_jit_simple,thresh));
	jit_class_addattr(_art_jit_simple_class,attr);
	
	/*
	attr = jit_object_new(_jit_sym_jit_attr_offset,
						  "pattern",
						  _jit_sym_symbol,
						  attrflags,
						  (method)0L,
						  (method)0L,
						  calcoffset(t_art_jit_simple,patt_name));
	jit_class_addattr(_art_jit_simple_class,attr);
	*/
	
	jit_class_register(_art_jit_simple_class);
	
	post("ARToolkit simple example ported to Jitter by Antoine Villeret - 2010 - compiled on %s at %s", __DATE__, __TIME__);
	
	return JIT_ERR_NONE;
}


t_jit_err art_jit_simple_matrix_calc(t_art_jit_simple *x, void *inputs, void *outputs)
{
	t_jit_err err=JIT_ERR_NONE;
	long in_savelock,out_savelock;
	t_jit_matrix_info in_minfo,out_minfo;
	char *in_bp, *out_bp;
	float *out_data;
	ARUint8 *in_data;
	void *in_matrix, *out_matrix;
	int i;
	
	// ARToolkit variables 
	ARMarkerInfo    *marker_info;
    int             marker_num;

	// Get input and output matrix pointer
	in_matrix 	= jit_object_method(inputs,_jit_sym_getindex,0);
	out_matrix 	= jit_object_method(outputs,_jit_sym_getindex,0);

	if (x&&in_matrix&&out_matrix) {
		
		// Lock matrix
		in_savelock = (long) jit_object_method(in_matrix,_jit_sym_lock,1);
		out_savelock = (long) jit_object_method(out_matrix,_jit_sym_lock,1);
		
		// Get matrix infos
		jit_object_method(in_matrix,_jit_sym_getinfo,&in_minfo);
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
		
		// Get matrix data pointer
		jit_object_method(in_matrix,_jit_sym_getdata,&in_bp);
		
		if (!in_bp) { err=JIT_ERR_GENERIC; goto out;}
		
		//compatible types?
		if (in_minfo.type!=_jit_sym_char) { 
			err=JIT_ERR_MISMATCH_TYPE; 
			goto out;
		}		

		//compatible planes?
		if (in_minfo.planecount !=4) { 
			err=JIT_ERR_MISMATCH_PLANE; 
			goto out;
		}		
		
		if (x->size[0]!=in_minfo.dim[0] || x->size[1]!=in_minfo.dim[1]) {
			x->size[0] = in_minfo.dim[0];
			x->size[1] = in_minfo.dim[1];
			if(art_jit_simple_settings(x)) { err=JIT_ERR_GENERIC; goto out;}
		}

		in_data = (ARUint8 *)in_bp;
		if( arDetectMarker(in_data, 100, &marker_info, &marker_num) < 0 ) {
			jit_error_sym((t_object *)x,(t_symbol *)"Can't detect marker");
			return -1;
		}
		
		// prepare output matrix
		out_minfo.dim[0] = marker_num;
		jit_object_method(out_matrix,_jit_sym_setinfo,&out_minfo);		// store output matrix infos
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);		// recall output matrix infos
		jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);			// get output matrix data pointer

		if (!out_bp) { 
			err=JIT_ERR_GENERIC; 
			goto out; 
		}
		out_data = (float *)out_bp;
		
		if(marker_num == 0) {
			for (i=0;i<14;i++)
			{
				out_data[i] = -1;
			}
		}
		else {
			
			// fill in output matrix with marker info
			for (i=0;i<marker_num;i++)
			{	
				out_data[0] = (float) marker_info[i].area;
				out_data[1] = (float) marker_info[i].id;
				out_data[2] = (float) marker_info[i].dir;
				out_data[3] = marker_info[i].cf;
				out_data[4] = marker_info[i].pos[0];
				out_data[5] = marker_info[i].pos[1];
				int j;
				for (j = 0; j<8; j++){
					out_data[j+6] = marker_info[i].vertex[j/2][j%2];
				}
				//memcpy(out_data+6, marker_info->vertex, 8*sizeof(float));
				
				
				out_data+=14;
			}
		}	
		
				
	} else {
		return JIT_ERR_INVALID_PTR;
	}
	
out:
	jit_object_method(out_matrix,_jit_sym_lock,out_savelock);
	jit_object_method(in_matrix,_jit_sym_lock,in_savelock);
	return err;
}

t_art_jit_simple *art_jit_simple_new(void)
{
	t_art_jit_simple *x;
	int i; 
	
	printf("new instance of jit.art.simple\n");
		
	if (x=(t_art_jit_simple *)jit_object_alloc(_art_jit_simple_class)) {
		// some initialisation
		x->thresh = 100;
		/*
		x->patt_name = "patt.hiro";
		x->patt_name2 = "patt.kanji";
		x->patt_width     = 80.0;
		x->patt_center[0] = 0.0;
		x->patt_center[1] = 0.0;
		x->patt_id = -1;
		x->patt_id2 = -1;
		*/
		x->size[0] = 320;
		x->size[1] = 240;
		for (i=0;i<16;i++)
		{
			x->pattern[i].id = -1;
			x->pattern[i].width = 80.0;
			x->pattern[i].center[0]=0.;
			x->pattern[i].center[1]=0.;
		}
		
		x->cparam_name = "/Developer/SDK/ARToolKit/bin/Data/camera_para.dat";
		art_jit_simple_settings(x);
		printf("settings done\n");
		//art_jit_simple_load(x,x->patt_name,x->patt_id);
		//art_jit_simple_load(x,x->patt_name2, x->patt_id2);
		//printf("loading done\n");
		
	} else {
		x = NULL;
	}	
	return x;
}

void art_jit_simple_free(t_art_jit_simple *x)
{
	//nada
}

void art_jit_simple_read(t_art_jit_simple *x, t_symbol *s, short argc, t_atom *argv)
{
	char filename[256];
	short path_id;
	int i=0, id=-1;
	t_filehandle handle;
	long type;
	long code;
	
	printf("read method\n");
	
	code = FOUR_CHAR_CODE( 'NULL' );
	
	printf("argc = %d\n",argc);
	
	printf("read method called...\n");
	if(argc == 2)
	{
		printf("argc = 2\n");
		if(argv[0].a_type != A_LONG || argv[1].a_type != A_SYM)
		{
			error("Invalid argument to read command. Make sure argument is int & symbol.");
			return;
		}
		strcpy(filename,argv[1].a_w.w_sym->s_name);
		printf("strcpy ok\n");
		id = argv[0].a_w.w_long;
	}
	else if(argc == 0)
	{
		printf("argc = 0\n");
		if(!open_dialog(filename, &path_id, &type,NULL,0))
		{
			// find the first empty slot
			for(i=0;i<16;i++)
			{
				if(x->pattern[i].id==-1) break;
			}
			if (i==16)
			{
				jit_object_error((t_object *)x, "Pattern table full. Remove some before allocating more.");
				return;
			}
			else id = i;
			//Open file
			if(path_opensysfile(filename, path_id, &handle, READ_PERM))
			{
				jit_error_sym((t_jit_object *)x, (t_symbol *)"Could not open file");
				return;
			}
			if (filename=="") {
				return;
			}
		}
	}
	else if (argc==1 && argv[0].a_type == A_SYM)
	{
		printf("argc = 1\n");
		strcpy(filename,argv[0].a_w.w_sym->s_name);
		printf(" name : %s\n",argv[0].a_w.w_sym->s_name);
		// find the first empty slot
		for(i=0;i<16;i++)
		{
			printf("i :%d\n",i);
			if(x->pattern[i].id==-1) break;
		}
		if (i==16)
		{
			jit_object_error((t_object *)x, "Pattern table full. Remove some before allocating more.");
			return;
		}
		else id = i;
		printf("id : %d,i : %d\n",id,i);
	}
	else {
		printf("arguments invalides");
		return;
	}

	strcpy(x->pattern[id].name,filename);
	printf("filename %d : %s\n",id,x->pattern[id].name);
	art_jit_simple_load(x,x->pattern+id);
}

void art_jit_simple_load(t_art_jit_simple *x, t_art_jit_pattern *pattern)
{
	char absolutepathname[256],bootpathname[256];
	short path_id;
	long type;
	
	printf("load method\n");
	// free pattern ID
	if(pattern->id!=-1)
	{
	if(arFreePatt(pattern->id)!=1){
		printf("error : can't remove pattern data\n");
	}
	}
	
	if(!(strcmp(pattern->name,""))){
		jit_error_sym((t_jit_object *)x, (t_symbol *) "No file to load");
		return;
	}
	
	printf("patt_name : %s \n",pattern->name);
	if(!locatefile_extended(pattern->name,&path_id,&type,&type,-1))
	{	
		printf("file %s was successfully found\n",pattern->name);
		//path_opensysfile(argv[0].a_w.w_sym->s_name, id, &handle, READ_PERM); 
	}
	else
	{
		printf("couldn't find file\n");
		error("Could not find file %s",pattern->name);
		return;
	}
	
	printf("filename : %s\n", pattern->name);
	path_topathname(path_id, pattern->name, absolutepathname);	// get absolute path name
	printf("absolute pathname : %s\n", absolutepathname);
	path_nameconform(absolutepathname, bootpathname, PATH_TYPE_ABSOLUTE, PATH_TYPE_BOOT); // tranform abolute path name to path relative to BOOT
	printf("boot pathname : %s\n",bootpathname);
	
	// load load description file
	if( (pattern->id=arLoadPatt(bootpathname)) < 0 ) {
		jit_error_sym((t_jit_object *)x, (t_symbol *)"error in loading pattern...");
		return;
	}
	else printf("load successful, patt_id : %d\n",pattern->id);
}

int art_jit_simple_settings(t_art_jit_simple *x)
{
	ARParam  wparam;
	
	printf("change settings\n");
	/* set the initial camera parameters */
	if( arParamLoad(x->cparam_name, 1, &wparam) < 0 ) {
		jit_error_sym((t_jit_object *)x, (t_symbol *)"Camera parameter load error !!\n");
		return -1;
	}
	arParamChangeSize( &wparam, x->size[0], x->size[1], &x->cparam );
	arInitCparam( &x->cparam );
	//printf("*** Camera Parameter ***\n");
	//arParamDisp( &x->cparam );
	return 0;
}